// Copyright 2001 Victor Joukov, Denis Kaznadzey
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





