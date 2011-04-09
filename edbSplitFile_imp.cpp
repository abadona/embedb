// Copyright 2001 Victor Joukov, Denis Kaznadzey
#include "edbSplitFile_imp.h"
#include "edbExceptions.h"
//#include <strstream>
#include <io.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>

#ifdef _WIN32
#define PATH_SPLIT_SYM "\\"
#else
#define PATH_SPLIT_SYM "/"
#endif


namespace edb
{

// const int32 SPLIT_FACTOR = 0x10; // 16 bytes
// const int32 SPLIT_FACTOR = 0x40; // 64 bytes

const int32 SPLIT_FACTOR = 0x40000000; // 1Gb

inline int32 fileNo (FilePos offset)
{
    return offset / SPLIT_FACTOR;
}
inline int32 fileOff (FilePos offset)
{
    return offset % SPLIT_FACTOR;
}


const char* name4number (const char* base_name, int32 number)
{
    char ibuf [64];
    static std::string last_created_name_;
    last_created_name_ = base_name;
    last_created_name_ += ".";
    last_created_name_ += itoa (number, ibuf, 10);
    last_created_name_ += ".edb";
    return last_created_name_.c_str ();
}

bool SplitFile_imp::open ()
{
    int32 file_number = 0;
    length_ = 0;
    long len = 0;
    while (1)
    {
        const char* name = name4number (base_name_.c_str (), file_number);
        Fid fid = fileHandleMgr.open (name);
        if (fid != -1)
        {
            if (file_number > 0 && len != SPLIT_FACTOR) throw FileStructureCorrupt ();
            fids_.push_back (fid);
            int h = fileHandleMgr.handle (fid);
            len = ::_filelength (h);
            length_ += len;
        }
        else
            break;
        file_number ++;
    }
    open_ = (file_number > 0);
    return open_;
}

bool SplitFile_imp::create ()
{
    // make sure there is no file with the name
    const char* name = name4number (base_name_.c_str (), 0);
    Fid fid = fileHandleMgr.open (name);
    if (fid != -1)
    {
        fileHandleMgr.close (fid);
        throw FileAllreadyExists ();
    }

    // create the very first, empty file
    fid = fileHandleMgr.create (name);
    if (fid == -1) throw CreateError ();
    length_ = 0;
    fids_.push_back (fid);
    open_ = true;
    return true;
}

SplitFile_imp::SplitFile_imp (const char* directory, const char* basename)
:
open_ (false),
curPos_ (0L),
curFile_ (0),
cpsync_ (true)
{
    base_name_ = "";
    base_name_ += directory;
    base_name_ += PATH_SPLIT_SYM;
    base_name_ += basename;
}

SplitFile_imp::~SplitFile_imp ()
{
    if (open_)
    {
        close ();
        open_ = false;
    }
}

bool SplitFile_imp::isOpen () const
{
    return open_;
}

BufLen SplitFile_imp::read (void* buf, BufLen byteno)
{
    if (!open_) throw FileNotOpen ();

    // read only up to the file end
    int32 first_file = fileNo (curPos_);
    int32 first_off  = fileOff (curPos_);
    int32 last_file, to_off;
    if (curPos_ + byteno < length_)
    {
        last_file  = fileNo (curPos_ + byteno);
        to_off     = fileOff (curPos_ + byteno);
    }
    else
    {
        last_file  = fileNo (length_);
        to_off     = fileOff (length_);
    }
    if (to_off == 0)
    {
        last_file --;
        to_off = SPLIT_FACTOR;
    }

    BufLen curpos = 0;
    for (int32 fno = first_file; fno <= last_file; fno ++)
    {
        int32 start = (fno == first_file)?first_off:0;
        int32 end   = (fno == last_file)?to_off:SPLIT_FACTOR;
        int h = fileHandleMgr.handle (fids_ [fno]);
        long rdpos = -1;
        if (fno != first_file || !cpsync_)
        {
            rdpos  = ::_lseek (h, start, SEEK_SET);
            if (rdpos != start) ERR ("Seek(for read) error")
        }
        // if request is to read beyond the eof, do not (read zero bytes)
        if (end > start)
        {
            int32 len   = end - start;
            int rdlen = ::_read (h, ((char*) buf) + curpos, len);
            if (rdlen != len) ERR("Read error");
            curpos += len;
        }
    }
    curPos_ += byteno;
    seek (curPos_); // to adjust curFile_ and cpsync_ if necessary
    return curpos;
}

BufLen SplitFile_imp::write (const void* buf, BufLen byteno)
{
    if (!open_) throw FileNotOpen ();

    int32 last_file_before = fids_.size () - 1;
    int32 first_file = fileNo (curPos_);
    int32 first_off  = fileOff (curPos_);
    int32 last_file  = fileNo (curPos_ + byteno);
    int32 to_off     = fileOff (curPos_ + byteno);
    if (to_off == 0)
    {
        last_file --;
        to_off = SPLIT_FACTOR;
    }

    BufLen curpos = 0;

    for (int32 fno = first_file; fno <= last_file; fno ++)
    {
        int32 start = (fno == first_file)?first_off:0;
        int32 end   = (fno == last_file)?to_off:SPLIT_FACTOR;
        int32 len   = end - start;
        for (int newFno = fids_.size () - 1; newFno <= fno; newFno ++)
        {
            Fid fid;
            if (newFno >= fids_.size ())
            {
                const char* new_name = name4number (base_name_.c_str (), newFno);
                fid = fileHandleMgr.create (new_name);
                if (fid == -1) throw CreateError ();
                fids_.push_back (fid);
            }
            else
                fid = fids_ [newFno];
            if (newFno < fno)
            {
                if (::_chsize (fileHandleMgr.handle (fid), SPLIT_FACTOR) != 0)
                {
                    if (errno == ENOSPC) throw NoDeviceSpace ();
                    else ERR("Unable to enlarge file");
                }
            }
        }
        int h = fileHandleMgr.handle (fids_ [fno]);
        if (fno != last_file_before || !cpsync_)
        {
            long wrpos  = ::_lseek (h, start, SEEK_SET);
            if (wrpos != start) ERR ("Seek(for write) error")
        }
        int wrlen = ::_write (h, ((char*) buf) + curpos, len);
        if (wrlen != len) ERR("Write error");
        curpos += len;
    }
    if (curPos_ + byteno > length_)
        length_ = curPos_ + byteno;
    curPos_ += byteno;
    seek (curPos_); // to adjust curFile_ and cpsync_ if necessary
    return curpos;
}

FilePos SplitFile_imp::seek (FilePos pos)
{
    if (!open_) throw FileNotOpen ();
    Fid fid = fileNo (pos);
    if (curPos_ != pos || curFile_ != fid)
    {
        curPos_ = pos;
        curFile_ = fid;
        cpsync_ = false;
    }
    return curPos_;
}

FilePos	SplitFile_imp::tell ()
{
    if (!open_) throw FileNotOpen ();
    return curPos_;
}

FilePos SplitFile_imp::length ()
{
    if (!open_) throw FileNotOpen ();
    return length_;
}

bool SplitFile_imp::chsize (FilePos newLength)
{
    if (!open_) throw FileNotOpen ();

    int32 cur_no_of_files = fids_.size ();
    int32 new_last_file   = fileNo (newLength);
    int32 last_file_size  = fileOff (newLength);
    if (last_file_size == 0 && new_last_file != 0)
    {
        new_last_file --;
        last_file_size = SPLIT_FACTOR;
    }

    if (newLength > length_)
    {
        for (int fileno = cur_no_of_files-1; fileno <= new_last_file; fileno ++)
        {
            if (fileno >= cur_no_of_files)
            {
                const char* new_name = name4number (base_name_.c_str (), fileno);
                Fid fid = fileHandleMgr.create (new_name);
                if (fid == -1) throw CreateError ();
                fids_.push_back (fid);
            }
            int h = fileHandleMgr.handle (fids_[fileno]);
            int32 newsize = (fileno == new_last_file)?last_file_size:SPLIT_FACTOR;
            if (::_chsize (h, newsize) != 0)
            {
                if (errno == ENOSPC) throw NoDeviceSpace ();
                else ERR("Unable to enlarge file");
            }
        }
    }
    else if (newLength < length_)
    {
        for (int fileno = cur_no_of_files - 1; fileno > new_last_file; fileno --)
        {
            Fid fid = fids_[fileno];
            fileHandleMgr.close (fid);
            const char* name = name4number (base_name_.c_str (), fileno);
            ::unlink (name);
            fids_.pop_back ();
        }
        int h = fileHandleMgr.handle (fids_[new_last_file]);
        if (::_chsize (h, last_file_size) != 0) ERR("Unable to truncate file");
    }
    length_ = newLength;
    cpsync_ = false; // not always, but we want to be on the safe side

    return true;
}
bool SplitFile_imp::commit ()
{
    // commit all handles
    bool toR = true;
    for (int i = 0; i < fids_.size (); i ++)
        if (!fileHandleMgr.commit (fids_ [i])) toR = false;
    return toR;
}

bool SplitFile_imp::close ()
{
    if (!open_) throw FileNotOpen ();

    bool toRet = true;
    open_ = false;
    for (FidVect::iterator itr = fids_.begin (); itr != fids_.end (); itr ++)
    {
        if (!fileHandleMgr.close (*itr)) toRet = false;
    }
    fids_.clear ();
    return toRet;
}

};