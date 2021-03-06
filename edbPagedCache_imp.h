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

#ifndef edbPagedCache_imp_h
#define edbPagedCache_imp_h

#include "edbPagedCache.h"
#include "edbPager.h"

#include <list>
#include <map>

namespace edb
{

class PagedCache_imp : public PagedCache
{
private:
    struct BufKey
    {
        BufKey (File& f, FilePos off) : file_ (&f), off_ (off) {}
        bool operator < (const BufKey& bk) const 
        { 
            return (file_ == bk.file_) ? (off_ < bk.off_) : (off_ < bk.off_); 
        }
        bool operator == (const BufKey& bk) const 
        { 
            return ((file_ == bk.file_) && (off_ == bk.off_)); 
        }
        bool operator != (const BufKey& bk) const 
        {
            return !operator == (bk);
        }
        File*   file_;
        FilePos off_;
    };
    struct Buffer
    {
        Buffer () { init (NULL, 0, 0, NULL); }
        void init (File* file, FilePos off, BufLen size, void* data)
        { 
            file_ = file, off_ = off, size_ = size, data_ = data, lockcnt_ = 0, markcnt_ = 0; 
        }
        File* file_;
        FilePos off_;
        BufLen size_;
        int32 lockcnt_;
        int32 markcnt_;
        std::list<Buffer*>::iterator mrupos_;
        void* data_;
    };
    typedef std::map <BufKey, Buffer> Bkeymap;
    typedef std::map <const void*, Buffer*> Bptrmap;
    typedef std::list <Buffer*> Bptrlist;

    Pager&      pager_;
    Bkeymap     keymap_;
    Bptrmap     ptrmap_;
    Bptrlist    mrulist_;
    uint32      maxsize_;
    uint32      cursize_;


    Buffer&     fetch_          (File& file, FilePos off, BufLen len);
    Buffer&     fake_           (File& file, FilePos off, BufLen len);
    void        dump_           (Buffer& buf);
    void        free_           (Buffer& buf);

    void        resolve_overlaps_(const Bkeymap::iterator& itr, const BufKey& key, BufLen len);
    void        process_overlaps_(File& file, FilePos off, BufLen len);
    void        check_paged_overlaps_ (File& file, FilePos off, BufLen len);
    void        ensurespace_    (BufLen len);
    Buffer&     add_            (BufKey& key, BufLen len);
    void        fill_           (Buffer& buf);



    void        lock_           (Buffer& buf);
    void        unlock_         (Buffer& buf);
    void        locked_         (Buffer& buf);
    void        mark_           (Buffer& buf);
    void        unmark_         (Buffer& buf);
    void        marked_         (Buffer& buf);


protected:
                PagedCache_imp  (Pager& pager, uint32 lcachesize);
public:
			    ~PagedCache_imp ();
    void*       fetch			(File& file, FilePos off, BufLen len, bool lock = false);
    void*       fake			(File& file, FilePos off, BufLen len, bool lock = false);
    bool        locked			(const void* buffer);
    void        lock			(const void* buffer);
    void        unlock			(const void* buffer);
    bool        marked			(const void* buffer);
    void        mark			(const void* buffer);
    void        unmark			(const void* buffer);
    bool        commit			(File& file);
    bool        chsize			(File& file, FilePos newSize);
    bool        close			(File& file);

    int32       getPageSize		() const;
    int32       getPoolSize		() const;

    uint64      getDumpCount	() const;
    uint64      getHitsCount	() const;
    uint64      getMissesCount	() const;

friend class PagedCacheFactory_imp;
};

class PagedCacheFactory_imp : public PagedCacheFactory
{
public:
                ~PagedCacheFactory_imp ();
    PagedCache& create (Pager& pager, uint32 lcachesize);
};
	
};

#endif