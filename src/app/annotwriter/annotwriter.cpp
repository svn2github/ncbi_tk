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
* Author:  Aaron Ucko, Mati Shomrat, NCBI
*
* File Description:
*   fasta-file generator application
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/annot_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <dbapi/driver/drivers.hpp>

#include <objtools/format/flat_file_config.hpp>
#include <objtools/format/flat_file_generator.hpp>
#include <objtools/format/flat_expt.hpp>
#include <objects/seqset/gb_release_file.hpp>

#include <objects/entrez2/Entrez2_boolean_element.hpp>
#include <objects/entrez2/Entrez2_boolean_reply.hpp>
#include <objects/entrez2/Entrez2_boolean_exp.hpp>
#include <objects/entrez2/Entrez2_eval_boolean.hpp>
#include <objects/entrez2/Entrez2_id_list.hpp>
#include <objects/entrez2/entrez2_client.hpp>

#include <objtools/cleanup/cleanup.hpp>

#include <util/compress/zlib.hpp>
#include <util/compress/stream.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/writers/writer_exception.hpp>
#include <objtools/writers/gtf_writer.hpp>
#include <objtools/writers/gff3_writer.hpp>
#include <objtools/writers/gff3flybase_writer.hpp>
#include <objtools/writers/wiggle_writer.hpp>
#include <objtools/writers/bed_track_record.hpp>
#include <objtools/writers/bed_feature_record.hpp>
#include <objtools/writers/bed_writer.hpp>
#include <objtools/writers/bedgraph_writer.hpp>
#include <objtools/writers/vcf_writer.hpp>
#include <objtools/writers/gvf_writer.hpp>
#include <objtools/writers/aln_writer.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//#define CANCELER_CODE
#if defined(CANCELER_CODE)
#include <conio.h>
//  ============================================================================
class TestCanceler: public ICanceled
//  ============================================================================
{
    bool IsCanceled() const { 
        return kbhit(); 
    };
};
#endif

//  ----------------------------------------------------------------------------
class CAnnotWriterApp : public CNcbiApplication
//  ----------------------------------------------------------------------------
{
public:
    void Init();
    int Run();

private:
    CNcbiOstream* xInitOutputStream(
        const CArgs& );
    CObjectIStream* xInitInputStream(
        const CArgs& args );
    CWriterBase* xInitWriter(
        const CArgs&,
        CNcbiOstream* );
    
    bool xTryProcessInputId(
        const CArgs& );
    bool xTryProcessInputFile(
        const CArgs& );

    bool xTryProcessSeqEntry(
        CScope&,
        CObjectIStream&);
    bool xTryProcessSeqAnnot(
        CScope&,
        CObjectIStream&);
    bool xTryProcessBioseq(
        CScope&,
        CObjectIStream&);
    bool xTryProcessBioseqSet(
        CScope&,
        CObjectIStream&);
    bool xTryProcessSeqAlign(
        CScope&,
        CObjectIStream&);
    bool xTryProcessSeqAlignSet(
        CScope&,
        CObjectIStream&);
    bool xTryProcessSeqSubmit(
        CScope&,
        CObjectIStream&);
        
    unsigned int xGffFlags( 
        const CArgs& );

    TSeqPos xGetFrom(
        const CArgs& args) const;

    TSeqPos xGetTo(
        const CArgs& args) const;

    string xAssemblyName() const;
    string xAssemblyAccession() const;
    void xTweakAnnotSelector(
        const CArgs&,
        SAnnotSelector&);

    CRef<CObjectManager> m_pObjMngr;
    CRef<CScope> m_pScope;
    CRef<CWriterBase> m_pWriter;
};

