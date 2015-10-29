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

#ifndef __TRANSL_H__
#define __TRANSL_H__

#include "edbBTree.h"

class WhileMatches : public edb::BTreeQueryBase
{
public:
    WhileMatches (const void *key, edb::LenT len) 
	: 
	edb::BTreeQueryBase (edb::BTREE_PARTIAL, true, key, len)
    {
	}
    bool done (const void *key, edb::LenT len, const void *val, edb::LenT vlen)
    {
        return memcmp (key_, key, len_) != 0;
    }
} ;

// exceptions
class UnInit {};
class NotFound {};


class Query
{
protected:
	edb::BTreeQueryBase* query_;
public:
	Query (const void* key, int klen);
	~Query ();
	friend class BTree;
};

class Cursor 
{
protected:
	edb::BTreeCursor* cursor_;
public:
	Cursor ();
	~Cursor ();
	friend class BTree;
};


// adapter class for BTree
class BTree
{
protected: 
	edb::BTree* btree_;
public:
	BTree ();
	~BTree ();
	
	void	init (edb::BTree* btree);

	void	insert (const void *key, unsigned short klen, const void *val, unsigned short vlen);
	void    initcursor (Cursor* cursor, Query* query);
	bool    fetch (Cursor* cursor, void *key, unsigned short* klen, void *val, unsigned short* vlen);
	bool    fetchval (Cursor* cursor, void *val, unsigned short* vlen);
	bool    remove (Cursor* cursor);
	bool	find (const void *key, unsigned short klen, void *val, unsigned short* vlen);

	bool	flush ();
	bool	close ();
	int		keySize () const;
	int		valSize () const;
	bool	isUnique () const;
	bool	isVarkey () const;
	bool	isRange () const;

};



BTree* open (const char* fname);
BTree* create (const char* fname, int vallen, int keylen = 0, bool unique = false, bool range = false);



#endif