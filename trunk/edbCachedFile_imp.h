// Copyright 2001 Victor Joukov, Denis Kaznadzey
#ifndef edbCachedFile_imp_h
#define edbCachedFile_imp_h

#include "edbCachedFile.h"
#include "edbFileCache.h"

namespace edb
{

class CachedFile_imp : public CachedFile
{
private:
    FileCache&  cache_;
    File&       file_;
    FilePos     curPos_;
protected:
                CachedFile_imp  (File& file, FileCache& cache);
public:
    bool        isOpen         () const;
    BufLen      read           (void* buf, BufLen byteno);
    BufLen      write          (const void* buf, BufLen byteno);
    FilePos     seek           (FilePos pos); 
    FilePos	    tell           ();            
    FilePos     length         ();            
    bool        chsize         (FilePos newLength);
    bool        commit         ();
    bool        close          ();
    bool        hintAdd        (FileCacheHint* hints, int hintno);
    bool        hintReset      ();

    friend class CachedFileFactory_imp;
};

class CachedFileFactory_imp : public CachedFileFactory
{
public:
    CachedFile& wrap            (File& file);

};

};

#endif

