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
