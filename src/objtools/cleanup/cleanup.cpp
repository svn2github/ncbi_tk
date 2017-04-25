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
 * Author:  Robert Smith
 *
 * File Description:
 *   Basic Cleanup of CSeq_entries.
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <serial/serialbase.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
// included for GetPubdescLabels and GetCitationList
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/biblio/Author.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/general/Name_std.hpp>
#include <objects/misc/sequence_macros.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/seqset_macros.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/taxon3/taxon3.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_vector_ci.hpp>
#include <objtools/cleanup/cleanup.hpp>
#include "cleanup_utils.hpp"

#include <util/strsearch.hpp>

#include "newcleanupp.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

enum EChangeType {
    eChange_UNKNOWN
};

// *********************** CCleanup implementation **********************


CCleanup::CCleanup(CScope* scope, EScopeOptions scope_handling)
{
    if (scope && scope_handling == eScope_UseInPlace) {
        m_Scope = scope;
    }
    else {
        m_Scope = new CScope(*(CObjectManager::GetInstance()));
        if (scope) {
            m_Scope->AddScope(*scope);
        }
    }
}


CCleanup::~CCleanup(void)
{
}


void CCleanup::SetScope(CScope* scope)
{
    m_Scope.Reset(new CScope(*(CObjectManager::GetInstance())));
    if (scope) {
        m_Scope->AddScope(*scope);
    }
}


static
CRef<CCleanupChange> makeCleanupChange(Uint4 options)
{
    CRef<CCleanupChange> changes;
    if (! (options  &  CCleanup::eClean_NoReporting)) {
        changes.Reset(new CCleanupChange);        
    }
    return changes;
}

#define CLEANUP_SETUP \
    CRef<CCleanupChange> changes(makeCleanupChange(options)); \
    CNewCleanup_imp clean_i(changes, options); \
    clean_i.SetScope(*m_Scope);

CConstRef<CCleanupChange> CCleanup::BasicCleanup(CSeq_entry& se, Uint4 options)
{
    CLEANUP_SETUP
    clean_i.BasicCleanupSeqEntry(se);
    return changes;
}


CConstRef<CCleanupChange> CCleanup::BasicCleanup(CSeq_submit& ss, Uint4 options)
{
    CLEANUP_SETUP
    clean_i.BasicCleanupSeqSubmit(ss);
    return changes;
}


/// Cleanup a Bioseq. 
CConstRef<CCleanupChange> CCleanup::BasicCleanup(CBioseq& bs, Uint4 options)
{
    CLEANUP_SETUP
    clean_i.BasicCleanupBioseq(bs);
    return changes;
}


CConstRef<CCleanupChange> CCleanup::BasicCleanup(CBioseq_set& bss, Uint4 options)
{
    CLEANUP_SETUP
    clean_i.BasicCleanupBioseqSet(bss);
    return changes;
}


CConstRef<CCleanupChange> CCleanup::BasicCleanup(CSeq_annot& sa, Uint4 options)
{
    CLEANUP_SETUP
    clean_i.BasicCleanupSeqAnnot(sa);
    return changes;
}


CConstRef<CCleanupChange> CCleanup::BasicCleanup(CSeq_feat& sf, Uint4 options)
{
    CLEANUP_SETUP
    clean_i.BasicCleanupSeqFeat(sf);
    return changes;
}


CConstRef<CCleanupChange> CCleanup::BasicCleanup(CBioSource& src, Uint4 options)
{
    CLEANUP_SETUP
    clean_i.BasicCleanupBioSource(src);
    return changes;
}


CConstRef<CCleanupChange> CCleanup::BasicCleanup(CSeq_entry_Handle& seh, Uint4 options)
{
    CRef<CCleanupChange> changes(makeCleanupChange(options));
    CNewCleanup_imp clean_i(changes, options);
    clean_i.SetScope(seh.GetScope());
    clean_i.BasicCleanupSeqEntryHandle(seh);
    return changes;
}


CConstRef<CCleanupChange> CCleanup::BasicCleanup(CBioseq_Handle& bsh,    Uint4 options)
{
    CRef<CCleanupChange> changes(makeCleanupChange(options));
    CNewCleanup_imp clean_i(changes, options);
    clean_i.SetScope(bsh.GetScope());
    clean_i.BasicCleanupBioseqHandle(bsh);
    return changes;
}


CConstRef<CCleanupChange> CCleanup::BasicCleanup(CBioseq_set_Handle& bssh, Uint4 options)
{
    CRef<CCleanupChange> changes(makeCleanupChange(options));
    CNewCleanup_imp clean_i(changes, options);
    clean_i.SetScope(bssh.GetScope());
    clean_i.BasicCleanupBioseqSetHandle(bssh);
    return changes;
}


CConstRef<CCleanupChange> CCleanup::BasicCleanup(CSeq_annot_Handle& sah, Uint4 options)
{
    CRef<CCleanupChange> changes(makeCleanupChange(options));
    CNewCleanup_imp clean_i(changes, options);
    clean_i.SetScope(sah.GetScope());
    clean_i.BasicCleanupSeqAnnotHandle(sah);
    return changes;
}


CConstRef<CCleanupChange> CCleanup::BasicCleanup(CSeq_feat_Handle& sfh,  Uint4 options)
{
    CRef<CCleanupChange> changes(makeCleanupChange(options));
    CNewCleanup_imp clean_i(changes, options);
    clean_i.SetScope(sfh.GetScope());
    clean_i.BasicCleanupSeqFeatHandle(sfh);
    return changes;
}




// *********************** Extended Cleanup implementation ********************
CConstRef<CCleanupChange> CCleanup::ExtendedCleanup(CSeq_entry& se,  Uint4 options)
{
    CLEANUP_SETUP
    clean_i.ExtendedCleanupSeqEntry(se);
    
    return changes;
}


CConstRef<CCleanupChange> CCleanup::ExtendedCleanup(CSeq_submit& ss,  Uint4 options)
{
    CLEANUP_SETUP
    clean_i.ExtendedCleanupSeqSubmit(ss);
    return changes;
}


CConstRef<CCleanupChange> CCleanup::ExtendedCleanup(CSeq_annot& sa,  Uint4 options)
{
    CLEANUP_SETUP
    clean_i.ExtendedCleanupSeqAnnot(sa); // (m_Scope->GetSeq_annotHandle(sa));
    return changes;
}

CConstRef<CCleanupChange> CCleanup::ExtendedCleanup(CSeq_entry_Handle& seh,  Uint4 options)
{
    CRef<CCleanupChange> changes(makeCleanupChange(options));
    CNewCleanup_imp clean_i(changes, options);
    clean_i.SetScope(seh.GetScope());
    clean_i.ExtendedCleanupSeqEntryHandle(seh); // (m_Scope->GetSeq_annotHandle(sa));
    return changes;
}


// *********************** CCleanupChange implementation **********************


CCleanupChange::CCleanupChange()
{
}


size_t CCleanupChange::ChangeCount() const
{
    return m_Changes.count();
}


bool CCleanupChange::IsChanged(CCleanupChange::EChanges e) const
{
    return m_Changes.test(e);
}


void CCleanupChange::SetChanged(CCleanupChange::EChanges e)
{
    m_Changes.set(e);
}


vector<CCleanupChange::EChanges> CCleanupChange::GetAllChanges() const
{
    vector<EChanges>  result;
    for (size_t i = eNoChange + 1; i < m_Changes.size(); ++i) {
        if (m_Changes.test(i)) {
            result.push_back( (EChanges) i);
        }
    }
    return result;
}


vector<string> CCleanupChange::GetAllDescriptions() const
{
    vector<string>  result;
    for (size_t i = eNoChange + 1; i < m_Changes.size(); ++i) {
        if (m_Changes.test(i)) {
            result.push_back( GetDescription((EChanges) i) );
        }
    }
    return result;
}


string CCleanupChange::GetDescription(EChanges e)
{
    if (e <= eNoChange  ||  e >= eNumberofChangeTypes) {
        return sm_ChangeDesc[eNoChange];
    }
    return sm_ChangeDesc[e];
}

// corresponds to the values in CCleanupChange::EChanges.
// They must be edited together.
const char* const CCleanupChange::sm_ChangeDesc[eNumberofChangeTypes + 1] = {
    "Invalid Change Code",
    // set when strings are changed.
    "Trim Spaces",
    "Clean Double Quotes",
    "Append To String",
    // set when lists are sorted or uniqued.
    "Clean Qualifiers List",
    "Clean Dbxrefs List",
    "Clean CitonFeat List",
    "Clean Keywords List",
    "Clean Subsource List",
    "Clean Orgmod List", 
    // Set when fields are moved or have content changes
    "Repair BioseqMol", //10
    "Change Feature Key", 
    "Normalize Authors",
    "Change Publication",
    "Change Qualifiers",
    "Change Dbxrefs",
    "Change Keywords",
    "Change Subsource",
    "Change Orgmod",
    "Change Exception",
    "Change Comment", //20
    // Set when fields are rescued
    "Change tRna",
    "Change rRna",
    "Change ITS",
    "Change Anticodon",
    "Change Code Break",
    "Change Genetic Code",
    "Copy GeneXref",
    "Copy ProtXref",
    // set when locations are repaired
    "Change Seqloc",
    "Change Strand", //30
    "Change WholeLocation",
    // set when MolInfo descriptors are affected
    "Change MolInfo Descriptor",
    // set when prot-xref is removed
    "Remove ProtXref",
    // set when gene-xref is removed
    "Remove GeneXref",
    // set when protein feature is added
    "Add Protein Feature",
    // set when feature is removed
    "Remove Feature",
    // set when feature is moved
    "Move Feature",
    // set when qualifier is removed
    "Remove Qualifier",
    // set when Gene Xref is created
    "Add GeneXref",
    // set when descriptor is removed
    "Remove Descriptor", //40
    "Remove Keyword",
    "Add Descriptor",
    "Move Descriptor",
    "Convert Feature to Descriptor", 
    "Collapse Set",
    "Change Feature Location",
    "Remove Annotation",
    "Convert Feature",
    "Remove Comment",
    "Add BioSource OrgMod", //50
    "Add BioSource SubSource",
    "Change BioSource Genome", 
    "Change BioSource Origin", 
    "Change BioSource Other",
    "Change SeqId", 
    "Remove Empty Publication",
    "Add Qualifier",
    "Cleanup Date",
    "Change BioseqInst",
    "Remove SeqID", // 60
    "Add ProtXref",
    "Change Partial",
    "Change Prot Names",
    "Change Prot Activities",
    "Change Site",
    "Change PCR Primers",
    "Change RNA-ref",
    "Move To Prot Xref",
    "Compress Spaces",
    "Strip serial", // 70
    "Remove Orgmod",
    "Remove SubSource",
    "Create Gene Nomenclature",
    "Clean Seq-feat xref",
    "Clean User-Object Or -Field",
    "Letter Case Change",
    "Change Bioseq-set Class",
    "Unique Without Sort",
    "Add RNA-ref",
    "Change Gene-ref", // 80
    "Clean Dbtag",
    "Change Biomol",
    "Change Cdregion",
    "Clean EC Number",
    "Remove Exception",
    "Add NcbiCleanupObject",
    "Clean Delta-ext",
    "Trim Flanking Quotes",
    "Clean Bioseq Title",
    "Decode XML", // 90
    "Remove Dup BioSource",
    "Clean Org-ref",
    "Trim Internal Semicolons",
    "Add SeqFeatXref",

    // set when any other change is made.
    "Change Other", 
    "Invalid Change Code"
};


CProt_ref::EProcessed s_ProcessedFromKey(const string& key)
{
    if (NStr::Equal(key, "sig_peptide")) {
        return CProt_ref::eProcessed_signal_peptide;
    } else if (NStr::Equal(key, "mat_peptide")) {
        return CProt_ref::eProcessed_mature;
    } else if (NStr::Equal(key, "transit_peptide")) {
        return CProt_ref::eProcessed_transit_peptide;
    } else if (NStr::Equal(key, "preprotein") || NStr::Equal(key, "proprotein")) {
        return CProt_ref::eProcessed_preprotein;
    } else if (NStr::Equal(key, "propeptide")) {
        return CProt_ref::eProcessed_propeptide;
    } else {
        return CProt_ref::eProcessed_not_set;
    }
}

string s_KeyFromProcessed(CProt_ref::EProcessed processed)
{
    switch (processed) {
    case CProt_ref::eProcessed_mature:
        return "mat_peptide";
        break;
    case CProt_ref::eProcessed_preprotein:
        return "preprotein";
        break;
    case CProt_ref::eProcessed_signal_peptide:
        return "sig_peptide";
        break;
    case CProt_ref::eProcessed_transit_peptide:
        return "transit_peptide";
        break;
    case CProt_ref::eProcessed_propeptide:
        return "propeptide";
        break;
    case CProt_ref::eProcessed_not_set:
        return kEmptyStr;
        break;
    }
    return kEmptyStr;
}


bool ConvertProteinToImp(CSeq_feat_Handle fh)
{
    if (fh.GetData().IsProt() && fh.GetData().GetProt().IsSetProcessed()) {
        string key = s_KeyFromProcessed(fh.GetData().GetProt().GetProcessed());
        if (!NStr::IsBlank(key)) {
            CRef<CSeq_feat> new_feat(new CSeq_feat());
            new_feat->Assign(*(fh.GetSeq_feat()));
            if (fh.GetData().GetProt().IsSetName() && !fh.GetData().GetProt().GetName().empty()) {
                CRef<CGb_qual> q(new CGb_qual());
                q->SetQual("product");
                q->SetVal(fh.GetData().GetProt().GetName().front());
                new_feat->SetQual().push_back(q);
            }
            new_feat->SetData().SetImp().SetKey(key);
            CSeq_feat_EditHandle efh(fh);
            efh.Replace(*new_feat);
            return true;
        }
    }
    return false;
}


bool s_IsPreprotein(CSeq_feat_Handle fh)
{
    if (!fh.IsSetData()) {
        return false;
    } else if (fh.GetData().IsProt() &&
        fh.GetData().GetProt().IsSetProcessed() &&
        fh.GetData().GetProt().GetProcessed() == CProt_ref::eProcessed_preprotein) {
        return true;
    } else if (fh.GetData().IsImp() &&
        fh.GetData().GetImp().IsSetKey() &&
        s_ProcessedFromKey(fh.GetData().GetImp().GetKey()) == CProt_ref::eProcessed_preprotein) {
        return true;
    } else {
        return false;
    }
}


void RescueProtProductQual(CSeq_feat& feat)
{
    if (!feat.IsSetQual() ||
        !feat.IsSetData() || 
        !feat.GetData().IsProt() ||
        feat.GetData().GetProt().IsSetName()) {
        return;
    }
    CSeq_feat::TQual::iterator it = feat.SetQual().begin();
    while (it != feat.SetQual().end()) {
        if ((*it)->IsSetQual() &&
            NStr::Equal((*it)->GetQual(), "product")) {
            if ((*it)->IsSetVal() && !NStr::IsBlank((*it)->GetVal())) {
                feat.SetData().SetProt().SetName().push_back((*it)->GetVal());
            }
            it = feat.SetQual().erase(it);
        } else {
            ++it;
        }
    }

    if (feat.SetQual().empty()) {
        feat.ResetQual();
    }
}


bool CCleanup::MoveFeatToProtein(CSeq_feat_Handle fh)
{
    CProt_ref::EProcessed processed = CProt_ref::eProcessed_not_set;
    if (fh.GetData().IsImp()) {
        if (!fh.GetData().GetImp().IsSetKey()) {
            return false;
        }
        processed = s_ProcessedFromKey(fh.GetData().GetImp().GetKey());
        if (processed == CProt_ref::eProcessed_not_set || processed == CProt_ref::eProcessed_preprotein) {
            return false;
        }
    } else if (s_IsPreprotein(fh)) {
        return ConvertProteinToImp(fh);
    }

    CBioseq_Handle parent_bsh = fh.GetScope().GetBioseqHandle(fh.GetLocation());

    if (!parent_bsh) {
        // feature is mispackaged
        return false;
    }
    if (parent_bsh.IsAa()) {
        // feature is already on protein sequence
        return false;
    }

    CConstRef<CSeq_feat> cds = sequence::GetOverlappingCDS(fh.GetLocation(), fh.GetScope());
    if (!cds || !cds->IsSetProduct()) {
        // there is no overlapping coding region feature, so there is no appropriate
        // protein sequence to move to
        return ConvertProteinToImp(fh);
    }

    bool require_frame = false;
    ITERATE(CBioseq::TId, id_it, parent_bsh.GetBioseqCore()->GetId()) {
        if ((*id_it)->IsEmbl() || (*id_it)->IsDdbj()) {
            require_frame = true;
            break;
        }
    }
    CRef<CSeq_loc> prot_loc = GetProteinLocationFromNucleotideLocation(fh.GetLocation(), *cds, fh.GetScope(), require_frame);

    if (!prot_loc) {
        return false;
    }

    CConstRef<CSeq_feat> orig_feat = fh.GetSeq_feat();
    CRef<CSeq_feat> new_feat(new CSeq_feat());
    new_feat->Assign(*orig_feat);
    if (new_feat->GetData().Which() == CSeqFeatData::e_Imp) {
        new_feat->SetData().SetProt().SetProcessed(processed);
        // if possible, rescue product qual
        RescueProtProductQual(*new_feat);
        if (processed == CProt_ref::eProcessed_mature &&
            !new_feat->GetData().GetProt().IsSetName()) {
            if (orig_feat->IsSetComment() && !NStr::IsBlank(orig_feat->GetComment())) {
                new_feat->SetData().SetProt().SetName().push_back(orig_feat->GetComment());
                new_feat->ResetComment();
            } else {
                new_feat->SetData().SetProt().SetName().push_back("unnamed");
            }
        }
    }

    // change location to protein
    new_feat->ResetLocation();
    new_feat->SetLocation(*prot_loc);
    SetFeaturePartial(*new_feat);

    CSeq_feat_EditHandle edh(fh);
    edh.Replace(*new_feat);
    CRef<CCleanupChange> changes(makeCleanupChange(0));
    CNewCleanup_imp clean_i(changes, 0);
    clean_i.SetScope(fh.GetScope());
    clean_i.BasicCleanupSeqFeat(*new_feat);

    CSeq_annot_Handle ah = fh.GetAnnot();

    CBioseq_Handle target_bsh = fh.GetScope().GetBioseqHandle(new_feat->GetLocation());
    CBioseq_EditHandle eh = target_bsh.GetEditHandle();

    // Find a feature table on the protein sequence to add the feature to.    
    CSeq_annot_Handle ftable;
    if (target_bsh.GetCompleteBioseq()->IsSetAnnot()) {
        ITERATE(CBioseq::TAnnot, annot_it, target_bsh.GetCompleteBioseq()->GetAnnot()) {
            if ((*annot_it)->IsFtable()) {
                ftable = fh.GetScope().GetSeq_annotHandle(**annot_it);
            }
        }
    }
    
    // If there is no feature table present, make one
    if (!ftable) {
        CRef<CSeq_annot> new_annot(new CSeq_annot());
        ftable = eh.AttachAnnot(*new_annot);
    }

    // add feature to the protein bioseq
    CSeq_annot_EditHandle aeh(ftable);
    aeh.TakeFeat(edh);

    // remove old annot if now empty
    if (CNewCleanup_imp::ShouldRemoveAnnot(*(ah.GetCompleteSeq_annot()))) {
        CSeq_annot_EditHandle orig(ah);
        orig.Remove();
    }

    return true;
}


