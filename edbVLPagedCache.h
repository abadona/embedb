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

#ifndef edbVLPagedCache_h
#define edbVLPagedCache_h

#include "edbFile.h"
#include "edbPager.h"
#include "edbError.h"

namespace edb
{

class VLPagedCache
{
public:
	virtual			    ~VLPagedCache		() {}
    virtual void*       fetch			(File& file, FilePos off, BufLen len, bool lock = false) = 0; // returns a pointer to the data contained in requested page
    virtual void*       fake			(File& file, FilePos off, BufLen len, bool lock = false) = 0; // returns a pointer to the fake buffer (used for complete page overwrite)
    virtual bool        locked			(const void* buffer) = 0; // checks weather the page is locked 
    virtual void        lock			(const void* buffer) = 0; // locks the page in memory
    virtual void        unlock			(const void* buffer) = 0; // unlocks the page in memory
    virtual bool        marked			(const void* buffer) = 0; // checks weather page is dirty
    virtual void        mark			(const void* buffer) = 0; // marks page as dirty
    virtual void        unmark			(const void* buffer) = 0; // marks page as clean
    virtual bool        commit			(File& file) = 0; // flushes the buffers to disk
    virtual bool        chsize			(File& file, FilePos newSize) = 0; // changes size of a file and releases cache pages which are not longer valid
    virtual bool        close			(File& file) = 0;

    virtual int32       getPageSize		() const = 0;
    virtual int32       getPoolSize		() const = 0;

    virtual uint64      getDumpCount	() const = 0;
    virtual uint64      getHitsCount	() const = 0;
    virtual uint64      getMissesCount	() const = 0;
};

class VLPagedCacheFactory
{
public:
    virtual             ~VLPagedCacheFactory () {};
    virtual VLPagedCache& create			(Pager& pager, uint32 lcachesize) = 0;
};

};

#endif