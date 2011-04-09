// Copyright 2001 Victor Joukov, Denis Kaznadzey
//#define FILE_HANDLE_MGR_UNIT_TEST
//#define FILE_UNIT_TEST
//#define PAGER_UNIT_TEST
#define BTREE_UNIT_TEST
//#define PAGED_FILE_UNIT_TEST
//#define VSTORAGE_UNIT_TEST
//#define FSTORAGE_UNIT_TEST

#include <stdio.h>
#include "edbError.h"
#include "edbTests.h"

#ifdef BTREE_UNIT_TEST
#include "edbBTree.h"
#endif

main ()
{
    try
    {
#ifdef FILE_HANDLE_MGR_UNIT_TEST
    fprintf (stderr, "Testing FileHandleMgr...\n");
    edb::testFileHandleMgr ();
#endif
#ifdef FILE_UNIT_TEST
    fprintf (stderr, "Testing File...\n");
    edb::testFile ();
#endif
#ifdef VLPAGED_FILE_UNIT_TEST
    fprintf (stderr, "Testing VLPagedFile...\n");
    edb::testVLPagedFile ();
#endif
#ifdef PAGER_UNIT_TEST
    fprintf (stderr, "Testing Pager...\n");
    edb::testPager ();
#endif
#ifdef PAGED_FILE_UNIT_TEST
    fprintf (stderr, "Testing PagedFile...\n");
    edb::testPagedFile ();
#endif
#ifdef BTREE_UNIT_TEST
    fprintf (stderr, "Testing B-tree...\n");
    edb::testBTree();
#endif
#ifdef VSTORAGE_UNIT_TEST
    fprintf (stderr, "Testing Variable-length storage...\n");
    edb::testVStorage();
#endif
#ifdef FSTORAGE_UNIT_TEST
    fprintf (stderr, "Testing Fixed-length storage...\n");
    edb::testFStorage();
#endif
    }
    catch (edb::Error& e)
    {
        fprintf (stderr, "Error: %s", (const char*) e);
    }
    return 0;    
}