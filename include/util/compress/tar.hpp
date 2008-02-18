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
 *           Anton Lavrentiev
 *
 * File Description:
 *   @file
 *   Tar archive API.
 *
 *   Supports subset of POSIX.1-1988 (ustar) format.
 *   Old GNU (POSIX 1003.1) and V7 formats are also supported partially.
 *   New archives are created using POSIX (genuine ustar) format, using
 *   GNU extensions for long names/links only when unavoidable.
 *   Can handle no exotics like sparse / contiguous files, special
 *   files (devices, FIFOs), multivolume / incremental archives, etc,
 *   but just regular files, directories, and links:  can extract
 *   both hard- and symlinks, but can store only symlinks.
 *
 */

#include <corelib/ncbifile.hpp>
#include <corelib/reader_writer.hpp>
#include <list>
#include <utility>


/** @addtogroup Compression
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
/// ETarMode --
///
/// Permission bits as defined in tar
///

enum ETarModeBits {
    // Special mode bits
    fTarSetUID    = 04000,   // set UID on execution
    fTarSetGID    = 02000,   // set GID on execution
    fTarSticky    = 01000,   // reserved (sticky bit)
    // File permissions
    fTarURead     = 00400,   // read by owner
    fTarUWrite    = 00200,   // write by owner
    fTarUExecute  = 00100,   // execute/search by owner
    fTarGRead     = 00040,   // read by group
    fTarGWrite    = 00020,   // write by group
    fTarGExecute  = 00010,   // execute/search by group
    fTarORead     = 00004,   // read by other
    fTarOWrite    = 00002,   // write by other
    fTarOExecute  = 00001    // execute/search by other
};
typedef unsigned int TTarMode; // Bitwise OR of ETarModeBits


/////////////////////////////////////////////////////////////////////////////
///
/// CTarException --
///
/// Define exceptions generated by the API.
///
/// CTarException inherits its basic functionality from CCoreException
/// and defines additional error codes for tar archive operations.

class NCBI_XUTIL_EXPORT CTarException : public CCoreException
{
public:
    /// Error types that file operations can generate.
    enum EErrCode {
        eUnsupportedTarFormat,
        eUnsupportedEntryType,
        eNameTooLong,
        eChecksum,
        eBadName,
        eCreate,
        eOpen,
        eRead,
        eWrite,
        eBackup,
        eMemory,
        eRestoreAttrs
    };

    /// Translate from an error code value to its string representation.
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eUnsupportedTarFormat: return "eUnsupportedTarFormat";
        case eUnsupportedEntryType: return "eUnsupportedEntryType";
        case eNameTooLong:          return "eNameTooLong";
        case eChecksum:             return "eChecksum";
        case eBadName:              return "eBadName";
        case eCreate:               return "eCreate";
        case eOpen:                 return "eOpen";
        case eRead:                 return "eRead";
        case eWrite:                return "eWrite";
        case eBackup:               return "eBackup";
        case eMemory:               return "eMemory";
        case eRestoreAttrs:         return "eRestoreAttrs";
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
/// Information about a tar archive entry.

class NCBI_XUTIL_EXPORT CTarEntryInfo
{
public:
    /// Archive entry type.
    enum EType {
        eFile        = CDirEntry::eFile,        ///< Regular file
        eDir         = CDirEntry::eDir,         ///< Directory
        eSymLink     = CDirEntry::eLink,        ///< Symbolic link
        eUnknown     = CDirEntry::eUnknown,     ///< Unknown type
        eHardLink    = eUnknown + 1,            ///< Hard link
        ePAXHeader,                             ///< POSIX extended header
        eGNULongName,                           ///< GNU long name
        eGNULongLink                            ///< GNU long link
    };

    // Constructor
    CTarEntryInfo(Uint8 pos = 0)
        : m_Type(eUnknown), m_Pos(pos)
    {
        memset(&m_Stat, 0, sizeof(m_Stat));
    }

    // No setters -- they are not needed for access by the user, and
    // thus are done directly from CTar for the sake of performance.

    // Getters only!
    EType         GetType(void)             const { return m_Type;          }
    const string& GetName(void)             const { return m_Name;          }
    const string& GetLinkName(void)         const { return m_LinkName;      }
    const string& GetUserName(void)         const { return m_UserName;      }
    const string& GetGroupName(void)        const { return m_GroupName;     }
    time_t        GetModificationTime(void) const { return m_Stat.st_mtime; }
    time_t        GetLastAccessTime(void)   const { return m_Stat.st_atime; }
    time_t        GetCreationTime(void)     const { return m_Stat.st_ctime; }
    Uint8         GetSize(void)             const { return m_Stat.st_size;  }
    TTarMode      GetMode(void)             const; // Raw mode as stored in tar
    void          GetMode(CDirEntry::TMode*            user_mode,
                          CDirEntry::TMode*            group_mode   = 0,
                          CDirEntry::TMode*            other_mode   = 0,
                          CDirEntry::TSpecialModeBits* special_bits = 0) const;
    int           GetUserId(void)           const { return m_Stat.st_uid;   }
    int           GetGroupId(void)          const { return m_Stat.st_gid;   }
    Uint8         GetPosition(void)         const { return m_Pos;           }

private:
    EType        m_Type;       ///< Type
    string       m_Name;       ///< Entry name
    string       m_LinkName;   ///< Link name if type is e{Sym|Hard}Link
    string       m_UserName;   ///< User name
    string       m_GroupName;  ///< Group name (empty string for MSWin)
    struct stat  m_Stat;       ///< Dir-entry compatible info
    Uint8        m_Pos;        ///< Position within archive

    friend class CTar;
};


NCBI_XUTIL_EXPORT ostream& operator << (ostream&, const CTarEntryInfo&);


/// Forward declaration of tar header used internally
struct SHeader;


//////////////////////////////////////////////////////////////////////////////
///
/// CTar class
///
/// (Throws exceptions on most errors.)
/// Note that if a stream constructor was used then CTar can only perform
/// one pass over the archive.  This means that only one full action will
/// succeed (and it the action was to update (e.g. append) the archive, it
/// has to be explicitly followed by Close().  Before next action, you should
/// explicitly reset the stream position to the beginning of the archive
/// for read/update operations or to end of archive for append operations.

class NCBI_XUTIL_EXPORT CTar
{
public:
    /// General flags
    enum EFlags {
        // --- Extract/List/Test ---
        /// Ignore blocks of zeros in archive.
        /// Generally, 2 or more consecutive zero blocks indicate EOF.
        fIgnoreZeroBlocks   = (1<<1),

        // --- Extract/Append/Update ---
        /// Follow symbolic links (instead of storing/extracting them)
        fFollowLinks        = (1<<2),

        // --- Extract --- (NB: fUpdate also applies to Update)
        /// Allow to overwrite destinations with entries from the archive
        fOverwrite          = (1<<3),
        /// Only update entries that are older than those already existing
        fUpdate             = (1<<4) | fOverwrite,
        /// Backup destinations if they exist (all entries including dirs)
        fBackup             = (1<<5) | fOverwrite,
        /// If destination entry exists, it must have the same type as source
        fEqualTypes         = (1<<6),
        /// Create extracted files with the same ownership
        fPreserveOwner      = (1<<7),
        /// Create extracted files with the same permissions
        fPreserveMode       = (1<<8),
        /// Preserve date/times for extracted files
        fPreserveTime       = (1<<9),
        /// Preserve all attributes
        fPreserveAll        = fPreserveOwner | fPreserveMode | fPreserveTime,

        // --- Extract/List ---
        fMaskNocase         = (1<<10),

        // --- Debugging ---
        fDumpBlockHeaders   = (1<<20),

        /// Default flags
        fDefault            = fOverwrite | fPreserveAll
    };
    typedef unsigned int TFlags;  ///< Bitwise OR of EFlags


    /// Constructors
    CTar(const string& filename, size_t blocking_factor = 20);
    /// Stream version does not at all use stream positioning and so
    /// is safe on non-positionable streams, like magnetic tapes :-I
    CTar(CNcbiIos& stream, size_t blocking_factor = 20);

    /// Destructor (finalize the archive if currently open).
    /// @sa
    ///   Close
    virtual ~CTar();


    /// Define a list of entries.
    typedef list< CTarEntryInfo > TEntries;

    /// Define a list of files with sizes.
    typedef list< pair<string, Uint8> > TFiles;


    //------------------------------------------------------------------------
    // Main functions
    //------------------------------------------------------------------------

    /// Create a new empty archive.
    ///
    /// If a file with such name already exists it will be rewritten.
    /// @sa
    ///   Append
    void Create(void);

    /// Close the archive making sure all pending output is flushed.
    ///
    /// Normally, direct call of this method need _not_ intersperse
    /// successive archive manipulations by other methods, as they open
    /// and close the archive automatically as necessary.  Rather, this
    /// call is to make sure the archive is complete earlier than it
    /// usually be done in the destructor of the CTar object.
    /// @sa
    ///   ~CTar
    void Close(void);

    /// Append an entry at the end of the archive that already exists.
    ///
    /// Appended entry can be either a file, a directory, or a symbolic link.
    /// The name is taken with respect to the base directory, if any set.
    /// Adding a directory results in all its files and subdirectories to
    /// get added (examine the return value to find out what has been added).
    /// Note that the final name of an entry may not contain embedded '..'.
    /// Leading slash in the absolute paths will be retained.  The names of
    /// all appended entries will be converted to Unix format (that is, to
    /// have forward slashes in the paths, and drive letter, if any on
    /// MS-Windows, stripped).  All entries will be added at the logical end
    /// (not always EOF) of the archive, when appending to an existing one.
    ///
    /// @return
    ///   A list of the appended entries.
    /// @sa
    ///   Create, Update, SetBaseDir
    auto_ptr<TEntries> Append(const string& name);

    /// Look for more recent copies, if available, of archive members,
    /// and place them at the end of the archive:
    ///
    /// if fUpdate is set in processing flags, only the existing archive
    /// entries (including directories) will be updated;  that is, Update(".")
    /// won't recursively add "." if "." is not the archive member;  it will,
    /// however, do the recursive update should "." be found in the archive.
    ///
    /// if fUpdate is unset, the existing entries will be updated (if their
    /// filesystem counterparts are newer), and inexistent entries will be
    /// added to the archive;  that is, Update(".") will recursively scan "."
    /// to update both existing entries (if newer files found), and also add
    /// new entries for any files/directories, which are currently not in.
    ///
    /// @return
    ///   A list of entries that have been updated.
    /// @sa
    ///   Append, SetBaseDir, SetFlags
    auto_ptr<TEntries> Update(const string& name);

/*
    // Delete an entry from the archive (not for use on magnetic tapes :-)
    void Delete(const string& name);

    // Find file system entries that differ from corresponding
    // entries already in the archive.
    auto_ptr<TEntries> Diff(const string& diff_dir);
*/

    /// Extract the entire archive (into either current directory or
    /// a directory otherwise specified by SetBaseDir()).
    ///
    /// Extract all archive entries, which names match pre-set mask.
    /// @sa
    ///   SetMask, SetBaseDir
    auto_ptr<TEntries> Extract(void);

    /// Get information about all matching archive entries.
    ///
    /// @return
    ///   An array containing information on those archive entries
    ///   which names match pre-set mask.
    /// @sa
    ///   SetMask
    auto_ptr<TEntries> List(void);

    /// Verify archive integrity.
    ///
    /// Read through the archive without actually extracting anything from it.
    void Test(void);

    /// Return archive size as if all specified input entries were put in it.
    /// Note that the return value is not exact but the upper bound of
    /// what the archive size can be expected.  This call does not recurse
    /// into any subdirectries but relies solely upon the information as
    /// passed via the parameter.
    ///
    /// The returned size includes all necessary alignments and padding.
    /// @return
    ///   An upper estimate of archive size given that all specified files
    ///   were stored in it (the actual size may turn out to be smaller).
    Uint8 EstimateArchiveSize(const TFiles& files) const;


    //------------------------------------------------------------------------
    // Utility functions
    //------------------------------------------------------------------------

    /// Get processing flags.
    TFlags GetFlags(void) const;

    /// Set processing flags.
    void   SetFlags(TFlags flags);

    /// Set name mask.
    ///
    /// The set of masks is used to process existing entries in archive,
    /// and apply to list and extract operations only.
    /// If masks are not defined then all archive entries will be processed.
    /// By default, the masks are used case sensitively.  To cancel this and
    /// use the masks case-insensitively SetFlags() can be called with
    /// fMaskIgnoreCase flag set.
    /// @param mask
    ///   Set of masks.
    /// @param if_to_own
    ///   Flag to take ownership on the masks (delete on destruction).
    /// @sa
    //    SetFlags, UnsetMask
    void SetMask(CMask* mask, EOwnership if_to_own = eNoOwnership);

    /// Unset name mask.
    ///
    /// Upon mask reset, all entries become subject to archive processing in
    /// list and extract operations.
    /// @sa
    ///   SetMask
    void UnsetMask(void);

    /// Get base directory to use for files while extracting from/adding to
    /// the archive, and in the latter case used only for relative paths.
    /// @sa
    ///   SetBaseDir
    const string& GetBaseDir(void) const;

    /// Set base directory to use for files while extracting from/adding to
    /// the archive, and in the latter case used only for relative paths.
    /// @sa
    ///   GetBaseDir
    void SetBaseDir(const string& dirname);

    /// Create an IReader, which can extract contents of one named file.
    ///
    /// Tar archive is deemed to be in the specified stream "is", properly
    /// positioned.  The extraction is done as if fFirstOnly flag was set.
    /// @return
    ///   IReader interface to read the file contents with;  0 on error.
    /// @sa
    ///   Extract, SetFlags
    static IReader* Extract(istream& is, const string& name, TFlags flags = 0);

