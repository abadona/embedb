##############################################################################
## This software module is developed by SciDM (Scientific Data Management) in 1998-2015
## 
## This program is free software; you can redistribute, reuse,
## or modify it with no restriction, under the terms of the MIT License.
## 
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
## 
## For any questions please contact Denis Kaznadzey at dkaznadzey@yahoo.com
##############################################################################

.PHONY : clean all

UNAME:=$(shell uname)
ifneq (,$(findstring CYGWIN,$(UNAME)))
  EXESUFFIX:=.exe
else
  EXESUFFIX:=
endif

CC=$(CCPATH)g++
#PIC := $(if $(filter $(shell uname -i),x86_64),-fPIC,)
OPTLEVEL := $(if $(OPTLEVEL),$(OPTLEVEL),-O0)
DEBUGLEVEL := $(if $(DEBUGLEVEL),$(DEBUGLEVEL),-g2)
CPPFLAGS=$(PIC) $(DEBUGLEVEL) $(OPTLEVEL) -D__x86__
AR=ar
ARFLAGS=rc

LIBNAME=edb
LIBTARGETDIR=./lib

LIBTARGETNAME=$(addsuffix .a, $(addprefix lib,$(LIBNAME)))
LIBTARGET=$(LIBTARGETDIR)/$(LIBTARGETNAME)
LIBTARGET=$(LIBTARGETDIR)/libedb.a

LIBS_SPEC=$(addprefix -l,$(LIBNAME))
LIB_DIRS_SPEC=$(addprefix -L,$(LIBTARGETDIR))

TESTTARGETNAME=edb_test$(EXESUFFIX)
TESTTARGETDIR=./bin
TESTTARGET=$(TESTTARGETDIR)/$(TESTTARGETNAME)

SYSINCLUDE_DIRS=
SYSINCLUDE_DIRS_SPEC=$(addprefix -I,$(SYSINCLUDE_DIRS))

INCLUDE_DIRS=
INCLUDE_DIRS_SPEC=$(addprefix -I,$(INCLUDE_DIRS))

SYSLIBS=
SYSLIBS_SPEC=$(addprefix -l,$(SYSLIBS))

SYSLIB_DIRS=
SYSLIB_DIRS_SPEC=$(addprefix -L,$(SYSLIB_DIRS))


#unused library modules
#edbDummyCache_imp
#edbSimplePager_imp
#edbDummyBufferedFile_imp
#edbVLPagedCache_imp \
#edbVLPagedFile_imp \

LIB_MODULES = \
edbBTree \
edbCachedFile_imp \
edbError \
edbFileHandleMgr_imp \
edbFStorage_imp \
edbPagedFile_imp \
edbPager_imp \
edbPagerMgr_imp \
edbSimpleCache_imp \
edbSplitFileFactory_imp \
edbSplitFile_imp \
edbSystemCache \
edbThePagerMgr \
edbVStorage_imp


#unused test modules
#edbVLPagedFile_test
#edbBufferedFile_test \

TEST_MODULES = \
edbFileHandleMgr_test \
edbPagedFile_test \
edbFStorage_test \
edbFile_test \
edbPager_test \
edbVStorage_test \
edbBTree_test \
driver

LIB_OBJS=$(addsuffix .o,$(LIB_MODULES))

TEST_OBJS=$(addsuffix .o,$(TEST_MODULES))

LIB_DEPENDENCIES=$(patsubst %.o,%.dep,$(LIB_OBJS))

TEST_DEPENDENCIES=$(patsubst %.o,%.dep,$(TEST_OBJS))

vpath %.h $(INCLUDE_DIRS)
vpath lib% $(LIB_DIRS)

%.o : %.cpp
	$(CC) $(CPPFLAGS) $(INCLUDE_DIRS_SPEC) $(SYSINCLUDE_DIRS_SPEC) -c -o $@ $<
	$(CC) $(CPPFLAGS) $(INCLUDE_DIRS_SPEC) $(SYSINCLUDE_DIRS_SPEC) -MM -MG $< >$*.dep

all : $(LIBTARGETDIR) $(LIBTARGET) $(TESTTARGETDIR) $(TESTTARGET)

$(LIBTARGETDIR) : $(LIBTARGETDIR)
	-mkdir $(LIBTARGETDIR)

$(TESTTARGETDIR) : $(TESTTARGETDIR)
	-mkdir $(TESTTARGETDIR)

$(LIBTARGET) : $(LIB_OBJS)
	$(AR) $(ARFLAGS) $@ $(LIB_OBJS)

$(TESTTARGET) :  $(TEST_OBJS) $(LIB_OBJS)
	$(CC)  $(CPPFLAGS) -o $@ $(TEST_OBJS) $(LIB_OBJS) $(SYSLIB_DIRS_SPEC) $(SYSLIBS_SPEC)

-include $(LIB_DEPENDENCIES)
-include $(TEST_DEPENDENCIES)

clean :
	rm -f $(LIB_OBJS)
	rm -f $(TEST_OBJS)
	rm -f $(LIBTARGET)
	rm -f $(TESTTARGET)
	rm -f $(LIB_DEPENDENCIES)
	rm -f $(TEST_DEPENDENCIES)
