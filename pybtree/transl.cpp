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

#include "transl.h"

#include "edbSplitFileFactory.h"
#include "edbPagedFileFactory.h"
#include <stdio.h>
#include <string.h>



#define MAX_PATH_SZ 2048

void splitPath (const char* fullname, char* path, int psize, char* fname, int fnsize)
{
#ifdef _MSC_VER // this is Microsoft compiler
	char psep = '\\';
#else
	char psep = '/';
#endif
	// find last separator
	int l = strlen (fullname) - 1;
	int fl = l;
	while (l >= 0)
	{
		if (fullname [l] == psep) break;
		l --;
	}
	int ln;
	if (l >= 0 && fullname [l] == psep)
	{
		ln = min_ (l, MAX_PATH_SZ-1);
		memcpy (path, fullname, ln);
		path [ln] = 0;
	}
	ln = min_ (fl - l - 1, MAX_PATH_SZ-1);
	memcpy (fname, fullname + l + 1, ln);
	fname [ln] = 0;
}


Query::Query (const void* key, int klen)
:
query_ (NULL)
{
	query_ = new WhileMatches (key, klen);
}
Query::~Query ()
{
	if (query_)
	{
		delete query_;
		query_ = NULL;
	}
}

Cursor::Cursor ()
:
cursor_ (NULL)
{
	cursor_ = new edb::BTreeCursor;
}
Cursor::~Cursor ()
{
	if (cursor_)
	{
		delete cursor_;
		cursor_ = NULL;
	}
}


BTree::BTree ()
:
btree_ (NULL)
{
}
BTree::~BTree ()
{
	if (btree_)
	{
		delete btree_;
		btree_ = NULL;
	}
}
void BTree::init (edb::BTree* btree)
{
	btree_ = btree;
}
void BTree::insert (const void *key, unsigned short klen, const void *val, unsigned short vlen)
{
	if (!btree_) throw UnInit ();
	btree_->insert (key, klen, val, vlen);
}
void BTree::initcursor (Cursor* cursor, Query* query)
{
	if (!btree_) throw UnInit ();
	btree_->initcursor (*(cursor->cursor_), *(query->query_));
}
bool BTree::fetch (Cursor* cursor, void *key, unsigned short* klen, void *val, unsigned short* vlen)
{
	if (!btree_) throw UnInit ();
	return btree_->fetch (*(cursor->cursor_), key, *klen, val, *vlen);
}
bool BTree::fetchval (Cursor* cursor, void *val, unsigned short* vlen)
{
	if (!btree_) throw UnInit ();
	return btree_->fetch (*(cursor->cursor_), val, *vlen);
}
bool BTree::remove (Cursor* cursor)
{
	if (!btree_) throw UnInit ();
	btree_->remove (*(cursor->cursor_));
	return true;
}
bool BTree::find (const void *key, unsigned short klen, void *val, unsigned short* vlen)
{
	if (!btree_) throw UnInit ();
	try
	{
		btree_->find (key, klen, val, *vlen);
		return true;
	}
	catch (edb::NotFound&)
	{
		return false;
	}
}
bool BTree::flush ()
{
	if (!btree_) throw UnInit ();
	return btree_->flush ();
}
bool BTree::close ()
{
	if (!btree_) throw UnInit ();
	edb::BTreeFile* file = btree_->getFile ();
	btree_->detach ();
	file->close ();
	return true;
}
int	BTree::keySize () const
{
	if (!btree_) throw UnInit ();
	return btree_->keySize ();
}
int	BTree::valSize () const
{
	if (!btree_) throw UnInit ();
	return btree_->valSize ();
}
bool BTree::isUnique () const
{
	if (!btree_) throw UnInit ();
	return btree_->isUnique ();
}
bool BTree::isVarkey () const
{
	if (!btree_) throw UnInit ();
	return btree_->isVarkey ();
}
bool BTree::isRange () const
{
	if (!btree_) throw UnInit ();
	return btree_->isRange ();
}

BTree* open (const char* fname)
{
	char dir [MAX_PATH_SZ];
	char name [MAX_PATH_SZ];
	splitPath (fname, dir, MAX_PATH_SZ, name, MAX_PATH_SZ);

	if (!edb::splitFileFactory.exists (dir, name)) return NULL;
	edb::BTreeFile& bf = edb::pagedFileFactory.wrap (edb::splitFileFactory.open (dir, name));
	edb::BTree* bt = new edb::BTree ();
	bt->attach (bf);

	BTree* res = new BTree;
	res->init (bt);
	
	return res;
}

BTree* create (const char* fname, int vallen, int keylen, bool unique, bool range)
{
	char dir [MAX_PATH_SZ];
	char name [MAX_PATH_SZ];
	splitPath (fname, dir, MAX_PATH_SZ, name, MAX_PATH_SZ);

	if (edb::splitFileFactory.exists (dir, name)) return NULL;
	edb::BTreeFile& bf = edb::pagedFileFactory.wrap (edb::splitFileFactory.create (dir, name));
	edb::BTree* bt = new edb::BTree ();
	edb::uint32 flags = 0;
	if (!unique) flags |= edb::BTREE_FLAGS_DUPLICATE;
	if (range) flags |=	 edb::BTREE_FLAGS_RANGE;
	if (keylen == 0) flags |= edb::BTREE_FLAGS_VARKEY;
	bt->init (bf, vallen, flags, keylen);
	
	BTree* res = new BTree;
	res->init (bt);
	
	return res;
}

