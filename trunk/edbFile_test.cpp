// Copyright 2001 Victor Joukov, Denis Kaznadzey
#include "edbFile.h"
#include "edbSplitFileFactory.h"
#include "edbCachedFileFactory.h"
#include "edbSystemCache.h"
#include <string.h>
#include <stdio.h>
//#include <iostream>
//#include "i64out.h"


namespace edb
{

#define TESTNAME "test"
#define TESTDIR  "."


static bool naiveTest ()
{
    char* ts  = "Kokos!!!\nBanan!!!A eto kto takoi?\nDa eto zhe ANANAS!\n";
    char* ts2 = "Prihodite begemoti, poedaite apelsini!\nA ogromniy gamadril pust' s'edaet mandain!\n";
    char buf [500];

    if (splitFileFactory.exists (TESTDIR, TESTNAME))
    {
        printf ( "Test file exists\n");
        if (splitFileFactory.erase (TESTDIR, TESTNAME))
            printf ( "Test file erased\n");
        else
            printf ( "Unable to erase!\n");
    }
    
    File& sf = splitFileFactory.create (TESTDIR, TESTNAME);
    printf ( "File created, size = %I64d", sf.length ());
    CachedFile& f = cachedFileFactory.wrap (sf);
    
    int32 wl = f.write (ts, strlen (ts));
    printf ( "Data written, size = I64d", f.length ());

    FilePos p = f.seek (0);
    printf ( "Seek to beginning returned %I64d\n", p);
    
    int32 rl = f.read (buf, 100);
    buf [rl] = 0;
    printf ( "Data read, datasize = %I64d :\n%s\n", rl, buf);

    if (!f.close ())
        printf ( "Could not close\n");
    else
        printf ( "Close Ok\n");
    
    File& sf1 = splitFileFactory.open (TESTDIR, TESTNAME);
    printf ( "File opened again, length = %I64d\n", sf1.length ());
    CachedFile& f1 = cachedFileFactory.wrap (sf1);

    rl = f1.read (buf, 20);
    buf [rl] = 0;
    printf ( "20 bytes of data read, rdsize = %d :\n%s\n", rl, buf);

    FilePos flen = f1.length ();
    p = f1.seek (flen);
    printf ( "Seek to length (%I64d) returned %I64d\n", flen, p);

    wl = f1.write (ts2, strlen (ts2));
    printf ( "Data written, size = %d\n", f1.length ());

    p = f1.seek (flen);
    printf ( "Seek to (%I64d) returned %I64d\n", flen, p);

    rl = f1.read (buf, 150);
    buf [rl] = 0;
    printf ( "Data read, datasize = %d :\n%s\n", rl, buf);

    p = f1.seek (0);
    printf ( "Seek to 0 returned %I64d\n", p);

    rl = f1.read (buf, 200);
    buf [rl] = 0;
    printf ( "Data read, datasize = %d :\n%s\n", rl, buf);

    p = f1.chsize (30);
    printf ( "Change size to 30, returned %I64d, len = %I64d\n", p, f1.length ());

    p = f1.seek (0);
    printf ( "Seek to 0, returned %I64d, length = %I64d\n", p, f1.length ());

    rl = f1.read (buf, 100);
    buf [rl] = 0;
    printf ( "Read, returned %d :\n%s\n", rl, buf);


    p = f1.chsize (200);
    printf ( "Change size to 200, returned %I64d, len = %I64d\n", p, f1.length ());

    p = f1.seek (10);
    printf ( "Seek to 10, returned %I64d, length = %I64d\n", p, f1.length ());

    rl = f1.read (buf, 200);
    buf [rl] = 0;
    printf ( "Read, returned %d:\n%d\n", rl, buf);

    p = f1.seek (500);
    printf ( "Seek to 500, returned %d, len = %I64d\n", p, f1.length ());

    wl = f1.write (ts2, strlen (ts2));
    printf ( "Write, returned %d, len = %I64d\n", p, f1.length ());

    p = f1.seek (500);
    printf ( "Seek to 500, returned %d, len = %I64d\n", p, f1.length ());

    rl = f1.read (buf, 200);
    buf [rl] = 0;
    printf ( "Read 200 bytes, returned %d, len = %I64d, contents:\n%s\n", rl, f1.length (), buf);

    if (!f1.close ())
        printf ( "Could not close\n");
    else
        printf ( "Close Ok\n");


    return true;
}

static bool sizeTest ()
{
    FileFactory& factory = splitFileFactory;

    const char* tn = "test";
    const char* td = ".";
    FilePos ts = 0x200000010; // 8 Gb and 16 bytes

    File& f = factory.create (td, tn);
    printf ( "Changing size to 8Gb\n");
    f.chsize (ts);
    printf ( "Done, new size is %I64d\n", f.length ());
    f.close ();

    File& f1 = factory.open (td, tn);
    printf ( "File open, size is %I64d\n", f1.length ());
    printf ( "Changing size to 0 bytes\n");
    f1.chsize (0);
    printf ( "Done, new size is %I64d\n", f1.length ());
    f1.close ();


    return true;

}

static bool repeatTest ()
{
    if (splitFileFactory.exists (TESTDIR, TESTNAME))
        splitFileFactory.erase (TESTDIR, TESTNAME);

    systemCache().setPageSize (0x1000); // 4 Kb
    
    File& sf = splitFileFactory.create (TESTDIR, TESTNAME);
    printf ( "File created, size = %I64d\n", sf.length ());
    CachedFile& f = cachedFileFactory.wrap (sf);


    uint32 iters = 10000;
    uint64 items = 0x400000; 
    uint64 nzero = 0xFFFFFFFFFFFFFFFFL;

    for (uint32 item = 0; item < items; item ++)
    {
        FilePos off = item*sizeof (uint64);
        f.seek (off);
        f.write (&nzero, sizeof (uint64));
        if (item % 1000000 == 0)
            printf ( "\rFilling item %d of %I64d", item, items);
    }
    printf ("\n");
    for (uint32 iter = 0; iter < iters; iter ++)
    {
        uint64 no = ((uint64)rand ())*rand ()%items;
        FilePos off = no*sizeof (uint64);
        f.seek (off);
        f.write (&no, sizeof (uint64));
        if (iter % 1000 == 0)
            printf ( "\rRand Writing item %d of %d", iter, iters);
    }
    printf ("\n");
    uint32 hits = 0;
    for (iter = 0; iter < iters; iter ++)
    {
        uint64 no = ((uint64)rand ())*rand ()%items;
        uint64 res;
        FilePos off = no*sizeof (uint64);
        f.seek (off);
        f.read (&res, sizeof (uint64));
        if (res != nzero)
        {
            hits ++;
            if (res != no) 
                printf ( "Error!");
        }
        if (iter % 1000 == 0)
            printf ( "\rRand Reading item %d of %d, hits %d", iter, iters, hits);
    }
    printf ("\n");

    f.close ();
    if (sf.isOpen ()) printf ( "File not closed");
    return true;
}   

bool testFile ()
{
    return repeatTest ();
    // return naiveTest ();
    // return sizeTest ();
}

};