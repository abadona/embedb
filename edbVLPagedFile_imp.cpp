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

#include "edbVLPagedFile_imp.h"
#include "edbSplitFileFactory.h"

namespace edb 
{
static VLPagedFileFactory_imp theFactory;
VLPagedFileFactory& pagedFileFactory = theFactory;


VLPagedFile_imp::VLPagedFile_imp (File& file, VLPagedCache& cache)
:
file_ (file),
cache_ (cache)
{
}

VLPagedFile_imp::~VLPagedFile_imp ()
{
    if (file_.isOpen ()) cache_.close (file_);
}

void* VLPagedFile_imp::fetch (FilePos pos, BufLen count, bool locked)
{
    return cache_.fetch (file_, pos, count, locked);
}

void* VLPagedFile_imp::fake (FilePos pos, BufLen count, bool locked)
{
    return cache_.fake (file_, pos, count, locked);
}

bool VLPagedFile_imp::locked (const void* page)
{
    return cache_.locked (page);
}

void VLPagedFile_imp::lock (const void* page)
{
    cache_.lock (page);
}

void VLPagedFile_imp::unlock (const void* page)
{
    cache_.unlock (page);
}

bool VLPagedFile_imp::marked (const void* page)
{
    return cache_.marked (page);
}

void VLPagedFile_imp::mark (const void* page)
{
    cache_.mark (page);
}

void VLPagedFile_imp::unmark (const void* page)
{
    cache_.unmark (page);
}

bool VLPagedFile_imp::flush ()
{
    return cache_.commit (file_);
}

FilePos VLPagedFile_imp::length ()
{
    return file_.length ();
}

bool VLPagedFile_imp::chsize (FilePos newSize)
{
    return cache_.chsize (file_, newSize);
}

bool VLPagedFile_imp::close ()
{
    return cache_.close (file_);
}

VLPagedFile& VLPagedFileFactory_imp::open (const char* directory, const char* basename, VLPagedCache& cache) 
{
    File& f = splitFileFactory.open (directory, basename);
    return *new VLPagedFile_imp (f, cache);
}

VLPagedFile& VLPagedFileFactory_imp::create (const char* directory, const char* basename, VLPagedCache& cache) 
{
    File& f = splitFileFactory.create (directory, basename);
    return *new VLPagedFile_imp (f, cache);
}

bool VLPagedFileFactory_imp::exists (const char* directory, const char* basename) 
{
    return splitFileFactory.exists (directory, basename);
}

bool VLPagedFileFactory_imp::erase (const char* directory, const char* basename)	 
{
    return splitFileFactory.erase (directory, basename);
}

bool VLPagedFileFactory_imp::rename (const char* src_directory, const char* src_basename, const char* dst_directory, const char* dst_basename) 
{
    return splitFileFactory.rename (src_directory, src_basename, dst_directory, dst_basename);
}


}
