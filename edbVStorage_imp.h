// Copyright 2001 Victor Joukov, Denis Kaznadzey
#ifndef edbVStorage_imp_h
#define edbVStorage_imp_h

#include "edbTypes.h"
#include "edbFile.h"
#include "edbVStorage.h"
#include <vector>
#include <string.h>

namespace edb
{

    // The layout for the variable-length storage file:

// --BEGIN OF FILE--
// SIGNATURE        (sizeof (FileSignature)) bytes      signature of the VLStorage file
// LASTBLOCK        (sizeof (RecLocator)) bytes         offset to sentinel (EODATA) block
// TOTAL_REC_NO     (sizeof (RecNum)) bytes             total number of records in the file
// TOTAL_USED_SPACE (sizeof (uint64)) bytes             total used space in bytes
// TOTAL_FREE_SPACE (sizeof (uint64)) bytes             total free space in bytes
// BANDNO           (sizeof (uint32)) bytes             number of bands in free space storage
// **** array of BANDNO BANDS:
//    MINSIZE       (sizeof (RecLocator)) bytes         the minimal length of free block in the band
//    COUNT         (sizeof (uint64)) bytes             number of elements in the band  
//    SPACE         (sizeof (uint64)) bytes             free space in this band
//    FIRST_BLOCK   (sizeof (RecLocator)) bytes         offset of first block in band
//    LAST_BLOCK    (sizeof (RecLocator)) bytes         offset of last block in band
// **** data area : consists of BLOCKS:
//    SIGNATURE     (sizeof (BlockSignature)) bytes     signature of the file
//    FREEFLAG      1 byte                              \xFF (free) or \x0 (occupied)
//    PREVBLOCK     (sizeof (RecLocator)) bytes         address of previous block. For the very first block, this is RECLOCATOR_MAX
//    NEXTBLOCK     (sizeof (RecLocator)) bytes         address of next block. For the very last block, this is RECLOCATOR_MAX
// **** block data: 
// if Occupied:  
//    DATA          (nextblock - thisaddr - sizeof(BLOCKHDR)) bytes of DATA
// if Free:      
//    PREVFREE      (sizeof (RecLocator)) bytes         address of the prev free block or RECLOCATOR_MAX if this is the first free block
//    NEXTFREE      (sizeof (RecLocator)) bytes         address of the next free block or RECLOCATOR_MAX if this is the last free block

// The block list always contains the sentinel block at the end, with RECLOCATOR_MAX in NEXTBLOCK field and with no data (the EOF may appear directly after header)


static const char BlockSignature [] = {'V', 's', 'B'}; 
static const char FileSignature  [] = {'V', 'a', 'R', 'l', 'E', 'n', 'G', 't', 'H', 's', 'T', 'o', 'R', 'a', 'G', 'e'}; 
static const unsigned char FREEFLAG = '\xff';
static const unsigned char USEDFLAG = '\0';


struct VRec_impDescriptor
{
    RecLocator locator_;
    RecLen length_;
    unsigned char free_;
};


class VStorage_imp : public VStorage
{
#ifdef _MSC_VER
#pragma pack (push, 1)
#pragma warning (push)
#pragma warning (disable : 4200) // non-standard extension - array with zero elements
#endif
    struct Band
    {
        Band () : first_ (RECLOCATOR_MAX), last_ (RECLOCATOR_MAX), minsize_ (0L), count_ (0L), space_ (0L) {}
        Band (RecLen minsize) : first_ (RECLOCATOR_MAX), last_ (RECLOCATOR_MAX), minsize_ (minsize), count_ (0L), space_ (0L) {}
        bool operator < (const Band& b) const { return minsize_ < b.minsize_; }
        RecLen          minsize_;
        uint64          count_;
        uint64          space_;
        RecLocator      first_;
        RecLocator      last_;
    };
    struct FileHdr 
    {
        FileHdr () { clean_ (); }
        void clean_ () { last_off_ = (0L), used_count_ = (0L), free_count_ = (0L), used_space_ = (0L), free_space_ = (0L), band_count_ = (0), memset (sign_, 0, sizeof (FileSignature)); }
        char        sign_ [sizeof (FileSignature)];
        RecLocator  last_off_;
        RecNum      used_count_;
        RecNum      free_count_;
        uint64      used_space_;
        uint64      free_space_;
        uint32      band_count_;
    };
    struct BlockHdr
    {
        BlockHdr () : free_ (FREEFLAG), prev_ (RECLOCATOR_MAX), next_ (RECLOCATOR_MAX) { memcpy (sign_, BlockSignature, sizeof (BlockSignature)); }
        char sign_ [sizeof (BlockSignature)];
        unsigned char free_;
        RecLocator prev_;
        RecLocator next_;
    };
    struct FreePos
    {
        void def () { prevfree_ = RECLOCATOR_MAX; nextfree_ = RECLOCATOR_MAX; }
        RecLocator prevfree_;
        RecLocator nextfree_;
    };
    struct Block
    {
        BlockHdr hdr_;
        union
        {
            FreePos fp_;
            char data_ [];
        };
    };

#ifdef _MSC_VER
#pragma warning (pop)
#pragma pack (pop)
#endif
    
    File&       file_;
    FileHdr     hdr_;
    Band*       bands_;


protected:

