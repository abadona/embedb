// Copyright 2001 Victor Joukov, Denis Kaznadzey
#ifndef edbDummyCache_imp_h
#define edbDummyCache_imp_h

#include "edbFileCache.h"

namespace edb
{

class DummyCache_imp : public FileCache
{
public:
    BufLen      read           (File& file, FilePos offset, void* buffer, BufLen size, uint16 priority);
    BufLen      write          (File& file, FilePos offset, const void* buffer, BufLen size, uint16 priority);
    bool        flush          (File& file);
    bool        close          (File& file);
    bool        hintAdd        (File& file, FileCacheHint* hints, int hintno);
    bool        hintReset      (File& file);

    uint32      getSize        () const;
    bool        setSize        (uint32 size);
    uint32      getPageSize    () const { return 0; }
    bool        setPageSize    (uint32 size) { return false; }
};

class DummyCacheFactory_imp : public FileCacheFactory
{
public:
    FileCache&  create         (uint32 pagesize = 0);
};

};

#endif