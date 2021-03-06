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

#ifndef edbIndex_h
#define edbIndex_h
#include "edbTypes.h"


namespace edb
{


class IndexItr 
{
public:
    virtual Value       operator *      () const = 0;
    virtual IndexItr&   operator ++     () = 0;
    virtual IndexItr&   operator ++     (int) = 0;
    virtual bool        operator ==     (const IndexItr&) const = 0;
    virtual bool        operator !=     (const IndexItr&) const = 0;
    virtual IndexItr&   end             () const = 0;
};

class Index
{
public:
    virtual             ~Index          () {}
    virtual ValueNum    count           (Key key, KeyLen keylen) = 0;
    virtual IndexItr&   getPartial      (Key key, KeyLen keylen) = 0;
    virtual IndexItr&   getExact        (Key key, KeyLen keylen) = 0;
    virtual bool        set             (Key key, KeyLen keylen, Value val) = 0;
    virtual bool        erase           (Key key, KeyLen keylen, Value val) = 0;
};

class IndexFactory
{
public:
    virtual             ~IndexFactory  () {}
    virtual Index&      open           (const char* directory, const char* basename) = 0;
    virtual Index&      create         (const char* directory, const char* basename) = 0;
    virtual bool        exists         (const char* directory, const char* basename) = 0;
    virtual bool        erase          (const char* directory, const char* basename) = 0;	
    virtual bool        rename         (const char* src_directory, const char* src_basename, const char* dst_directory, const char* dst_basename) = 0;

    virtual const char* methodName     () const = 0;
};

};

#endif