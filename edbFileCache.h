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

#ifndef edbFileCache_h
#define edbFileCache_h
#include "edbTypes.h"
#include "edbFile.h"

namespace edb
{

struct FileCacheHint 
{
    FilePos off;
    BufLen  len;
};

class FileCache
{

public:
    virtual             ~FileCache     () {}
    virtual BufLen      read           (File& file, FilePos offset, void* buffer, BufLen size, uint16 priority = 0) = 0;
    virtual BufLen      write          (File& file, FilePos offset, const void* buffer, BufLen size, uint16 priority = 0) = 0;
    virtual bool        flush          (File& file) = 0;
    virtual bool        close          (File& file) = 0;
    virtual bool        hintAdd        (File& file, FileCacheHint* hints, int hintno) = 0;
    virtual bool        hintReset      (File& file) = 0;

    virtual uint32      getSize        () const = 0;
    virtual bool        setSize        (uint32 size) = 0;
    virtual uint32      getPageSize    () const = 0;
    virtual bool        setPageSize    (uint32 size) = 0;
};

class FileCacheFactory
{
public:
    virtual             ~FileCacheFactory () {}
    virtual FileCache&  create         (uint32 pagesize = 0) = 0;
};

};

#endif