    // utility functions
    RecLocator  firstoff_      () const;    // calculates the offset of the first block of data
    RecLen      datalen_       (BlockHdr& blockhdr, RecLocator locator); // calculates the length of data area for the block
    uint32      calc_band_     (RecLen reclen); // calculates the number of free band for the block's length

    void        read_bhdr_     (BlockHdr& blockhdr, RecLocator locator); // reads in the block header at locator
    void        write_bhdr_    (BlockHdr& blockhdr, RecLocator locator); // writes out the block header at locator
    void        read_freepos_  (FreePos& freepos, RecLocator locator); // reads in the FreePos at locator
    void        write_freepos_ (FreePos& freepos, RecLocator locator); // writes out the FreePos at locator
    void        move_freepos_  (FreePos& freepos, uint32 bandno, RecLocator newplace); // writes the freepos at newplace, adjusting the next and prev free blocks
    void        flush_hdr_     (); // flushes control structures (header) to disk
    void        free_remove_   (RecLocator locator, uint32 bandno, RecLen space); // removes the block at Locator from free space control structures
    void        free_removeRec_(FreePos& freepos, uint32 bandno, RecLen space); // removes the FreePos from free space control structures
    void        free_add_      (RecLocator locator, uint32 bandno, RecLen space); // adds block at locator to free space control structures
    void        join_bhdrs_    (RecLocator loc, BlockHdr& block, RecLocator nextLoc, BlockHdr& nextBlock); // joins together the two blocks (makes them point to each other)
    void        split_free_    (RecLocator loc, BlockHdr& block, RecLen len); // splits the FREE block into two, first one of the size len. Marks first block as used, second as free.
    void        split_used_    (RecLocator loc, BlockHdr& block, RecLocator nextLocator, BlockHdr& nextBlock, RecLen len); // splits the USED block into two, first one of the size len. Marks first block as used, second as free. Assumes that the splitted block is followed by USED block.
    void        reuse_block_   (RecLocator loc, BlockHdr& block); // reuses the free block
    void        join_blocks_   (RecLocator loc, BlockHdr& block, BlockHdr& nextBlock); // joins the block with next free block
    void        resize_block_  (RecLocator loc, BlockHdr& block, BlockHdr& nextBlock, RecLen len); // expands or shrinks the block (by shrinking or expanding nextBlock) to accomodate the len bytes
    void        resize_last_   (RecLocator loc, BlockHdr& block, BlockHdr& nextBlock, RecLen len); // change size of the last block
    void        resize_mid_    (RecLocator loc, BlockHdr& block, BlockHdr& nextBlock, RecLen len); // change size of the intermediate
    RecLocator  alloc_at_end_  (RecLen len); // allocate the new block at the end of file
    RecLocator  alloc_at_band_ (RecLen len, uint32 bandno); // allocate the new block out of the free band
    void        move_rec_      (RecLocator dest, BlockHdr& srcBlock, RecLocator srcLoc);
	void		move_data_     (FilePos src_start, FilePos src_end, FilePos dest_start);
	void		move_and_fill_ (RecLocator src_locator, RecLocator dest_locator, const void* data, BufLen data_len, RecOff offset, RecLen old_data_len, RecLen full_len);

    // walk over ALL blocks (both free and used
    bool        firstRec_	   (VRec_impDescriptor& rec);
    bool        nextRec_       (VRec_impDescriptor& rec);

    // high_level functions
    void        init_          (const uint64* bandbounds = NULL, uint32 bandno = 0); // initializes the VS file
    void        open_          (); // opens the VS file and reads in the control structures


                VStorage_imp   (File& file);

public:
                ~VStorage_imp  ();
    // record - level operations 
    bool        isValid        (RecLocator locator);
    RecLocator  allocRec       (RecLen len);
    RecLen      getRecLen      (RecLocator locator);
    RecLocator  addRec         (const void* data, RecLen len);
    RecLocator  reallocRec     (RecLocator locator, RecLen len);
    bool        readRec        (RecLocator locator, void* data, BufLen len, RecOff offset = 0);
    bool        writeRec       (RecLocator locator, const void* data, BufLen len, RecOff offset = 0);
    bool        freeRec        (RecLocator locator);
	RecLocator  sliceRec       (RecLocator locator, const void* data, BufLen data_len, RecOff offset, RecLen old_data_len, RecLen full_len);

    // hinted prefetch 
    void        hintAdd        (VRecDescriptor* descriptors, uint32 number);
    void        hintReset      ();

    // record enumeration
    RecNum      getRecCount    () const;
    bool        firstRec	   (VRecDescriptor& rec);
    bool        nextRec	       (VRecDescriptor& rec);

    // storage - level operations     
    bool        flush          ();
    bool        close          ();
    bool        isOpen         () const;

    //statistics
    uint64      usedSpace      () const;
    uint64      freeSpace      () const;
    uint64      usedCount      () const;
    uint64      freeCount      () const;

    friend class VStorageFactory_imp;
};

class VStorageFactory_imp : public VStorageFactory
{
public:
                ~VStorageFactory_imp () {}
    VStorage&   wrap           (File& file);
    VStorage&   init           (File& file);
    bool        validate       (File& file);
};


};

#endif
