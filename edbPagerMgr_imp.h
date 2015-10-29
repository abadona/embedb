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

#ifndef edbPagerMgr_imp_h
#define edbPagerMgr_imp_h

#include "edbPagerMgr.h"
#include <map>

namespace edb
{

class PagerMgr_imp : public PagerMgr
{
    struct PagerUse
    {
        PagerUse () 
        : 
        pager_ (NULL), 
        use_ (0) 
        {
        }
        ~PagerUse () 
        { 
            if (pager_) 
                delete pager_; 
        }
        Pager* pager_;
        uint32 use_;
    };
    typedef std::map<uint32, PagerUse> I2pu;

                I2pu pagers_;

protected:
                PagerMgr_imp     () {}
public:
                ~PagerMgr_imp     ();
    Pager&      getPager         (uint32 pagesize = 0, uint32 poolsize_hint = 0);
    void        releasePager     (uint32 pagesize = 0);

friend class PagerMgrFactory_imp;
};

    class PagerMgrFactory_imp : public PagerMgrFactory
{
public:
    PagerMgr&   create           ();
};

}; // namespace edb

#endif