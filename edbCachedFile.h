// Copyright 2001 Victor Joukov, Denis Kaznadzey
#ifndef edbCachedFile_h
#define edbCachedFile_h

#include "edbFile.h"
#include "edbFileCache.h"

namespace edb
{

class CachedFile : public File
{
public:
    virtual             ~CachedFile    () {}
    virtual bool        isOpen         () const = 0;
    virtual BufLen      read           (void* buf, BufLen byteno) = 0;
    virtual BufLen      write          (const void* buf, BufLen byteno) = 0;
    virtual FilePos     seek           (FilePos pos) = 0; 
    virtual FilePos	    tell           () = 0;
    virtual FilePos     length         () = 0;
    virtual bool        chsize         (FilePos newLength) = 0;
    virtual bool        commit         () = 0;
    virtual bool        close          () = 0;
    virtual bool        hintAdd        (FileCacheHint* hints, int hintno) = 0;
    virtual bool        hintReset      () = 0;
};

class CachedFileFactory
{
public:
    virtual             ~CachedFileFactory () {}
    virtual CachedFile& wrap           (File& file) = 0;
};

};

#endif

