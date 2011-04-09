// Copyright 2001 Victor Joukov, Denis Kaznadzey
#ifndef edbExceptions_h
#define edbExceptions_h

#include "edbError.h"

namespace edb
{
// generic
class NotImplemented : public Error {};
class InternalError : public Error {};


// memory exception
class NotEnoughMemory : public Error {};

class BadParameters : public Error {public: BadParameters(const char *msg) : Error(msg) {} BadParameters() : Error() {}};

// File (i/o) exceptions
class IO_Error : public Error {};
class SeekError : public IO_Error {};
class ReadError : public IO_Error {};
class WriteError : public IO_Error {};
class CreateError : public IO_Error {};
class OpenError : public IO_Error {};
class NoDeviceSpace : public IO_Error {};

// File structure / logic errors
class FileError : public Error { public: FileError(const char *msg) : Error(msg) {} FileError() : Error() {}};
class FileStructureCorrupt : public FileError {public: FileStructureCorrupt(const char *msg) : FileError(msg) {} FileStructureCorrupt() : FileError() {}};
class FileAllreadyExists : public FileError {};
class FileAllreadyOpen : public FileError {};
class FileNotOpen : public FileError {};


// Cache exceptions
class CacheError : public Error {};
class BufferLocked : public CacheError {};
class InvalidBufptr : public CacheError {};
class NoCacheSpace : public CacheError {};

// VLStorage exceptions
class StorageError : public Error {};
class WrongFileType : public StorageError {};
class FreeBlockUsed : public StorageError {};
class BadLocator: public StorageError {};

// Index exceptions
class NotFound : public Error {};
class Duplicate : public Error {};

};

#endif
