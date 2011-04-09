#ifndef edbBufferedFile_h
#define edbBufferedFile_h

#include "edbTypes.h"

namespace edb
{

class BufferedFile
{
public:
    virtual             ~BufferedFile    () {}
    virtual const void* get              (FilePos pos, BufLen count) = 0;
    virtual BufLen      write            (void* buf, FilePos pos, BufLen count) = 0;
    virtual bool        flush            () = 0;
    virtual FilePos     length           () = 0;
    virtual bool        chsize           (FilePos newSize) = 0;
    virtual bool        close            () = 0;
};

class BufferedFileFactory
{
public:
    virtual               ~BufferedFileFactory   () {}
    virtual BufferedFile& open           (const char* directory, const char* basename) = 0;
    virtual BufferedFile& create         (const char* directory, const char* basename) = 0;
    virtual bool          exists         (const char* directory, const char* basename) = 0;
    virtual bool          erase          (const char* directory, const char* basename) = 0;	
    virtual bool          rename         (const char* src_directory, const char* src_basename, const char* dst_directory, const char* dst_basename) = 0;
};

};

#endif
