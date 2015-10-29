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

#ifndef edbRecStorage_h
#define edbRecStorage_h

#include "edbTypes.h"
#include "edbFile.h"

namespace edb
{

struct AttrDef
{
	AttrDef (char* name, int size) : name_ (name), size_ (size) {}
	char* name_;
	uint32 size_;
};

struct IndexDef
{
	IndexDef (char* name, char** attr_names, void (*callback_) (RecNum recno, void* key, int len)) : name_ (name), attr_names_ (attr_names), callback_ (callback) {}
	char* name_;
	char** attr_names_;
	void (*callback_) (RecNum recno, void* key, int len);
};

struct RecDef
{
	int attr_count_;
	AttrDef* attributes_;
	int index_count_;
	IndexDef* indices_;

	int getAttrNumber (char* attrname) const;
	const char* getAttrName (int attrNumber) const;

	int getIndexNumber (char* attrname) const;
	const char* getIndexName (int attrNumber) const;
};


class RecStorage
{
public:
    virtual             ~RecStorage       () {}

    // record - level operations
    virtual bool        isValid        (RecNum recnum) = 0;
    virtual RecNum      allocRec       () = 0;
    virtual bool        freeRec        (RecNum recnum) = 0;
	virtual bool		locate		   (RecNum recnum) = 0;
	virtual RecNum		current		   () = 0;

	// attribute-level operations
    virtual void*		ptr			   (RecNum recnum, int fieldNum) = 0;
	virtual bool		changed		   (RecNum recnum) = 0;
    
    // storage - level operations
    virtual FRecLen     getRecLen      () const = 0;
	virtual const RecDef& getRecDef    () const = 0;
    virtual RecNum      getRecCount    () const = 0;
    virtual bool        flush          () = 0;
    virtual bool        close          () = 0;
    virtual bool        isOpen         () const = 0;
};

class RecStorageFactory
{
public:
    virtual             ~RecStorageFactory () {}
    virtual RecStorage&   wrap         (File& file) = 0;
    virtual RecStorage&   init         (File& file, RecDef& recdef) = 0;
    virtual bool          validate     (File& file) = 0;
};

};

#endif