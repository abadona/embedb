# Microsoft Developer Studio Project File - Name="libedb" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libedb - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libedb.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libedb.mak" CFG="libedb - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libedb - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libedb - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=xicl6.exe
RSC=rc.exe

!IF  "$(CFG)" == "libedb - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "lib"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "USE_SGI_STL" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=xilink6.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "libedb - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "libedb___Win32_Debug"
# PROP BASE Intermediate_Dir "libedb___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "lib"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "USE_SGI_STL" /FR /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=xilink6.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"lib\libedbd.lib"

!ENDIF 

# Begin Target

# Name "libedb - Win32 Release"
# Name "libedb - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
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

SOURCE=.\edbPagedFile.h
# End Source File
# Begin Source File

SOURCE=.\edbPagedFile_imp.h
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

SOURCE=.\edbSystemCache.h
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
# End Group
# End Target
# End Project
