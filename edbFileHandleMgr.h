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

#ifndef edbFileHandleMgr_h
#define edbFileHandleMgr_h

#include "edbTypes.h"
#include "edbError.h"


namespace edb
{
// Exceptions

typedef int32 Fid;

class FileHandleMgr
{
public:
    virtual             ~FileHandleMgr () {}     

    virtual Fid         open           (const char* fname) = 0; // makes sure file is open and remembers in the managed set
    virtual Fid         create         (const char* fname) = 0; // creates the file and remembers in the managed set 
    virtual Fid         findFid        (const char* fname) = 0; // if the file open, returns fid; otherwise return -1
    virtual bool        isOpen         (const char* fname) = 0; // hecks weather the file is managed through handle cache. This does not relate to being PHYSICALLY open
    virtual bool        isValid        (Fid fid) = 0;           // checks weather the fileId is valid (open)
    virtual const char* findName       (Fid fid) = 0;           // finds the name for Fid if valid; otherwise, returns NULL
    virtual bool        close          (Fid fid) = 0;           // closes the file if open and removes entry from managed set
    virtual int         handle         (Fid fid) = 0;           // returns the file handle; file pointer is positioned at the place last operation leaved it
    virtual bool        commit         (Fid fid) = 0;           // makes sure  the file is actually flushed to disk (on currently closed just does nothing)

    virtual int         getPoolSize    () const = 0;            // handle pool size get and set
    virtual void        setPoolSize    (int newSize) = 0;       
    virtual uint64      getDropCount   () const = 0;            // how many handle displacements occured since the instantiation of handle manager
};

#ifndef FileHandleMgr_defined
    extern FileHandleMgr& fileHandleMgr;
#endif

// exceptions

};

#endif
