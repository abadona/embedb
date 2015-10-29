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

#ifndef edbPagedFile_imp_h
#define edbPagedFile_imp_h

#include "edbPagedFile.h"
#include "edbPager.h"

namespace edb
{

class PagedFile_imp : public PagedFile
{
private:
    File&         file_;
    Pager*        pager_;
    FilePos       flen_;
    void          checkLen_ (FilePos pageno, uint32 count);
protected:
                  PagedFile_imp (File& file, Pager& pager);
public:
                  ~PagedFile_imp ();
    void*         fetch             (FilePos pageno, uint32 count = 1, bool locked = false);
    void*         fake              (FilePos pageno, uint32 count = 1, bool locked = false);
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
    bool          isOpen            () const;
    uint32        getPageSize       (); 
    void          setPageSize       (uint32 newPageSize);

friend class PagedFileFactory_imp;
};

class PagedFileFactory_imp : public PagedFileFactory
{
public:
    PagedFile&    wrap              (File& file, uint32 pagesize = 0);
};

};

#endif