protected:
    /// Archive action
    enum EOpenMode {
        eNone = 0,
        eWO   = 1,
        eRO   = 2,
        eRW   = eRO | eWO
    };
    enum EAction {
        eUndefined =  eNone,
        eList      = (1 << 2) | eRO,
        eAppend    = (1 << 3) | eRW,
        eUpdate    = eList | eAppend,
        eExtract   = (1 << 4) | eRO,
        eTest      = eList | eExtract,
        eCreate    = (1 << 5) | eWO,
        eInternal  = (1 << 6)
    };
    /// IO completion code
    enum EStatus {
        eFailure = -1,
        eSuccess =  0,
        eContinue,
        eZeroBlock,
        eEOF
    };

    // Common part of initialization.
    void x_Init(void);

    // Open/close the archive.
    auto_ptr<TEntries> x_Open(EAction action);
    virtual void x_Close(void);

    // Flush the archive (writing an appropriate EOT if necessary).
    void x_Flush(void);

    // Backspace the archive.
    void x_Backspace(EAction action, size_t blocks);

    // Parse in extended entry information (POSIX) for the next entry.
    EStatus x_ParsePAXHeader(CTarEntryInfo& info, const string& buffer);

    // Read information about next entry in the archive.
    EStatus x_ReadEntryInfo(CTarEntryInfo& info, bool dump = false);

    // Pack either name or linkname into archive entry header.
    bool x_PackName(SHeader* header, const CTarEntryInfo& info, bool link);

    // Write information about entry into the archive.
    void x_WriteEntryInfo(const string& name, const CTarEntryInfo& info);

    // Read the archive and do the requested "action".
    auto_ptr<TEntries> x_ReadAndProcess(EAction action);

    // Process next entry from the archive.
    // If extract == FALSE, then just skip the entry without any processing.
    bool x_ProcessEntry(const CTarEntryInfo& info, bool extract = false,
                        const TEntries* done = 0);

    // Extract an entry from the archive into the file system,
    // and return the entry size (if any) still remaining in the archive.
    bool x_ExtractEntry(const CTarEntryInfo& info, Uint8& size,
                        const CDirEntry* dst, const CDirEntry* src = 0);

    // Restore attributes of an entry on the file system.
    // If 'dst' not specified, then the destination path will be constructed
    // from 'info', and the base directory (if any).  Otherwise, 'dst' will
    // be used "as is", assuming it corresponds to the specified 'info'.
    void x_RestoreAttrs(const CTarEntryInfo& info,
                        const CDirEntry* dst = 0) const;

    // Read/write specified number of bytes from/to the archive.
    const char* x_ReadArchive(size_t& n);
    void        x_WriteArchive(size_t n, const char* buffer = 0);

    // Check path and convert it to an archive name.
    string x_ToArchiveName(const string& path) const;

    // Convert from entry name to path in filesystem.
    string x_ToFilesystemPath(const string& name) const;

    // Append an entry to the archive.
    auto_ptr<TEntries> x_Append(const string&   name,
                                const TEntries* toc = 0);
    // Append a file/symlink entry to the archive.
    void x_AppendFile(const string& file, const CTarEntryInfo& info);

