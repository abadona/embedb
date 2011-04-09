#include "edbBufferedFileFactory.h"
#include <string>
#include <iostream>
#include <stdlib.h>
#include <time.h>
#include "i64out.h"

namespace edb
{

#define TSTDIR "."
#define TSTNAME "tbf"

bool naiveTest ()
{
    if (bufferedFileFactory.exists (TSTDIR, TSTNAME))
        bufferedFileFactory.erase (TSTDIR, TSTNAME);

    BufferedFile& bf = bufferedFileFactory.create (TSTDIR, TSTNAME);

    char* s = "Kokos and banan and strawberry";
    
    bf.write (s, 1000, strlen (s));

    const char* rs = (const char*) bf.get (1010, strlen (s));

    std::cout << "Read : " << rs << std::endl;


    bf.close ();

    return true;
}

bool volumeTest ()
{
    BufferedFile* b;
    if (bufferedFileFactory.exists (TSTDIR, TSTNAME))
        b = &bufferedFileFactory.open (TSTDIR, TSTNAME);
    else
        b = &bufferedFileFactory.create (TSTDIR, TSTNAME);
    BufferedFile& bf = *b;

    // test reading / writing 16-kb pages

    const FilePos pageno = 200000;  // 200 thousand == 3.2Gb
    const FilePos iterno = 10000; // 10 thousand == 160 Mb
    const int pagesize = 0x4000; // 16 Kb
    char buf [pagesize];

    // expand file
    std::cout << "Expanding file to " << pageno*pagesize/1024 << " Kb" << std::endl;
    bf.get (pageno*pagesize, 10);


    // write pages in random order
    clock_t start = clock ();
    std::cout << "Writing " << iterno << " pages" << std::endl;
    for (FilePos iter = 0; iter < iterno; iter ++)
    {
        FilePos pg = (((FilePos) rand ()) * rand ()) % pageno;
        FilePos off = pg * pagesize;
        memcpy (buf, &pg, sizeof (pg));
        bf.write (buf, off, pagesize);
        if (iter % 100 == 0) std::cout << "\r" << iter << ", " << (iter*pagesize*CLOCKS_PER_SEC/1024)/(clock () - start + 1) << " Kb/sec";
    }
    // read pages in random manner
    int misses = 0;
    start = clock ();
    std::cout << "Reading " << iterno << " pages" << std::endl;
    for (iter = 0; iter < iterno; iter ++)
    {
        FilePos pg = (((FilePos) rand ()) * rand ()) % pageno;
        FilePos off = pg * pagesize;
        const void* ptr = bf.get (off, pagesize);
        if (*(FilePos*) ptr != pg)
        {
            memcpy (buf, &pg, sizeof (pg));
            bf.write (buf, off, pagesize);
            misses ++;
        }
        if (iter % 100 == 0) std::cout << "\r" << iter << ", " << (iter*pagesize*CLOCKS_PER_SEC/1024)/(clock () - start + 1) << " Kb/sec, " << misses << " misses";
    }

    bf.close ();
    return true;
}

bool testBufferedFile ()
{
    // return naiveTest ();
    return volumeTest ();
}

};