bool CCleanup::MoveProteinSpecificFeats(CSeq_entry_Handle seh)
{
    bool any_change = false;
    CBioseq_CI bi(seh, CSeq_inst::eMol_na);
    while (bi) {
        SAnnotSelector sel(CSeqFeatData::e_Prot);
        sel.IncludeFeatType(CSeqFeatData::e_Psec_str);
        sel.IncludeFeatType(CSeqFeatData::e_Bond);
        for (CFeat_CI prot_it(*bi, sel); prot_it; ++prot_it) {
            any_change |= MoveFeatToProtein(*prot_it);
        }
        for (CFeat_CI imp_it(*bi, CSeqFeatData::e_Imp); imp_it; ++imp_it) {
            any_change |= MoveFeatToProtein(*imp_it);
        }
        ++bi;
    }
    return any_change;
}


bool CCleanup::IsGeneXrefUnnecessary(const CSeq_feat& sf, CScope& scope, const CGene_ref& gene_xref)
{
    if (gene_xref.IsSuppressed()) {
        return false;
    }

    CConstRef<CSeq_feat> gene = sequence::GetOverlappingGene(sf.GetLocation(), scope);    
    if (!gene || !gene->IsSetData() || !gene->GetData().IsGene()) {
        return false;
    }

    if (!gene->GetData().GetGene().RefersToSameGene(gene_xref)) {
        return false;
    }

    // see if other gene might also match
    sequence::TFeatScores scores;
    sequence::GetOverlappingFeatures(sf.GetLocation(), CSeqFeatData::e_Gene, CSeqFeatData::eSubtype_gene,
        sequence::eOverlap_Contained, scores, scope);
    if (scores.size() == 1) {
        return true;
    } else if (scores.size() == 0) {
        return false;
    }

    ITERATE(sequence::TFeatScores, g, scores) {
        if (g->second.GetPointer() != gene.GetPointer() && 
            sequence::Compare(g->second->GetLocation(), gene->GetLocation(), &scope, sequence::fCompareOverlapping) == sequence::eSame) {
            return false;
        }
    }
    return true;
}


bool CCleanup::RemoveUnnecessaryGeneXrefs(CSeq_feat& f, CScope& scope)
{
    if (!f.IsSetXref()) {
        return false;
    }
    bool any_removed = false;
    CSeq_feat::TXref::iterator xit = f.SetXref().begin();
    while (xit != f.SetXref().end()) {
        if ((*xit)->IsSetData() && (*xit)->GetData().IsGene() &&
            IsGeneXrefUnnecessary(f, scope, (*xit)->GetData().GetGene())) {
            xit = f.SetXref().erase(xit);
            any_removed = true;
        } else {
            ++xit;
        }
    }
    if (any_removed) {
        if (f.IsSetXref() && f.GetXref().empty()) {
            f.ResetXref();
        }
    }
    return any_removed;
}


bool CCleanup::RemoveUnnecessaryGeneXrefs(CSeq_entry_Handle seh)
{
    bool any_change = false;
    CScope& scope = seh.GetScope();

    for (CFeat_CI fi(seh); fi; ++fi) {
        if (fi->IsSetXref()) {
            CRef<CSeq_feat> new_feat(new CSeq_feat());
            new_feat->Assign(*(fi->GetOriginalSeq_feat()));
            bool any_removed = RemoveUnnecessaryGeneXrefs(*new_feat, scope);
            if (any_removed) {
                CSeq_feat_EditHandle edh(*fi);
                edh.Replace(*new_feat);
                any_change = true;
            }
        }
    }

    return any_change;
}

bool CCleanup::RemoveNonsuppressingGeneXrefs(CSeq_feat& f)
{
    if (!f.IsSetXref()) {
        return false;
    }
    bool any_removed = false;
    CSeq_feat::TXref::iterator xit = f.SetXref().begin();
    while (xit != f.SetXref().end()) {
        if ((*xit)->IsSetData() && (*xit)->GetData().IsGene() &&
            !(*xit)->GetData().GetGene().IsSuppressed()) {
            xit = f.SetXref().erase(xit);
            any_removed = true;
        } else {
            ++xit;
        }
    }
    if (any_removed) {
        if (f.IsSetXref() && f.GetXref().empty()) {
            f.ResetXref();
        }
    }
    return any_removed;
}


bool CCleanup::RepairXrefs(const CSeq_feat& src, CSeq_feat_Handle& dst, const CTSE_Handle& tse)
{
    if (!src.IsSetId() || !src.GetId().IsLocal()) {
        // can't create xref if no ID
        return false;
    }
    if (!CSeqFeatData::AllowXref(src.GetData().GetSubtype(), dst.GetData().GetSubtype())) {
        // only create reciprocal xrefs if permitted
        return false;
    }
    // don't create xref if already have xref to feature of same type as src
    bool has_xref = false;
    if (dst.IsSetXref()) {
        ITERATE(CSeq_feat::TXref, xit, dst.GetXref()) {
            if ((*xit)->IsSetId() && (*xit)->GetId().IsLocal()) {
                if ((*xit)->GetId().Equals(src.GetId())) {
                    // already have xref
                    has_xref = true;
                    break;
                } else {
                    const CTSE_Handle::TFeatureId& feat_id = (*xit)->GetId().GetLocal();
                    CTSE_Handle::TSeq_feat_Handles far_feats = tse.GetFeaturesWithId(CSeqFeatData::e_not_set, feat_id);
                    ITERATE(CTSE_Handle::TSeq_feat_Handles, fit, far_feats) {
                        if (fit->GetData().GetSubtype() == src.GetData().GetSubtype()) {
                            has_xref = true;
                            break;
                        }
                    }
                    if (has_xref) {
                        break;
                    }
                }
            }
        }
    }
    bool rval = false;
    if (!has_xref) {
        // to put into "editing mode"
        dst.GetAnnot().GetEditHandle();
        CSeq_feat_EditHandle eh(dst);
        CRef<CSeq_feat> cpy(new CSeq_feat());
        cpy->Assign(*(dst.GetSeq_feat()));
        cpy->AddSeqFeatXref(src.GetId());
        eh.Replace(*cpy);
        rval = true;
    }
    return rval;
}


bool CCleanup::RepairXrefs(const CSeq_feat& f, const CTSE_Handle& tse)
{
    bool rval = false;

    if (!f.IsSetId() || !f.IsSetXref()) {
        return rval;
    }

    ITERATE(CSeq_feat::TXref, xit, f.GetXref()) {
        if ((*xit)->IsSetId() && (*xit)->GetId().IsLocal()) {            
            const CTSE_Handle::TFeatureId& x_id = (*xit)->GetId().GetLocal();
            CTSE_Handle::TSeq_feat_Handles far_feats = tse.GetFeaturesWithId(CSeqFeatData::e_not_set, x_id);
            if (far_feats.size() == 1) {
                rval |= RepairXrefs(f, far_feats[0], tse);
            }
        }
    }
    return rval;
}


bool CCleanup::RepairXrefs(CSeq_entry_Handle seh)
{
    bool rval = false;
    const CTSE_Handle& tse = seh.GetTSE_Handle();

    CFeat_CI fi(seh);
    while (fi) {
        rval |= RepairXrefs(*(fi->GetSeq_feat()), tse);
        ++fi;
    }
    return rval;
}


bool CCleanup::FindMatchingLocusGene(CSeq_feat& f, const CGene_ref& gene_xref, CBioseq_Handle bsh)
{
    bool match = false;
    string locus1;
    if (gene_xref.IsSetLocus())
        locus1 = gene_xref.GetLocus();
    for (CFeat_CI feat_ci(bsh, SAnnotSelector(CSeqFeatData::eSubtype_gene)); feat_ci; ++feat_ci)
    {
        string locus2;
        if ( !f.Equals(*feat_ci->GetSeq_feat()) && feat_ci->GetSeq_feat()->IsSetData() && feat_ci->GetSeq_feat()->GetData().IsGene() 
             && feat_ci->GetSeq_feat()->GetData().GetGene().IsSetLocus())
        {
            locus2 = feat_ci->GetSeq_feat()->GetData().GetGene().GetLocus();
        }
        if (!locus1.empty() && !locus2.empty() && locus1 == locus2)
        {
            match = true;
            break;
        }
    }
    return match;
}

bool CCleanup::RemoveOrphanLocusGeneXrefs(CSeq_feat& f, CBioseq_Handle bsh)
{
    if (!f.IsSetXref()) {
        return false;
    }
    bool any_removed = false;
    CSeq_feat::TXref::iterator xit = f.SetXref().begin();
    while (xit != f.SetXref().end()) {
        if ((*xit)->IsSetData() && (*xit)->GetData().IsGene() &&
            !(*xit)->GetData().GetGene().IsSuppressed() && !FindMatchingLocusGene(f, (*xit)->GetData().GetGene(), bsh)) {
            xit = f.SetXref().erase(xit);
            any_removed = true;
        } else {
            ++xit;
        }
    }
    if (any_removed) {
        if (f.IsSetXref() && f.GetXref().empty()) {
            f.ResetXref();
        }
    }
    return any_removed;
}

bool CCleanup::FindMatchingLocus_tagGene(CSeq_feat& f, const CGene_ref& gene_xref, CBioseq_Handle bsh)
{
    bool match = false;
    string locus_tag1;
    if (gene_xref.IsSetLocus_tag())
        locus_tag1 = gene_xref.GetLocus_tag();
    for (CFeat_CI feat_ci(bsh, SAnnotSelector(CSeqFeatData::eSubtype_gene)); feat_ci; ++feat_ci)
    {
        string locus_tag2;
        if ( !f.Equals(*feat_ci->GetSeq_feat()) && feat_ci->GetSeq_feat()->IsSetData() && feat_ci->GetSeq_feat()->GetData().IsGene() 
             && feat_ci->GetSeq_feat()->GetData().GetGene().IsSetLocus_tag())
        {
            locus_tag2 = feat_ci->GetSeq_feat()->GetData().GetGene().GetLocus_tag();
        }
        if (!locus_tag1.empty() && !locus_tag2.empty() && locus_tag1 == locus_tag2)
        {
            match = true;
            break;
        }
    }
    return match;
}

bool CCleanup::RemoveOrphanLocus_tagGeneXrefs(CSeq_feat& f, CBioseq_Handle bsh)
{
    if (!f.IsSetXref()) {
        return false;
    }
    bool any_removed = false;
    CSeq_feat::TXref::iterator xit = f.SetXref().begin();
    while (xit != f.SetXref().end()) {
        if ((*xit)->IsSetData() && (*xit)->GetData().IsGene() &&
            !(*xit)->GetData().GetGene().IsSuppressed() && !FindMatchingLocus_tagGene(f, (*xit)->GetData().GetGene(), bsh)) {
            xit = f.SetXref().erase(xit);
            any_removed = true;
        } else {
            ++xit;
        }
    }
    if (any_removed) {
        if (f.IsSetXref() && f.GetXref().empty()) {
            f.ResetXref();
        }
    }
    return any_removed;
}


bool CCleanup::SeqLocExtend(CSeq_loc& loc, size_t pos, CScope& scope)
{
    size_t loc_start = loc.GetStart(eExtreme_Positional);
    size_t loc_stop = loc.GetStop(eExtreme_Positional);
    bool partial_start = loc.IsPartialStart(eExtreme_Positional);
    bool partial_stop = loc.IsPartialStop(eExtreme_Positional);
    ENa_strand strand = loc.GetStrand();
    CRef<CSeq_loc> new_loc(NULL);
    bool changed = false;

    if (pos < loc_start) {
        CRef<CSeq_id> id(new CSeq_id());
        id->Assign(*(loc.GetId()));
        CRef<CSeq_loc> add(new CSeq_loc(*id, pos, loc_start - 1, strand));
        add->SetPartialStart(partial_start, eExtreme_Positional);
        new_loc = sequence::Seq_loc_Add(loc, *add, CSeq_loc::fSort | CSeq_loc::fMerge_AbuttingOnly, &scope);
        changed = true;
    } else if (pos > loc_stop) {
        CRef<CSeq_id> id(new CSeq_id());
        id->Assign(*(loc.GetId()));
        CRef<CSeq_loc> add(new CSeq_loc(*id, loc_stop + 1, pos, strand));
        add->SetPartialStop(partial_stop, eExtreme_Positional);
        new_loc = sequence::Seq_loc_Add(loc, *add, CSeq_loc::fSort | CSeq_loc::fMerge_AbuttingOnly, &scope);
        changed = true;
    }
    if (changed) {
        loc.Assign(*new_loc);
    }
    return changed;
}


bool CCleanup::ExtendStopPosition(CSeq_feat& f, const CSeq_feat* cdregion, size_t extension)
{
    CRef<CSeq_loc> new_loc(&f.SetLocation());

    CRef<CSeq_loc> last_interval;
    if (new_loc->IsMix()) {
        last_interval = new_loc->SetMix().SetLastLoc();
    }
    else
    {
        last_interval = new_loc;
    }

    CConstRef<CSeq_id> id(last_interval->GetId());

    TSeqPos new_start;
    TSeqPos new_stop;

    // the last element of the mix or the single location MUST be converted into interval
    // whethe it's whole or point, etc
    if (last_interval->IsSetStrand() && last_interval->GetStrand() == eNa_strand_minus) {
        new_start = (cdregion ? cdregion->GetLocation().GetStart(eExtreme_Positional) :          
              last_interval->GetStart(eExtreme_Positional)) - extension;

        new_stop = last_interval->GetStop(eExtreme_Positional);
    }
    else {
        new_start = last_interval->GetStart(eExtreme_Positional);
        new_stop = (cdregion ? cdregion->GetLocation().GetStop(eExtreme_Positional) :
            last_interval->GetStop(eExtreme_Positional)) + extension;
    }
    last_interval->SetInt().SetFrom(new_start);
    last_interval->SetInt().SetTo(new_stop);
    last_interval->SetInt().SetId().Assign(*id);

    new_loc->SetPartialStop(false, eExtreme_Biological);

    return true;
}

bool CCleanup::ExtendToStopCodon(CSeq_feat& f, CBioseq_Handle bsh, size_t limit)
{
    const CSeq_loc& loc = f.GetLocation();

    CCdregion::TFrame frame = CCdregion::eFrame_not_set;
    const CGenetic_code* code = NULL;
    // we need to extract frame and cd_region from linked cd_region
    if (f.IsSetData() && f.GetData().IsCdregion())
    {
        if (f.GetData().GetCdregion().IsSetCode())
           code = &(f.GetData().GetCdregion().GetCode());
        if (f.GetData().GetCdregion().IsSetFrame())
           frame = f.GetData().GetCdregion().GetFrame();
    }

    size_t stop = loc.GetStop(eExtreme_Biological);
    // figure out if we have a partial codon at the end
    size_t orig_len = sequence::GetLength(loc, &(bsh.GetScope()));
    size_t len = orig_len;

    if (frame == CCdregion::eFrame_two) {
        len -= 1;
    } else if (frame == CCdregion::eFrame_three) {
        len -= 2;
    }
    
    size_t mod = len % 3;
    CRef<CSeq_loc> vector_loc(new CSeq_loc());
    vector_loc->SetInt().SetId().Assign(*(bsh.GetId().front().GetSeqId()));

    if (loc.IsSetStrand() && loc.GetStrand() == eNa_strand_minus) {
        vector_loc->SetInt().SetFrom(0);
        vector_loc->SetInt().SetTo(stop + mod - 1);
        vector_loc->SetStrand(eNa_strand_minus);
    } else {
        vector_loc->SetInt().SetFrom(stop - mod + 1);
        vector_loc->SetInt().SetTo(bsh.GetInst_Length() - 1);
    }

    CSeqVector seq(*vector_loc, bsh.GetScope(), CBioseq_Handle::eCoding_Iupac);
    // reserve our space
    size_t usable_size = seq.size();

    if (limit > 0 && usable_size > limit) {
        usable_size = limit;
    }

    // get appropriate translation table
    const CTrans_table & tbl =
        (code ? CGen_code_table::GetTransTable(*code) :
        CGen_code_table::GetTransTable(1));

    // main loop through bases
    CSeqVector::const_iterator start = seq.begin();

    size_t i;
    size_t k;
    size_t state = 0;
    size_t length = usable_size / 3;

    for (i = 0; i < length; ++i) {
        // loop through one codon at a time
        for (k = 0; k < 3; ++k, ++start) {
            state = tbl.NextCodonState(state, *start);
        }

        if (tbl.GetCodonResidue(state) == '*') {
            size_t extension = ((i + 1) * 3) - mod;
            ExtendStopPosition(f, 0, extension);
            return true;
        }
    }

    bool rval = false;
    if (usable_size < 3 && limit == 0) {
        if (loc.GetStrand() == eNa_strand_minus) {
            rval = SeqLocExtend(f.SetLocation(), 0, bsh.GetScope());
        } else {
            rval = SeqLocExtend(f.SetLocation(), bsh.GetInst_Length() - 1, bsh.GetScope());
        }
        f.SetLocation().SetPartialStop(true, eExtreme_Biological);
    }

    return rval;
}


