// This is low level interface to B-tree index.
//
// This interface relies on no-copy, no-allocation
// strategy. It means it never copies anything into
// its own objects, never copies anything to user-provided
// variables, except pointers to memory and never passes
// newly allocated memory outside.
// All this is done to provide performance, but it comes
// with some downsides. The user of the interface can not
// rely on stable memory after next call to the functions
// of this interface (and, generally, the whole edb package).
// If you need something to survive call to edb, copy this
// yourself.
// The module relies on caching level to manage its memory needs.
// So in multi-threaded setup to provide user with guarantee
// that memory does not change between calls, this should be
// somehow reflected in underlying caching interface.
//
// Copyright 2001 Victor Joukov, Denis Kaznadzey

#ifndef edbBTree_h
#define edbBTree_h


#include "edbTypes.h"
#include "edbExceptions.h"

// TODO: Parameterize over PagedFile
#include "edbPagedFile.h"
#define BTreeFile PagedFile

namespace edb
{

/////////////////////////////////////////////////////////////////////
// Const


// Result codes for find/insert/remove
// Success
const int BTREE_OK         = 0;
// No such key or key/value in index
const int BTREE_NOTFOUND   = 1;
// Try to insert already existing key in unique, or key/value in
// duplicate index
const int BTREE_DUPLICATE  = 2;
// For cursor operations - signals that no more answers found
const int BTREE_NOMORE     = 3;
// Hard errors
// Index may be broken - can not happen if everything is OK
const int BTREE_ERR_BROKEN = -1;
// Some parameter is invalid
const int BTREE_ERR_PARAM  = -2;
// I was so lazy, or just had no chance to work on this ;-)
const int BTREE_ERR_NOTIMP = -10000;

// Options for search query
// Match types for query
const int BTREE_EXACT = 0;
// Partial match
const int BTREE_PARTIAL = 1;
// Match by value as well
const int BTREE_BOTH = 2;
// Search the latest satisfying key
const int BTREE_LAST = 4;
// Adding entry here, do not forget to change
// internal BTREE_MATCH_MASK in the edbBtree.cpp file.

/////////////////////////////////////////////////////////////////////
// BTree variant flags - some ORed combinations may have sense
// Unique is just not duplicate ;-)
const uint32 BTREE_FLAGS_UNIQUE = 0x00000000;
// Does this index allow for duplicate keys
const uint32 BTREE_FLAGS_DUPLICATE = 0x00000001;
// Does it allow variable length key
const uint32 BTREE_FLAGS_VARKEY = 0x00000002;
// Does it support key range
const uint32 BTREE_FLAGS_RANGE  = 0x00000004;

// B-tree parameters
const uint32 BTREE_MINPAGESIZE = 1024;
const uint16 BTREE_DEFAULTPAGEMULT = 8;

// Logical page number
typedef uint64 LogPageNumT;

typedef uint16 LenT;
typedef uint64 ValueT;

class BTree;
class BTreeNodeHandler;

// Query is a three fold object, 1. giving
// start key/value, 2. what direction to move in at each step
// 3. satisfaction criteria - when query is done
class BTreeQueryBase
{
public:
    BTreeQueryBase (int match, bool forward, const void *key, LenT len) :
        match_(match),
        forward_(forward),
        key_(key),
        len_(len),
        val_(0),
        vlen_(0)
    {}
    BTreeQueryBase (int match, bool forward, const void *key, LenT len, const void *val, LenT vlen) :
        match_(match),
        forward_(forward),
        key_(key),
        len_(len),
        val_(val),
        vlen_(vlen)
    {}
    // this function is called before each fetch to find the direction of movement
    bool stepForward () { return forward_; }
    // this function is called after each fetch to find the limit of movement
    virtual bool done (const void *key, LenT len, const void *val, LenT vlen)
    {
        // One element
        return true;
    }
protected:
    friend BTreeNodeHandler; // remote friendship!
    friend BTree;
    int len () const { return len_; }
    int match_;
    bool forward_;
    const void *key_;
    LenT len_;
    const void *val_;
    LenT vlen_;
} ; // class BTreeQueryBase

// Base class for NodePtr and Cursor
// Auxiliary object for handling nodes in memory
// Actually, it is smart pointer, tracing locking
// of pages automatically
class BTreeNodePtrBase
{
public:
    BTreeNodePtrBase (BTreeFile *file, uint32 nodesize)
        :
        file_ (file),
        pageno_ ((LogPageNumT) -1),
        ptr_ (0),
        dfltnodesize_ (nodesize),
        data_ (0)
    {
    }
    virtual ~BTreeNodePtrBase ()
    {
        free ();
    }
    void fetch (LogPageNumT pageno, uint32 count=1, uint32 offset=0, uint32 nodesize=0)
    {
        free ();
        ptr_ = (char *) file_->fetch (pageno, count, true);
        data_ = ptr_ + offset;
        pageno_ = pageno;
        if (0 == nodesize) nodesize_ = dfltnodesize_;
        else nodesize_ = nodesize;
    }
    void fake (LogPageNumT pageno)
    {
        free ();
        ptr_ = (char *) file_->fake (pageno, 1, true);
        data_ = ptr_;
        pageno_ = pageno;
        nodesize_ = dfltnodesize_;
    }
    void free () { if (file_ && ptr_) file_->unlock (ptr_); clear (); }
    void clear () { ptr_ = 0; data_ = 0; }
    void mark () { if (ptr_) file_->mark (ptr_); }
    // linear assignment - all data are passed to target, and node
    // cleared
    void assign (BTreeNodePtrBase &node)
    {
        free ();
        file_     = node.file_;
        pageno_   = node.page();
        ptr_      = node.ptr();
        data_     = node.data_;
        nodesize_ = node.size();
        dfltnodesize_ = node.dfltnodesize_;
        node.clear ();
    }
    LogPageNumT page () { return pageno_; }
    char *ptr () { return ptr_; }
    const char *body () const { return data_; }
    bool valid () { return 0 != ptr_; }
    uint32 size () const { return nodesize_; }
protected:
    BTreeFile *file_;
    LogPageNumT pageno_;
    char *ptr_;
    char *data_;
    uint32 dfltnodesize_;
    uint32 nodesize_;
} ; // class BTreeNodePtrBase

class BTreeCursor : public BTreeNodePtrBase
{
public:
    BTreeCursor ();
    ~BTreeCursor ();
//protected:
    friend BTree;
    friend BTreeNodeHandler; // remote friendship!
    uint64          pos_;
    BTreeQueryBase *qry_;
    bool            fInit_;
private:
    BTreeCursor (const BTreeCursor &);
    BTreeCursor &operator= (const BTreeCursor &);
} ; // class BTreeCursor 


struct BTreeNodeHeader;
struct BTreeMasterPage;
class BTree
{
public:
    BTree ();
    ~BTree ();
    // Attach existing opened index file to this interface
    bool attach (BTreeFile &file);
    // Init existing opened file, attaching it to this interface
    bool init (BTreeFile &file,
        LenT   vallen,
        //uint16 pagemult=BTREE_DEFAULTPAGEMULT, // how large is the page in 1024 byte chunks
        uint32 flags=BTREE_FLAGS_VARKEY,
        LenT   keylen=0);
    // Detach file, flushing all pending buffers
    bool detach ();
    // Flush memory structures to file
    bool flush  ();
    // Insert new entry
    void insert (const void *key, LenT len, const void *val, LenT vlen);
    // Start the query. If pos is absent - use qry for finding first element
    // otherwise, use pos directly for positioning, and qry only for determining
    // step direction and end of query.
    void initcursor (BTreeCursor &cur, BTreeQueryBase &qry, uint64 pos = (uint64) -1);
    // Fetch value only by cursor
    bool fetch (BTreeCursor &cur, void *val, /*inout*/ LenT &vlen);
    // Fetch key and value by cursor
    bool fetch (BTreeCursor &cur, const void *&key, /*inout*/ LenT &klen, void *val, /*inout*/ LenT &vlen);
    // Remove all key-value pairs, satisfying the query
    uint64 remove (BTreeCursor &cur);
	// return position for entry, designated by cursor
	uint64 getpos (const void *key, LenT len, const void *val, LenT vlen, int match);
    //
    //
    // Obsolete but still handy
    // Find entry with given key
    void find (const void *key, LenT len, void *val, LenT &vlen, int match=BTREE_EXACT);
    // internal debug method
    int getCheckPoint () { return checkpoint_; }

