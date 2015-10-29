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