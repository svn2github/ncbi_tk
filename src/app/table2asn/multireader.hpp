#ifndef TABLE2ASN_MULTIREADER_HPP
#define TABLE2ASN_MULTIREADER_HPP

#include <util/format_guess.hpp>

BEGIN_NCBI_SCOPE

namespace objects
{
class CSeq_entry;
class CSeq_submit;
class CSeq_descr;
class CSeqdesc;
class ILineErrorListener;
class CIdMapper;
class CBioseq;
class CSeq_annot;
};

class CTable2AsnContext;
class CSerialObject;
class CAnnotationLoader;

//  ============================================================================
class CMultiReader
//  ============================================================================
{
public:
   CMultiReader(CTable2AsnContext& context);
   ~CMultiReader();
 
   CFormatGuess::EFormat LoadFile(const string& filename, CRef<objects::CSeq_entry>& entry, CRef<objects::CSeq_submit>& submit);
   void Cleanup(CRef<objects::CSeq_entry>);
   void WriteObject(const CSerialObject&, ostream&);
   void ApplyAdditionalProperties(objects::CSeq_entry& entry);
   void LoadTemplate(CTable2AsnContext& context, const string& ifname);
   void LoadDescriptors(const string& ifname, CRef<objects::CSeq_descr> & out_desc);
   CRef<objects::CSeq_descr> GetSeqDescr(CSerialObject* obj);
   void MergeDescriptors(objects::CSeq_descr & dest, const objects::CSeq_descr & source);
   void MergeDescriptors(objects::CSeq_descr & dest, const objects::CSeqdesc & source);
   void ApplyDescriptors(objects::CSeq_entry & obj, const objects::CSeq_descr & source);
   bool LoadAnnot(objects::CSeq_entry& obj, const string& filename);


protected:
private:
    CFormatGuess::EFormat xReadFile(CNcbiIstream& in, CRef<objects::CSeq_entry>& entry, CRef<objects::CSeq_submit>& submit);
    CRef<objects::CSeq_entry> xReadFasta(CNcbiIstream& instream);
    bool xReadASN1(CFormatGuess::EFormat format, CNcbiIstream& instream, CRef<objects::CSeq_entry>& entry, CRef<objects::CSeq_submit>& submit);
    CRef<objects::CSeq_entry> xReadGFF3(CNcbiIstream& instream);
    CRef<objects::CSeq_entry> xReadGTF(CNcbiIstream& instream);
    void x_PostProcessAnnot(objects::CSeq_entry& entry);
    bool xGetAnnotLoader(CAnnotationLoader& loader, CNcbiIstream& in, const string& filename);

    auto_ptr<CObjectIStream> xCreateASNStream(CFormatGuess::EFormat format, CNcbiIstream& instream);
    CRef<objects::CSeq_entry> CreateNewSeqFromTemplate(const CTable2AsnContext& context, objects::CBioseq& bioseq) const;

    CFormatGuess::EFormat xGetFormat(CNcbiIstream&) const;

    int  m_iFlags;
    string m_AnnotName;
    string m_AnnotTitle;

    CTable2AsnContext& m_context;
};

END_NCBI_SCOPE

#endif
