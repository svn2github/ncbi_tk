/*  $Id$
* ===========================================================================
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
* ===========================================================================
*/

#include <corelib/ncbistre.hpp>
#include <objects/objmgr/reader_id1.hpp>

#include <serial/enumvalues.hpp>
#include <serial/iterator.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasn.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/serial.hpp>

#include "strstreambuf.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

streambuf *CId1Reader::SeqrefStreamBuf(const CSeq_id &seqId, unsigned conn)
{
  try
  {
    return x_SeqrefStreamBuf(seqId, conn);
  }
  catch(const CIOException&)
  {
    Reconnect(conn);
    throw;
  }
}

streambuf *CId1Reader::x_SeqrefStreamBuf(const CSeq_id &seqId, unsigned conn)
{
  CConn_ServiceStream *server = m_Pool[conn % m_Pool.size()];
  {
    CID1server_request id1_request;
    id1_request.SetGetgi().Assign(seqId);
    CObjectOStreamAsnBinary server_output(*server);
    server_output << id1_request;
    server_output.Flush();
  }

  auto_ptr<CObjectIStream> server_input(CObjectIStream::Open(eSerial_AsnBinary, *server, false));
  CID1server_back id1_reply;
  *server_input >> id1_reply;

  int gi = 0;
  if(id1_reply.IsGotgi())
     gi = id1_reply.GetGotgi();
  else
    {
      LOG_POST("SeqId is not found");
      seqId.WriteAsFasta(cout);
    }

  auto_ptr<strstream> ss(new strstream);

  if(gi == 0)
    return new CStrStreamBuf(ss.release());

  {
    CID1server_request id1_request1;
    id1_request1.SetGetgirev(gi);
    CObjectOStreamAsnBinary server_output(*server);
    server_output << id1_request1;
    server_output.Flush();
  }
  
  *server_input >> id1_reply;
  
  for(CTypeConstIterator<CSeq_hist_rec> it = ConstBegin(id1_reply); it;  ++it)
  {
    int number = 0;
    string dbname;
    int lgi = 0;
    
    iterate(CSeq_hist_rec::TIds, it2, it->GetIds())
    {
      if((*it2)->IsGi())
      {
        lgi = (*it2)->GetGi();
      }
      else if((*it2)->IsGeneral())
      {
        dbname = (*it2)->GetGeneral().GetDb();
        const CObject_id& tag = (*it2)->GetGeneral().GetTag();
        if(tag.IsStr())
        {
          number = NStr::StringToInt(tag.GetStr());
        }
        else
        {
          number = (tag.GetId());
        }
      }
    }
    if(gi!=lgi) continue;
    CId1Seqref id1Seqref;
    id1Seqref.Gi() = gi;
    id1Seqref.Sat() = dbname;
    id1Seqref.SatKey() = number;
    id1Seqref.Flag() = 0;
    *ss << id1Seqref;
    break; // mk - get only the first one
  }
  
  return new CStrStreamBuf(ss.release());
}

void CId1Seqref::Save(ostream &os) const
{
  CSeqref::Save(os);
  os << m_Gi << m_Sat << m_SatKey;
}

void CId1Seqref::Restore(istream &is)
{
  CSeqref::Restore(is);
  is >> m_Gi >> m_Sat >> m_SatKey;
}

CSeqref* CId1Seqref::Dup() const
{
  return new CId1Seqref(*this);
}

CSeqref *CId1Reader::RetrieveSeqref(istream &is)
{
  CId1Seqref *id1Seqref = new CId1Seqref;
  id1Seqref->m_Reader = this;
  is >> *id1Seqref;
  return id1Seqref;
}

struct CId1StreamBuf : public streambuf
{
  CId1StreamBuf(CId1Seqref &id1Seqref, CConn_ServiceStream *server);
  ~CId1StreamBuf();
  CT_INT_TYPE underflow();
  void Close() { m_Server = 0; }
  
  CId1Seqref&                   m_Id1Seqref;
  CT_CHAR_TYPE                  buffer[1024];
  CConn_ServiceStream          *m_Server;
};

CId1StreamBuf::CId1StreamBuf(CId1Seqref &id1Seqref, CConn_ServiceStream *server) :
m_Id1Seqref(id1Seqref)
{
  
  CRef<CID1server_maxcomplex> params(new CID1server_maxcomplex);
  params->SetGi(m_Id1Seqref.Gi());
  params->SetEnt(m_Id1Seqref.SatKey());
  params->SetSat(m_Id1Seqref.Sat());
  
  CID1server_request id1_request;
  id1_request.SetGetsefromgi(*params);

  STimeout tmout;
  tmout.sec = 20;
  tmout.usec = 0;

  m_Server = server;
  {
    CObjectOStreamAsnBinary server_output(*m_Server);
    server_output << id1_request;
    server_output.Flush();
  }
}

