//////////////////////////////////////////////////////////////////////////////
//// This software module is developed by SciDM (Scientific Data Management) in 1998-2015
//// 
//// This program is free software; you can redistribute, reuse,
//// or modify it with no restriction, under the terms of the MIT License.
//// 
//// This program is distributed in the hope that it will be useful,
//// but WITHOUT ANY WARRANTY; without even the implied warranty of
//// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//// 
//// For any questions please contact Denis Kaznadzey at dkaznadzey@yahoo.com
//////////////////////////////////////////////////////////////////////////////

#include "edbBTree.h"

#include <time.h>
//#include <stdio.h>
#include <list>
#include "edbSplitFileFactory.h"
#include "edbPagedFileFactory.h"
#include "portability.h"
#include <cstring>
#include <iostream>


#define TSTDIR "."
#define TSTNAME "idx"
#define TSTNAME2 "idx2"

using namespace edb;

// Test parameters
//#define TEST_HARD
#if defined(TEST_HARD) || defined (_DEBUG)
const uint64 lim = 600L;
const uint64 report = 200000L;
const uint64 kOverstep = 5;
#else
const uint64 lim = 1000000L;
const uint64 report = 200000L;
const uint64 kOverstep = 1000;
#endif // TEST_HARD

//#define TEST_SCATTER
#ifndef TEST_SCATTER
inline uint64 msb64(uint64 v)
{
    uint64 r = 0;
    uint8 *p = (uint8 *) &r;
    for (int n = 64-8; n >= 0; n -= 8)
        *p++ = 0xff & (v >> n);
    return r;
}

inline uint64 msb64(const char *pv)
{
    uint64 v = *(uint64 *)pv;
    uint64 r = 0;
    uint8 *p = (uint8 *) &r;
    for (int n = 64-8; n >= 0; n -= 8)
        *p++ = 0xff & (v >> n);
    return r;
}
#else
inline uint64 msb64(uint64 v)
{
    return v;
}

inline uint64 msb64(const char *pv)
{
    return *(uint64 *)pv;
}
#endif

#if 0
typedef char *key_t;
typedef std::list<key_t> key_list_t;


void check (const void *key, BTree &bt, key_list_t &l)
{
    key_list_t::iterator i;
    uint64 val;
    LenT   vlen;
    const char *k = (const char *) key;
    for (i = l.begin(); i != l.end(); ++i) {
        if (0 != bt.find(*i, 8, &val, vlen) || (msb64(*i) != val && *((uint64 *)*i) != val))
          fprintf (stderr, "failed after 0x%02.2x%02.2x%02.2x%02.2x%02.2x%02.2x%02.2x%02.2x: key - 0x%02x%02x%02x%02x%02x%02x%02x%02x, val - %I64d\n",
             k[0], k[1], k[2], k[3], k[4], k[5], k[6], k[7],
             (*i)[0], (*i)[1], (*i)[2], (*i)[3], (*i)[4], (*i)[5], (*i)[6], (*i)[7], val);
    }
}

