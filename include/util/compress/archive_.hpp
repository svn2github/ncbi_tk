#ifndef UTIL_COMPRESS__ARCHIVE___HPP
#define UTIL_COMPRESS__ARCHIVE___HPP

/* $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors:  Vladimir Ivanov, Anton Lavrentiev
 *
 * File Description:
 *   Compression archive API - Common base classes and interfaces.
 *
 */

///  @file archive_.hpp
///  Archive API.

#include <corelib/ncbifile.hpp>
#include <util/compress/compress.hpp>


/** @addtogroup Compression
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
/// CArchiveException --
///
/// Define exceptions generated by the API.
/// CArchiveException inherits its basic functionality from CCoreException
/// and defines additional error codes for archive operations.

class NCBI_XUTIL_EXPORT CArchiveException : public CCoreException
{
public:
    /// Error types that file operations can generate.
    enum EErrCode {
        eMemory,
        eUnsupportedEntryType,
        eBadName,
        eCreate,
        eOpen,
        eClose,
        eList,
        eExtract,
        eAppend,
        eWrite,
        eBackup,
        eRestoreAttrs
    };

    /// Translate from an error code value to its string representation.
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eMemory:               return "eMemory";
        case eUnsupportedEntryType: return "eUnsupportedEntryType";
        case eBadName:              return "eBadName";
        case eCreate:               return "eCreate";
        case eOpen:                 return "eOpen";
        case eClose:                return "eClose";
        case eList:                 return "eList";
        case eExtract:              return "eExtract";
        case eAppend:               return "eAppend";
        case eWrite:                return "eWrite";
        case eBackup:               return "eBackup";
        case eRestoreAttrs:         return "eRestoreAttrs";
        default:                    return CException::GetErrCodeString();
        }
    }
    // Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT(CArchiveException, CCoreException);
};


//////////////////////////////////////////////////////////////////////////////
///
/// CArchiveEntryInfo class
///
/// Information about an archive entry.

class NCBI_XUTIL_EXPORT CArchiveEntryInfo
{
public:
    /// Archive entry type (the same as directory entry type).
    typedef CDirEntry::EType EType;

    // Constructor.
    CArchiveEntryInfo()
    {
        Reset();
    }

    void Reset(void)
    {
        memset(&m_Stat, 0, sizeof(m_Stat));
        m_Index = 0;
        m_Type = CDirEntry::eUnknown;
        m_CompressedSize = 0;
    }

    // No setters -- they are not needed for access by the user, and
    // thus are done directly from CArchive for the sake of performance.

    // Getters only!
    CDirEntry::EType GetType(void)             const { return m_Type;          }
    const string&    GetName(void)             const { return m_Name;          }
    const string&    GetLinkName(void)         const { return m_LinkName;      }
    const string&    GetUserName(void)         const { return m_UserName;      }
    const string&    GetGroupName(void)        const { return m_GroupName;     }
    const string&    GetComment(void)          const { return m_Comment;       }
    const size_t     GetIndex(void)            const { return m_Index;         }
    time_t           GetModificationTime(void) const { return m_Stat.st_mtime; }
    time_t           GetLastAccessTime(void)   const { return m_Stat.st_atime; }
    time_t           GetCreationTime(void)     const { return m_Stat.st_ctime; }
    Uint8            GetSize(void)             const { return m_Stat.st_size;  }
    mode_t           GetMode(void)             const; // Raw mode as stored in archive
    void             GetMode(CDirEntry::TMode*            user_mode,
                             CDirEntry::TMode*            group_mode   = 0,
                             CDirEntry::TMode*            other_mode   = 0,
                             CDirEntry::TSpecialModeBits* special_bits = 0) const;
    unsigned int     GetMajor(void)            const;
    unsigned int     GetMinor(void)            const;
    unsigned int     GetUserId(void)           const { return m_Stat.st_uid;   }
    unsigned int     GetGroupId(void)          const { return m_Stat.st_gid;   }

    // Comparison operator
    bool operator == (const CArchiveEntryInfo& info) const;

protected:
    size_t           m_Index;          ///< Entry index in the archive
    TNcbiSys_stat    m_Stat;           ///< Direntry-compatible info (as applicable)
    CDirEntry::EType m_Type;           ///< Type
    string           m_Name;           ///< Entry name
    string           m_LinkName;       ///< Link name if type is eLink
    string           m_UserName;       ///< User name
    string           m_GroupName;      ///< Group name
    string           m_Comment;        ///< Entry comment
    Uint8            m_CompressedSize; ///< Compressed size

    friend class CArchive;     // Setter
    friend class CArchiveZip;  // Setter
};


/// Nice TOC (table of contents) printout
NCBI_XUTIL_EXPORT ostream& operator << (ostream&, const CArchiveEntryInfo&);



//////////////////////////////////////////////////////////////////////////////
///
/// IArchive -- abstract interface class for archive formats.
///

class NCBI_XUTIL_EXPORT IArchive
{
public:
    // Type definitions.
    typedef CCompression::ELevel ELevel;

    /// Processing direction.
    enum EDirection {
        eRead,               ///< Reading from archive
        eWrite               ///< Writing into archive
    };
    /// Archive location.
    enum ELocation {
        eFile,               ///< File-based archive
        eMemory              ///< Memory-based archive
    };

    /// Type of user-defined callback for extraction from archive.
    ///
    /// It can be used to extract data from a single archive entry on the fly.
    /// @param info
    ///   A current entry that is processed.
    /// @param buf
    ///   Pointer to buffer with data.
    /// @param n
    ///   Size of data in the buffer.
    /// @return
    ///   The callback function should process all incoming data and return
    ///   'n' for success. If returning value don't equal the number of bytes
    ///   in the incoming buffer, that extraction fail.
    typedef size_t (*Callback_Write)(const CArchiveEntryInfo& info, const void* buf, size_t n);

public:
    /// Create new archive file.
    ///
    /// @param filename
    ///   File name of the archive to create.
    /// @note
    ///   File can be overwritten if exists.
    /// @sa
    ///   CreateMemory, AddEntryFromFile, AddEntryFromMemory
    virtual void CreateFile(const string& filename) = 0;

    /// Create new archive located in memory.
    ///
    /// @param initial_allocation_size
    ///   Estimated size of the archive, if known.
    ///   Bigger size allow to avoid extra memory reallocations.
    /// @sa
    ///   FinalizeMemory, AddEntryFromFile, AddEntryFromMemory
    virtual void CreateMemory(size_t initial_allocation_size = 0) = 0;

    /// Open archive file for reading.
    ///
    /// @param filename
    ///   File name of the existing archive to open.
    /// @sa
    ///   CreateFile, OpenMemory, ExtractEntryToFileSystem, ExtractEntryToMemory
    virtual void OpenFile(const string& filename) = 0;
    
    /// Open archive located in memory for reading.
    /// @param buf
    ///   Pointer to an archive located in memory. Used only to open already
    ///   existed archive for reading. 
    /// @param size
    ///   Size of the archive.
    /// @sa
    ///   CreateMemory, OpenFile, ExtractEntryToFileSystem, ExtractEntryToMemory
    virtual void OpenMemory(const void* buf, size_t size) = 0;

    /// Close the archive.
    /// 
    /// For file-based archives it flushes all pending output.
    /// All added entries will appears in the archive only after this call.
    /// This method will be automatically called from destructor
    /// if you forgot to call it directly. But note, that if the archive
    /// is created in memory, it will be lost. You should call 
    /// FinalizeMemory() to get it.
    /// @sa
    ///   FinalizeMemory, OpenFile, OpenMemory
    virtual void Close(void) = 0;

    /// Finalize the archive created in memory.
    ///
    /// Return pointer to buffer with created archive and its size.
    /// The only valid operation after this call is Close(). 
    /// @param buf
    ///   Pointer to an archive located in memory.
    /// @param size
    ///   Size of the newly created archive.
    /// @note
    ///   Do not forget to deallocate memory buffer after usage.
    /// @sa
    ///   CreateMemory, Close
    virtual void FinalizeMemory(void** buf, size_t* size) = 0;

    /// Returns the total number of entries in the archive.
    virtual size_t GetNumEntries(void) = 0;

    /// Get detailed information about an archive entry by index.
    ///
    /// @param index
    ///   Zero-based index of entry in the archive.
    /// @param info
    ///   Pointer to entry information structure that will be filled with
    ///   information about entry with specified index.
    /// @note
    ///   Note that the archive can contain multiple versions of the same entry
    ///   (in case if an update was done on it), all of which but the last one
    ///   are to be ignored.
    /// @sa
    ///   CArchiveEntryInfo
    virtual void GetEntryInfo(size_t index, CArchiveEntryInfo* info) = 0;

    /// Check that current archive format have support for specific feature.
    /// @sa CArchive
    virtual bool HaveSupport_Type(CDirEntry::EType type) = 0;
    virtual bool HaveSupport_AbsolutePath(void) = 0;

    /// Extracts an archive entry to file system.
    /// 
    /// @param info
    ///   Entry to extract.
    /// @param dst_path
    ///   Destination path for extracted entry.
    /// @note
    ///   This method do not set last accessed and modified times.
    ///   See CArchive.
    /// @sa 
    ///   ExtractEntryToMemory, ExtractEntryToCallback, CArchiveEntryInfo::GetSize
    virtual void ExtractEntryToFileSystem(const CArchiveEntryInfo& info,
                                          const string& dst_path) = 0;

    /// Extracts an archive file to a memory buffer.
    /// 
    /// Do nothing for entries of other types.
    /// @param info
    ///   Entry to extract.
    /// @param buf
    ///   Memory buffer for extracted data.
    /// @param size
    ///   Size of memory buffer.
    ///   Note, that the buffer size should be big enough to fit whole extracted file.
    /// @sa 
    ///   ExtractEntryToFileSystem, ExtractEntryToCallback, CArchiveEntryInfo::GetSize
    virtual void ExtractEntryToMemory(const CArchiveEntryInfo& info,
                                      void* buf, size_t size) = 0;

    /// Extracts an archive file using user-defined callback to process extracted data.
    ///
    /// Do nothing for entries of other types.
    /// @param info
    ///   Entry to extract.
    /// @param callback
    ///   User callback for processing extracted data on the fly.
    /// @sa 
    ///   ExtractEntryToFileSystem, ExtractEntryToMemory, CArchiveEntryInfo::GetSize
    virtual void ExtractEntryToCallback(const CArchiveEntryInfo& info,
                                        Callback_Write callback) = 0;

    /// Verify entry integrity.
    ///
    /// @param info
    ///   Entry to verify.
    virtual void TestEntry(const CArchiveEntryInfo& info) = 0;

    /// Don't need to be implemented for ZIP format.
    virtual void SkipEntry(const CArchiveEntryInfo& info) = 0;

    /// Add single entry to newly created archive from file system.
    ///
    /// @param info
    ///   Entry information to add. It should have name and type of adding entry at least.
    ///   If added entry is a directory, just add an information about it to archive.
    ///   CArchive process all directories recursively.
    /// @param src_path
    ///   Path to file system object to add.
    /// @param level
    ///   Compression level used to compress file .
    /// @sa
    ///   CreateFile, CreateMemory, AddEntryFromMemory
    virtual void AddEntryFromFileSystem(const CArchiveEntryInfo& info,
                                        const string& src_path, ELevel level) = 0;

    /// Add entry to newly created archive from memory buffer.
    /// 
    /// @param info
    ///   Entry information to add. It should have name and type of adding entry at least.
    /// @param buf
    ///   Memory buffer with data to add.
    /// @param size
    ///   Size of data in memory buffer.
    /// @param level
    ///   Compression level for added data.
    /// @sa
    ///   CreateFile, CreateMemory, AddEntryFromFileSystem
    virtual void AddEntryFromMemory(const CArchiveEntryInfo& info,
                                    void* buf, size_t size, ELevel level) = 0;

protected:
    EDirection m_Mode;       ///< Processing direction (read/write)
    ELocation  m_Location;   ///< Archive location (file/memory)
};


END_NCBI_SCOPE


/* @} */

#endif  /* UTIL_COMPRESS__ARCHIVE___HPP */