CId1StreamBuf::~CId1StreamBuf()
{}

CT_INT_TYPE CId1StreamBuf::underflow()
{
  if(! m_Server)
    return CT_EOF;

  size_t n = CIStream::Read(*m_Server, buffer, sizeof(buffer));
  setg(buffer, buffer, buffer + n);
  return n == 0 ? CT_EOF : CT_TO_INT_TYPE(buffer[0]);
}

streambuf *CId1Seqref::BlobStreamBuf(int a, int b, const CBlobClass &c, unsigned conn)
{
  try
  {
    return x_BlobStreamBuf(a, b, c, conn);
  }
  catch(const CIOException&)
  {
    m_Reader->Reconnect(conn);
    throw;
  }
}

streambuf *CId1Seqref::x_BlobStreamBuf(int, int, const CBlobClass &, unsigned conn)
{
  auto_ptr<CId1StreamBuf> b(new CId1StreamBuf(*this, m_Reader->m_Pool[conn % m_Reader->m_Pool.size()]));
  return b.release();
}

CSeq_entry *CId1Blob::Seq_entry()
{
  auto_ptr<CObjectIStream> objStream(CObjectIStream::Open(eSerial_AsnBinary, m_IStream, false));
  CID1server_back id1_reply;
  bool clenup=false;
  try
    {
      *objStream >> id1_reply;
      static_cast<CId1StreamBuf *>(m_IStream.rdbuf())->Close();
    }
  catch ( exception& e)
    {
      LOG_POST( "TROUBLE: reader_id1: can not read Seq-entry from reply: " << e.what() );
      clenup=true;
    }
  if(id1_reply.IsGotseqentry())
    m_Seq_entry = &id1_reply.SetGotseqentry();
  else if(id1_reply.IsGotdeadseqentry())
    m_Seq_entry = &id1_reply.SetGotdeadseqentry();
  if(clenup)
    m_Seq_entry=0;
  
  return m_Seq_entry;
}

CBlob *CId1Seqref::RetrieveBlob(istream &is)
{
  return new CId1Blob(is);
}

void CId1Blob::Save(ostream &os) const
{
  CBlob::Save(os);
}

void CId1Blob::Restore(istream &is)
{
  CBlob::Restore(is);
}

char* CId1Seqref::print(char *s,int size) const
{
  CNcbiOstrstream ostr(s,size);
  ostr << "SeqRef(" << Sat() << "," << SatKey () << "," << Gi() << ")" ;
  s[ostr.pcount()]=0;
  return s;
}

char* CId1Seqref::printTSE(char *s,int size) const
{
  CNcbiOstrstream ostr(s,size);
  ostr << "TSE(" << Sat() << "," << SatKey () << ")" ;
  s[ostr.pcount()]=0;
  return s;
}

int CId1Seqref::Compare(const CSeqref &seqRef,EMatchLevel ml) const
{
  const CId1Seqref *p = dynamic_cast<const CId1Seqref*>(& seqRef);
  if(p==0) throw runtime_error("Attempt to compare seqrefs from different sources");
  if(ml==eContext) return 0;

  //cout << "Compare" ; print(); cout << " vs "; p->print(); cout << endl;
    
  if(Sat() < p->Sat())  return -1;
  if(Sat() > p->Sat())  return 1;
  // Sat() == p->Sat()
  if(SatKey() < p->SatKey())  return -1;
  if(SatKey() > p->SatKey())  return 1;
  // blob == p->blob
  //cout << "Same TSE" << endl;
  if(ml==eTSE) return 0;
  if(Gi() < p->Gi())  return -1;
  if(Gi() > p->Gi())  return 1;
  //cout << "Same GI" << endl;
  return 0;
}

CId1Reader::CId1Reader(unsigned noConn)
{
#if !defined(_REENTRANT)
  noConn=1;
#endif
  for(unsigned i = 0; i < noConn; ++i)
    m_Pool.push_back(NewID1Service());
}

CId1Reader::~CId1Reader()
{
  for(unsigned i = 0; i < m_Pool.size(); ++i)
    delete m_Pool[i];
}

size_t CId1Reader::GetParallelLevel(void) const
{
  return m_Pool.size();
}

