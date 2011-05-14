// Copyright 2001 Victor Joukov, Denis Kaznadzey
#include "edbFileHandleMgr.h"

// *********************
// test function

#include <stdlib.h>
#include <stdio.h>
//#include <io.h>
#include <iostream>
#include <string>
#include <time.h>
#include <vector>
#include <string.h>
#include "i64out.h"

#ifdef __MSC_VER
#include <io.h>
#endif
#include "portability.h"

#define FILENO 100
#define POOLSIZE 98

namespace edb
{

static void naiveTest ()
{
    const int ITERNO = 100000;

    fileHandleMgr.setPoolSize (POOLSIZE);

    std::cerr << "Creating files" << std::endl;

    char buf [100];
    Fid  fids [FILENO];

    int i;
    for (i = 0; i < FILENO; i ++)
    {
        sprintf (buf, "TF%d", i);
        fids [i] = fileHandleMgr.create (buf);
        if (fids [i] == -1)
            std::cerr << "Unable to create file : " <<  buf << std::endl;
    }

    std::cerr <<  "Writing" << std::endl;
    for (i = 0; i < ITERNO; i ++)
    {
        int fileno = rand () % FILENO;
        int h = fileHandleMgr.handle (fids [fileno]);
        if (h == -1)
            std::cerr << "Could not get handle for : " << buf << std::endl;
        sprintf (buf, "TF%d - %d\n", fileno, i);
        ::sci_write (h, buf, strlen (buf));

        if (i % 1000 == 0)
            std::cerr << "\r" << i << " - " << fileHandleMgr.getDropCount ();
    }

    std::cerr << "\nClosing" << std::endl;
    for (i = 0; i < FILENO; i ++)
    {
        if (!fileHandleMgr.close (fids [i]))
            std::cerr << "Unable to close file : " << fids [i] << std::endl;
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
            std::cerr << "Unable to create file : " << buf << std::endl;
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
            // printf ( "\r%d, %I64d\n", i, fileHandleMgr.getDropCount ());
            std::cerr << "\r" << i << ", " << fileHandleMgr.getDropCount ();
    }
    int speed = ITERNO / (time (NULL) - t + 1);
    std::cerr << "\nSpeed is " <<  speed << " handles per second" << std::endl;


}

bool testFileHandleMgr ()
{
    // naiveTest ();
    speedTest ();
    
    std::cerr << "Done" << std::endl;

    return true;
}

};