bool CCleanup::SetBestFrame(CSeq_feat& cds, CScope& scope)
{
    bool changed = false;
    CCdregion::TFrame frame = CCdregion::eFrame_not_set;
    if (cds.GetData().GetCdregion().IsSetFrame()) {
        frame = cds.GetData().GetCdregion().GetFrame();
    }

    CCdregion::TFrame new_frame = CSeqTranslator::FindBestFrame(cds, scope);
    if (frame != new_frame) {
        cds.SetData().SetCdregion().SetFrame(new_frame);
        changed = true;
    }
    return changed;
}

// like C's function GetFrameFromLoc, but better
bool CCleanup::SetFrameFromLoc(CCdregion::EFrame &frame, const CSeq_loc& loc, CScope& scope)
{
    if (!loc.IsPartialStart(eExtreme_Biological)) {
        if (frame != CCdregion::eFrame_one) {
            frame = CCdregion::eFrame_one;
            return true;
        }
        return false;
    }
    if (loc.IsPartialStop(eExtreme_Biological)) {
        // cannot make a determination if both ends are partial
        return false;
    }

    const TSeqPos seq_len = sequence::GetLength(loc, &scope);

    CCdregion::EFrame desired_frame = CCdregion::eFrame_not_set;

    // have complete last codon, get frame from length
    switch( (seq_len % 3) + 1 ) {
        case 1:
            desired_frame = CCdregion::eFrame_one;
            break;
        case 2:
            desired_frame = CCdregion::eFrame_two;
            break;
        case 3:
            desired_frame = CCdregion::eFrame_three;
            break;
        default:
            // mathematically impossible
            _ASSERT(false);
            return false;
    }
    if (frame != desired_frame) {
        frame = desired_frame;
        return true;
    }
    return false;
}


bool CCleanup::SetFrameFromLoc(CCdregion &cdregion, const CSeq_loc& loc, CScope& scope)
{
    CCdregion::EFrame frame = CCdregion::eFrame_not_set;
    if (cdregion.IsSetFrame()) {
        frame = cdregion.GetFrame();
    }
    if (SetFrameFromLoc(frame, loc, scope)) {
        cdregion.SetFrame(frame);
        return true;
    } else {
        return false;
    }
}


bool IsTransSpliced(const CSeq_feat& feat)
{
    if (feat.IsSetExcept_text() && NStr::Find(feat.GetExcept_text(), "trans-splicing") != string::npos) {
        return true;
    } else {
        return false;
    }
}


bool CCleanup::ExtendToStopIfShortAndNotPartial(CSeq_feat& f, CBioseq_Handle bsh, bool check_for_stop)
{
    if (!f.GetData().IsCdregion()) {
        // not coding region
        return false;
    }
    if (sequence::IsPseudo(f, bsh.GetScope())) {
        return false;
    }
    if (f.GetLocation().IsPartialStop(eExtreme_Biological)) {
        return false;
    }
    CConstRef<CSeq_feat> mrna = sequence::GetOverlappingmRNA(f.GetLocation(), bsh.GetScope());
    if (mrna && mrna->GetLocation().GetStop(eExtreme_Biological) != f.GetLocation().GetStop(eExtreme_Biological)){
        return false;
    }

    if (check_for_stop) {
        string translation;
        try {
            CSeqTranslator::Translate(f, bsh.GetScope(), translation, true);
        } catch (CSeqMapException& e) {
            //unable to translate
            return false;
        } catch (CSeqVectorException& e) {
            //unable to translate
            return false;
        }
        if (NStr::EndsWith(translation, "*")) {
            //already has stop codon
            return false;
        }
    }

    return ExtendToStopCodon(f, bsh, 3);
}


bool CCleanup::LocationMayBeExtendedToMatch(const CSeq_loc& orig, const CSeq_loc& improved)
{
    if ((orig.GetStrand() == eNa_strand_minus &&
        orig.GetStop(eExtreme_Biological) > improved.GetStop(eExtreme_Biological)) ||
        (orig.GetStrand() != eNa_strand_minus &&
        orig.GetStop(eExtreme_Biological) < improved.GetStop(eExtreme_Biological))) {
        return true;
    }

    return false;
}

void CCleanup::SetProteinName(CProt_ref& prot_ref, const string& protein_name, bool append)
{
    if (append && prot_ref.IsSetName() &&
        prot_ref.GetName().size() > 0 &&
        !NStr::IsBlank(prot_ref.GetName().front())) {
        prot_ref.SetName().front() += "; " + protein_name;
    } else {
        prot_ref.ResetName();
        prot_ref.SetName().push_back(protein_name);
    }

}


void CCleanup::SetProteinName(CSeq_feat& cds, const string& protein_name, bool append, CScope& scope)
{
    bool added = false;
    if (cds.IsSetProduct()) {
        CBioseq_Handle prot = scope.GetBioseqHandle(cds.GetProduct());
        if (prot) {
            // find main protein feature
            CFeat_CI feat_ci(prot, CSeqFeatData::eSubtype_prot);
            if (feat_ci) {
                CRef<CSeq_feat> new_prot(new CSeq_feat());
                new_prot->Assign(feat_ci->GetOriginalFeature());
                SetProteinName(new_prot->SetData().SetProt(), protein_name, append);
                CSeq_feat_EditHandle feh(feat_ci->GetSeq_feat_Handle());
                feh.Replace(*new_prot);
            } else {
                // make new protein feature
                feature::AddProteinFeature(*(prot.GetCompleteBioseq()), protein_name, cds, scope);
            }
            added = true;
        }
    }
    if (!added) {
        if (cds.IsSetXref()) {
            // see if this seq-feat already has a prot xref
            NON_CONST_ITERATE(CSeq_feat::TXref, it, cds.SetXref()) {
                if ((*it)->IsSetData() && (*it)->GetData().IsProt()) {
                    SetProteinName((*it)->SetData().SetProt(), protein_name, append);
                    added = true;
                    break;
                }
            }
        }
        if (!added) {
            CRef<CSeqFeatXref> xref(new CSeqFeatXref());
            xref->SetData().SetProt().SetName().push_back(protein_name);
            cds.SetXref().push_back(xref);
        }
    }
}


const string& CCleanup::GetProteinName(const CProt_ref& prot)
{
    if (prot.IsSetName() && !prot.GetName().empty()) {
        return prot.GetName().front();
    } else {
        return kEmptyStr;
    }
}


const string& CCleanup::GetProteinName(const CSeq_feat& cds, CScope& scope)
{
    if (cds.IsSetProduct()) {
        CBioseq_Handle prot = scope.GetBioseqHandle(cds.GetProduct());
        if (prot) {
            CFeat_CI f(prot, CSeqFeatData::eSubtype_prot);
            if (f) {
                return GetProteinName(f->GetData().GetProt());
            }
        }
    }
    if (cds.IsSetXref()) {
        ITERATE(CSeq_feat::TXref, it, cds.GetXref()) {
            if ((*it)->IsSetData() && (*it)->GetData().IsProt()) {
                return GetProteinName((*it)->GetData().GetProt());
            }
        }
    }
    return kEmptyStr;
}


bool CCleanup::SetCDSPartialsByFrameAndTranslation(CSeq_feat& cds, CScope& scope)
{
    bool any_change = false;

    if (!cds.GetLocation().IsPartialStart(eExtreme_Biological) &&
        cds.GetData().GetCdregion().IsSetFrame() &&
        cds.GetData().GetCdregion().GetFrame() != CCdregion::eFrame_not_set &&
        cds.GetData().GetCdregion().GetFrame() != CCdregion::eFrame_one) {
        cds.SetLocation().SetPartialStart(true, eExtreme_Biological);
        any_change = true;
    }

    if (!cds.GetLocation().IsPartialStart(eExtreme_Biological) || !cds.GetLocation().IsPartialStop(eExtreme_Biological)) {
        // look for start and stop codon
        string transl_prot;
        try {
            CSeqTranslator::Translate(cds, scope, transl_prot,
                true,   // include stop codons
                false);  // do not remove trailing X/B/Z

        } catch (const runtime_error&) {
        }
        if (!NStr::IsBlank(transl_prot)) {
            if (!cds.GetLocation().IsPartialStart(eExtreme_Biological) && !NStr::StartsWith(transl_prot, "M")) {
                cds.SetLocation().SetPartialStart(true, eExtreme_Biological);
                any_change = true;
            }
            if (!cds.GetLocation().IsPartialStop(eExtreme_Biological) && !NStr::EndsWith(transl_prot, "*")) {
                cds.SetLocation().SetPartialStop(true, eExtreme_Biological);
                any_change = true;
            }
        }
    }

    any_change |= feature::AdjustFeaturePartialFlagForLocation(cds);

    return any_change;
}


bool CCleanup::ClearInternalPartials(CSeq_loc& loc, bool is_first, bool is_last)
{
    bool rval = false;
    switch (loc.Which()) {
        case CSeq_loc::e_Mix:
            rval |= ClearInternalPartials(loc.SetMix(), is_first, is_last);
            break;
        case CSeq_loc::e_Packed_int:
            rval |= ClearInternalPartials(loc.SetPacked_int(), is_first, is_last);
            break;
        default:
            break;
    }
    return rval;
}


bool CCleanup::ClearInternalPartials(CSeq_loc_mix& mix, bool is_first, bool is_last)
{
    bool rval = false;
    NON_CONST_ITERATE(CSeq_loc::TMix::Tdata, it, mix.Set()) {
        bool this_is_last = is_last && (*it == mix.Set().back());
        if ((*it)->IsMix() || (*it)->IsPacked_int()) {
            rval |= ClearInternalPartials(**it, is_first, this_is_last);
        } else {
            if (!is_first &&
                (*it)->IsPartialStart(eExtreme_Biological)) {
                (*it)->SetPartialStart(false, eExtreme_Biological);
                rval = true;
            }
            if (!this_is_last &&
                (*it)->IsPartialStop(eExtreme_Biological)) {
                (*it)->SetPartialStop(false, eExtreme_Biological);
                rval = true;
            }
        }
        is_first = false;
    }
    return rval;
}


bool CCleanup::ClearInternalPartials(CPacked_seqint& pint, bool is_first, bool is_last)
{
    bool rval = false;

    NON_CONST_ITERATE(CSeq_loc::TPacked_int::Tdata, it, pint.Set()) {
        bool this_is_last = is_last && (*it == pint.Set().back());
        if (!is_first && (*it)->IsPartialStart(eExtreme_Biological)) {
            (*it)->SetPartialStart(false, eExtreme_Biological);
            rval = true;
        }
        if (!this_is_last && (*it)->IsPartialStop(eExtreme_Biological)) {
            (*it)->SetPartialStop(false, eExtreme_Biological);
            rval = true;
        }
        is_first = false;
    }
    return rval;
}


bool CCleanup::ClearInternalPartials(CSeq_entry_Handle seh)
{
    bool rval = false;
    CFeat_CI f(seh);
    while (f) {
        CRef<CSeq_feat> new_feat(new CSeq_feat());
        new_feat->Assign(*(f->GetSeq_feat()));
        if (ClearInternalPartials(new_feat->SetLocation())) {
            CSeq_feat_EditHandle eh(f->GetSeq_feat_Handle());
            eh.Replace(*new_feat);
        }
        ++f;
    }

    return rval;
}


bool CCleanup::SetFeaturePartial(CSeq_feat& f)
{
    if (!f.IsSetLocation()) {
        return false;
    }
    bool partial = false;
    CSeq_loc_CI li(f.GetLocation());
    while (li && !partial) {
        if (li.GetFuzzFrom() || li.GetFuzzTo()) {
            partial = true;
            break;
        }
        ++li;
    }
    bool changed = false;
    if (f.IsSetPartial() && f.GetPartial()) {
        if (!partial) {
            f.ResetPartial();
            changed = true;
        }
    } else {
        if (partial) {
            f.SetPartial(true);
            changed = true;
        }
    }
    return changed;
}


bool CCleanup::UpdateECNumbers(CProt_ref::TEc & ec_num_list)
{
    bool changed = false;
    // CProt_ref::TEc is a list, so the iterator stays valid even if we 
    // add new entries after the current one
    NON_CONST_ITERATE(CProt_ref::TEc, ec_num_iter, ec_num_list) {
        string & ec_num = *ec_num_iter;
        size_t tlen = ec_num.length();
        CleanVisStringJunk(ec_num);
        if (tlen != ec_num.length()) {
            changed = true;
        }
        if (CProt_ref::GetECNumberStatus(ec_num) == CProt_ref::eEC_replaced &&
            !CProt_ref::IsECNumberSplit(ec_num)) {
            string new_val = CProt_ref::GetECNumberReplacement(ec_num);
            if (!NStr::IsBlank(new_val)) {
                ec_num = new_val;
                changed = true;
            }
        }

    }
    return changed;
}


bool CCleanup::RemoveBadECNumbers(CProt_ref::TEc & ec_num_list)
{
    bool changed = false;
    CProt_ref::TEc::iterator ec_num_iter = ec_num_list.begin();
    while (ec_num_iter != ec_num_list.end()) {
        string & ec_num = *ec_num_iter;
        size_t tlen = ec_num.length();
        CleanVisStringJunk(ec_num);
        if (tlen != ec_num.length()) {
            changed = true;
        }
        CProt_ref::EECNumberStatus ec_status = CProt_ref::GetECNumberStatus(ec_num);
        if (ec_status == CProt_ref::eEC_deleted || ec_status == CProt_ref::eEC_unknown || CProt_ref::IsECNumberSplit(ec_num)) {
            ec_num_iter = ec_num_list.erase(ec_num_iter);
            changed = true;
        } else {
            ++ec_num_iter;
        }

    }
    return changed;
}


bool CCleanup::FixECNumbers(CSeq_entry_Handle entry)
{
    bool any_change = false;
    CFeat_CI f(entry, CSeqFeatData::e_Prot);
    while (f) {
        if (f->GetData().GetProt().IsSetEc()) {
            bool this_change = false;
            CRef<CSeq_feat> new_feat(new CSeq_feat());
            new_feat->Assign(*(f->GetSeq_feat()));
            this_change = UpdateECNumbers(new_feat->SetData().SetProt().SetEc());
            this_change |= RemoveBadECNumbers(new_feat->SetData().SetProt().SetEc());
            if (new_feat->GetData().GetProt().GetEc().empty()) {
                new_feat->SetData().SetProt().ResetEc();
                this_change = true;
            }
            if (this_change) {
                CSeq_feat_EditHandle efh(*f);
                efh.Replace(*new_feat);
            }
        }
        ++f;
    }
    return any_change;
}


bool CCleanup::SetGenePartialByLongestContainedFeature(CSeq_feat& gene, CScope& scope)
{
    CBioseq_Handle bh = scope.GetBioseqHandle(gene.GetLocation());
    if (!bh) {
        return false;
    }
    CFeat_CI under(scope, gene.GetLocation());
    size_t longest = 0;
    CConstRef<CSeq_feat> longest_feat(NULL);
    
    while (under) {
        // ignore genes
        if (under->GetData().IsGene()) {

        } else {
            // must be contained in gene location
            sequence::ECompare loc_cmp = sequence::Compare(gene.GetLocation(), under->GetLocation(), &scope, sequence::fCompareOverlapping);
            
            if (loc_cmp == sequence::eSame || loc_cmp == sequence::eContains) {
                size_t len = sequence::GetLength(under->GetLocation(), &scope);
                // if longer than longest, record new length and feature
                if (len > longest) {
                    longest_feat.Reset(under->GetSeq_feat());
                }
            }
        }

        ++under;
    }
    bool changed = false;
    if (longest_feat) {
        changed = feature::CopyFeaturePartials(gene, *longest_feat);
    }
    return changed;
}


