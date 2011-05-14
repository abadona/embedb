// Copyright 2001 Victor Joukov, Denis Kaznadzey

// This is implementation of the B-tree based index.
// It implements three variations of index - unique key,
// duplicate key and so called range index, which maps
// ranges of consequitive numbers, e.g 1, 2, 3, 4, 5, -> 3, 4, 5, 6, 7
// It does this effectively, falling down to common B-Tree
// performance (with some space overhead - 4 bytes per key)
// if the input data differ in structure from range map.
// This index RELIES on MSB First layout of keys and values

// TODO:
//   0. merge stepCursor and removePair into stepCursor(cur, remove)
//   DONE
//   0.1 make removeEntry release free page and notify higher level that no more
//       page ref needed
//   Almost DONE by overloading decrementCounter procedure
//   with page housekeeping.
//   1. Rename keyAt into getKey and readKey and so with valueAt,
//     depending on whether it is copying version or not. Fix len of keyAt
//     somehow.
//   valueAt gone for good, replaced by readValue and compareValue
//   2. Do not compare pos and nkeys, because nkeys is actually nentries.
//     Either calculate (comp. intensive) or store true nkey for node, and
//     do not mix usage of nkeys and nentries.
//   3. Interface for count and implementation - count is attribute of payload
//     for internal nodes. It should be updated on each insert/remove for each
//     search path, and should be rolled back if this operation failed.
//   DONE
//   4. Test for cursor (fetch, remove)
//   5. Test for duplicate keys
//   6. Test for faulty conditions - insertion of existing key or key/value
//     search for non-existent key or key/value
//   7. Test for partial match
//   8. Test for freshly init'ed file (without detach/attach)
#include "edbTypes.h"
#include "edbExceptions.h"
#include "edbBTree.h"
#include "edbPagedFile.h"
#include <string.h>
#include <math.h>
#include <malloc.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

// Code configuration parameters
inline const char *cpoint(int n, const char *auxmsg=0)
{
    char buf[10];
    // itoa(n, buf, 10);
    sprintf (buf, "%d", n);
    int len = strlen(buf);
    if (auxmsg) len += strlen(auxmsg) + 1;
    char *res = new char[len+1];
    strcpy(res, buf);
    if (auxmsg) { strcat(res, " "); strcat(res, auxmsg); }
    return res;
}

// Debug and test
//#define TEST_VERBOSE
//#define DEBUG_FREESPACE (16*5)
#define DEBUG_FREESPACE (16*11) // duplicate

#ifdef TEST_VERBOSE
#include <stdio.h>
#include <stdlib.h>
#endif