//  ----------------------------------------------------------------------------
void CAnnotWriterApp::Init()
//  ----------------------------------------------------------------------------
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(
        GetArguments().GetProgramBasename(),
        "Convert ASN.1 to alternative file formats",
        false);
    
    // input
    {{
        arg_desc->AddOptionalKey( "i", "InputFile", 
            "Input file name", CArgDescriptions::eInputFile );
        arg_desc->AddOptionalKey( "id", "InputID",
            "Input ID (accession or GI number)", CArgDescriptions::eString );
    }}

    // format
    {{
    arg_desc->AddDefaultKey(
        "format", 
        "STRING",
        "Output format",
        CArgDescriptions::eString, 
        "gff" );
    arg_desc->SetConstraint(
        "format", 
        &(*new CArgAllow_Strings, 
            "gvf",
            "gff", "gff2", 
            "gff3",
            "gtf", 
            "wig", "wiggle",
            "bed",
            "bedgraph",
            "vcf",
            "aln" ) );
    }}

    // parameters
    {{
    arg_desc->AddDefaultKey(
        "assembly-name",
        "STRING",
        "Assembly name",
        CArgDescriptions::eString,
        "" );
    arg_desc->AddDefaultKey(
        "assembly-accn",
        "STRING",
        "Assembly accession",
        CArgDescriptions::eString,
        "" );
    arg_desc->AddDefaultKey(
        "default-method",
        "STRING",
        "GFF3 default method",
        CArgDescriptions::eString,
        "" );
    }}
    

    // output
    {{ 
        arg_desc->AddOptionalKey( "o", "OutputFile", 
            "Output file name", CArgDescriptions::eOutputFile );
        arg_desc->AddOptionalKey("from", "From",
            "Beginning of range", CArgDescriptions::eInteger );
        arg_desc->AddOptionalKey("to", "To",
            "End of range", CArgDescriptions::eInteger );
    }}

    //  flags
    {{
    arg_desc->AddFlag(
        "structibutes",
        "limit attributes to inter feature relationships",
        true );
    arg_desc->AddFlag(
        "skip-gene-features",
        "GTF only: Do not emit gene features (for GTF 2.2 compliancy)",
        true );
    arg_desc->AddFlag(
        "skip-exon-numbers",
        "GTF only: For exon features, do not emit exon numbers",
        true );
    arg_desc->AddFlag(
        "skip-headers",
        "GFF dialects: Do not generate GFF headers",
        true );
    arg_desc->AddFlag(
        "full-annots",
        "GFF dialects: Inherit annots from components, unless prohibited",
        true );
    arg_desc->AddFlag(
        "use-extra-quals",
        "GFF dialects: Restore original GFF type and attributes, if possible",
        true );
    arg_desc->AddFlag(
        "flybase",
        "GFF3 only: Use Flybase interpretation of the GFF3 spec",
        true );
    arg_desc->AddFlag(
        "no-sort", 
        "GFF3 alignments only: Do not sort alignments when fetching from ID",
        true);
    arg_desc->AddFlag(
        "micro-introns",
        "GFF3 only: Incorporate micro introns",
        true);
    arg_desc->AddFlag(
        "binary",
        "input file is binary ASN.1",
        true );
    }}
    
    SetupArgDescriptions(arg_desc.release());
}

//  ----------------------------------------------------------------------------
int CAnnotWriterApp::Run()
//  ----------------------------------------------------------------------------
{
    const CArgs& args = GetArgs();

	CONNECT_Init(&GetConfig());
    m_pObjMngr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*m_pObjMngr);
    m_pScope.Reset(new CScope(*m_pObjMngr));
    m_pScope->AddDefaults();

    try {
        CNcbiOstream* pOs = xInitOutputStream(args);
        m_pWriter.Reset(xInitWriter(args, pOs));
#if defined(CANCELER_CODE)
        TestCanceler canceller;
        m_pWriter->SetCanceler(&canceller);
#endif
        if (args["from"]) {
            m_pWriter->SetRange().SetFrom(xGetFrom(args));
        }

        if (args["to"]) {
            m_pWriter->SetRange().SetTo(xGetTo(args));
        }
        
        if (xTryProcessInputId(args)) {
            pOs->flush();
            return 0;
        }
        if (xTryProcessInputFile(args)) {
            pOs->flush();
            return 0;
        }
    }
    catch(CObjWriterException& e) {
        cerr << e.GetMsg() << endl;
        return 1;
    }
    catch (CException& e) {
        cerr << "Internal runtime error " << e.GetErrCodeString() << ": " 
            << e.GetMsg() << " (" << e.GetFile() << ":" << e.GetLine() << ")";
        return 1;
    }
    return 0;
}