bool CCleanup::SetMolinfoTech(CBioseq_Handle bsh, CMolInfo::ETech tech)
{
    CSeqdesc_CI di(bsh, CSeqdesc::e_Molinfo);
    if (di) {
        if (di->GetMolinfo().IsSetTech() && di->GetMolinfo().GetTech() == tech) {
            // no change necessary
            return false;
        } else {
            CSeqdesc* d = const_cast<CSeqdesc*>(&(*di));
            d->SetMolinfo().SetTech(tech);
            return true;
        }
    }
    CRef<CSeqdesc> m(new CSeqdesc());
    m->SetMolinfo().SetTech(tech);
    if (bsh.IsSetInst() && bsh.GetInst().IsSetMol() && bsh.IsAa()) {
        m->SetMolinfo().SetBiomol(CMolInfo::eBiomol_peptide);
    }
    CBioseq_EditHandle eh = bsh.GetEditHandle();
    eh.AddSeqdesc(*m);
    return true;
}


bool CCleanup::SetMolinfoBiomol(CBioseq_Handle bsh, CMolInfo::EBiomol biomol)
{
    CSeqdesc_CI di(bsh, CSeqdesc::e_Molinfo);
    if (di) {
        if (di->GetMolinfo().IsSetTech() && di->GetMolinfo().GetBiomol() == biomol) {
            // no change necessary
            return false;
        } else {
            CSeqdesc* d = const_cast<CSeqdesc*>(&(*di));
            d->SetMolinfo().SetBiomol(biomol);
            return true;
        }
    }
    CRef<CSeqdesc> m(new CSeqdesc());
    m->SetMolinfo().SetBiomol(biomol);
    CBioseq_EditHandle eh = bsh.GetEditHandle();
    eh.AddSeqdesc(*m);
    return true;
}


bool CCleanup::AddMissingMolInfo(CBioseq& seq, bool is_product)
{
    if (!seq.IsSetInst() || !seq.GetInst().IsSetMol()) {
        return false;
    }
    bool needs_molinfo = true;

    if (seq.IsSetDescr()) {
        NON_CONST_ITERATE(CBioseq::TDescr::Tdata, it, seq.SetDescr().Set()) {
            if ((*it)->IsMolinfo()) {
                needs_molinfo = false;
                if (seq.IsAa() && 
                    (!(*it)->GetMolinfo().IsSetBiomol() || 
                     (*it)->GetMolinfo().GetBiomol() == CMolInfo::eBiomol_unknown)) {
                    (*it)->SetMolinfo().SetBiomol(CMolInfo::eBiomol_peptide);
                }
            }
        }
    }
    if (needs_molinfo) {
        if (seq.IsAa()) {
            CRef<CSeqdesc> m(new CSeqdesc());
            m->SetMolinfo().SetBiomol(CMolInfo::eBiomol_peptide);
            if (is_product) {
                m->SetMolinfo().SetTech(CMolInfo::eTech_concept_trans);
            }
            seq.SetDescr().Set().push_back(m);
        } else if (seq.GetInst().GetMol() == CSeq_inst::eMol_rna && is_product) {
            CRef<CSeqdesc> m(new CSeqdesc());
            m->SetMolinfo().SetBiomol(CMolInfo::eBiomol_mRNA);
            m->SetMolinfo().SetTech(CMolInfo::eTech_standard);
            seq.SetDescr().Set().push_back(m);
        }
    }

    return needs_molinfo;
}


bool CCleanup::AddProteinTitle(CBioseq_Handle bsh)
{
    if (!bsh.IsSetInst() || !bsh.GetInst().IsSetMol() || !bsh.IsAa()) {
        return false;
    }
    if (bsh.IsSetId()) {
        ITERATE(CBioseq_Handle::TId, it, bsh.GetId()) {
            // do not add titles for sequences with certain IDs
            switch (it->Which()) {
                case CSeq_id::e_Pir:
                case CSeq_id::e_Swissprot:
                case CSeq_id::e_Patent:
                case CSeq_id::e_Prf:
                case CSeq_id::e_Pdb:
                    return false;
                    break;
                default:
                    break;
            }
        }
    }

    string new_defline = sequence::CDeflineGenerator().GenerateDefline(bsh, sequence::CDeflineGenerator::fIgnoreExisting);

    CAutoAddDesc title_desc(bsh.GetEditHandle().SetDescr(), CSeqdesc::e_Title);

    bool modified = title_desc.Set().SetTitle() != new_defline; // get or create a title
    if (modified)
      title_desc.Set().SetTitle().swap(new_defline);
    return modified;
}


bool CCleanup::RemoveNcbiCleanupObject(CSeq_entry &seq_entry)
{
    bool rval = false;
    if (seq_entry.IsSetDescr()) {
        CBioseq::TDescr::Tdata::iterator it = seq_entry.SetDescr().Set().begin();
        while (it != seq_entry.SetDescr().Set().end()) {
            if ((*it)->IsUser() && (*it)->GetUser().GetObjectType() == CUser_object::eObjectType_Cleanup){
                it = seq_entry.SetDescr().Set().erase(it);
                rval = true;
            }
            else {
                ++it;
            }
        }
        if (seq_entry.SetDescr().Set().empty()) {
            if (seq_entry.IsSeq()) {
                seq_entry.SetSeq().ResetDescr();
            }
            else if (seq_entry.IsSet()) {
                seq_entry.SetSet().ResetDescr();
            }
        }
    }
    if (seq_entry.IsSet() && seq_entry.GetSet().IsSetSeq_set()) {
        NON_CONST_ITERATE(CBioseq_set::TSeq_set, it, seq_entry.SetSet().SetSeq_set()) {
            rval |= RemoveNcbiCleanupObject(**it);
        }
    }
    return rval;
}


void GetSourceDescriptors(const CSeq_entry& se, vector<const CSeqdesc* >& src_descs)
{
    if (se.IsSetDescr()) {
        ITERATE(CBioseq::TDescr::Tdata, it, se.GetDescr().Get()) {
            if ((*it)->IsSource() && (*it)->GetSource().IsSetOrg()) {
                src_descs.push_back(*it);
            }
        }
    }

    if (se.IsSet() && se.GetSet().IsSetSeq_set()) {
        ITERATE(CBioseq_set::TSeq_set, it, se.GetSet().GetSeq_set()) {
            GetSourceDescriptors(**it, src_descs);
        }
    }
}


bool CCleanup::TaxonomyLookup(CSeq_entry_Handle seh)
{
    bool any_changes = false;

    vector<CRef<COrg_ref> > rq_list;
    vector<const CSeqdesc* > src_descs;
    vector<CConstRef<CSeq_feat> > src_feats;

    GetSourceDescriptors(*(seh.GetCompleteSeq_entry()), src_descs);
    vector<const CSeqdesc* >::iterator desc_it = src_descs.begin();
    while (desc_it != src_descs.end()) {
        // add org ref for descriptor to request list
        CRef<COrg_ref> org(new COrg_ref());
        org->Assign((*desc_it)->GetSource().GetOrg());
        rq_list.push_back(org);

        ++desc_it;
    }

    CFeat_CI feat(seh, SAnnotSelector(CSeqFeatData::e_Biosrc));
    while (feat) {
        if (feat->GetData().GetBiosrc().IsSetOrg()) {
            // add org ref for feature to request list
            CRef<COrg_ref> org(new COrg_ref());
            org->Assign(feat->GetData().GetBiosrc().GetOrg());
            rq_list.push_back(org);
            // add feature to list
            src_feats.push_back(feat->GetOriginalSeq_feat());
        }
        ++feat;
    }

    if (rq_list.size() > 0) {
        CTaxon3 taxon3;
        taxon3.Init();
        CRef<CTaxon3_reply> reply = taxon3.SendOrgRefList(rq_list);
        if (reply) {
            CTaxon3_reply::TReply::const_iterator reply_it = reply->GetReply().begin();

            // process descriptor responses
            desc_it = src_descs.begin();

            while (reply_it != reply->GetReply().end()
                && desc_it != src_descs.end()) {
                if ((*reply_it)->IsData() &&
                    !(*desc_it)->GetSource().GetOrg().Equals((*reply_it)->GetData().GetOrg())) {
                    any_changes = true;
                    CSeqdesc* desc = const_cast<CSeqdesc*>(*desc_it);
                    desc->SetSource().SetOrg().Assign((*reply_it)->GetData().GetOrg());
                    desc->SetSource().SetOrg().ResetSyn();
                }
                ++reply_it;
                ++desc_it;
            }

            // process feature responses
            vector<CConstRef<CSeq_feat> >::iterator feat_it = src_feats.begin();
            while (reply_it != reply->GetReply().end()
                && feat_it != src_feats.end()) {
                if ((*reply_it)->IsData() &&
                    !(*feat_it)->GetData().GetBiosrc().GetOrg().Equals((*reply_it)->GetData().GetOrg())) {
                    any_changes = true;
                    CRef<CSeq_feat> new_feat(new CSeq_feat());
                    new_feat->Assign(**feat_it);
                    new_feat->SetData().SetBiosrc().SetOrg().Assign((*reply_it)->GetData().GetOrg());
                    CSeq_feat_Handle fh = seh.GetScope().GetSeq_featHandle(**feat_it);
                    CSeq_feat_EditHandle efh(fh);
                    efh.Replace(*new_feat);
                }
                ++reply_it;
                ++feat_it;
            }
        }
    }

    return any_changes;
}

CRef<CSeq_entry> AddProtein(const CSeq_feat& cds, CScope& scope)
{
    CBioseq_Handle cds_bsh = scope.GetBioseqHandle(cds.GetLocation());
    if (!cds_bsh) {
        return CRef<CSeq_entry>(NULL);
    }
    CSeq_entry_Handle seh = cds_bsh.GetSeq_entry_Handle();
    if (!seh) {
        return CRef<CSeq_entry>(NULL);
    }

    CRef<CBioseq> new_product = CSeqTranslator::TranslateToProtein(cds, scope);
    if (new_product.Empty()) {
        return CRef<CSeq_entry>(NULL);
    }

    CRef<CSeqdesc> molinfo(new CSeqdesc());
    molinfo->SetMolinfo().SetBiomol(CMolInfo::eBiomol_peptide);
    molinfo->SetMolinfo().SetTech(CMolInfo::eTech_concept_trans_a);
    new_product->SetDescr().Set().push_back(molinfo);

    if (cds.IsSetProduct()) {
        CRef<CSeq_id> prot_id(new CSeq_id());
        prot_id->Assign(*(cds.GetProduct().GetId()));
        new_product->SetId().push_back(prot_id);
    }
    CRef<CSeq_entry> prot_entry(new CSeq_entry());
    prot_entry->SetSeq(*new_product);

    CSeq_entry_EditHandle eh = seh.GetEditHandle();
    if (!eh.IsSet()) {
        CBioseq_set_Handle nuc_parent = eh.GetParentBioseq_set();
        if (nuc_parent && nuc_parent.IsSetClass() && nuc_parent.GetClass() == objects::CBioseq_set::eClass_nuc_prot) {
            eh = nuc_parent.GetParentEntry().GetEditHandle();
        }
    }
    if (!eh.IsSet()) {
        eh.ConvertSeqToSet();
        // move all descriptors on nucleotide sequence except molinfo, title, and create-date to set
        eh.SetSet().SetClass(CBioseq_set::eClass_nuc_prot);
        CConstRef<CBioseq_set> set = eh.GetSet().GetCompleteBioseq_set();
        if (set && set->IsSetSeq_set()) {
            CConstRef<CSeq_entry> nuc = set->GetSeq_set().front();
            CSeq_entry_EditHandle neh = eh.GetScope().GetSeq_entryEditHandle(*nuc);
            CBioseq_set::TDescr::Tdata::const_iterator it = nuc->GetDescr().Get().begin();
            while (it != nuc->GetDescr().Get().end()) {
                if (!(*it)->IsMolinfo() && !(*it)->IsTitle() && !(*it)->IsCreate_date()) {
                    CRef<CSeqdesc> copy(new CSeqdesc());
                    copy->Assign(**it);
                    eh.AddSeqdesc(*copy);
                    neh.RemoveSeqdesc(**it);
                    if (nuc->IsSetDescr()) {
                        it = nuc->GetDescr().Get().begin();
                    }
                    else {
                        break;
                    }
                }
                else {
                    ++it;
                }
            }
        }
    }

    CSeq_entry_EditHandle added = eh.AttachEntry(*prot_entry);
    return prot_entry;
}


CRef<objects::CSeq_id> GetNewProteinId(size_t& offset, objects::CSeq_entry_Handle seh, objects::CBioseq_Handle bsh)
{
    string id_base;
    objects::CSeq_id_Handle hid;

    ITERATE(objects::CBioseq_Handle::TId, it, bsh.GetId()) {
        if (!hid || !it->IsBetter(hid)) {
            hid = *it;
        }
    }

    hid.GetSeqId()->GetLabel(&id_base, objects::CSeq_id::eContent);

    CRef<objects::CSeq_id> id(new objects::CSeq_id());
    string& id_label = id->SetLocal().SetStr();
    objects::CBioseq_Handle b_found;
    do
    {
        id_label = id_base + "_" + NStr::NumericToString(offset++);
        b_found = seh.GetBioseqHandle(*id);
    } while (b_found);

    return id;
}


bool CCleanup::SetGeneticCodes(CBioseq_Handle bsh)
{
    if (!bsh) {
        return false;
    }
    if (!bsh.IsNa()) {
        return false;
    }

    int bioseqGenCode = 0;
    CSeqdesc_CI src(bsh, CSeqdesc::e_Source);
    if (src) {
        bioseqGenCode = src->GetSource().GetGenCode();
    }    

    bool any_changed = false;
    // set Cdregion's gcode from BioSource (unless except-text)
    SAnnotSelector sel(CSeqFeatData::e_Cdregion);
    CFeat_CI feat_ci(bsh, sel);
    for (; feat_ci; ++feat_ci) {
        const CSeq_feat& feat = feat_ci->GetOriginalFeature();
        const CCdregion& cds = feat.GetData().GetCdregion();
        int cdregionGenCode = (cds.IsSetCode() ?
            cds.GetCode().GetId() :
            0);
        if (cdregionGenCode != bioseqGenCode)
        {
            // make cdregion's gencode match bioseq's gencode,
            // if allowed
            if (!feat.HasExceptionText("genetic code exception"))
            {
                CRef<CSeq_feat> new_feat(new CSeq_feat);
                new_feat->Assign(feat);
                CCdregion& new_cds = new_feat->SetData().SetCdregion();
                new_cds.ResetCode();
                new_cds.SetCode().SetId(bioseqGenCode);
                CSeq_feat_EditHandle edit_handle(*feat_ci);
                edit_handle.Replace(*new_feat);
                any_changed = true;
            }
        }
    }
    return any_changed;
}


// return position of " [" + sOrganism + "]", but only if it's
// at the end and there are characters before it.
// Also, returns the position of the organelle prefix in the title.
static SIZE_TYPE s_TitleEndsInOrganism(
    const string & sTitle,
    const string & sOrganism,
    SIZE_TYPE * out_piOrganellePos)
{
    if (out_piOrganellePos) {
        *out_piOrganellePos = NPOS;
    }

    SIZE_TYPE answer = NPOS;

    const string sPattern = " [" + sOrganism + "]";
    if (NStr::EndsWith(sTitle, sPattern, NStr::eNocase)) {
        answer = sTitle.length() - sPattern.length();
        if (answer < 1) {
            // title must have something before the pattern
            answer = NPOS;
        }
    } else {
        answer = NStr::FindNoCase(sTitle, sPattern, 0, NPOS, NStr::eLast);
        if (answer < 1 || answer == NPOS) {
            // pattern not found
            answer = NPOS;
        }
    }

    // find organelle prefix
    if (out_piOrganellePos) {
        for (unsigned int genome = CBioSource::eGenome_chloroplast;
            genome <= CBioSource::eGenome_chromatophore;
            genome++) {
            if (genome != CBioSource::eGenome_extrachrom &&
                genome != CBioSource::eGenome_transposon &&
                genome != CBioSource::eGenome_insertion_seq &&
                genome != CBioSource::eGenome_proviral &&
                genome != CBioSource::eGenome_virion &&
                genome != CBioSource::eGenome_chromosome)
            {
                string organelle = " (" + CBioSource::GetOrganelleByGenome(genome) + ")";
                SIZE_TYPE possible_organelle_start_pos = NStr::Find(sTitle, organelle);
                if (possible_organelle_start_pos != NPOS &&
                    NStr::EndsWith(CTempString(sTitle, 0, answer), organelle)) {
                    *out_piOrganellePos = possible_organelle_start_pos;
                    break;
                }

            }
        }
    }

    return answer;
}

static void s_RemoveOrgFromEndOfProtein(CBioseq& seq, string taxname)

{
    if (taxname.empty()) return;
    SIZE_TYPE taxlen = taxname.length();

    EDIT_EACH_SEQANNOT_ON_BIOSEQ(annot_it, seq) {
        CSeq_annot& annot = **annot_it;
        if (!annot.IsFtable()) continue;
        EDIT_EACH_FEATURE_ON_ANNOT(feat_it, annot) {
            CSeq_feat& feat = **feat_it;
            CSeqFeatData& data = feat.SetData();
            if (!data.IsProt()) continue;
            CProt_ref& prot_ref = data.SetProt();
            EDIT_EACH_NAME_ON_PROTREF(it, prot_ref) {
                string str = *it;
                if (str.empty()) continue;
                int len = str.length();
                if (len < 5) continue;
                if (str[len - 1] != ']') continue;
                SIZE_TYPE cp = NStr::Find(str, "[", 0, NPOS, NStr::eLast);
                if (cp == NPOS) continue;
                string suffix = str.substr(cp + 1);
                if (NStr::StartsWith(suffix, "NAD")) continue;
                if (suffix.length() != taxlen + 1) continue;
                if (NStr::StartsWith(suffix, taxname)) {
                    str.erase(cp);
                    Asn2gnbkCompressSpaces(str);
                    *it = str;
                }
            }
        }
    }
}

