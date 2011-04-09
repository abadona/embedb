if exist m:\edb goto EDBDIR
mkdir m:\edb
:EDBDIR
if exist m:\edb\lib goto LIBDIR
mkdir m:\edb\lib
:LIBDIR
if exist m:\edb\include goto INCDIR
mkdir m:\edb\include
:INCDIR
copy lib\* m:\edb\lib
copy edbBTree.h m:\edb\include
copy edbCachedFile.h m:\edb\include
copy edbCachedFileFactory.h m:\edb\include
copy edbError.h m:\edb\include
copy edbExceptions.h m:\edb\include
copy edbFile.h m:\edb\include
copy edbFileCache.h m:\edb\include
copy edbFileHandleMgr.h m:\edb\include
copy edbFStorage.h m:\edb\include
copy edbFStorageFactory.h m:\edb\include
copy edbPagedFile.h m:\edb\include
copy edbPagedFileFactory.h m:\edb\include
copy edbPager.h m:\edb\include
copy edbPagerFactory.h m:\edb\include
copy edbPagerMgr.h m:\edb\include
copy edbPagerMgrFactory.h m:\edb\include
copy edbSimpleCacheFactory.h m:\edb\include
copy edbSplitFileFactory.h m:\edb\include
copy edbSystemCache.h m:\edb\include
copy edbThePagerMgr.h m:\edb\include
copy edbTypes.h m:\edb\include
copy edbVStorage.h m:\edb\include
copy edbVStorageFactory.h m:\edb\include
copy driver.cpp shiptest
copy edbBTree_test.cpp shiptest
copy edbFileHandleMgr_test.cpp shiptest
copy edbFile_test.cpp shiptest
copy edbFStorage_test.cpp shiptest
copy edbPagedFile_test.cpp shiptest
copy edbPager_test.cpp shiptest
copy edbTests.h shiptest
copy edbVStorage_test.cpp shiptest
