# Microsoft Developer Studio Project File - Name="Embedb" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=Embedb - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Embedb.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Embedb.mak" CFG="Embedb - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Embedb - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "Embedb - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Embedb - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /D "NDEBUG" /D "_MYDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "USE_SGI_STL" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 /nologo /stack:0x2faf080 /subsystem:console /profile /machine:I386
# SUBTRACT LINK32 /debug

!ELSEIF  "$(CFG)" == "Embedb - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "USE_SGI_STL" /FR /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 /nologo /stack:0x2faf080 /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "Embedb - Win32 Release"
# Name "Embedb - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\driver.cpp
# End Source File
# Begin Source File

SOURCE=.\edbBTree.cpp
# End Source File
# Begin Source File

SOURCE=.\edbCachedFile_imp.cpp
# End Source File
# Begin Source File

SOURCE=.\edbError.cpp
# End Source File
# Begin Source File

SOURCE=.\edbFileHandleMgr_imp.cpp
# End Source File
# Begin Source File

SOURCE=.\edbFStorage_imp.cpp
# End Source File
# Begin Source File

SOURCE=.\edbPagedFile_imp.cpp
# End Source File
# Begin Source File

SOURCE=.\edbPager_imp.cpp
# End Source File
# Begin Source File

SOURCE=.\edbPagerMgr_imp.cpp
# End Source File
# Begin Source File

SOURCE=.\edbSimpleCache_imp.cpp
# End Source File
# Begin Source File

SOURCE=.\edbSplitFile_imp.cpp
# End Source File
# Begin Source File

SOURCE=.\edbSplitFileFactory_imp.cpp
# End Source File
# Begin Source File

SOURCE=.\edbSystemCache.cpp
# End Source File
# Begin Source File

SOURCE=.\edbThePagerMgr.cpp
# End Source File
# Begin Source File

SOURCE=.\edbVStorage_imp.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\edbBTree.h
# End Source File
# Begin Source File

SOURCE=.\edbCachedFile.h
# End Source File
# Begin Source File

SOURCE=.\edbCachedFile_imp.h
# End Source File
# Begin Source File

SOURCE=.\edbCachedFileFactory.h
# End Source File
# Begin Source File

SOURCE=.\edbError.h
# End Source File
# Begin Source File

SOURCE=.\edbExceptions.h
# End Source File
# Begin Source File

SOURCE=.\edbFile.h
# End Source File
# Begin Source File

SOURCE=.\edbFileCache.h
# End Source File
# Begin Source File

SOURCE=.\edbFileHandleMgr.h
# End Source File
# Begin Source File

SOURCE=.\edbFileHandleMgr_imp.h
# End Source File
# Begin Source File

SOURCE=.\edbFStorage.h
# End Source File
# Begin Source File

SOURCE=.\edbFStorage_imp.h
# End Source File
# Begin Source File

SOURCE=.\edbFStorageFactory.h
# End Source File
# Begin Source File

SOURCE=.\edbPagedFile.h
# End Source File
# Begin Source File

SOURCE=.\edbPagedFile_imp.h
# End Source File
# Begin Source File

SOURCE=.\edbPagedFileFactory.h
# End Source File
# Begin Source File

SOURCE=.\edbPager.h
# End Source File
# Begin Source File

SOURCE=.\edbPager_imp.h
# End Source File
# Begin Source File

SOURCE=.\edbPagerFactory.h
# End Source File
# Begin Source File

SOURCE=.\edbPagerMgr.h
# End Source File
# Begin Source File

SOURCE=.\edbPagerMgr_imp.h
# End Source File
# Begin Source File

SOURCE=.\edbPagerMgrFactory.h
# End Source File
# Begin Source File

SOURCE=.\edbSimpleCache_imp.h
# End Source File
# Begin Source File

SOURCE=.\edbSimpleCacheFactory.h
# End Source File
# Begin Source File

SOURCE=.\edbSplitFile_imp.h
# End Source File
# Begin Source File

SOURCE=.\edbSplitFileFactory.h
# End Source File
# Begin Source File

SOURCE=.\edbSystemCache.h
# End Source File
# Begin Source File

SOURCE=.\edbTests.h
# End Source File
# Begin Source File

SOURCE=.\edbThePagerMgr.h
# End Source File
# Begin Source File

SOURCE=.\edbTypes.h
# End Source File
# Begin Source File

SOURCE=.\edbVStorage.h
# End Source File
# Begin Source File

SOURCE=.\edbVStorage_imp.h
# End Source File
# Begin Source File

SOURCE=.\edbVStorageFactory.h
# End Source File
# End Group
# Begin Group "Test Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\edbBTree_test.cpp
# End Source File
# Begin Source File

SOURCE=.\edbFile_test.cpp
# End Source File
# Begin Source File

SOURCE=.\edbFileHandleMgr_test.cpp
# End Source File
# Begin Source File

SOURCE=.\edbFStorage_test.cpp
# End Source File
# Begin Source File

SOURCE=.\edbPagedFile_test.cpp
# End Source File
# Begin Source File

SOURCE=.\edbPager_test.cpp
# End Source File
# Begin Source File

SOURCE=.\edbVStorage_test.cpp
# End Source File
# End Group
# Begin Source File

SOURCE=".\B-tree node layout.txt"
# End Source File
# End Target
# End Project