bool CCleanup::AddPartialToProteinTitle(CBioseq &bioseq)
{
    // Bail if not protein
    if (!FIELD_CHAIN_OF_2_IS_SET(bioseq, Inst, Mol) ||
        bioseq.GetInst().GetMol() != NCBI_SEQMOL(aa))
    {
        return false;
    }

    // Bail if record is swissprot
    FOR_EACH_SEQID_ON_BIOSEQ(seqid_itr, bioseq) {
        const CSeq_id& seqid = **seqid_itr;
        if (FIELD_IS(seqid, Swissprot)) {
            return false;
        }
    }

    // gather some info from the Seqdesc's on the bioseq, into
    // the following variables
    bool bPartial = false;
    string sTaxname;
    string sOldName;
    string *psTitle = NULL;
    string organelle = kEmptyStr;

    // iterate for title
    EDIT_EACH_SEQDESC_ON_BIOSEQ(descr_iter, bioseq) {
        CSeqdesc &descr = **descr_iter;
        if (descr.IsTitle()) {
            psTitle = &GET_MUTABLE(descr, Title);
        }
    }

    // iterate Seqdescs from bottom to top
    // accumulate seqdescs into here
    typedef vector< CConstRef<CSeqdesc> > TSeqdescVec;
    TSeqdescVec vecSeqdesc;
    {
        FOR_EACH_SEQDESC_ON_BIOSEQ(descr_iter, bioseq) {
            vecSeqdesc.push_back(CConstRef<CSeqdesc>(&**descr_iter));
        }
        // climb up to get parent Seqdescs
        CConstRef<CBioseq_set> bioseq_set(bioseq.GetParentSet());
        for (; bioseq_set; bioseq_set = bioseq_set->GetParentSet()) {
            FOR_EACH_SEQDESC_ON_SEQSET(descr_iter, *bioseq_set) {
                vecSeqdesc.push_back(CConstRef<CSeqdesc>(&**descr_iter));
            }
        }
    }

    ITERATE(TSeqdescVec, descr_iter, vecSeqdesc) {
        const CSeqdesc &descr = **descr_iter;
        if (descr.IsMolinfo() && FIELD_IS_SET(descr.GetMolinfo(), Completeness)) {
            switch (GET_FIELD(descr.GetMolinfo(), Completeness)) {
            case NCBI_COMPLETENESS(partial):
            case NCBI_COMPLETENESS(no_left):
            case NCBI_COMPLETENESS(no_right):
            case NCBI_COMPLETENESS(no_ends):
                bPartial = true;
                break;
            default:
                break;
            }
            // stop at first molinfo
            break;
        }
    }

    ITERATE(TSeqdescVec, descr_iter, vecSeqdesc) {
        const CSeqdesc &descr = **descr_iter;
        if (descr.IsSource()) {
            const TBIOSOURCE_GENOME genome = (descr.GetSource().CanGetGenome() ?
                descr.GetSource().GetGenome() :
                CBioSource::eGenome_unknown);
            if (genome >= CBioSource::eGenome_chloroplast &&
                genome <= CBioSource::eGenome_chromatophore &&
                genome != CBioSource::eGenome_extrachrom &&
                genome != CBioSource::eGenome_transposon &&
                genome != CBioSource::eGenome_insertion_seq &&
                genome != CBioSource::eGenome_proviral &&
                genome != CBioSource::eGenome_virion &&
                genome != CBioSource::eGenome_chromosome)
            {
                organelle = CBioSource::GetOrganelleByGenome(genome);
            }

            if (FIELD_IS_SET(descr.GetSource(), Org)) {
                const COrg_ref & org = GET_FIELD(descr.GetSource(), Org);
                if (!RAW_FIELD_IS_EMPTY_OR_UNSET(org, Taxname)) {
                    sTaxname = GET_FIELD(org, Taxname);
                }
                if (NStr::StartsWith(sTaxname, organelle, NStr::eNocase)) {
                    organelle = kEmptyStr;
                }
                FOR_EACH_ORGMOD_ON_ORGREF(mod_iter, org) {
                    const COrgMod & orgmod = **mod_iter;
                    if (FIELD_EQUALS(orgmod, Subtype, NCBI_ORGMOD(old_name))) {
                        sOldName = GET_FIELD(orgmod, Subname);
                    }
                }
            }
            // stop at first source
            break;
        }
    }

    s_RemoveOrgFromEndOfProtein(bioseq, sTaxname);

    // bail if no title
    if ((NULL == psTitle) || psTitle->empty()) {
        return false;
    }

    // put title into a reference,
    // just because it's more convenient than a pointer
    string & sTitle = *psTitle;
    // remember original so we can see if we changed it
    const string sOriginalTitle = sTitle;

    // search for partial, must be just before bracketed organism
    SIZE_TYPE partialPos = NStr::Find(sTitle, ", partial [");
    if (partialPos == NPOS) {
        partialPos = NStr::Find(sTitle, ", partial (");
    }

    // find oldname or taxname in brackets at end of protein title
    SIZE_TYPE penult = NPOS;
    SIZE_TYPE suffixPos = NPOS; // will point to " [${organism name}]" at end
    if (!sOldName.empty() && !sTaxname.empty()) {
        suffixPos = s_TitleEndsInOrganism(sTitle, sOldName, &penult);
    }
    if (suffixPos == NPOS && !sTaxname.empty()) {
        suffixPos = s_TitleEndsInOrganism(sTitle, sTaxname, &penult);
        if (suffixPos != NPOS) {
            if (NStr::IsBlank(organelle) && penult != NPOS) {
            } else if (!NStr::IsBlank(organelle) && penult == NPOS) {
            } else if (penult != NPOS && sTitle.substr(penult) == organelle) {
            } else {
                // bail if no need to change partial text or [organism name]
                if (bPartial && partialPos != NPOS) {
                    return false;
                } else if (!bPartial && partialPos == NPOS){
                    return false;
                }
            }
        }
    }
    // do not change unless [genus species] was at the end
    if (suffixPos == NPOS) {
        return false;
    }

    // truncate bracketed info from end of title, will replace with current taxname
    sTitle.resize(suffixPos);
    if (penult != NPOS) {
        sTitle.resize(penult);
    }

    // if ", partial [" was indeed just before the [genus species], it will now be ", partial"
    // Note: 9 is length of ", partial"
    if (!bPartial  &&
        partialPos != string::npos &&
        (partialPos == (sTitle.length() - 9)))
    {
        sTitle.resize(partialPos);
    }
    NStr::TruncateSpacesInPlace(sTitle);

    //
    if (bPartial && partialPos == NPOS) {
        sTitle += ", partial";
    }
    if (!NStr::IsBlank(organelle)) {
        sTitle += " (" + string(organelle) + ")";
    }
    if (!sTaxname.empty()) {
        sTitle += " [" + sTaxname + "]";
    }

    if (sTitle != sOriginalTitle) {
        return true;
    } else {
        return false;
    }
}

bool CCleanup::RemovePseudoProduct(CSeq_feat& cds, CScope& scope)
{
    if (!sequence::IsPseudo(cds, scope) ||
        !cds.IsSetData() || !cds.GetData().IsCdregion() ||
        !cds.IsSetProduct()) {
        return false;
    }
    CBioseq_Handle pseq = scope.GetBioseqHandle(cds.GetProduct());
    if (pseq) {
        CFeat_CI prot(pseq, CSeqFeatData::eSubtype_prot);
        if (prot) {
            string label;
            if (prot->GetData().GetProt().IsSetName() &&
                !prot->GetData().GetProt().GetName().empty()) {
                label = prot->GetData().GetProt().GetName().front();
            } else if (prot->GetData().GetProt().IsSetDesc()) {
                label = prot->GetData().GetProt().GetDesc();
            }
            if (!NStr::IsBlank(label)) {
                if (cds.IsSetComment() && !NStr::IsBlank(cds.GetComment())) {
                    cds.SetComment(cds.GetComment() + "; " + label);
                } else {
                    cds.SetComment(label);
                }
            }
        }
        CBioseq_EditHandle pseq_e(pseq);
        pseq_e.Remove();
    }
    cds.ResetProduct();
    return true;
}


bool CCleanup::ExpandGeneToIncludeChildren(CSeq_feat& gene, CTSE_Handle& tse)
{
    if (!gene.IsSetXref() || !gene.IsSetLocation() || !gene.GetLocation().IsInt()) {
        return false;
    }
    bool any_change = false;
    TSeqPos gene_start = gene.GetLocation().GetStart(eExtreme_Positional);
    TSeqPos gene_stop = gene.GetLocation().GetStop(eExtreme_Positional);
    ITERATE(CSeq_feat::TXref, xit, gene.GetXref()) {
        if ((*xit)->IsSetId() && (*xit)->GetId().IsLocal()) {
            const CTSE_Handle::TFeatureId& feat_id = (*xit)->GetId().GetLocal();
            CTSE_Handle::TSeq_feat_Handles far_feats = tse.GetFeaturesWithId(CSeqFeatData::eSubtype_any, feat_id);
            ITERATE(CTSE_Handle::TSeq_feat_Handles, f, far_feats) {
                TSeqPos f_start = f->GetLocation().GetStart(eExtreme_Positional);
                TSeqPos f_stop = f->GetLocation().GetStop(eExtreme_Positional);
                if (f_start < gene_start) {
                    gene.SetLocation().SetInt().SetFrom(f_start);
                    gene_start = f_start;
                    any_change = true;
                }
                if (f_stop > gene_stop) {
                    gene.SetLocation().SetInt().SetTo(f_stop);
                    gene_stop = f_stop;
                    any_change = true;
                }
            }
        }
    }
    return any_change;
}


bool CCleanup::WGSCleanup(CSeq_entry_Handle entry)
{
    bool any_changes = false;

    size_t protein_id_counter = 1;
    SAnnotSelector sel(CSeqFeatData::e_Cdregion);
    for (CFeat_CI cds_it(entry, sel); cds_it; ++cds_it) {
        bool change_this_cds = false;
        CRef<CSeq_feat> new_cds(new CSeq_feat());
        new_cds->Assign(*(cds_it->GetSeq_feat()));
        if (sequence::IsPseudo(*(cds_it->GetSeq_feat()), entry.GetScope())) {
            change_this_cds = RemovePseudoProduct(*new_cds, entry.GetScope());
        } else {
            change_this_cds |= SetBestFrame(*new_cds, entry.GetScope());

            change_this_cds |= SetCDSPartialsByFrameAndTranslation(*new_cds, entry.GetScope());

            // retranslate
            if (new_cds->IsSetProduct() && entry.GetScope().GetBioseqHandle(new_cds->GetProduct())) {
                any_changes |= feature::RetranslateCDS(*new_cds, entry.GetScope());
            } else {
                // need to set product if not set
                if (!new_cds->IsSetProduct() && !sequence::IsPseudo(*new_cds, entry.GetScope())) {
                    CRef<CSeq_id> new_id = GetNewProteinId(protein_id_counter, entry, entry.GetScope().GetBioseqHandle(new_cds->GetLocation()));
                    if (new_id) {
                        new_cds->SetProduct().SetWhole().Assign(*new_id);
                        change_this_cds = true;
                    }
                }
                if (new_cds->IsSetProduct()) {
                    CRef<CSeq_entry> prot = AddProtein(*new_cds, entry.GetScope());
                    if (prot) {
                        any_changes = true;
                    }
                }
                any_changes |= feature::AdjustForCDSPartials(*new_cds, entry);
            }
            //prefer ncbieaa
            if (new_cds->IsSetProduct()) {
                CBioseq_Handle p = entry.GetScope().GetBioseqHandle(new_cds->GetProduct());
                if (p && p.IsSetInst() && p.GetInst().IsSetSeq_data() && p.GetInst().GetSeq_data().IsIupacaa()) {
                    CBioseq_EditHandle peh(p);
                    string current = p.GetInst().GetSeq_data().GetIupacaa().Get();
                    CRef<CSeq_inst> new_inst(new CSeq_inst());
                    new_inst->Assign(p.GetInst());
                    new_inst->SetSeq_data().SetNcbieaa().Set(current);
                    peh.SetInst(*new_inst);
                    any_changes = true;
                }
            }

            string current_name = GetProteinName(*new_cds, entry.GetScope());
            if (NStr::IsBlank(current_name)) {
                SetProteinName(*new_cds, "hypothetical protein", false, entry.GetScope());
                current_name = "hypothetical protein";
                change_this_cds = true;
            }

            CConstRef<CSeq_feat> mrna = sequence::GetmRNAforCDS(*(cds_it->GetSeq_feat()), entry.GetScope());
            if (mrna) {
                bool change_mrna = false;
                CRef<CSeq_feat> new_mrna(new CSeq_feat());
                new_mrna->Assign(*mrna);
                // Make mRNA name match coding region protein
                string mrna_name = new_mrna->GetData().GetRna().GetRnaProductName();
                if (NStr::IsBlank(mrna_name)
                    || (!NStr::Equal(current_name, "hypothetical protein") &&
                    !NStr::Equal(current_name, mrna_name))) {
                    string remainder;
                    new_mrna->SetData().SetRna().SetRnaProductName(current_name, remainder);
                    change_mrna = true;
                }
                // Adjust mRNA partials to match coding region
                change_mrna |= feature::CopyFeaturePartials(*new_mrna, *new_cds);
                if (change_mrna) {
                    CSeq_feat_Handle fh = entry.GetScope().GetSeq_featHandle(*mrna);
                    CSeq_feat_EditHandle feh(fh);
                    feh.Replace(*new_mrna);
                    any_changes = true;
                }
            }
        }

        if (change_this_cds) {
            CSeq_feat_EditHandle cds_h(*cds_it);
            
            cds_h.Replace(*new_cds);
            any_changes = true;

            //also need to redo protein title
        }

    }

    CTSE_Handle tse = entry.GetTSE_Handle();

    for (CFeat_CI gene_it(entry, SAnnotSelector(CSeqFeatData::e_Gene)); gene_it; ++gene_it) {
        bool change_this_gene;
        CRef<CSeq_feat> new_gene(new CSeq_feat());
        new_gene->Assign(*(gene_it->GetSeq_feat()));

        change_this_gene = ExpandGeneToIncludeChildren(*new_gene, tse);

        change_this_gene |= SetGenePartialByLongestContainedFeature(*new_gene, entry.GetScope());

        if (change_this_gene) {
            CSeq_feat_EditHandle gene_h(*gene_it);
            gene_h.Replace(*new_gene);
            any_changes = true;
        }
    }

    NormalizeDescriptorOrder(entry);

    for (CBioseq_CI bi(entry, CSeq_inst::eMol_na); bi; ++bi) {
        any_changes |= SetGeneticCodes(*bi);
    }

    CRef<CCleanupChange> changes(makeCleanupChange(0));
    CNewCleanup_imp exclean(changes, 0);
    exclean.ExtendedCleanup(entry);

    return any_changes;
}


bool CCleanup::x_HasShortIntron(const CSeq_loc& loc, size_t min_len)
{
    CSeq_loc_CI li(loc);
    while (li && li.IsEmpty()) {
        ++li;
    }
    if (!li) {
        return false;
    }
    while (li) {
        TSeqPos prev_end;
        ENa_strand prev_strand;
        if (li.IsSetStrand() && li.GetStrand() == eNa_strand_minus) {
            prev_end = li.GetRange().GetFrom();
            prev_strand = eNa_strand_minus;
        } else {
            prev_end = li.GetRange().GetTo();
            prev_strand = eNa_strand_plus;
        }
        ++li;
        while (li && li.IsEmpty()) {
            ++li;
        }
        if (li) {
            TSeqPos this_start;
            ENa_strand this_strand;
            if (li.IsSetStrand() && li.GetStrand() == eNa_strand_minus) {
                this_start = li.GetRange().GetTo();
                this_strand = eNa_strand_minus;
            } else {
                this_start = li.GetRange().GetFrom();
                this_strand = eNa_strand_plus;
            }
            if (this_strand == prev_strand) {
                if (abs((long int)this_start - (long int)prev_end) < min_len) {
                    return true;
                }
            }
        }
    }
    return false;
}


const string kLowQualitySequence = "low-quality sequence region";

bool CCleanup::x_AddLowQualityException(CSeq_feat& feat)
{
    bool any_change = false;
    if (!feat.IsSetExcept()) {
        any_change = true;
        feat.SetExcept(true);
    }
    if (!feat.IsSetExcept_text() || NStr::IsBlank(feat.GetExcept_text())) {
        feat.SetExcept_text(kLowQualitySequence);
        any_change = true;
    } else if (NStr::Find(feat.GetExcept_text(), kLowQualitySequence) == string::npos) {
        feat.SetExcept_text(feat.GetExcept_text() + "; " + kLowQualitySequence);
        any_change = true;
    }
    return any_change;
}


bool CCleanup::x_AddLowQualityException(CSeq_entry_Handle entry, CSeqFeatData::ESubtype subtype)
{
    bool any_changes = false;

    SAnnotSelector sel(subtype);
    for (CFeat_CI cds_it(entry, sel); cds_it; ++cds_it) {
        bool change_this_cds = false;
        CRef<CSeq_feat> new_cds(new CSeq_feat());
        new_cds->Assign(*(cds_it->GetSeq_feat()));
        if (!sequence::IsPseudo(*(cds_it->GetSeq_feat()), entry.GetScope()) &&
            x_HasShortIntron(cds_it->GetLocation())) {
            change_this_cds = x_AddLowQualityException(*new_cds);
        }

        if (change_this_cds) {
            CSeq_feat_EditHandle cds_h(*cds_it);

            cds_h.Replace(*new_cds);
            any_changes = true;
        }
    }
    return any_changes;
}


