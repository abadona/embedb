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

#define dummyCacheFactory_defined
#include "edbDummyCache_imp.h"

namespace edb
{
static DummyCacheFactory_imp theCache;
FileCacheFactory& dummyCacheFactory = theCache;

BufLen DummyCache_imp::read (File& file, FilePos offset, void* buffer, BufLen size, uint16 priority)
{
    file.seek (offset);
    return file.read (buffer, size);
}

BufLen DummyCache_imp::write (File& file, FilePos offset, const void* buffer, BufLen size, uint16 priority)
{
    file.seek (offset);
    return file.write (buffer, size);
}

bool DummyCache_imp::flush (File& file)
{
    return file.commit ();
}

bool DummyCache_imp::close (File& file)
{
    return file.close ();
}

bool DummyCache_imp::hintAdd (File& file, FileCacheHint* hints, int hintno)
{
    return true;
}

bool DummyCache_imp::hintReset (File& file)
{
    return true;
}

uint32 DummyCache_imp::getSize () const
{
    return 0;
}

bool DummyCache_imp::setSize (uint32 size)
{
    return true;
}

FileCache& DummyCacheFactory_imp::create (uint32 pagesize)
{
    return *new DummyCache_imp;
}

};