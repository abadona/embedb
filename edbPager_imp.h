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

#ifndef edbPager_imp_h
#define edbPager_imp_h

#include "edbPager.h"
#include <list>
#include <map>

namespace edb
{

class Pager_imp : public Pager
{
protected:
    enum AVAIL { NOT_AVAIL, ALL_FREE, WEIGHTED };
    typedef std::list<int> Ilist;
    struct PageKey
    {
        PageKey (File& f, uint64 page) : file_ (&f), page_ (page) {}
        bool operator < (const PageKey& pk) const 
        {
            return (file_ == pk.file_) ? (page_ < pk.page_) : (file_ < pk.file_); 
        }
        bool operator == (const PageKey& pk) const 
        {
            return (file_ == pk.file_) && (page_ == pk.page_); 
        }
        File*   file_;
        uint64  page_;
    };

    typedef std::map <PageKey, uint32> Pkeymap;

    struct Page
    {
        Page () : free_ (true), markcnt_ (0), lockcnt_ (0), masters_ (0), useno_ (0L) {}
        File*   file_; // file which contains the page
        uint64  page_; // page number in file
        bool    free_; // free flag, =true if node is unused
        uint32  markcnt_; // mark count
        uint32  lockcnt_; // lock count
        uint32  masters_; // number of pages in a row managed together with this page. For managed pages, masters_ = 0
        uint64  useno_;
        Ilist::iterator mrulist_pos_; // position of the page in MRU list
        Ilist::iterator freelist_pos_; // position of the page in free pages list
    };
    Page*       pages_;         // array of page descriptors
    Ilist       freelist_;      // list of the numbers of free slots
    Ilist       mrulist_;       // list of the numbers of used slots in MRU order
    Pkeymap     addrmap_;       // map (File*, pagenumber -> slot_number)
    char*       arena_;         // the memory area for storing the pages
    char*       arenabuf_;      // the 'unaligned' memory for the arena
    uint64      dumpcnt_;       // number of dumped events
    uint64      misses_;        // number of cache misses
    uint64      hits_;          // number of cache hits
    uint32*     hlpbuf_;        // buffer to hold temporary sets of pageidxses (for deletes) - to avoid heap allocs/frees
    FilePos     last_dumped_;   // the previous page written to a disk (for non-continous dumps counting)
    uint64      cur_pageuse_;   // page use counter

    uint32      pagesize_;      // size of the page
    uint32      poolsize_;      // size of the pages pool (in number of pages)

    // helper methods
    uint32      process_overlaps_ (File& file, FilePos pageno, uint32 count); // finds all in-cache overlapping ranges. 
                                // Separates the portions of overlaps that are not covered by pageno..count interval into valid independent slotranges
                                // Sets page_ for covered slots and saves the covered slots into hlpbuf_. Returns number of covered slots saved.
    bool        uninterrupted_hlp_range_ (uint32 ccnt); // checks whether the range of (ccnt) pages saved in hlpbuf_ comprises uninterrupted row
    uint32      assemble_hlp_slotrange_ (uint32 ccnt); // assembles slotrange out of the range of (ccnt) pages saved in hlpbuf_. Returns master slot index
    void        free_hlp_buf_ (uint32 ccnt); // frees pages referred from hlpbuf_
    void        fill_ (uint32 slotidx, File& file, FilePos pageno, uint32 count, uint32 ccnt); // reads in / copies from hlpbuf_ the pages into slotrange pointed by slotidx
    void        fakefill_ (uint32 slotidx, File& file, FilePos pageno, uint32 count); // just stores the file ptrs and page numbers in slots

    // low-level basic service methods
    void        markfree_   (uint32 slotidx);   // unconditionally puts the page on free list and marks it free
    void        markused_   (uint32 slotidx);   // removes from free list; marks used;
    uint32      slotidx_    (const void* data); // finds the slot index for the buffer start;
    void*       slotaddr_   (uint32 slotidx);   // returns the page data address associated with slotidx
    void        popmru_     (uint32 slotidx);   // puts the slot on the topmost position in MostRecentlyUsed list
    uint32      getmaster_  (uint32 slotidx);   // returns master slot index for a given slot
    void        addmaster_  (uint32 slotidx);   // adds (allready marked as used) slot to the master lists: addrmap_ and mrulist_
    void        removemaster_ (uint32 slotidx); // removes slot from the master lists: addrmap_ and mrulist_

