#ifndef UTIL_COMPRESS__TAR__HPP
#define UTIL_COMPRESS__TAR__HPP

/*  $Id$
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
 * Authors:  Vladimir Ivanov
 *
 * File Description:  TAR archive API
 *                    Now supported only POSIX.1-1988 (ustar) format.
 *                    GNU tar format is supported partially.
 *                    New archives creates using GNU format.
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_mask.hpp>

/** @addtogroup Compression
 *
 * @{
 */

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
/// CTarException --
///
/// Define exceptions generated by TAR API.
///
/// CTarException inherits its basic functionality from CCoreException
/// and defines additional error codes for TAR-archive operations.

class NCBI_XUTIL_EXPORT CTarException : public CCoreException
{
public:
    /// Error types that file operations can generate.
    enum EErrCode {
        eUnsupportedTarFormat,
        eUnsupportedEntryType,
        eBadName,
        eLongName,
        eCRC,
        eCreate,
        eOpen,
        eRead,
        eWrite,
        eMemory
    };

    /// Translate from an error code value to its string representation.
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eUnsupportedTarFormat: return "eUnsupportedTarFormat";
        case eUnsupportedEntryType: return "eUnsupportedEntryType";
        case eBadName:              return "eBadName";
        case eLongName:             return "eTooLongName";
        case eCRC:                  return "eCRC";
        case eCreate:               return "eCreate";
        case eOpen:                 return "eOpen";
        case eRead:                 return "eRead";
        case eWrite:                return "eWrite";
        case eMemory:               return "eMemory";
        default:                    return CException::GetErrCodeString();
        }
    }
    // Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT(CTarException, CCoreException);
};


//////////////////////////////////////////////////////////////////////////////
///
/// CTarEntryInfo class
///
/// Store information about TAR archive entry

class NCBI_XUTIL_EXPORT CTarEntryInfo
{
public:
    /// Which entry type.
    enum EType {
        eFile        = CDirEntry::eFile,    ///< Regular file
        eDir         = CDirEntry::eDir,     ///< Directory
        eLink        = CDirEntry::eLink,    ///< Symbolic link
        eUnknown     = CDirEntry::eUnknown, ///< Unknown type
        eGNULongName = eUnknown + 1,        ///< GNU long name
        eGNULongLink = eUnknown + 2         ///< GNU long link
    };

    // Constructor
    CTarEntryInfo(void)
        : m_Type(eUnknown), m_Size(0) {}

    // Setters
    void SetName(const string& name)     { m_Name = name; }
    void SetType(EType type)             { m_Type = type; }
    void SetSize(Int8 size)              { m_Size = (streamsize)size; }
    void SetLinkName(const string& name) { m_LinkName = name; }

    // Getters
    string GetName(void) const     { return m_Name; }
    EType  GetType(void) const     { return m_Type; }
    Int8   GetSize(void) const     { return m_Size; }
    string GetLinkName(void) const { return m_LinkName; }

private:
    string      m_Name;       //< Name of file
    string      m_LinkName;   //< Name of linked file if type is eLink
    EType       m_Type;       //< Type
    streamsize  m_Size;       //< File size (or 0)

    friend class CTar;
};


//////////////////////////////////////////////////////////////////////////////
///
/// CTar class
///
/// Throw exceptions on error.
/// Note that if the stream constructor was used, that CTar can take only
/// one pass along archive. This means that only one action will be success.
/// Before second action, you should set up a stream pointer to beginning of
/// archive yourself, if it is possible.

class NCBI_XUTIL_EXPORT CTar
{
public:
    /// General flags
    enum EFlags {
        // create
        //fDereferenceLinks    = 0x001,   ///< Don't put in archive symlinks; put the files they point to

        // list / extract 
        fIgnoreZeroBlocks    = 0x002,   ///< Ignore blocks of zeros in archive (normally mean EOF)
        fKeepOldFiles        = 0x004,   ///< Keep existing files; don't overwrite them from archive
        //fRestoreOwner        = 0x010,   ///< Create extracted files with the same ownership
        //fRestorePermissions  = 0x020,   ///< Create extracted files with the same permissions
        //fRestoreTimestamp    = 0x040,   ///< Restore timestamp for extracted files
        //fRestoreAll          = fRestoreOwner | fRestorePermissions | fRestoreTimestamp,

