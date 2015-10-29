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

#define edbError_cpp
#include "edbError.h"
#include <stdio.h>
#include <string.h>

namespace edb
{

ErrorStream ers;

Error::Error ()
{
    msg_ [0] = 0;
}

Error::Error (const char* s)
{
    strncpy (msg_, s, MAX_ERR_MSG_LEN-1);
    msg_ [MAX_ERR_MSG_LEN-1] = 0;
}

void ErrorStream::throw_ ()
{
    Error toThrow (str ().c_str ());
    throw toThrow;
}


};





