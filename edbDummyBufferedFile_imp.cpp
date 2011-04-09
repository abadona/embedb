#define bufferedFileFactory_defined

#include "edbDummyBufferedFile_imp.h"
#include "edbSplitFileFactory.h"


namespace edb 
{
static DummyBufferedFileFactory_imp theFactory;
BufferedFileFactory& bufferedFileFactory = theFactory;


DummyBufferedFile_imp::DummyBufferedFile_imp (File& file)
:
file_ (file)
{
}
DummyBufferedFile_imp::~DummyBufferedFile_imp ()
{
    if (file_.isOpen ()) file_.close ();
}

void DummyBufferedFile_imp::dump (int bno)
{
    Buffer& curbuf = buffers_ [bno];
    if (curbuf.free_) return;
    if (curbuf.dirty_)
    {
        file_.seek (curbuf.fileoff_);
        file_.write (curbuf.data_, curbuf.size_);
        curbuf.dirty_ = false;
    }
    curbuf.free_ = true;
    lru_.erase (curbuf.lru_pos_);
}

void DummyBufferedFile_imp::commit (int bno)
{
    Buffer& curbuf = buffers_ [bno];
    if (curbuf.free_) return;
    if (curbuf.dirty_)
    {
        file_.seek (curbuf.fileoff_);
        file_.write (curbuf.data_, curbuf.size_);
        curbuf.dirty_ = false;
    }
}

void DummyBufferedFile_imp::forget (int bno)
{
    Buffer& curbuf = buffers_ [bno];
    if (curbuf.free_) return;
    curbuf.free_ = true;
    lru_.erase (curbuf.lru_pos_);
}

void DummyBufferedFile_imp::pop (int bno)
{
    Buffer& curbuf = buffers_ [bno];
    if (!curbuf.free_)
        lru_.erase (curbuf.lru_pos_);
    curbuf.free_ = false;
    lru_.push_front (bno);
    curbuf.lru_pos_ = lru_.begin ();
}

int DummyBufferedFile_imp::worst ()
{
    for (int bno = 0; bno < DBF_BUFNO; bno ++)
        if (buffers_ [bno].free_)
            break;
    if (bno == DBF_BUFNO)
        bno = lru_.back ();
    return bno;
}

const void* DummyBufferedFile_imp::get (FilePos pos, BufLen count)
{
    if (count > DBF_BUFSZ) ERR("Read request length overflow");
    // check weather already in some buffer
    for (int bno = 0; bno < DBF_BUFNO; bno ++)
    {
        Buffer& curbuf = buffers_ [bno];
        if ((!curbuf.free_) &&
            (pos >= curbuf.fileoff_) &&
            (pos + count <=  curbuf.fileoff_ + curbuf.size_))
        {
            pop (bno);
            return curbuf.data_ + (pos - curbuf.fileoff_);
        }
    }

    // dump overlaps (for clarity :))
    for (int kbno = 0; kbno < DBF_BUFNO; kbno ++)
    {
        Buffer& cb = buffers_ [kbno];
        if ((!cb.free_) && (cb.fileoff_ < pos + count) && (pos < cb.fileoff_ + cb.size_)) // intersection
            dump (kbno);
    }

    // if read behing EOF, expand the file
    if (file_.length () < pos + count)
        file_.chsize (pos + count);

    // find the free or lru buffer for filling
    bno = worst ();
    dump (bno);
    pop (bno);
    Buffer& curbuf = buffers_ [bno];
    file_.seek (pos);
    file_.read (curbuf.data_, count);
    curbuf.fileoff_ = pos;
    curbuf.size_ = count;

    return curbuf.data_;
}

BufLen DummyBufferedFile_imp::write (void* buf, FilePos pos, BufLen count)
{
    if (count > DBF_BUFSZ) ERR("Write request length overflow");
    // find weather within buffer; if yes, write there and mark dirty
    for (int bno = 0; bno < DBF_BUFNO; bno ++)
    {
        Buffer& curbuf = buffers_ [bno];
        if ((!curbuf.free_) &&
            (pos >= curbuf.fileoff_) &&
            (pos + count <=  curbuf.fileoff_ + DBF_BUFSZ))
        {
            pop (bno);
            int off = pos - curbuf.fileoff_;
            memcpy (curbuf.data_ + off, buf, count);
            if (off + count > curbuf.size_) curbuf.size_ = off + count;
            curbuf.dirty_ = true;
        }
    }
    // if not, fill new buffer with this data (?)
    bno = worst ();
    dump (bno);
    pop (bno);
    Buffer& curbuf = buffers_ [bno];
    curbuf.dirty_ = true;
    curbuf.fileoff_ = pos;
    curbuf.size_ = count;
    memcpy (curbuf.data_, buf, count);

    // kill overlaps 
    for (int kbno = 0; kbno < DBF_BUFNO; kbno ++)
    {
        if (kbno == bno) continue;
        Buffer& cb = buffers_ [kbno];
        if ((!cb.free_) && (cb.fileoff_ < pos + count) && (pos < cb.fileoff_ + cb.size_)) // intersection
            dump (kbno);
    }
    return count;
}

bool DummyBufferedFile_imp::flush ()
{
    for (int bno = 0; bno < DBF_BUFNO; bno ++)
        commit (bno);
    return file_.commit ();
}

FilePos DummyBufferedFile_imp::length ()
{
    return file_.length ();
}

bool DummyBufferedFile_imp::chsize (FilePos newSize)
{
    // if shrinking, free covered pages / dump overlapped one if any
    if (newSize < file_.length ())
    {
        for (int bno = 0; bno < DBF_BUFNO; bno ++)
        {
            Buffer& cb = buffers_ [bno];
            if (cb.fileoff_ >= newSize)
                forget (bno);
            else if (cb.fileoff_ < newSize && cb.fileoff_ + cb.size_ > newSize)
            {
                commit (bno);
                cb.size_ -= (cb.fileoff_ - newSize);
            }
        }
    }
    return file_.chsize (newSize);
}

bool DummyBufferedFile_imp::close ()
{
    for (int bno = 0; bno < DBF_BUFNO; bno ++)
        dump (bno);
    return file_.close ();
}

BufferedFile& DummyBufferedFileFactory_imp::open (const char* directory, const char* basename) 
{
    File& f = splitFileFactory.open (directory, basename);
    return *new DummyBufferedFile_imp (f);
}

BufferedFile& DummyBufferedFileFactory_imp::create (const char* directory, const char* basename) 
{
    File& f = splitFileFactory.create (directory, basename);
    return *new DummyBufferedFile_imp (f);
}

bool DummyBufferedFileFactory_imp::exists (const char* directory, const char* basename) 
{
    return splitFileFactory.exists (directory, basename);
}

bool DummyBufferedFileFactory_imp::erase (const char* directory, const char* basename)	 
{
    return splitFileFactory.erase (directory, basename);
}

bool DummyBufferedFileFactory_imp::rename (const char* src_directory, const char* src_basename, const char* dst_directory, const char* dst_basename) 
{
    return splitFileFactory.rename (src_directory, src_basename, dst_directory, dst_basename);
;
}



};
