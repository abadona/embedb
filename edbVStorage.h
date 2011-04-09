// Copyright 2001 Victor Joukov, Denis Kaznadzey
#ifndef edbVStorage_h
#define edbVStorage_h

#include "edbTypes.h"
#include "edbFile.h"

namespace edb
{

struct VRecDescriptor
{
    RecLocator locator_;
    RecLen length_;
};

class VStorage
{
public:
    virtual             ~VStorage      () {}
    // record - level operations 
    virtual bool        isValid        (RecLocator locator) = 0;
    virtual RecLocator  allocRec       (RecLen len) = 0;
    virtual RecLen      getRecLen      (RecLocator locator) = 0;
    virtual RecLocator  addRec         (const void* data, RecLen len) = 0;
    virtual RecLocator  reallocRec     (RecLocator locator, RecLen len) = 0;
    virtual bool        readRec        (RecLocator locator, void* data, BufLen len, RecOff offset = 0) = 0;
    virtual bool        writeRec       (RecLocator locator, const void* data, BufLen len, RecOff offset = 0) = 0;
    virtual bool        freeRec        (RecLocator locator) = 0;
	virtual RecLocator  sliceRec       (RecLocator locator, const void* data, BufLen data_len, RecOff offset, RecLen old_data_len, RecLen full_len) = 0;

    // hinted prefetch 
    virtual void        hintAdd        (VRecDescriptor* descriptors, uint32 number) = 0;
    virtual void        hintReset      () = 0;

    // record enumeration
    virtual RecNum      getRecCount    () const = 0;
    virtual bool        firstRec	   (VRecDescriptor& rec) = 0;
    virtual bool        nextRec	       (VRecDescriptor& rec) = 0;

    // storage - level operations     
    virtual bool        flush          () = 0;
    virtual bool        close          () = 0;
    virtual bool        isOpen         () const = 0;

    // statistics
    virtual uint64      usedSpace      () const = 0;
    virtual uint64      freeSpace      () const = 0;
    virtual uint64      usedCount      () const = 0;
    virtual uint64      freeCount      () const = 0;
};

class VStorageFactory
{
public:
    virtual             ~VStorageFactory () {}
    virtual VStorage&   wrap           (File& file) = 0;
    virtual VStorage&   init           (File& file) = 0;
    virtual bool        validate       (File& file) = 0;
};

};

#endif
