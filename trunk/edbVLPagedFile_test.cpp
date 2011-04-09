#include "edbVLPagedFileFactory.h"
#include "edbVLPagedCacheFactory.h"
#include "edbSimplePagerFactory.h"
#include <string>
#include <iostream>
#include <stdlib.h>
#include <time.h>
#include "i64out.h"

namespace edb
{

#define TSTDIR "."
#define TSTNAME "tbf"

const int32 pagesize = 0x10; // 0x4000; // 16 Kb
const int32 poolsize = 100; // 10000; // 160 Mb cache
const int32 varsize  = 0x1000; // 4 Kb long buffers cache 

static bool naiveTest ()
{
    if (pagedFileFactory.exists (TSTDIR, TSTNAME))
        pagedFileFactory.erase (TSTDIR, TSTNAME);

    Pager& pager = simplePagerFactory.create (pagesize, poolsize);
    VLPagedCache& cache = pagedCacheFactory.create (pager, varsize);
    VLPagedFile& bf = pagedFileFactory.create (TSTDIR, TSTNAME, cache);

    char* s = "Kokos and banan and strawberry";
    
    char* buf = (char*) bf.fetch (1000, 100);
    memcpy (buf, s, strlen (s));
    bf.mark (buf);

    char* rs = (char*) bf.fetch (1010, strlen (s));

    std::cout << "Read : " << rs << std::endl;


    bf.close ();

    return true;
}

static bool volumeTest ()
{
    Pager& pager = simplePagerFactory.create (pagesize, poolsize);
    VLPagedCache& cache = pagedCacheFactory.create (pager, varsize);
    VLPagedFile* b;
    if (pagedFileFactory.exists (TSTDIR, TSTNAME))
    {
        b = &pagedFileFactory.open (TSTDIR, TSTNAME, cache);
        std::cout << "Pre-existing file used. The misses may occure due to pre-existing data" << std::endl;
    }
    else
        b = &pagedFileFactory.create (TSTDIR, TSTNAME, cache);
    VLPagedFile& bf = *b;

    // test reading / writing 16-kb pages

    const FilePos pageno = 100; // 10000; // 100000; // 100000; // 100 thousand == 200 Mb 
    const FilePos iterno = 2000; // 200000; // 200 thousand == 4200 Mb
    const int ps = 64; // 2000;  // 2 Kb - some pages will overlap boundary!

    // expand file
    std::cout << "Expanding file to " << pageno*pagesize << " bytes" << std::endl;
    for (FilePos iter = 0; iter < pageno; iter ++)
    {
        FilePos off = iter * ps;
        char* buf = (char*) bf.fake (off, ps);
        memset (buf, 0, ps);
        bf.mark (buf);
        if (iter % 100 == 0) std::cout << "\r" << iter << "(" << pageno << "), " << (iter*ps*CLOCKS_PER_SEC/1024)/(clock () + 1) << " Kb/sec";
    }
    bf.flush ();
    // bf.chsize (pageno*ps);

    int32 boff = ps-10;

    // write pages in random order
    clock_t start = clock ();
    std::cout << std::endl << "Writing " << iterno << " pages" << std::endl;
    for (iter = 0; iter < iterno; iter ++)
    {
        FilePos pg = (((FilePos) rand ()) * rand ()) % pageno;
        FilePos off = pg * ps;
        char* buf = (char*) bf.fake (off, ps);
        memset (buf, 0, ps);
        *buf = '*';
        *(FilePos*) (buf + boff) = pg;
        bf.mark (buf);
        if (iter % 100 == 0) std::cout << "\r" << iter << ", " << (iter*ps*CLOCKS_PER_SEC/1024)/(clock () - start + 1) << " Kb/sec";
    }
    // read pages in random manner
    int misses = 0;
    start = clock ();
    std::cout << std::endl << "Reading " << iterno << " pages" << std::endl;
    for (iter = 0; iter < iterno; iter ++)
    {
        FilePos pg = (((FilePos) rand ()) * rand ()) % pageno;
        FilePos off = pg * ps;
        char* buf = (char*) bf.fetch (off, ps);
        if (*buf != '*')
        {
            *buf = '*';
            *(FilePos*) (buf + boff) = pg;
            bf.mark (buf);
            misses ++;
        }
        else
        {
            FilePos written = written = *(FilePos*) (buf + boff);
            if (written != pg)
                std::cout << "ERROR: position miss!" << std::endl;
        }
        if (iter % 100 == 0) std::cout << "\r" << iter << ", " << (iter*ps*CLOCKS_PER_SEC/1024)/(clock () - start + 1) << " Kb/sec, " << misses << " misses";
    }

    bf.close ();
    return true;
}

bool testVLPagedFile ()
{
    //// return naiveTest ();
    return volumeTest ();
}

};