bool CCleanup::AddLowQualityException(CSeq_entry_Handle entry)
{
    bool any_changes = x_AddLowQualityException(entry, CSeqFeatData::eSubtype_cdregion);
    any_changes |= x_AddLowQualityException(entry, CSeqFeatData::eSubtype_mRNA);
    return any_changes;
}


// maps the type of seqdesc to the order it should be in 
// (lowest to highest)
typedef SStaticPair<CSeqdesc::E_Choice, int>  TSeqdescOrderElem;
static const TSeqdescOrderElem sc_seqdesc_order_map[] = {
    // Note that ordering must match ordering
    // in CSeqdesc::E_Choice
        { CSeqdesc::e_Mol_type, 13 },
        { CSeqdesc::e_Modif, 14 },
        { CSeqdesc::e_Method, 15 },
        { CSeqdesc::e_Name, 7 },
        { CSeqdesc::e_Title, 1 },
        { CSeqdesc::e_Org, 16 },
        { CSeqdesc::e_Comment, 6 },
        { CSeqdesc::e_Num, 11 },
        { CSeqdesc::e_Maploc, 9 },
        { CSeqdesc::e_Pir, 18 },
        { CSeqdesc::e_Genbank, 22 },
        { CSeqdesc::e_Pub, 5 },
        { CSeqdesc::e_Region, 10 },
        { CSeqdesc::e_User, 8 },
        { CSeqdesc::e_Sp, 17 },
        { CSeqdesc::e_Dbxref, 12 },
        { CSeqdesc::e_Embl, 21 },
        { CSeqdesc::e_Create_date, 24 },
        { CSeqdesc::e_Update_date, 25 },
        { CSeqdesc::e_Prf, 19 },
        { CSeqdesc::e_Pdb, 20 },
        { CSeqdesc::e_Het, 4 },
        
        
        { CSeqdesc::e_Source, 2 },
        { CSeqdesc::e_Molinfo, 3 },
        { CSeqdesc::e_Modelev, 23 }
};
typedef CStaticPairArrayMap<CSeqdesc::E_Choice, int> TSeqdescOrderMap;
DEFINE_STATIC_ARRAY_MAP(TSeqdescOrderMap, sc_SeqdescOrderMap, sc_seqdesc_order_map);

static
int s_SeqDescToOrdering(CSeqdesc::E_Choice chs) {
    // ordering assigned to unknown
    const int unknown_seqdesc = (1 + sc_SeqdescOrderMap.size());

    TSeqdescOrderMap::const_iterator find_iter = sc_SeqdescOrderMap.find(chs);
    if (find_iter == sc_SeqdescOrderMap.end()) {
        return unknown_seqdesc;
    }

    return find_iter->second;
}

static
bool s_SeqDescLessThan(const CRef<CSeqdesc> &desc1, const CRef<CSeqdesc> &desc2)
{
    CSeqdesc::E_Choice chs1, chs2;

    chs1 = desc1->Which();
    chs2 = desc2->Which();

    return (s_SeqDescToOrdering(chs1) < s_SeqDescToOrdering(chs2));
}

bool CCleanup::NormalizeDescriptorOrder(CSeq_descr& descr)
{
    bool rval = false;
    if (!seq_mac_is_sorted(descr.Set().begin(), descr.Set().end(), s_SeqDescLessThan)) {
        descr.Set().sort(s_SeqDescLessThan);
        rval = true;
    }
    return rval;
}

bool CCleanup::NormalizeDescriptorOrder(CSeq_entry_Handle seh)
{
    bool rval = false;

    CSeq_entry_CI ci(seh, CSeq_entry_CI::fRecursive);
    while (ci) {
        CSeq_entry_EditHandle edit(*ci);
        if (edit.IsSetDescr()) {
            rval |= NormalizeDescriptorOrder(edit.SetDescr());
        }
        ++ci;
    }

    return rval;
}


bool CCleanup::RemoveUnseenTitles(CSeq_entry_EditHandle::TSeq seq)
{
    bool removed = false;
    if (seq.IsSetDescr()) {
        CConstRef<CSeqdesc> last_title(NULL);
        ITERATE(CBioseq::TDescr::Tdata, d, seq.GetDescr().Get()) {
            if ((*d)->IsTitle()) {
                if (last_title) {
                    seq.RemoveSeqdesc(*last_title);
                    removed = true;
                }
                last_title.Reset(d->GetPointer());
            }
        }
    }
    return removed;
}


bool CCleanup::RemoveUnseenTitles(CSeq_entry_EditHandle::TSet set)
{
    bool removed = false;
    if (set.IsSetDescr()) {
        CConstRef<CSeqdesc> last_title(NULL);
        ITERATE(CBioseq::TDescr::Tdata, d, set.GetDescr().Get()) {
            if ((*d)->IsTitle()) {
                if (last_title) {
                    set.RemoveSeqdesc(*last_title);
                    removed = true;
                }
                last_title.Reset(d->GetPointer());
            }
        }
    }
    return removed;
}


bool CCleanup::AddGenBankWrapper(CSeq_entry_Handle seh)
{
    if (seh.IsSet() && seh.GetSet().IsSetClass() &&
        seh.GetSet().GetClass() == CBioseq_set::eClass_genbank) {
        return false;
    }
    CSeq_entry_EditHandle eh(seh);
    eh.ConvertSeqToSet(CBioseq_set::eClass_genbank);
    return true;
}


void s_GetAuthorsString(string *out_authors, const CAuth_list& auth_list)
{
    string & auth_str = *out_authors;
    auth_str.clear();

    if (!auth_list.IsSetNames()) {
        return;
    }

    vector<string> name_list;

    if (auth_list.GetNames().IsStd()) {
        ITERATE(CAuth_list::TNames::TStd, auth_it, auth_list.GetNames().GetStd()) {
            if ((*auth_it)->IsSetName()) {
                string label = "";
                (*auth_it)->GetName().GetLabel(&label);
                name_list.push_back(label);
            }
        }
    } else if (auth_list.GetNames().IsMl()) {
        copy(BEGIN_COMMA_END(auth_list.GetNames().GetMl()),
            back_inserter(name_list));
    } else if (auth_list.GetNames().IsStr()) {
        copy(BEGIN_COMMA_END(auth_list.GetNames().GetStr()),
            back_inserter(name_list));
    }

    if (name_list.size() == 0) {
        return;
    } else if (name_list.size() == 1) {
        auth_str = name_list.back();
        return;
    }

    // join most of them by commas, but the last one gets an "and"
    string last_author;
    last_author.swap(name_list.back());
    name_list.pop_back();
    // swap is faster than assignment
    NStr::Join(name_list, ", ").swap(auth_str);
    auth_str += "and ";
    auth_str += last_author;

    return;
}


void s_GetAuthorsString(
    string *out_authors_string, const CPubdesc& pd)
{
    string & authors_string = *out_authors_string;
    authors_string.clear();

    FOR_EACH_PUB_ON_PUBDESC(pub, pd) {
        if ((*pub)->IsSetAuthors()) {
            s_GetAuthorsString(&authors_string, (*pub)->GetAuthors());
            break;
        }
    }
}


void CCleanup::GetPubdescLabels
(const CPubdesc& pd,
vector<int>& pmids, vector<int>& muids, vector<int>& serials,
vector<string>& published_labels,
vector<string>& unpublished_labels)
{
    string label = "";
    bool   is_published = false;
    bool   need_label = false;

    if (!pd.IsSetPub()) {
        return;
    }
    ITERATE(CPubdesc::TPub::Tdata, it, pd.GetPub().Get()) {
        if ((*it)->IsPmid()) {
            pmids.push_back((*it)->GetPmid());
            is_published = true;
        } else if ((*it)->IsMuid()) {
            muids.push_back((*it)->GetMuid());
            is_published = true;
        } else if ((*it)->IsGen()) {
            if ((*it)->GetGen().IsSetCit()
                && NStr::StartsWith((*it)->GetGen().GetCit(), "BackBone id_pub", NStr::eNocase)) {
                need_label = true;
            }
            if ((*it)->GetGen().IsSetSerial_number()) {
                serials.push_back((*it)->GetGen().GetSerial_number());
                if ((*it)->GetGen().IsSetCit()
                    || (*it)->GetGen().IsSetJournal()
                    || (*it)->GetGen().IsSetDate()) {
                    need_label = true;
                }
            } else {
                need_label = true;
            }
        } else if ((*it)->IsArticle() && (*it)->GetArticle().IsSetIds()) {
            is_published = true;
            ITERATE(CArticleIdSet::Tdata, id, (*it)->GetArticle().GetIds().Get()) {
                if ((*id)->IsPubmed()) {
                    pmids.push_back((*id)->GetPubmed());
                    is_published = true;
                } else if ((*id)->IsMedline()) {
                    muids.push_back((*id)->GetMedline());
                }
            }
            need_label = true;
        } else {
            need_label = true;
        }
        if (need_label && NStr::IsBlank(label)) {
            // create unique label
            (*it)->GetLabel(&label, CPub::eContent, true);
            string auth_str;
            s_GetAuthorsString(&auth_str, pd);
            label += "; ";
            label += auth_str;
        }
    }
    if (!NStr::IsBlank(label)) {
        if (is_published) {
            published_labels.push_back(label);
        } else {
            unpublished_labels.push_back(label);
        }
    }
}


vector<CConstRef<CPub> > CCleanup::GetCitationList(CBioseq_Handle bsh)
{
    vector<CConstRef<CPub> > pub_list;

    // first get descriptor pubs
    CSeqdesc_CI di(bsh, CSeqdesc::e_Pub);
    while (di) {
        vector<int> pmids;
        vector<int> muids;
        vector<int> serials;
        vector<string> published_labels;
        vector<string> unpublished_labels;
        GetPubdescLabels(di->GetPub(), pmids, muids, serials, published_labels, unpublished_labels);
        if (pmids.size() > 0) {
            CRef<CPub> pub(new CPub());
            pub->SetPmid().Set(pmids[0]);
            pub_list.push_back(pub);
        } else if (muids.size() > 0) {
            CRef<CPub> pub(new CPub());
            pub->SetMuid(muids[0]);
            pub_list.push_back(pub);
        } else if (serials.size() > 0) {
            CRef<CPub> pub(new CPub());
            pub->SetGen().SetSerial_number(serials[0]);
            pub_list.push_back(pub);
        } else if (published_labels.size() > 0) {
            CRef<CPub> pub(new CPub());
            pub->SetGen().SetCit(published_labels[0]);
            pub_list.push_back(pub);
        } else if (unpublished_labels.size() > 0) {
            CRef<CPub> pub(new CPub());
            pub->SetGen().SetCit(unpublished_labels[0]);
            pub_list.push_back(pub);
        }

        ++di;
    }
    // now get pub features
    CFeat_CI fi(bsh, SAnnotSelector(CSeqFeatData::e_Pub));
    while (fi) {
        vector<int> pmids;
        vector<int> muids;
        vector<int> serials;
        vector<string> published_labels;
        vector<string> unpublished_labels;
        GetPubdescLabels(fi->GetData().GetPub(), pmids, muids, serials, published_labels, unpublished_labels);
        if (pmids.size() > 0) {
            CRef<CPub> pub(new CPub());
            pub->SetPmid().Set(pmids[0]);
            pub_list.push_back(pub);
        } else if (muids.size() > 0) {
            CRef<CPub> pub(new CPub());
            pub->SetMuid(muids[0]);
            pub_list.push_back(pub);
        } else if (serials.size() > 0) {
            CRef<CPub> pub(new CPub());
            pub->SetGen().SetSerial_number(serials[0]);
            pub_list.push_back(pub);
        } else if (published_labels.size() > 0) {
            CRef<CPub> pub(new CPub());
            pub->SetGen().SetCit(published_labels[0]);
            pub_list.push_back(pub);
        } else if (unpublished_labels.size() > 0) {
            CRef<CPub> pub(new CPub());
            pub->SetGen().SetCit(unpublished_labels[0]);
            pub_list.push_back(pub);
        }

        ++fi;
    }
    return pub_list;
}


bool CCleanup::RemoveDuplicatePubs(CSeq_descr& descr)
{
    bool any_change = false;
    CSeq_descr::Tdata::iterator it1 = descr.Set().begin();
    while (it1 != descr.Set().end()) {
        if ((*it1)->IsPub()) {
            CSeq_descr::Tdata::iterator it2 = it1;
            ++it2;
            while (it2 != descr.Set().end()) {
                if ((*it2)->IsPub() && (*it1)->GetPub().Equals((*it2)->GetPub())) {
                    it2 = descr.Set().erase(it2);
                    any_change = true;
                } else {
                    ++it2;
                }
            }
        }
        ++it1;
    }
    return any_change;
}


bool s_FirstPubMatchesSecond(const CPubdesc& pd1, const CPubdesc& pd2)
{
    if (pd1.Equals(pd2)) {
        return true;
    } else if (pd1.IsSetPub() && pd2.IsSetPub() && pd1.GetPub().Get().size() == 1) {
        ITERATE(CPubdesc::TPub::Tdata, it, pd2.GetPub().Get()) {
            if (pd1.GetPub().Get().front()->Equals(**it)) {
                return true;
            }
        }
    }
    return false;
}


bool CCleanup::PubAlreadyInSet(const CPubdesc& pd, const CSeq_descr& descr)
{
    ITERATE(CSeq_descr::Tdata, d, descr.Get()) {
        if ((*d)->IsPub() && s_FirstPubMatchesSecond(pd, (*d)->GetPub())) {
            return true;
        }
    }
    return false;
}


bool CCleanup::OkToPromoteNpPub(const CBioseq& b)
{
    bool is_embl_or_ddbj = false;
    ITERATE(CBioseq::TId, id, b.GetId()) {
        if ((*id)->IsEmbl() || (*id)->IsDdbj()) {
            is_embl_or_ddbj = true;
            break;
        }
    }
    return !is_embl_or_ddbj;
}


bool CCleanup::OkToPromoteNpPub(const CPubdesc& pd)
{
    if (pd.IsSetNum() || pd.IsSetName() || pd.IsSetFig() || pd.IsSetComment()) {
        return false;
    } else {
        return true;
    }
}


void CCleanup::MoveOneFeatToPubdesc(CSeq_feat_Handle feat, CRef<CSeqdesc> d, CBioseq_Handle b, bool remove_feat)
{
    // add descriptor to nuc-prot parent or sequence itself
    CBioseq_set_Handle parent = b.GetParentBioseq_set();
    if (!CCleanup::OkToPromoteNpPub(*(b.GetCompleteBioseq()))) {
        // add to sequence
        CBioseq_EditHandle eh(b);
        eh.AddSeqdesc(*d);
        RemoveDuplicatePubs(eh.SetDescr());
        NormalizeDescriptorOrder(eh.SetDescr());
    } else if (parent && parent.IsSetClass() &&
        parent.GetClass() == CBioseq_set::eClass_nuc_prot &&
        parent.IsSetDescr() && PubAlreadyInSet(d->GetPub(), parent.GetDescr())) {
        // don't add descriptor, just delete feature 
    } else if (OkToPromoteNpPub((d)->GetPub()) &&
        parent && parent.IsSetClass() &&
        parent.GetClass() == CBioseq_set::eClass_nuc_prot) {
        CBioseq_set_EditHandle eh(parent);
        eh.AddSeqdesc(*d);
        RemoveDuplicatePubs(eh.SetDescr());
        NormalizeDescriptorOrder(eh.SetDescr());
    } else {
        CBioseq_EditHandle eh(b);
        eh.AddSeqdesc(*d);
        RemoveDuplicatePubs(eh.SetDescr());
        NormalizeDescriptorOrder(eh.SetDescr());
    }
    if (remove_feat) {
        // remove feature
        CSeq_feat_EditHandle feh(feat);
        feh.Remove();
    }
}


bool CCleanup::ConvertPubFeatsToPubDescs(CSeq_entry_Handle seh)
{
    bool any_change = false;
    for (CBioseq_CI b(seh); b; ++b) {
        for (CFeat_CI p(*b, CSeqFeatData::e_Pub); p; ++p) {
            if (p->GetLocation().IsInt() &&
                p->GetLocation().GetStart(eExtreme_Biological) == 0 &&
                p->GetLocation().GetStop(eExtreme_Biological) == b->GetBioseqLength() - 1) {
                CRef<CSeqdesc> d(new CSeqdesc());
                d->SetPub().Assign(p->GetData().GetPub());
                if (p->IsSetComment()) {
                    if (d->GetPub().IsSetComment() && !NStr::IsBlank(d->GetPub().GetComment())) {
                        d->SetPub().SetComment(d->GetPub().GetComment() + "; " + p->GetComment());
                    } else {
                        d->SetPub().SetComment();
                    }
                }
                MoveOneFeatToPubdesc(*p, d, *b);
                any_change = true;
            }
        }
    }
    return any_change;
}


bool IsSiteRef(const CSeq_feat& sf)
{
    if (sf.GetData().IsImp() &&
        sf.GetData().GetImp().IsSetKey() &&
        NStr::Equal(sf.GetData().GetImp().GetKey(), "Site-ref")) {
        return true;
    } else {
        return false;
    }
}


