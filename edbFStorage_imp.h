// Copyright 2001 Victor Joukov, Denis Kaznadzey
#ifndef edbFStorage_h
#define edbFStorage_imp_h

#include "edbTypes.h"
#include "edbFStorage.h"

// Layout of the FixedLengthStorage file:
// --- BEGIN OF FILE ---
// signature    sizeof (FileSignature)
// reclen       sizeof (FRecLen)
// rectotal     sizeof (RecNum)
// freetotal    sizeof (RecNum)
// headersize   sizeof (FRecLen)
// header       headersize bytes
// Records array - rectotal records
//      consists of Records:
//          1 byte          freeflag (USEDFLAG for used; FREEFLAG for free)
//          reclen bytes    data


namespace edb
{

static const char FileSignature  [] = {'F', 'i', 'X', 'l', 'E', 'n', 'G', 't', 'H', 's', 'T', 'o', 'R', 'a', 'G', 'e'}; 
static const unsigned char FREEFLAG = '\xff';
static const unsigned char USEDFLAG = '\0';


class FStorage_imp : public FStorage
{
#ifdef _MSC_VER
#pragma pack (push, 1)
#pragma warning (push)
//#pragma warning (disable : 4200) // non-standard extension - array with zero elements
#endif
    struct FileHdr 
    {
        FileHdr () { clean_ (); }
        void clean_ () { reclen_ = 0, rectotal_ = 0L, freetotal_ = 0L, hdrsize_ = 0; memset (sign_, 0, sizeof (FileSignature)); }
        char        sign_ [sizeof (FileSignature)];
        FRecLen     reclen_;
        RecNum      rectotal_;
        RecNum      freetotal_;
        FRecLen     hdrsize_;
    };
#ifdef _MSC_VER
//#pragma warning (pop)
#pragma pack (pop)
#endif


    File&       file_;
    FileHdr     hdr_;

    void        init_ (FRecLen reclen, FRecLen headersize);
    void        open_ ();
    void        dump_hdr_ ();
    FilePos     first_ ();
    FilePos     first_free_ ();
    FilePos     offset_ (RecNum recno);

                FStorage_imp   (File& file);
public:
    // record - level operations
    bool        isValid        (RecNum recnum);
    RecNum      allocRec       ();
    RecNum      addRec         (void* data);
    bool        readRec        (RecNum recnum, void* data);
    bool        writeRec       (RecNum recnum, void* data);
    bool        freeRec        (RecNum recnum);

    // hinted prefetch 
    void        hintAdd        (RecNum* recnum_buffer, uint32 number);
    void        hintReset      ();

    // storage - level operations
    FRecLen     getRecLen      () const;
    RecNum      getRecCount    () const;
    RecNum      getFreeCount   () const;
    bool        flush          ();
    bool        close          ();
    bool        isOpen         () const;
    FRecLen     getheaderSize  () const;
    void        readHeader     (void* buffer, BufLen buflen);
    void        writeHeader    (void* buffer, BufLen buflen);

    friend class FStorageFactory_imp;
};

class FStorageFactory_imp : public FStorageFactory
{
public:
    FStorage&   wrap           (File& file);
    FStorage&   init           (File& file, FRecLen reclen, FRecLen headersize = 0);
    bool        validate       (File& file);
};

};

#endif