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

#include "edbSplitFileFactory.h"
#include "edbPagedFileFactory.h"
#include "edbThePagerMgr.h"

#include <stdio.h>
#include <string>
#include <iostream>
#include <stdlib.h>
#include <time.h>
#include "i64out.h"
#include <string.h>

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

    const char* tpl = "Kokostrawberry";
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

    // printf ( "Written to two pages: %s\n", s);
    std::cerr << "Written to two pages: " <<  s << std::endl;
    // printf ( "Read from one page: %s\n", rs);
    std::cerr << "Read from one page: " << rs << std::endl;

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
        // printf ( "Opening existing file: %s\\%s\n", TSTDIR, TSTNAME);
        std::cerr << "Opening existing file: " << TSTDIR << "\\" << TSTNAME << std::endl;
        file = &splitFileFactory.open (TSTDIR, TSTNAME);
    }
    else
    {
        // printf ( "Creating new file: %s\\%s\n", TSTDIR, TSTNAME);
        std::cerr << "Creating new file: " << TSTDIR << "\\" << TSTNAME << std::endl;
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

    // printf ( "File length = %I64d bytes, page size = %d bytes\n", pf.length (), pf.getPageSize ());
    std::cerr << "File length = " << pf.length ();

    // test reading / writing 16-kb pages

    const FilePos pageno = 4000; // 100000; // 100000; // 100 thousand == 200 Mb 
    const FilePos iterno = 8000; // 200 thousand == 4200 Mb
    const uint32 ps = pf.getPageSize (); // 2000;  // 2 Kb - some pages will overlap boundary!
    const uint32 max_stretch_size = 2;

    // expand file
    FilePos iter;
    clock_t start = clock ();
    // printf ( "Expanding file to %I64d bytes\n", pageno*ps);
    std::cerr << "Expanding file to " << pageno*ps <<  " bytes" << std::endl;
    for (iter = 0; iter < pageno; iter ++)
    {
        char* buf = (char*) pf.fake (iter);
        memset (buf, 0xF, ps);
        pf.mark (buf);
        if (iter % 100 == 0) 
            // printf ( "\r%I64d (%I64d), %I64d  Kb/sec", iter, pageno, (iter*ps*CLOCKS_PER_SEC/1024)/(clock () - start + 1));
            std::cerr << "\r" << iter << " (" << pageno << ") " << (iter*ps*CLOCKS_PER_SEC/1024)/(clock () - start + 1) << " Kb/sec";
    }
    pf.flush ();

    // write pages in random order
    start = clock ();
    uint32 touchedcnt = 0;
    uint32 covercnt   = 0;
    uint32 maxp = 0;
    // printf ("\nWriting %I64d pages\n", iterno);
    std::cerr << "\nWriting " << iterno << " pages" << std::endl;
    for (iter = 0; iter < iterno; iter ++)
    {
        FilePos pg = (((FilePos) rand ()) * rand ()) % pageno;
        uint32 count = rand () % max_stretch_size + 1;
        if (pg + count >= pageno)
            count = pageno - pg;
        char* buf = (char*) pf.fetch (pg, count);
        int pp;
        for (pp = 0; pp < count; pp ++)
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
            // printf ( "\r%I64d, %I64d Kb/sec, %d ranges, %d pages, %d upb", iter, (iter*ps*CLOCKS_PER_SEC/1024)/(clock () - start + 1), touchedcnt, covercnt, maxp);
            std::cerr << "\r" << iter << ", " << (iter*ps*CLOCKS_PER_SEC/1024)/(clock () - start + 1) << " Kb/sec, " << touchedcnt << " ranges, " << covercnt << "pages ," << maxp << " upb";
    }
    // read pages in random manner
    int misses = 0;
    int regets = 0;
    int hits = 0;
    start = clock ();
    // printf ("\nReading %I64d pages", iterno);
    std::cerr << "\nReading " << iterno << " pages" << std::endl;
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
                // printf ( "ERROR: position miss!");
                std::cerr << "ERROR: position miss!" <<std::endl;
        }
        if (iter % 100 == 0) 
            // printf ( "\r%I64d, %I64d Kb/sec, %d misses, %d hits, %d regets", iter, (iter*ps*CLOCKS_PER_SEC/1024)/(clock () - start + 1), misses, hits, regets);
            std::cerr << "\r" << iter << ", " << (iter*ps*CLOCKS_PER_SEC/1024)/(clock () - start + 1) << " Kb/sec, " << misses << " misses, " << hits << " hits, " << regets << " regets";
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
        // printf ( "Opening existing file: %s\\%s\n", TSTDIR, TSTNAME);
        std::cerr << "Opening existing file: " << TSTDIR << "\\" << TSTNAME << std::endl;
        file = &splitFileFactory.open (TSTDIR, TSTNAME);
    }
    else
    {
        // printf ( "Creating new file: %s\\%s\n", TSTDIR, TSTNAME);
        std::cerr << "Creating new file: " << TSTDIR << "\\" << TSTNAME << std::endl;
        file = &splitFileFactory.create (TSTDIR, TSTNAME);
    }

    PagedFile& pf = pagedFileFactory.wrap (*file);

    // printf ( "File length = %I64d bytes, page size = %d bytes\n", pf.length (), pf.getPageSize ());
    std::cerr << "File length = " << pf.length () << " bytes, page size = " << pf.getPageSize () << " bytes" << std::endl;

    // printf ( "Changing size to 0\n");
    std::cerr << "Changing size to 0" << std::endl;

    pf.chsize (0);

    // printf ( "File length = %I64d bytes\n", pf.length ());
    std::cerr << "File length = " << pf.length () << " bytes" << std::endl;

    // printf ( "Faking page 10\n");
    std::cerr << "Faking page 10" << std::endl;

    void* buf = pf.fake  (10);

    // printf ( "File length = %I64d bytes\n", pf.length ());
    std::cerr << "File length = " << pf.length () << " bytes" << std::endl;

    // printf ( "Fetching page 20\n");
    std::cerr << "Fetching page 20" << std::endl;

    buf = pf.fetch  (20);

    // printf ( "File length = %I64d bytes\n", pf.length ());
    std::cerr << "File length = " << pf.length () << " bytes" << std::endl;

    // printf ( "Marking page 20\n");
    std::cerr << "Marking  page 20" << std::endl;

    *(char*)buf = '*';
    pf.mark (buf);

    // printf ( "File length = %I64d bytes\n", pf.length ());
    std::cerr << "File length = " << pf.length () << " bytes" << std::endl;

    // printf ( "Committing page 20\n");
    std::cerr << "Committing page 20" << std::endl;

    pf.flush ();

    // printf ( "File length = %I64d bytes\n", pf.length ());
    std::cerr << "File length = " << pf.length () << " bytes" << std::endl;

    // printf ( "Fetching page 100\n");
    std::cerr << "Fetching page 100" << std::endl;

    pf.fetch (100);

    //printf ( "File length = %I64d bytes\n", pf.length ());
    std::cerr << "File length = " << pf.length () << " bytes" << std::endl;

    // printf ( "Closing file\n");
    std::cerr << "Closing file" << std::endl;

    pf.close ();
    if (pf.isOpen ()) 
        // printf ( "ERROR: File open after close!");
        std::cerr << "ERROR: File open after close!" << std::endl;
    
    // printf ( "re-opening\n");
    std::cerr << "re-opening" << std::endl;

    file = &splitFileFactory.open (TSTDIR, TSTNAME);
    PagedFile& pf1 = pagedFileFactory.wrap (*file);

    // printf ( "File length = %I64d bytes, page size = %d bytes\n", pf1.length (), pf1.getPageSize ());
    std::cerr << "File length = " << pf1.length () << " bytes, page size = " << pf1.getPageSize () << std::endl;

    // printf ( "Changing size to 55\n");
    std::cerr << "Changing size to 55" << std::endl;

    pf1.chsize (55);

    std::cerr << "File length = " << pf1.length () << " bytes" << std::endl;

    // printf ( "Closing file\n");
    std::cerr << "Closing file" << std::endl;

    pf1.close ();

    // printf ( "re-opening\n");
    std::cerr << "re-opening" << std::endl;

    file = &splitFileFactory.open (TSTDIR, TSTNAME);
    PagedFile& pf2 = pagedFileFactory.wrap (*file);

    // printf ( "File length = %I64d bytes\n", pf2.length ());
    std::cerr << "File length = " << pf1.length () << " bytes" << std::endl;

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
