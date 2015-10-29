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

#include "edbFStorage.h"
#include "edbSplitFileFactory.h"
#include "edbCachedFileFactory.h"
#include "edbFStorageFactory.h"

#include <stdio.h>
#include <stdlib.h>
//#include <conio.h>
#include <iostream>
#include <iomanip>
#include "i64out.h"
#include <set>

namespace edb
{

static const char tstd [] = ".";
static const char tstn [] = "tstFS";
static const FRecLen reclen = 100;
static const FRecLen hdrlen = 400;
static const uint64 tstsize = 100000;

static const unsigned MAX_TEST_ITER = 5;



static bool naiveTest ()
{
    if (splitFileFactory.exists (tstd, tstn))
        splitFileFactory.erase (tstd, tstn);

    File& f = splitFileFactory.create (tstd, tstn);
    File& cf = cachedFileFactory.wrap (f);
    FStorage& fs = fStorageFactory.init (cf, reclen, hdrlen);

    char hdrline [] = "a eto prosto zagolovok";

    fs.writeHeader (hdrline, sizeof (hdrline));

    // printf ("File init, recno = %I64d, recsize = %d", fs.getRecCount (), fs.getRecLen ());
    std::cerr << "File init, recno = " << fs.getRecCount () << ", recsize = " << fs.getRecLen () << fs.getRecLen () << std::endl;
    char buf [reclen];
#if 0
    // printf ( "Writing %I64d records", tstsize);
    std::cerr << "Writing " <<  tstsize << " records" << std::endl;
    for (uint64 u = 0; u < tstsize; u ++)
    {
        *(uint64*) buf = u;
        fs.addRec (buf);
        if (u % 10000 == 0)
            // printf ("%I64u, totalrec = %I64d\r", u,  fs.getRecCount ());
            std::cerr << u << ", totalrec = " << fs.getRecCount () << "\r";
    }

    // printf ( "Reading %I64d records", tstsize);
    stdd::cerr << "Reading " << tstsize << " records" << std::endl;
    for (u = 0; u < tstsize; u ++)
    {
        fs.readRec (u, buf);
        if (*(uint64*) buf != u)
            ERR("Error");
        if (u % 10000 == 0)
            // printf ("%I64u, totalrec = %I64d\r", u,  fs.getRecCount ());
            std::cerr << u << ", totalrec = " << fs.getRecCount () << std::endl;
    }

    // printf ( "Deleting %I64d records", tstsize);
    std::cerr << "Deleting " << tstsize << " records" << std::endl;
    for (u = 0; u < tstsize; u ++)
    {
        fs.freeRec (u);
        if (u % 10000 == 0)
            // printf ("%I64u, totalrec = %I64d\r", u,  fs.getRecCount ());
            std::cerr << u << ", totalrec = " << fs.getRecCount () << "\r";
    }
#endif
    // printf ( "Random testing");
    std::cerr << "Random testing" << std::endl;
    unsigned test_iter = 0;
    while (test_iter ++ <= MAX_TEST_ITER)
    {
//        if (kbhit ())
//            if (getch () == 27)
//                break;

        int addno = rand () % tstsize + 1;
        int skipno = (rand () * rand ()) % tstsize + 1; 
        int delno = rand () + 1;

        if (delno > fs.getRecCount ())
            delno = fs.getRecCount ();

        if (skipno > fs.getRecCount () - delno)
            skipno = fs.getRecCount () - delno;

        // printf ( "Adding %d\r", addno);
        std::cerr << "Adding " << addno << "\r";
        for (int i = 0; i < addno; i ++)
        {
            RecNum rno = fs.allocRec ();
            *(uint64*) buf = rno;
            fs.writeRec (rno, buf);
        }

        int skip = 0;
        RecNum curno = 0;
        // printf ( "Skiping %d\r", skipno);
        std::cerr << "Skiping " << skipno << "\r";
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
        // printf ( "Deleting %d\r", delno);
        std::cerr << "Deleting " << delno << "\r";
        for (int d = 0; d < delno && curno + d < fs.getRecCount (); d ++)
            if (fs.isValid (curno + d))
                fs.freeRec (curno + d);

        // printf ( "Checking %I64d\r", fs.getRecCount () + fs.getFreeCount ());
        std::cerr << "Checking " << fs.getRecCount () + fs.getFreeCount () << "\r";
                
        for (RecNum rno = 0; rno < fs.getRecCount () + fs.getFreeCount (); rno ++)
            if (fs.isValid (rno))
            {
                fs.readRec (rno, buf);
                if (*(uint64*) buf != rno)
                    ERR("Error");
            }
            // printf ("\nused = %I64d, free = %I64d", fs.getRecCount (), fs.getFreeCount ());
            std::cerr << std::endl << "used = " << fs.getRecCount () << ", free = " << fs.getFreeCount () ;
    }

    fs.close ();
    return true;
}

bool testFStorage ()
{
	return naiveTest ();
}

};