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

#ifndef edbSimplePager_imp_h
#define edbSimplePager_imp_h

#include "edbPager.h"
#include <list>
#include <map>

namespace edb
{


class SimplePager : public Pager
{
protected:
    typedef std::list<int> Ilist;
    struct PageKey
    {
        PageKey (File& f, uint64 page) : file_ (&f), page_ (page) {}
        bool operator < (const PageKey& pk) const 
        { 
            return (file_ == pk.file_) ? (page_ < pk.page_) : (file_ < pk.file_); 
        }
        File*   file_;
        uint64  page_;
    };
    typedef std::map <PageKey, uint32> Pkeymap;
    struct Page
    {
        Page () : free_ (true), markcnt_ (0), lockcnt_ (0), masters_ (1) {}
        File*   file_; // file which contains the page
        uint64  page_; // page number in file
        bool    free_; // free flag, =true if node is unused
        uint32  markcnt_; // mark count
        uint32  lockcnt_; // lock count
        uint32  masters_; // number of pages in a row managed together with this page. For managed pages, masters_ = 0
        Ilist::iterator mrulist_pos_; // position of the page in MRU list
        Ilist::iterator freelist_pos_; // position of the page in free pages list
    };
                Page*       pages_;         // array of page descriptors
                Ilist       freelist_;      // list of the numbers of free slots
                Ilist       mrulist_;       // list of the numbers of used slots in MRU order
                Pkeymap     addrmap_;       // map (File*, pagenumber -> slot_number)
                char*       arena_;         // the memory area for storing the pages
                uint64      dumpcnt_;       // number of dumped events
                uint64      misses_;        // number of cache misses
                uint64      hits_;          // number of cache hits
                uint32*     hlpbuf_;        // buffer to hold temporary sets of pageidxses (for deletes) - to avoid heap allocs/frees
                
                uint32      pagesize_;      // size of the page
                uint32      poolsize_;      // size of the pages pool (in number of pages)

    uint32      fetch_      (File& file, uint64 pageno, uint32 count); // fetches page(s) into memory, takes care of everything
    uint32      fake_       (File& file, uint64 pageno, uint32 count); // fake-loads the page(s) into empty(!) cache slot(s) (for full overwrite)
    uint32      getFree_    (uint32 count); // finds the (stretch of) empty page(s); frees (dumps) dirty one(s) if necessary
    void        load_       (uint32 page_idx, File& file, uint64 pageno, uint32 count); // loads the page(s) into empty(!) cache slot(s)
    void        markloaded_ (uint32 page_idx, File& file, uint64 pageno, uint32 count); // fakes the load
    void        free_       (uint32 page_idx); // marks the page structure as free (does not do the dump!)
    void        dump_       (uint32 page_idx); // dumps the page and marks as clean  
    void        pop_        (uint32 page_idx); // moves to the beginning of the MRU list
    uint32      pageno_     (const void* page_address) const; // calculates page index by pointer to the beginning
    Pkeymap::iterator check_cont (uint32 page_idx, uint32& length); // calculates number of adjacent dirty slots. Returns the iterator into addrmap_ for first slot in a row
    uint32      find_master_(uint32 page_idx); // finds the master page index for the page index
    bool        checkslotno_(uint32 page_idx, uint32 count); // checks weather there are count non-locked slots left to page_idx (including the one)
    void        prep_detach_(); // dumps all marked pages; frees all pages. If something locked, throws exception
    void        free_intersectors_(File& file, uint64 pageno, uint32 count);
    void        init_       (uint32 pagesize, uint32 poolsize); // initializes empty cache

    void        dumpX_      (Pkeymap::iterator itr); // dumps all the the dirty pages that appear in a row after current


                SimplePager (uint32 pagesize, uint32 poolsize);

public:
                ~SimplePager ();
    void*       fetch       (File& file, uint64 pageno, bool lock = false, uint32 count = 1);
    void*       fake        (File& file, uint64 pageno, bool lock = false, uint32 count = 1);
    bool        locked      (const void* page);
    void        lock        (const void* page);
    void        unlock      (const void* page);
    bool        marked      (const void* page);
    void        mark        (const void* page);
    void        unmark      (const void* page);
    bool        commit      (File& file);
    bool        chsize      (File& file, FilePos newSize);
    bool        close       (File& file);
    bool        detach      (File& file);

    uint64      pageno      (const void* page);
    File&       file        (const void* page);
    void*       pageaddr    (const void* ptr, uint32* count = NULL);

    void*       checkpage   (File& file, uint64 pageno, uint32* count = NULL);

    uint32      getPageSize () const;
    uint32      getPoolSize () const;
    bool        setPoolSize (uint32 poolsize);

    uint64      getDumpCount() const;
    uint64      getHitsCount() const;
    uint64      getMissesCount() const;

    friend class SimplePagerFactory;
};

class SimplePagerFactory : public PagerFactory
{
public:
    Pager&      create      (uint32 pagesize, uint32 poolsize);
};

};

#endif