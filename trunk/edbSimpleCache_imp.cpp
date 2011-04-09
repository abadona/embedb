// Copyright 2001 Victor Joukov, Denis Kaznadzey
#define simpleCacheFactory_defined

#include "edbSimpleCache_imp.h"
#include "edbThePagerMgr.h"

namespace edb
{
static SimpleCacheFactory_imp theFactory;
FileCacheFactory& simpleCacheFactory = theFactory;

SimpleCache_imp::SimpleCache_imp (Pager& pager)
:
pager_ (&pager)
{
}

BufLen SimpleCache_imp::read (File& file, FilePos offset, void* buffer, BufLen size, uint16 priority)
{
    uint32 pgsize = pager_->getPageSize ();
    uint64 first_page = offset / pgsize;
    uint32 first_start = offset % pgsize;
    uint64 last_page  = (offset + size) / pgsize;
    uint32 last_end = (offset + size) % pgsize;
    if (last_end == 0)
    {
        last_end = pgsize;
        last_page --;
    }

    for (uint64 pageno = first_page; pageno <= last_page; pageno ++)
    {
        uint32 start = (pageno == first_page)?first_start:0;
        uint32 end   = (pageno == last_page)?last_end:pgsize;
        char* b = (char*) pager_->fetch (file, pageno);
        memcpy (buffer, b + start, end - start);
        buffer = ((char*)buffer) + end - start;
    }
    return size;
}

BufLen SimpleCache_imp::write (File& file, FilePos offset, const void* buffer, BufLen size, uint16 priority)
{
    uint32 pgsize = pager_->getPageSize ();
    uint64 first_page = offset / pgsize;
    uint32 first_start = offset % pgsize;
    uint64 last_page  = (offset + size) / pgsize;
    uint32 last_end = (offset + size) % pgsize;
    if (last_end == 0)
    {
        last_end = pgsize;
        last_page --;
    }

    for (uint64 pageno = first_page; pageno <= last_page; pageno ++)
    {
        uint32 start = (pageno == first_page)?first_start:0;
        uint32 end   = (pageno == last_page)?last_end:pgsize;

        char* b = (pageno == first_page || pageno == last_page)? ((char*) pager_->fetch (file, pageno)) : ((char*) pager_->fake (file, pageno));
        memcpy (b + start, buffer, end - start);
        buffer = ((char*)buffer) + end - start;
        pager_->mark (b);
    }
    return size;
}

bool SimpleCache_imp::flush (File& file)
{
    return pager_->commit (file);
}

bool SimpleCache_imp::close (File& file)
{
    return pager_->close (file);
}

bool SimpleCache_imp::hintAdd (File& file, FileCacheHint* hints, int hintno)
{
    return true;
}

bool SimpleCache_imp::hintReset (File& file)
{
    return true;
}

uint32 SimpleCache_imp::getSize () const
{
    return pager_->getPoolSize () * pager_->getPageSize ();
}

bool SimpleCache_imp::setSize (uint32 size)
{
    uint32 newPoolSize = size / pager_->getPageSize ();
    if (!newPoolSize) return false;
    pager_->setPoolSize (newPoolSize);
    return true;
}

uint32 SimpleCache_imp::getPageSize () const
{
    return pager_->getPageSize ();
}

bool SimpleCache_imp::setPageSize (uint32 newPageSize)
{
    if (!newPageSize) ERR("Zero page size requested");
    uint32 oldPageSize = pager_->getPageSize ();
    Pager& pager = thePagerMgr().getPager (newPageSize);
    thePagerMgr().releasePager (oldPageSize);
    pager_ = &pager;
    return true;
}

FileCache& SimpleCacheFactory_imp::create (uint32 pagesize)
{
    Pager& pager = thePagerMgr ().getPager (pagesize);
    return *new SimpleCache_imp (pager);
}

};