	// some info
	int keySize () const { return keylen_; }
	int valSize () const { return vallen_; }
	bool isUnique () const { return 0 == (flags_ & BTREE_FLAGS_DUPLICATE); }
	bool isVarkey () const { return 0 != (flags_ & BTREE_FLAGS_VARKEY); }
	bool isRange () const  { return 0 != (flags_ & BTREE_FLAGS_RANGE); }

	BTreeFile* getFile () { return file_; }

protected:
    BTreeNodeHandler *handler_; // handler for non-root pages
    BTreeFile *file_;        // underlying file for index
    bool nativeByteOrder_;   // future extension - can operate foreign byteorder
    uint64 rootnodeoff_;     // offset of the root node
    uint64 firstnodeoff_;    // offset of first non-root node
    uint64 rootfreenodeoff_; // offset of root node of free space
    uint16 pagemult_;        // size of non-root node in BTREE_MINPAGESIZE pages
    // Parameters, derived from pagemult_
    uint32 nodesize_;        // BTREE_MINPAGESIZE * pagemult_
    uint32 rootnodesize_;    // BTREE_MINPAGESIZE * ceil (4/3 * pagemult_)
    // Behavioral parameters of index - range and unique
    uint32 flags_;
    LenT keylen_;            // length of the key
    LenT vallen_;            // length of payload value
    int  checkpoint_;        // for error tracing
private:
    void assignHandler ();
    void prepareMasterPage (BTreeMasterPage &mp);
    void prepareNodePage (BTreeNodeHeader &np, uint32 nodesize);
} ; // class BTree

} ; // namespace edb

#endif /* edbBTree_h */
