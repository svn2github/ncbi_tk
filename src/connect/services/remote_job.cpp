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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Maxim Didenko
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/blob_storage.hpp>
#include <corelib/rwstream.hpp>

#include <connect/services/grid_rw_impl.hpp>
#include <connect/services/remote_job.hpp>

BEGIN_NCBI_SCOPE

const size_t kMaxBlobInlineSize = 500;
//////////////////////////////////////////////////////////////////////////////
//
static inline CNcbiOstream& s_Write(CNcbiOstream& os, const string& str)
{
    os << str.size() << ' ' << str;
    return os;
}

static inline CNcbiIstream& s_Read(CNcbiIstream& is, string& str)
{
    string::size_type len;
    is >> len;
    vector<char> buf(len+1);
    is.read(&buf[0], len+1);
    str.assign(buf.begin()+1, buf.end());
    return is;
}

//////////////////////////////////////////////////////////////////////////////
//
class CBlobStreamHelper
{
public:
    CBlobStreamHelper(IBlobStorage& storage, string& data, size_t& data_size)
        : m_Storage(storage), m_Data(data), m_DataSize(data_size)
    {
    }

    ~CBlobStreamHelper()
    {
    }

    CNcbiOstream& GetOStream() 
    {
        if (!m_OStream.get()) {
            _ASSERT(!m_IStream.get());
            IWriter* writer = 
                new CStringOrBlobStorageWriter(kMaxBlobInlineSize,
                                               m_Storage,
                                               m_Data);
            m_OStream.reset(new CWStream(writer, 0, 0, CRWStreambuf::fOwnWriter));
        }
        return *m_OStream; 
    }

    CNcbiIstream& GetIStream() 
    {
        if (!m_IStream.get()) {
            _ASSERT(!m_OStream.get());
            IReader* reader = new CStringOrBlobStorageReader(m_Data,
                                                             m_Storage,
                                                             IBlobStorage::eLockWait,
                                                             m_DataSize);
            m_IStream.reset(new CRStream(reader,0,0,CRWStreambuf::fOwnReader));
        }
        return *m_IStream; 
    }
    void Reset()
    {
        m_IStream.reset();
        m_OStream.reset();
    }
private:
    IBlobStorage& m_Storage;
    auto_ptr<CNcbiIstream> m_IStream;
    auto_ptr<CNcbiOstream> m_OStream;
    string& m_Data;
    size_t& m_DataSize;
};
//////////////////////////////////////////////////////////////////////////////
//

class CRemoteAppRequest_Impl
{
public:
    explicit CRemoteAppRequest_Impl(IBlobStorageFactory& factory)
        : m_InBlob(factory.CreateInstance()), m_StdInDataSize(0)
    {
        m_StdIn.reset(new CBlobStreamHelper(*m_InBlob, 
                                            m_InBlobIdOrData,
                                            m_StdInDataSize));
    }

    ~CRemoteAppRequest_Impl() 
    {
    }

    CNcbiOstream& GetStdInForWrite() 
    { 
        return m_StdIn->GetOStream();
    }
    CNcbiIstream& GetStdInForRead() 
    { 
        return m_StdIn->GetIStream();
    }

    void SetCmdLine(const string& cmdline) { m_CmdLine = cmdline; }
    const string& GetCmdLine() const { return m_CmdLine; }

    void AddFileForTransfer(const string& fname) 
    { 
        m_Files.insert(fname); 
    }

    void Serialize(CNcbiOstream& os);
    void Deserialize(CNcbiIstream& is);

    void CleanUp();
    void Reset();

private:
    static CAtomicCounter sm_DirCounter;
    static string sm_TmpDirPath;
    
    auto_ptr<IBlobStorage>   m_InBlob;
    size_t                   m_StdInDataSize;
    mutable auto_ptr<CBlobStreamHelper>   m_StdIn;

    string m_InBlobIdOrData;
    string m_CmdLine;

    string m_TmpDirName;
    typedef set<string> TFiles;
    TFiles m_Files;
};

CAtomicCounter CRemoteAppRequest_Impl::sm_DirCounter;
string CRemoteAppRequest_Impl::sm_TmpDirPath = ".";


