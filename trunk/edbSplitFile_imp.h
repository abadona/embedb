// Copyright 2001 Victor Joukov, Denis Kaznadzey
#ifndef edbSplitFile_imp_h
#define edbSplitFile_imp_h
#include "edbFile.h"
#include "edbFileHandleMgr.h"

#include <string>
#include <vector>

namespace edb
{

typedef std::vector <Fid> FidVect;

class SplitFile_imp : public File
{
private:
    bool        open_;
    std::string base_name_;
    FidVect     fids_;

    FilePos     curPos_;
    Fid         curFile_;
    bool        cpsync_;
    FilePos     length_;


    bool        open    ();
    bool        create  ();

protected:
                SplitFile_imp  (const char* directory, const char* basename);
public:
                ~SplitFile_imp ();
    bool        isOpen         () const;
    BufLen      read           (void* buf, BufLen byteno);
    BufLen      write          (const void* buf, BufLen byteno);
    FilePos     seek           (FilePos pos);
    FilePos	    tell           ();
    FilePos     length         ();
    bool        chsize         (FilePos newLength);
    bool        commit         ();
    bool        close          ();

friend class SplitFileFactory_imp;
};

void name4number    (const char* base_name, int32 number, char* dest, unsigned destlen);

class SplitFileFactory_imp : public FileFactory
{
public:
    File&       open           (const char* directory, const char* basename);
    File&       create         (const char* directory, const char* basename);
    bool        exists         (const char* directory, const char* basename);
    bool        erase          (const char* directory, const char* basename);	
    bool        rename         (const char* src_directory, const char* src_basename, const char* dst_directory, const char* dst_basename);
};

};

#endif