protected:
    string         m_FileName;     ///< Tar archive file name.
    CNcbiFstream*  m_FileStream;   ///< File stream of the archive.
    EOpenMode      m_OpenMode;     ///< What was it opened for.
    CNcbiIos*      m_Stream;       ///< Archive stream (used for all I/O).
    const size_t   m_BufferSize;   ///< Buffer(record) size for I/O operations.
    size_t         m_BufferPos;    ///< Position within the record.
    Uint8          m_StreamPos;    ///< Position in stream (0-based).
    char*          m_BufPtr;       ///< Page unaligned buffer pointer.
    char*          m_Buffer;       ///< I/O buffer (page-aligned).
    TFlags         m_Flags;        ///< Bitwise OR of flags.
    CMask*         m_Mask;         ///< Masks for list/test/extract.
    EOwnership     m_MaskOwned;    ///< Flag to take ownership for m_Mask.
    bool           m_IsModified;   ///< True after at least one write.
    string         m_BaseDir;      ///< Base directory for relative paths.
    const string*  m_Current;      ///< Current entry name being processed.

private:
    // Prohibit assignment and copy
    CTar& operator=(const CTar&);
    CTar(const CTar&);
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
void CTar::Close(void)
{
    x_Flush();
    x_Close();
}

inline
auto_ptr<CTar::TEntries> CTar::Append(const string& name)
{
    x_Open(eAppend);
    return x_Append(name);
}

inline
auto_ptr<CTar::TEntries> CTar::Update(const string& name)
{
    auto_ptr<TEntries> toc = x_Open(eUpdate);
    return x_Append(name, toc.get());
}

inline
auto_ptr<CTar::TEntries> CTar::List(void)
{
    return x_Open(eList);
}

inline
void CTar::Test(void)
{
    x_Open(eTest);
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
    m_Mask      = mask;
    m_MaskOwned = if_to_own;
}

inline
void CTar::UnsetMask(void)
{
    if ( m_MaskOwned ) {
        delete m_Mask;
    }
    m_Mask = 0;
}

inline
const string& CTar::GetBaseDir(void) const
{
    return m_BaseDir;
}


END_NCBI_SCOPE


/* @} */


#endif  /* UTIL_COMPRESS__TAR__HPP */