void CRemoteAppRequest_Impl::Serialize(CNcbiOstream& os)
{
    m_StdIn->Reset();
    typedef map<string,string> TFmap;
    TFmap file_map;
    ITERATE(TFiles, it, m_Files) {
        const string& fname = *it;
        CFile file(fname);
        string blobid;
        if (file.Exists()) {
            CNcbiIfstream inf(fname.c_str());
            if (inf.good()) {
                CNcbiOstream& of = m_InBlob->CreateOStream(blobid);
                of << inf.rdbuf();
                m_InBlob->Reset();
                file_map[blobid] = fname;
            }
        }
    }

    m_InBlob->Reset();

    s_Write(os, m_CmdLine);
    s_Write(os, m_InBlobIdOrData);

    os << file_map.size() << ' ';
    ITERATE(TFmap, itf, file_map) {
        s_Write(os, itf->first);
        s_Write(os, itf->second);
    }
    Reset();
}
void CRemoteAppRequest_Impl::Deserialize(CNcbiIstream& is)
{
    Reset();

    s_Read(is, m_CmdLine);
    s_Read(is, m_InBlobIdOrData);

    int fcount = 0;
    vector<string> args;
    is >> fcount;
    if ( fcount > 0 )
        NStr::Tokenize(m_CmdLine, " ", args);
    for( int i = 0; i < fcount; ++i) {
        if (i == 0) {
            m_TmpDirName = sm_TmpDirPath + CDirEntry::GetPathSeparator() +
                NStr::UIntToString(sm_DirCounter.Add(1));
            CDir(m_TmpDirName).CreatePath();           
        }
        string blobid, fname;
        s_Read(is, blobid);
        s_Read(is, fname);
        CFile file(fname);
        string nfname = m_TmpDirName + CDirEntry::GetPathSeparator() 
            + file.GetName();
        CNcbiOfstream of(nfname.c_str());
        if (of.good()) {
            CNcbiIstream& is = m_InBlob->GetIStream(blobid);
            of << is.rdbuf();
            m_InBlob->Reset();
            for(vector<string>::iterator it = args.begin();
                it != args.end(); ++it) {
                string& arg = *it;
                SIZE_TYPE pos = NStr::Find(arg, fname);
                if (pos == NPOS)
                    continue;
                if ( (pos == 0 || !isalnum((unsigned char)arg[pos-1]) )
                     && pos + fname.size() == arg.size())
                    arg = NStr::Replace(arg, fname, nfname);
            }
        }
    }
    if ( fcount > 0 ) {
        m_CmdLine = "";
        for(vector<string>::const_iterator it = args.begin();
            it != args.end(); ++it) {
            if (it != args.begin())
                m_CmdLine += ' ';
            m_CmdLine += *it;
        }
    }
}

void CRemoteAppRequest_Impl::CleanUp()
{
    if (!m_TmpDirName.empty()) {
        CDir(m_TmpDirName).Remove();
        m_TmpDirName = "";
    }
}

void CRemoteAppRequest_Impl::Reset()
{
    m_StdIn->Reset();
    m_InBlobIdOrData = "";
    m_StdInDataSize = 0;
    m_CmdLine = "";
    m_Files.clear();
}
CRemoteAppRequest::
CRemoteAppRequest(IBlobStorageFactory& factory)
    : m_Impl(new CRemoteAppRequest_Impl(factory))
{
}

CRemoteAppRequest::~CRemoteAppRequest()
{
}

void CRemoteAppRequest::AddFileForTransfer(const string& fname)
{
    m_Impl->AddFileForTransfer(fname);
}

void CRemoteAppRequest::Send(CNcbiOstream& os)
{
    m_Impl->Serialize(os);    
}


CNcbiOstream& CRemoteAppRequest::GetStdIn()
{
    return m_Impl->GetStdInForWrite();
}
void CRemoteAppRequest::SetCmdLine(const string& cmdline)
{
    m_Impl->SetCmdLine(cmdline);
}

CRemoteAppRequest_Executer::
CRemoteAppRequest_Executer(IBlobStorageFactory& factory)
    : m_Impl(new CRemoteAppRequest_Impl(factory))
{
}
CRemoteAppRequest_Executer::~CRemoteAppRequest_Executer()
{
}

CNcbiIstream& CRemoteAppRequest_Executer::GetStdIn()
{
    return m_Impl->GetStdInForRead();
}
const string& CRemoteAppRequest_Executer::GetCmdLine() const 
{
    return m_Impl->GetCmdLine();
}

void CRemoteAppRequest_Executer::Receive(CNcbiIstream& is)
{
    m_Impl->Deserialize(is);
}

void CRemoteAppRequest_Executer::CleanUp()
{
    m_Impl->CleanUp();
}

//////////////////////////////////////////////////////////////////////////////
//

class CRemoteAppResult_Impl;

