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

#ifndef edbVLPagedFile_imp_h
#define edbVLPagedFile_imp_h

#include "edbVLPagedFile.h"
#include "edbFile.h"

namespace edb
{

class VLPagedFile_imp : public VLPagedFile
{
private:
    File&         file_;
    VLPagedCache&   cache_;
protected:
                  VLPagedFile_imp (File& file, VLPagedCache& cache);
public:
                  ~VLPagedFile_imp ();
    void*         fetch             (FilePos pos, BufLen count, bool locked = false);
    void*         fake              (FilePos pos, BufLen count, bool locked = false);
    bool          locked            (const void* page);
    void          lock              (const void* page);
    void          unlock            (const void* page);
    bool          marked            (const void* page);
    void          mark              (const void* page);
    void          unmark            (const void* page);
    bool          flush             ();
    FilePos       length            ();
    bool          chsize            (FilePos newSize);
    bool          close             ();

friend class VLPagedFileFactory_imp;
};

class VLPagedFileFactory_imp : public VLPagedFileFactory
{
public:
    VLPagedFile&    open           (const char* directory, const char* basename, VLPagedCache& cache);
    VLPagedFile&    create         (const char* directory, const char* basename, VLPagedCache& cache);
    bool          exists         (const char* directory, const char* basename);
    bool          erase          (const char* directory, const char* basename);
    bool          rename         (const char* src_directory, const char* src_basename, const char* dst_directory, const char* dst_basename);
};

};

#endif
