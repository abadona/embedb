// Copyright 2001 Victor Joukov, Denis Kaznadzey
#ifndef edbError_h
#define edbError_h

#include <string>
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
                         
class ErrorStream 
{
private:
    char msg_ [MAX_ERR_MSG_LEN];
    int lpos_;
public:
    ErrorStream ();
    void throw_ ();
    ErrorStream& operator << (const char* s);
    ErrorStream& operator << (char* s);
    ErrorStream& operator << (int8 s);
    ErrorStream& operator << (uint8 s);
    ErrorStream& operator << (int16 s);
    ErrorStream& operator << (uint16 s);
    ErrorStream& operator << (int32 s);
    ErrorStream& operator << (uint32 s);
    ErrorStream& operator << (int64 s);
    ErrorStream& operator << (uint64 s);
};



#ifndef edbError_cpp
extern ErrorStream ers;
#endif

#define ERR(x) { ers << (const char*)x << " (module " << (const char*)__FILE__ << ", line " << (int16)__LINE__ << ")"; ers.throw_ (); }

};

#endif // edbError_h
