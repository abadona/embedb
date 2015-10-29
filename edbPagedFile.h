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

#ifndef edbPagedFile_h
#define edbPagedFile_h

#include "edbTypes.h"
#include "edbFile.h"

namespace edb
{

class PagedFile
{
public:
    virtual            ~PagedFile        () {}
    // makes sure the requested portion of the file is loaded and returns pointer to it
    virtual void*      fetch             (FilePos pageno, uint32 count = 1, bool locked = false) = 0;
    // creates the buffer that will be used to overwrite the requested portion of the file
    virtual void*      fake              (FilePos pageno, uint32 count = 1, bool locked = false) = 0;
    virtual bool       locked            (const void* page) = 0; // checks whether the page is locked 
    virtual void       lock              (const void* page) = 0; // locks the page in memory
    virtual void       unlock            (const void* page) = 0; // unlocks the page in memory
    virtual bool       marked            (const void* page) = 0; // checks whether page is dirty
    virtual void       mark              (const void* page) = 0; // marks page as dirty
    virtual void       unmark            (const void* page) = 0; // marks page as clean
    virtual bool       flush             () = 0; // flush buffers to file; makes sure the information is written to a device
    virtual FilePos    length            () = 0; // returns length of the file
    virtual bool       chsize            (FilePos newSize) = 0; // changes the size of the file
    virtual bool       close             () = 0; // closes the file
    virtual bool       isOpen            () const = 0;
    virtual uint32     getPageSize       () = 0;
    virtual void       setPageSize       (uint32 newPageSize) = 0;
};

class PagedFileFactory
{
public:
    virtual            ~PagedFileFactory () {}
    virtual PagedFile& wrap              (File& file, uint32 pagesize = 0) = 0; // pagesize == 0 forces use of system's default
};

};

#endif
