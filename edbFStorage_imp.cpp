// Copyright 2001 Victor Joukov, Denis Kaznadzey
#define FStorageFactory_defined
#include "edbFStorage_imp.h"
#include "edbExceptions.h"

namespace edb
{
static FStorageFactory_imp theFactory;
FStorageFactory& fStorageFactory = theFactory;

#define FSTORAGE_IMP_DEBUG

inline FilePos FStorage_imp::first_ ()
{
    return sizeof (FileHdr) + hdr_.hdrsize_;
}
inline FilePos FStorage_imp::first_free_ ()
{
    return offset_ (hdr_.rectotal_);   
}
inline FilePos FStorage_imp::offset_ (RecNum recno)
{
    return first_ () + recno * (hdr_.reclen_ + 1);
}

FStorage_imp::FStorage_imp   (File& file)
:
file_ (file)
{
}

void FStorage_imp::init_ (FRecLen reclen, FRecLen headersize)
{
    hdr_.clean_ ();
    memcpy (hdr_.sign_, FileSignature, sizeof (FileSignature));
    hdr_.reclen_ = reclen;
    hdr_.hdrsize_ = headersize;
    file_.chsize (0L);
    dump_hdr_ ();
}

void FStorage_imp::open_ ()
{
    hdr_.clean_ ();
    if (file_.seek (0) != 0) 
        throw SeekError ();
    if (file_.read (&hdr_, sizeof (hdr_) != sizeof (hdr_)))
        throw WrongFileType ();
    if (memcmp (hdr_.sign_, FileSignature, sizeof (FileSignature)))
        throw WrongFileType ();
}

void FStorage_imp::dump_hdr_ ()
{
    file_.seek (0L);
    file_.write (&hdr_, sizeof (hdr_));
}

// record - level operations
bool FStorage_imp::isValid (RecNum recnum)
{
    // check whether number is valid
    if (recnum >= hdr_.rectotal_) 
        return false;
    // check whether record is not deleted
    uint8 freeflag;
    FilePos off = offset_ (recnum);
    if (file_.seek (off) != off) 
        throw SeekError ();
    if (file_.read (&freeflag, sizeof (freeflag)) != sizeof (freeflag)) 
        throw ReadError ();
#ifdef FSTORAGE_IMP_DEBUG
    if (freeflag != FREEFLAG && freeflag != USEDFLAG)
        ERR("Wrong value for free flag!");
#endif
    return (freeflag == USEDFLAG);
}

RecNum FStorage_imp::allocRec ()
{
    RecNum unused = hdr_.rectotal_;
    hdr_.rectotal_ ++;

    uint8 freeflag = USEDFLAG;
    FilePos off = offset_ (unused);
    if (file_.seek (off) != off) 
        throw SeekError ();
    if (file_.write (&freeflag, sizeof (freeflag)) != sizeof (freeflag)) 
        throw WriteError ();

    return unused;
}

RecNum FStorage_imp::addRec (void* data)
{
    RecNum newRecno = allocRec ();
    writeRec (newRecno, data);
    return newRecno;
}

bool FStorage_imp::readRec (RecNum recnum, void* data)
{
    // check whether number is valid
    if (recnum >= hdr_.rectotal_) 
        return false;
    // check whether record is not deleted
    uint8 freeflag;
    FilePos off = offset_ (recnum);
    if (file_.seek (off) != off) 
        throw SeekError ();
    if (file_.read (&freeflag, sizeof (freeflag)) != sizeof (freeflag)) 
        throw ReadError ();
#ifdef FSTORAGE_IMP_DEBUG
    if (freeflag != FREEFLAG && freeflag != USEDFLAG)
        ERR("Wrong value for free flag!");
#endif
    if (freeflag == FREEFLAG)
        return false;
    if (file_.read (data, hdr_.reclen_) != hdr_.reclen_)
        throw ReadError ();
    return true;
}

bool FStorage_imp::writeRec (RecNum recnum, void* data)
{
    // check whether number is valid
    if (recnum >= hdr_.rectotal_) 
        return false;
    // check whether record is not deleted
    uint8 freeflag;
    FilePos off = offset_ (recnum);
    if (file_.seek (off) != off) 
        throw SeekError ();
    if (file_.read (&freeflag, sizeof (freeflag)) != sizeof (freeflag)) 
        throw ReadError ();
#ifdef FSTORAGE_IMP_DEBUG
    if (freeflag != FREEFLAG && freeflag != USEDFLAG)
        ERR("Wrong value for free flag!");
#endif
    if (freeflag == FREEFLAG)
        return false;
    if (file_.write (data, hdr_.reclen_) != hdr_.reclen_)
        throw WriteError ();
    return true;
}

bool FStorage_imp::freeRec (RecNum recnum)
{
    // check whether number is valid
    if (recnum >= hdr_.rectotal_) 
        return false;
    // check whether record is not deleted
    uint8 freeflag;
    FilePos off = offset_ (recnum);
    if (file_.seek (off) != off) 
        throw SeekError ();
    if (file_.read (&freeflag, sizeof (freeflag)) != sizeof (freeflag)) 
        throw ReadError ();
#ifdef FSTORAGE_IMP_DEBUG
    if (freeflag != FREEFLAG && freeflag != USEDFLAG)
        ERR("Wrong value for free flag!");
#endif
    if (freeflag == FREEFLAG)
        return false;

    freeflag = FREEFLAG;
    if (file_.seek (off) != off) 
        throw SeekError ();
    if (file_.write (&freeflag, sizeof (freeflag)) != sizeof (freeflag)) 
        throw WriteError ();
    hdr_.freetotal_ ++;
    return true;
}


// hinted prefetch 
void FStorage_imp::hintAdd (RecNum* recnum_buffer, uint32 number)
{
}

void FStorage_imp::hintReset ()
{
}


// storage - level operations
FRecLen FStorage_imp::getRecLen () const
{
    return hdr_.reclen_;
}

RecNum FStorage_imp::getRecCount () const
{
    return hdr_.rectotal_ - hdr_.freetotal_;
}

RecNum FStorage_imp::getFreeCount () const
{
    return hdr_.freetotal_;
}

bool FStorage_imp::flush ()
{
    dump_hdr_ ();
    return file_.commit ();
}

bool FStorage_imp::close ()
{
    dump_hdr_ ();
    return file_.close ();
}

bool FStorage_imp::isOpen () const
{
    if (file_.isOpen ())
        if (memcmp (hdr_.sign_, FileSignature, sizeof (FileSignature)) == 0)
            return true;
    return false;
}

FRecLen FStorage_imp::getheaderSize  () const
{
    return hdr_.hdrsize_;
}

void FStorage_imp::readHeader     (void* buffer, BufLen buflen)
{
    BufLen rsize = min_ (buflen, hdr_.hdrsize_);
    if (!rsize) return;

    if (file_.seek (sizeof (FileHdr)) != sizeof (FileHdr))
        throw SeekError ();
    if (file_.read (buffer, rsize) != rsize)
        throw ReadError ();
}

void FStorage_imp::writeHeader    (void* buffer, BufLen buflen)
{
    BufLen wsize = min_ (buflen, hdr_.hdrsize_);
    if (!wsize) return;

    if (file_.seek (sizeof (FileHdr)) != sizeof (FileHdr))
        throw SeekError ();
    if (file_.write (buffer, wsize) != wsize)
        throw WriteError ();
}


FStorage& FStorageFactory_imp::wrap (File& file)
{
    FStorage_imp& fs = *new FStorage_imp (file);
    fs.open_ ();
    return fs;
}
FStorage& FStorageFactory_imp::init (File& file, FRecLen reclen, FRecLen headersize)
{
    FStorage_imp& fs = *new FStorage_imp (file);
    fs.init_ (reclen, headersize);
    return fs;
}
bool FStorageFactory_imp::validate (File& file)
{
    FStorage_imp& storage = *new FStorage_imp (file);
    try
    {
        storage.open_ ();
        storage.close ();
    }
    catch (WrongFileType&)
    {
        return false;
    }
    catch (FileStructureCorrupt&)
    {
        return false;
    }
    return true;
}


};