bool CCleanup::IsMinPub(const CPubdesc& pd, bool is_refseq_prot)
{
    if (!pd.IsSetPub()) {
        return true;
    }
    bool found_non_minimal = false;
    ITERATE(CPubdesc::TPub::Tdata, it, pd.GetPub().Get()) {
        if ((*it)->IsMuid() || (*it)->IsPmid()) {
            if (is_refseq_prot) {
                found_non_minimal = true;
                break;
            }
        } else if ((*it)->IsGen()) {
            const CCit_gen& gen = (*it)->GetGen();
            if (gen.IsSetCit() && !gen.IsSetJournal() &&
                !gen.IsSetAuthors() && !gen.IsSetVolume() &&
                !gen.IsSetPages()) {
                //minimalish, keep looking
            } else {
                found_non_minimal = true;
            }
        } else {
            found_non_minimal = true;
            break;
        }
    }
    
    return !found_non_minimal;
}


bool CCleanup::RescueSiteRefPubs(CSeq_entry_Handle seh)
{
    bool found_site_ref = false;
    CFeat_CI f(seh, CSeqFeatData::e_Imp);
    while (f && !found_site_ref) {
        if (IsSiteRef(*(f->GetSeq_feat()))) {
            found_site_ref = true;
        }
        ++f;
    }
    if (!found_site_ref) {
        return false;
    }

    bool any_change = false;
    for (CBioseq_CI b(seh); b; ++b) {
        bool is_refseq_prot = false;
        if (b->IsAa()) {
            ITERATE(CBioseq::TId, id_it, b->GetCompleteBioseq()->GetId()) {
                if ((*id_it)->IsOther()) {
                    is_refseq_prot = true;
                    break;
                }
            }
        }

        for (CFeat_CI p(*b); p; ++p) {
            if (!p->IsSetCit() || p->GetCit().Which() != CPub_set::e_Pub) {
                continue;
            }            

            bool is_site_ref = IsSiteRef(*(p->GetSeq_feat()));
            ITERATE(CSeq_feat::TCit::TPub, c, p->GetCit().GetPub()) {
                CRef<CSeqdesc> d(new CSeqdesc());
                if ((*c)->IsEquiv()) {
                    ITERATE(CPub_equiv::Tdata, t, (*c)->GetEquiv().Get()) {
                        CRef<CPub> pub_copy(new CPub());
                        pub_copy->Assign(**t);
                        d->SetPub().SetPub().Set().push_back(pub_copy);
                    }

                } else {
                    CRef<CPub> pub_copy(new CPub());
                    pub_copy->Assign(**c);
                    d->SetPub().SetPub().Set().push_back(pub_copy);
                }
                if (is_site_ref) {
                    d->SetPub().SetReftype(CPubdesc::eReftype_sites);
                } else {
                    d->SetPub().SetReftype(CPubdesc::eReftype_feats);
                }
                CRef<CCleanupChange> changes(makeCleanupChange(0));
                CNewCleanup_imp pubclean(changes, 0);
                pubclean.BasicCleanup(d->SetPub(), ShouldStripPubSerial(*(b->GetCompleteBioseq())));
                if (!IsMinPub(d->SetPub(), is_refseq_prot)) {
                    MoveOneFeatToPubdesc(*p, d, *b, false);
                }
            }
            if (is_site_ref) {

                CSeq_feat_EditHandle feh(*p);
                CSeq_annot_Handle annot = feh.GetAnnot();

                feh.Remove();

                // remove old annot if now empty
                if (CNewCleanup_imp::ShouldRemoveAnnot(*(annot.GetCompleteSeq_annot()))) {
                    CSeq_annot_EditHandle annot_edit(annot);
                    annot_edit.Remove();
                }

            }
            any_change = true;
        }
    }
    return any_change;
}


bool CCleanup::AreBioSourcesMergeable(const CBioSource& src1, const CBioSource& src2)
{
    if (src1.IsSetOrg() && src1.GetOrg().IsSetTaxname() &&
        src2.IsSetOrg() && src2.GetOrg().IsSetTaxname() &&
        NStr::Equal(src1.GetOrg().GetTaxname(), src2.GetOrg().GetTaxname())) {
        return true;
    } else {
        return false;
    }
}


bool CCleanup::MergeDupBioSources(CBioSource& src1, const CBioSource& add)
{
    bool any_change = false;
    // genome
    if ((!src1.IsSetGenome() || src1.GetGenome() == CBioSource::eGenome_unknown) &&
        add.IsSetGenome() && add.GetGenome() != CBioSource::eGenome_unknown) {
        src1.SetGenome(add.GetGenome());
        any_change = true;
    }
    // origin
    if ((!src1.IsSetOrigin() || src1.GetOrigin() == CBioSource::eOrigin_unknown) &&
        add.IsSetOrigin() && add.GetOrigin() != CBioSource::eOrigin_unknown) {
        src1.SetOrigin(add.GetOrigin());
        any_change = true;
    }
    // focus
    if (!src1.IsSetIs_focus() && add.IsSetIs_focus()) {
        src1.SetIs_focus();
        any_change = true;
    }

    // merge subtypes
    if (add.IsSetSubtype()) {
        ITERATE(CBioSource::TSubtype, it, add.GetSubtype()) {
            CRef<CSubSource> a(new CSubSource());
            a->Assign(**it);
            src1.SetSubtype().push_back(a);
        }
        any_change = true;
    }

    x_MergeDupOrgRefs(src1.SetOrg(), add.GetOrg());

    return any_change;
}


bool CCleanup::x_MergeDupOrgNames(COrgName& on1, const COrgName& add)
{
    bool any_change = false;

    // OrgMods
    if (add.IsSetMod()) {
        ITERATE(COrgName::TMod, it, add.GetMod()) {
            CRef<COrgMod> a(new COrgMod());
            a->Assign(**it);
            on1.SetMod().push_back(a);
        }
        any_change = true;
    }

    // gcode
    if ((!on1.IsSetGcode() || on1.GetGcode() == 0) && add.IsSetGcode() && add.GetGcode() != 0) {
        on1.SetGcode(add.GetGcode());
        any_change = true;
    }

    // mgcode
    if ((!on1.IsSetMgcode() || on1.GetMgcode() == 0) && add.IsSetMgcode() && add.GetMgcode() != 0) {
        on1.SetMgcode(add.GetMgcode());
        any_change = true;
    }

    // lineage
    if (!on1.IsSetLineage() && add.IsSetLineage()) {
        on1.SetLineage(add.GetLineage());
        any_change = true;
    }

    // div
    if (!on1.IsSetDiv() && add.IsSetDiv()) {
        on1.SetDiv(add.GetDiv());
        any_change = true;
    }

    return any_change;
}


bool HasMod(const COrg_ref& org, const string& mod)
{
    if (!org.IsSetMod()) {
        return false;
    }
    ITERATE(COrg_ref::TMod, it, org.GetMod()) {
        if (NStr::Equal(*it, mod)) {
            return true;
        }
    }
    return false;
}


bool CCleanup::x_MergeDupOrgRefs(COrg_ref& org1, const COrg_ref& add)
{
    bool any_change = false;
    // mods
    if (add.IsSetMod()) {
        ITERATE(COrg_ref::TMod, it, add.GetMod()) {
            if (!HasMod(org1, *it)) {
                org1.SetMod().push_back(*it);
                any_change = true;
            }
        }
    }

    // dbxrefs
    if (add.IsSetDb()) {
        ITERATE(COrg_ref::TDb, it, add.GetDb()) {
            CRef<CDbtag> a(new CDbtag());
            a->Assign(**it);
            org1.SetDb().push_back(a);
        }
        any_change = true;
    }

    // synonyms
    if (add.IsSetSyn()) {
        ITERATE(COrg_ref::TSyn, it, add.GetSyn()) {
            org1.SetSyn().push_back(*it);
        }
        any_change = true;
    }

    if (add.IsSetOrgname()) {
        any_change |= x_MergeDupOrgNames(org1.SetOrgname(), add.GetOrgname());
    }

    return any_change;
}


bool CCleanup::MergeDupBioSources(CSeq_descr & seq_descr)
{
    bool any_change = false;
    CSeq_descr::Tdata::iterator src1 = seq_descr.Set().begin();
    while (src1 != seq_descr.Set().end()) {
        if ((*src1)->IsSource() && (*src1)->GetSource().IsSetOrg() && (*src1)->GetSource().GetOrg().IsSetTaxname()) {
            CSeq_descr::Tdata::iterator src2 = src1;
            ++src2;
            while (src2 != seq_descr.Set().end()) {
                if ((*src2)->IsSource() &&
                    AreBioSourcesMergeable((*src1)->GetSource(), (*src2)->GetSource())) {
                    MergeDupBioSources((*src1)->SetSource(), (*src2)->GetSource());

                    CRef<CCleanupChange> changes(makeCleanupChange(0));
                    CNewCleanup_imp srcclean(changes, 0);
                    srcclean.ExtendedCleanup((*src1)->SetSource());
                    src2 = seq_descr.Set().erase(src2);
                    any_change = true;
                } else {
                    ++src2;
                }
            }
        }
        ++src1;
    }
    return any_change;
}

/// Remove duplicate biosource descriptors
bool CCleanup::RemoveDupBioSource(CSeq_descr& descr)
{
    bool any_change = false;
    vector<CConstRef<CBioSource> > src_list;
    CSeq_descr::Tdata::iterator d = descr.Set().begin();
    while (d != descr.Set().end()) {
        if ((*d)->IsSource()) {
            bool found = false;
            ITERATE(vector<CConstRef<CBioSource> >, s, src_list) {
                if ((*d)->GetSource().Equals(**s)) {
                    found = true;
                    break;
                }
            }
            if (found) {
                d = descr.Set().erase(d);
                any_change = true;
            } else {
                CConstRef<CBioSource> src(&((*d)->GetSource()));
                src_list.push_back(src);
                ++d;
            }
        } else {
            ++d;
        }
    }
    return any_change;
}


CRef<CBioSource> CCleanup::BioSrcFromFeat(const CSeq_feat& f)
{
    if (!f.IsSetData() || !f.GetData().IsBiosrc()) {
        return CRef<CBioSource>(NULL);
    }
    CRef<CBioSource> src(new CBioSource());
    src->Assign(f.GetData().GetBiosrc());

    // move comment to subsource note
    if (f.IsSetComment()) {
        CRef<CSubSource> s(new CSubSource());
        s->SetSubtype(CSubSource::eSubtype_other);
        s->SetName(f.GetComment());
        src->SetSubtype().push_back(s);

    }

    // move dbxrefs on feature to source
    if (f.IsSetDbxref()) {
        ITERATE(CSeq_feat::TDbxref, it, f.GetDbxref()) {
            CRef<CDbtag> a(new CDbtag());
            a->Assign(**it);
            src->SetOrg().SetDb().push_back(a);
        }
    }
    CRef<CCleanupChange> changes(makeCleanupChange(0));
    CNewCleanup_imp srcclean(changes, 0);
    srcclean.ExtendedCleanup(*src);

    return src;
}


bool CCleanup::ConvertSrcFeatsToSrcDescs(CSeq_entry_Handle seh)
{
    bool any_change = false;
    for (CBioseq_CI b(seh); b; ++b) {
        bool transgenic_or_focus = false;
        CSeqdesc_CI existing_src(*b, CSeqdesc::e_Source);
        while (existing_src && !transgenic_or_focus) {
            if (existing_src->GetSource().IsSetIs_focus() ||
                existing_src->GetSource().HasSubtype(CSubSource::eSubtype_transgenic)) {
                transgenic_or_focus = true;
            }
            ++existing_src;
        }
        if (transgenic_or_focus) {
            continue;
        }
        for (CFeat_CI p(*b, CSeqFeatData::e_Biosrc); p; ++p) {
            if (p->GetLocation().IsInt() &&
                p->GetLocation().GetStart(eExtreme_Biological) == 0 &&
                p->GetLocation().GetStop(eExtreme_Biological) == b->GetBioseqLength() - 1) {
                CRef<CSeqdesc> d(new CSeqdesc());
                d->SetSource().Assign(*(BioSrcFromFeat(*(p->GetSeq_feat()))));

                // add descriptor to nuc-prot parent or sequence itself
                CBioseq_set_Handle parent = b->GetParentBioseq_set();
                if (parent && parent.IsSetClass() &&
                    parent.GetClass() == CBioseq_set::eClass_nuc_prot) {
                    CBioseq_set_EditHandle eh(parent);
                    eh.AddSeqdesc(*d);
                    MergeDupBioSources(eh.SetDescr());
                    RemoveDupBioSource(eh.SetDescr());
                    NormalizeDescriptorOrder(eh.SetDescr());
                } else {
                    CBioseq_EditHandle eh(*b);
                    eh.AddSeqdesc(*d);
                    MergeDupBioSources(eh.SetDescr());
                    RemoveDupBioSource(eh.SetDescr());
                    NormalizeDescriptorOrder(eh.SetDescr());
                }

                // remove feature
                CSeq_feat_EditHandle feh(*p);
                feh.Remove();

                any_change = true;
            }
        }
    }
    return any_change;
}



bool CCleanup::FixGeneXrefSkew(CSeq_entry_Handle seh)
{
    CFeat_CI fi(seh);
    size_t num_gene_locus = 0;
    size_t num_gene_locus_tag = 0;
    size_t num_gene_xref_locus = 0;
    size_t num_gene_xref_locus_tag = 0;

    while (fi) {
        if (fi->GetData().IsGene()) {
            if (fi->GetData().GetGene().IsSetLocus()) {
                num_gene_locus++;
            }
            if (fi->GetData().GetGene().IsSetLocus_tag()) {
                num_gene_locus_tag++;
            }
        } else if (fi->IsSetXref()) {
            const CGene_ref* g = fi->GetGeneXref();
            if (g) {
                if (g->IsSetLocus()) {
                    num_gene_xref_locus++;
                }
                if (g->IsSetLocus_tag()) {
                    num_gene_xref_locus_tag++;
                }
            }
        }
        if (num_gene_locus > 0) {
            if (num_gene_locus_tag > 0) {
                return false;
            }
            if (num_gene_xref_locus > 0) {
                return false;
            }
        }
        if (num_gene_locus_tag > 0) {
            if (num_gene_locus > 0) {
                return false;
            }
            if (num_gene_xref_locus_tag > 0) {
                return false;
            }
        }
        ++fi;
    }

    bool any_change = false;
    if (num_gene_locus == 0 && num_gene_locus_tag > 0) {
        if (num_gene_xref_locus > 0 && num_gene_xref_locus_tag == 0) {
            fi.Rewind();
            while (fi) {
                if (!fi->GetData().IsGene() && fi->GetGeneXref() != NULL) {
                    bool this_change = false;
                    CRef<CSeq_feat> new_f(new CSeq_feat());
                    new_f->Assign(*(fi->GetSeq_feat()));
                    NON_CONST_ITERATE(CSeq_feat::TXref, it, new_f->SetXref()) {
                        if ((*it)->IsSetData() && (*it)->GetData().IsGene()
                            && (*it)->GetData().GetGene().IsSetLocus()) {
                            (*it)->SetData().SetGene().SetLocus_tag((*it)->GetData().GetGene().GetLocus());
                            (*it)->SetData().SetGene().ResetLocus();
                            this_change = true;
                        }
                    }
                    if (this_change) {
                        CSeq_feat_EditHandle eh(*fi);
                        eh.Replace(*new_f);
                    }
                }
                ++fi;
            }
        }
    } else if (num_gene_locus > 0 && num_gene_locus_tag == 0) {
        if (num_gene_xref_locus == 0 && num_gene_xref_locus_tag > 0) {
            fi.Rewind();
            while (fi) {
                if (!fi->GetData().IsGene() && fi->GetGeneXref() != NULL) {
                    bool this_change = false;
                    CRef<CSeq_feat> new_f(new CSeq_feat());
                    new_f->Assign(*(fi->GetSeq_feat()));
                    NON_CONST_ITERATE(CSeq_feat::TXref, it, new_f->SetXref()) {
                        if ((*it)->IsSetData() && (*it)->GetData().IsGene()
                            && (*it)->GetData().GetGene().IsSetLocus_tag()) {
                            (*it)->SetData().SetGene().SetLocus((*it)->GetData().GetGene().GetLocus_tag());
                            (*it)->SetData().SetGene().ResetLocus_tag();
                            this_change = true;
                        }
                    }
                    if (this_change) {
                        CSeq_feat_EditHandle eh(*fi);
                        eh.Replace(*new_f);
                        any_change = true;
                    }
                }
                ++fi;
            }
        }
    }
    return any_change;
}


bool CCleanup::ShouldStripPubSerial(const CBioseq& bs)
{
    bool strip_serial = true;
    ITERATE(CBioseq::TId, id, bs.GetId()) {
        const CSeq_id& sid = **id;
        switch (sid.Which()) {
        case NCBI_SEQID(Genbank):
        case NCBI_SEQID(Tpg):
        {
            const CTextseq_id& tsid = *GET_FIELD(sid, Textseq_Id);
            if (FIELD_IS_SET(tsid, Accession)) {
                const string& acc = GET_FIELD(tsid, Accession);
                if (acc.length() == 6) {
                    strip_serial = false;
                }
            }
        }
        break;
        case NCBI_SEQID(Embl):
        case NCBI_SEQID(Ddbj):
            strip_serial = false;
            break;
        case NCBI_SEQID(not_set):
        case NCBI_SEQID(Local):
        case NCBI_SEQID(Other):
        case NCBI_SEQID(General):
            break;
        case NCBI_SEQID(Gibbsq):
        case NCBI_SEQID(Gibbmt):
        case NCBI_SEQID(Pir):
        case NCBI_SEQID(Swissprot):
        case NCBI_SEQID(Patent):
        case NCBI_SEQID(Prf):
        case NCBI_SEQID(Pdb):
        case NCBI_SEQID(Gpipe):
        case NCBI_SEQID(Tpe):
        case NCBI_SEQID(Tpd):
            strip_serial = false;
            break;
        default:
            break;
        }
    }
    return strip_serial;
}


