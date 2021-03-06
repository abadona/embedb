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

#define cachedFileFactory_defined
#include "edbCachedFile_imp.h"
#include "edbSystemCache.h"

namespace edb
{
static CachedFileFactory_imp theFactory;
CachedFileFactory& cachedFileFactory = theFactory;

CachedFile_imp::CachedFile_imp  (File& file, FileCache& cache)
:
file_  (file),
cache_ (cache),
curPos_ (0L)
{
}

bool CachedFile_imp::isOpen () const
{
    return file_.isOpen ();
}

BufLen CachedFile_imp::read (void* buf, BufLen byteno)
{
    BufLen read = cache_.read (file_, curPos_, buf, byteno);
    curPos_ += read;
    return read;
}

BufLen CachedFile_imp::write (const void* buf, BufLen byteno)
{
    BufLen written = cache_.write (file_, curPos_, buf, byteno);
    curPos_ += written;
    return written;
}

FilePos CachedFile_imp::seek (FilePos pos)
{
    return curPos_ = pos;
}

FilePos	CachedFile_imp::tell ()
{
    return curPos_;
}

FilePos CachedFile_imp::length ()
{
    return file_.length ();
}

bool CachedFile_imp::chsize (FilePos newLength)
{
    return file_.chsize (newLength);
}

bool CachedFile_imp::commit ()
{
    return cache_.flush (file_);
}

bool CachedFile_imp::close ()
{
    return cache_.close (file_);
}

bool CachedFile_imp::hintAdd (FileCacheHint* hints, int hintno)
{
    return cache_.hintAdd (file_, hints, hintno);
}

bool CachedFile_imp::hintReset ()
{
    return cache_.hintReset (file_);
}

CachedFile& CachedFileFactory_imp::wrap (File& file)
{
    // get the cache
    FileCache& cache = systemCache ();
    // attach to the file
    return *new CachedFile_imp (file, cache);
}


};