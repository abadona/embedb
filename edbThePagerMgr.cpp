// Copyright 2001 Victor Joukov, Denis Kaznadzey
#define thePagerMgr_defined
#include "edbPagerMgrFactory.h"

namespace edb
{

static PagerMgr* theMgr = NULL;

PagerMgr& thePagerMgr ()
{
    if (!theMgr)
        theMgr = &pagerMgrFactory.create ();
    return *theMgr;
}

};