void insert (key_list_t &l, const void *key)
{
    char *k = new char[8];
    memcpy (k, key, 8);
    l.push_back (k);
}
#endif
#ifdef TEST_HARD
class KeyVal {
public:
    KeyVal() : key_(0), klen_(0), val_(0), vlen_(0) {}
    KeyVal(const void *key, LenT klen, const void *val, LenT vlen) :
        klen_(klen),
        vlen_(vlen)
    {
        memcpy(key_ = new char[klen_], key, klen_); 
        memcpy(val_ = new char[vlen_], val, vlen_); 
    }
    KeyVal(const KeyVal &rhs)
    {
        klen_ = rhs.klen_;
        vlen_ = rhs.vlen_;
        memcpy(key_ = new char[klen_], rhs.key_, klen_); 
        memcpy(val_ = new char[vlen_], rhs.val_, vlen_); 
    }
    ~KeyVal()
    {
        delete [] key_;
        delete [] val_;
    }
    void *key_;
    LenT klen_;
    void *val_;
    LenT vlen_;
private:
    KeyVal &operator=(const KeyVal&);
} ;
typedef std::list<KeyVal> KeyValList;
inline void insert(KeyValList &l, const void *key, LenT klen, const void *val, LenT vlen)
{
    l.push_back(KeyVal(key, klen, val, vlen));
}
inline void check(const void *key, BTree &bt, KeyValList &l)
{
    KeyValList::iterator i;
    char val[256];
    LenT vlen;
    for (i = l.begin(); i != l.end(); ++i) {
        try 
        { 
            bt.find ((*i).key_, (*i).klen_, val, vlen); 
            
        }
        catch (Error &) {
            int n;
            const unsigned char *k = (const unsigned char *) key;
            // printf ("failed after 0x");
            stf::cerr << "failed after 0x";
            for (n = 0; n < (*i).klen_; ++n)
                // ("%c%c", "0123456789abcdef"[k[n] >> 4], "0123456789abcdef"[k[n] & 0xf]);
                std::cerr << "0123456789abcdef"[k[n] >> 4] << "0123456789abcdef"[k[n] & 0xf];
            // printf (" at 0x");
            std::cerr << " at 0x";
            k = (const unsigned char *) (*i).key_;
            for (n = 0; n < (*i).klen_; ++n)
                // printf ("%c%c", "0123456789abcdef"[k[n] >> 4], "0123456789abcdef"[k[n] & 0xf]);
                std::cerr << "0123456789abcdef"[k[n] >> 4] << "0123456789abcdef"[k[n] & 0xf] 
            // printf ("\n");
            std::cerr << std::endl;
        }
    }
}
#else
typedef void *KeyValList;
inline void insert(KeyValList &l, const void *key, LenT klen, const void *val, LenT vlen)
{
}
inline void check(const void *key, BTree &bt, KeyValList &l)
{
}
#endif

bool testInsert (BTree &bt, uint32 flags, uint64 overstep)
{
    KeyValList check_list;
    bool failed = false;
    uint64 insFirst, insLast;
    time_t tbeg = time (0);
    time_t insertTime;
    uint64 i;
    for (i = 0; i < lim; ++i) {
        uint64 key, val;
        if (0 == overstep) val = msb64 (i);
        else val = msb64 (i/overstep + i);
        if (BTREE_FLAGS_DUPLICATE == flags) key = msb64 (i >> 1);
        else key = msb64 (i);
        try { bt.insert (&key, sizeof (key), &val, sizeof (val)); }
        catch (Error &) {
            if (!failed) {
                // printf ("insert failure at %I64d\n", i);
                std::cerr << "insert failure at " << i << std::endl;
                insFirst = i;
            }
            insLast = i;
            failed = true;
        }
        insert(check_list, &key, sizeof (key), &val, sizeof (val));
        check(&key, bt, check_list);
        if (i % report == 0)
            // printf ("%I64d, elapsed %d\r", i, time(0)-tbeg);
            std::cerr << i << ", elapsed " << time(0)-tbeg << "\r";
    }
    insertTime = time(0)-tbeg;
    // printf ("%I64d, elapsed %d\n", i, insertTime);
    std::cerr << i << ", elapsed " << insertTime << std::endl;
    if (failed)
        // printf ("Insert failed first %I64d, last %I64d\n", insFirst, insLast);
        std::cerr << "Insert failed first " << insFirst << ", last " << insLast << std::endl;
    return !failed;
}

