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

#define pagedFileFactory_defined
#include "edbPagedFile_imp.h"
#include "edbThePagerMgr.h"

namespace edb 
{

static PagedFileFactory_imp theFactory;
PagedFileFactory& pagedFileFactory = theFactory;

PagedFile_imp::PagedFile_imp (File& file, Pager& pager)
:
file_ (file),
pager_ (&pager)
{
    flen_ = file.length ();
}

PagedFile_imp::~PagedFile_imp ()
{
    if (file_.isOpen ()) pager_->close (file_);
}

void PagedFile_imp::checkLen_ (FilePos pageno, uint32 count)
{
    FilePos lpos = (pageno + count)*pager_->getPageSize ();
    if (flen_ < lpos) flen_ = lpos;
}

void* PagedFile_imp::fetch (FilePos pageno, uint32 count, bool locked)
{
    void* buffer = pager_->fetch (file_, pageno, locked, count);
    checkLen_ (pageno, count);
    return buffer;
}

void* PagedFile_imp::fake (FilePos pageno, uint32 count, bool locked)
{
    void* buffer = pager_->fake (file_, pageno, locked, count);
    checkLen_ (pageno, count);
    return buffer;
}

bool PagedFile_imp::locked (const void* page)
{
    return pager_->locked (page);
}

void PagedFile_imp::lock (const void* page)
{
    pager_->lock (page);
}

void PagedFile_imp::unlock (const void* page)
{
    pager_->unlock (page);
}

bool PagedFile_imp::marked (const void* page)
{
    return pager_->marked (page);
}

void PagedFile_imp::mark (const void* page)
{
    pager_->mark (page);
}

void PagedFile_imp::unmark (const void* page)
{
    pager_->unmark (page);
}

bool PagedFile_imp::flush ()
{
    return pager_->commit (file_);
}

FilePos PagedFile_imp::length ()
{
    return flen_;
}

bool PagedFile_imp::chsize (FilePos newSize)
{
    bool result = pager_->chsize (file_, newSize);
    if (result) flen_ = newSize;
    return result;
}

bool PagedFile_imp::close ()
{
    return pager_->close (file_);
}

bool PagedFile_imp::isOpen () const
{
    return file_.isOpen ();
}

uint32 PagedFile_imp::getPageSize       ()
{
    return pager_->getPageSize ();
}
void PagedFile_imp::setPageSize       (uint32 newPageSize)
{
    if (!newPageSize) ERR("Zero page size requested");
    uint32 oldPageSize = pager_->getPageSize ();
    Pager& pager = thePagerMgr().getPager (newPageSize);
    thePagerMgr().releasePager (oldPageSize);
    pager_ = &pager;
}

PagedFile& PagedFileFactory_imp::wrap (File& file, uint32 pagesize) 
{
    Pager& pager = thePagerMgr ().getPager (pagesize);
    return *new PagedFile_imp (file, pager);
}


}
