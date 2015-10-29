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

#ifdef embedb_debug
%module pybtreeD
#else
%module pybtree
#endif

%{
#pragma warning (disable : 4786)
#include "transl.h"
%}


%typemap(in) int64 {
	$1 = PyLong_AsLongLong($input);
}

%typemap(out) int64 {
	$result = PyLong_FromLongLong($1); 
}

%include "transl.h"