        // default flags
        fDefault             = 0
    };
    typedef int TFlags;  ///< Binary OR of "EFlags"


    /// Constructors
    CTar(const string& file_name);
    CTar(CNcbiIos& stream);

    /// Destructor
    virtual ~CTar(void);

    /// Define a vector of pointers to entries.
    typedef vector< AutoPtr<CTarEntryInfo> > TEntries;


    //------------------------------------------------------------------------
    // Main functions
    //------------------------------------------------------------------------

    /// Create a new empty archive
    ///
    /// If file with such name already exists it will be rewritten.
    void Create(void);

    /// Append entries to the end of an archive that already exists.
    ///
    /// Append entries can be directories and files. Entries names cannot
    /// contains '..'. Leading slash for absolute pathes wil be removed.
    /// All names will be converted to Unix format.
    /// The entry will be added to the end of archive.
    void Append(const string& entry_name);

/*
    /// Only append files that are newer than copy in archive.
    ///
    /// Add more recent copies of archive members to the end of an
    /// archive, if they exist.
    void Update();

    // delete from the archive (not for use on magnetic tapes :-))
    void Delete();

    // Add one or more pre-existing archives to the end of another archive.
    void Concatenate();

    // Find differences between entries in archive and their counterparts
    // in the file system
    TEntries Diff(const string& diff_dir);
*/

    /// Extract archive to specified directory.
    ///
    /// Extract all archive entries which names matches specified masks.
    /// @param dst_dir
    ///   Directory to extract files.
    /// @sa SetMask,  UnsetMask
    void Extract(const string& dst_dir);

    /// Get information about archive entries.
    ///
    /// @return
    ///   An array containing all archive entries which names matches
    ///   specified masks.
    /// @sa SetMask
    TEntries List(void);

    /// Test archive integrity.
    /// 
    /// Emulate extracting files from archive without creating it on a disk.
    /// @sa SetMask
    void Test(void);


    //------------------------------------------------------------------------
    // Utility functions
    //------------------------------------------------------------------------

    /// Get flags.
    TFlags GetFlags(void) const;

    /// Set flags.
    void SetFlags(TFlags flags);

    /// Set name mask.
    ///
    /// Use this set of masks to process entries which names matches
    /// this masks while trying to list/test/extract entries from TAR archive.
    /// If masks is not defined that process all archive entries.
    /// @param mask
    ///   Set of masks.
    /// @param if_to_own
    ///   Flag to take ownership on the masks (delete on destruction).
    void SetMask(CMask *mask, EOwnership if_to_own = eNoOwnership);

    /// Unset used name masks.
    ///
    /// Means that all entries in the archive will be processed.
    void UnsetMask();

    /// Get base directory to seek added files
    string GetBaseDir(void);

    /// Set base directory to seek added files
    ///
    /// Used for files that have relative path.
    void SetBaseDir(const string& dir_name);

protected:
    /// File archive open mode
    enum EOpenMode {
        eCreate,
        eRead,
        eUpdate,
        eUnknown
    };
    enum EStatus {
        eSuccess = 0,
        eFailure,
        eEOF,
        eZeroBlock
    };
    enum EAction {
        eList,
        eExtract,
        eTest
    };
    
    // Open/close file
    void  x_Open(EOpenMode mode);
    void  x_Close(void);
    
    // Read information about next entry in the TAR archive
    EStatus x_ReadEntryInfo(CTarEntryInfo& info);

    // Write information about entry into TAR archive
    void x_WriteEntryInfo(const string& entry_name, CDirEntry::EType type);

    // Reader. Read archive and process some "action".
    void x_ReadAndProcess(EAction action, void *data = 0);

    // Process next entry from archive accordingly to specified action.
    // If do_process == TRUE, just skip entry in the stream.
    void x_ProcessEntry(const CTarEntryInfo& info, bool do_process,
                        EAction action, void *data = 0);

    // Increase absolute position in the stream
    void x_IncreaseStreamPos(streamsize size);

