// Copyright 2001 Victor Joukov, Denis Kaznadzey
#include "edbVStorage.h"
#include "edbSplitFileFactory.h"
#include "edbCachedFileFactory.h"
#include "edbVStorageFactory.h"

#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
//#include <iostream>
//#include <iomanip>
//#include "i64out.h"
#include <set>




namespace edb
{

static const uint32 SMALLRECMAX = 100;
static const uint32 MIDRECMAX = 10000;
static const uint32 LARGERECMAX = 1000000;

static const uint32 SMALLPREF = 50;
static const uint32 MIDPREF = 100;

static const uint32 MXROW = 20000;
static const uint32 REPORTINT = 100000;

static const char tstd [] = ".";
static const char tstn [] = "tstVS";

RecLen randrecsz ()
{
    RecLen ret;
    bool largeormid = ((rand () % SMALLPREF) == 0);
    if (largeormid)
    {
        bool large = ((rand () % MIDPREF) == 0);
        if (large)
            ret = (rand () * rand ())%LARGERECMAX + 1;
        else
            ret = (rand () * rand ())%MIDRECMAX + 1;
    }
    else
        ret = (rand () * rand ())%SMALLRECMAX + 1;
    return ret;
}

static bool opTest ()
{
    File* fp;
    VStorage* vsp;
    if (splitFileFactory.exists (tstd, tstn))
    {
        fp = &splitFileFactory.open (tstd, tstn);
        File& cf = cachedFileFactory.wrap (*fp);
        vsp = &vStorageFactory.wrap (cf);
        printf ("Existing file opened:\n");
    }
    else
    {
        fp = &splitFileFactory.create (tstd, tstn);
        File& cf = cachedFileFactory.wrap (*fp);
        vsp = &vStorageFactory.init (cf);
        printf ("New file created:\n");
    }
    VStorage& vs = *vsp;

    printf ("usedCount : %9I64d, usedSpace : %9I64d, freeCount : %9I64d, freeSpace %9I64d\n", vs.usedCount (), vs.usedSpace (), vs.freeCount (), vs.freeSpace ());
    // std::cout << "usedCount : " << std::setw (9) << vs.usedCount () << ", usedSpace : " << std::setw (9) << vs.usedSpace () << ", freeCount : " << std::setw (9) << vs.freeCount () << ", freeSpace : " << std::setw (9) << vs.freeSpace () << "\n";

    char* buf = new char [LARGERECMAX + 2*sizeof (RecLocator) + sizeof (RecLen)];
    std::set <RecLocator> rls;

    __int64 totallocs = 0;
    __int64 totfrees = 0;
    __int64 totreads = 0;
    __int64 totreallocs = 0;

    RecLocator dels [MXROW];

    while (1)
    {
        if (kbhit ())
            if (getch () == 27)
                break;
        
        int allocno = rand () % MXROW+1;
        int delno = rand () % MXROW+1;
        int readno = rand () % MXROW+1;
        int reallocno = rand () % MXROW+1;

        // allocate some
        for (int u = 0; u < allocno; u ++)
        {
            RecLen rlen = 2 * sizeof (RecLocator) + sizeof (RecLen) + randrecsz ();
            RecLocator a = vs.allocRec (rlen);
            totallocs ++;
            if (totallocs % REPORTINT == 0)
            {
                printf ("N Alloc : ");
                printf ("%I64d, Frees: %I64d, Reads: %I64d, Reallocs: %I64d, ", totallocs, totfrees, totreads, totreallocs);
                printf ("usedCount : %9I64d, usedSpace : %9I64d, freeCount : %9I64d, freeSpace %9I64d\n", vs.usedCount (), vs.usedSpace (), vs.freeCount (), vs.freeSpace ());
                // std::cout << "N Alloc : " << std::setw (9) << totallocs << ", Frees : " << std::setw (9) << totfrees << ", Reads : " << std::setw (9) << totreads << ", Reallocs : " << std::setw (9) << totreallocs << ", usedCount : " << std::setw (9) << vs.usedCount () << ", usedSpace : " << std::setw (9) << vs.usedSpace () << ", freeCount : " << std::setw (9) << vs.freeCount () << ", freeSpace : " << std::setw (9) << vs.freeSpace () << "\n";
            }
            *(RecLocator*) buf = a;
            *(RecLen*) (buf + sizeof (RecLocator)) = rlen;
            *(RecLocator*) (buf + (rlen - sizeof (RecLocator))) = a;

            vs.writeRec (a, buf, rlen);
            rls.insert (a);
        }
//#if 0      
        // delete some
        std::set <RecLocator>::iterator itr = rls.begin ();
        int skipcnt = (rls.size () < delno) ? 0 : ((rand () * rand ()) % (rls.size () - delno + 1));
        int delcnt = 0;
        while (itr != rls.end () && skipcnt > 0)
        {
                itr ++;
                skipcnt --;
        }
        while (itr != rls.end () && delcnt < delno)
        {
                dels [delcnt ++] = (*itr);
                itr ++;
        }
        for (int d = 0; d < delcnt; d ++)
        {
            vs.freeRec (dels [d]);
            rls.erase (dels [d]);
            totfrees ++;
            if (totfrees % REPORTINT == 0)
            {
                printf ("N Free : ");
                printf ("%I64d, Frees: %I64d, Reads: %I64d, Reallocs: %I64d, ", totallocs, totfrees, totreads, totreallocs);
                printf ("usedCount : %9I64d, usedSpace : %9I64d, freeCount : %9I64d, freeSpace %9I64d\n", vs.usedCount (), vs.usedSpace (), vs.freeCount (), vs.freeSpace ());
            }
        }

        // reallocate some
        itr = rls.begin ();
        delcnt = 0;
        skipcnt = (rls.size () < reallocno) ? 0 : ((rand () * rand ()) % (rls.size () - reallocno + 1));
        while (itr != rls.end () && skipcnt > 0)
        {
            itr ++;
            skipcnt --;
        }
        while (itr != rls.end () && delcnt < reallocno)
        {
            dels [delcnt ++] = (*itr);
            itr ++;
        }
        for (d = 0; d < delcnt; d ++)
        {
            RecLen newLen = 2 * sizeof (RecLocator) + sizeof (RecLen) + randrecsz ();
            RecLocator rl = dels [d];
            RecLocator newLocator = vs.reallocRec (rl, newLen);
            
            rls.erase (rl);
            rls.insert (newLocator);
            totreallocs ++;
            if (totreallocs % REPORTINT == 0)
            {
                printf ("N Realloc : ");
                printf ("%I64d, Frees: %I64d, Reads: %I64d, Reallocs: %I64d, ", totallocs, totfrees, totreads, totreallocs);
                printf ("usedCount : %9I64d, usedSpace : %9I64d, freeCount : %9I64d, freeSpace %9I64d\n", vs.usedCount (), vs.usedSpace (), vs.freeCount (), vs.freeSpace ());
            }

            *(RecLocator*) buf = newLocator;
            *(RecLen*) (buf + sizeof (RecLocator)) = newLen;
            *(RecLocator*) (buf + (newLen - sizeof (RecLocator))) = newLocator;

            vs.writeRec (newLocator, buf, newLen);

        }


        // check some
        itr = rls.begin ();
        skipcnt = (rls.size () < readno) ? 0 : ((rand () * rand ()) % (rls.size () - readno + 1));
        while (itr != rls.end () && skipcnt > 0)
        {
            itr ++;
            skipcnt --;
        }
        while (itr != rls.end () && readno > 0)
        {
            RecLocator cp = *itr;
            RecLen reallen = vs.getRecLen (cp);

            // RecLen rlen = vs.getRecLen (cp);
            vs.readRec (cp, buf, reallen);
            if (*(RecLocator*) buf != cp)
                ERR ("Error!");
            RecLen rlen = (*(RecLen*) (buf + sizeof (RecLocator)));
            if (rlen > reallen)
                ERR ("Error!");
            if (*(RecLocator*) (buf + (rlen - sizeof (RecLocator))) != cp)
                ERR ("Error!");
            itr ++;
            readno --;

            totreads ++;
            if (totreads % REPORTINT == 0)
            {
                printf ("N Read : ");
                printf ("%I64d, Frees: %I64d, Reads: %I64d, Reallocs: %I64d, ", totallocs, totfrees, totreads, totreallocs);
                printf ("usedCount : %9I64d, usedSpace : %9I64d, freeCount : %9I64d, freeSpace %9I64d\n", vs.usedCount (), vs.usedSpace (), vs.freeCount (), vs.freeSpace ());
            }
        }
// #endif
    }
    vs.close ();
	return true;
}

#define ALSZ 1000
#define INCR1 100
#define INCR2 1940
static bool reallocTest ()
{
    if (splitFileFactory.exists (tstd, tstn))
        splitFileFactory.erase (tstd, tstn);

    File& f = splitFileFactory.create (tstd, tstn);
    File& cf = cachedFileFactory.wrap (f);
    VStorage& vs = vStorageFactory.init (cf);

    RecLocator rl1 = vs.allocRec (ALSZ);
    RecLocator rl2 = vs.allocRec (ALSZ);
    RecLocator rl3 = vs.allocRec (ALSZ);
    RecLocator rl4 = vs.allocRec (ALSZ);

    vs.freeRec (rl2);

    RecLocator nrl1 = vs.reallocRec (rl1, ALSZ+INCR1);

    //std::cout << "Reallocated 1 (locator "<<rl1<<" ,size "<<ALSZ<<") to size "<<ALSZ+INCR1<<", newLocator "<<nrl1<<std::endl;
    //std::cout <<     "usedCount : " << std::setw (9) << vs.usedCount () << ", usedSpace : " << std::setw (9) << vs.usedSpace () << ", freeCount : " << std::setw (9) << vs.freeCount () << ", freeSpace : " << std::setw (9) << vs.freeSpace () << std::endl;

    RecLocator nrl4 = vs.reallocRec (rl4, ALSZ+INCR1);
    //std::cout << "Reallocated 4 (locator "<<rl4<<" ,size "<<ALSZ<<") to size "<<ALSZ+INCR1<<", newLocator "<<nrl4<<std::endl;
    //std::cout <<     "usedCount : " << std::setw (9) << vs.usedCount () << ", usedSpace : " << std::setw (9) << vs.usedSpace () << ", freeCount : " << std::setw (9) << vs.freeCount () << ", freeSpace : " << std::setw (9) << vs.freeSpace () << std::endl;

    RecLocator nrl3 = vs.reallocRec (rl3, ALSZ+INCR1);
    //std::cout << "Reallocated 3 (locator "<<rl3<<" ,size "<<ALSZ<<") to size "<<ALSZ+INCR1<<", newLocator "<<nrl3<<std::endl;
    //std::cout <<     "usedCount : " << std::setw (9) << vs.usedCount () << ", usedSpace : " << std::setw (9) << vs.usedSpace () << ", freeCount : " << std::setw (9) << vs.freeCount () << ", freeSpace : " << std::setw (9) << vs.freeSpace () << std::endl;

    RecLocator nnrl1 = vs.reallocRec (nrl1, ALSZ+INCR1+INCR2);
    //std::cout << "Reallocated 1 (locator "<<nrl1<<" ,size "<<ALSZ+INCR1<<") to size "<<ALSZ+INCR1+INCR2<<", newLocator "<<nnrl1<<std::endl;
    //std::cout <<     "usedCount : " << std::setw (9) << vs.usedCount () << ", usedSpace : " << std::setw (9) << vs.usedSpace () << ", freeCount : " << std::setw (9) << vs.freeCount () << ", freeSpace : " << std::setw (9) << vs.freeSpace () << std::endl;

    RecLocator nnrl12 = vs.reallocRec (nrl1, ALSZ);


    //std::cout << "Closing file: ";
    //std::cout << "usedCount : " << std::setw (9) << vs.usedCount () << ", usedSpace : " << std::setw (9) << vs.usedSpace () << ", freeCount : " << std::setw (9) << vs.freeCount () << ", freeSpace : " << std::setw (9) << vs.freeSpace () << "\n";
    vs.close ();
	return true;
}

bool casesTest ()
{
    if (splitFileFactory.exists (tstd, tstn))
        splitFileFactory.erase (tstd, tstn);

    File& f = splitFileFactory.create (tstd, tstn);
    File& cf = cachedFileFactory.wrap (f);
    VStorage& vs = vStorageFactory.init (cf);

    // test allocation
    RecLocator rl1 = vs.allocRec (ALSZ);
    //std::cout << "Allocated 1 (locator "<<rl1<<" ,size "<<ALSZ<<std::endl;
    //std::cout <<     "usedCount : " << std::setw (9) << vs.usedCount () << ", usedSpace : " << std::setw (9) << vs.usedSpace () << ", freeCount : " << std::setw (9) << vs.freeCount () << ", freeSpace : " << std::setw (9) << vs.freeSpace () << std::endl;

    vs.freeRec (rl1);
    //std::cout << "Freed 1 (locator "<<rl1<<" ,size "<<ALSZ<<std::endl;
    //std::cout <<     "usedCount : " << std::setw (9) << vs.usedCount () << ", usedSpace : " << std::setw (9) << vs.usedSpace () << ", freeCount : " << std::setw (9) << vs.freeCount () << ", freeSpace : " << std::setw (9) << vs.freeSpace () << std::endl;


    vs.close ();
    return true;
}

bool readTest ()
{
    File* fp;
    VStorage* vsp;
    if (!splitFileFactory.exists (tstd, tstn))
    {
        printf ("No file found. Nothing to check.\n");
    }
    else
    {
        fp = &splitFileFactory.open (tstd, tstn);
        File& cf = cachedFileFactory.wrap (*fp);
        vsp = &vStorageFactory.wrap (cf);
        printf ("Existing file opened:\n");
    }
    VStorage& vs = *vsp;
    printf ("usedCount : %9I64d, usedSpace : %9I64d, freeCount : %9I64d, freeSpace %9I64d\n", vs.usedCount (), vs.usedSpace (), vs.freeCount (), vs.freeSpace ());


    VRecDescriptor vrd;
    uint64 currno = 0;
    if (vs.firstRec (vrd)) do
    {
        RecLocator cloc;
        RecLen clen;
        
        vs.readRec (vrd.locator_, &cloc, sizeof (RecLocator));
        if (cloc != vrd.locator_) 
            ERR("First Locator");
        vs.readRec (vrd.locator_, &clen, sizeof (RecLen), sizeof (RecLocator));
        vs.readRec (vrd.locator_, &cloc, sizeof (RecLocator), clen - sizeof (RecLocator));
        if (cloc != vrd.locator_) 
            ERR("Second Locator");
        if (currno % REPORTINT == 0)
            printf ("%I64d\r", currno);
        currno ++;
    }
    while (vs.nextRec (vrd));

    return true;
}

bool delTest ()
{
    File* fp;
    VStorage* vsp;
    if (!splitFileFactory.exists (tstd, tstn))
    {
        printf ("No file found. Nothing to check.\n");
    }
    else
    {
        fp = &splitFileFactory.open (tstd, tstn);
        File& cf = cachedFileFactory.wrap (*fp);
        vsp = &vStorageFactory.wrap (cf);
        printf ("Existing file opened:\n");
    }
    VStorage& vs = *vsp;
    printf ("usedCount : %9I64d, usedSpace : %9I64d, freeCount : %9I64d, freeSpace %9I64d\n", vs.usedCount (), vs.usedSpace (), vs.freeCount (), vs.freeSpace ());

    std::set <RecLocator> locset;

    VRecDescriptor vrd;
    uint64 currno = 0;
    if (vs.firstRec (vrd)) do
    {
        locset.insert (vrd.locator_);
        if (currno % REPORTINT == 0)
            printf ("Marked for delete %I64d\r", currno);
        currno ++;
    }
    while (vs.nextRec (vrd));

    printf ("\n");
    currno = 0;
    std::set<RecLocator>::iterator itr = locset.begin ();
    while (itr != locset.end ())
    {
        vs.freeRec (*itr);
        if (currno % REPORTINT == 0)
            printf ("Deleted %I64d\r", currno);
        currno ++;
        itr ++;
    }

    vs.close ();
    return true;
}

bool allocTest ()
{
    File* fp;
    VStorage* vsp;
    if (splitFileFactory.exists (tstd, tstn))
    {
        fp = &splitFileFactory.open (tstd, tstn);
        File& cf = cachedFileFactory.wrap (*fp);
        vsp = &vStorageFactory.wrap (cf);
        printf ("Existing file opened:\n");
    }
    else
    {
        fp = &splitFileFactory.create (tstd, tstn);
        File& cf = cachedFileFactory.wrap (*fp);
        vsp = &vStorageFactory.init (cf);
        printf ("New file created:\n");
    }
    VStorage& vs = *vsp;

    printf ("usedCount : %9I64d, usedSpace : %9I64d, freeCount : %9I64d, freeSpace %9I64d\n", vs.usedCount (), vs.usedSpace (), vs.freeCount (), vs.freeSpace ());

    int totallocs = 0;
    while (1)
    {
        if (kbhit ())
            if (getch () == 27)
                break;
        
        int allocno = 100;

        // allocate some
        for (int u = 0; u < allocno; u ++)
        {
            RecLen rlen = 2 * sizeof (RecLocator) + sizeof (RecLen) + randrecsz ();
            RecLocator a = vs.allocRec (rlen);
            totallocs ++;
            if (totallocs % REPORTINT == 0)
                printf ("Alloc : %I64d\r", totallocs);

            vs.writeRec (a, &a, sizeof (a));
            vs.writeRec (a, &rlen, sizeof (rlen), sizeof (a));
            vs.writeRec (a, &a, sizeof (a), rlen - sizeof (RecLocator));
        }
    }
    printf ("usedCount : %9I64d, usedSpace : %9I64d, freeCount : %9I64d, freeSpace %9I64d\n", vs.usedCount (), vs.usedSpace (), vs.freeCount (), vs.freeSpace ());
    vs.close ();
    return true;
}

bool testVStorage ()
{
    //return casesTest ();
    //return reallocTest ();
    opTest ();
    // readTest ();
    // delTest ();
    // allocTest ();
    return true;
}

};