// Copyright 2001 Victor Joukov, Denis Kaznadzey
#ifndef edbTable_h
#define edbTable_h
#include "edbTypes.h"
#include "edbIndex.h"

namespace edb
{

#define NAMESIZE 32

struct FieldDef
{
    char     name [NAMESIZE];
    uint16   size;
};
class Table
{
public:
    virtual                 ~Table () {}
    // reflection
    virtual const FieldDef* getFieldDef () const = 0;
    virtual uint16          getFieldId  (const char* fieldname) = 0;
    virtual uint16          getFieldName(uint16 fieldId) = 0; 

    // state
    virtual uint64          getSize     () const = 0;

    // record location / access
    virtual bool            valid       (RecNum recno);
    virtual bool            access      (RecNum recno);
    virtual bool            commit      ();
    virtual bool            erase       ();  
    virtual bool            setField    (uint16 fldno, void* data, int len);
    virtual bool            getField    (uint16 fldno, void* data, int len);


};

class TableFactory
{
public:
    virtual Table&      create         (const char* directory, const char* basename, FieldDef* fields);
    virtual Table&      open           (const char* directory, const char* basename);
    virtual bool        exists         (const char* directory, const char* basename);
    virtual bool        erase          (const char* directory, const char* basename);
    virtual bool        rename         (const char* src_directory, const char* src_basename, const char* dst_directory, const char* dst_basename) = 0;
};

};

#endif