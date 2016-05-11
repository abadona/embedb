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

#define splitFileFactory_defined
#include "portability.h"
#include "edbSplitFile_imp.h"
#include "edbExceptions.h"
#include <errno.h>
#include <iterator>

namespace edb
{

static SplitFileFactory_imp theFileFactory;
FileFactory& splitFileFactory = theFileFactory;

static const unsigned MAXBUF = 4096;

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
    char name [MAXBUF];
    name4number (f.base_name_.c_str (), 0, name, MAXBUF);

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
    char name [MAXBUF];
    while (1)
    {
        name4number (f.base_name_.c_str (), fno, name, MAXBUF);
        if (::sci_unlink (name) != 0)
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