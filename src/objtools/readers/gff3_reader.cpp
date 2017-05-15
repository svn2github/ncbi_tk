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
 * Author:  Frank Ludwig
 *
 * File Description:
 *   GFF file reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/stream_utils.hpp>

#include <util/static_map.hpp>
#include <util/line_reader.hpp>

#include <serial/iterator.hpp>
#include <serial/objistrasn.hpp>

// Objects includes
#include <objects/general/Int_fuzz.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Dbtag.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annot_id.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/so_map.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objtools/readers/read_util.hpp>
#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/gff2_reader.hpp>
#include <objtools/readers/gff3_reader.hpp>
#include <objtools/readers/gff2_data.hpp>
#include <objtools/error_codes.hpp>

#include <algorithm>

//#include "gff3_data.hpp"

#define NCBI_USE_ERRCODE_X   Objtools_Rd_RepMask

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

unsigned int CGff3Reader::msGenericIdCounter = 0;

//  ----------------------------------------------------------------------------
string CGff3Reader::xNextGenericId()
//  ----------------------------------------------------------------------------
{
    return string("generic") + NStr::IntToString(msGenericIdCounter++);
}

//  ----------------------------------------------------------------------------
bool CGff3ReadRecord::AssignFromGff(
    const string& strRawInput )
//  ----------------------------------------------------------------------------
{
    if (!CGff2Record::AssignFromGff(strRawInput)) {
        return false;
    }
    if (m_strType == "pseudogene") {
        m_strType = "gene";
        m_Attributes["pseudo"] = "true";
        return true;
    }
    if (m_strType == "pseudogenic_transcript") {
        m_strType = "transcript";
        m_Attributes["pseudo"] = "true";
        return true;
    }        
    if (m_strType == "pseudogenic_tRNA") {
        m_strType = "tRNA";
        m_Attributes["pseudo"] = "true";
        return true;
    }
    if (m_strType == "pseudogenic_rRNA") {
        m_strType = "rRNA";
        m_Attributes["pseudo"] = "true";
        return true;
    }
    if (m_strType == "pseudogenic_exon") {
        m_strType = "exon";
        return true;
    }
    if (m_strType == "pseudogenic_CDS") {
        m_strType = "CDS";
        m_Attributes["pseudo"] = "true";
        return true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
string CGff3ReadRecord::x_NormalizedAttributeKey(
    const string& strRawKey )
//  ---------------------------------------------------------------------------
{
    string strKey = CGff2Record::xNormalizedAttributeKey( strRawKey );
    if ( 0 == NStr::CompareNocase( strRawKey, "ID" ) ) {
        return "ID";
    }
    if ( 0 == NStr::CompareNocase( strKey, "Name" ) ) {
        return "Name";
    }
    if ( 0 == NStr::CompareNocase( strKey, "Alias" ) ) {
        return "Alias";
    }
    if ( 0 == NStr::CompareNocase( strKey, "Parent" ) ) {
        return "Parent";
    }
    if ( 0 == NStr::CompareNocase( strKey, "Target" ) ) {
        return "Target";
    }
    if ( 0 == NStr::CompareNocase( strKey, "Gap" ) ) {
        return "Gap";
    }
    if ( 0 == NStr::CompareNocase( strKey, "Derives_from" ) ) {
        return "Derives_from";
    }
    if ( 0 == NStr::CompareNocase( strKey, "Note" ) ) {
        return "Note";
    }
    if ( 0 == NStr::CompareNocase( strKey, "Dbxref" )  ||
        0 == NStr::CompareNocase( strKey, "Db_xref" ) ) {
        return "Dbxref";
    }
    if ( 0 == NStr::CompareNocase( strKey, "Ontology_term" ) ) {
        return "Ontology_term";
    }
    return strKey;
}

//  ----------------------------------------------------------------------------
CGff3Reader::CGff3Reader(
    unsigned int uFlags,
    const string& name,
    const string& title ):
//  ----------------------------------------------------------------------------
    CGff2Reader( uFlags, name, title )
{
    CGff2Record::ResetId();
}

//  ----------------------------------------------------------------------------
CGff3Reader::~CGff3Reader()
//  ----------------------------------------------------------------------------
{
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::IsInGenbankMode() const
//  ----------------------------------------------------------------------------
{
    return (m_iFlags & CGff3Reader::fGenbankMode);
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::x_UpdateFeatureCds(
    const CGff2Record& gff,
    CRef<CSeq_feat> pFeature)
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_feat> pAdd = CRef<CSeq_feat>(new CSeq_feat);
    if (!x_FeatureSetLocation(gff, pAdd)) {
        return false;
    }
    pFeature->SetLocation().Add(pAdd->GetLocation());
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::x_UpdateAnnotFeature(
    const CGff2Record& record,
    CRef< CSeq_annot > pAnnot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    CRef< CSeq_feat > pFeature(new CSeq_feat);

    string type = record.Type();
    NStr::ToLower(type);
    if (type == "exon" || type == "five_prime_utr" || type == "three_prime_utr") {
        return xUpdateAnnotExon(record, pFeature, pAnnot, pEC);
    }
    if (type == "cds") {
        return xUpdateAnnotCds(record, pFeature, pAnnot, pEC);
    }
    if (type == "gene") {
        return xUpdateAnnotGene(record, pFeature, pAnnot, pEC);
    }
    if (type == "mrna") {
        return xUpdateAnnotMrna(record, pFeature, pAnnot, pEC);
    }
    return xUpdateAnnotGeneric(record, pFeature, pAnnot, pEC);
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::xVerifyExonLocation(
    const string& mrnaId,
    const CGff2Record& exon,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    map<string,CRef<CSeq_interval> >::const_iterator cit = mMrnaLocs.find(mrnaId);
    if (cit == mMrnaLocs.end()) {
        return false;
    }
    const CSeq_interval& containingInt = cit->second.GetObject();
    const CRef<CSeq_loc> pContainedLoc = exon.GetSeqLoc(m_iFlags);
    const CSeq_interval& containedInt = pContainedLoc->GetInt();
    if (containedInt.GetFrom() < containingInt.GetFrom()) {
        return false;
    }
    if (containedInt.GetTo() > containingInt.GetTo()) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::xUpdateAnnotExon(
    const CGff2Record& record,
    CRef<CSeq_feat> pFeature,
    CRef<CSeq_annot> pAnnot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    list<string> parents;
    if (record.GetAttribute("Parent", parents)) {
        for (list<string>::const_iterator it = parents.begin(); it != parents.end(); 
                ++it) {
            const string& parentId = *it; 
            if (!xVerifyExonLocation(parentId, record, pEC)) {
                //create a free agent "exon" feature
                if (!record.InitializeFeature(m_iFlags, pFeature)) {
                    return false;
                }
                CRef<CSeq_feat> pParent;
                if (!xGetParentFeature(*pFeature, pParent)  ||  
                        !pParent->GetData().IsGene()) {
//                if (!xGetParentFeature(*pFeature, pParent)) {
                    AutoPtr<CObjReaderLineException> pErr(
                        CObjReaderLineException::Create(
                        eDiag_Error,
                        0,
                        "Bad data line: Exon record referring to non-existing mRNA or gene parent.",
                        ILineError::eProblem_FeatureBadStartAndOrStop));
                    ProcessError(*pErr, pEC);
                    return false;
                }
                if (! xAddFeatureToAnnot(pFeature, pAnnot)) {
                    return false;
                }
                return true;
            }
            IdToFeatureMap::iterator fit = m_MapIdToFeature.find(parentId);
            if (fit != m_MapIdToFeature.end()) {
                CRef<CSeq_feat> pParent = fit->second;
                if (!record.UpdateFeature(m_iFlags, pParent)) {
                    return false;
                }
            }
        }
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CGff3Reader::xUpdateAnnotCds(
    const CGff2Record& record,
    CRef<CSeq_feat> pFeature,
    CRef<CSeq_annot> pAnnot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    if (!xVerifyCdsParents(record)) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Bad data line: CDS record with bad parent assignments.",
            ILineError::eProblem_GeneralParsingError) );
        ProcessError(*pErr, pEC);
        return false;
    }

    list<string> parents;
    record.GetAttribute("Parent", parents);
    map<string, string> impliedCdsFeats;

    // Preliminary:
    //  We do not support multiparented CDS features in -genbank mode yet.
    if (IsInGenbankMode()  &&  parents.size() > 1){
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Unsupported: CDS record with multiple parents.",
            ILineError::eProblem_GeneralParsingError) );
        ProcessError(*pErr, pEC);
        return false;
    }

    // Step 1:
    // Locations of parent mRNAs are constructed on the fly, by joining in the 
    //  locations of child exons and CDSs as we discover them. Do this
    //  first.
    // IDs for CDS features are derived from the IDs of their parent features.
    //  Generate a list of CDS IDs this record pertains to as we cycle through
    //  the parent list.
    //
    for (list<string>::const_iterator it = parents.begin(); it != parents.end(); 
            ++it) {
        string parentId = *it;
        bool parentIsGene = false;
        //update parent location:
        IdToFeatureMap::iterator featIt = m_MapIdToFeature.find(parentId);
        if (featIt != m_MapIdToFeature.end()) {
            CRef<CSeq_feat> pParent = featIt->second;
            if (pParent->GetData().IsGene()) {
                parentIsGene = true;
            }
            if (!record.UpdateFeature(m_iFlags, pParent)) {
                return false;
            }
            //rw-143:
            // if parent type is miscRNA then change it to mRNA:
            if (pParent->GetData().IsRna()  &&  
                    pParent->GetData().GetRna().GetType()  ==  CRNA_ref::eType_other) {
                pParent->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
            }
        }

        //generate applicable CDS ID:
        string siblingId("cds");
        if (!record.GetAttribute("ID", siblingId)  ||  !parentIsGene) {
            siblingId = string("cds:") + parentId;
        }
        impliedCdsFeats[siblingId] = parentId;
    }
    // deal with unparented cds
    if (parents.empty()) {
        string cdsId;
        if (!record.GetAttribute("ID", cdsId)) {
            if (IsInGenbankMode()) {
                cdsId = xNextGenericId();
            }
            else {
                cdsId = "cds";
            }
        }
        impliedCdsFeats[cdsId] = "";
    }

    // Step 2:
    // For every sibling CDS feature, look if there is already a feature with that
    //  ID under construction.
    // If there is, use record to update feature under construction.
    // If there isn't, use record to initialize a brand new feature.
    //
    for (map<string, string>::iterator featIt = impliedCdsFeats.begin(); 
            featIt != impliedCdsFeats.end(); ++featIt) {
        string cdsId = featIt->first;
        string parentId = featIt->second;
        IdToFeatureMap::iterator idIt = m_MapIdToFeature.find(cdsId);
        if (idIt != m_MapIdToFeature.end()) {
            //found feature with ID in question: update
            record.UpdateFeature(m_iFlags, idIt->second);
        }
        else {
            //didn't find feature with that ID: create new one
            pFeature.Reset(new CSeq_feat);
            record.InitializeFeature(m_iFlags, pFeature);
            if (!parentId.empty()) {
                xFeatureSetQualifier("Parent", parentId, pFeature);
                xFeatureSetXrefParent(parentId, pFeature);
                if (m_iFlags & fGeneXrefs) {
                    xFeatureSetXrefGrandParent(parentId, pFeature);
                }
            }
            xAddFeatureToAnnot(pFeature, pAnnot);
            m_MapIdToFeature[cdsId] = pFeature;
        }
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CGff3Reader::xFeatureSetXrefGrandParent(
    const string& parent,
    CRef<CSeq_feat> pFeature)
//  ----------------------------------------------------------------------------
{
    IdToFeatureMap::iterator it = m_MapIdToFeature.find(parent);
    if (it == m_MapIdToFeature.end()) {
        return false;
    }
    CRef<CSeq_feat> pParent = it->second;
    const string &grandParentsStr = pParent->GetNamedQual("Parent");
    list<string> grandParents;
    NStr::Split(grandParentsStr, ",", grandParents, 0);
    for (list<string>::const_iterator gpcit = grandParents.begin();
            gpcit != grandParents.end(); ++gpcit) {
        IdToFeatureMap::iterator gpit = m_MapIdToFeature.find(*gpcit);
        if (gpit == m_MapIdToFeature.end()) {
            return false;
        }
        CRef<CSeq_feat> pGrandParent = gpit->second;

        //xref grandchild->grandparent
        CRef<CFeat_id> pGrandParentId(new CFeat_id);
        pGrandParentId->Assign(pGrandParent->GetId());
        CRef<CSeqFeatXref> pGrandParentXref(new CSeqFeatXref);
        pGrandParentXref->SetId(*pGrandParentId);  
        pFeature->SetXref().push_back(pGrandParentXref);

        //xref grandparent->grandchild
        CRef<CFeat_id> pGrandChildId(new CFeat_id);
        pGrandChildId->Assign(pFeature->GetId());
        CRef<CSeqFeatXref> pGrandChildXref(new CSeqFeatXref);
        pGrandChildXref->SetId(*pGrandChildId);
        pGrandParent->SetXref().push_back(pGrandChildXref);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::xFeatureSetXrefParent(
    const string& parent,
    CRef<CSeq_feat> pChild)
//  ----------------------------------------------------------------------------
{
    IdToFeatureMap::iterator it = m_MapIdToFeature.find(parent);
    if (it == m_MapIdToFeature.end()) {
        return false;
    }
    CRef<CSeq_feat> pParent = it->second;

    //xref child->parent
    CRef<CFeat_id> pParentId(new CFeat_id);
    pParentId->Assign(pParent->GetId());
    CRef<CSeqFeatXref> pParentXref(new CSeqFeatXref);
    pParentXref->SetId(*pParentId);  
    pChild->SetXref().push_back(pParentXref);

    //xref parent->child
    CRef<CFeat_id> pChildId(new CFeat_id);
    pChildId->Assign(pChild->GetId());
    CRef<CSeqFeatXref> pChildXref(new CSeqFeatXref);
    pChildXref->SetId(*pChildId);
    pParent->SetXref().push_back(pChildXref);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::xUpdateAnnotGeneric(
    const CGff2Record& record,
    CRef<CSeq_feat> pFeature,
    CRef<CSeq_annot> pAnnot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    string id;
    if (record.GetAttribute("ID", id)) {
        IdToFeatureMap::iterator it = m_MapIdToFeature.find(id);
        if (it != m_MapIdToFeature.end()) {
            return record.UpdateFeature(m_iFlags, it->second);
        }
    }

    string featType = record.Type();
    if (featType == "stop_codon_read_through"  ||  featType == "selenocysteine") {
        string cdsParent;
        if (!record.GetAttribute("Parent", cdsParent)) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                0,
                "Bad data line: Unassigned code break.",
                ILineError::eProblem_GeneralParsingError) );
            ProcessError(*pErr, pEC);
            return false;
        }
        IdToFeatureMap::iterator it = m_MapIdToFeature.find(cdsParent);
        if (it == m_MapIdToFeature.end()) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                0,
                "Bad data line: Code break assigned to missing feature.",
                ILineError::eProblem_GeneralParsingError) );
            ProcessError(*pErr, pEC);
            return false;
        }

        CRef<CCode_break> pCodeBreak(new CCode_break); 
        CSeq_interval& cbLoc = pCodeBreak->SetLoc().SetInt();        
        CRef< CSeq_id > pId = CReadUtil::AsSeqId(record.Id(), m_iFlags);
        cbLoc.SetId(*pId);
        cbLoc.SetFrom(record.SeqStart());
        cbLoc.SetTo(record.SeqStop());
        if (record.IsSetStrand()) {
            cbLoc.SetStrand(record.Strand());
        }
        pCodeBreak->SetAa().SetNcbieaa(
            (featType == "selenocysteine") ? 'U' : 'X');
        CRef<CSeq_feat> pCds = it->second;
        CCdregion& cdRegion = pCds->SetData().SetCdregion();
        list< CRef< CCode_break > >& codeBreaks = cdRegion.SetCode_break();
        codeBreaks.push_back(pCodeBreak);
        return true;
    }
    if (!record.InitializeFeature(m_iFlags, pFeature)) {
        return false;
    }
    if (! xAddFeatureToAnnot(pFeature, pAnnot)) {
        return false;
    }
    string strId;
    if ( record.GetAttribute("ID", strId)) {
        m_MapIdToFeature[strId] = pFeature;
    }
    if (pFeature->GetData().IsRna()) {
        CRef<CSeq_interval> rnaLoc(new CSeq_interval);
        rnaLoc->Assign(pFeature->GetLocation().GetInt());
        mMrnaLocs[strId] = rnaLoc;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::xUpdateAnnotMrna(
    const CGff2Record& record,
    CRef<CSeq_feat> pFeature,
    CRef<CSeq_annot> pAnnot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    string id;
    if (record.GetAttribute("ID", id)) {
        IdToFeatureMap::iterator it = m_MapIdToFeature.find(id);
        if (it != m_MapIdToFeature.end()) {
            return record.UpdateFeature(m_iFlags, it->second);
        }
    }

    if (!record.InitializeFeature(m_iFlags, pFeature)) {
        return false;
    }
    CRef<CSeq_interval> mrnaLoc(new CSeq_interval);
    CSeq_loc::E_Choice choice = pFeature->GetLocation().Which();
    if (choice != CSeq_loc::e_Int) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Internal error: Unexpected location type.",
            ILineError::eProblem_BadFeatureInterval));
    }
    mrnaLoc->Assign(pFeature->GetLocation().GetInt());
    mMrnaLocs[id] = mrnaLoc;

    string parentsStr;
    if ((m_iFlags & fGeneXrefs)  &&  record.GetAttribute("Parent", parentsStr)) {
        list<string> parents;
        NStr::Split(parentsStr, ",", parents, 0);
        for (list<string>::const_iterator cit = parents.begin();
                cit != parents.end();
                ++cit) {
            if (!xFeatureSetXrefParent(*cit, pFeature)) {
                AutoPtr<CObjReaderLineException> pErr(
                    CObjReaderLineException::Create(
                    eDiag_Warning,
                    0,
                    "Bad data line: mRNA record with bad parent assignment.",
                    ILineError::eProblem_MissingContext) );
                ProcessError(*pErr, pEC);
            }
        }
    }

    if (! xAddFeatureToAnnot(pFeature, pAnnot)) {
        return false;
    }
    string strId;
    if ( record.GetAttribute("ID", strId)) {
        m_MapIdToFeature[strId] = pFeature;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::xUpdateAnnotGene(
    const CGff2Record& record,
    CRef<CSeq_feat> pFeature,
    CRef<CSeq_annot> pAnnot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    return xUpdateAnnotGeneric(record, pFeature, pAnnot, pEC);
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::xAddFeatureToAnnot(
    CRef< CSeq_feat > pFeature,
    CRef< CSeq_annot > pAnnot )
//  ----------------------------------------------------------------------------
{
    pAnnot->SetData().SetFtable().push_back( pFeature ) ;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::xVerifyCdsParents(
    const CGff2Record& record)
//  ----------------------------------------------------------------------------
{
    string id;
    string parents;
    if (!record.GetAttribute("ID", id)) {
        return true;
    }
    record.GetAttribute("Parent", parents);
    map<string, string>::iterator it = mCdsParentMap.find(id);
    if (it == mCdsParentMap.end()) {
        mCdsParentMap[id] = parents;
        return true;
    }
    return (it->second == parents);
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::xReadInit()
//  ----------------------------------------------------------------------------
{
    if (!CGff2Reader::xReadInit()) {
        return false;
    }
    mCdsParentMap.clear();
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::xIsIgnoredFeatureType(
    const string& featureType)
//  ----------------------------------------------------------------------------
{
    typedef CStaticArraySet<string, PNocase> STRINGARRAY;

    string ftype(CSoMap::ResolveSoAlias(featureType));

    static const char* const ignoredTypesAlways_[] = {
        "protein"
    };
    DEFINE_STATIC_ARRAY_MAP(STRINGARRAY, ignoredTypesAlways, ignoredTypesAlways_);    
    STRINGARRAY::const_iterator cit = ignoredTypesAlways.find(ftype);
    if (cit != ignoredTypesAlways.end()) {
        return true;
    }
    if (!IsInGenbankMode()) {
        return false;
    }

    /* -genbank mode:*/
    static const char* const specialTypesGenbank_[] = {
        "antisense_RNA",
        "autocatalytically_spliced_intron",
        "guide_RNA",
        "hammerhead_ribozyme",
        "lnc_RNA",
        "miRNA",
        "piRNA",
        "rasiRNA",
        "ribozyme",
        "RNase_MRP_RNA",
        "RNase_P_RNA",
        "scRNA",
        "selenocysteine",
        "siRNA",
        "snoRNA",
        "snRNA",
        "SRP_RNA",
        "stop_codon_read_through",
        "telomerase_RNA",
        "vault_RNA",
        "Y_RNA"
    };
    DEFINE_STATIC_ARRAY_MAP(STRINGARRAY, specialTypesGenbank, specialTypesGenbank_);    

    static const char* const ignoredTypesGenbank_[] = {
        "apicoplast_chromosome",
        "assembly",
        "cDNA_match",
        "chloroplast_chromosome",
        "chromoplast_chromosome",
        "chromosome",
        "contig",
        "cyanelle_chromosome",
        "dna_chromosome",
        "EST_match",
        "expressed_sequence_match",
        "intron",
        "leucoplast_chromosome",
        "macronuclear_chromosome",
        "match",
        "match_part",
        "micronuclear_chromosome",
        "mitochondrial_chromosome",
        "nuclear_chromosome",
        "nucleomorphic_chromosome",
        "nucleotide_motif",
        "nucleotide_to_protein_match",
        "partial_genomic_sequence_assembly",
        "protein_match",
        "replicon",
        "rna_chromosome",
        "sequence_assembly",
        "supercontig",
        "translated_nucleotide_match",
        "ultracontig",
    };
    DEFINE_STATIC_ARRAY_MAP(STRINGARRAY, ignoredTypesGenbank, ignoredTypesGenbank_);    

    cit = specialTypesGenbank.find(ftype);
    if (cit != specialTypesGenbank.end()) {
        return false;
    }

    cit = ignoredTypesGenbank.find(ftype);
    if (cit != ignoredTypesGenbank.end()) {
        return true;
    }

    return false;
}

END_objects_SCOPE
END_NCBI_SCOPE
