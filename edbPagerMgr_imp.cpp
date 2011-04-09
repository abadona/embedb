// Copyright 2001 Victor Joukov, Denis Kaznadzey
#define pagerMgrFactory_defined
#include "edbPagerMgr_imp.h"
#include "edbPagerFactory.h"

namespace edb
{
//const uint32 initial_pager_space = 0x200; // 512 bytes
const uint32 initial_pager_space = 0x4000000; // 64Mb == 2048 32-Kb pages

//const uint32 default_page_size   = 0x10; // 16 bytes
const uint32 default_page_size   = 0x8000; // 32 Kbytes

static PagerMgrFactory_imp theFactory;
PagerMgrFactory& pagerMgrFactory = theFactory;

PagerMgr_imp::~PagerMgr_imp ()
{
}

Pager& PagerMgr_imp::getPager (uint32 pagesize, uint32 poolsize_hint)
{
    if (!pagesize) pagesize = default_page_size;
    if (!poolsize_hint) poolsize_hint = initial_pager_space / pagesize;
    I2pu::iterator itr = pagers_.find (pagesize);
    if (itr != pagers_.end ())
    {
        PagerUse& pu = (*itr).second;
        pu.use_ ++;
        return *pu.pager_;
    }
    PagerUse& pu = pagers_ [pagesize];
    pu.pager_ = &pagerFactory.create (pagesize, poolsize_hint);
    pu.use_ = 1;
    return *pu.pager_;
}
void PagerMgr_imp::releasePager (uint32 pagesize)
{
    if (!pagesize) pagesize = default_page_size;
    I2pu::iterator itr = pagers_.find (pagesize);
    if (itr != pagers_.end ())
    {
        PagerUse& pu = (*itr).second;
        pu.use_ --;
        if (!pu.use_)
            pagers_.erase (itr);
    }
}

PagerMgr& PagerMgrFactory_imp::create ()
{
    return *new PagerMgr_imp ();
}

};