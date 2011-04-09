#ifndef edbDummyBufferedFile_imp_h
#define edbDummyBufferedFile_imp_h

#include "edbBufferedFile.h"
#include "edbFile.h"
#include <list>

namespace edb
{
#define DBF_BUFSZ 0x10000 // 64 Kb
#define DBF_BUFNO 16      // 1 Mb buffers (per file!!!)
    typedef std::list<int> IntList;

class DummyBufferedFile_imp : public BufferedFile
{
private:
    
    struct Buffer
    {
        Buffer () : free_ (true), dirty_ (false) {}
        FilePos fileoff_;
        BufLen size_;
        bool free_;
        bool dirty_;
        IntList::iterator lru_pos_;
        char data_ [DBF_BUFSZ];
    };
    
    Buffer        buffers_ [DBF_BUFNO];
    File&         file_;
    IntList       lru_;

    void          dump           (int bufno);
    void          commit         (int bufno);
    void          forget         (int bufno);
    void          pop            (int bufno);
    int           worst          ();

protected:
                  DummyBufferedFile_imp (File& file);
public:
                  ~DummyBufferedFile_imp ();
    const void*   get            (FilePos pos, BufLen count);
    BufLen        write          (void* buf, FilePos pos, BufLen count);
    bool          flush          ();
    FilePos       length         ();
    bool          chsize         (FilePos newSize);
    bool          close          ();

    friend class DummyBufferedFileFactory_imp;
};

class DummyBufferedFileFactory_imp : public BufferedFileFactory
{
public:
    BufferedFile& open           (const char* directory, const char* basename);                                                              
    BufferedFile& create         (const char* directory, const char* basename);                                                              
    bool          exists         (const char* directory, const char* basename);                                                              
    bool          erase          (const char* directory, const char* basename);	                                                             
    bool          rename         (const char* src_directory, const char* src_basename, const char* dst_directory, const char* dst_basename); 
};

};

#endif
