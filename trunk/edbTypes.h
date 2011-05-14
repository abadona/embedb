// Copyright 2001 Victor Joukov, Denis Kaznadzey
#ifndef edbTypes_h
#define edbTypes_h

namespace edb
{
typedef unsigned char      uint8;
typedef signed char        int8;
typedef unsigned short     uint16;
typedef signed short       int16;
typedef unsigned int       uint32;
typedef signed int         int32;

#if defined (_MSC_VER)
typedef unsigned __int64   uint64;
typedef signed __int64     int64;
#elif defined (__GNUC__)
typedef unsigned long long uint64;
typedef signed long long   int64;
#else
#error Unknown platform
#endif

#define UINT64_MAX 0xFFFFFFFFFFFFFFFFL
#define UINT32_MAX 0xFFFFFFFF
#define UINT16_MAX 0xFFFF
#define UINT8_MAX  0xFF
#define INT64_MAX  0x7FFFFFFFFFFFFFFFL
#define INT64_MIN  ((-INT64_MAX)-1)
#define INT32_MAX  0x7FFFFFFF
#define INT32_MIN  ((-INT32_MAX)-1)
#define INT16_MAX  0x7FFF
#define INT16_MIN  ((-INT16_MAX)-1)
#define INT8_MAX   0x7F
#define INT8_MIN   ((-INT8_MAX)-1)

typedef uint64 FilePos;
typedef uint32 BufLen;

#define FILEPOS_MAX UINT64_MAX
#define BUFLEN_MAX  UINT32_MAX

typedef uint64 RecLocator;
typedef uint64 RecOff;
typedef uint64 RecLen;
typedef uint64 RecNum;

#define RECLOCATOR_MAX UINT64_MAX
#define RECOFF_MAX UINT64_MAX
#define RECLEN_MAX UINT64_MAX
#define RECNUM_MAX UINT64_MAX

typedef uint32 FRecLen;
#define FRECLEN_MAX UINT32_MAX


typedef void* Key;
typedef uint16 KeyLen;
typedef uint64 Value;
typedef uint64 ValueNum;

#ifndef NULL
#define NULL 0
#endif

#ifndef min_
#define min_(x,y) (((x)<(y))?(x):(y))
#endif
#ifndef max_
#define max_(x,y) (((x)>(y))?(x):(y))
#endif

#ifdef _MSC_VER
#pragma warning (disable : 4786)
#endif

};

#endif