    // Read/write specified number of bytes from/to stream
    streamsize x_ReadStream(char *buffer, streamsize n);
    void       x_WriteStream(char *buffer, streamsize n);

    // Check path and convert it to archive name
    string x_ToArchiveName(const string& path);

    // Append dir entry to archive
    void x_Append(const string& entry_name);
    void x_AppendFile(const string& entry_name);

protected:
    string         m_FileName;       ///< TAR archive file name
    CNcbiIos*      m_Stream;         ///< Achive stream (used for I/O)
    CNcbiFstream*  m_FileStream;     ///< File archive stream
    EOpenMode      m_FileStreamMode; ///< File stream open mode
    streampos      m_StreamPos;      ///< Current position in m_Stream
    streamsize     m_BufferSize;     ///< Buffer size for IO operations
    char*          m_Buffer;         ///< Pointer to working I/O buffer
    TFlags         m_Flags;          ///< Bitwise OR of flags
    CMask*         m_Mask;           ///< Masks for list/test/extract
    EOwnership     m_MaskOwned;      ///< Flag to take ownership for m_Mask
    string         m_BaseDir;        ///< Base directory to seek added files without full pathes
                   
    string         m_LongName;       ///< Previously defined long name
    string         m_LongLinkName;   ///< Previously defined long link name
};


//////////////////////////////////////////////////////////////////////////////
//
// Inline methods
//

inline
void CTar::Create(void)
{
    x_Open(eCreate);
}

inline
void CTar::Append(const string& entry_name)
{
    x_Open(eUpdate);
    x_Append(entry_name);
}

inline
CTar::TEntries CTar::List(void)
{
    TEntries entries;
    x_ReadAndProcess(eList, &entries);
    return entries;
}

inline
void CTar::Extract(const string& dst_dir)
{
    x_ReadAndProcess(eExtract, (void*)&dst_dir);
}

inline
void CTar::Test(void)
{
    x_ReadAndProcess(eTest);
}

inline
CTar::TFlags CTar::GetFlags(void) const
{
    return m_Flags;
}

inline
void CTar::SetFlags(TFlags flags)
{
    m_Flags = flags;
}

inline
void CTar::SetMask(CMask *mask, EOwnership if_to_own)
{
    UnsetMask();
    m_Mask = mask;
    m_MaskOwned = if_to_own;
}

inline
void CTar::UnsetMask()
{
    // Delete owned mask
    if ( m_MaskOwned ) {
        delete m_Mask;
        m_Mask = 0;
    }
}

inline
string CTar::GetBaseDir(void)
{
    return m_BaseDir;
}

inline
void CTar::SetBaseDir(const string& dir_name)
{
    m_BaseDir = CDirEntry::AddTrailingPathSeparator(dir_name);
}

inline 
streamsize CTar::x_ReadStream(char *buffer, streamsize n)
{
    streamsize nread = m_Stream->rdbuf()->sgetn(buffer, n);
    x_IncreaseStreamPos(nread);
    return nread;
}

inline 
void CTar::x_WriteStream(char *buffer, streamsize n)
{
    streamsize nwrite = m_Stream->rdbuf()->sputn(buffer, n);
    if ( nwrite != n ) {
        NCBI_THROW(CTarException, eWrite, "Cannot write to archive");
    }
    x_IncreaseStreamPos(nwrite);
}

inline 
void CTar::x_IncreaseStreamPos(streamsize size)
{
    m_StreamPos += size;
}


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2005/01/31 14:23:35  ivanov
 * Added class CTarEntryInfo to store information about TAR entry.
 * Added CTar methods:           Create, Append, List, Test.
 * Added CTar utility functions: GetFlags/SetFlags, SetMask/UnsetMask,
 *                               GetBaseDir/SetBaseDir.
 *
 * Revision 1.2  2004/12/14 17:55:48  ivanov
 * Added GNU tar long name support
 *
 * Revision 1.1  2004/12/02 17:46:14  ivanov
 * Initial draft revision
 *
 * ===========================================================================
 */

#endif  /* UTIL_COMPRESS__TAR__HPP */