//  -----------------------------------------------------------------------------
CNcbiOstream* CAnnotWriterApp::xInitOutputStream(
    const CArgs& args)
//  -----------------------------------------------------------------------------
{
    if (args["o"]) {
        try {
            return &args["o"].AsOutputFile();
        }
        catch(...) {
            NCBI_THROW(CObjWriterException, eArgErr, 
                "xInitOutputStream: Unable to create output file \"" +
                args["o"].AsString() +
                "\"");
        }
    }
    return &cout;
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::xTryProcessInputId(
    const CArgs& args)
//  -----------------------------------------------------------------------------
{
    if(!args["id"]) {
        return false;
    }
    CSeq_id_Handle seqh = CSeq_id_Handle::GetHandle(args["id"].AsString());
    CBioseq_Handle bsh = m_pScope->GetBioseqHandle(seqh);
    if (!args["skip-headers"]) {
        m_pWriter->WriteHeader();
    }
    m_pWriter->WriteBioseqHandle(bsh);
    m_pWriter->WriteFooter();
    return true;
}    

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::xTryProcessInputFile(
    const CArgs& args)
//  -----------------------------------------------------------------------------
{
    auto_ptr<CObjectIStream> pIs;

    if(args["id"]) {
        return false;
    }

    pIs.reset( xInitInputStream( args ) );

    set<TTypeInfo> knownTypes, matchingTypes;
    knownTypes.insert(CSeq_entry::GetTypeInfo());
    knownTypes.insert(CSeq_annot::GetTypeInfo());
    knownTypes.insert(CBioseq::GetTypeInfo());
    knownTypes.insert(CBioseq_set::GetTypeInfo());
    knownTypes.insert(CSeq_align::GetTypeInfo());
    knownTypes.insert(CSeq_align_set::GetTypeInfo());
    knownTypes.insert(CSeq_submit::GetTypeInfo());

    while (!pIs->EndOfData()) {
        matchingTypes = pIs->GuessDataType(knownTypes);
        if (matchingTypes.empty()) {
           NCBI_THROW(CObjWriterException, eBadInput, 
                "xTryProcessInputFile: Unidentifiable input object");
        }
        if (matchingTypes.size() > 1) {
           NCBI_THROW(CObjWriterException, eBadInput, 
                "xTryProcessInputFile: Ambiguous input object");
        }

        const TTypeInfo typeInfo = *matchingTypes.begin();
        if (typeInfo == CSeq_entry::GetTypeInfo()) {
            if (!xTryProcessSeqEntry(*m_pScope, *pIs)) {
               NCBI_THROW(CObjWriterException, eBadInput, 
                   "xTryProcessInputFile: Unable to process Seq-entry object");
            }
            continue;
        }
        if (typeInfo == CSeq_annot::GetTypeInfo()) {
            if (!xTryProcessSeqAnnot(*m_pScope, *pIs)) {
               NCBI_THROW(CObjWriterException, eBadInput, 
                   "xTryProcessInputFile: Unable to process Seq-annot object");
            }
            continue;
        }

        if (typeInfo == CBioseq::GetTypeInfo()) {
            if (!xTryProcessBioseq(*m_pScope, *pIs)) {
               NCBI_THROW(CObjWriterException, eBadInput, 
                   "xTryProcessInputFile: Unable to process Bioseq object");
            }
            continue;
        }
        if (typeInfo == CBioseq_set::GetTypeInfo()) {
            if (!xTryProcessBioseqSet(*m_pScope, *pIs)) {
               NCBI_THROW(CObjWriterException, eBadInput, 
                   "xTryProcessInputFile: Unable to process Bioseq-set object");
            }
            continue;
        }
        if (typeInfo == CSeq_align::GetTypeInfo()) {
            if (!xTryProcessSeqAlign(*m_pScope, *pIs)) {
               NCBI_THROW(CObjWriterException, eBadInput, 
                   "xTryProcessInputFile: Unable to process Seq-align object");
            }
            continue;
        }
        if (typeInfo == CSeq_align_set::GetTypeInfo()) {
            if (!xTryProcessSeqAlignSet(*m_pScope, *pIs)) {
               NCBI_THROW(CObjWriterException, eBadInput, 
                   "xTryProcessInputFile: Unable to process Seq-align-set object");
            }
            continue;
        }
        if (typeInfo == CSeq_submit::GetTypeInfo()) {
            if (!xTryProcessSeqSubmit(*m_pScope, *pIs)) {
               NCBI_THROW(CObjWriterException, eBadInput, 
                   "xTryProcessInputFile: Unable to process Seq-submit object");
            }
            continue;
        }
    }
    pIs.reset();
    return true;
}


//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::xTryProcessSeqSubmit(
    CScope& scope,
    CObjectIStream& istr)
