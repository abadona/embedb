// Copyright 2001 Victor Joukov, Denis Kaznadzey
#include "edbFStorage.h"
#include "edbSplitFileFactory.h"
#include "edbCachedFileFactory.h"
#include "edbFStorageFactory.h"

#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
//#include <iostream>
//#include <iomanip>
//#include "i64out.h"
#include <set>

namespace edb
{

static const char tstd [] = ".";
static const char tstn [] = "tstFS";
static const FRecLen reclen = 100;
static const FRecLen hdrlen = 400;
static const uint64 tstsize = 1000000;


static bool naiveTest ()
{
    if (splitFileFactory.exists (tstd, tstn))
        splitFileFactory.erase (tstd, tstn);

    File& f = splitFileFactory.create (tstd, tstn);
    File& cf = cachedFileFactory.wrap (f);
    FStorage& fs = fStorageFactory.init (cf, reclen, hdrlen);

    char hdrline [] = "a eto prosto zagolovok";

    fs.writeHeader (hdrline, sizeof (hdrline));

    printf ("File init, recno = %I64d, recsize = %d", fs.getRecCount (), fs.getRecLen ());
    char buf [reclen];
#if 0
    printf ( "Writing %I64d records", tstsize);
    for (uint64 u = 0; u < tstsize; u ++)
    {
        *(uint64*) buf = u;
        fs.addRec (buf);
        if (u % 10000 == 0)
            printf ("%I64u, totalrec = %I64d\r", u,  fs.getRecCount ());
    }

    printf ( "Reading %I64d records", tstsize);
    for (u = 0; u < tstsize; u ++)
    {
        fs.readRec (u, buf);
        if (*(uint64*) buf != u)
            ERR("Error");
        if (u % 10000 == 0)
            printf ("%I64u, totalrec = %I64d\r", u,  fs.getRecCount ());
    }

    printf ( "Deleting %I64d records", tstsize);
    for (u = 0; u < tstsize; u ++)
    {
        fs.freeRec (u);
        if (u % 10000 == 0)
            printf ("%I64u, totalrec = %I64d\r", u,  fs.getRecCount ());
    }
#endif
    printf ( "Random testing");
    while (1)
    {
        if (kbhit ())
            if (getch () == 27)
                break;

        int addno = rand () + 1;
        int skipno = (rand () * rand ()) + 1; 
        int delno = rand () + 1;

        if (delno > fs.getRecCount ())
            delno = fs.getRecCount ();

        if (skipno > fs.getRecCount () - delno)
            skipno = fs.getRecCount () - delno;

        printf ( "Adding %d\r", addno);
        for (int i = 0; i < addno; i ++)
        {
            RecNum rno = fs.allocRec ();
            *(uint64*) buf = rno;
            fs.writeRec (rno, buf);
        }

        int skip = 0;
        RecNum curno = 0;
        printf ( "Skiping %d\r", skipno);
        while (curno < fs.getRecCount ())
        {
            if (fs.isValid (curno))
            {
                skip ++;
                if (skip == skipno)
                    break;
            }
            curno ++;
        }
        printf ( "Deleting %d\r", delno);
        for (int d = 0; d < delno && curno + d < fs.getRecCount (); d ++)
            if (fs.isValid (curno + d))
                fs.freeRec (curno + d);

        printf ( "Checking %I64d\r", fs.getRecCount () + fs.getFreeCount ());
        for (RecNum rno = 0; rno < fs.getRecCount () + fs.getFreeCount (); rno ++)
            if (fs.isValid (rno))
            {
                fs.readRec (rno, buf);
                if (*(uint64*) buf != rno)
                    ERR("Error");
            }
            printf ("\nused = %I64d, free = %I64d", fs.getRecCount (), fs.getFreeCount ());
    }

    fs.close ();
    return true;
}

bool testFStorage ()
{
	return naiveTest ();
}

};