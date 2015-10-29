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

#define BTREE_UNIT_TEST
//#define PAGED_FILE_UNIT_TEST
//#define VSTORAGE_UNIT_TEST
//#define FSTORAGE_UNIT_TEST

#include "edbError.h"
#include "edbTests.h"
#include <iostream>

#ifdef BTREE_UNIT_TEST
#include "edbBTree.h"
#endif

main ()
{
    std::cerr << "edb unittest collection" << std::endl;
    try
    {
#ifdef FILE_HANDLE_MGR_UNIT_TEST
    std::cerr << "Testing FileHandleMgr..." << std::endl;
    edb::testFileHandleMgr ();
    std::cerr << "Done with testing FileHandleMgr." << std::endl;
#endif
#ifdef FILE_UNIT_TEST
    std::cerr << "Testing File..." << std::endl;
    edb::testFile ();
    std::cerr << "Done with testing File." << std::endl;
#endif
#ifdef VLPAGED_FILE_UNIT_TEST
    std::cerr << "Testing VLPagedFile..." << std::endl;
    edb::testVLPagedFile ();
    std::cerr << "Done with testing VLPagedFile." << std::endl;
#endif
#ifdef PAGER_UNIT_TEST
    std::cerr << "Testing Pager..." << std::endl;
    edb::testPager ();
    std::cerr << "Done with testing Pager." << std::endl;
#endif
#ifdef PAGED_FILE_UNIT_TEST
    std::cerr << "Testing PagedFile..." << std::endl;
    edb::testPagedFile ();
    std::cerr << "Done with testing PagedFile." << std::endl;
#endif
#ifdef BTREE_UNIT_TEST
    std::cerr << "Testing B-tree..." << std::endl;
    edb::testBTree();
    std::cerr << "Done with testing B-tree." << std::endl;
#endif
#ifdef VSTORAGE_UNIT_TEST
    std::cerr << "Testing Variable-length storage..." << std::endl;
    edb::testVStorage();
    std::cerr << "Done with testing Variable-length storage." << std::endl;
#endif
#ifdef FSTORAGE_UNIT_TEST
    std::cerr << "Testing Fixed-length storage..." << std::endl;
    edb::testFStorage();
    std::cerr << "Done with testing Fixed-length storage..." << std::endl;
#endif
    }
    catch (edb::Error& e)
    {
        std::cerr << "Error: " << (const char*) e << std::endl ;
    }
    catch (...)
    {
        std::cerr << "Unhandled exception caught" << std::endl;
    }
    return 0;
}