//  -----------------------------------------------------------------------------
{
    typedef CSeq_submit::C_Data::TEntrys ENTRIES;
    typedef CSeq_submit::C_Data::TAnnots ANNOTS;

    CSeq_submit submit;
    try {
        istr.Read(ObjectInfo(submit));
    }
    catch (CException&) {
        return false;
    }

    CSeq_submit::TData& data = submit.SetData();
    if (data.IsEntrys()) {
        if (!GetArgs()["skip-headers"]) {
            m_pWriter->WriteHeader();
        }
        ENTRIES& entries = data.SetEntrys();
        for (ENTRIES::iterator cit = entries.begin(); cit != entries.end(); ++cit) {
            CSeq_entry& entry = **cit;
            CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);
            m_pWriter->WriteSeqEntryHandle(seh, xAssemblyName(), xAssemblyAccession());
            m_pWriter->WriteFooter();
            scope.RemoveEntry(entry);
        }
        return true;
    }
    if (data.IsAnnots()) {
        if (!GetArgs()["skip-headers"]) {
            m_pWriter->WriteHeader();
        }
        ANNOTS& annots = data.SetAnnots();
        for (ANNOTS::iterator cit = annots.begin(); cit != annots.end(); ++cit) {
            m_pWriter->WriteAnnot(**cit, xAssemblyName(), xAssemblyAccession());
            m_pWriter->WriteFooter();
        }
        return true;
    }
    return true;
}


//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::xTryProcessSeqEntry(
    CScope& scope,
    CObjectIStream& istr)
//  -----------------------------------------------------------------------------
{
    CSeq_entry seq_entry;
    try {
        istr.Read(ObjectInfo(seq_entry));
    }
    catch (CException&) {
        return false;
    }

    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry( seq_entry );
    if (!GetArgs()["skip-headers"]) {
        m_pWriter->WriteHeader();
    }
    m_pWriter->WriteSeqEntryHandle(seh, xAssemblyName(), xAssemblyAccession());
    m_pWriter->WriteFooter();
    scope.RemoveEntry(seq_entry);
    return true;
}


//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::xTryProcessSeqAnnot(
    CScope& /*scope*/,
    CObjectIStream& istr)
