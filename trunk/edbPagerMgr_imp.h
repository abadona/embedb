// Copyright 2001 Victor Joukov, Denis Kaznadzey
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