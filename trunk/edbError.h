// Copyright 2001 Victor Joukov, Denis Kaznadzey
#ifndef edbError_h
#define edbError_h

#include <string>
#include <sstream>
#include "edbTypes.h"

namespace edb
{

#define MAX_ERR_MSG_LEN 512

class Error
{
protected:
    char msg_ [MAX_ERR_MSG_LEN];
    Error ();
public:
    Error (const char* s);
    operator const char* () {return msg_;}
};

class ErrorStream : public std::ostringstream
{
public:
    void throw_ ();
};



#ifndef edbError_cpp
extern ErrorStream ers;
#endif

#define ERR(x) { ers << (const char*)x << " (module " << (const char*)__FILE__ << ", line " << (int16)__LINE__ << ")"; ers.throw_ (); }

};

#endif // edbError_h
