// Copyright 2001 Victor Joukov, Denis Kaznadzey
#include "edbFileHandleMgr.h"

// *********************
// test function

#include <stdlib.h>
#include <stdio.h>
#include <io.h>
//#include <iostream>
#include <string>
#include <time.h>
#include <vector>
//#include "i64out.h"

#define FILENO 100
#define POOLSIZE 98

namespace edb
{

static void naiveTest ()
{
    const int ITERNO = 100000;

    fileHandleMgr.setPoolSize (POOLSIZE);

    printf ("Creating files\n");
    
    char buf [100];
    Fid  fids [FILENO];

    for (int i = 0; i < FILENO; i ++)
    {
        sprintf (buf, "TF%d", i);
        fids [i] = fileHandleMgr.create (buf);
        if (fids [i] == -1)
            printf ( "Unable to create file : %s\n", buf);
    }

    printf ( "Writing\n");
    for (i = 0; i < ITERNO; i ++)
    {
        int fileno = rand () % FILENO;
        int h = fileHandleMgr.handle (fids [fileno]);
        if (h == -1)
            printf ( "Could not get handle for : %s\n", buf);
        sprintf (buf, "TF%d - %d\n", fileno, i);
        write (h, buf, strlen (buf));

        if (i % 1000 == 0)
            printf ("\r%d - %I64d", i, fileHandleMgr.getDropCount ());
    }

    printf ("\nClosing\n");
    for (i = 0; i < FILENO; i ++)
    {
        if (!fileHandleMgr.close (fids [i]))
            printf ( "Unable to close file : %d\n", fids [i]);
    }
}

static void speedTest ()
{
    const int ITERNO = 10000000;
    const int poolsize = 500;
    fileHandleMgr.setPoolSize (poolsize);
    char buf [100];
    Fid fids [poolsize];
    int i;

    for (i = 0; i < poolsize; i ++)
    {
        sprintf (buf, "test.%d.tst", i);
        // open file
        fids [i] = fileHandleMgr.create (buf);
        if (fids [i] == -1)
        {
            printf ( "Unable to create file : %s\n", buf);
            return;
        }

        // get handle - make sure file is open
        fileHandleMgr.handle (fids [i]);
    }
    time_t t = time (NULL);
    for (i = 0; i < ITERNO; i ++)
    {
        int fno = rand () % poolsize;
        fileHandleMgr.handle (fids [fno]);

        if (i % 100000 == 0)
            printf ( "\r%d, %I64d\n", i, fileHandleMgr.getDropCount ());
    }
    int speed = ITERNO / (time (NULL) - t + 1);
    printf ("\nSpeed is %d handles per second\n", speed);


}

bool testFileHandleMgr ()
{
    // naiveTest ();
    speedTest ();
    
    printf ("Done\n");

    return true;
}

};