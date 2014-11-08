
//////////////////////////////////////////////////////////////////////////////
// This software module is developed by SCIDM team in 1999-2008.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
// For any questions please contact SciDM team by email at scidmteam@yahoo.com
//////////////////////////////////////////////////////////////////////////////

#ifndef __portability_h__
#define __portability_h__

// long file support flavours for different platforms
#if defined (_MSC_VER)
    #include <io.h>
    #define sci_stat _stati64
    #define sci_stat_struc _stati64
    #define sci_open _open
    #define sci_read _read
    #define sci_write _write
    #define sci_close _close
    #define sci_lseek _lseeki64
    #define sci_tell _telli64
    #define sci_sopen _sopen
    #define sci_screate _sopen
    #define sci_commit _commit
    #define sci_filelength _filelength
    #define sci_chsize chsize
#elif defined (__CYGWIN__)
    #include <unistd.h>
    #define sci_stat stat
    #define sci_stat_struc stat
    #define sci_open open
    #define sci_read read
    #define sci_write write
    #define sci_close close
    #define sci_lseek lseek
    #define sci_tell tell
    #define sci_sopen(FNAME, FLAGS, SMODE) open(FNAME, FLAGS)
    #define sci_screate(FNAME, FLAGS, SMODE, FMODE) open(FNAME, FLAGS, FMODE)
    #define sci_commit fsync
    #define sci_filelength filelength
    #define sci_chsize ftruncate
#elif defined (__MACOSX__)
    #include <unistd.h>
    #define sci_stat stat
    #define sci_stat_struc stat
    #define sci_open open
    #define sci_read read
    #define sci_write write
    #define sci_close close
    #define sci_lseek lseek
    #define sci_tell tell
    #define sci_sopen(FNAME, FLAGS, SMODE) open(FNAME, FLAGS)
    #define sci_screate(FNAME, FLAGS, SMODE, FMODE) open(FNAME, FLAGS, FMODE)
    #define sci_commit fsync
    #define sci_filelength filelength
    #define sci_chsize ftruncate
#else // plain unix :)
    #include <unistd.h>
    #define sci_stat stat64
    #define sci_stat_struc stat64
    #define sci_open open64
    #define sci_read read
    #define sci_write write
    #define sci_close close
    #define sci_lseek lseek64
    #define sci_tell(FD) lseek64(FD, 0, SEEK_CUR)
    #define sci_sopen(FNAME, FLAGS, SMODE) open(FNAME, FLAGS)
    #define sci_screate(FNAME, FLAGS, SMODE, FMODE) open(FNAME, FLAGS, FMODE)
    #define sci_commit fsync
    inline off64_t sci_filelength (int fd)
    {
        off64_t curpos = lseek64 (fd, 0, SEEK_CUR);
        off64_t result = lseek64 (fd, 0, SEEK_END);
        lseek64 (fd, curpos, SEEK_SET);
        return result;
    }
    #define sci_chsize ftruncate64
    #define sci_unlink unlink
#endif

#if defined (_MSC_VER)
    #define strncasecmp strnicmp
    #define strcasecmp stricmp
#endif

#if !defined (_MSC_VER)
    #define _O_BINARY O_BINARY
    #define _O_RDWR O_RDWR
    #define _O_RDONLY O_RDONLY
    #define _O_WRONLY O_WRONLY
    #define _O_CREAT O_CREAT
    #define _O_APPEND O_APPEND
    #define _O_EXCL O_EXCL
    #define _SH_DENYWR 0
    #define _S_IREAD S_IREAD
    #define _S_IWRITE S_IWRITE
    
    #define __int64 long long

    #ifndef O_BINARY
    #define O_BINARY 0
    #endif
#endif


#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif


#endif // __portability_h__
