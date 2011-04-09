// Copyright 2001 Victor Joukov, Denis Kaznadzey
#include "i64out.h"
#include <stdio.h>

std::ostream& operator << (std::ostream& o, __int64 ii)
{
    char buf [64];
    sprintf (buf, "%I64d", ii);
    return o << buf;
}
std::ostream& operator << (std::ostream& o, unsigned __int64 ii)
{
    char buf [64];
    sprintf (buf, "%I64u", ii);
    return o << buf;
}
