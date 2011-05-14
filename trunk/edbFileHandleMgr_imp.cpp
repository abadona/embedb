// Copyright 2001 Victor Joukov, Denis Kaznadzey
#define FileHandleMgr_defined

#include "edbFileHandleMgr_imp.h"
#include "edbExceptions.h"
#include "portability.h"
//#include <io.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef _MSC_VER
#include <share.h>
#endif


namespace edb
{

static FileHandleMgr_imp theFileHandleMgr;
FileHandleMgr& fileHandleMgr = theFileHandleMgr;

const int DEFAULT_HANDLE_POOL_SIZE = 200;

FileHandleMgr_imp::FileHandleMgr_imp ()
{
    poolSize_ = DEFAULT_HANDLE_POOL_SIZE;
    drops_ = 0L;
}

Fid FileHandleMgr_imp::add (const char* fname)
{
    // find empty place
    Fid fid;
    for (fid = 0; fid < files_.size (); fid ++)
    {
        FileInfo& fi = files_ [fid];
        if (fi.free_)
        {
            fi.init (fname);
            return fid;
        }
    }
    fid = files_.size ();
    files_.resize (fid + 1);
    files_ [fid].name_ = fname;
    return fid;
}

Fid FileHandleMgr_imp::open (const char* fname)
{
    // if allready open:
    if (findFid (fname) != -1) throw FileAllreadyOpen ();
    // test weather can open
    int h = ::sci_sopen (fname, _O_BINARY|_O_RDWR, _SH_DENYWR);
    if (h == -1) return -1;
    if (::sci_close (h) != 0) ERR("Unable to close file");
    // put into the array
    return add (fname);
}

Fid FileHandleMgr_imp::create (const char* fname)
{
    // if allready open:
    if (findFid (fname) != -1) throw FileAllreadyOpen ();
    // test weather can open
    int h = ::sci_screate (fname, _O_CREAT|_O_EXCL|_O_BINARY|O_RDWR, _SH_DENYWR, _S_IWRITE|_S_IREAD);
    if (h == -1) return -1;
    if (::sci_close (h) != 0) ERR("Unable to close file");
    // put into the array
    return add (fname);
}

Fid FileHandleMgr_imp::findFid (const char* fname)
{
    NameFidMap::iterator itr = name2fid_.find (fname);
    if (itr == name2fid_.end ())
        return -1;
    else
        return (*itr).second;
}

bool FileHandleMgr_imp::isOpen (const char* fname)
{
    return (name2fid_.find (fname) != name2fid_.end ());
}

bool FileHandleMgr_imp::isValid (Fid fid)
{
    if (fid >= files_.size ()) return false;
    if (files_ [fid].free_) return false;
    return true;
}

const char* FileHandleMgr_imp::findName (Fid fid)
{
    if (!isValid (fid)) return NULL;
    return files_ [fid].name_.c_str ();
}

bool FileHandleMgr_imp::close (Fid fid)
{
    if (!isValid (fid)) throw FileNotOpen ();
    FileInfo& fi = files_ [fid];
    if (fi.handle_ != -1)
    {
        if (::sci_close (fi.handle_) == -1) ERR("Unable to close file: invalid handle");
        fi.handle_ = -1;
        mru_.erase (fi.mrupos_);
    }
    name2fid_.erase (fi.name_.c_str ());
    fi.free_ = true;
    return true;
}

int FileHandleMgr_imp::handle (Fid fid)
{
    int ret = -1;
    if (!isValid (fid)) throw FileNotOpen ();
    FileInfo& fi = files_ [fid];
    if (fi.handle_ == -1)
    {
        if (mru_.size () == poolSize_)
        {
            // drop the least recently used file, remembering the state
            FileInfo& lru_file = files_ [mru_.back ()];
            mru_.pop_back ();
            lru_file.position_ = sci_tell (lru_file.handle_);
            ::sci_close (lru_file.handle_);
            lru_file.handle_ = -1;
            drops_ ++;
        }
        const char* fname = fi.name_.c_str ();
        ret = fi.handle_ = ::sci_sopen (fname, _O_BINARY|_O_RDWR, _SH_DENYWR);
        if (ret == -1)
        {
            ers << "Unable to open file "<< fname << ", OS error " << errno << " : " << strerror (errno);
            ERR ("");
        }
        if (fi.position_ != ::sci_lseek (ret, fi.position_, SEEK_SET))
        {
            ERR ("Seek error");
        }
        mru_.push_front (fid);
        fi.mrupos_ = mru_.begin ();
    }
    else
    {
        // move to the beginning of mru list
        mru_.erase (fi.mrupos_);
        mru_.push_front (fid);
        fi.mrupos_ = mru_.begin (); 
        ret = fi.handle_;
    }
    return ret;
}

bool FileHandleMgr_imp::commit (Fid fid)
{
    int ret = -1;
    if (!isValid (fid)) throw FileNotOpen ();
    FileInfo& fi = files_ [fid];
    if (fi.handle_ != -1)
       ::sci_commit (fi.handle_);
    // we do not treat the commit as an access, so we do not move file to the beginning of mru list
    return true;
}

int FileHandleMgr_imp::getPoolSize () const
{
    return poolSize_;
}

void FileHandleMgr_imp::setPoolSize (int newSize)
{
    if (newSize < 1) ERR("Internal")
    poolSize_ = newSize;
}

uint64 FileHandleMgr_imp::getDropCount () const
{
    return drops_;
}

};

