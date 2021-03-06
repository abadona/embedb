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

#ifndef edbFStorage_h
#define edbFStorage_h

#include "edbTypes.h"
#include "edbFile.h"

namespace edb
{

class FStorage
{
public:
    virtual             ~FStorage       () {}
    // record - level operations
    virtual bool        isValid        (RecNum recnum) = 0;
    virtual RecNum      allocRec       () = 0;
    virtual RecNum      addRec         (void* data) = 0;
    virtual bool        readRec        (RecNum recnum, void* data) = 0;
    virtual bool        writeRec       (RecNum recnum, void* data) = 0;
    virtual bool        freeRec        (RecNum recnum) = 0;

    // hinted prefetch 
    virtual void        hintAdd        (RecNum* recnum_buffer, uint32 number) = 0;
    virtual void        hintReset      () = 0;

    // storage - level operations
    virtual FRecLen     getRecLen      () const = 0;
    virtual RecNum      getRecCount    () const = 0;
    virtual RecNum      getFreeCount   () const = 0;
    virtual bool        flush          () = 0;
    virtual bool        close          () = 0;
    virtual bool        isOpen         () const = 0;
    virtual FRecLen     getheaderSize  () const = 0;
    virtual void        readHeader     (void* buffer, BufLen buflen) = 0;
    virtual void        writeHeader    (void* buffer, BufLen buflen) = 0;
};

class FStorageFactory
{
public:
    virtual             ~FStorageFactory () {}
    virtual FStorage&   wrap           (File& file) = 0;
    virtual FStorage&   init           (File& file, FRecLen reclen, FRecLen headersize = 0) = 0;
    virtual bool        validate       (File& file) = 0;
};

};

#endif