bool testSelect (BTree &bt, uint32 flags, uint64 overstep)
{
    bool failed = false;
    uint64 nfFirst, nfLast;
    time_t tbeg = time (0);
    time_t selectTime;
    uint64 lim1;
    if (BTREE_FLAGS_DUPLICATE == flags) lim1 = lim / 2;
    else lim1 = lim;
    uint64 i = 0;
    for (; i < lim1; ++i) {
        uint64 key;
        key = msb64 (i);
        uint64 val; LenT vlen;
        uint64 iover = i;
        if (0 != overstep) iover = i/overstep + i;
        bool oops = false;
        bool panic = false;
        vlen = sizeof(val);
        try { bt.find (&key, sizeof (key), &val, vlen); } //, BTREE_EXACT | BTREE_LAST); 
        catch (NotFound &) {
            oops = true;
        }
        catch (BadParameters &e) {
            // printf("bad parameters %s\n", (const char *) e);
            std::cerr << "bad parameters " << (const char *) e << std::endl;
        }
        catch (Error &) {
            oops = true;
            panic = true;
        }
        if (oops ||
            (BTREE_FLAGS_DUPLICATE == flags) ? msb64 (val)/2 != iover : msb64 (val) != iover) {
            if (!failed) {
                if (oops) {
                    if (panic) 
                        // printf ("something wrong at %I64d\n", i);
                        std::cerr << "something wrong at " << i << std::endl;
                    else
                        // printf ("not found at %I64d\n", i);
                        std::cerr << "not found at " << i << std::endl;
                } else
                    // printf ("value mismatch %I64d - %I64d\n", i, msb64(val));
                    std::cerr << "value mismatch " << i << " - " << msb64(val) << std::endl;
                nfFirst = i;
            }
            nfLast = i;
            failed = true;
        }
        if (i % report == 0)
            // printf ("%I64d, elapsed %d\r", i, time(0)-tbeg);
            std::cerr << i << ", elapsed " << time(0)-tbeg << std::endl;
    }
    selectTime = time(0)-tbeg;
    // printf ("%I64d, elapsed %d\n", i, selectTime);
    std::cerr << i  << ", elapsed " << selectTime << std::endl;
    if (failed)
        // printf ("Not found first %I64d, last %I64d\n", nfFirst, nfLast);
        std::cerr << "Not found first " << nfFirst << ", last " << nfLast << std::endl;
    return !failed;
}

class WhileSameKey : public BTreeQueryBase
{
public:
    WhileSameKey(const void *key, LenT len) : BTreeQueryBase(BTREE_PARTIAL, true, key, len)
    {}
    bool done(const void *key, LenT len, const void *val, LenT vlen)
    {
        return memcmp(key_, key, len_) != 0;
    }
} ;

class UntilTheEnd : public BTreeQueryBase
{
public:
    UntilTheEnd(const void *key, LenT len) : BTreeQueryBase(BTREE_PARTIAL, true, key, len)
    {}
    bool done(const void *key, LenT len, const void *val, LenT vlen)
    {
        return false;
    }
} ;

class NKeys : public BTreeQueryBase
{
public:
    NKeys(const void *key, LenT len, uint64 cnt) :
      BTreeQueryBase(BTREE_PARTIAL, true, key, len),
      cnt_(cnt)
    {}
    bool done(const void *key, LenT len, const void *val, LenT vlen)
    {
        return --cnt_ == 0;
    }
private:
    uint64 cnt_;
} ;

bool testCursor (BTree &bt, uint32 flags, uint64 overstep)
{
    bool failed = false;
    time_t tbeg = time (0);
    // for 0x70000 there should be 65536(0x10000) unique
    // and 0x14240 duplicate
#ifndef _DEBUG
//    const char myKey[] = {0,0,0,0,0,0x06};
    const char myKey[] = {0,0,0,0,0,0,0,0};
#else
//    const char myKey[] = {0,0,0,0,0,0,0x01};
    const char myKey[] = {0,0,0,0,0,0,0,0};
#endif
    UntilTheEnd qry(myKey, sizeof(myKey));
    BTreeCursor cur;
    bt.initcursor(cur, qry);
    uint64 cnt = 0;
    char key[8];
    uint16 len = 8;
    while (bt.fetch(cur, key, len)) ++cnt;
    time_t tlps = time (0) - tbeg;
    // printf ("Fetched %I64d, elapsed %d\n", cnt, tlps);
    std::cerr << "Fetched " << cnt << ", elapsed " << tlps << std::endl;
    return !failed;
}