namespace edb
{
// Internal flags
const int BTREE_MATCH_MASK = BTREE_PARTIAL | BTREE_BOTH | BTREE_LAST;

/////////////////////////////////////////////////////////////////////
// Const
const char   BTREE_SIGNATURE[]         = "EdBbTrEe";
const char   BTREE_NODE_SIGNATURE[]    = "NoDe";
const char   BTREE_FREE_SIGNATURE[]    = "FrEe";
const uint32 BTREE_VERSION             = 0;
// Little endian - method borrowed from TIFF
const char   BTREE_NATIVE_BYTEORDER[]  = "II";
const char   BTREE_FOREIGN_BYTEORDER[] = "MM";

/////////////////////////////////////////////////////////////////////
// !!! CCBO - conditionally convert byte order, now is identity
#define CCBO16(o, x) (x)
#define CCBO32(o, x) (x)
#define CCBO64(o, x) (x)

// !!! CBO - convert byte order, now is identity
#define CBO16(x) (x)
#define CBO32(x) (x)
#define CBO64(x) (x)


inline uint64 msb64(uint64 v)
{
    uint64 r = 0;
    uint8 *p = (uint8 *) &r;
    for (int n = 64-8; n >= 0; n -= 8)
        *p++ = (uint8) (0xff & (v >> n));
    return r;
}

/////////////////////////////////////////////////////////////////////
// Structures, describing physical layout of BTree nodes
#ifdef _MSC_VER
#pragma pack (push, 1)
#pragma warning (push)
#pragma warning (disable : 4200) // non-standard extension - array with zero elements
#endif

// Page number size, does not add much size overhead to the database anyway,
// so the default is 48 bit integer.
// Page reference definition
typedef struct { uint16 hi; uint32 lo; } ShortPageRef;
typedef struct { uint16 hi; uint16 cnthi; uint32 lo; uint32 cntlo; } PageRef;
// Size of page reference on disk
const uint32 PageRefSize = 12;
// Read/write
static inline LogPageNumT ReadPageNum (const void *base, uint16 n = 0)
{
    const PageRef *e = (const PageRef *) base + n;
    return (LogPageNumT(e->hi) << sizeof (e->lo) * 8) | LogPageNumT(e->lo);
}

static inline void WritePageNum (void *base, uint16 n, LogPageNumT v)
{
    PageRef *e = (PageRef *) base + n;
    e->hi = (uint16) (v >> sizeof (e->lo) * 8);
    e->lo = (uint32) v;
}

static inline uint64 ReadCount (const void *base, uint16 n)
{
    const PageRef *e = (const PageRef *) base + n;
    return (uint64(e->cnthi) << sizeof (e->cntlo) * 8) | uint64(e->cntlo);
}

static inline void WriteCount (void *base, uint16 n, uint64 v)
{
    PageRef *e = (PageRef *) base + n;
    e->cnthi = (uint16) (v >> sizeof (e->cntlo) * 8);
    e->cntlo = (uint32) v;
}

// Layout of master page - this is mirror of file page at offset 0
struct BTreeMasterPage   // version 0
{
    char    signature[8];     // should be MbDbBtRe
    char    byteorder[2];     // either MM (Big Endian) or II (Little Endian)
    // following fields are layed out according to endianness
    uint32  version;          // index version (now 0)
    // following fields are defined by index version field
    // this layout is for version 0
    uint32  flags;            // flags - is range index, does it allow duplicate keys etc.
    uint64  rootnodeoff;      // offset of the root node
    uint64  firstnodeoff;     // offset of first non-root node
    uint64  rootfreenodeoff;  // offset of root node of free space
    uint32  nodesize;         // size of non-root page
    uint16  keylen;           // size of key for fixed size key index, 0 for variable
    uint16  vallen;           // size of user value (without overhead)
} ;

struct BTreeNodeHeader
{
    char    signature[4];  // should be NoDe
    PageRef left;          // pointer to left sibling
    PageRef right;         // pointer to right sibling
    uint8   level;         // level of node, 0 - leaf
    uint8   flags;         // flags
    uint32  len;           // total length of data in the node
    uint16  nkeys;         // number of keys in the node
    char    data[0];       // the rest of the node
} ;

struct BTreeFreeHeader
{
    char    signature[4];  // should be FrEe
    PageRef next;          // pointer to next free node
    char    data[0];       // the rest of the node
} ;

typedef uint32 RangeLenT;

const LogPageNumT DanglingPageRef = (LogPageNumT) -1;
enum { EXPAND_NO=0, EXPAND_INSERT, EXPAND_REMOVE };

#ifdef _MSC_VER
#pragma warning (pop)
#pragma pack (pop)
#endif

// Key comparison
inline int keycomp (const void *buf1, size_t cnt1, const void *buf2, size_t cnt2)
{
#if 0
    int res;
    size_t cnt = min_ (cnt1, cnt2);
    res = memcmp (buf1, buf2, cnt);
    if (res == 0) {
        if (cnt1 < cnt2) res = -1;
        else if (cnt1 >= cnt2) res = 1;
    }
    return res;
#else
    return memcmp (buf1, buf2, min_ (cnt1, cnt2));
#endif
}

// Ad-hoc lexcomp with extra parameter, which is used for comparison
// in duplicate key case. In this case val is extra addition to buf2.
inline int keyvalcomp (const char *buf1, size_t cnt1, const char *buf2, size_t cnt2, const char *val, size_t cntv)
{
    int res;
    size_t cnt = min_ (cnt1, cnt2);
    res = memcmp (buf1, buf2, cnt);
    if (res == 0) {
        if (cnt1 < cnt2) res = -1;
        else if (cnt1 >= cnt2) {
            // Compare additional buffer
            cnt1 -= cnt2;
            buf1 += cnt2;
            cnt = min_ (cnt1, cntv);
            res = memcmp (buf1, val, cnt);
            if (res == 0) {
                if (cnt1 < cntv) res = -1;
                else if (cnt1 > cntv) res = 1;
            }
        }
    }
    return res;
}

/////////////////////////////////////////////////////////////////////
// Handler classes
/////////////////////////////////////////////////////////////////////

// Auxiliary object for handling nodes in memory
// Actually, it is smart pointer, tracing locking
// of pages automatically
class BTreeNodePtr : public BTreeNodePtrBase
{
public:
    BTreeNodePtr (BTreeFile &file, uint32 nodesize)
        :
        BTreeNodePtrBase (&file, nodesize)
    {}
    // for trace support
    BTreeNodePtr ()
        :
        BTreeNodePtrBase (0, 0)
    {}
    void fetch (LogPageNumT pageno, uint32 count=1, uint32 offset=0, uint32 nodesize=0)
    {
        BTreeNodePtrBase::fetch (pageno, count, offset, nodesize);
        // Check signature
        if (0 != memcmp (header()->signature,
            BTREE_NODE_SIGNATURE,
            sizeof (header()->signature))) throw FileStructureCorrupt(cpoint(__LINE__));
    }
    BTreeNodeHeader *header () { return (BTreeNodeHeader *) data_; }
    uint8 level () const { return ((BTreeNodeHeader *) data_)->level; }
    bool isleaf () const { return 0 == ((BTreeNodeHeader *) data_)->level; }
    uint16 nkeys () { return ((BTreeNodeHeader *) data_)->nkeys; }
    char *data () { return ((BTreeNodeHeader *) data_)->data; }
    // node siblings
    LogPageNumT readLeft() { return ReadPageNum(&header()->left); }
    LogPageNumT readRight() { return ReadPageNum(&header()->right); }
    void writeLeft(LogPageNumT pg) { WritePageNum(&header()->left, 0, pg); }
    void writeRight(LogPageNumT pg) { WritePageNum(&header()->right, 0, pg); }
} ;

struct Trace
{
    Trace() { ptr = 0; }
    void push(BTreeNodePtrBase &node, int pos)
    {
        rgnp[ptr].assign(node);
        rgn[ptr] = pos;
        ++ptr;
    }
    int size() { return ptr; }
    int ptr;
    BTreeNodePtr rgnp[16];
    int rgn[16];
} ;

class BTreeNodeHandler
{
public:
    BTreeNodeHandler (BTreeFile &file,
      uint64 rootnodeoff,
      uint64 firstnodeoff,
      uint64 rootfreenodeoff,
      uint32 nodesize,
      uint32 rootnodesize,
      uint16 vallen,
      uint32 flags)
    :
    file_(file),
    rootnodeoff_(rootnodeoff),
    firstnodeoff_(firstnodeoff),
    rootfreenodeoff_(rootfreenodeoff),
    nodesize_(nodesize),
    rootnodesize_(rootnodesize),
    vallen_(vallen),
    flags_(flags)
    {}
    virtual ~BTreeNodeHandler () {}
    void insert (const void *key, LenT len, const void *val)
    {
        checkInsertParams(key, len, val);
        // Find leaf node, providing that all pages on path
        // have place to insert new key
        BTreeNodePtr node (file_, nodesize_);
        Trace trace;
        findLeaf (key, len, val, BTREE_BOTH, EXPAND_INSERT, node, &trace);
        insert(node, key, len, val);
        incrementCounter(trace);
    }
    void find(BTreeCursor &cur, Trace *trace, int expand=EXPAND_NO)
    {
        checkFindParams(cur.qry_->key_, cur.qry_->len_, cur.qry_->val_, cur.qry_->match_);
        // Find leaf page
        BTreeNodePtr node (file_, nodesize_);
        findLeaf (cur.qry_->key_, cur.qry_->len_, cur.qry_->val_, cur.qry_->match_, expand, node, trace);
        // Search leaf node
        uint64 pos = searchLeaf (node, cur.qry_->key_, cur.qry_->len_, cur.qry_->val_, cur.qry_->match_, false);
        cur.assign (node);
        cur.pos_ = pos;
    }
    void initcursor(BTreeCursor &cur, int expand, Trace *trace = 0)
    {
        if (cur.pos_ != (uint64) -1) {
            // Position cursor to cur.pos_
            if (expand) throw BadParameters();
            findByPos(cur, trace);
        } else {
            find(cur, trace, expand);
        }
    }
    bool fetch (BTreeCursor &cur, const void *&key, LenT &len, void *val)
    {
        bool first = !cur.ptr_;
        if (cur.ptr_) {
            // Page already locked, step forward or backward
            if (!stepCursor (cur)) return false;
        } else {
            // First time
            try {
                initcursor(cur, EXPAND_NO);
            } catch (NotFound &) {
                cur.free (); // regular situation, can safely release node
                return false;
            } catch (Error &) {
                throw;
            }
        }
        // Return value
        readValue (cur, cur.pos_, val);
        // Key
        keyAt (cur, cur.pos_, key, len); // FIXME, see TODO in the beginning
        // FIXME: due to copying nature of find, readValue (used earlier) and keyAt this
        // function can change key, len and val even if it returns BTREE_NOMORE due
        // to query limit of the fetch.
        if (!first && cur.qry_->done (key, len, val, vallen_)) {
            cur.free ();
            return false;
        }
        return true;
    }
    uint64 remove (BTreeCursor &cur)
    {
        uint64 leafStart;
        Trace trace;
        try {
            // FIXME: wrong place for BTREE_FLAGS_RANGE, BTreeNodeHandler
            // should not know about it! Push it down by hierarchy.
            initcursor(cur, !!(flags_ & BTREE_FLAGS_RANGE) ? EXPAND_REMOVE : EXPAND_NO, &trace);
            leafStart = countPairs(*((BTreeNodePtr *) &cur), cur.pos_);
        } catch (NotFound &) {
            cur.free (); // regular situation, can safely release node
            return 0;
        } catch (Error &) {
            throw;
        }
        uint64 cnt = 0;
		do {
            ++cnt;
			// Delete key/value and set cursor to the next value to be deleted
            if (!stepCursor (cur, true)) break;
			// Check new positions
		} while (!done(cur));
        decrementCounter(trace, leafStart, cnt);
		return cnt;
    }
    uint64 getpos(const void *key, LenT len, const void *val, LenT vlen, int match)
    {
        Trace trace;
        BTreeQueryBase qry(match, true, key, len, val, vlen);
        BTreeCursor cur;
        // manually initialize all cursor's relevant fields
        cur.file_   = &file_;
        cur.dfltnodesize_ = nodesize_;
        cur.qry_    = &qry;
        cur.fInit_  = true;
        find(cur, &trace);
        uint64 r = 0;
        int size = trace.size();
        for (int n = 0; n < size; ++n) {
            BTreeNodePtr &node = trace.rgnp[n];
            int pos = trace.rgn[n];
            // if pos == 0 then we pass incorrect value of
            // InvalidPos, but we can just not call countPairs
            // to avoid this mess
            if (pos > 0) r += countPairs(node, pos-1);
        }
        r += countPairs(*(BTreeNodePtr *)&cur, cur.pos_);
        return r;
    }
protected:
    // new node handler signature
    enum { InvalidPos = (uint64) -1 };
    virtual void *getKey (BTreeNodePtr &node, uint16 pos) = 0;
    virtual void readValue (BTreeNodePtrBase &node, uint64 pos, void *val) = 0;
    virtual int  compareValue(BTreeNodePtrBase &node, uint64 pos, const void *val, uint16 vlen) = 0;
    virtual bool done (BTreeCursor &cur) = 0;
    virtual bool stepCursor (BTreeCursor &cur, bool remove = false) = 0;
    virtual void incrementCounter(Trace &trace) = 0;
    virtual void decrementCounter(Trace &trace, uint64 leafStart, uint64 decrement) = 0;
    virtual uint64 countPairs(BTreeNodePtr &node, uint64 pos = InvalidPos) = 0;
    virtual void checkInsertParams(const void *key, LenT len, const void *val) = 0;
    virtual void checkFindParams(const void *key, LenT len, const void *val, int match) = 0;
    virtual LogPageNumT handleDanglingPageRef(BTreeNodePtr &parent, int pos,
        const void *key, LenT klen, const void *val) = 0;
    // old signature
    virtual bool keyAt (BTreeNodePtrBase &node, uint16 pos, const void *&key, LenT len) = 0;
    virtual LogPageNumT refAt (BTreeNodePtr &node, uint16 pos) = 0;
    virtual bool check (BTreeNodePtr &node) const = 0;
    virtual int searchNode (BTreeNodePtr &node, const void *key, LenT klen, const void *val, int match, bool fInsert) = 0;
    virtual int searchNodeByCount(BTreeNodePtr &node, uint64 &count) = 0;
    virtual uint64 searchLeaf (BTreeNodePtr &node, const void *key, LenT klen, const void *val, int match, bool fInsert) = 0;
    virtual uint64 searchLeafByCount(BTreeNodePtr &node, uint64 count) = 0;
    virtual bool enoughSpace (BTreeNodePtr &node) = 0;
    virtual bool splitRoot (BTreeNodePtr &node) = 0;
    virtual bool makeroom (BTreeNodePtr &parent, int nodePos,
        const void *key, LenT klen,
        BTreeNodePtr &node) = 0;
    virtual int insert (BTreeNodePtr &node, const void *key, LenT klen, const void *val) = 0;
    // fields
    BTreeFile &file_;
    uint64 rootnodeoff_;
    uint64 firstnodeoff_;
    uint64 rootfreenodeoff_;
    uint32 nodesize_;
    uint32 rootnodesize_;
    uint16 vallen_;
    uint32 flags_;
    // service for subclasses
    bool newNode (BTreeNodePtr &node)
    {
        if (rootfreenodeoff_) {
            BTreeMasterPage *mp =
                (BTreeMasterPage *) file_.fetch (0, 1);
            LogPageNumT pg = rootfreenodeoff_;
            node.fetch(pg);
            BTreeNodeHeader *nh = node.header();
            if (0 == memcmp(&(nh->signature), BTREE_FREE_SIGNATURE, sizeof (nh->signature))) {
                BTreeFreeHeader *fp = (BTreeFreeHeader *) nh;
                LogPageNumT pgnext = ReadPageNum(&fp->next, 0);
                memset(node.ptr(), 0, nodesize_);
                memcpy (nh->signature, BTREE_NODE_SIGNATURE, sizeof (nh->signature));
                mp->rootfreenodeoff = pgnext;
                rootfreenodeoff_ = pgnext;
                file_.mark(mp);
                node.mark();
                return true;
            } // else allocate from the end
        }
        FilePos len = file_.length ();
        if (len % nodesize_ != 0) return false;
        LogPageNumT pg = len / nodesize_;
        node.fake (pg);
        memset (node.ptr(), 0, nodesize_);
        BTreeNodeHeader *np = node.header();
        memcpy (np->signature, BTREE_NODE_SIGNATURE, sizeof (np->signature));
        node.mark ();
        return true;
    }
    void chainToFreeList(LogPageNumT pg)
    {
        BTreeMasterPage *mp =
            (BTreeMasterPage *) file_.fetch (0, 1);
        LogPageNumT prev = rootfreenodeoff_;
        BTreeFreeHeader *fp = (BTreeFreeHeader *) file_.fetch(pg);
        memcpy (&fp->signature, BTREE_FREE_SIGNATURE, sizeof (fp->signature));
        WritePageNum(&fp->next, 0, prev);
        mp->rootfreenodeoff = pg;
        rootfreenodeoff_ = pg;
        file_.mark(fp);
        file_.mark(mp);
#if defined (_DEBUG) && defined (TEST_VERBOSE)
        std::cerr <<"free page" << pg << std::endl;
#endif
    }
#if 0
    int reclaimSubtree(LogPageNumT pg)
    {
        int res = BTREE_OK;
        BTreeNodePtr node(file_, nodesize_);
        if (pg == 0) {
            // See comment in findLeaf about root node
            node.fetch (0, 2, (uint32) rootnodeoff_, rootnodesize_);
        } else {
            node.fetch(pg);
        }
        if (node.level()) {
            for (int n = 0; n <= node.nkeys(); ++n) {
                res = reclaimSubtree(refAt(node, n));
                if (BTREE_OK != res) break;
            }
        } else {
            node.free();
            if (pg != 0) chainToFreeList(pg);
        }
        return res;
    }
#endif
private:
	// Find first leaf node containing given key and value.
	// Whether it will use the value depends on searchNode method,
	// it should do so only for duplicate index and if key is mentioned
	// in full length.
    // Expects that node is initialized with file_ and nodesize_.
	// FIXME_NOW: provide a way to find LAST leaf node containing the key
	// for implementing BTREE_LAST flag.
    void findLeaf (const void *key, LenT len, const void *val,
        int match,
		int expand,    // do we need to expand tree, and for what purpose
		BTreeNodePtr &node, // node pointer goes there
		Trace *trace = 0 // pointer to a space for search trace
		) {
        // Find leaf node, and if required, provide that
        // all pages on path have place to insert new key
        // As a prerequisite node should be defined in calling proc as
        // BTreeNodePtr node (file_, nodesize_);
        BTreeNodePtr parent (file_, nodesize_);
        // Master rec and root node
		// Root node is larger than all other nodes, it takes rest of master page and
		// whole next page. That is why we fetch root node with extra 3 parameters to
		// NodePtr - this enable it to handle body() and size() methods properly.
        node.fetch (0, 2, (uint32) rootnodeoff_, rootnodesize_);
        if (expand && !enoughSpace(node) && !splitRoot(node))
            throw FileStructureCorrupt(cpoint(__LINE__));
        int myMatch = match | BTREE_PARTIAL;
        while (node.level()) {
            // Check that node is well-built
            if (!check (node)) throw FileStructureCorrupt(cpoint(__LINE__));
            // Inexact search for next level node
            int pos = searchNode (node, key, len, val, myMatch, false);
            if (pos < 0) throw FileStructureCorrupt(cpoint(__LINE__));
            LogPageNumT pg = refAt (node, pos);
            parent.assign (node);
            if (DanglingPageRef == pg) {
                // In internal nodes each key associates with two page references - 
                // left and right. When deleting stuff there can be a case that a
                // key is alone on the node and one of this references points nowhere.
                // This case is handled at removal by inserting DanglingPageRef, and
                // hadnled during the search for insert by inserting empty nodes and
                // moving the DanglingPageRef into them as needed until we reach leaf,
                // which we just can leave empty.
                //
                // We should actually know, what are we doing, insert or remove
                // because Range key removal which usually requires that we provide
                // space for the operation, should just throw NotFound as well as search,
                // instead of trying to fix the tree.
                // Thats is why we have expand to be 3-state: no, yes for insert, yes for delete
                if (EXPAND_NO == expand || EXPAND_REMOVE == expand) throw NotFound();
                pg = handleDanglingPageRef(parent, pos, key, len, val);
            }
            try { node.fetch (pg); } catch (Error &) { throw FileStructureCorrupt(cpoint(__LINE__)); }
            // whether the node has place for extra key?
            // NB: if the result of makeroom depends on actual key, then for various
            // length index this would not work for deletion, because we present it
            // a key which is going to be deleted, but this can split entry and introduce
            // possibly larger key which will not fit. So we probably better pass expand
            // as a parameter to makeroom to get the necessary calculations
            if (expand && !enoughSpace(node) &&
                !makeroom (parent, pos, key, len, node))
                 throw FileStructureCorrupt(cpoint(__LINE__));
            if (trace) {
                // put parent (by assign'ing) and pos into trace
                trace->push(parent, pos);
            }
        }
    }
    void findByPos(BTreeCursor &cur, Trace *trace)
    {
        // Find leaf page
        BTreeNodePtr node (file_, nodesize_);
        uint64 count = cur.pos_;
        BTreeNodePtr parent (file_, nodesize_);
        node.fetch (0, 2, (uint32) rootnodeoff_, rootnodesize_);
        while (node.level()) {
            // Check that node is well-built
            if (!check (node)) throw FileStructureCorrupt(cpoint(__LINE__));
            // Inexact search for next level node
            int pos = searchNodeByCount(node, count);
            if (pos < 0) throw FileStructureCorrupt(cpoint(__LINE__));
            LogPageNumT pg = refAt (node, pos);
            parent.assign (node);
            if (DanglingPageRef == pg) throw NotFound();
            try { node.fetch (pg); } catch (Error &) { throw FileStructureCorrupt(cpoint(__LINE__)); }
            if (trace) {
                // put parent (by assign'ing) and pos into trace
                trace->push(parent, pos);
            }
        }
        uint64 pos = searchLeafByCount(node, count);
        cur.assign (node);
        cur.pos_ = pos;
    }
} ;

// Native byteorder handlers

// Base fixed key handler. Unique key, non-range value.
class FixedNodeHandler : public BTreeNodeHandler
{
public:
    FixedNodeHandler (
        BTreeFile &file,
        uint64 rootnodeoff,
        uint64 firstnodeoff,
        uint64 rootfreenodeoff,
        uint32 nodesize,
        uint32 rootnodesize,
        uint16 vallen,
        uint32 flags,
        uint16 keylen)
    :
        BTreeNodeHandler (
            file,
            rootnodeoff,
            firstnodeoff,
            rootfreenodeoff,
            nodesize,
            rootnodesize,
            vallen,
            flags)
    {
        truekeylen_ = keylen;
        if (flags_ & BTREE_FLAGS_DUPLICATE) {
            payloadlen_ = 0;
            keylen_ = keylen + vallen_;
        } else {
            payloadlen_ = vallen_;
            keylen_ = keylen;
        }
    }
protected:
    void *getKey (BTreeNodePtr &node, uint16 pos)
    {
        if (pos >= node.nkeys()) return 0;
        return node.data()+pos*(uint32)keylen_;
    }
    char *getVals(BTreeNodePtr &node)
    {
        return node.data()+node.nkeys()*(uint32)keylen_;
    }
    // This methods implements unique key, non-range value
    // it is substituted in both DupKey and Range subclasses
    void readValue (BTreeNodePtrBase &node, uint64 pos, void *val)
    {
        const BTreeNodeHeader *np = (const BTreeNodeHeader *) node.body ();
        assert (0 == np->level);
        if (pos >= np->nkeys) throw FileStructureCorrupt(cpoint(__LINE__));
        const char *vals = np->data+np->nkeys*keylen_;
        memcpy (val, vals+pos*vallen_, vallen_);
    }
    int compareValue(BTreeNodePtrBase &node, uint64 pos, const void *val, uint16 vlen)
    {
        const BTreeNodeHeader *np = (const BTreeNodeHeader *) node.body ();
        assert (0 == np->level);
        if (pos >= np->nkeys) throw FileStructureCorrupt(cpoint(__LINE__));
        const char *vals = np->data+np->nkeys*keylen_;
        return keycomp(vals+pos*vallen_, vallen_, val, vlen);
    }
    bool done (BTreeCursor &cur)
    {
        uint16 pos = (uint16) cur.pos_; // use only lower bits of pos
        BTreeNodeHeader *node = (BTreeNodeHeader *)cur.body();
        const void *key = node->data+pos*(uint32)keylen_;
        const void *val = node->data+node->nkeys*(uint32)keylen_+pos*(uint32)vallen_;
        return cur.qry_->done(key, keylen_, val, vallen_);
    }
    bool stepCursor (BTreeCursor &cur, bool remove) {
        // For FixedNodeHandler cur.pos_ is position of pair in node
        // it is 16 bit and equivalent to position of entry
        int newpos = (int) (uint16) cur.pos_;
        BTreeNodePtr &node = *(BTreeNodePtr *)&cur;
        if (remove) {
            removeEntry(node, (uint16) cur.pos_, 1);
            --newpos;
        }
        newpos += cur.qry_->stepForward() ? 1 : -1;
        if (newpos < 0 || newpos >= node.nkeys()) {
            LogPageNumT pg = cur.qry_->stepForward() ? node.readRight() : node.readLeft();
            if (0 >= pg) {
                cur.free ();
                return false;
            }
            if (remove && 0 == node.nkeys()) {
                // remove leaf node here
                reclaimNode(node);
            }
            try { cur.fetch (pg); }
            catch (Error &) {
                cur.free ();
                throw FileStructureCorrupt(cpoint(__LINE__));
            }
            newpos = 0;
        }
        cur.pos_ = cur.qry_->stepForward() ? newpos : ((BTreeNodePtr *)&cur)->nkeys() - 1;
        return true;
    }
    void checkInsertParams(const void *key, LenT len, const void *val)
    {
        if (len != truekeylen_) throw BadParameters(cpoint(__LINE__));
    }
    void checkFindParams(const void *key, LenT len, const void *val, int match)
    {
        // Searching for key which is larger than key length of index
        // makes no sense
        if (len > truekeylen_) throw BadParameters(cpoint(__LINE__));
        // If PARTIAL flag does not set, key length should match
        // one of the index
        if (!(match & BTREE_PARTIAL) && truekeylen_ != len) throw BadParameters(cpoint(__LINE__));
        // Nonsensical query
        if ((match & BTREE_PARTIAL) && (match & BTREE_BOTH)) throw BadParameters(cpoint(__LINE__));
    }
    //
    bool keyAt (BTreeNodePtrBase &node, uint16 pos, const void *&key, LenT len)
    {
        const BTreeNodeHeader *np = (const BTreeNodeHeader *) node.body ();
        if (pos >= np->nkeys) return false;
        key = np->data+pos*(uint32)keylen_;
        len = keylen_;
        return true;
    }
    LogPageNumT refAt (BTreeNodePtr &node, uint16 pos)
    {
        assert (node.level() > 0);
        if (pos > node.nkeys()) return (LogPageNumT) -1;
        char *vals = node.data()+node.nkeys()*(uint32)keylen_;
        return ReadPageNum(vals, pos);
    }
    // This method implements both unique and duplicate key (based on parameters)
    // but not range value. Thus it is substituted in RangeNodeHandler.
    // It is also used to insert page reference into inner pages, that is why
    // it have parameter 'after', which means that we are inserting key and its
    // value goes after it, not before as usual. Conceptual layout for inner page
    // (real layout groups keys and refs together) is as following:
    // | ref0 | key0 | ref1 | key1 | ref2 | etc.
    virtual int insertAt (BTreeNodePtr &node, int pos, const void *key, LenT klen, const void *val, bool after=false)
    {
        uint16 nkeys = node.nkeys();
        char *keys = node.data();
        char *vals = keys + nkeys*keylen_;
        size_t len;
        char *src;
        uint32 vallen = node.level() ? PageRefSize : payloadlen_;
        LenT keylen = node.level() || !(flags_ & BTREE_FLAGS_DUPLICATE) ? keylen_ : keylen_ - vallen_;
        assert (keylen == klen);
        if (vallen > 0) {
            // make room for value
            src = vals+(after ? pos+1 : pos)*(uint32)vallen;
            if (pos < nkeys) {
                len = (nkeys-pos)*(uint32)vallen;
                memmove(src+keylen_+vallen, src, len);
            }
            // copy value
            memcpy(src+keylen_, val, vallen);
        }
        // make room for key
        len = (nkeys-pos)*(uint32)keylen_ + (after ? pos+1 : pos)*vallen;
        src = keys+pos*(uint32)keylen_;
        memmove(src+keylen_, src, len);
        // copy key
        memcpy(src, key, keylen);
        if (keylen < keylen_) {
            assert (flags_ & BTREE_FLAGS_DUPLICATE);
            memcpy(src+keylen, val, vallen_);
        }
        node.header()->nkeys++;
        return BTREE_OK;
    }
    bool check (BTreeNodePtr &node) const
    {
        BTreeNodeHeader *np = node.header();
        uint16 nkeys = np->nkeys;
        uint32 len = keylen_ * nkeys; // size of keys
         // size of values for leaf node or refs to other pages for internal node
        len += node.level() ? PageRefSize * (nkeys + 1) : payloadlen_ * nkeys;
        return len == np->len &&
            len + sizeof (BTreeNodeHeader) <= node.size();
    }
    void setlen (BTreeNodePtr &node)
    {
        BTreeNodeHeader *np = node.header();
        uint16 nkeys = np->nkeys;
        uint32 len = keylen_ * nkeys; // size of keys
         // size of values for leaf node or refs to other pages for internal node
        len += node.level() ? PageRefSize * (nkeys + 1) : payloadlen_ * nkeys;
        assert (len + sizeof (BTreeNodeHeader) <= node.size());
        np->len = len;
    }
	const char *findEntry (const char *keys, int nkeys, const void *key, LenT klen, const void *val, int match)
	{
        // Binary search - modified STL version of lower_bound or upper_bound (for Range)
		int n = nkeys;
        const char *f = keys;
        const char *l = f + nkeys*keylen_;
        int thresh = !!(flags_ & BTREE_FLAGS_RANGE) || !!(match & BTREE_LAST) ? 1 : 0;
        if (!(flags_ & BTREE_FLAGS_DUPLICATE) || !(match & BTREE_BOTH)) {
            // unique, or duplicate with search by key part only
            for (; 0 < n;) {
                int n2 = n / 2;
                const char *m = f + n2*keylen_;
                // if (!(key < *m)) or (*m <= key) in case of range
                // if (*m < key)) for all others
                if (keycomp (m, keylen_, key, klen) < thresh) {
                    f = m + keylen_;
                    n -= n2 + 1;
                } else {
                    n = n2;
                }
            }
        } else {
            // duplicate search by both key and value
            for (; 0 < n;) {
                int n2 = n / 2;
                const char *m = f + n2*keylen_;
                // if (!(key < *m)) or (*m <= key) in case of range
                // if (*m < key)) for all others
                if (keyvalcomp (m, keylen_, (const char *) key, klen, (const char *) val, vallen_) < thresh) {
                    f = m + keylen_;
                    n -= n2 + 1;
                } else {
                    n = n2;
                }
            }
        }
		return f;
	}
    void reclaimNode(BTreeNodePtr &node)
    {
        LogPageNumT pgleft = node.readLeft();
        LogPageNumT pgright = node.readRight();
        LogPageNumT pgcur = node.page();
        if (pgleft > 0) {
            BTreeNodePtr lnode(file_, nodesize_);
            lnode.fetch(pgleft);
            lnode.writeRight(pgright);
            lnode.mark();
        }
        if (pgright > 0) {
            BTreeNodePtr rnode(file_, nodesize_);
            rnode.fetch(pgright);
            rnode.writeLeft(pgleft);
            rnode.mark();
        }
        chainToFreeList(pgcur);
    }
    void reclaimNodeChain(BTreeNodePtr &node, uint64 count, uint64 &rest)
    {
        LogPageNumT pgleft = node.readLeft();
        LogPageNumT pgcur = node.page();
        uint64 cntNode = countNodePairs(node);
        uint64 deleted = 0;
        while (count >= cntNode) {
            count -= cntNode;
            LogPageNumT pgright = node.readRight();
            if (pgright <= 0) break; // ??? throw FileStructureCorrupt(cpoint(__LINE__)); 
            node.fetch(pgright);
            chainToFreeList(pgcur);
            ++deleted;
            pgcur = pgright;
            cntNode = countNodePairs(node);
        }
        // Sibling links
        if (deleted) {
            node.writeLeft(pgleft);
            node.mark();
            if (pgleft > 0) {
                BTreeNodePtr lnode(file_, nodesize_);
                lnode.fetch(pgleft);
                lnode.writeRight(pgcur);
                lnode.mark();
            }
        }
        rest = count;
    }
    int searchNodeByCount(BTreeNodePtr &node, uint64 &count)
    {
        uint64 cumul = 0;
        char *vals = getVals(node);
        int n = 0;
        int size = node.nkeys();
        uint64 poscnt;
        while (n < size && cumul + (poscnt = ReadCount(vals, n)) < count) {
            cumul += poscnt; ++n;
        }
        count -= cumul;
        return n;
    }
    uint64 searchLeafByCount(BTreeNodePtr &node, uint64 count)
    {
        if (count >= node.nkeys()) throw NotFound();
        return count;
    }
    void incrementCounter(Trace &trace)
    {
        int size = trace.size();
        for (int n = 0; n < size; ++n) {
            BTreeNodePtr &node = trace.rgnp[n];
            char *vals = getVals(node);
            int pos = trace.rgn[n];
            WriteCount(vals, pos, ReadCount(vals, pos)+1);
            node.mark();
        }
    }
    void removeLeftKeys(BTreeNodePtr &node, uint64 dcr)
    {
        char *vals = getVals(node);
        uint64 cnt;
        int pos = 0;
        while ((cnt = ReadCount(vals, 0)) <= dcr) {
            if (node.nkeys() == 1) {
                // create left dangling pointer here
                WriteCount(vals, 0, 0);
                WritePageNum(vals, 0, DanglingPageRef);
                dcr -= cnt;
                pos = 1;
                cnt = ReadCount(vals, pos);
                break;
            }
            removeEntry(node, 0, 1);
            dcr -= cnt;
            vals = getVals(node);
        }
        assert (dcr < cnt);
        if (dcr > 0)
            WriteCount(vals, pos, cnt - dcr);
    }
    void decrementCounter(Trace &trace, uint64 leafStart, uint64 decrement)
    {
        uint64 cumulLeft = leafStart;
        int size = trace.size();
        for (int n = trace.size()-1; n >= 0; --n) {
            BTreeNodePtr &node = trace.rgnp[n];
            char *vals = getVals(node);
            int nkeys = node.nkeys();
            int pos = trace.rgn[n];
            uint64 dcr = decrement;

            uint64 nodeLeft = 0;
            for (int k = 0; k < pos; ++k)
                nodeLeft += ReadCount(vals, k);
            // Each deletion is three part - partialL keys from node
            // then reclaimNodeChain, then partialR keys from node (which
            // is set to first non-wholly-deleted node) 
            // ----[---------- ---- ---- ---- ----------]----
            //       partialL       chain      partialR
            // partialL is implicit, i.e. we do not calculate it, we
            // operate with decrement instead
            uint64 partialR = 0;
            if (pos == 0 && cumulLeft == 0) {
                if (dcr >= countNodePairs(node))
                    // this node will be empty, so it belongs to chain
                    reclaimNodeChain(node, dcr, partialR);
                else
                    // everything happens inside this node
                    removeLeftKeys(node, dcr);
            } else {
                // this node is partially filled, so process it
                // and step right to next node if something left to delete
                char *vals = getVals(node);
                uint64 cnt;
                bool hasKeys;
                if (cumulLeft) {
                    cnt = ReadCount(vals, pos);
                    uint64 myDcr = min_(dcr, cnt - cumulLeft);
                    WriteCount(vals, pos, cnt - myDcr);
                    ++pos;
                    dcr -= myDcr;
                }
                // here pos is guaranteed to be > 0, because if cumulLeft > 0 it was
                // incremented in 'if' above, and if cumulLeft == 0 and pos == 0
                // it is handled in former branch
                // wipe out chain of entries with their right pointers
                while ((hasKeys = pos <= node.nkeys()) && (cnt = ReadCount(vals, pos)) <= dcr) {
                    if (node.nkeys() == 1) {
                        // create right dangling pointer here
                        WriteCount(vals, pos, 0);
                        WritePageNum(vals, pos, DanglingPageRef);
                        dcr -= cnt;
                        hasKeys = false;
                        break;
                    }
                    removeEntry(node, pos-1, 1, true);
                    dcr -= cnt;
                    vals = getVals(node);
                }
                if (dcr > 0) {
                    if (hasKeys && cnt > dcr) {
                        WriteCount(vals, pos, cnt - dcr);
                    } else {
                        node.mark();
                        LogPageNumT rpg = node.readRight();
                        if (rpg <= 0) throw FileStructureCorrupt(cpoint(__LINE__));
                        node.fetch(rpg);
                        reclaimNodeChain(node, dcr, partialR);
                    }
                }
            }
            if (partialR) removeLeftKeys(node, partialR);
            node.mark();
            cumulLeft += nodeLeft;
        }
    }
    uint64 countNodePairs(BTreeNodePtr &node, uint64 pos = InvalidPos)
    {
        uint64 cnt = 0;
        int nkeys = node.nkeys();
        const char *vals = node.data() + nkeys*keylen_;
        // number of child page refs is one more than
        // number of keys
        if (InvalidPos != pos) nkeys = pos;
        for (int i = 0; i <= nkeys; ++i)
            cnt += ReadCount(vals, i);
        return cnt;
    }
    uint64 countPairs(BTreeNodePtr &node, uint64 pos = InvalidPos)
    {
        if (node.level()) {
            return countNodePairs(node, pos);
        } else {
            if (InvalidPos != pos) {
                if (pos > node.nkeys()) throw FileStructureCorrupt(cpoint(__LINE__));
                return pos;
            } else {
                return node.nkeys();
            }
        }
    }
    // Find key inside the node.
    int searchNode (BTreeNodePtr &node, const void *key, LenT klen, const void *val, int match, bool fInsert)
    {
        const char *keys = node.data();
        const char *l = keys + node.nkeys()*keylen_;
		const char *f = findEntry (keys, node.nkeys(), key, klen, val, match);
        int index = (f-keys) / keylen_;
        if (!(flags_ & BTREE_FLAGS_RANGE)) {
            // index contains lower bound
            // if search is exact, check that we found the element
            // modified binary search test 
            if (!(match & BTREE_PARTIAL) && !fInsert) {
                if (index == node.nkeys()) {
                    // if we have no keys and not going to insert
                    // it means failure
                    if (0 == node.nkeys()) return -1;
                    // adjust found key
                    f -= keylen_;
					--index;
                }
                int res = memcmp (f, key, klen);
                if (res != 0) return -1;
            }
        }
        return index;
    }
    uint64 searchLeaf (BTreeNodePtr &node, const void *key, LenT klen, const void *val, int match, bool fInsert)
    {
        const char *keys = node.data();
		const char *f = findEntry (keys, node.nkeys(), key, klen, val, match);
        int index = (f-keys) / keylen_;
        if (match & BTREE_LAST) f -= keylen_;
        // index contains lower bound
        if (fInsert) {
            // Check here that place is not occupied
            if (index < node.nkeys()) {
                if (0 == keycomp(f, keylen_, key, keylen_)) {
                    if (!(flags_ & BTREE_FLAGS_DUPLICATE)) throw Duplicate();
                    if (0 == compareValue(node, index, val, vallen_)) throw Duplicate();
                }
            }
        } else {
            // Check we are not out of bounds
            if (index < 0 || index == node.nkeys()) throw NotFound();
            // if search is exact, check that we found the element
            if (0 != keycomp (f, keylen_, key, klen) != 0) throw NotFound();
            if (!(match & BTREE_PARTIAL) && (match & BTREE_BOTH)) {
                // Check that value also matches
                if (0 != compareValue(node, index, val, vallen_)) throw NotFound();
            }
        }
        return index;
    }
    bool enoughSpace(BTreeNodePtr &node)
    {
#if defined (_DEBUG) && defined (DEBUG_FREESPACE)
        uint32 freelen = DEBUG_FREESPACE - node.header()->len;
#else
        uint32 freelen = node.size() - node.header()->len - sizeof (BTreeNodeHeader);
#endif
        uint32 vallen = node.level() ? PageRefSize : payloadlen_;
        return freelen > keylen_ + vallen;
    }
    bool splitRoot(BTreeNodePtr &root)
    {
        BTreeNodePtr l(file_, nodesize_);
        BTreeNodePtr r(file_, nodesize_);
        if (!newNode (l) || !newNode (r)) return false;

        // Assign root's level to new nodes
        uint8 level = root.header()->level;
        l.header()->level = r.header()->level = level;

        // Sibling links
        l.writeRight(r.page());
        r.writeLeft(l.page());

        // Split root keyset in halves
        uint16 nkeys = root.nkeys();
#if defined (_DEBUG) && defined (TEST_VERBOSE)
        std::cerr << std::endl << "Splitting root at level " << (int) level << ", nkeys " << (int) nkeys << std::endl;
#endif
        char *rootkeys = root.data();
        uint16 median, rbegin, lnkeys, rnkeys;
        rbegin = lnkeys = nkeys / 2;
        if (flags_ & BTREE_FLAGS_RANGE) {
            median = lnkeys;
            if (level) ++rbegin;
        } else {
            median = lnkeys-1;
            if (level) --lnkeys;
        }
        rnkeys = nkeys-rbegin;
        copy (l, 0, root, 0, lnkeys);
        copy (r, 0, root, rbegin, rnkeys);
        // Adjust level
        root.header()->level = level+1;
        // Make median key the only key with pointers to two new pages
        memcpy(rootkeys, rootkeys+median*keylen_, keylen_);
        root.header()->nkeys = 1;
        // recalc rootvals for root with 1 key
        char *rootvals = rootkeys + keylen_; // rootkeys + 1*keylen_;
        WritePageNum(rootvals, 0, l.page());
        WritePageNum(rootvals, 1, r.page());
        WriteCount(rootvals, 0, countPairs(l));
        WriteCount(rootvals, 1, countPairs(r));
        // Recalc lengths
        setlen (l);
        setlen (r);
        setlen (root);
        // Mark for writing
        l.mark ();
        r.mark ();
        root.mark ();
        return true;
    }
    bool makeroom (BTreeNodePtr &parent, int nodePos,
        const void *key, LenT klen,
        BTreeNodePtr &node)
    {
        // Current node is full, we should try to make room
        // by first redistributing with neighbour, of if it
        // fails by splitting this node and its neighbour
        // into three nodes (one of them created anew).

        BTreeNodePtr nodeSib(file_, nodesize_);
        uint16 nkeys;
        nkeys = node.nkeys();

        // If this node is not rightmost for its parent we try right sibling
        // If this node is not leftmost for its parent we try left sibling
        LogPageNumT pgSib, pg = node.page ();
        LogPageNumT pgs[2];
        uint16 nkeysSib[2];
        int ntries = 0;
        if (nodePos < parent.nkeys()) {
            LogPageNumT pgSib = node.readRight();
            assert (pgSib > 0 && pgSib == refAt (parent, nodePos+1));
            pgs[ntries++] = pgSib;
        }
        if (nodePos > 0) {
            LogPageNumT pgSib = node.readLeft();
            assert (pgSib > 0 && pgSib == refAt (parent, nodePos-1));
            pgs[ntries++] = pgSib;
        }
        assert (ntries > 0);
        int n;
        bool found = false;
        for (n = 0; n < ntries; ++n) {
            nodeSib.fetch (pgs[n]);
            nkeysSib[n] = nodeSib.nkeys();
            // To be valid redist the difference should be
            // at least 2
            if (nkeysSib[n]+1 < nkeys) {
                pgSib = pgs[n];
                found = true;
                break;
            }
        }
        nodeSib.free ();
        LogPageNumT ref;
        if (found) {
            // Redistribute by rotating
            // Move half of difference to another node
            int count = (nkeys-nkeysSib[n])/2;
            if (pgSib == node.readLeft()) {
                // rotate left
                count = -count;
                --nodePos;
            }
            rotate (parent, nodePos, count);
            // Find the node where our key now
            // FIXME: change memcmp into keycomp
            if (memcmp (key, keyAt (parent, nodePos), klen) > 0) nodePos += 1;
            // else nodePos += 0;
        } else {
            // If both left and right sibling satisfy fullness condition
            // we prefer right one. Anyway, if we're here, it means that
            // neither sibling fit for redist, thus first one fits for
            // split for sure
            if (pgs[0] == node.readLeft()) --nodePos;
            uint32 nkeysSum = nkeys+nkeysSib[0];
            // First rotate creates new node and move
            // there 2/3 of keys (or 1/3 of sum) from middle node
            // It can fail due to new node creation failure
            rotate (parent, nodePos+1, nkeysSum/3, true);
            // This one just move 1/3 (or 1/6 of sum) from leftmost into
            // middle node, thus evenly distributing
            // two nodes into three.
            rotate (parent, nodePos, nkeysSum/6);
            // Find the node where our key now
            // FIXME: change memcmp into keycomp
            if (memcmp (key, keyAt (parent, nodePos+1), klen) > 0) nodePos += 2;
            else if (memcmp (key, keyAt (parent, nodePos), klen) > 0) nodePos += 1;
            // else nodePos += 0;
        }
        ref = refAt (parent, nodePos);
        node.fetch (ref);
        return true;
    }
    // helper for handleDanglingPageRef
    void newSibling(BTreeNodePtr &parent, int pos, BTreeNodePtr &sib, BTreeNodePtr &node)
    {
        newNode(node);
        LogPageNumT newPageNum = node.page();
        node.header()->level = parent.level()-1;
        if (pos) {
            // Right dangling pointer
            LogPageNumT rRef = sib.readRight();
            node.writeRight(rRef);
            node.writeLeft(sib.page());
            sib.writeRight(newPageNum);
            if (rRef > 0) {
                BTreeNodePtr rr(file_, nodesize_);
                rr.fetch (rRef);
                rr.writeLeft(newPageNum);
                rr.mark ();
            }
        } else {
            // Left dangling pointer
            LogPageNumT lRef = sib.readLeft();
            node.writeLeft(lRef);
            node.writeRight(sib.page());
            sib.writeLeft(newPageNum);
            if (lRef > 0) {
                BTreeNodePtr ll(file_, nodesize_);
                ll.fetch(lRef);
                ll.writeRight(newPageNum);
                ll.mark();
            }
        }

    }
    // patch DanglingPageRef by propagating it one level lower
    LogPageNumT handleDanglingPageRef(BTreeNodePtr &parent, int pos,
        const void *key, LenT klen, const void *val)
    {
        // Dangling pointer is only possible either to the left
        // of only key on a page (pos == 0) or to the right (pos == 1)
        assert (pos == 0 || pos == 1);
        BTreeNodePtr sib(file_, nodesize_); // sibling of possible new node
        // pos can be only 0 or 1, so 1-pos is the opposite, that is
        // the only sibling of the node we are going to insert
        sib.fetch(refAt(parent, 1-pos));
        if (sib.nkeys() > 1) {
            // go ahead inserting node
            BTreeNodePtr nn(file_, nodesize_); // new node
            newSibling(parent, pos, sib, nn);
            rotate(parent, 0, pos ? sib.nkeys() / 2 : -sib.nkeys() / 2);
            return nn.page();
        } else {
            // pointless to insert new node - can not rotate afterwards,
            // modify parent key into given key, so that search path goes
            // into existing 'sib' node
            // TEST_ME
            assert(1 == parent.nkeys());
            // code adapted from insertAt
            char *keys = parent.data();
            LenT keylen = !(flags_ & BTREE_FLAGS_DUPLICATE) ? keylen_ : keylen_ - PageRefSize;
            assert (keylen == klen);
            memcpy(keys, key, keylen);
            if (keylen < keylen_) {
                memcpy(keys+keylen, val, vallen_);
            }
            return sib.page();
        }
    }
    int insert (BTreeNodePtr &node, const void *key, LenT klen, const void *val)
    {
        uint16 nkeys = node.nkeys();
        char *keys = node.data();
        char *vals = keys + nkeys*keylen_;
        uint64 pos = searchLeaf (node, key, klen, val, BTREE_BOTH, true);
        int res = insertAt (node, pos, key, klen, val);
        if (BTREE_OK != res) return res;
        setlen (node);
        node.mark ();
        return BTREE_OK;
    }
    int insertRefAt(BTreeNodePtr &node, int pos, const void *key, LenT klen, LogPageNumT val, bool after=false)
    {
        PageRef r;
        WritePageNum (&r, 0, val);
        WriteCount(&r, 0, 0);
        return FixedNodeHandler::insertAt (node, pos, key, klen, &r, after);
    }
    // Delete 'count' key-value pairs from 'node' at 'pos'
    // 'right' defines whether right page reference should be deleted
    // instead of default left
    void removeEntry (BTreeNodePtr &node, uint16 pos, uint16 count, bool right=false)
    {
        BTreeNodeHeader *hdr = node.header();
        assert (pos < hdr->nkeys && pos+count <= hdr->nkeys);
        assert (!(0 == hdr->level) || !right);
        // Shortcuts
        uint32 vallen = hdr->level ? PageRefSize : payloadlen_;
        char *keys = hdr->data;
        char *vals = keys + hdr->nkeys*(uint32)keylen_;
        char *p = keys + pos*(uint32)keylen_;
        uint32 keyshift = count*(uint32)keylen_;
        uint32 valshift = keyshift + count*vallen;
        uint16 valPos = right ? pos+1 : pos;
        size_t len = (hdr->nkeys - (pos+count))*(uint32)keylen_ + valPos*vallen;
        memcpy (p, p + keyshift, len);
        p += len;
        len = (hdr->nkeys - (valPos+count))*vallen;
        if (hdr->level) len += vallen;
        memcpy (p, p+valshift, len);
        hdr->nkeys -= count;
        setlen (node);
        node.mark();
    }
    // This value is used internally, and actually is not length of the key,
    // but the length of structure, which is compact with the key, which is
    // moved together with the key, and can be compared like the key.
    // This is used for implementing duplicate key index by appending value
    // to the key.
    uint16 keylen_;
    // This is true value of key length
    uint16 truekeylen_;
    // Payload length. Payload is usually the value, but
    // in case of duplicate key it is empty.
    uint16 payloadlen_;
private:
    void *keyAt (BTreeNodePtr &node, uint16 pos)
    {
        if (pos >= node.nkeys()) return 0;
        return node.data()+pos*(uint32)keylen_;
    }
    // This function copies 'count' key-value pairs from nodeSrc.posSrc
    // to nodeDst.posDst, and if node is internal it copies extra value
    // (which is page ref) and if 'key' is non-null inserts 'key' before
    // or after the copied key-value range
    void copy (
        BTreeNodePtr &nodeDst, uint16 posDst,
        BTreeNodePtr &nodeSrc, uint16 posSrc,
        uint16 count,
        const void *key=0, bool after=false
        )
    {
        BTreeNodeHeader *hdrDst = nodeDst.header();
        BTreeNodeHeader *hdrSrc = nodeSrc.header();
        assert (hdrDst->level == hdrSrc->level);
        uint8 level = hdrDst->level;
        assert (!(0 == level) || 0 == key);
        assert (posDst <= hdrDst->nkeys);
        assert ((level ? posSrc <= hdrSrc->nkeys : posSrc < hdrSrc->nkeys) && posSrc+count <= hdrSrc->nkeys);
        // Some shortcuts
        uint16 nkDst = hdrDst->nkeys;
        uint16 nkSrc = hdrSrc->nkeys;
        uint32 vallen = level ? PageRefSize : payloadlen_;
        char *keysDst = hdrDst->data;
        char *valsDst = keysDst + nkDst*(uint32)keylen_;
        char *keysSrc = hdrSrc->data;
        char *valsSrc = keysSrc + nkSrc*(uint32)keylen_;
        //
        size_t len;
        char *p;
        uint32 valslen = vallen * (level ? count+1 : count);
        uint32 keyshift = (uint32)keylen_ * (level && key ? count+1 : count);
        uint32 valshift = keyshift + valslen;
        // make room for values
        uint16 posDstVal = level && key && !after ? posDst+1 : posDst;
        p = valsDst+posDstVal*vallen;
        if (posDstVal < nkDst) {
            len = (nkDst-posDstVal)*vallen;
            if (level) len += vallen;
            memmove(p+valshift, p, len);
        }
        // copy values
        memcpy(p+keyshift, valsSrc+posSrc*vallen, valslen);
        // make room for keys
        len = (nkDst-posDst)*(uint32)keylen_ + posDstVal*vallen;
        p = keysDst+posDst*(uint32)keylen_;
        if (len > 0) memmove(p+keyshift, p, len);
        // copy keys
        if (level && key && !after) p += keylen_;
        memcpy(p, keysSrc+posSrc*(uint32)keylen_, count*(uint32)keylen_);
        hdrDst->nkeys += count;
        if (hdrDst->level && key) {
            p = keysDst+posDst*(uint32)keylen_;
            if (after) p += count*(uint32) keylen_;
            memcpy (p, key, keylen_);
            hdrDst->nkeys++;
        }
        setlen (nodeDst);
    }
    //
    void rotate (BTreeNodePtr &parent, uint16 pos, int count, bool createNew=false)
    {
        assert (count != 0);
        BTreeNodePtr l(file_, nodesize_);
        BTreeNodePtr r(file_, nodesize_);
        uint8 level = parent.level() - 1;
        uint16 newpos;
        if (createNew) {
            if (count > 0) {
                // this can be reached ONLY from 2/3 split at the second position
                // of key pair, that is it should be strictly greater than first (0th)
                // position in parent node.
                assert (pos > 0);
                // make new node the right sibling
                newNode(r);
                l.fetch (refAt (parent, pos));
                r.header()->level = level;
                // insert new node at parent, the key is the same as
                // for previous node, which is safe if operation
                // will not complete
                insertRefAt (parent, pos, keyAt (parent, pos-1), keylen_, r.page(), true); // ref after the key
                // update sibling links ??? change to ShortPageRef - w/o count
                LogPageNumT rRef = l.readRight();
                r.writeRight(rRef);
                r.writeLeft(l.page());
                l.writeRight(r.page());
                if (rRef > 0) {
                    // This is performance hit, but we forced
                    // to update right neighbour's link
                    BTreeNodePtr rr(file_, nodesize_);
                    rr.fetch (rRef);
                    rr.writeLeft(r.page());
                    rr.mark ();
                }
                newpos = pos;
            } else {
                assert (false); // !!! to mark untested code
                // make new node the left sibling
                newNode (l);
                r.fetch (refAt (parent, pos));
                --pos;
            }
            setlen (parent);
        } else {
            l.fetch (refAt (parent, pos));
            r.fetch (refAt (parent, pos+1));
        }
        uint16 lnkeys = l.nkeys();
        if (count < 0) {
            // left rotate
            count = -count;
            if (level)
                // internal
                copy (l, lnkeys, r, 0, count-1, createNew ? 0 : keyAt (parent, pos), false); // before
            else
                // leaf
                copy (l, lnkeys, r, 0, count);
            if (flags_ & BTREE_FLAGS_RANGE)
                memcpy (keyAt (parent, pos), keyAt (r, level ? count-1 : count), keylen_);
            else
                memcpy (keyAt (parent, pos), keyAt (r, count-1), keylen_);
            removeEntry (r, 0, count, false); // left ref
        } else {
            // right rotate
            uint16 keyToParent = lnkeys-count;
            // at this point the situation is asymmetric, right rotate copy the last
            // key of left node, to provide correct search path, as well as left
            // rotate, but this means offset by one for leaf node.
            if (0 == level) --keyToParent;
            if (level)
                // internal
                copy (r, 0, l, lnkeys-(count-1), count-1, createNew ? 0 : keyAt (parent, pos), true); // after
            else
                // leaf
                copy (r, 0, l, lnkeys-count, count);
            if (flags_ & BTREE_FLAGS_RANGE)
                memcpy (keyAt (parent, pos), keyAt (l, lnkeys-count), keylen_);
            else
                memcpy (keyAt (parent, pos), keyAt (l, keyToParent), keylen_);
            removeEntry (l, lnkeys-count, count, level != 0); // right ref, for internal node
        }
        setlen (l);
        setlen (r);
        char *parentvals = parent.data() + parent.nkeys()*(uint32) keylen_;
        WriteCount(parentvals, pos, countPairs(l));
        WriteCount(parentvals, pos+1, countPairs(r));
        // mark nodes for writing
        l.mark();
        r.mark();
        parent.mark();
    }
} ; // FixedNodeHandler

class DupKeyNodeHandler : public FixedNodeHandler
{
public:
    DupKeyNodeHandler (
        BTreeFile &file,
        uint64 rootnodeoff,
        uint64 firstnodeoff,
        uint64 rootfreenodeoff,
        uint32 nodesize,
        uint32 rootnodesize,
        uint16 vallen,
        uint32 flags,
        uint16 keylen)
    :
        FixedNodeHandler (
            file,
            rootnodeoff,
            firstnodeoff,
            rootfreenodeoff,
            nodesize,
            rootnodesize,
            vallen,
            flags,
            keylen)
    {}
protected:
    void readValue (BTreeNodePtrBase &node, uint64 pos, void *val)
    {
        const BTreeNodeHeader *np = (const BTreeNodeHeader *) node.body ();
        assert (0 == np->level);
        if (pos >= np->nkeys) throw FileStructureCorrupt(cpoint(__LINE__));
        const char *p = np->data+pos*(uint32)keylen_;
        memcpy (val, p + keylen_ - vallen_, vallen_);
    }
    int compareValue(BTreeNodePtrBase &node, uint64 pos, const void *val, uint16 vlen)
    {
        const BTreeNodeHeader *np = (const BTreeNodeHeader *) node.body ();
        assert (0 == np->level);
        if (pos >= np->nkeys) throw FileStructureCorrupt(cpoint(__LINE__));
        const char *p = np->data+pos*(uint32)keylen_;
        return keycomp(p + keylen_ - vallen_, vallen_, val, vlen);
    }
    int searchNode (BTreeNodePtr &node, const void *key, LenT klen, const void *val, int match, bool fInsert)
    {
        const char *keys = node.data();
		assert (0 != node.level());
        if (fInsert)
            return FixedNodeHandler::searchNode (node, key, klen, val, match, true);
        // We need to censor 'match' a bit
        //   Search for leaf node implies BTREE_PARTIAL and BTREE_BOTH flags,
        // for duplicate key index this results in using keyvalcomp, which
        // first tests key against initial part of buf1, then if they are
        // equal, test the rest of buf1 against value. If you request partial
        // search in duplicate key index with query key length less than
        // actual key length for this index, it can lead to erroneous comparison
        // of key part of buf1 with value.
        //   Anyway this kind of search - partial on both key and value does
        // not make sense. That is why we censor match which we pass to
        // findEntry to exclude BTREE_BOTH if klen is less that actual keylen_.
        int m = match;
        if (klen < truekeylen_) m &= ~BTREE_BOTH;
		const char *f = findEntry (keys, node.nkeys (), key, klen, val, m);
		int index = (f-keys) / keylen_;
        // if search is exact, check that we found the element
        // modified binary search test 
        if (!(match & BTREE_PARTIAL)) {
            if (index == node.nkeys ()) return -1;
            if (memcmp (f, key, klen) != 0) return -1;
        } else if (index == node.nkeys ()) {
            // this condition can hold true only for inexact
            // search in leaf node
            return node.nkeys(); // position after the last key
        }
        return index;
    }
} ; // DupKeyNodeHandler

class RangeNodeHandler : public FixedNodeHandler
{
public:
    RangeNodeHandler (
        BTreeFile &file,
        uint64 rootnodeoff,
        uint64 firstnodeoff,
        uint64 rootfreenodeoff,
        uint32 nodesize,
        uint32 rootnodesize,
        uint16 vallen,
        uint32 flags,
        uint16 keylen)
    :
        FixedNodeHandler (
            file,
            rootnodeoff,
            firstnodeoff,
            rootfreenodeoff,
            nodesize,
            rootnodesize,
            vallen,
            flags,
            keylen)
    {
        payloadlen_ += sizeof (RangeLenT);
    }
protected:
    void readValue (BTreeNodePtrBase &node, uint64 pos, void *val)
    {
        const BTreeNodeHeader *np = (const BTreeNodeHeader *) node.body ();
        uint32 entry = getEntry (pos);
        RangeLenT dist = getDist (pos);
        if (entry >= np->nkeys) throw FileStructureCorrupt(cpoint(__LINE__));
        const uint8 *vals = (uint8 *) np->data+np->nkeys*keylen_;
        const uint8 *beg = vals + entry*(vallen_ + sizeof (RangeLenT));
        RangeLenT len = *((RangeLenT *) beg);
        beg += sizeof (RangeLenT);
        // keys in range index are numbers in Most Significant Byte
        // first ('MSB first' or MSB) order to provide adjacency of
        // near keys in index
        if (dist >= len) throw FileStructureCorrupt(cpoint(__LINE__));
        msbadd ((uint8 *) val, beg, dist, vallen_);
    }
    int compareValue(BTreeNodePtrBase &node, uint64 pos, const void *val, uint16 vlen)
    {
        void *val1 = alloca (vallen_);
        readValue(node, pos, val1);
        return keycomp(val1, vallen_, val, vlen);
    }
    uint64 searchLeaf (BTreeNodePtr &node, const void *key, LenT klen, const void *val, int match, bool fInsert)
    {
        const char *keys = node.data();
		const char *f = findEntry (keys, node.nkeys (), key, klen, val, match);
        int entry = (f-keys) / keylen_;
        // index contains upper bound
        if (fInsert) return makePos (entry, 0); // for insert precise position is irrelevant
        // modified binary search test
        if (entry == 0) throw NotFound();
        if (entry == node.nkeys ()) {
            // ??? use keycomp - may be not because range can work only
            // with memcmp as keycomp
            int res = memcmp (f-keylen_, key, klen);
            if (res > 0) throw NotFound();
        }
		// upper_bound-1
		--entry; f -= keylen_;
		RangeLenT dist = msbdist ((uint8 *) key, (uint8 *) f, keylen_);
        if (dist >= getEntryLength(node, entry)) throw NotFound();
        return makePos (entry, dist);
    }
    uint64 searchLeafByCount(BTreeNodePtr &node, uint64 count)
    {
        throw NotFound();
        uint64 cumul = 0;
        char *vals = getVals(node);
        int n = 0;
        int size = node.nkeys();
        uint32 len;
        while (n < size && cumul + (len = getEntryLength(node, n)) < count) {
            cumul += len; ++n;
        }
        if (n == size) throw NotFound();
        return makePos(n, count - cumul);
    }
    bool done (BTreeCursor &cur)
    {
        BTreeNodeHeader *node = (BTreeNodeHeader *)cur.body();
        const void *key = node->data+getEntry(cur.pos_)*(uint32)keylen_;
        void *val = alloca (vallen_);
        readValue(cur, cur.pos_, val);
        return cur.qry_->done(key, keylen_, val, vallen_);
    }
    bool stepCursor (BTreeCursor &cur, bool remove) {
        uint32 entry = getEntry(cur.pos_);
        RangeLenT dist = getDist(cur.pos_);
        BTreeNodePtr &node = *(BTreeNodePtr *)&cur;
        if (remove) {
            RangeLenT entryLen;
            if (dist == 0) {
                // at the beginning - increment key, decrement length, increment value
                uint8 *keys = (uint8 *) node.data();
                msbincr(keys + entry*(uint32) keylen_, keylen_);
                RangeLenT *plen = (RangeLenT *) (getVals(node) + entry*(vallen_ + sizeof (RangeLenT)));
                --(*plen);
                msbincr((uint8 *) (plen+1), vallen_);
            } else if (dist == (entryLen = getEntryLength(node, entry)) - 1) {
                // at the end - decrement length
                RangeLenT *plen = (RangeLenT *) (getVals(node) + entry*(vallen_ + sizeof (RangeLenT)));
                --(*plen);
            } else {
                // in the middle - split entry
                uint8 *key = ((uint8 *) node.data()) + entry*(uint32) keylen_;
                uint8 *val = (uint8 *) getVals(node) + entry*(vallen_ + sizeof (RangeLenT));
                RangeLenT len = *((RangeLenT *) val);
                uint8 *newkey = (uint8 *) alloca(keylen_);
                uint8 *newval = (uint8 *) alloca (sizeof(RangeLenT) + vallen_);
                if (len <= dist) throw FileStructureCorrupt(cpoint(__LINE__));
                *((RangeLenT *) val) = dist;
                msbadd(newkey, key, dist+1, keylen_);
                msbadd ((uint8 *)newval+sizeof(RangeLenT), val+sizeof(RangeLenT), dist+1, vallen_);
                *((RangeLenT *) newval) = len-dist-1;
                ++entry;
                FixedNodeHandler::insertAt(node, entry, newkey, keylen_, newval);
                dist = 0;
            }
            if (0 == getEntryLength(node, entry))
                removeEntry(node, entry, 1);
            --dist;
        }
        if (cur.qry_->stepForward()) {
            ++dist;
            if (dist >= getEntryLength(node, entry)) {
                ++entry;
                if (entry >= node.nkeys()) {
                    LogPageNumT pg = node.readRight();
                    if (0 >= pg) {
                        cur.free ();
                        return false;
                    }
                    if (remove && 0 == node.nkeys()) {
                        // remove leaf node here
                        reclaimNode(node);
                    }
                    try { cur.fetch (pg); }
                    catch (Error &) {
                        cur.free ();
                        throw FileStructureCorrupt(cpoint(__LINE__));
                    }
                    entry = 0;
                }
                dist = 0;
            }
        } else {
            if (dist < 1) {
                if (entry < 1) {
                    LogPageNumT pg = node.readLeft();
                    if (0 >= pg) {
                        cur.free ();
                        return false;
                    }
                    try { cur.fetch (pg); }
                    catch (Error &) {
                        cur.free ();
                        throw FileStructureCorrupt(cpoint(__LINE__));
                    }
                    entry = ((BTreeNodePtr *) &cur)->nkeys() - 1;
                } else {
                    --entry;
                }
                dist = getEntryLength(*((BTreeNodePtr *) &cur), entry);
            } else {
                --dist;
            }
        }
        cur.pos_ = makePos(entry, dist);
        return true;
    }
    uint64 countPairs(BTreeNodePtr &node, uint64 pos)
    {
        if (node.level()) return countNodePairs(node, pos);
        uint64 cnt = 0;
        uint16 nkeys;
        if (InvalidPos != pos) {
            uint32 entry = getEntry(pos);
            if (entry > node.nkeys()) throw FileStructureCorrupt(cpoint(__LINE__));
            nkeys = entry;
        } else {
            nkeys = node.nkeys();
        }
        const uint8 *pval = (uint8 *) getVals(node);
        for (uint16 n = 0; n < nkeys; ++n) {
            cnt += *((RangeLenT *) pval);
            pval += vallen_ + sizeof (RangeLenT);
        }
        if (InvalidPos != pos) cnt += getDist(pos);
        return cnt;
    }
    int insertAt (BTreeNodePtr &node, int pos, const void *key, LenT klen, const void *val, bool after)
    {
        const void *srcVal = val;
        if (0 == node.level()) {
            if (node.nkeys()) {
                int pos1 = pos;
                if (pos1 > 0) --pos1;
                // calc diff between given key and key at pos in MSB
                uint8 *keys = (uint8 *) node.data();
                uint32 dist = msbdist ((uint8 *) key, keys + pos1*(uint32) keylen_, keylen_);
                uint8 *vals = (uint8 *) getVals(node);
                const uint8 *beg = vals + pos1*(vallen_ + sizeof (RangeLenT));
                RangeLenT *plen = (RangeLenT *) beg;
                beg += sizeof (RangeLenT);
                if (dist < *plen) return BTREE_DUPLICATE;
                if (dist == *plen &&
                    dist < kMaxDist &&
                    dist == msbdist ((uint8 *) val, beg, vallen_)) {
                    // FIXME: optimize: check that it does not overlap with right neighbour
                    // We can safely do not check for this, and do not merge adjacent
                    // ranges, because the index will correctly work even without this
                    // merge, though with slight performance impact.
                    ++*plen;
                    return BTREE_OK;
                }
            }
            void *scratchval = alloca (sizeof(RangeLenT) + vallen_);
            *((RangeLenT *) scratchval) = 1;
            memcpy ((uint8 *)scratchval+sizeof(RangeLenT), val, vallen_);
            srcVal = scratchval;
        }
        return FixedNodeHandler::insertAt (node, pos, key, klen, srcVal, after);
    }
private:
    // This function returns distance between two numbers of same length
    // or kMaxDist if it is >= 2**32-1
    enum { kMaxDist = RangeLenT(-1L) };
    RangeLenT msbdist (const uint8 *p1, const uint8 *p2, uint16 len)
    {
        RangeLenT d = 0;
        while (len-- > 0) {
            if (d > (RangeLenT(kMaxDist) >> 8)) return kMaxDist;
            d = (d << 8) + *p1++ - *p2++;
        }
        return d;
    }
    // Add number pointed by pa1 of length len to a2 and place result
    // into r, which should be of length len
    void msbadd (uint8 *r, const uint8 *pa1, RangeLenT a2, uint16 len)
    {
        uint16 res = 0;
        while (len-- > 0) {
            res += pa1[len] + (uint8) a2;
            r[len] = (uint8) res;
            res >>= 8;
            a2  >>= 8;
        }
    }
    void msbincr(uint8 *r, uint16 len)
    {
        uint16 res = 1;
        while (len-- > 0) {
            res += r[len];
            r[len] = (uint8) res;
            res >>= 8;
        }
    }
	uint64 makePos (uint32 entry, uint32 dist)
	{
		return ((uint64) dist << sizeof (uint32) * 8) | (uint64) entry;
	}
	uint32 getEntry (uint64 pos)
	{
        return pos & (uint32) -1;
	}
	uint32 getDist (uint64 pos)
	{
        return pos >> sizeof (uint32) * 8;
	}
    uint32 getEntryLength(BTreeNodePtr &node, int entry)
    {
        if (entry >= node.nkeys()) return 0;
        const uint8 *vals = (uint8 *) getVals(node);
        const uint8 *beg = vals + entry*(vallen_ + sizeof (RangeLenT));
        return *((RangeLenT *) beg);
    }
} ; // RangeNodeHandler


/////////////////////////////////////////////////////////////////////
// Private methods
/////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////
void BTree::assignHandler ()
{
    if (flags_ & BTREE_FLAGS_RANGE) {
        // Range index - fixed size, non-duplicate key
        handler_ = new RangeNodeHandler (*file_,
            rootnodeoff_,
            firstnodeoff_,
            rootfreenodeoff_,
            nodesize_,
            rootnodesize_,
            vallen_,
            flags_,
            keylen_);
    } else {
        // Non-range index
        if (!(flags_ & BTREE_FLAGS_VARKEY)) {
            if (!(flags_ & BTREE_FLAGS_DUPLICATE)) {
                // Fixed key index
                handler_ = new FixedNodeHandler (*file_,
                    rootnodeoff_,
                    firstnodeoff_,
                    rootfreenodeoff_,
                    nodesize_,
                    rootnodesize_,
                    vallen_,
                    flags_,
                    keylen_);
            } else {
                // Fixed key index
                handler_ = new DupKeyNodeHandler (*file_,
                    rootnodeoff_,
                    firstnodeoff_,
                    rootfreenodeoff_,
                    nodesize_,
                    rootnodesize_,
                    vallen_,
                    flags_,
                    keylen_);
            }
        } else {
            // Variable key index
#if 0
            handler_ = new VariableNodeHandler (*file_,
                rootnodeoff_,
                firstnodeoff_,
                rootfreenodeoff_,
                nodesize_,
                rootnodesize_,
                vallen_,
                flags_);
#endif
        }
    }
}

/////////////////////////////////////////////////////////////////////
void BTree::prepareMasterPage (BTreeMasterPage &mp)
{
    memset (&mp, 0, BTREE_MINPAGESIZE);
    memcpy (mp.signature, BTREE_SIGNATURE, sizeof (mp.signature));
    memcpy (mp.byteorder,
        nativeByteOrder_ ? BTREE_NATIVE_BYTEORDER : BTREE_FOREIGN_BYTEORDER,
        sizeof (mp.byteorder));
    mp.version      = CCBO32(nativeByteOrder_, BTREE_VERSION);
    mp.rootnodeoff  = CCBO64(nativeByteOrder_, rootnodeoff_);
    mp.firstnodeoff = CCBO64(nativeByteOrder_, firstnodeoff_);
    mp.rootfreenodeoff = CCBO64(nativeByteOrder_, rootfreenodeoff_);
    mp.flags        = CCBO32(nativeByteOrder_, flags_);
    mp.nodesize     = CCBO32(nativeByteOrder_, nodesize_);
    mp.keylen       = CCBO16(nativeByteOrder_, keylen_);
    mp.vallen       = CCBO16(nativeByteOrder_, vallen_);
}

/////////////////////////////////////////////////////////////////////
void BTree::prepareNodePage (BTreeNodeHeader &np, uint32 nodesize)
{
    memset (&np, 0, nodesize);
    memcpy (np.signature, BTREE_NODE_SIGNATURE, sizeof (np.signature));
    // Kind of excessive (see memset above), but for clarity
    WritePageNum(&np.left, 0, 0);
    WritePageNum(&np.right, 0, 0);
    np.level = 0;
    np.flags = 0;
    np.nkeys = 0;
}

/////////////////////////////////////////////////////////////////////
// Public interface
/////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////
BTree::BTree ()
:
handler_ (0),
file_ (0)
{
}

BTree::~BTree ()
{
    detach ();
}

/////////////////////////////////////////////////////////////////////
bool BTree::attach (BTreeFile &file)
{
    checkpoint_ = 0;
    if (handler_) detach ();
    file_ = &file;
    // Read master record and verify it
    BTreeMasterPage *mp =
        (BTreeMasterPage *) file_->fetch (0, 1);
    if (memcmp (mp->signature,
                BTREE_SIGNATURE,
                sizeof (mp->signature)) != 0)
        return false;
    ++checkpoint_;
    if (memcmp (mp->byteorder,
                BTREE_NATIVE_BYTEORDER,
                sizeof (mp->byteorder)) != 0)
        return false; // we do not support alien byteorder
    ++checkpoint_;
    // Load parameters of index from master record
    nativeByteOrder_ = true;
    if (CCBO32(nativeByteOrder_, mp->version) != BTREE_VERSION)
        return false;
    ++checkpoint_;
    rootnodeoff_ = CCBO64(nativeByteOrder_, mp->rootnodeoff);
    firstnodeoff_ = CCBO64(nativeByteOrder_, mp->firstnodeoff);
    rootfreenodeoff_ = CCBO64(nativeByteOrder_, mp->rootfreenodeoff);
    nodesize_ = CCBO32(nativeByteOrder_, mp->nodesize);
    pagemult_ = nodesize_ / BTREE_MINPAGESIZE;
    // Check that pagesize of pager is compatible with node size
    if (nodesize_ != file_->getPageSize ()) return false;
    ++checkpoint_;
    // Sanity check - first non-root page should be on nodesize boundary
    if (0 != firstnodeoff_ % nodesize_) return false;
    ++checkpoint_;
    // Knuth B*-tree root page contains 2*floor ((2*m-2)/3) keys
    rootnodesize_ = uint32 (ceil (4.0/3.0 * double (pagemult_))) * BTREE_MINPAGESIZE;
    assert (rootnodeoff_+rootnodesize_ <= 2*nodesize_);
    keylen_ = CCBO16(nativeByteOrder_, mp->keylen);
    flags_  = CCBO32(nativeByteOrder_, mp->flags);
    vallen_ = CCBO16(nativeByteOrder_, mp->vallen);
    // Sanity check - range should have fixed unique key
    if ((flags_ & BTREE_FLAGS_RANGE)
      && ((flags_ & BTREE_FLAGS_VARKEY)
        || (flags_ & BTREE_FLAGS_DUPLICATE))) return false;
    ++checkpoint_;
    assignHandler ();
#if 0 // We now just check that pagesize is compatible with the rest of system
    // At last we can set page size
    file_->setPageSize (nodesize_);
#endif
    // Check root page
    // !!!
    return true;
}

/////////////////////////////////////////////////////////////////////
bool BTree::init (BTreeFile &file,
                  LenT vallen,
                  //uint16 pagemult,
                  uint32 flags,
                  LenT keylen)
{
    checkpoint_ = 0;
    if (handler_) detach ();
    // Sanity check - range should have fixed non-duplicate key
    if ((flags & BTREE_FLAGS_RANGE)
      && ((flags & BTREE_FLAGS_VARKEY)
        || (flags & BTREE_FLAGS_DUPLICATE))) return false;
    ++checkpoint_;
    file_ = &file;
    nativeByteOrder_ = true;
    nodesize_ = file_->getPageSize ();
    // can not work if page is not multiple of BTREE_MINPAGESIZE.
    // ??? artificial limitation
    if (0 != nodesize_ % BTREE_MINPAGESIZE) return false;
    ++checkpoint_;
    pagemult_ = nodesize_ / BTREE_MINPAGESIZE;
    // See comment in 'attach' above
    rootnodesize_ = uint32 (ceil (4.0/3.0 * double (pagemult_))) * BTREE_MINPAGESIZE;
    rootnodeoff_  = BTREE_MINPAGESIZE;
    firstnodeoff_ = rootnodeoff_ + rootnodesize_;
    // Provide that first non-root page is at the nodesize boundary
    firstnodeoff_ = (firstnodeoff_+nodesize_/2) / nodesize_;
    firstnodeoff_ *= nodesize_;
    // Not used yet
    rootfreenodeoff_ = 0L;
    // Behavioral parameters of index
    keylen_ = keylen;
    flags_ = flags;
    vallen_ = vallen;
    // We have enough info here to assign node handlers
    assignHandler ();
    // Truncate file
    file_->chsize (0);
    // We allocate space for master record and root node
    // in one stroke, and they should occupy no more
    // than 2 pages of nodesize_ length
    assert (rootnodeoff_+rootnodesize_ <= 2*nodesize_);
    char *p = (char *) file_->fake (0, 2);
    // Create master record for file
    BTreeMasterPage *mp = (BTreeMasterPage *) p;
    prepareMasterPage (*mp);
    // Write empty root page
    BTreeNodeHeader *root = (BTreeNodeHeader *) (p+rootnodeoff_);
    prepareNodePage (*root, rootnodesize_);
    // Flush master record and root node
    file_->mark (p);
    return file_->flush ();
}

/////////////////////////////////////////////////////////////////////
bool BTree::detach ()
{
    bool res = this->flush ();
    file_ = 0;

    delete handler_;
    handler_ = 0;
    return res;
}

/////////////////////////////////////////////////////////////////////
bool BTree::flush  ()
{
    if (!file_) return true;
    return file_->flush ();
}

/////////////////////////////////////////////////////////////////////
// Insert new entry
void BTree::insert (const void *key, LenT len, const void *val, LenT vlen)
{
    // FIXME: handler::insert still returns error code
    handler_->insert (key, len, val);
}

/////////////////////////////////////////////////////////////////////
// Start the query
void BTree::initcursor (BTreeCursor &cur, BTreeQueryBase &qry, uint64 pos)
{
    if (!(flags_ & BTREE_FLAGS_VARKEY) && qry.len() > keylen_) throw BadParameters(cpoint(__LINE__));
    // Emulate BTreeNodePtrBase constructor, blocked by BTreeCursor's redefinition
    cur.file_   = file_;
    cur.dfltnodesize_ = nodesize_;
    //
    cur.pos_ = pos;
    // Cursor's fields
    cur.qry_    = &qry;
    cur.fInit_  = true;
}

/////////////////////////////////////////////////////////////////////
// Fetch value only by cursor
bool BTree::fetch (BTreeCursor &cur, void *val, LenT &vlen)
{
    // Discard key and its length
    const void *key;
    LenT len = 0;
    return fetch (cur, key, len, val, vlen);
}

/////////////////////////////////////////////////////////////////////
// Fetch key and value by cursor
bool BTree::fetch (BTreeCursor &cur, const void *&key, LenT &len, void *val, LenT &vlen)
{
    if (!cur.fInit_ || vlen < vallen_) throw BadParameters(cpoint(__LINE__, cur.fInit_ ? "vlen < vallen_" : "cursor not initialized"));
    vlen = vallen_;
    return handler_->fetch (cur, key, len, val);
}

/////////////////////////////////////////////////////////////////////
// Remove all key-value pairs, satisfying the query
uint64 BTree::remove(BTreeCursor &cur)
{
    if (!cur.fInit_) throw BadParameters(cpoint(__LINE__));
    return handler_->remove(cur);
}

uint64 BTree::getpos(const void *key, LenT len, const void *val, LenT vlen, int match)
{
    return handler_->getpos(key, len, val, vlen, match);
}

/////////////////////////////////////////////////////////////////////
void BTree::find (const void *key, LenT len, void *val, LenT &vlen, int match)
{
    BTreeQueryBase qry(match, true, key, len, val, vlen);
    BTreeCursor cur;
    initcursor(cur, qry);
    if (!fetch(cur, val, vlen)) throw NotFound();
}

/////////////////////////////////////////////////////////////////////
// BTreeCursor
/////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////
BTreeCursor::BTreeCursor ()
    :
    BTreeNodePtrBase (0, 0),
    qry_    (0),
    fInit_  (false)
{
}

/////////////////////////////////////////////////////////////////////
BTreeCursor::~BTreeCursor ()
{
}

} ; // namespace edb
