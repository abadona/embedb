// Copyright 2001 Victor Joukov, Denis Kaznadzey
#ifndef edbIdMap_h
#define edbIdMap_h
#include "edbTypes.h"



namespace edb
{

class ObjectId;

class IdMap 
{
public:
    virtual             ~IdMap () {}
    virtual bool        contains (ObjectId&);
    virtual RecNum      get (ObjectId&);
    virtual RecNum      set (ObjectId&, RecNum);
    virtual bool        erase (ObjectId&);

};

class IdMapFactory
{
public:
    virtual             ~IdMapFactory  () {}
	virtual IdMap&      open           (const char* directory, const char* basename) = 0;
	virtual IdMap&      create         (const char* directory, const char* basename) = 0;
	virtual bool        exists         (const char* directory, const char* basename) = 0;
	virtual bool        erase          (const char* directory, const char* basename) = 0;	
    virtual bool        rename         (const char* src_directory, const char* src_basename, const char* dst_directory, const char* dst_basename) = 0;

};

};

#endif