bool testRemove(BTree &bt, uint32 flags, uint64 overstep)
{
    time_t tbeg = time (0);
#ifndef _DEBUG
//    const char myKey[] = {0,0,0,0,0,0x06};
    const char myKey[] = {0,0,0,0,0,0,1,0};
    const char endKey[] = {0,0,0,0,0,0,1,0x65};
#else
//    const char myKey[] = {0,0,0,0,0,0,0x01};
    const char myKey[] = {0,0,0,0,0,0,0,0x06};
    const char endKey[] = {0,0,0,0,0,0,0,0x6b};
#endif
    // UntilTheEnd qry(myKey, sizeof(myKey));
    // WhileSameKey qry(myKey, sizeof(myKey));
    uint64 pos = bt.getpos(endKey, sizeof(endKey), 0, 0, BTREE_EXACT);
    // printf("Pos: %I64d\n", pos);
    std::cerr << "Pos: " << pos << std::endl;
    NKeys qry(myKey, sizeof(myKey),101);
    BTreeCursor cur;
    bt.initcursor(cur, qry);
    int res;
    res = bt.remove(cur);
    time_t tlps = time (0) - tbeg;
    // printf ("Remove result is %d\nElasped %d\n", res, tlps);
    std::cerr << "Remove result is " << res << "\nnElasped " << tlps << std::endl;
    pos = bt.getpos(endKey, sizeof(endKey), 0, 0, BTREE_EXACT);
    // printf("Pos: %I64d\n", pos);
    std::cerr << "Pos: " << pos << std::endl;
    return BTREE_OK == res;
}

bool testDriver (const char *testName, uint32 flags)
{
    BTree bt;

    // printf ("%s\n", testName);
    std::cerr << testName << std::endl;
    // Erase old files if it exists
    if (splitFileFactory.exists (TSTDIR, TSTNAME))
        splitFileFactory.erase (TSTDIR, TSTNAME);

    uint64 overstep = 0;
    if (flags & BTREE_FLAGS_RANGE) overstep = kOverstep;
    bool succ = true;
    // Create paged file on split file storage
    BTreeFile& bf = pagedFileFactory.wrap (splitFileFactory.create (TSTDIR, TSTNAME));
    // Init btree over it
    if (bt.init (bf, sizeof (uint64), flags, sizeof (uint64))) {
        succ = succ && testInsert(bt, flags, overstep);
        succ = succ && testSelect(bt, flags, overstep);
        succ = succ && testCursor(bt, flags, overstep);
        succ = succ && testRemove(bt, flags, overstep);
        testSelect(bt, flags, overstep);
    } else {
        std::cerr << "Can not init btree over file, checkpoint " << bt.getCheckPoint() << std::endl;
    }
    

    // Close files
    bt.detach ();
    bf.close ();

    // Clean up
    if (splitFileFactory.exists (TSTDIR, TSTNAME))
        splitFileFactory.erase (TSTDIR, TSTNAME);
    return succ;
}

#if 0
//const uint64 badkey =  681318;
//const uint64 badinsind= 2729364;
//const uint64 badlim = 2730000;

const uint64 badkey =  10;
const uint64 badinsind= 63;
const uint64 badlim = 70;

