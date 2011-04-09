// Copyright 2001 Victor Joukov, Denis Kaznadzey
#define edbError_cpp
#include "edbError.h"
#include <stdio.h>

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

ErrorStream::ErrorStream ()
{
    msg_[0] = 0;
    lpos_ = 0;
}

void ErrorStream::throw_ ()
{
    Error toThrow (msg_);
    lpos_ = 0;
    msg_ [0] = 0;
    throw toThrow;
}

ErrorStream& ErrorStream::operator << (const char* s)
{
    strncpy (msg_+lpos_, s, MAX_ERR_MSG_LEN-1-lpos_);
    lpos_ = max_ (MAX_ERR_MSG_LEN-1, lpos_ + strlen (s));
    msg_[lpos_] = 0;
    return *this;
}

ErrorStream& ErrorStream::operator << (char* s)
{
    strncpy (msg_+lpos_, s, MAX_ERR_MSG_LEN-1-lpos_);
    lpos_ = max_ (MAX_ERR_MSG_LEN-1, lpos_ + strlen (s));
    msg_[lpos_] = 0;
    return *this;
}

ErrorStream& ErrorStream::operator << (int8 s)
{
    if (lpos_ < MAX_ERR_MSG_LEN-1)
    {
        msg_ [lpos_++] = s;
        msg_ [lpos_] = 0;
    }
    return *this;
}

ErrorStream& ErrorStream::operator << (uint8 s)
{
    if (lpos_ < MAX_ERR_MSG_LEN-1)
    {
        msg_ [lpos_++] = s;
        msg_ [lpos_] = 0;
    }
    return *this;
}

ErrorStream& ErrorStream::operator << (int16 s)
{
    char buf [64];
    sprintf (buf, "%hd", s);
    strncpy (msg_, buf, MAX_ERR_MSG_LEN-1);
    lpos_ = min_ (MAX_ERR_MSG_LEN-1, lpos_+strlen (buf));
    msg_[lpos_] = 0;
    return *this;
}

ErrorStream& ErrorStream::operator << (uint16 s)
{
    char buf [64];
    sprintf (buf, "%hu", s);
    strncpy (msg_, buf, MAX_ERR_MSG_LEN-1);
    lpos_ = min_ (MAX_ERR_MSG_LEN-1, lpos_+strlen (buf));
    msg_[lpos_] = 0;
    return *this;
}

ErrorStream& ErrorStream::operator << (int32 s)
{
    char buf [64];
    sprintf (buf, "%ld", s);
    strncpy (msg_, buf, MAX_ERR_MSG_LEN-1);
    lpos_ = min_ (MAX_ERR_MSG_LEN-1, lpos_+strlen (buf));
    msg_[lpos_] = 0;
    return *this;
}

ErrorStream& ErrorStream::operator << (uint32 s)
{
    char buf [64];
    sprintf (buf, "%lu", s);
    strncpy (msg_, buf, MAX_ERR_MSG_LEN-1);
    lpos_ = min_ (MAX_ERR_MSG_LEN-1, lpos_+strlen (buf));
    msg_[lpos_] = 0;
    return *this;
}

ErrorStream& ErrorStream::operator << (int64 s)
{
    char buf [64];
    sprintf (buf, "%I64d", s);
    strncpy (msg_, buf, MAX_ERR_MSG_LEN-1);
    lpos_ = min_ (MAX_ERR_MSG_LEN-1, lpos_+strlen (buf));
    msg_[lpos_] = 0;
    return *this;
}

ErrorStream& ErrorStream::operator << (uint64 s)
{
    char buf [64];
    sprintf (buf, "%I64u", s);
    strncpy (msg_, buf, MAX_ERR_MSG_LEN-1);
    lpos_ = min_ (MAX_ERR_MSG_LEN-1, lpos_+strlen (buf));
    msg_[lpos_] = 0;
    return *this;
}


};





