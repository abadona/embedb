// Copyright 2001 Victor Joukov, Denis Kaznadzey
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