void CId1Reader::SetParallelLevel(size_t size)
{
  size_t poolSize = m_Pool.size();
  for(size_t i = size; i < poolSize; ++i)
    delete m_Pool[i];

  m_Pool.resize(size);

  for(size_t i = poolSize; i < size; ++i)
    m_Pool[i] = NewID1Service();
}

CConn_ServiceStream *CId1Reader::NewID1Service()
{
    _TRACE("CId1Reader("<<this<<")->NewID1Service()");
  STimeout tmout;
  tmout.sec = 20;
  tmout.usec = 0;
  return new CConn_ServiceStream("ID1", fSERV_Any, 0, 0, &tmout);
}

void CId1Reader::Reconnect(size_t conn)
{
    _TRACE("Reconnect("<<conn<<")");
  conn = conn % m_Pool.size();
  delete m_Pool[conn];
  m_Pool[conn] = NewID1Service();
}

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* $Log$
* Revision 1.26  2003/02/04 18:53:36  dicuccio
* Include file clean-up
*
* Revision 1.25  2002/12/26 20:53:25  dicuccio
* Minor tweaks to relieve compiler warnings in MSVC
*
* Revision 1.24  2002/12/26 16:39:24  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.23  2002/11/18 19:48:43  grichenk
* Removed "const" from datatool-generated setters
*
* Revision 1.22  2002/07/25 15:01:51  grichenk
* Replaced non-const GetXXX() with SetXXX()
*
* Revision 1.21  2002/05/09 21:40:59  kimelman
* MT tuning
*
* Revision 1.20  2002/05/06 03:28:47  vakatov
* OM/OM1 renaming
*
* Revision 1.19  2002/05/03 21:28:10  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.18  2002/04/17 19:52:02  kimelman
* fix: no gi found
*
* Revision 1.17  2002/04/09 16:10:56  ucko
* Split CStrStreamBuf out into a common location.
*
* Revision 1.16  2002/04/08 20:52:26  butanaev
* Added PUBSEQ reader.
*
* Revision 1.15  2002/03/29 02:47:05  kimelman
* gbloader: MT scalability fixes
*
* Revision 1.14  2002/03/27 20:23:50  butanaev
* Added connection pool.
*
* Revision 1.13  2002/03/26 23:31:08  gouriano
* memory leaks and garbage collector fix
*
* Revision 1.12  2002/03/26 18:48:58  butanaev
* Fixed bug not deleting streambuf.
*
* Revision 1.11  2002/03/26 17:17:02  kimelman
* reader stream fixes
*
* Revision 1.10  2002/03/26 15:39:25  kimelman
* GC fixes
*
* Revision 1.9  2002/03/25 18:12:48  grichenk
* Fixed bool convertion
*
* Revision 1.8  2002/03/25 17:49:13  kimelman
* ID1 failure handling
*
* Revision 1.7  2002/03/25 15:44:47  kimelman
* proper logging and exception handling
*
* Revision 1.6  2002/03/22 21:50:21  kimelman
* bugfix: avoid history rtequest for nonexistent sequence
*
* Revision 1.5  2002/03/21 19:14:54  kimelman
* GB related bugfixes
*
* Revision 1.4  2002/03/21 01:34:55  kimelman
* gbloader related bugfixes
*
* Revision 1.3  2002/03/20 04:50:13  kimelman
* GB loader added
*
* Revision 1.2  2002/01/16 18:56:28  grichenk
* Removed CRef<> argument from choice variant setter, updated sources to
* use references instead of CRef<>s
*
* Revision 1.1  2002/01/11 19:06:22  gouriano
* restructured objmgr
*
* Revision 1.7  2001/12/17 21:38:24  butanaev
* Code cleanup.
*
* Revision 1.6  2001/12/13 00:19:25  kimelman
* bugfixes:
*
* Revision 1.5  2001/12/12 21:46:40  kimelman
* Compare interface fix
*
* Revision 1.4  2001/12/10 20:08:02  butanaev
* Code cleanup.
*
* Revision 1.3  2001/12/07 21:24:59  butanaev
* Interface development, code beautyfication.
*
* Revision 1.2  2001/12/07 16:43:58  butanaev
* Fixed includes.
*
* Revision 1.1  2001/12/07 16:10:23  butanaev
* Switching to new reader interfaces.
*
* Revision 1.3  2001/12/06 20:37:05  butanaev
* Fixed timeout problem.
*
* Revision 1.2  2001/12/06 18:06:22  butanaev
* Ported to linux.
*
* Revision 1.1  2001/12/06 14:35:23  butanaev
* New streamable interfaces designed, ID1 reimplemented.
*
*/
