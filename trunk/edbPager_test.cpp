// Copyright 2001 Victor Joukov, Denis Kaznadzey
#include "edbPager.h"
#include "edbFile.h"

// #include "edbSimplePagerFactory.h"
#include "edbPagerFactory.h"

#include "edbSplitFileFactory.h"

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <iostream>
#include "i64out.h"
#include <string.h>

namespace edb
{

static const char* tdir   = ".";
static const char* tfile  = "pager_tst";
static const int32 pagesize = 0x1000; // 16 Kb
static const int32 poolsize = 10000; // 160 Mb of cache space

static bool naiveTest ()
{

    File& file = (splitFileFactory.exists (tdir, tfile))? splitFileFactory.open (tdir, tfile) : splitFileFactory.create (tdir, tfile);
    // Pager& pager = simplePagerFactory.create (pagesize, poolsize);
    Pager& pager = pagerFactory.create (pagesize, poolsize);

    uint64 pageno = poolsize * 2;  // 1280 Mb data file
    uint64 iterno = poolsize * 4; // 100,000 iterations
    // srand ((unsigned) clock ());

    FilePos inisize = pageno*pagesize;
    // printf ( "Changing size to %I64d KBytes\n", inisize/1024);
    std::cerr << "Changing size to " <<  inisize/1024 << " KBytes" << std::endl;
    pager.chsize (file, inisize);
    
    // printf ( "Filling the file, %I64d pages\n", pageno);
    std::cerr << "Filling the file, " << pageno << " pages" << std::endl;
    clock_t stt = clock ();
    uint32 i, previ = 0;
    for (i = 0; i < pageno; i ++)
    {
        char* buf = (char*) pager.fake (file, i);
        memset (buf, 0, pagesize);
        pager.mark (buf);
        if (i % 100 == 0) 
        {
            // printf ( "\r%I64d, %I64d items/sec, %I64d dumps, %I64d hits, %I64d misses.", i, ((i - previ) * CLOCKS_PER_SEC) / (clock () - stt + 1), pager.getDumpCount (), pager.getHitsCount (), pager.getMissesCount ());
            std::cerr << "\r" << i << ", " << ((i - previ) * CLOCKS_PER_SEC) / (clock () - stt + 1) << " items/sec, " << pager.getDumpCount () << " dumps, " << pager.getHitsCount () << " hits, " << pager.getMissesCount () << " misses.";
            previ = i;
            stt = clock ();
        }
    }


    // printf ("\nWriting data, %I64d elements", iterno);
    std::cerr << "\nWriting data, " << iterno << " elements" << std::endl;
    stt = clock ();
    previ = 0;
    for (i = 0; i < iterno; i ++)
    {
        uint64 p = (rand ()*rand ()) % pageno;
        int count = 1 + rand () % 3;
        char* buf = (char*) pager.fake (file, p, false, count);
        memset (buf, 0, pagesize*count);
        *(uint64*) (buf + (pagesize-sizeof (uint64))) = p;
        *buf = '*';
        pager.mark (buf);
        if (i % 100 == 0) 
        {
            // printf ( "\r%I64d, %I64d items/sec, %I64d dumps, %I64d hits, %I64d misses.", i, ((i - previ) * CLOCKS_PER_SEC) / (clock () - stt + 1), pager.getDumpCount (), pager.getHitsCount (), pager.getMissesCount ());
            std::cerr << "\r" << i << ", " << ((i - previ) * CLOCKS_PER_SEC) / (clock () - stt + 1) << " items/sec, " << pager.getDumpCount () << " dumps, " << pager.getHitsCount () << " hits, " << pager.getMissesCount () << " misses.";
            previ = i;
            stt = clock ();
        }
    }
    // printf ("\nReading data, %I64d elements\n", iterno);
    std::cerr << "\nReading data, " << iterno << " elements" << std::endl;
    stt = clock ();
    previ = 0;
    int repeats = 0;
    for (i = 0; i < iterno; i ++)
    {
        uint64 p = (rand ()*rand ()) % pageno;
        int count = 1 + rand () % 3;
        const char* d = (const char*) pager.fetch (file, p, false, count);
        if (*d == '*')
        {
            repeats ++;
            if (*(uint64*) (d + (pagesize-sizeof (uint64))) != p)
                // printf ( "ERROR ! POSITION MISS!");
                std::cerr << "ERROR ! POSITION MISS!" << std::endl;
        }
        if (i % 100 == 0) 
        {
            // printf ( "\r%I64d (%d repeats), %I64d items/sec, %I64d dumps, %I64d hits, %I64d misses.", i, repeats, ((i - previ) * CLOCKS_PER_SEC) / (clock () - stt + 1), pager.getDumpCount (), pager.getHitsCount (), pager.getMissesCount ());
            std::cerr << "\r" << i << " (" << repeats << " repeats), " << ((i - previ) * CLOCKS_PER_SEC) / (clock () - stt + 1) << " items/sec, " << pager.getDumpCount () << " dumps, " << pager.getHitsCount () << " hits, " << pager.getMissesCount () << " misses.";
            previ = i;
            stt = clock ();
        }
    }
    pager.close (file);
    return true;
}

bool chsizeTest ()
{
    File& file = (splitFileFactory.exists (tdir, tfile))? splitFileFactory.open (tdir, tfile) : splitFileFactory.create (tdir, tfile);
    // Pager& pager = simplePagerFactory.create (pagesize, poolsize);
    Pager& pager = pagerFactory.create (pagesize, poolsize);

    pager.chsize (file, 100000);
    char* b = (char*) pager.fetch (file, 1);
    *b = '*';
    pager.mark (b);
    b = (char*) pager.fetch (file, 0);
    *b = '#';
    pager.mark (b);
    pager.chsize (file, 0);

    pager.close (file);
    return true;
}

bool boundsTest ()
{
    const uint32 pagesize = 0x8;   // 8 bytes
    const uint32 poolsize = 0x2;   // 2 pages

    if (splitFileFactory.exists (tdir, tfile))
        splitFileFactory.erase (tdir, tfile);

    File& file = splitFileFactory.create (tdir, tfile);
    Pager& pager = pagerFactory.create (pagesize, poolsize);

    uint64 pgno;
    for (pgno = 0; pgno < 1000; pgno ++)
    {
        uint64* ptr = (uint64*) pager.fake (file, pgno);
        *ptr = pgno;
        pager.mark (ptr);
    }

    for (pgno = 0; pgno < 1000; pgno ++)
    {
        uint64* ptr = (uint64*) pager.fetch (file, pgno);
        if (*ptr != pgno)
            // printf ( "Print: OBSDACHA");
            std::cerr << "ERROR counting pages!" << std::endl;
    }



    pager.close (file);
    return true;
}

bool testPager ()
{
    // return boundsTest ();
    return naiveTest ();
    // return chsizeTest ();
}

};