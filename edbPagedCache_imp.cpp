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

#define pagedCacheFactory_defined
#include "edbPagedCache_imp.h"
#include <vector>

namespace edb
{
static PagedCacheFactory_imp theFactory;
PagedCacheFactory& pagedCacheFactory = theFactory;

///////////////////////
// PagedCache

PagedCache_imp::PagedCache_imp (Pager& pager, uint32 lcachesize) 
:
pager_ (pager),
maxsize_ (lcachesize),
cursize_ (0)
{
}

PagedCache_imp::~PagedCache_imp () 
{
}

void PagedCache_imp::ensurespace_ (BufLen len)
{
   Bptrlist::reverse_iterator lruitr = mrulist_.rbegin ();
    while (maxsize_ < cursize_ + len)
    {
        // if it is not locked
        if (lruitr == mrulist_.rend ()) ERR("Could not fit into cache size due to locks!");
        if (!(*lruitr)->lockcnt_)
        {
            // dump it
            dump_ (*(*lruitr));
            // free memory
            free_ (*(*lruitr));
        }
        // pick next lru item
        lruitr ++;
    }
}

PagedCache_imp::Buffer& PagedCache_imp::add_ (PagedCache_imp::BufKey& key, BufLen len)
{
    // check weather it is the time to free some space
    ensurespace_ (len);
    // allocate new Buffer
    Buffer& buf = keymap_ [key];
    // allocate memory 
    char* data = new char [len];
    if (!data) throw NoMemoryForCache ();
    cursize_ += len;
    buf.init (key.file_, key.off_, len, data);
    ptrmap_ [data] = &buf;
    mrulist_.push_front (&buf);
    buf.mrupos_ = mrulist_.begin ();
    return buf;
}

void PagedCache_imp::free_ (Buffer& buf)
{
    mrulist_.erase (buf.mrupos_);
    ptrmap_.erase (buf.data_);
    delete [] buf.data_;
    cursize_ -= buf.size_;
    keymap_.erase (BufKey (*buf.file_, buf.off_));
}

void PagedCache_imp::fill_ (Buffer& buf)
{
    uint32 pgsize = pager_.getPageSize ();
    uint64 pageno = buf.off_  / pgsize;
    uint64 pageno_end = (buf.off_ + buf.size_ - 1) / pgsize;
    uint32 accum = 0;
    for (uint64 p = pageno; p <= pageno_end; p++)
    {
        char* pagedata = (char*) pager_.fetch (*buf.file_, p);
        uint32 start = (p == pageno)?(buf.off_ % pgsize):(0);
        uint32 end   = (p == pageno_end)?((buf.off_ + buf.size_) % pgsize):(pgsize);
        if (!end) end = pgsize;
        memcpy (((char*) buf.data_) + accum, pagedata + start, end - start);
        accum += end - start;
    }
}

void PagedCache_imp::dump_ (Buffer& buf)
{
    if (!buf.markcnt_) return;
    uint32 pgsize = pager_.getPageSize ();
    uint64 pageno = buf.off_  / pgsize;
    uint64 pageno_end = (buf.off_ + buf.size_ - 1) / pgsize;
    uint32 accum = 0;
    for (uint64 p = pageno; p <= pageno_end; p++)
    {
        uint32 start = (p == pageno)?(buf.off_ % pgsize):(0);
        uint32 end   = (p == pageno_end)?((buf.off_ + buf.size_) % pgsize):(pgsize);
        if (!end) end = pgsize;
        char* pagedata = ((p == pageno && start != 0) || (p == pageno_end && end != pgsize)) ? (char*) pager_.fetch (*buf.file_, p) : (char*) pager_.fake (*buf.file_, p);
        memcpy (pagedata + start, ((char*) buf.data_) + accum, end - start);
        accum += end - start;
        pager_.mark (pagedata);
    }
    buf.markcnt_ = 0;
}

// Checks managed buffers for overlap with given key:len range.
// the buffers which overlap and not locked are dumped and freed. If there is 
//   a locked one, returns false - (this is an error in caller's logic)
void PagedCache_imp::resolve_overlaps_ (const Bkeymap::iterator& itr, const BufKey& key, BufLen len)
{
    // check forward neighbours (including current one). Perfect match is not overlap here!
    Bkeymap::iterator i1 = itr;
    std::vector<Buffer*> toErase;

    while (i1 != keymap_.end ())
    {
        Buffer& buf = (*i1).second;
        if ((key.off_ < buf.off_ + buf.size_) && (buf.off_ < key.off_ + len) && !(key.off_ == buf.off_ && len == buf.size_))
        {
            if (buf.lockcnt_) throw BufferLocked ();
            else { dump_ (buf); toErase.push_back (&buf); }
        }
        else
            break; // end of overlapped zone
        i1 ++;
    }
    // check prev neighbour
    i1 = itr;
    if (i1 != keymap_.begin ()) do
    {
        i1 --;
        Buffer& buf = (*i1).second;
        if ((key.off_ < buf.off_ + buf.size_) && (buf.off_ < key.off_ + len))
        {
            if (buf.lockcnt_)  throw BufferLocked ();
            else { dump_ (buf); toErase.push_back (&buf); }
        }
        else
            break; // end of overlapped zone
    }
    while (i1 != keymap_.begin ());

    for (std::vector<Buffer*>::iterator ei = toErase.begin (); ei != toErase.end (); ei ++)
        free_ (*(*ei));
}

void PagedCache_imp::process_overlaps_ (File& file, FilePos off, BufLen len)
{
    BufKey key (file, off);
    Bkeymap::iterator itr = keymap_.lower_bound (key);
    resolve_overlaps_ (itr, key, len);
}

void PagedCache_imp::check_paged_overlaps_ (File& file, FilePos off, BufLen len)
{
    // check pages overlapping with (off, off+len) range
    // raise exception if some page is locked

    uint32 pgsize = pager_.getPageSize ();
    uint64 pageno_start = off / pgsize;
    uint64 pageno_end   = (off + len - 1) / pgsize;

    for (uint64 pg = pageno_start; pg < pageno_end; pg ++)
    {
        void* paddr = pager_.checkpage (file, pg);
        if (paddr)
            if (pager_.locked (paddr)) throw throw BufferLocked ();
    }
}

PagedCache_imp::Buffer& PagedCache_imp::fetch_ (File& file, FilePos off, BufLen len)
{
    check_paged_overlaps_ (file, off, len);
    // check weather in managed cache allready
    BufKey key (file, off);
    Bkeymap::iterator itr = keymap_.lower_bound (key);
    if ((*itr).first == key)  // length are guaranteed to be equal here (non-equal are treated as overlaps and allready removed from cache on the way here)
        return (*itr).second;
    else
    {
        // add buffer
        Buffer& buf = add_ (key, len);
        // fetch pages and copy them into allocated memory
        fill_ (buf);
        return buf;
    }
}

PagedCache_imp::Buffer& PagedCache_imp::fake_ (File& file, FilePos off, BufLen len)
{
    check_paged_overlaps_ (file, off, len);
    // check weather in managed cache allready
    BufKey key (file, off);
    Bkeymap::iterator itr = keymap_.lower_bound (key);
    if ((*itr).first == key) // length are guaranteed to be equal here (non-equal are treated as overlaps and allready removed from cache on the way here)
        return (*itr).second;
    else
        return  add_ (key, len);
}

void* PagedCache_imp::fetch (File& file, FilePos off, BufLen len, bool lock)
{
    process_overlaps_ (file, off, len);
    // determine if should be managed or external
    uint32 pgsize = getPageSize ();
    uint64 pageno = off / pgsize;
    uint64 pageno_end = (off + len - 1) / pgsize;
    if (pageno != pageno_end)
    {
        Buffer& buf = fetch_ (file, off, len);
        if (lock) buf.lockcnt_ ++;
        return buf.data_;
    }
    else
    {
        char* pagebuf =  (char*) pager_.fetch (file, pageno, lock);
        uint32 pageoff = off % pgsize;
        return pagebuf + pageoff;
    }
}

void* PagedCache_imp::fake (File& file, FilePos off, BufLen len, bool lock)
{
    process_overlaps_ (file, off, len);
    // determine if should be managed or external
    uint32 pgsize = getPageSize ();
    uint64 pageno = off / pgsize;
    uint64 pageno_end = (off + len - 1) / pgsize;
    if (pageno != pageno_end)
    {
        Buffer& buf = fake_ (file, off, len);
        if (lock) buf.lockcnt_ ++;
        return buf.data_;
    }
    else
    {
        char* pagebuf =  (char*) pager_.fake (file, pageno, lock);
        uint32 pageoff = off % pgsize;
        return pagebuf + pageoff;
    }
}

bool PagedCache_imp::locked	(const void* buffer)
{
    void* pageaddr = pager_.pageaddr (buffer);
    if (pageaddr) 
        return pager_.locked (pageaddr);
    Bptrmap::iterator itr = ptrmap_.find (buffer);
    if (itr == ptrmap_.end ()) throw InvalidBufptr ();
    return ((*itr).second->lockcnt_ != 0);
}

void PagedCache_imp::lock (const void* buffer)
{
    void* pageaddr = pager_.pageaddr (buffer);
    if (pageaddr) 
        pager_.lock (pageaddr);
    else
    {
        Bptrmap::iterator itr = ptrmap_.find (buffer);
        if (itr == ptrmap_.end ()) throw InvalidBufptr ();
        (*itr).second->lockcnt_ ++;
    }
}

void PagedCache_imp::unlock (const void* buffer)
{
    void* pageaddr = pager_.pageaddr (buffer);
    if (pageaddr) 
        pager_.unlock (pageaddr);
    else
    {
        Bptrmap::iterator itr = ptrmap_.find (buffer);
        if (itr == ptrmap_.end ()) throw InvalidBufptr ();
        if ((*itr).second->lockcnt_)
            (*itr).second->lockcnt_ --;
    }
}

bool PagedCache_imp::marked (const void* buffer)
{
    void* pageaddr = pager_.pageaddr (buffer);
    if (pageaddr) 
        return pager_.marked (pageaddr);
    Bptrmap::iterator itr = ptrmap_.find (buffer);
    if (itr == ptrmap_.end ()) throw InvalidBufptr ();
    return ((*itr).second->markcnt_ != 0);
}

void PagedCache_imp::mark (const void* buffer)
{
    void* pageaddr = pager_.pageaddr (buffer);
    if (pageaddr) 
        pager_.mark (pageaddr);
    else
    {
        Bptrmap::iterator itr = ptrmap_.find (buffer);
        if (itr == ptrmap_.end ()) throw InvalidBufptr ();
        (*itr).second->markcnt_ ++;
    }
}

void PagedCache_imp::unmark (const void* buffer)
{
    void* pageaddr = pager_.pageaddr (buffer);
    if (pageaddr) 
        pager_.unmark (pageaddr);
    else
    {
        Bptrmap::iterator itr = ptrmap_.find (buffer);
        if (itr == ptrmap_.end ()) throw InvalidBufptr ();
        if ((*itr).second->markcnt_)
            (*itr).second->markcnt_ --;
    }
}

bool PagedCache_imp::commit (File& file)
{
    // dump all dirty buffers that belong to file
    BufKey startKey (file, 0);
    BufKey endKey   (*((&file)+1), 0);

    Bkeymap::iterator start = keymap_.lower_bound (startKey);
    Bkeymap::iterator end   = keymap_.lower_bound (endKey);

    for (Bkeymap::iterator itr = start; itr != end; itr ++)
        dump_ ((*itr).second);
    
    return pager_.commit (file);
}

bool PagedCache_imp::chsize (File& file, FilePos newSize)
{
    // if shrinking - free all buffers that belong to file and lay above newSize; 
    // dump and free overlapping one
    if (newSize < file.length ())
    {
        BufKey startKey (file, newSize);
        BufKey endKey   (*((&file)+1), 0);

        Bkeymap::iterator start = keymap_.lower_bound (startKey);
        Bkeymap::iterator end   = keymap_.lower_bound (endKey);

        // check for the possible overlap
        Bkeymap::iterator prev = start;
        if (prev != keymap_.begin ())
        {
            prev --;
            if ((*prev).second.off_ + (*prev).second.size_ > newSize)
                dump_ ((*prev).second);
        }

        for (Bkeymap::iterator itr = start; itr != end; itr ++)
            free_ ((*itr).second);

        keymap_.erase (start, end);
    }

    return pager_.chsize (file, newSize);
}

bool PagedCache_imp::close (File& file)
{
    // dump and free all buffers that belongs to file
    BufKey startKey (file, 0);
    BufKey endKey   (*((&file)+1), 0);

    Bkeymap::iterator start = keymap_.lower_bound (startKey);
    Bkeymap::iterator end   = keymap_.lower_bound (endKey);

    for (Bkeymap::iterator itr = start; itr != end; itr ++)
    {
        dump_ ((*itr).second);
        free_ ((*itr).second);
    }

    keymap_.erase (start, end);
    
    return pager_.close (file);
}

int32 PagedCache_imp::getPageSize () const
{
    return pager_.getPageSize ();
}

int32 PagedCache_imp::getPoolSize () const
{
    return pager_.getPoolSize ();
}

uint64 PagedCache_imp::getDumpCount	() const
{
    return pager_.getDumpCount	();
}

uint64 PagedCache_imp::getHitsCount () const
{
    return pager_.getHitsCount ();
}

uint64 PagedCache_imp::getMissesCount () const
{
    return pager_.getMissesCount ();
}

///////////////////////
// Factory

PagedCacheFactory_imp::~PagedCacheFactory_imp ()
{
}

PagedCache& PagedCacheFactory_imp::create (Pager& pager, uint32 lcachesize)
{
    return *new PagedCache_imp (pager, lcachesize);
}

}