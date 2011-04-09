// Copyright 2001 Victor Joukov, Denis Kaznadzey
#define splitFileFactory_defined
#include "edbSplitFile_imp.h"
#include "edbExceptions.h"
#include <io.h>
#include <errno.h>
#include <iterator>

namespace edb
{

static SplitFileFactory_imp theFileFactory;
FileFactory& splitFileFactory = theFileFactory;

File& SplitFileFactory_imp::open (const char* directory, const char* basename)
{
    // create the File object
    SplitFile_imp& f = *new SplitFile_imp (directory, basename);

    // open
    if (!f.open ()) throw OpenError ();

    // the file is ready
    return f;
}
File& SplitFileFactory_imp::create (const char* directory, const char* basename)
{
    SplitFile_imp& f = *new SplitFile_imp (directory, basename);

    // create
    f.create ();

    // the file is ready
    return f;
}
bool SplitFileFactory_imp::exists (const char* directory, const char* basename)
{
    // create the File object

    SplitFile_imp f (directory, basename);

    // try to open
    const char* name = name4number (f.base_name_.c_str (), 0);
    
    Fid fid = fileHandleMgr.open (name);
    if (fid != -1)
    {
        fileHandleMgr.close (fid);
        return true;
    }
    else
        return false;
}
bool SplitFileFactory_imp::erase (const char* directory, const char* basename)	
{
    bool toR = false;
    
    // create the File object
    SplitFile_imp f (directory, basename);

    // try to open
    int fno = 0;
    while (1)
    {
        const char* name = name4number (f.base_name_.c_str (), fno);
        if (::unlink (name) != 0)
        {
            if (errno == ENOENT)
            {
                errno = 0;
                break;
            }
            else
            {
                errno = 0;
                toR = false;
            }
        }
        fno ++;
    }
    return toR;
}
bool SplitFileFactory_imp::rename (const char* src_directory, const char* src_basename, const char* dst_directory, const char* dst_basename)
{
    return false;
}

};