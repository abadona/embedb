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

#define edbPagerFactory_defined
#include "edbPager_imp.h"
#include "edbExceptions.h"
#include <vector>
#include <string.h>

//#define PAGER_IMP_DEBUG                                                

namespace edb
{

// the maximal step between pages to use as 'continous range'
#define GAP_FACTOR 1 
// number of continous pages in a dump sequence which is long enough to stop serching for better
#define LONG_ENOUGH_SEQ 8  
// maximal allowed size of allocation unit (in pages)
#define MAX_PAGEROW_LEN 2
// number of times the clean slot is better then best(longest dumpable) dirty one
#define CLEAN_SLOT_FACTOR 2
// number of times the empty slot is better then best(longest dumpable) dirty one
#define EMPTY_SLOT_FACTOR 4
// number of iterations through lru pages passed without score improvement which cancels further search
#define UNIMPROVED_COUNT 16
// the restriction on the dump done at once
#define MAX_DUMP_LEN (MAX_PAGEROW_LEN*2+LONG_ENOUGH_SEQ)

// cache pages dump policy
#define FAVOR_MIN_WRITE_VOLUME
// #define FAVOR_LONG_DUMPS
static const uint32 MEM_PAGE_SIZE = 0x1000; // 4 Kb


static PagerFactory_imp theFactory;
PagerFactory& pagerFactory = theFactory;

Pager_imp::Pager_imp (uint32 pagesize, uint32 poolsize)
:
pagesize_ (0),
poolsize_ (0),
pages_ (NULL),
arena_ (NULL),
arenabuf_ (NULL),
hlpbuf_ (NULL),
dumpcnt_ (0),
hits_ (0),
misses_ (0),
last_dumped_ (UINT32_MAX),
cur_pageuse_ (0L)
{
    init_ (pagesize, poolsize);
}

Pager_imp::~Pager_imp ()
{
    // calling detach_ ensures that all dirty pages are saved. 
    // If some files not fully saved are closed by this time, exception will be thrown on write attempt
    detach_ ();
}

uint32 Pager_imp::process_overlaps_ (File& file, FilePos pageno, uint32 count)
{
#ifdef PAGER_IMP_DEBUG
    if (!count || count > MAX_PAGEROW_LEN)
        ERR("process_overlaps_: parameter 'count' is invalid")
#endif 
    // set counter to 0
    uint32 overlaps_count = 0;
    // find the positions for the first and last possible pages in the addrmap_
    FilePos first_page = (pageno > MAX_PAGEROW_LEN)?(pageno-MAX_PAGEROW_LEN):0;
    Pkeymap::iterator begin_pos = addrmap_.lower_bound (PageKey (file, first_page));
    // Pkeymap::iterator end_pos  = addrmap_.lower_bound (PageKey (file, pageno + count));

    // buffers for removed and added master slots: we cannot add / remove duting iteration, since such operations invalidate iterators
    // mostly MAX_PAGEROW_LEN removed slots
    uint32 toRemove [MAX_PAGEROW_LEN];
    uint32 removeCount = 0;
    // mostly 1 added slots
    uint32 toAdd [1];
    uint32 addCount = 0;
    // iterate from first to last position; for every page
    for (Pkeymap::iterator itr = begin_pos; itr != addrmap_.end (); itr ++)
    {
        if ((*itr).first.file_ != &file || (*itr).first.page_ >= pageno + count)
            break;
        uint32 slotidx = (*itr).second;
        Page& page = pages_ [slotidx];
        uint32 subordcount = page.masters_;
#ifdef PAGER_IMP_DEBUG
        if (page.free_)
            ERR("process_overlaps_: free page in address map");
        if (!page.masters_)
            ERR("process_overlaps_: subordinate page in address map");
#endif
        // check whether overlaps
        if (page.page_ + subordcount > pageno)
        {
#ifdef PAGER_IMP_DEBUG
            if (page.lockcnt_)
                ERR("process_overlaps_: overlap with locked slotrange");
#endif
            // save markcnt      
            uint32 markcnt = page.markcnt_;

            if (page.page_ < pageno) // there is preceeding portion. shrink the slotrange to this portion only
            {
                page.masters_ = pageno - page.page_;
            }
            else // there is no preceeding portion. save the master slot for removal from master control structures
            {
                toRemove [removeCount ++] = slotidx;
#ifdef PAGER_IMP_DEBUG
                if (removeCount > MAX_PAGEROW_LEN)
                    ERR("process_overlaps_: too many covered items marked for removal (>MAX_PAGEROW_LEN)");
#endif
            }
            // position of the first slot in requested range as if it was mapped onto the current slotrange
            // virtual first slot position may be (semm to be) negative, so it is signed value. (assuming unsigned arithmetics is correct when converted to signed)
            int32 virtual_fslot_pos = (page.page_ < pageno) ? (slotidx + (pageno - page.page_)) : (slotidx - (page.page_ - pageno));
            // save internal portion and markcnt_
            //  need to cast to signed because in comparisons unsigned takes precedence
            uint32 internal_begin = max_ (((int32)virtual_fslot_pos), ((int32)slotidx));
            uint32 internal_end = min_ (((int32)(virtual_fslot_pos + count)), ((int32)(slotidx + subordcount)));
            for (uint32 si = internal_begin; si < internal_end; si ++)
            {
                Page& curp = pages_ [si];
#ifdef PAGER_IMP_DEBUG
                if (curp.page_ != page.page_ + (si - slotidx))
                    ERR ("Page number for a slot is invalid");
#endif
                curp.markcnt_ = markcnt;
//                curp.masters_ = 0;
                hlpbuf_ [overlaps_count ++] = si;
#ifdef PAGER_IMP_DEBUG
                if (overlaps_count > MAX_PAGEROW_LEN)
                    ERR("process_overlaps_: too many common pages (>MAX_PAGEROW_LEN) encountered");
#endif
            }
            // if there is postceeding portion, make separate range out of it
            if (page.page_ + subordcount > pageno + count)
            {
                pages_ [internal_end].masters_ = (page.page_ + subordcount) - (pageno + count);
                pages_ [internal_end].markcnt_ = markcnt;
                toAdd [addCount ++] = internal_end;
#ifdef PAGER_IMP_DEBUG
                if (addCount > 1)
                    ERR("process_overlaps_: too many items marked for addition (>1)");
#endif
            }
        }
    }
    // now add and remove saved slots
    for (int ri = 0; ri < removeCount; ri ++)
        removemaster_ (toRemove [ri]);
    for (int ai = 0; ai < addCount; ai ++)
        addmaster_ (toAdd [ai]);

    // return number of 'common slots' saved to hlpbuf_
    return overlaps_count;
}


bool Pager_imp::uninterrupted_hlp_range_ (uint32 ccnt)
{
    uint32 prevslot;
    FilePos prevpage;
    for (uint32 i = 0; i < ccnt; i ++)
    {
        uint32 slotidx = hlpbuf_ [i];
#ifdef PAGER_IMP_DEBUG
        if (slotidx >= poolsize_)
            ERR("uninterrupted_hlp_range_: Slot index out of range")
#endif
        Page& page = pages_ [slotidx];
#ifdef PAGER_IMP_DEBUG
        if (page.free_)
            ERR("uninterrupted_hlp_range_: free page encountered");
        if (page.lockcnt_)
            ERR("uninterrupted_hlp_range_: locked page encountered");
        if (page.masters_)
            ERR("uninterrupted_hlp_range_: masterpage encountered");
#endif
        if (i)
        {
            if (slotidx != prevslot + 1)
                return false;
            if (page.page_ != prevpage + 1)
                return false;
        }
        prevslot = slotidx;
        prevpage = page.page_;
    }
    return true;
}

uint32 Pager_imp::assemble_hlp_slotrange_ (uint32 ccnt)
{
    uint32 sum_mark_cnt = 0;
    for (uint32 i = 0; i < ccnt; i ++)
    {
        uint32 slotidx = hlpbuf_ [i];
#ifdef PAGER_IMP_DEBUG
        if (slotidx >= poolsize_)
            ERR("assemble_hlp_slotrange_: Slot index out of range");
#endif
        Page& page = pages_ [slotidx];
#ifdef PAGER_IMP_DEBUG
        if (page.free_)
            ERR("assemble_hlp_slotrange_: free page encountered");
        if (page.lockcnt_)
            ERR("assemble_hlp_slotrange_: locked page encountered");
        if (page.masters_)
            ERR("assemble_hlp_slotrange_: masterpage encountered");
#endif
        if (!i)
        {
            page.masters_ = ccnt;
            addmaster_ (slotidx);
        }
        sum_mark_cnt += page.markcnt_;
        page.markcnt_ = 0;
        // Note: the page_ and file_ members of slots should be correct here! (slots refer to valid data...)
    }
    pages_ [*hlpbuf_].markcnt_ = sum_mark_cnt;
    hits_ += ccnt;
    return *hlpbuf_;
}

void Pager_imp::free_hlp_buf_ (uint32 ccnt)
{
    for (uint32 si = 0; si < ccnt; si ++)
    {
        uint32 slotidx = hlpbuf_ [si];
#ifdef PAGER_IMP_DEBUG
        if (slotidx >= poolsize_)
            ERR("free_hlp_buf_: slot index out of range");
#endif
        markfree_ (slotidx);
    }
}

void Pager_imp::fill_ (uint32 slotidx, File& file, FilePos pageno, uint32 count, uint32 ccnt)
{
#ifdef PAGER_IMP_DEBUG
    if (slotidx + count > poolsize_)
        ERR("fill_: slot index out of range");
    if (count > MAX_PAGEROW_LEN)
        ERR("fill_: count is too big (>MAX_PAGEROW_LEN)");
    if (ccnt > MAX_PAGEROW_LEN)
        ERR("fill_: common slots number is too big (>MAX_PAGEROW_LEN)");
#endif
    uint32 cur_common = 0;
    uint32 readrow_start = 0;
    Page& firstslot = pages_ [slotidx];
    for (uint32 pgi = 0; pgi < count; pgi ++)
    {
        Page& slot = pages_ [slotidx + pgi];
        // if there are more common slots
        if (cur_common < ccnt)
        {
            uint32 common_slotidx = hlpbuf_ [cur_common];
#ifdef PAGER_IMP_DEBUG
            if (common_slotidx >= poolsize_)
                ERR("fill_: common slot index out of range");
#endif
            // if current common slot is right for this page
            if (pages_ [common_slotidx].page_ == pageno + pgi)
            {
                // read the accumulated row of pages (if any)
                if (readrow_start != pgi)
                {
                    misses_ += pgi - readrow_start;
                    read_ (slotidx + readrow_start, file, pageno + readrow_start, pgi - readrow_start);
                }

                // copy the contents of common page
                memcpy (arena_ + (slotidx + pgi) * pagesize_, arena_ + common_slotidx * pagesize_, pagesize_);
                // carry the mark count (Count (not bool) here becomes confusing. Just sum them up - this really does no matter ;))
                firstslot.markcnt_ += pages_ [common_slotidx].markcnt_;
                // update hit counter 
                hits_ ++;

#ifdef PAGER_IMP_DEBUG
                if (pages_ [common_slotidx].file_ != &file)
                    ERR("fill_: invalid file in common pages");
                if (pages_ [common_slotidx].page_ != pageno + pgi)
                    ERR("fill_: invalid page in common pages");
#endif
                // store the pagenumber and file
                slot.file_ = &file;
                slot.page_ = pageno + pgi;
                // increment current common
                cur_common ++;
                // remember where we finished
                readrow_start = pgi + 1;
            }
        }
    }
    // read the orphan row of pages
    if (readrow_start != count)
    {
        misses_ += count - readrow_start;
        read_ (slotidx + readrow_start, file, pageno + readrow_start, count - readrow_start);
    }

    // add to master structure
    addmaster_ (slotidx);
}

void Pager_imp::fakefill_ (uint32 slotidx, File& file, FilePos pageno, uint32 count)
{
#ifdef PAGER_IMP_DEBUG
    if (slotidx + count > poolsize_)
        ERR("fakefill_: slot index out of range");
    if (count > MAX_PAGEROW_LEN)
        ERR("fakefill_: count is too big (>MAX_PAGEROW_LEN)");
#endif
    for (uint32 pgi = 0; pgi < count; pgi ++)
    {
        Page& slot = pages_ [slotidx + pgi];
        // store the pagenumber and file
        slot.file_ = &file;
        slot.page_ = pageno + pgi;
    }
    // add to master structure
    addmaster_ (slotidx);
}

void Pager_imp::markfree_ (uint32 slotidx)
{
#ifdef PAGER_IMP_DEBUG
    if (slotidx >= poolsize_)
        ERR("markfree_: slot index out of range");
#endif
    Page& page = pages_ [slotidx];
    freelist_.push_front (slotidx);
    page.freelist_pos_ = freelist_.begin ();
    page.free_ = true;
    page.lockcnt_ = 0;
    page.markcnt_ = 0;
    page.masters_ = 0;
}

void Pager_imp::markused_ (uint32 slotidx)
{
#ifdef PAGER_IMP_DEBUG
    if (slotidx >= poolsize_)
        ERR("markused_: slot index out of range");
#endif
    Page& page = pages_ [slotidx];
#ifdef PAGER_IMP_DEBUG
    if (!page.free_) 
        ERR("markused_: page is not free");
    if (page.freelist_pos_ == freelist_.end ()) 
        ERR("markused_: page is pointing to freelist.end ()");
    if (*page.freelist_pos_ != slotidx) 
        ERR("markused_: page does not point to itself in freelist");
#endif
    freelist_.erase (page.freelist_pos_);
    page.freelist_pos_ = freelist_.end ();
    page.free_ = false;
}

uint32 Pager_imp::slotidx_ (const void* data)
{
#ifdef PAGER_IMP_DEBUG
    if (((char*) data) < arena_ || ((char*) data) > arena_ + poolsize_*pagesize_ || (((char*) data) - arena_) % pagesize_ != 0)
        ERR("slotidx_: Invalid address for the page slot passed");
#endif
    uint32 slotidx = (((char*) data) - arena_) / pagesize_;
#ifdef PAGER_IMP_DEBUG
    Page& page = pages_ [slotidx];
    if (page.free_)
        ERR("slotidx_: Index for free page requested");
    if (!page.masters_)
        ERR("slotidx_: Index for subordinate page requested");
#endif
    return slotidx;
}

void* Pager_imp::slotaddr_ (uint32 slotidx)
{
#ifdef PAGER_IMP_DEBUG
    if (slotidx >= poolsize_)
        ERR("slotaddr_: slot index out of range");
#endif
    return arena_ + slotidx*pagesize_;
}

void Pager_imp::popmru_ (uint32 slotidx)
{
#ifdef PAGER_IMP_DEBUG
    if (slotidx >= poolsize_)
        ERR("popmru_: slot index out of range");
#endif 
    Page& page = pages_ [slotidx];
#ifdef PAGER_IMP_DEBUG
    if (page.free_)
        ERR("popmru_: page is free");
    if (!page.masters_)
        ERR("popmru_: page is subordinate");
    if (page.mrulist_pos_ == mrulist_.end ())
        ERR("popmru_:: page points to the end of MRU list");
    if (*page.mrulist_pos_ != slotidx)
        ERR("popmru_:: page does not point to itself in MRU list");
#endif
    mrulist_.erase (page.mrulist_pos_);
    mrulist_.push_front (slotidx);
    page.mrulist_pos_ = mrulist_.begin ();
    page.useno_ = cur_pageuse_;
    cur_pageuse_ ++;
}

uint32 Pager_imp::getmaster_ (uint32 slotidx)
{
    uint32 toRet = slotidx;
#ifdef PAGER_IMP_DEBUG
    if (toRet >= poolsize_)
        ERR("getmaster_: slot index out of range");
#endif 
    while (1) 
    {
#ifdef PAGER_IMP_DEBUG
        if (pages_ [toRet].free_)
            ERR("getmaster_: free page encountered");
#endif
        if (pages_ [toRet].masters_)    
            break;
        toRet --;
    }
    
    return toRet;
}

void Pager_imp::addmaster_ (uint32 slotidx)
{
#ifdef PAGER_IMP_DEBUG
    if (slotidx >= poolsize_)
        ERR("addmaster_: slot index out of range");
#endif 
    Page& page = pages_ [slotidx];
#ifdef PAGER_IMP_DEBUG
    if (page.free_)
        ERR("addmaster_: page is free");
    if (page.lockcnt_)
        ERR("addmaster_: page is locked");
    if (!page.masters_)
        ERR("addmaster_: page masters_ == 0");
    if (addrmap_.find (PageKey (*page.file_, page.page_)) != addrmap_.end ())
        ERR("addmaster_: page is allready in addrmap_ map");
    if (page.mrulist_pos_ != mrulist_.end ())
        ERR("addmaster_: page refers to non-end position in MRUlist");
#endif 
    mrulist_.push_front (slotidx);
    page.mrulist_pos_ = mrulist_.begin ();
    addrmap_ [PageKey (*page.file_, page.page_)] = slotidx;
    page.useno_ = cur_pageuse_;
    cur_pageuse_ ++;
#ifdef PAGER_IMP_DEBUG
    if (mrulist_.size() != addrmap_.size ()) 
        ERR("addmaster_:MRUlist size does not match ADDRMAP size!")
#endif 
}

void Pager_imp::removemaster_ (uint32 slotidx)
{
#ifdef PAGER_IMP_DEBUG
    if (slotidx >= poolsize_)
        ERR("addmaster_: slot index out of range");
#endif 
    Page& page = pages_ [slotidx];
#ifdef PAGER_IMP_DEBUG
    if (page.free_)
        ERR("removemaster_: page is free");
    if (page.lockcnt_)
        ERR("removemaster_: page is locked");
    if (!page.masters_)
        ERR("removemaster_: page masters_ == 0");
    if (addrmap_.find (PageKey (*page.file_, page.page_)) == addrmap_.end ())
        ERR("removemaster_: page not in addrmap_ map");
    if (page.mrulist_pos_ == mrulist_.end ())
        ERR("removemaster_: page refers to end position in MRUlist");
    if (*page.mrulist_pos_ != slotidx)
        ERR("removemaster_: page refers to wrong place in MRUlist");
#endif 
    mrulist_.erase (page.mrulist_pos_);
    page.mrulist_pos_ = mrulist_.end ();
    page.useno_ = UINT64_MAX;
    int delcnt = addrmap_.erase (PageKey (*page.file_, page.page_));
#ifdef PAGER_IMP_DEBUG
    if (delcnt != 1)
        ERR("removemaster_: 0 items deleted from addrmap_")
    if (mrulist_.size() != addrmap_.size ()) 
        ERR("removemaster_: MRUlist size does not match ADDRMAP size!")
#endif 
    page.masters_ = 0;
}

void Pager_imp::dump_ (uint32 slotidx)
{
#ifdef PAGER_IMP_DEBUG
    if (slotidx >= poolsize_)
        ERR("dump_: slot index out of range");
#endif 
    Page& page = pages_ [slotidx];
#ifdef PAGER_IMP_DEBUG
    if (page.free_)
        ERR("dump_: free slot passed");
    if (!page.masters_)
        ERR("dump_: subordinate slot passed");
#endif
    if (page.markcnt_)
    {
        // write it 
        FilePos fileoff = page.page_*pagesize_;
        if (page.file_->seek (fileoff) != fileoff) ERR("Seek error");
        if (page.file_->write (arena_ + slotidx*pagesize_, page.masters_*pagesize_) != page.masters_*pagesize_) ERR("Write error");
        // release dirty flag
        page.markcnt_ = 0;
        // update dumpcnt_ if prev dump was 'too far away'
        if (!(page.page_ >= last_dumped_ && page.page_ <= last_dumped_ + (GAP_FACTOR-1)))
            dumpcnt_ ++;
        // remember last dumped
        last_dumped_ = page.page_+page.masters_; 
    }
}

void Pager_imp::dumpslots_ (uint32 slotidx, uint32 count)
{
#ifdef PAGER_IMP_DEBUG
    if (slotidx >= poolsize_)
        ERR("dumpslot_: slot index out of range");
#endif 
    Page& page = pages_ [slotidx];
#ifdef PAGER_IMP_DEBUG
    if (page.free_)
        ERR("dumpslot_: free slot passed");
#endif
    // write it 
    FilePos fileoff = page.page_*pagesize_;
    if (page.file_->seek (fileoff) != fileoff) ERR("Seek error");
    if (page.file_->write (arena_ + slotidx*pagesize_, pagesize_*count) != pagesize_*count) ERR("Write error");

    // update dumpcnt_ if prev dump was 'too far away'
    if (!(page.page_ >= last_dumped_ && page.page_ <= last_dumped_ + (GAP_FACTOR-1)))
        dumpcnt_ ++;
    // remember last dumped
    last_dumped_ = page.page_+1; 
}


void Pager_imp::free_ (uint32 slotidx)
{
#ifdef PAGER_IMP_DEBUG
    if (slotidx >= poolsize_)
        ERR("free_: slot index out of range");
#endif 
    Page& master_page = pages_ [slotidx];
#ifdef PAGER_IMP_DEBUG
    if (master_page.free_)
        ERR("free_: slot allready free");
    if (!master_page.masters_)
        ERR("free_: slot is subordinate");
    if (master_page.lockcnt_)
        ERR("free_: slotrange is locked");
#endif
    uint32 end_slot = slotidx + master_page.masters_;
    for (uint32 si = slotidx; si < end_slot; si ++)
    {
        Page& page = pages_ [si];
        if (si == slotidx)
        {
#ifdef PAGER_IMP_DEBUG
            if (page.mrulist_pos_ == mrulist_.end ())
                ERR("free_: master slot MRU list position refers to the end of MRU list");
            if (*page.mrulist_pos_ != slotidx)
                ERR("free_: master slot MRU list position does not refer to itsef");
#endif
            mrulist_.erase (page.mrulist_pos_);
            page.useno_ = UINT64_MAX;
            int delcnt = addrmap_.erase (PageKey (*page.file_, page.page_));
#ifdef PAGER_IMP_DEBUG
            if (!delcnt)
                ERR("free_:key not found in addrmap")
            if (mrulist_.size() != addrmap_.size ()) 
                ERR("free_:MRUlist size does not match ADDRMAP size!")
#endif
        }
#ifdef PAGER_IMP_DEBUG
        else
        {
            if (page.free_)
                ERR("free_: slot allready free");
            if (page.masters_)
                ERR("free_: subordinate slot marked as master");
            if (page.lockcnt_)
                ERR("free_: subordinate slot is locked");
        }
#endif
        markfree_ (si);
    }
}

void Pager_imp::freeslot_ (uint32 slotidx)
{
#ifdef PAGER_IMP_DEBUG
    if (slotidx >= poolsize_)
        ERR("freeslot_: slot index out of range");
#endif 
    Page& page = pages_ [slotidx];
#ifdef PAGER_IMP_DEBUG
    if (page.free_)
        ERR("freeslot_: slot allready free");
    if (page.masters_ && page.lockcnt_)
        ERR("freeslot_: page is locked");
#endif
    if (page.masters_)
    {
#ifdef PAGER_IMP_DEBUG
        if (page.mrulist_pos_ == mrulist_.end ())
            ERR("freeslot_: master slot MRU list position refers to the end of MRU list");
        if (*page.mrulist_pos_ != slotidx)
            ERR("freeslot_: master slot MRU list position does not refer to itsef");
#endif
        mrulist_.erase (page.mrulist_pos_);
        page.useno_ = UINT64_MAX;
        int delcnt = addrmap_.erase (PageKey (*page.file_, page.page_));
#ifdef PAGER_IMP_DEBUG
        if (!delcnt)
            ERR("freeslot_:key not found in addrmap")
        if (mrulist_.size() != addrmap_.size ()) 
            ERR("freeslot_:MRUlist size does not match ADDRMAP size!")
#endif
    }
    markfree_ (slotidx);
}

void Pager_imp::read_ (uint32 slotidx, File& file, FilePos pageno, uint32 count)
{
#ifdef PAGER_IMP_DEBUG
    if (slotidx + count > poolsize_)
        ERR("read_: slot index out of range");
    if (count > MAX_PAGEROW_LEN)
        ERR("read_: slotrange length is too big (>MAX_PAGEROW_LEN)");
#endif 
    if (file.seek (pageno*pagesize_) != pageno*pagesize_) throw IOError ("Seek error");
    if (file.read (arena_ + slotidx*pagesize_, count*pagesize_) == -1) throw IOError ("Read error"); // beyonf EOF read may return lesser then requested. This is Ok (?-for fake, what about fetch?)
    for (uint32 si = 0; si < count; si ++)
    {
        Page& page = pages_ [slotidx + si];
        page.page_ = pageno + si;
        page.file_ = &file;
    }
}

void Pager_imp::lock_ (uint32 slotidx)
{
#ifdef PAGER_IMP_DEBUG
    if (slotidx >= poolsize_)
        ERR("lock_: slot index out of range");
#endif 
    Page& page = pages_ [slotidx];
#ifdef PAGER_IMP_DEBUG
    if (page.free_)
        ERR("lock_: page is free");
    if (!page.masters_)
        ERR("lock_: page is subordinate");
#endif
    page.lockcnt_ ++;
}

void Pager_imp::unlock_ (uint32 slotidx)
{
#ifdef PAGER_IMP_DEBUG
    if (slotidx >= poolsize_)
        ERR("unlock_: slot index out of range");
#endif 
    Page& page = pages_ [slotidx];
#ifdef PAGER_IMP_DEBUG
    if (page.free_)
        ERR("unlock_: page is free");
    if (!page.masters_)
        ERR("unlock_: page is subordinate");
#endif
    if (page.lockcnt_ > 0)
        page.lockcnt_ --;
}

bool Pager_imp::locked_ (uint32 slotidx)
{
#ifdef PAGER_IMP_DEBUG
    if (slotidx >= poolsize_)
        ERR("locked_: slot index out of range");
#endif 
    Page& page = pages_ [slotidx];
#ifdef PAGER_IMP_DEBUG
    if (page.free_)
        ERR("locked_: page is free");
    if (!page.masters_)
        ERR("locked_: page is subordinate");
#endif
    return page.lockcnt_ > 0;
}

void Pager_imp::mark_ (uint32 slotidx)
{
#ifdef PAGER_IMP_DEBUG
    if (slotidx >= poolsize_)
        ERR("mark_: slot index out of range");
#endif 
    Page& page = pages_ [slotidx];
#ifdef PAGER_IMP_DEBUG
    if (page.free_)
        ERR("mark_: page is free");
    if (!page.masters_)
        ERR("mark_: page is subordinate");
#endif
    page.markcnt_ ++;
}

void Pager_imp::unmark_ (uint32 slotidx)
{
#ifdef PAGER_IMP_DEBUG
    if (slotidx >= poolsize_)
        ERR("unmark_: slot index out of range");
#endif 
    Page& page = pages_ [slotidx];
#ifdef PAGER_IMP_DEBUG
    if (page.free_)
        ERR("unmark_: page is free");
    if (!page.masters_)
        ERR("unmark_: page is subordinate");
#endif
    if (page.markcnt_ > 0)
        page.markcnt_ --;
}

bool Pager_imp::marked_ (uint32 slotidx)
{
#ifdef PAGER_IMP_DEBUG
    if (slotidx >= poolsize_)
        ERR("marked_: slot index out of range");
#endif 
    Page& page = pages_ [slotidx];
#ifdef PAGER_IMP_DEBUG
    if (page.free_)
        ERR("marked_: page is free");
    if (!page.masters_)
        ERR("marked_: page is subordinate");
#endif
    return page.markcnt_ > 0;
}

void Pager_imp::init_ (uint32 pagesize, uint32 poolsize)
{
    // remember pagesize and poolsize
    pagesize_ = pagesize;
    poolsize_ = poolsize;
    // allocate page descriptors array
    pages_ = new Page [poolsize_];
    if (!pages_) ERR("Not enough memory for page pool");
    // allocate paged memory arena
    arenabuf_ = new char [poolsize_*pagesize_ + MEM_PAGE_SIZE];
    // align by MEMPAGESIZE boundary
    uint64 off = ((uint64) arenabuf_) % MEM_PAGE_SIZE;
    if (off)
        arena_ = arenabuf_ + MEM_PAGE_SIZE - off;
    else
        arena_ = arenabuf_;
    // populate free list : initially all nodes are free. Also make sure mulist_pos_ point to the end of mrulist
    for (uint32 slotidx = 0; slotidx < poolsize_; slotidx ++)
    {
        markfree_ (slotidx);
        pages_ [slotidx].mrulist_pos_ = mrulist_.end ();
    }
    // allocate temporary (helper) buffer
    hlpbuf_ = new uint32 [poolsize_];
    if (!hlpbuf_) ERR ("Not enough memory for temp set");
}

void Pager_imp::detach_ (bool freedata)
{
    // dump all dirty slotranges, remembering them in hlpbuf_ for further freing
    // (we cnot free them right here because map iterator gets invalidated on element removal)
    uint32 toFreeNo = 0;
    for (Pkeymap::iterator itr = addrmap_.begin (); itr != addrmap_.end (); itr ++)
    {
        uint32 slotidx = (*itr).second;
        dump_ (slotidx);
        hlpbuf_ [toFreeNo ++] = slotidx;
#ifdef PAGER_IMP_DEBUG
        if (toFreeNo > poolsize_)
            ERR("Too many entries found in addrmap (>=poolsize_)");
#endif
    }
    // now free all slotranges saved in hlpbuf_
    for (uint32 delidx = 0; delidx < toFreeNo; delidx ++)
        free_ (hlpbuf_ [delidx]);
#ifdef PAGER_IMP_DEBUG
    if (addrmap_.size ()) 
        ERR("addrmap not empty after free");
    if (mrulist_.size ()) 
        ERR("mrulist not empty after free");
#endif
    if (freedata)
    {
        // remove everything from freelist as well - after detach_ the init_ should be called, so freelist will populate again
        freelist_.clear ();
        // delete all the buffers allocated during initialization
        if (pages_)  { delete [] pages_;  pages_ = NULL;  }
        if (arenabuf_) { delete [] arenabuf_; arenabuf_ = NULL; arena_ = NULL; }
        if (hlpbuf_) { delete [] hlpbuf_; hlpbuf_ = NULL; }
    }
}

uint32 Pager_imp::fetch_ (File& file, FilePos pageno, uint32 count)
{
#ifdef PAGER_IMP_DEBUG
    if (count > MAX_PAGEROW_COUNT) 
        ERR("fetch_: too many pages requested");
#endif
    // check whether there is an exact match
    Pkeymap::iterator itr = addrmap_.find (PageKey (file, pageno));
    if (itr != addrmap_.end ())
    {
        uint32 slotidx = (*itr).second;
        if (pages_ [slotidx].masters_ == count)
        {
            popmru_ (slotidx);
            hits_ += count;
            return slotidx;
        }
    }

    // find overlaps
    uint32 common_count = process_overlaps_ (file, pageno, count);
    if (common_count == count && uninterrupted_hlp_range_ (common_count))
        return assemble_hlp_slotrange_ (common_count);

    // allocate space for the count pages. Do not discard the common pages.
    uint32 slotidx = allocate_ (count, common_count);
    // move / read the pages
    fill_ (slotidx, file, pageno, count, common_count);
    // free common slots
    free_hlp_buf_ (common_count);
    // return the new first page
    return slotidx;
}

uint32 Pager_imp::fake_ (File& file, FilePos pageno, uint32 count)
{
#ifdef PAGER_IMP_DEBUG
    if (count > MAX_PAGEROW_COUNT) 
        ERR("fake_: too many pages requested");
#endif
    // check whether there is an exact match
    Pkeymap::iterator itr = addrmap_.find (PageKey (file, pageno));
    if (itr != addrmap_.end ())
    {
        uint32 slotidx = (*itr).second;
        if (pages_ [slotidx].masters_ == count)
        {
            popmru_ (slotidx);
            return slotidx;
        }
    }
    // find overlaps
    uint32 common_count = process_overlaps_ (file, pageno, count);
    // allocate space for the count pages. Do not discard the common pages.
    uint32 slotidx = allocate_ (count, common_count);
    // set the file refs and page numbers for slots
    fakefill_ (slotidx, file, pageno, count);
    // free common slots
    free_hlp_buf_ (common_count);
    // return the new first page
    return slotidx;
}

uint32 Pager_imp::allocate_ (uint32 count, uint32 preserved_count)
{   
    // find the best slot
    uint32 slotno = lookupfree_ (count, preserved_count);
    // make the range starting from slotno, dumping / changing overlapping slotranges as needed
    makerange_ (slotno, count);
    // return the master slot
    return slotno;
}

void Pager_imp::makerange_ (uint32 slotidx, uint32 count)
{
#ifdef PAGER_IMP_DEBUG
    if (slotidx + count > poolsize_)
        ERR("makerange_: request on the out-of-boundaries range");
    if (count > MAX_PAGEROW_LEN)
        ERR("makerange_: slotrange is too long (>MAX_PAGEROW_LEN)");
#endif

    // for every slot
    uint32 si;
    for (si = slotidx; si < slotidx + count;)
    {
        Page& slot = pages_ [si];
        // if page used
        if (!slot.free_)
        {
            // find master
            uint32 masteridx = getmaster_ (si);
            Page& masterslot = pages_ [masteridx];
            uint32 rangelen = masterslot.masters_;

#if defined (FAVOR_LONG_DUMPS)
            if (masterslot.markcnt_)
            {
                uint32 dumpcnt;
                Pkeymap::iterator pagerow_begin = dump_len_ (masteridx, dumpcnt);
#ifdef PAGER_IMP_DEBUG
                if (dumpcnt == 0)
                    ERR("makerange_: internal");
#endif
                dumpx_ (pagerow_begin);
#ifdef PAGER_IMP_DEBUG
                if (masterslot.markcnt_)
                    ERR("makerange_: internal");
#endif
            }
#endif

#ifdef PAGER_IMP_DEBUG
            // DEBUG check whether not locked
            if (masterslot.lockcnt_)
                ERR("makerange_: overlap with locked range");
#endif
            // save markcnt      
            uint32 markcnt = masterslot.markcnt_;

            // separate preceeding portion if any:
            // shrink the slotrange to the preceeding portion only
            if (masteridx < slotidx)
                masterslot.masters_ = slotidx - masteridx;
            else 
                // otherwise, remove the page from master's structures
                removemaster_ (masteridx);

            // dump inner portion if page dirty
            uint32 inner_begin = max_ (slotidx, masteridx);
            uint32 inner_end   = min_ (slotidx + count, masteridx + rangelen); 
#if defined (FAVOR_MIN_WRITE_VOLUME)
            if (markcnt)
                dumpslots_ (inner_begin, inner_end - inner_begin);
#endif

            // if there is postceeding portion, separate it; copy mark count
            if (slotidx + count < masteridx + rangelen)
            {
                pages_ [slotidx + count].masters_ = ((masteridx + rangelen) - (slotidx + count));
                pages_ [slotidx + count].markcnt_ = markcnt;
                addmaster_ (slotidx + count);
            }
#ifdef PAGER_IMP_DEBUG
            if (si >= inner_end)
                ERR("Internal");
#endif
            si = inner_end;
        }
        else // else (page free)
        {
            markused_ (si);
            si ++;
        }
    }
    // now all slots in a range are taken care of. Walk them and
    for (si = slotidx; si < slotidx + count; si ++)
    {
        Page& slot = pages_ [si];
        // mark first page as master and add to master's structures
        if (si == slotidx)
        {
            slot.masters_ = count;
            slot.markcnt_ = 0;

        }
        else // mark rest as subordinates
        {
            slot.masters_ = 0;
            slot.markcnt_ = 0;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////
// Strategy methods

uint32 Pager_imp::lookupfree_ (uint32 count, uint32 preserved_count)
{
    // pick from free list such that has 'count' unlocked slots left
    uint64 best_weight = 0;
    uint32 iter_count = 0;
    uint32 best_slot = UINT32_MAX;

    // walk free list first
    for (Ilist::iterator fli = freelist_.begin (); fli != freelist_.end () && iter_count < UNIMPROVED_COUNT; fli ++)
    {
        uint64 weight;
        uint32 slot_idx = *fli;
        switch (check_avail_left_ (slot_idx, count, preserved_count, weight))
        {
        case ALL_FREE:  
            return slot_idx;
        case WEIGHTED:  
            if (best_weight < weight)
            {
                best_weight = weight;
                best_slot = slot_idx;
                iter_count = 0;
            }
            else
                iter_count ++;
        case NOT_AVAIL:  
            break;
        }
    }
            
    // pick from LRU list such that has 'count' unlocked slots left
    for (Ilist::reverse_iterator lruitr = mrulist_.rbegin (); lruitr != mrulist_.rend () && iter_count < UNIMPROVED_COUNT; lruitr ++)
    {
        uint64 weight;
        uint32 slot_idx = *lruitr;
        switch (check_avail_left_ (slot_idx, count, preserved_count, weight))
        {
        case ALL_FREE:  
            return slot_idx;
        case WEIGHTED:  
            if (best_weight < weight)
            {
                best_weight = weight;
                best_slot = slot_idx;
                iter_count = 0;
            }
            else
                iter_count ++;
        case NOT_AVAIL:
            break;
        }
    }
    if (best_slot != UINT32_MAX)
        return best_slot;

    throw NoCacheSpace ();
}

Pager_imp::AVAIL Pager_imp::check_avail_left_ (uint32 slotidx, uint32 count, uint32 preserved_count, uint64& weight)
{
#ifdef DEBUG_PAGHER_IMP
    if (slotidx >= poolsize_)
        ERR("check_avail_left_: slot index out of range");
#endif
    if (slotidx + count > poolsize_)
        return NOT_AVAIL;
    uint32 preserved_idx = 0; 
    bool allfree = true;
    weight = 0L;
    uint64 oldest_useno = cur_pageuse_;
    if (mrulist_.size ()) 
        oldest_useno = pages_ [*(mrulist_.rbegin ())].useno_;

    for (uint32 i = 0; i < count; i ++)
    {
        if (pages_ [slotidx + i].free_)
        {
#if defined (FAVOR_LONG_DUMPS)
             weight += (cur_pageuse_ - oldest_useno + 1)*LONG_ENOUGH_SEQ*EMPTY_SLOT_FACTOR;
#elif defined (FAVOR_MIN_WRITE_VOLUME)
            weight += (cur_pageuse_ - oldest_useno + 1)*EMPTY_SLOT_FACTOR;
#else
            #pragma error "write preference policy not defined"
#endif
        }
        else
        {
            allfree = false;
            // check whether in preserved
            for (int pidx = 0; pidx < preserved_count; pidx ++)
                if (slotidx + i == hlpbuf_ [pidx])
                    return NOT_AVAIL;
            // check whether locked
            uint32 masteridx = getmaster_ (slotidx + i);
            Page& masterpage = pages_ [masteridx];
            if (masterpage.lockcnt_)
                return NOT_AVAIL;
            if (!masterpage.markcnt_)
            {
#if defined (FAVOR_LONG_DUMPS)
                weight += (cur_pageuse_ - masterpage.useno_ + 1)*LONG_ENOUGH_SEQ*CLEAN_SLOT_FACTOR;
#elif defined (FAVOR_MIN_WRITE_VOLUME)
                weight += (cur_pageuse_ - masterpage.useno_ + 1)*CLEAN_SLOT_FACTOR;
#else
            #pragma error "write preference policy not defined"
#endif
            }
            else
            {
#if defined (FAVOR_LONG_DUMPS)
                uint32 dumprow_len;
                dump_len_ (masteridx, dumprow_len);
                weight += (cur_pageuse_ - oldest_useno + 1)*dumprow_len;
#elif defined (FAVOR_MIN_WRITE_VOLUME)
                weight += (cur_pageuse_ - oldest_useno + 1);
#else
            #pragma error "write preference policy not defined"
#endif
            }
        }
    }
    if (allfree) return ALL_FREE;
    else return WEIGHTED;
}

Pager_imp::Pkeymap::iterator Pager_imp::dump_len_ (uint32 slotidx, uint32& length)
{
    Page& page = pages_ [slotidx];
    length = page.masters_;
    PageKey startKey (*page.file_, page.page_);
    Pkeymap::iterator itr_r = addrmap_.find (startKey);
#ifdef _MYDEBUG
    if (itr_r == addrmap_.end ()) ERR("dump_len: for non-master slot requested");
#endif
    Pkeymap::iterator itr_f = itr_r;
    // roll back untill there is continous line of used marked pages
    while (itr_r != addrmap_.begin () && length < LONG_ENOUGH_SEQ) 
    {
        Pkeymap::iterator next_step = itr_r;
        next_step --;
        if ((*next_step).first.file_ != page.file_) 
            break;
        Page& prevpage = pages_ [(*next_step).second];
        if (!prevpage.markcnt_)
            break;
        if (prevpage.page_ + prevpage.masters_ + GAP_FACTOR < (*itr_r).first.page_)
            break;
        itr_r = next_step;
        length += prevpage.masters_;
    }
    // roll forward untill there is continous line of pages
    while (length < LONG_ENOUGH_SEQ)
    {
        Pkeymap::iterator next_step = itr_f;
        next_step ++;
        if (next_step == addrmap_.end ()) 
            break;
        if ((*next_step).first.file_ != page.file_) 
            break;
        Page& curpage = pages_ [(*itr_f).second];
        if ((*next_step).first.page_ > curpage.page_ + curpage.masters_ + GAP_FACTOR)
            break;
        if (!pages_[(*next_step).second].markcnt_)
            break;
        itr_f = next_step;
        length += curpage.masters_;
    }
    return itr_r;
}

void Pager_imp::dumpx_ (Pager_imp::Pkeymap::iterator begin)
{
    uint32 length = 0;
    Pkeymap::iterator itr_f = begin;
    while (length < MAX_DUMP_LEN)
    {
        uint32 slotidx = (*itr_f).second;
        dump_ (slotidx);
        Pkeymap::iterator next_step = itr_f;
        next_step ++;
        if (next_step == addrmap_.end ()) 
            break;
        if ((*next_step).first.file_ != (*begin).first.file_) 
            break;
        Page& curpage = pages_ [(*itr_f).second];
        if ((*next_step).first.page_ > curpage.page_ + curpage.masters_ + GAP_FACTOR)
            break;
        if (!pages_[(*next_step).second].markcnt_)
            break;
        itr_f = next_step;
        length += curpage.masters_;
    }
}

///////////////////////////////////////////////////////////////////////////////////////
// Public methods

void* Pager_imp::fetch (File& file, uint64 pageno, bool lock, uint32 count)
{
    uint32 slotidx = fetch_ (file, pageno, count);
    if (lock)
        lock_ (slotidx);
    return slotaddr_ (slotidx);
}

void* Pager_imp::fake (File& file, uint64 pageno, bool lock, uint32 count)
{
    uint32 slotidx = fake_ (file, pageno, count);
    if (lock)
        lock_ (slotidx);
    return slotaddr_ (slotidx);
}

bool Pager_imp::locked (const void* buffer)
{
    return locked_ (slotidx_ (buffer));
}

void Pager_imp::lock (const void* buffer)
{
    lock_ (slotidx_ (buffer));
}

void Pager_imp::unlock (const void* buffer)
{
    unlock_ (slotidx_ (buffer));
}

bool Pager_imp::marked (const void* buffer)
{
    return marked_ (slotidx_ (buffer));
}

void Pager_imp::mark (const void* buffer)
{
    mark_ (slotidx_ (buffer));
}

void Pager_imp::unmark (const void* buffer)
{
    unmark_ (slotidx_ (buffer));
}

bool Pager_imp::commit (File& file)
{
    // for every slotrange belonging to a file
    for (Pkeymap::iterator itr = addrmap_.lower_bound (PageKey (file, 0L));
        itr != addrmap_.end () && (*itr).first.file_ == &file;
        itr ++)
    {
#ifdef PAGER_IMP_DEBUG
        uint32 slotidx = (*itr).second;
        if (slotidx >= poolsize_) 
            ERR("commit: slot index out of range");
        Page& page = pages_ [slotidx];
        if (page.free_)
            ERR("commit: free page in addrmap");
        if (!page.masters_)
            ERR("commit: subordinate page in addrmap");
        if (page.lockcnt_)
            ERR("commit: locked page found");
#endif 
        dump_ ((*itr).second);
    }
    return file.commit ();
}

bool Pager_imp::chsize (File& file, FilePos newSize)
{
    // if newSize < current size:
    if (newSize < file.length ())
    {
        // mark all releasing pages as free. Do not dump anything
        uint32 toFreeNo = 0;
        // for every slotrange belonging to a file
        uint64 first_freed_page = (newSize + pagesize_ - 1) / pagesize_;
        for (Pkeymap::iterator itr = addrmap_.lower_bound (PageKey (file, first_freed_page));
            itr != addrmap_.end () && (*itr).first.file_ == &file;
            itr ++)
        {
#ifdef PAGER_IMP_DEBUG
            uint32 slotidx = (*itr).second;
            if (slotidx >= poolsize_)
                ERR ("chsize: slot index out of range");
            Page& page = pages_ [slotidx];
            if (page.free_)
                ERR("chsize: free page in addrmap");
            if (!page.masters_)
                ERR("chsize: subordinate page in addrmap");
            if (page.lockcnt_)
                ERR("chsize: locked page found");
#endif 
            // save into hlpbuf_ for later freing (cannot free here - removal from map invalidates iterator)
            hlpbuf_ [toFreeNo ++] = (*itr).second;
        }
        for (uint32 d = 0; d < toFreeNo; d ++)
            free_ (hlpbuf_ [d]);
    }
    // call File::chsize
    return file.chsize (newSize);
}

bool Pager_imp::close (File& file)
{
    detach (file);
    return file.close ();
}

bool Pager_imp::detach (File& file)
{
    uint32 toFreeNo = 0;
    // for every slotrange belonging to a file
    for (Pkeymap::iterator itr = addrmap_.lower_bound (PageKey (file, 0L));
        itr != addrmap_.end () && (*itr).first.file_ == &file;
        itr ++)
    {
        uint32 slotidx = (*itr).second;
#ifdef PAGER_IMP_DEBUG
        Page& page = pages_ [slotidx];
        if (page.free_)
            ERR("detach: free page in addrmap");
        if (!page.masters_)
            ERR("detach: subordinate page in addrmap");
        if (page.lockcnt_)
            ERR("detach: locked page found");
#endif 
        // dump and save into hlpbuf_ for later freing (cannot free here - removal from map invalidates iterator)
        dump_ (slotidx);
        hlpbuf_ [toFreeNo ++] = slotidx;
#ifdef PAGER_IMP_DEBUG
        if (toFreeNo > poolsize_)
            ERR("detach: too many pages found (>poolsize)");
#endif 
    }
    // free all saved slotranges
    for (uint32 d = 0; d < toFreeNo; d ++)
        free_ (hlpbuf_ [d]);
    return true;
}

uint64 Pager_imp::pageno (const void* data)
{
    return pages_ [slotidx_ (data)].page_;
}

File& Pager_imp::file (const void* data)
{
    return *pages_ [slotidx_ (data)].file_;
}

void* Pager_imp::pageaddr (const void* ptr, uint32* count)
{
    if (((const char*) ptr) < arena_ || ((const char*) ptr) > arena_ + pagesize_*poolsize_)
        return NULL;
    uint32 off = ((const char*) ptr) - arena_;
    uint32 slotidx = off / pagesize_;
    if (pages_ [slotidx].free_) 
        return NULL;
    uint32 masterslotidx = getmaster_ (slotidx);
    if (count)
        *count = pages_ [masterslotidx].masters_;
    return arena_ + masterslotidx * pagesize_;
}

void* Pager_imp::checkpage (File& file, uint64 pageno, uint32* count)
{
    Pkeymap::iterator itr = addrmap_.find (PageKey (file, pageno));
    if (itr == addrmap_.end ())
        return NULL;
    else
    {
        uint32 slotidx = (*itr).second;
#ifdef PAGER_IMP_DEBUG
        if (slotidx >= poolsize_)
            ERR("checkpage: slot index out of range (>=poolsize_)");
        if (pages_ [slotidx].free_)
            ERR("checkpage: addrmap refers to free page");
        if (!pages_ [slotidx].masters_)
            ERR("checkpage: addrmap refers to subordinate page");
#endif
        if (count)
            *count = pages_ [slotidx].masters_;
        return arena_ + slotidx * pagesize_;
    }
}

uint64 Pager_imp::getDumpCount () const
{
    return dumpcnt_;
}

uint64 Pager_imp::getHitsCount () const
{
    return hits_;
}

uint64 Pager_imp::getMissesCount () const
{
    return misses_;
}

uint32 Pager_imp::getPageSize () const
{
    return pagesize_;
}

uint32 Pager_imp::getPoolSize () const
{
    return poolsize_;
}

bool Pager_imp::setPoolSize (uint32 poolsize)
{
#ifdef PAGER_IMP_DEBUG
    if (poolsize == 0) ERR("setPoolSize: Zero pool size requested");
#endif
    if (poolsize != poolsize_)
    {
        detach_ ();
        init_ (pagesize_, poolsize);
    }
    return true;
}

Pager& PagerFactory_imp::create (uint32 pagesize, uint32 poolsize)
{
    return *new Pager_imp (pagesize, poolsize);
}


};
