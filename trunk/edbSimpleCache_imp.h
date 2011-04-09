// Copyright 2001 Victor Joukov, Denis Kaznadzey
#ifndef edbSimpleCache_imp_h
#define edbSimpleCache_imp_h

#include "edbFileCache.h"
#include "edbThePagerMgr.h"

namespace edb
{

class SimpleCache_imp : public FileCache
{
protected:
                Pager* pager_;

                SimpleCache_imp (Pager& pager);
public:
    BufLen      read           (File& file, FilePos offset, void* buffer, BufLen size, uint16 priority);
    BufLen      write          (File& file, FilePos offset, const void* buffer, BufLen size, uint16 priority);
    bool        flush          (File& file);
    bool        close          (File& file);
    bool        hintAdd        (File& file, FileCacheHint* hints, int hintno);
    bool        hintReset      (File& file);

    uint32      getSize        () const;
    bool        setSize        (uint32 size);
    uint32      getPageSize    () const;
    bool        setPageSize    (uint32 size);

friend class SimpleCacheFactory_imp;
};

class SimpleCacheFactory_imp : public FileCacheFactory
{
public:
    FileCache&  create          (uint32 pagesize = 0);
};

};

#endif