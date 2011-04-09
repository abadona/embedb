// Copyright 2001 Victor Joukov, Denis Kaznadzey
#ifndef edbFile_h
#define edbFile_h
#include "edbTypes.h"
#include "edbError.h"

namespace edb
{

class File 
{
public:
    virtual             ~File          () {}
    virtual bool        isOpen         () const = 0;
    virtual BufLen      read           (void* buf, BufLen byteno) = 0;
    virtual BufLen      write          (const void* buf, BufLen byteno) = 0;
    virtual FilePos     seek           (FilePos pos) = 0;
    virtual FilePos	    tell           () = 0;
    virtual FilePos     length         () = 0;
    virtual bool        chsize         (FilePos newLength) = 0;
    virtual bool        commit         () = 0;
    virtual bool        close          () = 0;
};


class FileFactory
{
public:
    virtual             ~FileFactory   () {}
    virtual File&       open           (const char* directory, const char* basename) = 0;
    virtual File&       create         (const char* directory, const char* basename) = 0;
    virtual bool        exists         (const char* directory, const char* basename) = 0;
    virtual bool        erase          (const char* directory, const char* basename) = 0;	
    virtual bool        rename         (const char* src_directory, const char* src_basename, const char* dst_directory, const char* dst_basename) = 0;
};

class IOError : public Error { public: IOError (const char* msg) : Error (msg) {}};


};

#endif
