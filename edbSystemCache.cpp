// Copyright 2001 Victor Joukov, Denis Kaznadzey
#include "edbSystemCache.h"
#include "edbSimpleCacheFactory.h"

namespace edb
{

static FileCache* theCache = NULL;

FileCache& systemCache ()
{
	if (!theCache)
		theCache = &simpleCacheFactory.create ();
    return *theCache;
}

};