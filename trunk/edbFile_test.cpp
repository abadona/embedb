// Copyright 2001 Victor Joukov, Denis Kaznadzey
#include "edbFile.h"
#include "edbSplitFileFactory.h"
#include "edbCachedFileFactory.h"
#include "edbSystemCache.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
//#include "i64out.h"


#include "edbCachedFile_imp.h"
#include "edbPagedFile_imp.h"


namespace edb
{

#define TESTNAME "test"
#define TESTDIR  "."


static bool naiveTest ()
{
    const char* ts  = "Kokos!!!\nBanan!!!A eto kto takoi?\nDa eto zhe ANANAS!\n";
    const char* ts2 = "Prihodite begemoti, poedaite apelsini!\nA ogromniy gamadril pust' s'edaet mandain!\n";
    char buf [500];

    if (splitFileFactory.exists (TESTDIR, TESTNAME))
    {
        // printf ( "Test file exists\n");
        std::cerr << "Test file exists" << std::endl;
        if (splitFileFactory.erase (TESTDIR, TESTNAME))
            //printf ( "Test file erased\n");
            std::cerr << "Test file erased" << std::endl;
        else
            // printf ( "Unable to erase!\n");
            std::cerr << "Unable to erase!" << std::endl;
    }

    File& sf = splitFileFactory.create (TESTDIR, TESTNAME);
    //printf ( "File created, size = %I64d", sf.length ());
    std::cerr << "File created, size = " << sf.length () << std::endl;
    CachedFile& f = cachedFileFactory.wrap (sf);

    int32 wl = f.write (ts, strlen (ts));
    // printf ( "Data written, size = I64d", f.length ());
    std::cerr << "Data written, size = " << f.length () << std::endl;

    FilePos p = f.seek (0);
    // printf ( "Seek to beginning returned %I64d\n", p);
    std::cerr << "Seek to beginning returned " << p << std::endl;

    int32 rl = f.read (buf, 100);
    buf [rl] = 0;
    // printf ( "Data read, datasize = %I64d :\n%s\n", rl, buf);
    std::cerr << "Data read, datasize = " << rl << " :\"" << buf << "\"" << std::endl;

    if (!f.close ())
        // printf ( "Could not close\n");
        std::cerr << "Could not close" << std::endl;
    else
        // printf ( "Close Ok\n");
        std::cerr << "Close Ok" << std::endl;

    File& sf1 = splitFileFactory.open (TESTDIR, TESTNAME);
    // printf ( "File opened again, length = %I64d\n", sf1.length ());
    std::cerr << "File opened again, length = " << sf1.length () << std::endl;
    CachedFile& f1 = cachedFileFactory.wrap (sf1);

    rl = f1.read (buf, 20);
    buf [rl] = 0;
    // printf ( "20 bytes of data read, rdsize = %d :\n%s\n", rl, buf);
    std::cerr << "20 bytes of data read, rdsize = " << rl << " :" << buf << std::endl;

    FilePos flen = f1.length ();
    p = f1.seek (flen);
    // printf ( "Seek to length (%I64d) returned %I64d\n", flen, p);
    std::cerr << "Seek to length (" << flen << ") returned " << p << std::endl;

    wl = f1.write (ts2, strlen (ts2));
    // printf ( "Data written, size = %d\n", f1.length ());
    std::cerr << "Data written, size = " << f1.length () << std::endl;

    p = f1.seek (flen);
    // printf ( "Seek to (%I64d) returned %I64d\n", flen, p);
    std::cerr << "Seek to (" << flen << ") returned " << p << std::endl;

    rl = f1.read (buf, 150);
    buf [rl] = 0;
    // printf ( "Data read, datasize = %d :\n%s\n", rl, buf);
    std::cerr << "Data read, datasize = " << rl << ": \"" << buf << "\"" << std::endl;

    p = f1.seek (0);
    // printf ( "Seek to 0 returned %I64d\n", p);
    std::cerr << "Seek to 0 returned " << p << std::endl;

    rl = f1.read (buf, 200);
    buf [rl] = 0;
    // printf ( "Data read, datasize = %d :\n%s\n", rl, buf);
    std::cerr << "Data read, datasize = " << rl << ": \"" << buf << "\"" << std::endl;

    p = f1.chsize (30);
    // printf ( "Change size to 30, returned %I64d, len = %I64d\n", p, f1.length ());
    std::cerr << "Change size to 30, returned " << p << ", len = " << f1.length () << std::endl;

    p = f1.seek (0);
    // printf ( "Seek to 0, returned %I64d, length = %I64d\n", p, f1.length ());
    std::cerr << "Seek to 0, returned " << p << ", length = " << f1.length () << std::endl;

    rl = f1.read (buf, 100);
    buf [rl] = 0;
    // printf ( "Read, returned %d :\n%s\n", rl, buf);
    std::cerr << "Read, returned " << rl << ": \"" << buf << "\"" << std::endl;


    p = f1.chsize (200);
    // printf ( "Change size to 200, returned %I64d, len = %I64d\n", p, f1.length ());
    std::cerr << "Change size to 200, returned " << p << ", len = " << f1.length () << std::endl;

    p = f1.seek (10);
    // printf ( "Seek to 10, returned %I64d, length = %I64d\n", p, f1.length ());
    std::cerr << "Seek to 10, returned " << p << ", length = " << f1.length () << std::endl;

    rl = f1.read (buf, 200);
    buf [rl] = 0;
    // printf ( "Read, returned %d:\n%d\n", rl, buf);
    std::cerr << "Read, returned " << rl << ": \"" << buf << "\"" << std::endl;

    p = f1.seek (500);
    // printf ( "Seek to 500, returned %d, len = %I64d\n", p, f1.length ());
    std::cerr << "Seek to 500, returned " << p << ", len = " << f1.length () << std::endl;

    wl = f1.write (ts2, strlen (ts2));
    // printf ( "Write, returned %d, len = %I64d\n", p, f1.length ());
    std::cerr << "Write, returned " << p << ", len = " << f1.length () << std::endl;

    p = f1.seek (500);
    // printf ( "Seek to 500, returned %d, len = %I64d\n", p, f1.length ());
    std::cerr << "Seek to 500, returned " << p << ", len = " << f1.length () << std::endl;

    rl = f1.read (buf, 200);
    buf [rl] = 0;
    // printf ( "Read 200 bytes, returned %d, len = %I64d, contents:\n%s\n", rl, f1.length (), buf);
    std::cerr << "Read 200 bytes, returned " << rl << ", len = " << ": \"" << f1.length () << ", content : \"" << buf << "\"" << std::endl;

    if (!f1.close ())
        // printf ( "Could not close\n");
        std::cerr << "Could not close" << std::endl;
    else
        // printf ( "Close Ok\n");
        std::cerr << "Close Ok" << std::endl;

    return true;
}

static bool sizeTest ()
{
    FileFactory& factory = splitFileFactory;

    const char* tn = "test";
    const char* td = ".";
    FilePos ts = 0x200000010; // 8 Gb and 16 bytes

    File& f = factory.create (td, tn);
    // printf ( "Changing size to 8Gb\n");
    std::cerr << "Changing size to " << ts / (1024 * 1024) << "Mb" << std::endl;
    f.chsize (ts);
    // printf ( "Done, new size is %I64d\n", f.length ());
    std::cerr << "Done, new size is " << f.length () << std::endl;
    std::cerr << "Closing and re-opening file" << std::endl;
    f.close ();
    File& f1 = factory.open (td, tn);
    // printf ( "File open, size is %I64d\n", f1.length ());
    std::cerr << "File open, size is " << f1.length () << std::endl;
    // printf ( "Changing size to 0 bytes\n");
    std::cerr << "Changing size to 0 bytes" << std::endl;
    f1.chsize (0);
    // printf ( "Done, new size is %I64d\n", f1.length ());
    std::cerr << "Done, new size is " << f.length () << std::endl;
    f1.close ();

    return true;

}

static bool repeatTest ()
{
    if (splitFileFactory.exists (TESTDIR, TESTNAME))
        splitFileFactory.erase (TESTDIR, TESTNAME);

    systemCache().setPageSize (0x1000); // 4 Kb

    File& sf = splitFileFactory.create (TESTDIR, TESTNAME);
    // printf ( "File created, size = %I64d\n", sf.length ());
    std::cerr << "File created, size = " << sf.length () << std::endl;
    CachedFile& f = cachedFileFactory.wrap (sf);

    uint32 iters = 10000;
    uint64 items = 0x400000; 
    uint64 nzero = 0xFFFFFFFFFFFFFFFFL;

    uint32 item;

    // fill in 'items' 'nzero'es at the beginning of a file
    for (item = 0; item < items; item ++)
    {
        FilePos off = item*sizeof (uint64);
        f.seek (off);
        f.write (&nzero, sizeof (uint64));
        if (item % 1000000 == 0)
            // printf ( "\rFilling item %d of %I64d", item, items);
            std::cerr << "\rFilling item " << item << " of " << items;
    }
    // printf ("\n");
    std::cerr << "\r" << item << " items filled in                     " << std::endl;
// #if 0
    uint32 iter;
    for (iter = 0; iter < iters; iter ++)
    {
        uint64 no = ((uint64)rand ())*rand ()%items;
        FilePos off = no*sizeof (uint64);
        f.seek (off);
        f.write (&no, sizeof (uint64));
        if (iter % 1000 == 0)
            // printf ( "\rRand Writing item %d of %d", iter, iters);
            std::cerr << "\rRand Writing item " << iter << " of " << iters;
    }
    // printf ("\n");
    std::cerr << "\r" << iter << " random numbers are written at positions 0 - " << iter << std::endl;
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
                std::cerr << "ERROR!" << std::endl;
        }
        if (iter % 1000 == 0)
            // printf ( "\rRand Reading item %d of %d, hits %d", iter, iters, hits);
            std::cerr << "\rRand Reading item " << iter << " of " << iters << ", hits " << hits << std::endl; 
    }
// #endif
    // printf ("\n");
    std::cerr << std::endl;

    f.close ();
    if (sf.isOpen ()) 
        // printf ( "File not closed");
        std::cerr << "File not closed" << std::endl;
    // cleanup
    splitFileFactory.erase (TESTDIR, TESTNAME);
    return true;
}

bool testFile ()
{
    std::cerr << "Running file tests" << std::endl;
    return repeatTest ();
    return naiveTest ();
    return sizeTest ();
    std::cerr << "File tests complete." << std::endl;
}

};