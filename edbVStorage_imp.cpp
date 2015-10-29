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

#define VStorageFactory_defined
#include "edbVStorage_imp.h"
#include "edbExceptions.h"

namespace edb
{
static VStorageFactory_imp theFactory;
VStorageFactory& vStorageFactory = theFactory;

#define VSTORAGE_IMP_DEBUG

//#define SMALL_LADDER
#ifndef SMALL_LADDER
static uint64 DEF_BANDBOUNDS [] = {0x10000000, 0x4000000, 0x1000000, 0x400000, 0x100000, 0x40000, 0x10000, 0x4000}; // 256 Mb, 64Mb, 16Mb, 4Mb, 1Mb, 256Kb, 64Kb, 16Kb
#else
static uint64 DEF_BANDBOUNDS [] = {0x100, 0x80, 0x40}; // 256 bytes. 128 bytes , 64 bytes
#endif
static uint32 DEF_BANDNO = (sizeof (DEF_BANDBOUNDS)/sizeof (uint64));



VStorage_imp::VStorage_imp (File& file)
:
file_ (file),
bands_ (NULL)
{
}

VStorage_imp::~VStorage_imp ()
{
    // delete managed objects 
    if (bands_) delete [] bands_;
}

inline RecLocator VStorage_imp::firstoff_ () const
{
    return  sizeof (FileHdr) + hdr_.band_count_ * sizeof (Band);
}

inline RecLen VStorage_imp::datalen_ (VStorage_imp::BlockHdr& blockhdr, RecLocator locator)
{
    return (blockhdr.next_ == RECLOCATOR_MAX) ? 0 : ((blockhdr.next_ - locator) - sizeof (BlockHdr));
}


uint32 VStorage_imp::calc_band_ (RecLen reclen)
{
    uint32 bidx;
    for (bidx = 0; bidx < hdr_.band_count_; bidx ++)
        if (reclen >= bands_ [bidx].minsize_)
            break;
    return bidx;
}

void VStorage_imp::read_bhdr_ (VStorage_imp::BlockHdr& blockhdr, RecLocator locator)
{
#ifdef VSTORAGE_IMP_DEBUG
    if ((locator > hdr_.last_off_) || (locator < firstoff_ ()))
        ERR("read_bhdr_: wrong locator passed");
#endif
    // seek to locator
    FilePos seekpos = file_.seek (locator);
    if (seekpos != locator) 
        throw BadLocator ();
    // read block header
    BufLen rdlen = file_.read (&blockhdr, sizeof (blockhdr));
    if (rdlen != sizeof (blockhdr))
        throw BadLocator ();
    // check weather it has proper signature
    if (memcmp (blockhdr.sign_, BlockSignature, sizeof (BlockSignature)))
        throw BadLocator ();
}

void VStorage_imp::write_bhdr_ (VStorage_imp::BlockHdr& blockhdr, RecLocator locator)
{
#ifdef VSTORAGE_IMP_DEBUG
    if ((locator > hdr_.last_off_) || (locator < firstoff_ ()))
        ERR("write_bhdr_: wrong locator passed");
    // check weather the passed block is correct
    if (memcmp (blockhdr.sign_, BlockSignature, sizeof (BlockSignature)))
        throw BadParameters ();
#endif
    // seek to locator
    FilePos seekpos = file_.seek (locator);
    if (seekpos != locator) 
        throw BadLocator ();
    // read block header
    BufLen wrlen = file_.write (&blockhdr, sizeof (blockhdr));
    if (wrlen != sizeof (blockhdr))
        throw BadLocator ();
}

void VStorage_imp::read_freepos_ (VStorage_imp::FreePos& freepos, RecLocator locator)
{
#ifdef VSTORAGE_IMP_DEBUG
    if ((locator > hdr_.last_off_) || (locator < firstoff_ ()))
        throw BadParameters ();
#endif
    if (file_.seek (locator + sizeof (BlockHdr)) != locator + sizeof (BlockHdr))
        throw BadParameters ();
    if (file_.read (&freepos, sizeof (freepos)) != sizeof (freepos))
        throw BadParameters ();
}

void VStorage_imp::write_freepos_ (VStorage_imp::FreePos& freepos, RecLocator locator)
{
#ifdef VSTORAGE_IMP_DEBUG
    if ((locator > hdr_.last_off_) || (locator < firstoff_ ()))
        throw BadParameters ();
#endif
    if (file_.seek (locator + sizeof (BlockHdr)) != locator + sizeof (BlockHdr))
        throw BadParameters ();
    if (file_.write (&freepos, sizeof (freepos)) != sizeof (freepos))
        throw BadParameters ();
}

void VStorage_imp::move_freepos_ (VStorage_imp::FreePos& freepos, uint32 bandno, RecLocator newPlace)
{
    // change next and prev free blocks
    if (freepos.nextfree_ == RECLOCATOR_MAX)
        bands_[bandno].last_ = newPlace;
    else
    {
        FreePos nextFreePos;
        read_freepos_ (nextFreePos, freepos.nextfree_);
        nextFreePos.prevfree_ = newPlace;
        write_freepos_ (nextFreePos, freepos.nextfree_);
    }

    if (freepos.prevfree_ == RECLOCATOR_MAX)
        bands_[bandno].first_ = newPlace;
    else
    {
        FreePos prevFreePos;
        read_freepos_ (prevFreePos, freepos.prevfree_);
        prevFreePos.nextfree_ = newPlace;
        write_freepos_ (prevFreePos, freepos.prevfree_);
    }
    
    // write down the firstFreepos at splitLocator 
    write_freepos_ (freepos, newPlace);
}
void VStorage_imp::flush_hdr_ ()
{
    // write the control structures to a file
    file_.seek (0);
    // write file header
    file_.write (&hdr_, sizeof (hdr_));
    // write bands
    file_.write (bands_, sizeof (Band) * hdr_.band_count_);
}

void VStorage_imp::free_remove_ (RecLocator locator, uint32 bandno, RecLen space)
{
#ifdef VSTORAGE_IMP_DEBUG
    if (bandno > hdr_.band_count_)
        ERR("free_remove_: bandno partameter out of range");
#endif

    if (bandno == hdr_.band_count_) 
    {
        hdr_.free_count_ --;
        hdr_.free_space_ -= space;
        return;
    }

    // read current freepos ref
    FreePos freepos;
    read_freepos_ (freepos, locator);

    // removeRec
    free_removeRec_ (freepos, bandno, space);

}

void VStorage_imp::free_removeRec_ (VStorage_imp::FreePos& freepos, uint32 bandno, RecLen space)
{
#ifdef VSTORAGE_IMP_DEBUG
    if (bandno >= hdr_.band_count_)
        ERR("free_remove_: bandno partameter out of range");
#endif

    // treat prev. member
    if (freepos.prevfree_ == RECLOCATOR_MAX)
        bands_ [bandno].first_ = freepos.nextfree_;
    else
    {
        // change the 'nextfree' reference from prev free block
        FreePos prev_freepos;
        read_freepos_ (prev_freepos, freepos.prevfree_);
        prev_freepos.nextfree_ = freepos.nextfree_;
        write_freepos_ (prev_freepos, freepos.prevfree_);
    }

    // treat next member
    if (freepos.nextfree_ == RECLOCATOR_MAX)
        bands_ [bandno].last_ = freepos.prevfree_;
    else
    {
        // change the 'prevfree' reference from next free block
        FreePos next_freepos;
        read_freepos_ (next_freepos, freepos.nextfree_);
        next_freepos.prevfree_ = freepos.prevfree_;
        write_freepos_ (next_freepos, freepos.nextfree_);
    }

#ifdef VSTORAGE_IMP_DEBUG
    if (bands_ [bandno].count_ == 0) 
        ERR("!");
#endif
    bands_ [bandno].count_ --;
    bands_ [bandno].space_ -= space;
#ifdef VSTORAGE_IMP_DEBUG
    if (bands_ [bandno].count_ == 0 && bands_[bandno].space_ != 0) 
        ERR("!");
#endif


    hdr_.free_count_ --;
    hdr_.free_space_ -= space;

}

void VStorage_imp::free_add_ (RecLocator locator, uint32 bandno, RecLen space)
{
#ifdef VSTORAGE_IMP_DEBUG
    if (bandno > hdr_.band_count_)
        ERR("free_add_: bandno partameter out of range");
    if (calc_band_ (space) != bandno)
        ERR("free_add_: wrong band");
#endif

    // adding to the tail of the band's freenode list
    if (bandno < hdr_.band_count_) 
    {
        // if list is empty, add the block to it
        FreePos newfreepos;
        newfreepos.def ();
        if (bands_ [bandno].last_ == RECLOCATOR_MAX)
        {
#ifdef VSTORAGE_IMP_DEBUG
            if (bands_ [bandno].first_ != RECLOCATOR_MAX)
                ERR("Inconsistent free band!")
#endif
            write_freepos_ (newfreepos, locator);
            bands_ [bandno].last_ = locator;
            bands_ [bandno].first_ = locator;
        }
        else 
        {
            // read the last one 
            FreePos lastfreepos;
            read_freepos_ (lastfreepos, bands_ [bandno].last_);
            // add the new free block to the end of the list (after last)
            lastfreepos.nextfree_ = locator;
            newfreepos.prevfree_ = bands_ [bandno].last_;
            write_freepos_ (lastfreepos, bands_ [bandno].last_);
            write_freepos_ (newfreepos, locator);
            bands_ [bandno].last_ = locator;
        }
        bands_ [bandno].count_ ++;
        bands_ [bandno].space_ += space;
    }
    hdr_.free_count_ ++;
    hdr_.free_space_ += space;
}


void VStorage_imp::join_bhdrs_ (RecLocator loc, BlockHdr& block, RecLocator nextLoc, BlockHdr& nextBlock)
{
    block.next_ = nextLoc;
    nextBlock.prev_ = loc;
    write_bhdr_ (block, loc);
    write_bhdr_ (nextBlock, nextLoc);
}

void VStorage_imp::split_free_ (RecLocator locator, BlockHdr& block, RecLen len)
{
#ifdef VSTORAGE_IMP_DEBUG
    if (block.free_ != FREEFLAG)
        ERR("split_free: splitting non-free block");
#endif
    
    RecLocator splitLocator = locator + sizeof (BlockHdr) + len;
    RecLen oldLen = datalen_ (block, locator);

    uint32 oldBand = calc_band_ (oldLen);
    FreePos oldFreePos;
    if (oldBand != hdr_.band_count_)
        read_freepos_ (oldFreePos, locator);

    RecLocator nextLocator = block.next_;
    BlockHdr nextBlock;
    read_bhdr_ (nextBlock, nextLocator);

#ifdef VSTORAGE_IMP_DEBUG
    if (splitLocator + sizeof (BlockHdr) + sizeof (FreePos) > nextLocator)
        ERR("split_bhdr: split block remainder is too short");
#endif

    // process block list - insert the split block

    BlockHdr splitBlock;

    block.free_ = USEDFLAG;
    block.next_ = splitLocator;
    splitBlock.prev_ = locator;
    splitBlock.next_ = nextLocator;
    splitBlock.free_ = FREEFLAG;
    nextBlock.prev_ = splitLocator;
    write_bhdr_ (block, locator);
    write_bhdr_ (splitBlock, splitLocator);
    write_bhdr_ (nextBlock, nextLocator);

    // process the freelists
    RecLen splitLen = datalen_ (splitBlock, splitLocator);
    uint32 splitBand = calc_band_ (splitLen);

    if (oldBand != hdr_.band_count_)
    {
        // add the remainder (splitBlock) to freelist
        if (splitBand != oldBand) // if band changed
        {
            // remove block from freelist
            free_removeRec_ (oldFreePos, oldBand, oldLen);
            // add splitBlock to freelist
            free_add_ (splitLocator, splitBand, splitLen);
        }
        else // same band
        {
            // 'reuse' the freepos - write it to the new (split) position
            move_freepos_ (oldFreePos, oldBand, splitLocator);

            // adjust global info
            bands_[oldBand].space_ -= oldLen;
            bands_[oldBand].space_ += splitLen;
            hdr_.free_space_ -= oldLen;
            hdr_.free_space_ += splitLen;
        }
    }
    else
    {
        hdr_.free_space_ -= oldLen;
        hdr_.free_space_ += splitLen;
    }
    hdr_.used_count_ ++;
    hdr_.used_space_ += len;
}

void VStorage_imp::split_used_ (RecLocator locator, BlockHdr& block, RecLocator nextLocator, BlockHdr& nextBlock, RecLen len)
{
#ifdef VSTORAGE_IMP_DEBUG
    if (block.free_ == FREEFLAG)
        ERR("split_used: splitting free block");
#endif
    
    RecLocator splitLocator = locator + sizeof (BlockHdr) + len;
    RecLen oldLen = datalen_ (block, locator);

#ifdef VSTORAGE_IMP_DEBUG
    if (splitLocator + sizeof (BlockHdr) + sizeof (FreePos) > nextLocator)
        ERR("split_bhdr: split block remainder is too short");
#endif

    // process block list - insert the split block

    BlockHdr splitBlock;

    block.next_ = splitLocator;
    splitBlock.prev_ = locator;
    splitBlock.next_ = nextLocator;
    splitBlock.free_ = FREEFLAG;
    nextBlock.prev_ = splitLocator;
    write_bhdr_ (block, locator);
    write_bhdr_ (splitBlock, splitLocator);
    write_bhdr_ (nextBlock, nextLocator);

    // process the freelists
    RecLen splitLen = datalen_ (splitBlock, splitLocator);
    uint32 splitBand = calc_band_ (splitLen);
	free_add_ (splitLocator, splitBand, splitLen);

	// reduce the used space by len - oldlen. 
    hdr_.used_space_ -= oldLen;
	hdr_.used_space_ += len;
}

void VStorage_imp::reuse_block_ (RecLocator locator, VStorage_imp::BlockHdr& block)
{
#ifdef VSTORAGE_IMP_DEBUG
    if (block.free_ != FREEFLAG)
        ERR("reuse_block_: non-free block passed");
#endif
    block.free_ = USEDFLAG;
    write_bhdr_ (block, locator);
    RecLen len = datalen_ (block, locator);
    uint32 band = calc_band_ (len);
    free_remove_ (locator, band, len);
    hdr_.used_count_ ++;
    hdr_.used_space_ += len;
}

void VStorage_imp::join_blocks_ (RecLocator locator, BlockHdr& block, BlockHdr& nextBlock)
{
    // determine len and band for nextBlock
    RecLen nextLen = datalen_ (nextBlock, block.next_);
    uint32 nextBand = calc_band_ (nextLen);
    
    // remove next block from free list
    free_remove_ (block.next_, nextBand, nextLen);

    // read nextNext block
    BlockHdr nextNextBlock;
    read_bhdr_ (nextNextBlock, nextBlock.next_);

    // join block with next block
    join_bhdrs_ (locator, block, nextBlock.next_, nextNextBlock);

    // update statistics
    hdr_.used_space_ += nextLen + sizeof (BlockHdr);
}

void VStorage_imp::resize_block_ (RecLocator locator, VStorage_imp::BlockHdr& block, VStorage_imp::BlockHdr& nextBlock, RecLen newLen)
{
#ifdef VSTORAGE_IMP_DEBUG
    if (block.free_ != USEDFLAG)
        ERR("resize_block_: free block passed");
    if (nextBlock.free_ != FREEFLAG)
        ERR("resize_block_: resizing with used next block");
#endif
    // if at the end
    if (nextBlock.next_ == RECLOCATOR_MAX)
        resize_last_ (locator, block, nextBlock, newLen);
    else
        resize_mid_ (locator, block, nextBlock, newLen);
}

void VStorage_imp::resize_last_ (RecLocator locator, VStorage_imp::BlockHdr& block, VStorage_imp::BlockHdr& nextBlock, RecLen newLen)
{
#ifdef VSTORAGE_IMP_DEBUG
    if (block.next_ != hdr_.last_off_)
        ERR("resize_last_: not a recorded last block");
    if (nextBlock.next_ != RECLOCATOR_MAX)
        ERR("resize_last_: sentinel next pointer is invalid");
#endif
    // calculate length of last block
    RecLen oldLen = datalen_ (block, locator);

    // calculate the new sentinel location
    RecLocator newSentinelLoc = locator + sizeof (BlockHdr) + newLen;

    // change the hdr_ to point to new sentinel's location
    hdr_.last_off_ = newSentinelLoc;

    // write the sentinel to new location
    write_bhdr_ (nextBlock, newSentinelLoc);

    // change the block to point to new sentinel's location
    block.next_ = newSentinelLoc;
    write_bhdr_ (block, locator);
    

    // update statistics (due to unsigned arithmetics this is done in two shots)
    hdr_.used_space_ += newLen;
    hdr_.used_space_ -= oldLen;
}

void VStorage_imp::resize_mid_ (RecLocator locator, VStorage_imp::BlockHdr& block, VStorage_imp::BlockHdr& nextBlock, RecLen newLen)
{
#ifdef VSTORAGE_IMP_DEBUG
    if (block.next_ == RECLOCATOR_MAX)
        ERR("resize_mid_: resizing sentinel");
    if (nextBlock.next_ == RECLOCATOR_MAX)
        ERR("resize_mid_: next block is sentinel");
#endif

    RecLen oldLen = datalen_ (block, locator);

    // remember next block's FreePos
    RecLen nextLen = datalen_ (nextBlock, block.next_);
    uint32 nextBand = calc_band_ (nextLen);
    FreePos nextFreePos;
    if (nextBand != hdr_.band_count_)
        read_freepos_ (nextFreePos, block.next_);

#ifdef VSTORAGE_IMP_DEBUG
    if (nextLen + datalen_ (block, locator) < newLen + sizeof (FreePos))
        ERR("resize_mid_: not enough space (called for wrong block)");
#endif

    // read the nextNext block
    BlockHdr nextNextBlock;
    read_bhdr_ (nextNextBlock, nextBlock.next_);

    // change the nextBlock location. Current and nextNext do not change their locations
    RecLocator newNextLocator = locator + sizeof (BlockHdr) + newLen;
    write_bhdr_ (nextBlock, newNextLocator);
    block.next_ = newNextLocator;
    write_bhdr_ (block, locator);
    nextNextBlock.prev_ = newNextLocator;
    write_bhdr_ (nextNextBlock, nextBlock.next_);

    // process freelists
    RecLen newNextLen = datalen_ (nextBlock, newNextLocator);
    uint32 newNextBand = calc_band_ (newNextLen);
    if (nextBand != newNextBand)
    {
        if (nextBand != hdr_.band_count_)
            free_removeRec_ (nextFreePos, nextBand, nextLen);
        else
        {
            hdr_.free_count_ --;
            hdr_.free_space_ -= nextLen;
        }
        free_add_ (newNextLocator, newNextBand, newNextLen);
    }
    else
    {
        if (nextBand != hdr_.band_count_)
        {
            // change the prevfree and nextfree references to next block; save nextFreePos into the next block
            move_freepos_ (nextFreePos, nextBand, newNextLocator);
            // update statistics
            bands_ [nextBand].space_ -= nextLen;
            bands_ [nextBand].space_ += newNextLen;
        }
        hdr_.free_space_ -= nextLen;
        hdr_.free_space_ += newNextLen;
    }
    hdr_.used_space_ -= oldLen;
    hdr_.used_space_ += newLen;
}

void VStorage_imp::init_ (const uint64* bandbounds, uint32 bandno)
{
    // set default parameters if needed
    if (!bandbounds || !bandno)
    {
        bandno = DEF_BANDNO;
        bandbounds = DEF_BANDBOUNDS;
    }

#ifdef VSTORAGE_IMP_DEBUG
    if (bandno == 0) 
        ERR ("init_: Wrong band number (0)");
    // check order
    uint64 prev_bound = UINT64_MAX;
    for (uint32 dbidx = 0; dbidx < bandno; dbidx ++)
    {
        if (prev_bound <= bandbounds [dbidx])
            ERR("init_: Band boundaries are not in decreasing order");
        prev_bound = bandbounds [dbidx];
    }
#endif
   
    // allocate bands array and init it
    hdr_.band_count_ = bandno;
    if (bands_) delete [] bands_;
    bands_ = new Band [hdr_.band_count_];
    for (uint32 bidx = 0; bidx < hdr_.band_count_; bidx ++)
        bands_ [bidx].minsize_ = bandbounds [bidx];

    memcpy (hdr_.sign_, FileSignature, sizeof (FileSignature));
    hdr_.last_off_ = firstoff_ ();
    hdr_.used_count_ = 0L;
    hdr_.free_count_ = 0L;
    hdr_.used_space_ = 0L;
    hdr_.free_space_ = 0L;

    // shrink the file
    file_.chsize (0);

    // write header
    flush_hdr_ ();

    // write sentinel block
    BlockHdr sentinel;
    file_.write (&sentinel, sizeof (sentinel));
    file_.commit ();
}

void VStorage_imp::open_ ()
{
    // clean header
    hdr_.clean_ ();
    // position at the beginning
    file_.seek (0L);
    // read signature
    file_.read (&hdr_, sizeof (hdr_));
    // check signature
    if (memcmp (hdr_.sign_, FileSignature, sizeof (FileSignature)))
        throw WrongFileType ();
    // read last_off_
    if (bands_) delete [] bands_;
    bands_ = new Band [hdr_.band_count_];
    // read bands
    if (file_.read (bands_, sizeof (Band)*hdr_.band_count_) != sizeof (Band)*hdr_.band_count_)
        throw FileStructureCorrupt ();
}

bool VStorage_imp::isValid (RecLocator locator)
{
    // read the record header
    BlockHdr blockhdr;
    file_.seek (locator);
    file_.read (&blockhdr, sizeof (blockhdr));
    // check weather it has proper signature
    if (memcmp (blockhdr.sign_, BlockSignature, sizeof (BlockSignature)))
        return false;
    // check weather it is not free
    if (blockhdr.free_ == FREEFLAG)
        return false;
    return true;
}

RecLocator VStorage_imp::allocRec (RecLen len)
{
    // find largest non-empty band
    uint32 bandidx = 0;
    while ((bandidx < hdr_.band_count_) && !bands_ [bandidx].count_) bandidx ++;
    // check weather this band is large enough
    if ((bandidx && len >= bands_ [bandidx - 1].minsize_ ) || (bandidx == hdr_.band_count_))
        return alloc_at_end_ (len);
    else
        return alloc_at_band_ (len, bandidx);
}

RecLocator VStorage_imp::alloc_at_end_ (RecLen len)
{
    // read the sentinel block
    BlockHdr oldSentinel;
    RecLocator oldSentinelLocator = hdr_.last_off_;
    read_bhdr_ (oldSentinel, oldSentinelLocator);

    // calc the new sentinel locator
    RecLocator newSentinelLocator = oldSentinelLocator + sizeof (BlockHdr) + len;

    // modify the former sentinel block 
    oldSentinel.free_ = USEDFLAG;
    oldSentinel.next_ = newSentinelLocator;
    write_bhdr_ (oldSentinel, oldSentinelLocator);

    // save the new sentinel locator
    hdr_.last_off_ = newSentinelLocator;

    // create new sentinel block
    BlockHdr newSentinel;
    newSentinel.prev_ = oldSentinelLocator;
    write_bhdr_ (newSentinel, newSentinelLocator);

    // update counts
    hdr_.used_count_ ++;
    hdr_.used_space_ += len;
    
    // return the locator of newly allocated (former sentinel) block
    return oldSentinelLocator;
}

RecLocator VStorage_imp::alloc_at_band_ (RecLen len, uint32 bandno)
{
#ifdef VSTORAGE_IMP_DEBUG
    if (bands_ [bandno].first_ == RECLOCATOR_MAX)
        ERR("alloc_at_band_: allocating from empty band");
#endif
    // load the first block from band
    BlockHdr firstBlock;
    RecLocator firstLocator = bands_ [bandno].first_;
    read_bhdr_ (firstBlock, firstLocator);

    // check whether fits
    RecLen firstLen = datalen_ (firstBlock, firstLocator);
    if (len > firstLen) // if not, call alloc_at_end
        return alloc_at_end_ (len);
    else if ((len <= firstLen) && (len + sizeof (BlockHdr) + sizeof (FreePos) > firstLen)) // reuse
        reuse_block_ (firstLocator, firstBlock);
    else
        split_free_ (firstLocator, firstBlock, len); 

    return firstLocator;
}

bool VStorage_imp::firstRec_ (VRec_impDescriptor& rec)
{
    if (!(hdr_.used_count_ + hdr_.free_count_)) return false;
    BlockHdr blockhdr;
    RecLocator locator = firstoff_ ();
    read_bhdr_ (blockhdr, locator);
    rec.locator_ = locator;
    rec.length_ = datalen_ (blockhdr, locator);
    rec.free_ = blockhdr.free_;
    return true;
}

bool VStorage_imp::nextRec_ (VRec_impDescriptor& rec)
{
    BlockHdr blockhdr;
    RecLocator locator = rec.locator_ + rec.length_ + sizeof (BlockHdr);
    read_bhdr_ (blockhdr, locator);
    if (blockhdr.next_ == RECLOCATOR_MAX) return false;
    rec.locator_ = locator;
    rec.length_ = (blockhdr.next_ - locator) - sizeof (BlockHdr);
    rec.free_ = blockhdr.free_;
    return true;
}

RecLen VStorage_imp::getRecLen (RecLocator locator)
{
    // read the record header
    BlockHdr blockhdr;
    read_bhdr_ (blockhdr, locator);
    if (blockhdr.free_ == FREEFLAG)
        throw FreeBlockUsed ();
    return datalen_ (blockhdr, locator);
}

RecLocator VStorage_imp::addRec (const void* data, RecLen len)
{
    RecLocator locator = allocRec (len);
    writeRec (locator, data, len);
    return locator;
}

RecLocator VStorage_imp::reallocRec (RecLocator locator, RecLen newLen)
{
    // read blockhdr under the locator
    BlockHdr block;
    read_bhdr_ (block, locator);
    RecLen oldLen = datalen_ (block, locator);

    // read the next block
    BlockHdr nextBlock;
    read_bhdr_ (nextBlock, block.next_);

    // if last block
    if (nextBlock.next_ == RECLOCATOR_MAX)
    {
        resize_last_ (locator, block, nextBlock, newLen);
        return locator;
    }
    // if followed by free block 
    else if (nextBlock.free_ == FREEFLAG)
    {
        RecLen nextLen = datalen_ (nextBlock, block.next_);
	    RecLen unitedSpace = nextLen + oldLen + sizeof (BlockHdr);

		// if next (free) block is long enough - elongate using some free block's space
        if (newLen <= unitedSpace && newLen + sizeof (BlockHdr) + sizeof (FreePos) > unitedSpace) // no space for splitter - join blocks
	    {
		    join_blocks_ (locator, block, nextBlock);
			return locator;
        }
	    else if (newLen + sizeof (BlockHdr) + sizeof (FreePos) <= unitedSpace) // enough space for splitter - move the boundary between blocks
		{
			resize_mid_ (locator, block, nextBlock, newLen);
            return locator;
	    }
		else // if no way to elongate - allocate
		{
		    RecLocator newLocator = allocRec (newLen); // allocate new block
			move_rec_ (newLocator, block, locator); // move data
		    freeRec (locator); // free old block
		    return newLocator; // return new locator
		}
    }
	else if (oldLen < newLen)
	{
		// new block is longer; mast move
	    RecLocator newLocator = allocRec (newLen); // allocate new block
		move_rec_ (newLocator, block, locator); // move data
	    freeRec (locator); // free old block
	    return newLocator; // return new locator
	}
	else if (newLen + sizeof (BlockHdr) + sizeof (FreePos) <= oldLen)
	{
		// shrink the block. The freing space is long enough - add to free space pool
		split_used_ (locator, block, block.next_, nextBlock, newLen);
		// the old locator is valid
		return locator;
	}
	else // shrink and not enough space for freeing: leave as is
		return locator;
}

void VStorage_imp::move_rec_ (RecLocator destLoc, VStorage_imp::BlockHdr& srcBlock, RecLocator srcLoc)
{
    BlockHdr destBlock;
    read_bhdr_ (destBlock, destLoc);
    RecLen srcLen = datalen_ (srcBlock, srcLoc);
    RecLen destLen = datalen_ (destBlock, destLoc);
    RecLen moveLen = min_ (srcLen, destLen);
    FilePos srcStart  = srcLoc + sizeof (BlockHdr);
    FilePos srcEnd    = srcLoc + sizeof (BlockHdr) + moveLen;
    FilePos destStart = destLoc + sizeof (BlockHdr);
	move_data_ (srcStart, srcEnd, destStart);
}

// use block size = 128 Kb
#define TRANSFER_SZ 0x20000  
void VStorage_imp::move_data_ (FilePos srcStart, FilePos srcEnd, FilePos destStart)
{
    char transfer_buf [TRANSFER_SZ];
	if (srcStart > destStart)
	{
	    while (srcStart < srcEnd)
		{
			BufLen curRd = min_ (TRANSFER_SZ, srcEnd - srcStart);
			if (file_.seek (srcStart) != srcStart) throw SeekError ();
			if (file_.read (transfer_buf, curRd) != curRd) throw ReadError ();
			if (file_.seek (destStart) != destStart) throw SeekError ();
			if (file_.write (transfer_buf, curRd) != curRd) throw WriteError ();
			srcStart += curRd;
			destStart += curRd;
		}
	}
	else if (srcStart < destStart)
	{
		FilePos destEnd = destStart + (srcEnd - srcStart);
		while (srcStart < srcEnd)
		{
			BufLen curRd = min_ (TRANSFER_SZ, srcEnd - srcStart);
			srcEnd -= curRd;
			destEnd -= curRd;
			if (file_.seek (srcEnd) != srcEnd) throw SeekError ();
			if (file_.read (transfer_buf, curRd) != curRd) throw ReadError ();
			if (file_.seek (destEnd) != destEnd) throw SeekError ();
			if (file_.write (transfer_buf, curRd) != curRd) throw WriteError ();
		}
	}
}

bool VStorage_imp::readRec (RecLocator locator, void* data, BufLen len, RecOff offset)
{
    // read header; this (usually) fetches first portion of record in cache
    BlockHdr blockhdr;
    read_bhdr_ (blockhdr, locator);
    // check free
    if (blockhdr.free_ == FREEFLAG)
        throw FreeBlockUsed ();
    // check boundaries 
    if (offset + len > datalen_ (blockhdr, locator))
        throw BadParameters ();
    // seek and read
    FilePos seekpos = file_.seek (locator + sizeof (BlockHdr) + offset);
    if (seekpos != locator + sizeof (BlockHdr) +  offset)
        throw BadParameters ();
    BufLen rdsize = file_.read (data, len);
    if (rdsize != len)
        throw BadParameters ();
    return true;
}

bool VStorage_imp::writeRec (RecLocator locator, const void* data, BufLen len, RecOff offset)
{
    // read header; this (usually) fetches first portion of record in cache
    BlockHdr blockhdr;
    read_bhdr_ (blockhdr, locator);
    // check free
    if (blockhdr.free_ == FREEFLAG)
        throw FreeBlockUsed ();
    // check boundaries 
    if (offset + len > datalen_ (blockhdr, locator))
        throw BadParameters ();
    // seek and write
    FilePos seekpos = file_.seek (locator + sizeof (BlockHdr) + offset);
    if (seekpos != locator + sizeof (BlockHdr) + offset)
        throw BadParameters ();
    BufLen wrsize = file_.write (data, len);
    if (wrsize != len)
        throw BadParameters ();
    return true;
}

bool VStorage_imp::freeRec (RecLocator locator)
{
    // read current block
    RecLocator curLocator = locator;
    BlockHdr curBlock;
    read_bhdr_ (curBlock, curLocator);
    if (curBlock.free_ == FREEFLAG)
        throw FreeBlockUsed ();
    RecLen curlen = datalen_ (curBlock, curLocator);
    
    // read 'prev' block
    RecLocator prevLocator = curBlock.prev_;
    BlockHdr prevBlock;
    if (prevLocator != RECLOCATOR_MAX)
        read_bhdr_ (prevBlock, prevLocator);

    // read 'next' block
    RecLocator nextLocator = curBlock.next_;
    BlockHdr nextBlock;
    if (nextLocator != RECLOCATOR_MAX)
        read_bhdr_ (nextBlock, nextLocator);

    if (prevLocator != RECLOCATOR_MAX && prevBlock.free_ == FREEFLAG && nextBlock.free_ == USEDFLAG) // if 'prev' is free and 'next' is used
    {
        // .........
        // prevBlock     FREE
        // curBlock      ***
        // nextBlock     USED
        // .........

        // determine 'prev' band
        RecLen oldlen = datalen_ (prevBlock, prevLocator);
        uint32 old_band = calc_band_ (oldlen);
        
        // join with 'current' (change length)
        join_bhdrs_ (curBlock.prev_, prevBlock, curBlock.next_, nextBlock); 
        
        // check weather new length falls into different band
        RecLen newlen = datalen_ (prevBlock, prevLocator);
        uint32 new_band = calc_band_ (newlen);
        if (old_band != new_band)
        {
            // remove 'prev' from old band and put into the end of new one
            free_remove_ (prevLocator, old_band, oldlen);
            free_add_ (prevLocator, new_band, newlen);
        }
        else 
        {
            if (old_band < hdr_.band_count_)// just adjust the space
            {
                bands_[old_band].space_ -= oldlen;
                bands_[old_band].space_ += newlen;
            }
            hdr_.free_space_ += (newlen - oldlen);
        }
    }
    else if (nextLocator != RECLOCATOR_MAX && (prevBlock.free_ == USEDFLAG || prevLocator == RECLOCATOR_MAX) && nextBlock.free_ == FREEFLAG) // if 'next' is free and 'prev' is used
    {
        // .........
        // prevBlock     USED of NONE
        // curBlock      ***
        // nextBlock     FREE (may be sentinel!)
        // nextNextBlock ????

        // load nextNext block
        RecLocator nextNextLocator = nextBlock.next_;
        if (nextNextLocator != RECLOCATOR_MAX) // not a sentinel
        {
            BlockHdr nextNextBlock;
            read_bhdr_ (nextNextBlock, nextNextLocator);

            // determine 'next' band
            RecLen oldlen = datalen_ (nextBlock, nextLocator);
            uint32 old_band = calc_band_ (oldlen);

            // join with 'current'
            curBlock.free_ = FREEFLAG;
            join_bhdrs_ (curLocator, curBlock, nextNextLocator, nextNextBlock);

            // determine new length and new band
            RecLen newlen = datalen_ (curBlock, curLocator);
            uint32 new_band = calc_band_ (newlen);
        
            // unconditionally remove / add into the free storage
            free_remove_ (nextLocator, old_band, oldlen);
            free_add_ (curLocator, new_band, newlen);
        }
        else // nextBlock is sentinel
        {
            // remember the length
            RecLen curlen = datalen_ (curBlock, curLocator);
            // write sentinel at the current position
            curBlock.free_ = FREEFLAG;
            curBlock.next_ = RECLOCATOR_MAX;
            write_bhdr_ (curBlock, curLocator);
            // update the header
            hdr_.last_off_ = curLocator;
        }
    }
    else if (prevLocator != RECLOCATOR_MAX && nextLocator != RECLOCATOR_MAX && prevBlock.free_ == FREEFLAG && nextBlock.free_ == FREEFLAG) // if both 'prev' and 'next' are free
    {
        // .........
        // prevBlock     FREE
        // curBlock      ***
        // nextBlock     FREE (may be sentinel!)
        // nextNextBlock ????

        // determine prev lengths
        RecLen prevlen = datalen_ (prevBlock, prevLocator);
        uint32 prev_band = calc_band_ (prevlen);

        // load nextNext block
        RecLocator nextNextLocator = nextBlock.next_;
        if (nextNextLocator != RECLOCATOR_MAX)
        {
            BlockHdr nextNextBlock;
            if (nextNextLocator != RECLOCATOR_MAX)
                read_bhdr_ (nextNextBlock, nextNextLocator);

            // determine next lengths
            RecLen nextlen = datalen_ (nextBlock, nextLocator);
            uint32 next_band = calc_band_ (nextlen);

            // join the prev and nextNext blocks
            join_bhdrs_ (prevLocator, prevBlock, nextNextLocator, nextNextBlock);

            // remove next block from freelist
            free_remove_ (nextLocator, next_band, nextlen);

            // check whether new band matches prev one
            RecLen newlen = datalen_ (prevBlock, prevLocator);
            uint32 new_band = calc_band_ (newlen);
            if (prev_band != new_band)
            {
                free_remove_ (prevLocator, prev_band, prevlen);
                free_add_ (prevLocator, new_band, newlen);
            }
            else 
            {
                if (prev_band < hdr_.band_count_)
                {
                    bands_[prev_band].space_ += newlen;
                    bands_[prev_band].space_ -= prevlen;
                }
                hdr_.free_space_ += newlen;
                hdr_.free_space_ -= prevlen;
            }
        }
        else // nextBlock is sentinel
        {
            // remember the cur len
            RecLen curlen = datalen_ (curBlock, curLocator);
            // remove prev from it's band 
            free_remove_ (prevLocator, prev_band, prevlen);
            // write sentinel at prev position
            prevBlock.free_ = FREEFLAG;
            prevBlock.next_ = RECLOCATOR_MAX;
            write_bhdr_ (prevBlock, prevLocator);
            // update the header
            hdr_.last_off_ = prevLocator;
        }
    }
    else // both 'prev' and 'next' are used
    {
        // USED or NONE
        // *
        // USED

        // determine band
        curBlock.free_ = FREEFLAG;
        write_bhdr_ (curBlock, curLocator);
        RecLen newlen = datalen_ (curBlock, curLocator);
        uint32 new_band = calc_band_ (newlen);
        // add to it if any
        free_add_ (curLocator, new_band, newlen);
    }

    hdr_.used_count_ --;
    hdr_.used_space_ -= curlen;
    
    return true;
}


void VStorage_imp::move_and_fill_ (RecLocator src_locator, RecLocator dest_locator, const void* data, BufLen data_len, RecOff offset, RecLen old_data_len, RecLen full_len)
{
	// move the portion before offset if any
	FilePos srcStart = src_locator + sizeof (BlockHdr);
	FilePos srcEnd   = src_locator + sizeof (BlockHdr) + offset;
	FilePos destStart = dest_locator + sizeof (BlockHdr);
	move_data_ (srcStart, srcEnd, destStart);
	
	// move the portion after the offset + dstLen, if any
	srcStart = src_locator + sizeof (BlockHdr) + offset + old_data_len;
	srcEnd = src_locator + sizeof (BlockHdr) + full_len;
	destStart = dest_locator + sizeof (BlockHdr) + offset + data_len;
	move_data_ (srcStart, srcEnd, destStart);

	// fill in the data if any
	if (data_len)
	{
		FilePos seekpos = file_.seek (dest_locator + sizeof (BlockHdr) + offset);
		if (seekpos != dest_locator + sizeof (BlockHdr) + offset) throw BadParameters ();
		BufLen wrsize = file_.write (data, data_len);
		if (wrsize != data_len) throw BadParameters ();
	}
}



RecLocator VStorage_imp::sliceRec (RecLocator locator, const void* data, BufLen data_len, RecOff offset, RecLen old_data_len, RecLen full_len)
{
	// get block at locator, get size
	BlockHdr block;
	read_bhdr_ (block, locator);
	RecLen blocklen = datalen_ (block, locator);

	if (blocklen < full_len) throw BadParameters ();
	if (old_data_len && offset + old_data_len > full_len) throw BadParameters ();
	
	RecLen newlen;
	if (offset > full_len)
		newlen = offset + data_len;
	else
		newlen = full_len + data_len - old_data_len;

	// here we need to 'expand' the block (if it is at the end or before the long enough free one) or realloc it.
	// read the next block
	BlockHdr nextBlock;
	read_bhdr_ (nextBlock, block.next_);

	// if last block
	if (nextBlock.next_ == RECLOCATOR_MAX)
	{
        if (newlen > full_len)
        {
		    resize_last_ (locator, block, nextBlock, newlen);
		    move_and_fill_ (locator, locator, data, data_len, offset, old_data_len, full_len);
        }
        else
        {
		    move_and_fill_ (locator, locator, data, data_len, offset, old_data_len, full_len);
		    resize_last_ (locator, block, nextBlock, newlen);
        }
		return locator;
	}
	// if followed by free block 
	else if (nextBlock.free_ == FREEFLAG)
	{
		RecLen nextLen = datalen_ (nextBlock, block.next_);
		RecLen unitedSpace = nextLen + blocklen + sizeof (BlockHdr);

		// if next (free) block is long enough - elongate using some free block's space
		if (newlen <= unitedSpace && newlen + sizeof (BlockHdr) + sizeof (FreePos) > unitedSpace) // no space for splitter - join blocks
		{
		    join_blocks_ (locator, block, nextBlock);
		    move_and_fill_ (locator, locator, data, data_len, offset, old_data_len, full_len);
			return locator;
		}
		else if (newlen + sizeof (BlockHdr) + sizeof (FreePos) <= unitedSpace) // enough space for splitter - move the boundary between blocks
		{
            if (newlen > full_len)
            {
    			resize_mid_ (locator, block, nextBlock, newlen);
	    		move_and_fill_ (locator, locator, data, data_len, offset, old_data_len, full_len);
            }
            else
            {
	    		move_and_fill_ (locator, locator, data, data_len, offset, old_data_len, full_len);
    			resize_mid_ (locator, block, nextBlock, newlen);
            }
			return locator;
		}
		else // if no way to elongate - allocate
		{
			RecLocator new_locator = allocRec (newlen); // allocate new block
			move_and_fill_ (locator, new_locator, data, data_len, offset, old_data_len, full_len);
			freeRec (locator); // free old block
			return new_locator; // return new locator
		}
	}
	else if (blocklen < newlen)
	{
		// new block is longer; mast move
		RecLocator new_locator = allocRec (newlen); // allocate new block
		move_and_fill_ (locator, new_locator, data, data_len, offset, old_data_len, full_len); // move data
		freeRec (locator); // free old block
		return new_locator; // return new locator
	}
	else if (newlen + sizeof (BlockHdr) + sizeof (FreePos) <= blocklen)
	{
		// shrink the block. The freing space is long enough - add to free space pool
		move_and_fill_ (locator, locator, data, data_len, offset, old_data_len, full_len); // move data
		split_used_ (locator, block, block.next_, nextBlock, newlen);
		// the old locator is valid
		return locator;
	}
	else // shrink and not enough space for freeing: leave as is
	{
		move_and_fill_ (locator, locator, data, data_len, offset, old_data_len, full_len); // move data
		return locator;
	}
}


void VStorage_imp::hintAdd (VRecDescriptor* descriptors, uint32 number)
{
}

void VStorage_imp::hintReset ()
{
}

RecNum VStorage_imp::getRecCount () const
{
    return hdr_.used_count_;
}

bool VStorage_imp::firstRec (VRecDescriptor& rec)
{
    if (!hdr_.used_count_) return false;

    VRec_impDescriptor descr;
    bool res = firstRec_ (descr);
    while (res && descr.free_ == FREEFLAG)
        res = nextRec_ (descr);
    if (!res) return false;
    rec.length_ = descr.length_;
    rec.locator_ = descr.locator_;
    return true;
}

bool VStorage_imp::nextRec (VRecDescriptor& rec)
{
    VRec_impDescriptor descr;
    descr.length_ = rec.length_;
    descr.locator_ = rec.locator_;
    bool res = nextRec_ (descr);
    while (res && descr.free_ == FREEFLAG)
        res = nextRec_ (descr);
    if (!res) return false;
    rec.length_ = descr.length_;
    rec.locator_ = descr.locator_;
    return true;
}


// storage - level operations 
bool VStorage_imp::flush ()
{
    // write control structures (header and bands)
    flush_hdr_ ();
    // flush the file
    return file_.commit ();
}

bool VStorage_imp::close ()
{
    // write control structures (header and bands)
    flush_hdr_ ();
    // cut file after the end
    file_.commit ();
    file_.chsize (hdr_.last_off_ + sizeof (BlockHdr));
    // close the file
    return file_.close ();
}

bool VStorage_imp::isOpen () const
{
    return file_.isOpen ();
}

uint64      VStorage_imp::usedSpace      () const
{
    return hdr_.used_space_;
}

uint64      VStorage_imp::freeSpace      () const
{
    return hdr_.free_space_;
}

uint64      VStorage_imp::usedCount      () const
{
    return hdr_.used_count_;
}

uint64      VStorage_imp::freeCount      () const
{
    return hdr_.free_count_;
}



VStorage& VStorageFactory_imp::wrap (File& file)
{
    VStorage_imp& storage = *new VStorage_imp (file);
    storage.open_ ();
    return storage;
}

VStorage& VStorageFactory_imp::init (File& file)
{
    VStorage_imp& storage = *new VStorage_imp (file);
    storage.init_ ();
    return storage;
}

bool VStorageFactory_imp::validate (File& file)
{
    VStorage_imp& storage = *new VStorage_imp (file);
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



}