    void        dump_       (uint32 slotidx);   // if slotrange is dirty, writes the contents to file and clears dirty state
    void        dumpslots_  (uint32 slotidx, uint32 count); // unconditionally writes the contents of slots to file
    void        free_       (uint32 slotidx);   // removes the slotrange from all referring lists and returns to free storage
    void        freeslot_   (uint32 slotidx);   // unconditionally removes the slot from all referring lists if any and returns to free storage
    void        read_       (uint32 slotidx, File& file, FilePos pageno, uint32 count = 1); // reads the contents of the page range into the slotrange

    void        lock_       (uint32 slotidx);   // Increments lock count on the pagerange starting with slotidx;
    void        unlock_     (uint32 slotidx);   // Decrements lock count on the pagerange starting with slotidx;
    bool        locked_     (uint32 slotidx);   // Checks the pagerange starting with slotidx for being locked;
    void        mark_       (uint32 slotidx);   // Increments mark count on the pagerange starting with slotidx;
    void        unmark_     (uint32 slotidx);   // Decrements lock count on the pagerange starting with slotidx;
    bool        marked_     (uint32 slotidx);   // Checks the pagerange starting with slotidx for being marked as dirty;


    // high-level methods
    void        init_       (uint32 pagesize, uint32 poolsize); // initializes the internal data structures
    void        detach_     (bool freedata = true); // flashes and frees all data / frees all structures if freedata is true
    uint32      fetch_      (File& file, FilePos pageno, uint32 count); // fetches the page(s), returns data address
    uint32      fake_       (File& file, FilePos pageno, uint32 count); // fake-fetches the page(s), returns data address
    uint32      allocate_   (uint32 count, uint32 preserved_count = 0); // allocates the count continous slots. Uses ((free or LRU) + longest dump + not-locked + not-preserved) strategy 
                                                                        // the preserved slots are those which indexes are stored in hlpbuf_; their number is preserved_count
    void        makerange_  (uint32 slotidx, uint32 count); // turns the range into the slotrange, mastered by slot slotno.


    // strategy methods
    uint32      lookupfree_ (uint32 count, uint32 preserved_count = 0); // looks up the freelist and then lrulist for best slot range for (re)allocation
                            // the found slot number. Throws NoCacheSpace if not found
    AVAIL       check_avail_left_ (uint32 slotidx, uint32 count, uint32 preserved_count, uint64& weight); // checks whether the slot has count of unlocked and not preserved slots left to itself
                                // the preserved slots of the number preserved_count are stored in hlpbuf
                                // calculates preference weight for this slot use
    Pkeymap::iterator 
                dump_len_   (uint32 slotidx, uint32& length); // checks how many pages could be dumped at once around the one in passed slot. Checks at most LONG_ENOUGH_SEQ slots
    void        dumpx_      (Pkeymap::iterator begin);  // dumps the row of pages following the one referred by begin - until the 'end of file' or too long gap is found
                Pager_imp   (uint32 pagesize, uint32 poolsize);

public:
                ~Pager_imp  ();
    void*       fetch       (File& file, uint64 pageno, bool lock = false, uint32 count = 1);
    void*       fake        (File& file, uint64 pageno, bool lock = false, uint32 count = 1);
    bool        locked      (const void* data);
    void        lock        (const void* data);
    void        unlock      (const void* data);
    bool        marked      (const void* data);
    void        mark        (const void* data);
    void        unmark      (const void* data);
    bool        commit      (File& file);
    bool        chsize      (File& file, FilePos newSize);
    bool        close       (File& file);
    bool        detach      (File& file);

    uint64      pageno      (const void* data);
    File&       file        (const void* data);
    void*       pageaddr    (const void* data, uint32* count = NULL);

    void*       checkpage   (File& file, uint64 pageno, uint32* count = NULL);

    uint32      getPageSize () const;
    uint32      getPoolSize () const;
    bool        setPoolSize (uint32 poolsize);

    uint64      getDumpCount() const;
    uint64      getHitsCount() const;
    uint64      getMissesCount() const;

    friend class PagerFactory_imp;
};

class PagerFactory_imp : public PagerFactory
{
public:
    Pager&      create      (uint32 pagesize, uint32 poolsize);
};

};

#endif