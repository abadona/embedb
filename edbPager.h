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

#ifndef edbPager_h
#define edbPager_h

#include "edbFile.h"

namespace edb
{

class Pager
{
public:
	virtual			    ~Pager		() {}
    virtual void*       fetch       (File& file, uint64 pageno, bool lock = false, uint32 count = 1) = 0; // returns a pointer to the data contained in requested page(s)
    virtual void*       fake        (File& file, uint64 pageno, bool lock = false, uint32 count = 1) = 0; // returns a pointer to the fake page(s) (used for complete page(s) overwrite)
    virtual bool        locked      (const void* page) = 0; // checks whether the page(s) is(are) locked. 
    virtual void        lock        (const void* page) = 0; // locks the page(s) in memory. 
    virtual void        unlock      (const void* page) = 0; // unlocks the page(s) in memory
    virtual bool        marked      (const void* page) = 0; // checks whether the page(s) is(are) dirty
    virtual void        mark        (const void* page) = 0; // marks page(s) as dirty
    virtual void        unmark      (const void* page) = 0; // marks page(s) as clean
    virtual bool        commit      (File& file) = 0; // flushes the buffers to disk
    virtual bool        chsize      (File& file, FilePos newSize) = 0; // changes size of a file and releases cache pages which are no longer valid
    virtual bool        close       (File& file) = 0; // flushes data to file and closes it
    virtual bool        detach      (File& file) = 0; // flushes data to file and forgets the buffers; if any of them locked, throws exception

    virtual uint64      pageno      (const void* page) = 0; // finds the page number cached in page buffer
    virtual File&       file        (const void* page) = 0; // finds the file which portion is cached in page buffer
    virtual void*       pageaddr    (const void* ptr, uint32* count = NULL) = 0; // returns the proper base page address for pointer or NULL if not managed or invalid

    virtual void*       checkpage   (File& file, uint64 pageno, uint32* count = NULL) = 0; // determins weather the page is currently cached; returns address of cached page or NULL if not cached

    virtual uint32      getPageSize () const = 0;
    virtual uint32      getPoolSize () const = 0;
    virtual bool        setPoolSize (uint32 poolsize) = 0;

    virtual uint64      getDumpCount() const = 0;
    virtual uint64      getHitsCount() const = 0;
    virtual uint64      getMissesCount() const = 0;
};

class PagerFactory
{
public:
    virtual             ~PagerFactory () {};
    virtual Pager&      create      (uint32 pagesize, uint32 poolsize) = 0;
};

};


#endif