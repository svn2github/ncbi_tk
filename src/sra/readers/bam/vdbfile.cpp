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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   Access to VDB KFile, that supports remote cached file access
 *
 */

#include <ncbi_pch.hpp>
#include <sra/readers/bam/vdbfile.hpp>

#include <kfs/file.h>
#include <vfs/manager.h>
#include <vfs/path.h>
#include <strstream>

#ifndef NCBI_THROW2_FMT
# define NCBI_THROW2_FMT(exception_class, err_code, message, extra)     \
    throw NCBI_EXCEPTION2(exception_class, err_code, FORMAT(message), extra)
#endif


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;


DEFINE_BAM_REF_TRAITS(VFSManager, );
DEFINE_BAM_REF_TRAITS(VPath, );
DEFINE_BAM_REF_TRAITS(KFile, const);


void CBamVFSManager::x_Init()
{
    if ( rc_t rc = VFSManagerMake(x_InitPtr()) ) {
        NCBI_THROW2_FMT(CBamException, eInitFailed,
                        "CBamVFSManager: "
                        "cannot get VFSManager", rc);
    }
}


#ifdef NCBI_OS_MSWIN
static inline
bool s_HasWindowsDriveLetter(const string& s)
{
    // first symbol is letter, and second symbol is colon (':')
    return s.length() >= 2 && isalpha(s[0]&0xff) && s[1] == ':';
}
#endif


bool CBamVDBPath::IsPlainAccession(const string& acc_or_path)
{
#ifdef NCBI_OS_MSWIN
    return !s_HasWindowsDriveLetter(acc_or_path) &&
        acc_or_path.find_first_of("/\\") == NPOS;
#else
    return acc_or_path.find('/') == NPOS;
#endif
}


bool CBamVDBPath::HasSchemaPrefix(const string& path)
{
    // Convert Windows path with drive letter
    // C:\Users\Public -> /C/Users/Public
    if (path[0] == 'h' &&
        (NStr::StartsWith(path, "http://") ||
         NStr::StartsWith(path, "https://"))) {
        return true;
    }
    if (path[0] == 'f' &&
        (NStr::StartsWith(path, "ftp://"))) {
        return true;
    }
    return false;
}


bool CBamVDBPath::IsSysPath(const string& acc_or_path)
{
    if (IsPlainAccession(acc_or_path)) {
        return false;
    }
    if (HasSchemaPrefix(acc_or_path)) {
        return false;
    }
    return true;
}


void CBamVDBPath::x_Init(const CBamVFSManager& mgr, const string& path)
{
    if (IsSysPath(path)) {
        if (rc_t rc = VFSManagerMakeSysPath(mgr, x_InitPtr(), path.c_str())) {
            NCBI_THROW2_FMT(CBamException, eInitFailed,
                "CBamVDBPath(" << path << "): "
                "cannot create VPath", rc);
        }
    }
    else {
        if (rc_t rc = VFSManagerMakePath(mgr, x_InitPtr(), path.c_str())) {
            NCBI_THROW2_FMT(CBamException, eInitFailed,
                "CBamVDBPath(" << path << "): "
                "cannot create VPath", rc);
        }
    }
}


CTempString CBamVDBPath::GetString() const
{
    String str;
    if ( rc_t rc = VPathGetPath(*this, &str) ) {
        NCBI_THROW2_FMT(CBamException, eInitFailed,
                        "CBamVDBPath::GetString(): "
                        "VPathGetPath() failed", rc);
    }
    return CTempString(str.addr, str.size);
}


CBamVDBFile::CBamVDBFile(const string& path)
{
    CBamVFSManager mgr;
    x_Init(mgr, CBamVDBPath(mgr, path));
}


void CBamVDBFile::x_Init(const CBamVFSManager& mgr, const CBamVDBPath& path)
{
    if ( rc_t rc = VFSManagerOpenFileRead(mgr, x_InitPtr(), path) ) {
        NCBI_THROW2_FMT(CBamException, eInitFailed,
                        "CBamVDBFile("<<path.GetString()<<"): "
                        "cannot open KFile", rc);
    }
    m_Path = path;
}


size_t CBamVDBFile::GetSize() const
{
    uint64_t size;
    if ( rc_t rc = KFileSize(*this, &size) ) {
        NCBI_THROW2_FMT(CBamException, eInitFailed,
                        "CBamVDBFile::GetSize(): "
                        "cannot get size for "<<m_Path.GetString(), rc);
    }
    if ( size_t(size) != size ) {
        NCBI_THROW_FMT(CBamException, eInitFailed,
                       "CBamVDBFile::GetSize(): "
                       "size of "<<m_Path.GetString()<<" is too big: "<<size);
    }
    return size_t(size);
}


size_t CBamVDBFile::Read(size_t pos, char* buffer, size_t buffer_size)
{
    size_t nread;
    if ( rc_t rc = KFileRead(*this, pos, buffer, buffer_size, &nread) ) {
        NCBI_THROW2_FMT(CBamException, eInitFailed,
                        "CBamVDBFile::Read(): "
                        "read failed from "<<m_Path.GetString(), rc);
    }
    return nread;
}


size_t CBamVDBFile::ReadAll(size_t pos, char* buffer, size_t buffer_size)
{
    size_t nread;
    if ( rc_t rc = KFileReadAll(*this, pos, buffer, buffer_size, &nread) ) {
        NCBI_THROW2_FMT(CBamException, eInitFailed,
                        "CBamVDBFile::ReadAll(): "
                        "read failed from "<<m_Path.GetString(), rc);
    }
    return nread;
}


void CBamVDBFile::ReadExactly(size_t pos, char* buffer, size_t buffer_size)
{
    if ( rc_t rc = KFileReadExactly(*this, pos, buffer, buffer_size) ) {
        NCBI_THROW2_FMT(CBamException, eInitFailed,
                        "CBamVDBFile::ReadExactly(): "
                        "read failed from "<<m_Path.GetString(), rc);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
