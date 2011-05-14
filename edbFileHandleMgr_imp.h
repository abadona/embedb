// Copyright 2001 Victor Joukov, Denis Kaznadzey
#ifndef edbFileHandleMgr_imp_h
#define edbFileHandleMgr_imp_h

#ifdef _MSC_VER
#pragma warning (disable : 4786)
#endif

#include <map>
#include <string>
#include <list>
#include <vector>
#include <string.h>
#include "edbFileHandleMgr.h"

namespace edb
{

typedef std::list <Fid> FidList;

struct FileInfo
{
    FileInfo () {init ("");}
    FileInfo (const char* name) {init (name);} 
    void init (const char* name) { name_ = name; handle_ = -1; position_ = 0; free_ = false; }
    std::string name_;
    int         handle_;
    long        position_;
    FidList::iterator mrupos_;
    bool        free_;
};
struct StringCompare
{   bool operator () (const char* s1, const char* s2) const
    {  return (strcmp (s1, s2) < 0); } };

typedef std::map    <const char*, Fid, StringCompare> NameFidMap;
typedef std::vector <FileInfo> FileInfoVect;

class FileHandleMgr_imp : public FileHandleMgr
{
    FileInfoVect files_;
    NameFidMap   name2fid_;
    FidList      mru_;
    int          poolSize_;
    // some statisticss
    uint64       drops_;

    Fid          add (const char* fname);
        
public:
                 FileHandleMgr_imp ();
    Fid          open           (const char* fname);
    Fid          create         (const char* fname);
    Fid          findFid        (const char* fname);
    bool         isOpen         (const char* fname);
    bool         isValid        (Fid fid);
    const char*  findName       (Fid fid);
    bool         close          (Fid fid);
    int          handle         (Fid fid);
    bool         commit         (Fid fid);

    int          getPoolSize    () const;
    void         setPoolSize    (int newSize);
    uint64       getDropCount   () const;
};


};

#endif