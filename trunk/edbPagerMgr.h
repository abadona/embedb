// Copyright 2001 Victor Joukov, Denis Kaznadzey
#ifndef edbPagerMgr_h
#define edbPagerMgr_h

#include "edbTypes.h"
#include "edbPager.h"

namespace edb
{

class PagerMgr
{
public:
    virtual                 ~PagerMgr        () {}
    virtual     Pager&      getPager         (uint32 pagesize = 0, uint32 poolsize_hint = 0) = 0;
    virtual     void        releasePager     (uint32 pagesize = 0) = 0;
};

class PagerMgrFactory
{
public:
    virtual                 ~PagerMgrFactory () {};
    virtual     PagerMgr&   create           () = 0;
};

}; // namespace edb

#endif