// Copyright 2001 Victor Joukov, Denis Kaznadzey
#ifndef i64out_h
#define i64out_h

#include <ostream>

std::ostream& operator << (std::ostream&, __int64 ii);
std::ostream& operator << (std::ostream&, unsigned __int64 ii);


#endif