class CRemoteAppResult_Impl
{
public:
    explicit CRemoteAppResult_Impl(IBlobStorageFactory& factory)
        : m_OutBlob(factory.CreateInstance()), m_OutBlobSize(0), 
          m_ErrBlob(factory.CreateInstance()), m_ErrBlobSize(0),
          m_RetCode(-1)
    {
        m_StdOut.reset(new CBlobStreamHelper(*m_OutBlob,
                                             m_OutBlobIdOrData,
                                             m_OutBlobSize));
        m_StdErr.reset(new CBlobStreamHelper(*m_ErrBlob,
                                             m_ErrBlobIdOrData,
                                             m_ErrBlobSize));
    }
    ~CRemoteAppResult_Impl()
    {
    }    

    CNcbiOstream& GetStdOutForWrite() 
    { 
        return m_StdOut->GetOStream();
    }
    CNcbiIstream& GetStdOutForRead() 
    { 
        return m_StdOut->GetIStream();
    }

    CNcbiOstream& GetStdErrForWrite() 
    { 
        return m_StdErr->GetOStream();
    }
    CNcbiIstream& GetStdErrForRead() 
    { 
        return m_StdErr->GetIStream();
    }

    int GetRetCode() const { return m_RetCode; }
    void SetRetCode(int ret_code) { m_RetCode = ret_code; }

    void Serialize(CNcbiOstream& os);
    void Deserialize(CNcbiIstream& is);

    void Reset();

private:
    auto_ptr<IBlobStorage> m_OutBlob;
    string m_OutBlobIdOrData;
    size_t m_OutBlobSize;
    mutable auto_ptr<CBlobStreamHelper> m_StdOut;

    auto_ptr<IBlobStorage> m_ErrBlob;
    string m_ErrBlobIdOrData;
    size_t m_ErrBlobSize;
    mutable auto_ptr<CBlobStreamHelper> m_StdErr;
    int m_RetCode;

};

void CRemoteAppResult_Impl::Serialize(CNcbiOstream& os)
{
    m_StdOut->Reset();
    m_StdErr->Reset();
    s_Write(os, m_OutBlobIdOrData);
    s_Write(os, m_ErrBlobIdOrData);
    os << m_RetCode;
    Reset();
}
void CRemoteAppResult_Impl::Deserialize(CNcbiIstream& is)
{
    Reset();
    s_Read(is, m_OutBlobIdOrData);
    s_Read(is, m_ErrBlobIdOrData);
    m_RetCode = -1;
    is >> m_RetCode;
}

void CRemoteAppResult_Impl::Reset()
{
    m_OutBlobIdOrData = "";
    m_OutBlobSize = 0;
    m_StdOut->Reset();

    m_ErrBlobIdOrData = "";
    m_ErrBlobSize = 0;
    m_StdErr->Reset();
    m_RetCode = -1;
}



CRemoteAppResult_Executer::
CRemoteAppResult_Executer(IBlobStorageFactory& factory)
    : m_Impl(new CRemoteAppResult_Impl(factory))
{
}
CRemoteAppResult_Executer::~CRemoteAppResult_Executer()
{
}

CNcbiOstream& CRemoteAppResult_Executer::GetStdOut()
{
    return m_Impl->GetStdOutForWrite();
}
CNcbiOstream& CRemoteAppResult_Executer::GetStdErr()
{
    return m_Impl->GetStdErrForWrite();
}

void CRemoteAppResult_Executer::SetRetCode(int ret_code)
{
    m_Impl->SetRetCode(ret_code);
}

void CRemoteAppResult_Executer::Send(CNcbiOstream& os)
{
    m_Impl->Serialize(os);
}


CRemoteAppResult::
CRemoteAppResult(IBlobStorageFactory& factory)
    : m_Impl(new CRemoteAppResult_Impl(factory))
{
}
CRemoteAppResult::~CRemoteAppResult()
{
}

CNcbiIstream& CRemoteAppResult::GetStdOut()
{
    return m_Impl->GetStdOutForRead();
}
CNcbiIstream& CRemoteAppResult::GetStdErr()
{
    return m_Impl->GetStdErrForRead();
}
int CRemoteAppResult::GetRetCode() const
{
    return m_Impl->GetRetCode();
}

void CRemoteAppResult::Receive(CNcbiIstream& is)
{
    m_Impl->Deserialize(is);
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 6.3  2006/03/16 15:13:59  didenko
 * Remaned CRemoteJob... to CRemoteApp...
 * + Comments
 *
 * Revision 6.2  2006/03/15 17:30:12  didenko
 * Added ability to use embedded NetSchedule job's storage as a job's 
 * input/output data instead of using it as a NetCache blob key. This reduces 
 * a network traffic and increases job submittion speed.
 *
 * Revision 6.1  2006/03/07 17:17:12  didenko
 * Added facility for running external applications throu NetSchedule service
 *
 * ===========================================================================
 */
 
