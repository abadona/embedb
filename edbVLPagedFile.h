#ifndef edbVLPagedFile_h
#define edbVLPagedFile_h

#include "edbTypes.h"
#include "edbVLPagedCache.h"

namespace edb
{

class VLPagedFile
{
public:
    virtual            ~VLPagedFile        () {}
    // makes sure the requested portion of the file is loaded and returns pointer to it
    virtual void*      fetch             (FilePos pos, BufLen count, bool locked = false) = 0;
    // creates the buffer which will be used to overwrite the requested portion of the file
    virtual void*      fake              (FilePos pos, BufLen count, bool locked = false) = 0;
    virtual bool       locked            (const void* page) = 0; // checks whether the page is locked 
    virtual void       lock              (const void* page) = 0; // locks the page in memory
    virtual void       unlock            (const void* page) = 0; // unlocks the page in memory
    virtual bool       marked            (const void* page) = 0; // checks whether page is dirty
    virtual void       mark              (const void* page) = 0; // marks page as dirty
    virtual void       unmark            (const void* page) = 0; // marks page as clean
    virtual bool       flush             () = 0;
    virtual FilePos    length            () = 0;
    virtual bool       chsize            (FilePos newSize) = 0;
    virtual bool       close             () = 0;
};

class VLPagedFileFactory
{
public:
    virtual            ~VLPagedFileFactory () {}
    virtual VLPagedFile& open              (const char* directory, const char* basename, VLPagedCache& cache) = 0;
    virtual VLPagedFile& create            (const char* directory, const char* basename, VLPagedCache& cache) = 0;
    virtual bool       exists            (const char* directory, const char* basename) = 0;
    virtual bool       erase             (const char* directory, const char* basename) = 0;	
    virtual bool       rename            (const char* src_directory, const char* src_basename, const char* dst_directory, const char* dst_basename) = 0;
};

};

#endif