bool CCleanup::RenormalizeNucProtSets(CSeq_entry_Handle seh)
{
    bool change_made = false;
    CConstRef<CSeq_entry> entry = seh.GetCompleteSeq_entry();
    if (seh.IsSet() && seh.GetSet().IsSetClass() &&
        entry->GetSet().IsSetSeq_set()) {
        CBioseq_set::TClass set_class = seh.GetSet().GetClass();        
        if (set_class == CBioseq_set::eClass_nuc_prot) {
            if (entry->GetSet().GetSeq_set().size() == 1 &&
                entry->GetSet().GetSeq_set().front()->IsSeq()) {
                CSeq_entry_EditHandle eh = seh.GetEditHandle();
                eh.ConvertSetToSeq();
                if (eh.GetSeq().IsSetDescr()) {
                    RemoveUnseenTitles(eh.SetSeq());
                    NormalizeDescriptorOrder(eh.SetSeq().SetDescr());
                }
                change_made = true;
            }
        } else if (set_class == CBioseq_set::eClass_genbank ||
            set_class == CBioseq_set::eClass_mut_set ||
            set_class == CBioseq_set::eClass_pop_set ||
            set_class == CBioseq_set::eClass_phy_set ||
            set_class == CBioseq_set::eClass_eco_set ||
            set_class == CBioseq_set::eClass_wgs_set ||
            set_class == CBioseq_set::eClass_gen_prod_set ||
            set_class == CBioseq_set::eClass_small_genome_set) {
            ITERATE(CBioseq_set::TSeq_set, s, entry->GetSet().GetSeq_set()) {
                CSeq_entry_Handle ch = seh.GetScope().GetSeq_entryHandle(**s);
                change_made |= RenormalizeNucProtSets(ch);
            }
        }
    }
    return change_made;
}


bool CCleanup::DecodeXMLMarkChanged(std::string & str)
{
// return false;
    bool change_made = false;

    // This is more complex than you might initially think is necessary
    // because this needs to be as efficient as possible since it's
    // called on every single string in an object.

    SIZE_TYPE amp = str.find('&');
    if( NPOS == amp ) {
        // Check for the common case of no replacements required
        return change_made;
    }

    // transformations done by this function:
    const static struct {
        string src_word;
        string result_word;
    } transformations[] = {
        // all start with an implicit ampersand
        // and end with an implicit semi-colon
        { "amp",      "&"      },
        { "apos",     "\'"     },
        { "gt",       ">"      },
        { "lt",       "<"      },
        { "quot",     "\""     },
        { "#13&#10",  ""       },
        { "#13;&#10", ""       },
        { "#916",     "Delta"  },
        { "#945",     "alpha"  },
        { "#946",     "beta"   },
        { "#947",     "gamma"  },
        { "#952",     "theta"  },
        { "#955",     "lambda" },
        { "#956",     "mu"     },
        { "#957",     "nu"     },
        { "#8201",    ""       },
        { "#8206",    ""       },
        { "#8242",    "'"      },
        { "#8594",    "->"     },
        { "#8722",    "-"      },
        { "#8710",    "delta"  },
        { "#64257",   "fi"     },
        { "#64258",   "fl"     },
        { "#65292",   ","      }
    };

    // Collisions should be rare enough that the CFastMutex is
    // faster than recreating the searcher each time this function is called
    static CTextFsm<int> searcher;
    // set searcher's state, if not already done
    {
        // just in case of the tiny chance that two threads try to prime
        // the searcher at the same time.
        static CFastMutex searcher_mtx;
        CFastMutexGuard searcher_mtx_guard( searcher_mtx );
        if( ! searcher.IsPrimed() ) {
            for( size_t idx = 0;
                idx < sizeof(transformations)/sizeof(transformations[0]); 
                ++idx ) 
            {
                // match type is index into transformations array
                searcher.AddWord( transformations[idx].src_word, idx );
            }
            searcher.Prime();
        }
    }

    // a smart compiler probably won't need this manual optimization,
    // but just in case.
    const SIZE_TYPE str_len = str.length();

    // fill result up to the first '&'
    string result;
    result.reserve( str_len ); 
    copy( str.begin(), str.begin() + amp,
        back_inserter(result) );

    // at the start of each loop, the result is filled in
    // up to the ampersand (amp)
    while( amp != NPOS && amp < str_len ) {

        // find out what the ampersand code represents
        // (if it represents anything)
        int state = searcher.GetInitialState();
        SIZE_TYPE search_pos = (amp + 1);
        if (str[search_pos] == ' ') {
            break;
        }
        for( ; search_pos < str_len ; ++search_pos ) {
            const char ch = str[search_pos];
            if( ch == ';' ) {
                break;
            }
            if( ch == '&' && state == 0 ) {
                --search_pos; // so we don't skip over the '&'
                state = searcher.GetInitialState(); // force "no-match"
                break;
            }
            state = searcher.GetNextState(state, ch);
        }

        if( search_pos == str_len && searcher.IsMatchFound(state) ) {
            // copy the translation of the XML code:
            _ASSERT( searcher.GetMatches(state).size() == 1 );
            const int match_idx = searcher.GetMatches(state)[0];
            const string & result_word = transformations[match_idx].result_word;
            copy( result_word.begin(), result_word.end(),
                back_inserter(result) );
            change_made = true;
            break;
        }

        if( search_pos >= str_len ) {
            // we reached the end without finding anything, so
            // copy the rest and break
            copy( str.begin() + amp, str.end(),
                back_inserter(result) );
            break;
        }

        if( searcher.IsMatchFound(state) ) {
            // copy the translation of the XML code:
            _ASSERT( searcher.GetMatches(state).size() == 1 );
            const int match_idx = searcher.GetMatches(state)[0];
            const string & result_word = transformations[match_idx].result_word;
            copy( result_word.begin(), result_word.end(),
                back_inserter(result) );
            change_made = true;
        } else {
            // no match found, so copy the text we looked at
            // as-is
            copy( str.begin() + amp, str.begin() + search_pos + 1,
                back_inserter(result) );
        }

        // find next_amp
        if( str[search_pos] == '&' ) {
            // special case that occurs when there are multiple '&' together
            ++search_pos;
            result += '&';
        }
        SIZE_TYPE next_amp = str.find('&', search_pos );
        if( NPOS == next_amp ) {
            // no more amps; copy the rest and break
            copy( str.begin() + search_pos + 1, str.end(),
                back_inserter(result) );
            break;
        }

        // copy up to the next amp
        if( (search_pos + 1) < next_amp ) {
            copy( str.begin() + search_pos + 1, str.begin() + next_amp,
                back_inserter(result) );
        }
        amp = next_amp;
    }

    if (change_made) {
      str = result;
    }

    return change_made;
}


CRef<CSeq_loc> CCleanup::GetProteinLocationFromNucleotideLocation(const CSeq_loc& nuc_loc, const CSeq_feat& cds, CScope& scope, bool require_inframe)
{
    if (require_inframe) {
        feature::ELocationInFrame is_in_frame = feature::IsLocationInFrame(scope.GetSeq_featHandle(cds), nuc_loc);
        bool is_ok = false;
        switch (is_in_frame) {
            case feature::eLocationInFrame_InFrame:
                is_ok = true;
                break;
            case feature::eLocationInFrame_BadStart:
                if (cds.GetLocation().GetStart(eExtreme_Biological) == nuc_loc.GetStart(eExtreme_Biological)) {
                    is_ok = true;
                }
                break;
            case feature::eLocationInFrame_BadStop:
                if (cds.GetLocation().GetStop(eExtreme_Biological) == nuc_loc.GetStop(eExtreme_Biological)) {
                    is_ok = true;
                }
                break;
            case feature::eLocationInFrame_BadStartAndStop:
                if (cds.GetLocation().GetStart(eExtreme_Biological) == nuc_loc.GetStart(eExtreme_Biological) &&
                    cds.GetLocation().GetStop(eExtreme_Biological) == nuc_loc.GetStop(eExtreme_Biological)) {
                    is_ok = true;
                }
                break;
            case feature::eLocationInFrame_NotIn:
                break;
        }
        if (!is_ok) {
            return CRef<CSeq_loc>(NULL);
        }
    }
    CRef<CSeq_loc> new_loc;
    CRef<CSeq_loc_Mapper> nuc2prot_mapper(
        new CSeq_loc_Mapper(cds, CSeq_loc_Mapper::eLocationToProduct, &scope));
    new_loc = nuc2prot_mapper->Map(nuc_loc);
    if (!new_loc) {
        return CRef<CSeq_loc>(NULL);
    }

    const CSeq_id* sid = new_loc->GetId();
    const CSeq_id* orig_id = nuc_loc.GetId();
    if (!sid || (orig_id && sid->Equals(*orig_id))) {
        // unable to map to protein location
        return CRef<CSeq_loc>(NULL);
    }

    new_loc->ResetStrand();

    // if location includes stop codon, remove it
    CBioseq_Handle prot = scope.GetBioseqHandle(*sid);
    if (prot && new_loc->GetStop(objects::eExtreme_Positional) >= prot.GetBioseqLength())
    {
        CRef<CSeq_id> sub_id(new CSeq_id());
        sub_id->Assign(*sid);
        CSeq_loc sub(*sub_id, prot.GetBioseqLength(), new_loc->GetStop(objects::eExtreme_Positional), new_loc->GetStrand());
        new_loc = sequence::Seq_loc_Subtract(*new_loc, sub, CSeq_loc::fMerge_All | CSeq_loc::fSort, &scope);
        if (nuc_loc.IsPartialStop(eExtreme_Biological)) {
            new_loc->SetPartialStop(true, eExtreme_Biological);
        }
    }

    if (!new_loc->IsInt() && !new_loc->IsPnt()) {
        CRef<CSeq_loc> tmp = sequence::Seq_loc_Merge(*new_loc, CSeq_loc::fMerge_All, &scope);
        new_loc = tmp;
    }

    if (!cds.GetLocation().IsPartialStart(eExtreme_Biological)) {
        if (new_loc->IsPartialStart(eExtreme_Biological)) {
            new_loc->SetPartialStart(false, eExtreme_Biological);
        }
    }
    if (!cds.GetLocation().IsPartialStop(eExtreme_Biological)) {
        if (new_loc->IsPartialStop(eExtreme_Biological)) {
            new_loc->SetPartialStop(false, eExtreme_Biological);
        }
    }



    return new_loc;
}


CRef<CSeq_loc> CCleanup::GetProteinLocationFromNucleotideLocation(const CSeq_loc& nuc_loc, CScope& scope)
{
    CConstRef<CSeq_feat> cds = sequence::GetOverlappingCDS(nuc_loc, scope);
    if (!cds || !cds->IsSetProduct()) {
        // there is no overlapping coding region feature, so there is no appropriate
        // protein sequence to move to
        return CRef<CSeq_loc>(NULL);
    }

    return GetProteinLocationFromNucleotideLocation(nuc_loc, *cds, scope);
}



bool CCleanup::RepackageProteins(const CSeq_feat& cds, CBioseq_set_Handle np)
{
    if (!cds.IsSetProduct() || !cds.GetProduct().IsWhole()) {
        // no product, or product is specified weirdly
        return false;
    }
    CBioseq_Handle protein = np.GetTSE_Handle().GetBioseqHandle(cds.GetProduct().GetWhole());
    if (!protein) {
        // protein is not in the same TSE
        return false;
    }
    if (protein.GetParentBioseq_set() == np) {
        // already in the right set
        return false;
    }
    CBioseq_set_EditHandle eh(np);
    CSeq_entry_Handle ph = protein.GetSeq_entry_Handle();
    CSeq_entry_EditHandle peh(ph);
    eh.TakeEntry(peh);
    return true;
}


bool CCleanup::RepackageProteins(CSeq_entry_Handle seh)
{
    bool changed = false;
    CSeq_entry_CI si(seh, CSeq_entry_CI::fRecursive | CSeq_entry_CI::fIncludeGivenEntry, CSeq_entry::e_Set);
    while (si) {
        CBioseq_set_Handle set = si->GetSet();
        if (set.IsSetClass() && set.GetClass() == CBioseq_set::eClass_nuc_prot && set.HasAnnots()) {
            ITERATE(CBioseq_set::TAnnot, annot_it, set.GetCompleteBioseq_set()->GetAnnot()) {
                if ((*annot_it)->IsSetData() && (*annot_it)->IsFtable()) {
                    ITERATE(CSeq_annot::TData::TFtable, feat_it, (*annot_it)->GetData().GetFtable()) {
                        if ((*feat_it)->IsSetData() && (*feat_it)->GetData().IsCdregion()) {
                            changed |= RepackageProteins(**feat_it, set);
                        }
                    }
                }
            }
        }
        ++si;
    }
    return changed;
}


bool CCleanup::ConvertDeltaSeqToRaw(CSeq_entry_Handle seh, CSeq_inst::EMol filter)
{
    bool any_change = false;
    for (CBioseq_CI bi(seh, filter); bi; ++bi) {
        CBioseq_Handle bsh = *bi;
        CRef<CSeq_inst> inst(new CSeq_inst());
        inst->Assign(bsh.GetInst());
        if (inst->ConvertDeltaToRaw()) {
            CBioseq_EditHandle beh(bsh);
            beh.SetInst(*inst);
            any_change = true;
        }
    }
    return any_change;
}


bool CCleanup::ParseCodeBreak(const CSeq_feat& feat, CCdregion& cds, const string& str, CScope& scope)
{
    if (str.empty() || !feat.IsSetLocation()) {
        return false;
    }

    const CSeq_id* feat_loc_seq_id = feat.GetLocation().GetId();
    if (!feat_loc_seq_id) {
        return false;
    }

    string::size_type aa_pos = NStr::Find(str, "aa:");
    string::size_type len = 0;
    string::size_type loc_pos, end_pos;
    char protein_letter = 'X';
    CRef<CSeq_loc> break_loc;

    if (aa_pos == string::npos) {
        aa_pos = NStr::Find(str, ",");
        if (aa_pos != string::npos) {
            aa_pos = NStr::Find(str, ":", aa_pos);
        }
        if (aa_pos != string::npos) {
            aa_pos++;
        }
    } else {
        aa_pos += 3;
    }

    if (aa_pos != string::npos) {
        while (aa_pos < str.length() && isspace(str[aa_pos])) {
            aa_pos++;
        }
        while (aa_pos + len < str.length() && isalpha(str[aa_pos + len])) {
            len++;
        }
        if (len != 0) {
            protein_letter = ValidAminoAcid(str.substr(aa_pos, len));
        }
    }

    loc_pos = NStr::Find(str, "(pos:");
    if (loc_pos == string::npos) {
        return false;
    }
    loc_pos += 5;
    while (loc_pos < str.length() && isspace(str[loc_pos])) {
        loc_pos++;
    }

    end_pos = NStr::Find(str, ",aa:", loc_pos);
    if (end_pos == NPOS) {
        end_pos = NStr::Find(str, ",", loc_pos);
        if (end_pos == NPOS) {
            end_pos = str.length();
        }
    }

    string pos = NStr::TruncateSpaces(str.substr(loc_pos, end_pos - loc_pos));

    // handle multi-interval positions by adding a join() around them
    if (pos.find_first_of(",") != string::npos) {
        pos = "join(" + pos + ")";
    }

    break_loc = ReadLocFromText(pos, feat_loc_seq_id, &scope);
    if (FIELD_IS_SET(feat.GetLocation(), Strand) && GET_FIELD(feat.GetLocation(), Strand) == eNa_strand_minus) {
        break_loc->SetStrand(GET_FIELD(feat.GetLocation(), Strand));
    } else {
        RESET_FIELD(*break_loc, Strand);
    }

    if (break_loc == NULL
        || (break_loc->IsInt()
        && sequence::Compare(*break_loc, feat.GetLocation(), &scope, sequence::fCompareOverlapping) != sequence::eContained)
        || (break_loc->IsInt() && sequence::GetLength(*break_loc, &scope) > 3)) {
        return false;
    }

    // need to build code break object and add it to coding region
    CRef<CCode_break> newCodeBreak(new CCode_break());
    CCode_break::TAa& aa = newCodeBreak->SetAa();
    aa.SetNcbieaa(protein_letter);
    newCodeBreak->SetLoc(*break_loc);

    CCdregion::TCode_break& orig_list = cds.SetCode_break();
    orig_list.push_back(newCodeBreak);

    return true;
}


bool CCleanup::ParseCodeBreaks(CSeq_feat& feat, CScope& scope)
{
    if (!feat.IsSetData() || !feat.GetData().IsCdregion() || 
        !feat.IsSetQual() || !feat.IsSetLocation()) {
        return false;
    }

    bool any_removed = false;
    CSeq_feat::TQual::iterator it = feat.SetQual().begin();
    while (it != feat.SetQual().end()) {
        if ((*it)->IsSetQual() &&
            NStr::EqualNocase((*it)->GetQual(), "transl_except") &&
            (*it)->IsSetVal() &&
            ParseCodeBreak(feat, feat.SetData().SetCdregion(), (*it)->GetVal(), scope)) {
            it = feat.SetQual().erase(it);
            any_removed = true;
        } else {
            ++it;
        }
    }
    if (feat.GetQual().size() == 0) {
        feat.ResetQual();
    }
    return any_removed;
}



END_SCOPE(objects)
END_NCBI_SCOPE
