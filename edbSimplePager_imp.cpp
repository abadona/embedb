// Copyright 2001 Victor Joukov, Denis Kaznadzey
#define edbSimplePagerFactory_defined
#include "edbSimplePager_imp.h"
#include <vector>

namespace edb
{
static SimplePagerFactory theFactory;
PagerFactory& simplePagerFactory = theFactory;

#define GAP_FACTOR 1
#define LRU_CHECK_FACTOR 20
#define LONG_ENOUGH_SEQ  8
#define MAX_PAGEROW_LEN  10

SimplePager::SimplePager (uint32 pagesize, uint32 poolsize)
:
pagesize_ (0),
poolsize_ (0),
pages_ (NULL),
arena_ (NULL),
hlpbuf_ (NULL),
dumpcnt_ (0),
hits_ (0),
misses_ (0)
{
    init_ (pagesize, poolsize);
}

void SimplePager::init_ (uint32 pagesize, uint32 poolsize)
{
    pagesize_ = pagesize;
    poolsize_ = poolsize;

    pages_ = new Page [poolsize_];
    if (!pages_) ERR("Not enough memory for page pool");
    arena_ = new char [poolsize_*pagesize_];
    if (!arena_) ERR ("Not enough memory for page cache data");
    for (int i = 0; i < poolsize_; i ++)
    {
        freelist_.push_front (i);
        pages_ [i].freelist_pos_ = freelist_.begin ();
    }
    hlpbuf_ = new uint32 [poolsize_];
    if (!hlpbuf_) ERR ("Not enough memory for temp set");
}

SimplePager::~SimplePager ()
{
    if (pages_) delete [] pages_;
    if (arena_) delete [] arena_;
    if (hlpbuf_) delete [] hlpbuf_;
}

uint32 SimplePager::find_master_ (uint32 page_idx)
{
    while (page_idx >= 0)
    {
#ifdef _MYDEBUG
        if (pages_ [page_idx].free_) ERR("Page subordinate structure corrupt");
#endif
        if (pages_ [page_idx].masters_)
            return page_idx;
        page_idx --;

    }
    ERR("Page subordinate structure corrupt");
    return 0;
}
void SimplePager::free_intersectors_ (File& file, uint64 pageno, uint32 count)
{
    uint64 begin_pgno = (pageno < MAX_PAGEROW_LEN)? 0 : pageno - MAX_PAGEROW_LEN;
    uint64 end_pgno = pageno + count;

    Pkeymap::iterator beg_itr = addrmap_.lower_bound (PageKey (file, begin_pgno));
    Pkeymap::iterator end_itr = addrmap_.lower_bound (PageKey (file, end_pgno));

    int toDelCount = 0;
    for (Pkeymap::iterator itr = beg_itr; itr != end_itr; itr ++)
    {
        uint32 pageidx = (*itr).second;
        Page page = pages_ [pageidx];
        if (page.masters_ && page.page_ + page.masters_ > pageno)
            hlpbuf_ [toDelCount ++] = pageidx;
    }

    for (int ii = 0; ii < toDelCount; ii ++)
    {
        dump_ (hlpbuf_[ii]);
        free_ (hlpbuf_[ii]);
    }
}

uint32 SimplePager::fetch_ (File& file, uint64 pageno, uint32 count)
{
    // check weather in cache
    Pkeymap::iterator itr = addrmap_.find (PageKey (file, pageno));
    if (itr != addrmap_.end ()) // if yes 
    {
        uint32 pgidx = (*itr).second;
        if (pages_[pgidx].masters_ == count) 
        {
            hits_ ++;
            // put in front of mru list
            pop_ (pgidx);
            return pgidx;
        }
    }

    // dump and free all slotranges that are covered or intersecting with the requested one
    misses_ ++;
    free_intersectors_ (file, pageno, count);
    // find the index for the page
    uint32 page_idx = getFree_ (count);
    load_ (page_idx, file, pageno, count);
    return page_idx;
}

uint32 SimplePager::fake_ (File& file, uint64 pageno, uint32 count)
{
    // check weather in cache
    Pkeymap::iterator itr = addrmap_.find (PageKey (file, pageno));
    if (itr != addrmap_.end ()) // if yes 
    {
        uint32 pgidx = (*itr).second;
        if (!pages_[pgidx].masters_ != count)
        {
            uint32 masteridx = find_master_ (pgidx);
            dump_ (masteridx);
            free_ (masteridx);
        }
        else
        {
            hits_ ++;
            // put in front of mru list
            pop_ (pgidx);
            return pgidx;
        }
    }
    else
    {
        // dump and free all slotranges that are covered or intersecting with the requested one
        free_intersectors_ (file, pageno, count);
    }
    misses_ ++;
    // find the index for the page
    uint32 page_idx = getFree_ (count);
    markloaded_ (page_idx, file, pageno, count);
    return page_idx;
}

uint32 SimplePager::getFree_ (uint32 count) 
{
    uint32 slotno;
    bool found = false;

    // pick from free list such that has 'count' unlocked slots left
    for (Ilist::iterator fli = freelist_.begin (); fli != freelist_.end () && !found; fli ++)
        if (checkslotno_ (*fli, count))
            slotno = *fli, found = true;
    // if no such slot in free list - pick from lru list such that has 'count' unlocked slots left. 
    if (!found)
    {
        // Check several to pick the longest continous range for dumping
        uint32 longest_length = 0;
        Pkeymap::iterator longest_start;
        uint32 longest_pos;
        uint32 number_checked = 0;

        for (Ilist::reverse_iterator lruitr = mrulist_.rbegin (); lruitr != mrulist_.rend () && !found; lruitr ++)
        {
            uint32 cs = *lruitr;
#ifdef _MYDEBUG
            if (!pages_[cs].masters_) ERR("Non-master in MRU list!");
#endif
            if (checkslotno_ (cs, count))
            {
                Page& page = pages_ [cs];
                // check continous range
                uint32 cur_length;
                Pkeymap::iterator cur_start = check_cont (cs, cur_length);
                if (cur_length > longest_length)
                {
                    longest_length = cur_length;
                    longest_start = cur_start;
                    longest_pos = *lruitr;
                }
                number_checked ++;
                if (longest_length >= LONG_ENOUGH_SEQ)
                    break;
                if (number_checked > LRU_CHECK_FACTOR)
                    break;
                
            }
        }
        if (number_checked)
        {
            dumpX_ (longest_start);
            slotno = longest_pos, found = true;
        }

    }
    // if no stretch could be found, raise exception
    if (!found)
        ERR("Could not get enough continous space for paging")

    // free the 'count' slots starting form 'slotno'
    for (uint32 page_idx = slotno; page_idx < slotno + count; page_idx ++)
    {
        Page& page = pages_ [page_idx];
        if (page.masters_ && !page.free_)
        {
            dump_ (page_idx);
            free_ (page_idx);
        }
    }
    // return first slot number in stretch
    return slotno;
}

bool SimplePager::checkslotno_ (uint32 page_idx, uint32 count)
{
    for (uint32 pgno = page_idx; pgno < page_idx + count && pgno < poolsize_; pgno ++)
    {
        Page& page = pages_ [pgno];
        if (!page.free_ && page.lockcnt_)
            return false;
    }
    return (pgno == page_idx + count);
}

void SimplePager::load_ (uint32 page_idx, File& file, uint64 pageno, uint32 count)
{
    Page& page = pages_ [page_idx];
#ifdef _MYDEBUG
    // check for consistency
    for (uint32 pi = page_idx; pi < page_idx + count; pi ++)
        if (!pages_[pi].free_) ERR("loading into occupied page!")
#endif
    // if before file end
    FilePos flength = file.length ();
    FilePos pageoff = pageno*pagesize_;
    if (flength >= pageoff)
    {
        if (file.seek (pageoff) != pageoff) ERR("Seek error");
        uint32 rdsize = min_ (pagesize_* count, flength - pageoff);
        if (file.read (arena_ + page_idx*pagesize_, rdsize) != rdsize) ERR("Read error");
    }
    markloaded_ (page_idx, file, pageno, count);
}

void SimplePager::markloaded_ (uint32 page_idx, File& file, uint64 pageno, uint32 count)
{
    for (uint32 pi = 0; pi < count; pi ++)
    {
        Page& page = pages_ [page_idx + pi];
#ifdef _MYDEBUG
        // check for consistency
        if (!page.free_) ERR("marking as loaded allready occupied page!");
#endif
        page.free_  = false;
        freelist_.erase (page.freelist_pos_);
        page.file_  = &file;
        page.page_  = pageno + pi;
        page.markcnt_ = 0;
        page.lockcnt_= 0;
        if (!pi)
        {
#ifdef _MYDEBUG
            if (mrulist_.size() != addrmap_.size ()) ERR("MRUlist size does not match ADDRMAP size!")
#endif
            page.masters_ = count;
            addrmap_ [PageKey (file, pageno)] = page_idx;
            mrulist_.push_front (page_idx);
            page.mrulist_pos_ = mrulist_.begin ();
#ifdef _MYDEBUG
            if (mrulist_.size() != addrmap_.size ()) ERR("MRUlist size does not match ADDRMAP size!")
#endif
        }
        else
            page.masters_ = 0;
    }
}

void SimplePager::dump_ (uint32 page_idx)
{
    Page& page = pages_ [page_idx];
#ifdef _MYDEBUG
    if (!page.masters_) ERR("Dump request on subordinate page");
    if (page.free_) ERR("Dump request on free page");
#endif
    if (page.markcnt_)
    {
        // write it 
        FilePos fileoff = page.page_*pagesize_;
        if (page.file_->seek (fileoff) != fileoff) ERR("Seek error");
        if (page.file_->write (arena_ + page_idx*pagesize_, page.masters_*pagesize_) != page.masters_*pagesize_) ERR("Write error");
        // release dirty flags
        for (uint32 pi = page_idx; pi < page_idx + page.masters_; pi ++)
            pages_ [pi].markcnt_ = 0;
    }
}

void SimplePager::dumpX_ (Pkeymap::iterator itr)
{
    File* file = (*itr).first.file_;
    while (1)
    {
        dump_ ((*itr).second);

        Pkeymap::iterator next_step = itr;
        next_step ++;
        if (next_step == addrmap_.end ()) 
            break;
        if ((*next_step).first.file_ != file) 
            break;
        if ((*next_step).first.page_ > (*itr).first.page_ + GAP_FACTOR)
            break;
        if (!pages_[(*next_step).second].markcnt_)
            break;
        itr = next_step;
    }
    dumpcnt_ ++;
}

SimplePager::Pkeymap::iterator SimplePager::check_cont (uint32 page_idx, uint32& length)
{
    length = 1;
    Page& page = pages_ [page_idx];
    PageKey startKey (*page.file_, page.page_);
    Pkeymap::iterator itr_r = addrmap_.find (startKey);
#ifdef _MYDEBUG
    if (itr_r == addrmap_.end ()) ERR("check_cont for bad slot requested");
#endif
    Pkeymap::iterator itr_f = itr_r;
    // roll back untill there is continous line of pages
    while (itr_r != addrmap_.begin ()) 
    {
        Pkeymap::iterator next_step = itr_r;
        next_step --;
        if ((*next_step).first.file_ != page.file_) 
            break;
        if ((*next_step).first.page_ < (*itr_r).first.page_ - GAP_FACTOR)
            break;
        if (!pages_[(*next_step).second].markcnt_)
            break;
        itr_r = next_step;
        length ++;
    }
    // roll forward untill there is continous line of pages
    while (1)
    {
        Pkeymap::iterator next_step = itr_f;
        next_step ++;
        if (next_step == addrmap_.end ()) 
            break;
        if ((*next_step).first.file_ != page.file_) 
            break;
        if ((*next_step).first.page_ > (*itr_f).first.page_ + GAP_FACTOR)
            break;
        if (!pages_[(*next_step).second].markcnt_)
            break;
        itr_f = next_step;
        length ++;
    }
    return itr_r;
}

void SimplePager::free_ (uint32 page_idx)
{
    if (pages_ [page_idx].free_) return;
#ifdef _MYDEBUG
    if (!pages_ [page_idx].masters_) ERR("Free request on subordinate node");
#endif
    uint32 toPage = page_idx + pages_ [page_idx].masters_;
    for (uint32 pi = page_idx; pi < toPage; pi ++)
    {
        Page& page = pages_ [pi];
        if (page.lockcnt_) ERR ("Free request on locked page");
        if (pi == page_idx)
        {
            mrulist_.erase (page.mrulist_pos_);
            if (!addrmap_.erase (PageKey (*page.file_, page.page_))) ERR ("key not found in addrmap");
#ifdef _MYDEBUG
            if (mrulist_.size() != addrmap_.size ()) ERR("MRUlist size does not match ADDRMAP size!")
#endif
        }
        page.free_ = true;
        freelist_.push_front (pi);
        page.freelist_pos_ = freelist_.begin ();
    }
}

void SimplePager::pop_ (uint32 page_idx)
{
    Page& page = pages_ [page_idx];
    if (page.mrulist_pos_ == mrulist_.begin ())
        return;
    mrulist_.erase (page.mrulist_pos_);
    mrulist_.push_front (page_idx);
    page.mrulist_pos_ = mrulist_.begin ();
#ifdef _MYDEBUG
    if (mrulist_.size() != addrmap_.size ()) ERR("MRUlist size does not match ADDRMAP size!")
#endif

}

void SimplePager::prep_detach_ ()
{
    uint32 toFreeNo = 0;
    for (Pkeymap::iterator itr = addrmap_.begin (); itr != addrmap_.end (); itr ++)
    {
        uint32 pageidx = (*itr).second;
        dump_ (pageidx);
        hlpbuf_ [toFreeNo ++] = pageidx;
    }
    for (int d = 0; d < toFreeNo; d ++)
        free_ (hlpbuf_ [d]);
#ifdef _MYDEBUG
    if (addrmap_.size ()) ERR("Error freing all pages. Some pages are not freed")
#endif
    mrulist_.clear ();
    freelist_.clear ();
    
    if (pages_)  { delete [] pages_;  pages_ = NULL;  }
    if (arena_)  { delete [] arena_;  arena_ = NULL;  }
    if (hlpbuf_) { delete [] hlpbuf_; hlpbuf_ = NULL; }
}

void* SimplePager::fetch (File& file, uint64 pageno, bool lock, uint32 count)
{
    if (count > MAX_PAGEROW_LEN) ERR("Too many pages in a row requested, max is 4");
    uint32 page_idx = fetch_ (file, pageno, count);
    if (lock)
    {
        uint32 rowsize = pages_ [page_idx].masters_;
        for (uint32 rowpos = 0; rowpos < rowsize; rowpos ++)
            pages_ [page_idx + rowpos].lockcnt_ ++;
    }
    return arena_ + page_idx * pagesize_;
}

void* SimplePager::fake (File& file, uint64 pageno, bool lock, uint32 count)
{
    if (count > MAX_PAGEROW_LEN) ERR("Too many pages in a row requested, max is 4");
    uint32 page_idx = fake_ (file, pageno, count);
    if (lock)
    {
        uint32 rowsize = pages_ [page_idx].masters_;
        for (uint32 rowpos = 0; rowpos < rowsize; rowpos ++)
            pages_ [page_idx + rowpos].lockcnt_ ++;
    }
    return arena_ + page_idx * pagesize_;
}

uint32 SimplePager::pageno_ (const void* page_address) const
{
    int pageno = (((char*) page_address) - arena_) / pagesize_;
#ifdef _MYDEBUG
    if ((((char*) page_address) < arena_) || ((((char*) page_address) - arena_) % pagesize_)) goto ERROR;
    if (pageno >= poolsize_) goto ERROR;
#endif
    return pageno;
#ifdef _MYDEBUG
ERROR:
    ERR("Wrong page address");
#endif
}

bool SimplePager::locked (const void* buffer)
{
    Page& page = pages_ [pageno_ (buffer)];
#ifdef _MYDEBUG
    if (page.free_) ERR("Lock check on free page");
    if (page.masters_) ERR("Lock check on subordinate page");
#endif
    return (page.lockcnt_ != 0);
}

void SimplePager::lock (const void* buffer)
{
    uint32 pageno = pageno_ (buffer);
#ifdef _MYDEBUG
    if (pages_ [pageno].free_) ERR("Lock requested on free page");
    if (!pages_ [pageno].masters_) ERR ("Lock requested on subordinate page");
#endif
    pages_ [pageno].lockcnt_ ++;
}

void SimplePager::unlock (const void* buffer)
{
    uint32 pageno = pageno_ (buffer);
#ifdef _MYDEBUG
    if (pages_ [pageno].free_) ERR("Unlock requested on free page");
    if (!pages_ [pageno].masters_) ERR ("Unlock requested on subordinate page");
#endif
    Page& page = pages_ [pageno];
    if (page.lockcnt_)
        page.lockcnt_ --;
}

bool SimplePager::marked (const void* buffer)
{
    Page& page = pages_ [pageno_ (buffer)];
#ifdef _MYDEBUG
    if (page.free_) ERR("Mark check on free page");
    if (page.masters_) ERR("Mark check on free page");
#endif
    return (page.markcnt_ != 0);
}

void SimplePager::mark (const void* buffer)
{
    uint32 pageno = pageno_ (buffer);
#ifdef _MYDEBUG
    if (pages_ [pageno].free_) ERR("Mark requested on free page");
    if (!pages_ [pageno].masters_) ERR ("Mark requested on subordinate page");
#endif
    pages_ [pageno].markcnt_ ++;
}

void SimplePager::unmark (const void* buffer)
{
    uint32 pageno = pageno_ (buffer);
#ifdef _MYDEBUG
    if (pages_ [pageno].free_) ERR("Unmark requested on free page");
    if (!pages_ [pageno].masters_) ERR ("Unmark requested on subordinate page");
#endif
    Page& page = pages_ [pageno];
    if (page.markcnt_)
        page.markcnt_ --;
}

bool SimplePager::commit (File& file)
{
    // for every page belonging to a file
    PageKey startKey (file, 0L);
    Pkeymap::iterator itr = addrmap_.lower_bound (startKey);
    int64 prevpage = -GAP_FACTOR-1;
    while (itr != addrmap_.end () && (*itr).first.file_ == &file)
    {
        if (prevpage < (*itr).first.page_ - GAP_FACTOR)
            dumpcnt_ ++;
        prevpage = (*itr).first.page_;

        dump_ ((*itr).second);
        itr ++;
    }
    return file.commit ();
}

bool SimplePager::chsize (File& file, FilePos newSize)
{
    // if newSize < current size:
    if (newSize < file.length ())
    {
        // mark all releasing pages as free. Do not dump anything
        int toFreeNo = 0;
        uint64 first_freed_page = (newSize + pagesize_ - 1) / pagesize_;
        PageKey startKey (file, first_freed_page);
        Pkeymap::iterator itr = addrmap_.lower_bound (startKey);
        while (itr != addrmap_.end () && (*itr).first.file_ == &file)
        {
            uint32 pageidx = (*itr).second;
            if (!pages_ [pageidx].free_ && pages_ [pageidx].masters_)
                hlpbuf_ [toFreeNo ++] = pageidx;
            itr ++;
        }
        for (int d = 0; d < toFreeNo; d ++)
            free_ (hlpbuf_ [d]);
    }
    // call File::chsize
    return file.chsize (newSize);
}

bool SimplePager::close (File& file)
{
    detach (file);
    return file.close ();
}

bool SimplePager::detach (File& file)
{
    // for every page belonging to a file
    PageKey startKey (file, 0L);
    Pkeymap::iterator itr = addrmap_.lower_bound (startKey);
    int64 prevpage = -GAP_FACTOR-1;
    int toFreeNo = 0;
    while (itr != addrmap_.end () && (*itr).first.file_ == &file)
    {
        if (prevpage < (*itr).first.page_ - GAP_FACTOR)
            dumpcnt_ ++;
        prevpage = (*itr).first.page_;
        if (!pages_ [(*itr).second].free_ && pages_ [(*itr).second].masters_)
        {
            dump_ ((*itr).second);
            hlpbuf_ [toFreeNo ++] = (*itr).second;
        }
        itr ++;
    }
    for (int d = 0; d < toFreeNo; d ++)
        free_ (hlpbuf_ [d]);
    return true;
}

uint64 SimplePager::pageno (const void* page)
{
    uint32 pgno = pageno_ (page);
    return pages_ [pgno].page_;
}

File& SimplePager::file (const void* page)
{
    uint32 pgno = pageno_ (page);
    return *pages_ [pgno].file_;
}

void* SimplePager::pageaddr (const void* ptr, uint32* count)
{
    if (((const char*) ptr) < arena_ || ((const char*) ptr) > arena_ + pagesize_*poolsize_)
        return NULL;
    uint32 off = ((const char*) ptr) - arena_;
    uint32 pageno = off / pagesize_;
    if (pages_ [pageno].free_) 
        return NULL;
    if (count)
        *count = pages_ [pageno].masters_;
    return arena_ + pageno*pagesize_;
}

void* SimplePager::checkpage (File& file, uint64 pageno, uint32* count)
{
    PageKey startKey (file, pageno);
    Pkeymap::iterator itr = addrmap_.find (startKey);
    if (itr == addrmap_.end ())
        return NULL;
    if (count)
        *count = pages_ [(*itr).second].masters_;
    return arena_ + (*itr).second*pagesize_;
}

uint64 SimplePager::getDumpCount () const
{
    return dumpcnt_;
}

uint64 SimplePager::getHitsCount () const
{
    return hits_;
}

uint64 SimplePager::getMissesCount () const
{
    return misses_;
}

uint32 SimplePager::getPageSize () const
{
    return pagesize_;
}

uint32 SimplePager::getPoolSize () const
{
    return poolsize_;
}

bool SimplePager::setPoolSize (uint32 poolsize)
{
#ifdef DEBUG_
    if (poolsize == 0) ERR("Zero pool size requested");
#endif
    if (poolsize != poolsize_)
    {
        prep_detach_ ();
        init_ (pagesize_, poolsize);
    }
    return true;
}


Pager& SimplePagerFactory::create (uint32 pagesize, uint32 poolsize)
{
    return *new SimplePager (pagesize, poolsize);
}


};