//  -----------------------------------------------------------------------------
{
    CRef<CSeq_annot> pSeqAnnot(new CSeq_annot);
    try {
        istr.Read(ObjectInfo(*pSeqAnnot));
        //istr.Read(ObjectInfo(*pSeqAnnot), CObjectIStream::eNoFileHeader);
    }
    catch (CException&) {
        return false;
    }
   
    if (!GetArgs()["skip-headers"]) {
        m_pWriter->WriteHeader();
    }
    m_pWriter->WriteAnnot(*pSeqAnnot, xAssemblyName(), xAssemblyAccession());
    m_pWriter->WriteFooter();
    return true;
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::xTryProcessBioseq(
    CScope& scope,
    CObjectIStream& istr)
//  -----------------------------------------------------------------------------
{

    CBioseq bioseq;
    try {
        istr.Read(ObjectInfo(bioseq));
    }
    catch (CException&) {
        return false;
    }
    CBioseq_Handle bsh = scope.AddBioseq(bioseq);
    if (!GetArgs()["skip-headers"]) {
        m_pWriter->WriteHeader();
    }
    m_pWriter->WriteBioseqHandle(bsh, xAssemblyName(), xAssemblyAccession() );
    m_pWriter->WriteFooter();
    scope.RemoveBioseq(bsh);
    return true;
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::xTryProcessBioseqSet(
    CScope& scope,
    CObjectIStream& istr)
//  -----------------------------------------------------------------------------
{
    CBioseq_set seq_set;
    try {
        istr.Read(ObjectInfo(seq_set));
    }
    catch (CException&) {
        return false;
    }
    CSeq_entry se;
    se.SetSet( seq_set );
    scope.AddTopLevelSeqEntry( se );
    if (!GetArgs()["skip-headers"]) {
        m_pWriter->WriteHeader();
    }
    m_pWriter->WriteSeqEntryHandle( 
        scope.GetSeq_entryHandle(se), xAssemblyName(), xAssemblyAccession());
    m_pWriter->WriteFooter();
    scope.RemoveEntry( se );
    return true;
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::xTryProcessSeqAlign(
    CScope& /*scope*/,
    CObjectIStream& istr)
//  -----------------------------------------------------------------------------
{
    CSeq_align align;
    try {
        istr.Read(ObjectInfo(align));
    }
    catch (CException&) {
        return false;
    }
    if (!GetArgs()["skip-headers"]) {
        m_pWriter->WriteHeader();
    }
    m_pWriter->WriteAlign( align, xAssemblyName(), xAssemblyAccession());

    m_pWriter->WriteFooter();
    return true;
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::xTryProcessSeqAlignSet(
    CScope& /*scope*/,
    CObjectIStream& istr)
//  -----------------------------------------------------------------------------
{


    CSeq_align_set align_set;
    try {
        istr.Read(ObjectInfo(align_set));
    }
    catch (CException&) {
        return false;
    }
    if(!GetArgs()["skip-headers"]) {
        m_pWriter->WriteHeader();
    }
    bool first = true;
    ITERATE(CSeq_align_set::Tdata, align_iter, align_set.Get()) {
        if(first && !GetArgs()["skip-headers"]) {
            m_pWriter->WriteAlign( **align_iter, xAssemblyName(), xAssemblyAccession());
            first = false;
        } else {
            m_pWriter->WriteAlign( **align_iter);
        }
    }
    m_pWriter->WriteFooter();
    return true;
}

//  -----------------------------------------------------------------------------
CObjectIStream* CAnnotWriterApp::xInitInputStream( 
    const CArgs& args )
//  -----------------------------------------------------------------------------
{
    ESerialDataFormat serial = eSerial_AsnText;
    if (args["binary"]) {
        serial = eSerial_AsnBinary;
    }
    CNcbiIstream* pInputStream = &NcbiCin;
		
    bool bDeleteOnClose = false;
    if (args["i"]) {
    	const char* infile = args["i"].AsString().c_str();
        pInputStream = new CNcbiIfstream(infile, ios::binary);
        bDeleteOnClose = true;
    }
    CObjectIStream* pI = CObjectIStream::Open( 
        serial, *pInputStream, (bDeleteOnClose ? eTakeOwnership : eNoOwnership));
    if (!pI) {
        NCBI_THROW(CObjWriterException, eArgErr, 
            "annotwriter: Unable to open input file");
    }
    return pI;
}

//  -----------------------------------------------------------------------------
unsigned int CAnnotWriterApp::xGffFlags(
    const CArgs& args )
//  -----------------------------------------------------------------------------
{
   unsigned int eFlags = CGff2Writer::fNormal;
    if (args["structibutes"]) {
        eFlags |= CGtfWriter::fStructibutes;
    }
    if (args["skip-gene-features"]) {
        eFlags |= CGtfWriter::fNoGeneFeatures;
    }
    if ( args["skip-exon-numbers"] ) {
        eFlags |= CGtfWriter::fNoExonNumbers;
    }
    if ( args["use-extra-quals"] ) {
        eFlags |= CGff3Writer::fExtraQuals;
    }
    if (args["micro-introns"]) {
        eFlags |= CGff3Writer::fMicroIntrons;
    }
    return eFlags;
}

//  ----------------------------------------------------------------------------
TSeqPos CAnnotWriterApp::xGetFrom(
    const CArgs& args) const
//  ----------------------------------------------------------------------------
{
    if (args["from"]) {
        return static_cast<TSeqPos>(args["from"].AsInteger()-1);
    }

    return CRange<TSeqPos>::GetWholeFrom();
}


//  ----------------------------------------------------------------------------
TSeqPos CAnnotWriterApp::xGetTo(
    const CArgs& args) const
//  ----------------------------------------------------------------------------
{
    if (args["to"]) {
        return static_cast<TSeqPos>(args["to"].AsInteger()-1);
    }

    return CRange<TSeqPos>::GetWholeTo();
}


//  ----------------------------------------------------------------------------
CWriterBase* CAnnotWriterApp::xInitWriter(
    const CArgs& args,
    CNcbiOstream* pOs )
//  ----------------------------------------------------------------------------
{
    if (!m_pScope  || !pOs) {
        NCBI_THROW(CObjWriterException, eArgErr, 
            "xInitWriter: Writer object needs valid scope and output stream");
    }

    const string strFormat = args["format"].AsString();
    if (strFormat == "gff"  ||  strFormat == "gff2") { 
        CGff2Writer* pWriter = new CGff2Writer(*m_pScope, *pOs, xGffFlags(args));
        xTweakAnnotSelector(args, pWriter->SetAnnotSelector());
        return pWriter;
    }

    if (strFormat == "gff3") { 
        const bool sortAlignments = args["no-sort"] ? false : true;
        if (args["flybase"]) {
            CGff3FlybaseWriter* pWriter = new CGff3FlybaseWriter(*m_pScope, *pOs, sortAlignments);
            return pWriter;
        }
        CGff3Writer* pWriter = new CGff3Writer(*m_pScope, *pOs, xGffFlags(args), sortAlignments);
        xTweakAnnotSelector(args, pWriter->SetAnnotSelector());
        if (args["default-method"]) {
            pWriter->SetDefaultMethod(args["default-method"].AsString());
        }
        return pWriter;
    }
    if (strFormat == "gtf") {
        CGtfWriter* pWriter = new CGtfWriter(*m_pScope, *pOs, xGffFlags(args));
        xTweakAnnotSelector(args, pWriter->SetAnnotSelector());
        return pWriter;
    }
    if (strFormat == "gvf") { 
        CGvfWriter* pWriter = new CGvfWriter( *m_pScope, *pOs, xGffFlags(args));
        xTweakAnnotSelector(args, pWriter->SetAnnotSelector());
        return pWriter;
    }
    if (strFormat == "wiggle"  ||  strFormat == "wig") {
        return new CWiggleWriter(*m_pScope, *pOs, 0);
    }
    if (strFormat == "bed") {
        return new CBedWriter(*m_pScope, *pOs, 12);
    }
    if (strFormat == "bedgraph") {
        return new CBedGraphWriter(*m_pScope, *pOs, 4);
    }
    if (strFormat == "vcf") {
        return new CVcfWriter(*m_pScope, *pOs);
    }
    if (strFormat == "aln") {
        return new CAlnWriter(*m_pScope, *pOs, 0);
    }
    NCBI_THROW(CObjWriterException, eArgErr, 
        "xInitWriter: No suitable writer for format \"" + strFormat + "\"");
}

//  ----------------------------------------------------------------------------
string CAnnotWriterApp::xAssemblyName() const
//  ----------------------------------------------------------------------------
{
    return GetArgs()["assembly-name"].AsString();
}

//  ----------------------------------------------------------------------------
string CAnnotWriterApp::xAssemblyAccession() const
//  ----------------------------------------------------------------------------
{
    return GetArgs()["assembly-accn"].AsString();
}

//  ---------------------------------------------------------------------------
void CAnnotWriterApp::xTweakAnnotSelector(
    const CArgs& args,
    SAnnotSelector& sel)
//  ---------------------------------------------------------------------------
{
   if (args["full-annots"]) {
        sel.SetResolveDepth(kMax_Int);
        sel.SetResolveAll().SetAdaptiveDepth();
    }
    else {
        sel.SetAdaptiveDepth(false);
        sel.SetExactDepth(true);
        sel.SetResolveDepth(0);
    }
}

END_NCBI_SCOPE
USING_NCBI_SCOPE;

//  ===========================================================================
int main(int argc, const char** argv)
//  ===========================================================================
{
	return CAnnotWriterApp().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}