bool testDupBadKey ()
{
    BTree bt;

    // printf ("DupBadKey\n");
    std::cerr << "DupBadKey" << std::endl;
    // Erase old files if it exists
    if (splitFileFactory.exists (TSTDIR, TSTNAME))
        splitFileFactory.erase (TSTDIR, TSTNAME);

    bool succ = false;
    // Create paged file on split file storage
    BTreeFile& bf = pagedFileFactory.wrap (splitFileFactory.create (TSTDIR, TSTNAME));
    // Init btree over it
    if (bt.init (bf, sizeof (uint64), BTREE_FLAGS_DUPLICATE, sizeof (uint64))) {
        bool failed = false;
        uint64 insFirst, insLast;
        time_t tbeg = time (0);
        time_t insertTime;
        for (uint64 i = 0; i < badlim; ++i) {
            uint64 key, val, val1;
            LenT vlen;
            val = msb64 (i);
            key = msb64 (i >> 1);
            if (i == badinsind) {
                key = key;
            }
            int res = bt.insert (&key, sizeof (key), &val, sizeof (val));
            if (i % report == 0)
                // printf ("%I64d, elapsed %d\r", i, time(0)-tbeg);
                std::cerr << i << ", elapsed " << time(0)-tbeg << "\r";
            
            if (BTREE_OK == res && (key << 1) == i) {
                res = bt.find (&key, sizeof (key), &val1, vlen);
                if (BTREE_OK == res && !failed) failed = msb64(val1) / 2 != i;
            }
            if (BTREE_OK == res && (i >> 1) > badkey) {
                key = msb64 (badkey);
                res = bt.find (&key, sizeof (key), &val1, vlen);
                if (BTREE_OK != res) {
                    if (!failed) {
                        // printf ("badkey can not be retrieved at %I64d\n", i);
                        std::cerr << "badkey can not be retrieved at " << i << std::endl;
                        insFirst = i;
                    }
                    insLast = i;
                    failed = true;
                } else if (msb64(val1) / 2 != badkey) {
                    if (!failed) {
                        // printf ("badkey mismatch at %I64d\n", i);
                        std::cerr << "badkey mismatch at " << i << std::endl;
                        insFirst = i;
                    }
                    insLast = i;
                    failed = true;
                }
            }
            if (BTREE_OK != res) {
                if (!failed) {
                    // printf ("insert failure at %I64d\n", i);
                    std::cerr << "insert failure at " << i << std::endl;
                    insFirst = i;
                }
                insLast = i;
                failed = true;
            }
        }
        insertTime = time(0)-tbeg;
        // printf ("%I64d, elapsed %d\n", i, insertTime);
        std::cerr << i << ", elapsed " << insertTime << std::endl;
        if (failed)
            // printf ("Insert failed first %I64d, last %I64d\n", insFirst, insLast);
            std::cerr << "Insert failed first " << insFirst << ", last " << insLast << std::endl;
        succ = !failed;
    } else {
        // fprintf (stderr, "Can not init btree over file, checkpoint %d\n", bt.getCheckPoint());
        std::cerr << "Can not init btree over file, checkpoint " << bt.getCheckPoint() << std::endl;
    }

    // Close files
    bt.detach ();
    bf.close ();

    // Clean up
    if (splitFileFactory.exists (TSTDIR, TSTNAME))
        splitFileFactory.erase (TSTDIR, TSTNAME);
    return succ;
}
#endif

namespace edb {
bool testBTree ()
{
    //testDupBadKey ();
    /* Results:
        Taurus, 4/17/2002
        Testing B-tree...
        Duplicate
        1000000, elapsed 17  16  16
        500000, elapsed   7   8   7
        Range
        1000000, elapsed  7   7   6
        1000000, elapsed  7   7   7
        Unique
        1000000, elapsed 52  52  52
        1000000, elapsed 13  13  13

        Castor, 7/1/2002
        USE_COUNT
        Testing B-tree...
        Duplicate
        1000000, elapsed 10
        500000, elapsed 4
        Range
        1000000, elapsed 5
        1000000, elapsed 4
        Unique
        1000000, elapsed 26
        1000000, elapsed 7

        !USE_COUNT
        Testing B-tree...
        Duplicate
        1000000, elapsed 9
        500000, elapsed 4
        Range
        1000000, elapsed 4
        1000000, elapsed 4
        Unique
        1000000, elapsed 23
        1000000, elapsed 7
    */
    testDriver ("Duplicate", BTREE_FLAGS_DUPLICATE);
    testDriver ("Unique", BTREE_FLAGS_UNIQUE);
    testDriver ("Range", BTREE_FLAGS_RANGE);
    return true;
}
} // namespace edb
