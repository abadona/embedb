// Copyright 2001 Victor Joukov, Denis Kaznadzey
#include "edbSplitFileFactory.h"
#include "edbPagedFileFactory.h"
#include "edbThePagerMgr.h"

#include <stdio.h>
#include <string>
//#include <iostream>
#include <stdlib.h>
#include <time.h>
//#include "i64out.h"

namespace edb
{

#define TSTDIR "."
#define TSTNAME "tbf"



static bool naiveTest ()
{
    if (splitFileFactory.exists (TSTDIR, TSTNAME))
        splitFileFactory.erase (TSTDIR, TSTNAME);

    PagedFile& pf = pagedFileFactory.wrap (splitFileFactory.create (TSTDIR, TSTNAME));
    pf.setPageSize (16);

    char* tpl = "Kokostrawberry";
    int tplen = strlen (tpl);
    uint32 psize = pf.getPageSize ();

    
    char* s = new char [psize*2];
    char* t = s;
    while (t-s < psize*2-1)
    {
        *t = tpl [(t-s)%tplen];
        t ++;
    }
    s [psize*2-1] = 0; 

    char* buf = (char*) pf.fetch (1000, 2);
    memcpy (buf, s, strlen (s)+1);
    pf.mark (buf);

    char* rs = (char*) pf.fetch (1001);

    printf ( "Written to two pages: %s\n", s);
    printf ( "Read from one page: %s\n", rs);


    pf.close ();

    return true;
}

static bool volumeTest ()
{
    File* file;
    // erase first
    if (splitFileFactory.exists (TSTDIR, TSTNAME))
        splitFileFactory.erase (TSTDIR, TSTNAME);

    if (splitFileFactory.exists (TSTDIR, TSTNAME))
    {
        printf ( "Opening existing file: %s\\%s\n", TSTDIR, TSTNAME);
        file = &splitFileFactory.open (TSTDIR, TSTNAME);
    }
    else
    {
        printf ( "Creating new file: %s\\%s\n", TSTDIR, TSTNAME);
        file = &splitFileFactory.create (TSTDIR, TSTNAME);
    }

    PagedFile& pf = pagedFileFactory.wrap (*file);

#if 0
    uint32 npagesz = 0x10;
    uint32 npoolsz = 100;

    Pager& pgr = thePagerMgr ().getPager (npagesz, npoolsz);
    pf.setPageSize (npagesz);
    thePagerMgr ().releasePager (npagesz);
#endif

    printf ( "File length = %I64d bytes, page size = %d bytes\n", pf.length (), pf.getPageSize ());

    // test reading / writing 16-kb pages

    const FilePos pageno = 4000; // 100000; // 100000; // 100 thousand == 200 Mb 
    const FilePos iterno = 8000; // 200 thousand == 4200 Mb
    const uint32 ps = pf.getPageSize (); // 2000;  // 2 Kb - some pages will overlap boundary!
    const uint32 max_stretch_size = 2;

    // expand file
    FilePos iter;
    clock_t start = clock ();
    printf ( "Expanding file to %I64d bytes\n", pageno*ps);
    for (iter = 0; iter < pageno; iter ++)
    {
        char* buf = (char*) pf.fake (iter);
        memset (buf, 0xF, ps);
        pf.mark (buf);
        if (iter % 100 == 0) printf ( "\r%I64d (%I64d), %I64d  Kb/sec", iter, pageno, (iter*ps*CLOCKS_PER_SEC/1024)/(clock () - start + 1));
    }
    pf.flush ();

    // write pages in random order
    start = clock ();
    uint32 touchedcnt = 0;
    uint32 covercnt   = 0;
    uint32 maxp = 0;
    printf ("\nWriting %I64d pages\n", iterno);
    for (iter = 0; iter < iterno; iter ++)
    {
        FilePos pg = (((FilePos) rand ()) * rand ()) % pageno;
        uint32 count = rand () % max_stretch_size + 1;
        if (pg + count >= pageno)
            count = pageno - pg;
        char* buf = (char*) pf.fetch (pg, count);
        for (int pp = 0; pp < count; pp ++)
            if (*(buf+pp*ps) != 0xF)
                break;
        if (pp == count)
        {
            *buf = '*';
            for (int pp = 1; pp < count; pp ++) 
                *(buf+pp*ps) = '-';
            *(uint32*)  (buf+1) = count;
            *(FilePos*) (buf + count*ps - 8) = pg;
            pf.mark (buf);
            touchedcnt ++;
            covercnt += count;
            if (maxp < pg)
                maxp = pg;
        }
        if (iter % 100 == 0) 
            printf ( "\r%I64d, %I64d Kb/sec, %d ranges, %d pages, %d upb", iter, (iter*ps*CLOCKS_PER_SEC/1024)/(clock () - start + 1), touchedcnt, covercnt, maxp);
    }
    // read pages in random manner
    int misses = 0;
    int regets = 0;
    int hits = 0;
    start = clock ();
    printf ("\nReading %I64d pages", iterno);
    for (iter = 0; iter < iterno; iter ++)
    {
        FilePos pg = (((FilePos) rand ()) * rand ()) % pageno;

        char* buf = (char*) pf.fetch (pg);
        if (*buf != '*')
        {
            misses ++;
        }
        else
        {
            hits ++;
            uint32 count = *(uint32*) (buf + 1);
            if (count != 1)
            {
                regets ++;
                buf = (char*) pf.fetch (pg, count);
            }
            if (*(FilePos*) (buf + count*ps - 8) != pg)
                printf ( "ERROR: position miss!");
        }
        if (iter % 100 == 0) 
            printf ( "\r%I64d, %I64d Kb/sec, %d misses, %d hits, %d regets", iter, (iter*ps*CLOCKS_PER_SEC/1024)/(clock () - start + 1), misses, hits, regets);
    }

    pf.close ();
    return true;
}

bool sizeTest ()
{
    File* file;
    // erase first
    if (splitFileFactory.exists (TSTDIR, TSTNAME))
        splitFileFactory.erase (TSTDIR, TSTNAME);

    if (splitFileFactory.exists (TSTDIR, TSTNAME))
    {
        printf ( "Opening existing file: %s\\%s\n", TSTDIR, TSTNAME);
        file = &splitFileFactory.open (TSTDIR, TSTNAME);
    }
    else
    {
        printf ( "Creating new file: %s\\%s\n", TSTDIR, TSTNAME);
        file = &splitFileFactory.create (TSTDIR, TSTNAME);
    }

    PagedFile& pf = pagedFileFactory.wrap (*file);

    printf ( "File length = %I64d bytes, page size = %d bytes\n", pf.length (), pf.getPageSize ());

    printf ( "Changing size to 0\n");

    pf.chsize (0);

    printf ( "File length = %I64d bytes\n", pf.length ());

    printf ( "Faking page 10\n");

    void* buf = pf.fake  (10);

    printf ( "File length = %I64d bytes\n", pf.length ());

    printf ( "Fetching page 20\n");

    buf = pf.fetch  (20);

    printf ( "File length = %I64d bytes\n", pf.length ());

    printf ( "Marking page 20\n");

    *(char*)buf = '*';
    pf.mark (buf);

    printf ( "File length = %I64d bytes\n", pf.length ());

    printf ( "Committing page 20\n");

    pf.flush ();

    printf ( "File length = %I64d bytes\n", pf.length ());

    printf ( "Fetching page 100\n");

    pf.fetch (100);

    printf ( "File length = %I64d bytes\n", pf.length ());

    printf ( "Closing file\n");

    pf.close ();
    if (pf.isOpen ()) printf ( "ERROR: File open after close!");

    printf ( "re-opening\n");

    file = &splitFileFactory.open (TSTDIR, TSTNAME);
    PagedFile& pf1 = pagedFileFactory.wrap (*file);

    printf ( "File length = %I64d bytes, page size = %d bytes\n", pf1.length (), pf1.getPageSize ());

    printf ( "Changing size to 55\n");

    pf1.chsize (55);

    printf ( "File length = %I64d bytes\n", pf1.length ());

    printf ( "Closing file\n");

    pf1.close ();

    printf ( "re-opening\n");

    file = &splitFileFactory.open (TSTDIR, TSTNAME);
    PagedFile& pf2 = pagedFileFactory.wrap (*file);

    printf ( "File length = %I64d bytes\n", pf2.length ());
    pf2.close ();

    return true;
}

bool testPagedFile ()
{
    //return naiveTest ();
    return volumeTest ();
    //return sizeTest ();
}

};