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
* Author:  Colleen Bollin
*
* File Description:
*   Generate unique definition lines for a set of sequences using organism
*   descriptions and feature clauses.
*/

#include <ncbi_pch.hpp>
#include <objmgr/util/autodef.hpp>
#include <corelib/ncbimisc.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/seq_loc_util.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/RNA_gen.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/general/Dbtag.hpp>

#include <serial/iterator.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

            
CAutoDef::CAutoDef()
    : m_Cancelled(false)
{
}


CAutoDef::~CAutoDef()
{
}


bool s_NeedFeatureClause(const CBioseq& b) 
{
    if (!b.IsSetAnnot()) {
        return true;
    }
    size_t num_features = 0;

    ITERATE(CBioseq::TAnnot, a, b.GetAnnot()) {
        if ((*a)->IsFtable()) {
            num_features += (*a)->GetData().GetFtable().size();
            if (num_features > 100) {
                break;
            }
        }
    }
    if (num_features < 100) {
        return true;
    } else {
        return false;
    }
}

void CAutoDef::AddSources (CSeq_entry_Handle se)
{
    
    // add sources to modifier combination groups
    CBioseq_CI seq_iter(se, CSeq_inst::eMol_na);
    for ( ; seq_iter; ++seq_iter ) {
        CSeqdesc_CI dit((*seq_iter), CSeqdesc::e_Source);
        if (dit) {
            string feature_clauses = s_NeedFeatureClause(*(seq_iter->GetCompleteBioseq())) ? x_GetFeatureClauses(*seq_iter) : kEmptyStr;
            const CBioSource& bsrc = dit->GetSource();
            m_OrigModCombo.AddSource(bsrc, feature_clauses);
        }
    }

    // set default exclude_sp values
    m_OrigModCombo.SetExcludeSpOrgs (m_OrigModCombo.GetDefaultExcludeSp());
}


void CAutoDef::x_SortModifierListByRank(TModifierIndexVector &index_list, CAutoDefSourceDescription::TAvailableModifierVector &modifier_list) 
{
    unsigned int k, j, tmp;
    if (index_list.size() < 2) {
        return;
    }
    for (k = 0; k < index_list.size() - 1; k++) {
        for (j = k + 1; j < index_list.size(); j++) { 
            if (modifier_list[index_list[k]].GetRank() > modifier_list[index_list[j]].GetRank()) {
                 tmp = index_list[k];
                 index_list[k] = index_list[j];
                 index_list[j] = tmp;
             }
         }
     }
}


void CAutoDef::x_GetModifierIndexList(TModifierIndexVector &index_list, CAutoDefSourceDescription::TAvailableModifierVector &modifier_list)
{
    unsigned int k;
    TModifierIndexVector remaining_list;

    index_list.clear();
    remaining_list.clear();
    
    // note - required modifiers should be removed from the list
    
    // first, look for all_present and all_unique modifiers
    for (k = 0; k < modifier_list.size(); k++) {
        if (modifier_list[k].AllPresent() && modifier_list[k].AllUnique()) {
            index_list.push_back(k);
        } else if (modifier_list[k].AnyPresent()) {
            remaining_list.push_back(k);
        }
    }
    x_SortModifierListByRank(index_list, modifier_list);
    x_SortModifierListByRank(remaining_list, modifier_list);
    
    for (k = 0; k < remaining_list.size(); k++) {
        index_list.push_back(remaining_list[k]);
    }
}


bool CAutoDef::x_IsOrgModRequired(unsigned int mod_type)
{
    return false;
}


bool CAutoDef::x_IsSubSrcRequired(unsigned int mod_type)
{
    if (mod_type == CSubSource::eSubtype_endogenous_virus_name
        || mod_type == CSubSource::eSubtype_plasmid_name
        || mod_type == CSubSource::eSubtype_transgenic) {
        return true;
    } else {
        return false;
    }
}


unsigned int CAutoDef::GetNumAvailableModifiers()
{
    CAutoDefSourceDescription::TAvailableModifierVector modifier_list;
    modifier_list.clear();
    m_OrigModCombo.GetAvailableModifiers (modifier_list);

    unsigned int num_present = 0;
    for (unsigned int k = 0; k < modifier_list.size(); k++) {
        if (modifier_list[k].AnyPresent()) {
            num_present++;
        }
    }
    return num_present;    
}


struct SAutoDefModifierComboSort {
    bool operator()(const CAutoDefModifierCombo& s1,
                    const CAutoDefModifierCombo& s2) const
    {
        return (s1 < s2);
    }
};



CAutoDefModifierCombo * CAutoDef::FindBestModifierCombo()
{
    CAutoDefModifierCombo *best = NULL;
    TModifierComboVector  combo_list;

    combo_list.clear();
    combo_list.push_back (new CAutoDefModifierCombo(&m_OrigModCombo));


    TModifierComboVector tmp, add_list;
    TModifierComboVector::iterator it;
    CAutoDefSourceDescription::TModifierVector mod_list, mods_to_try;
    bool stop = false;
    unsigned int  k;

    mod_list.clear();

    if (combo_list[0]->GetMaxInGroup() == 1) {
        stop = true;
    }

    while (!stop) {
        stop = true;
        it = combo_list.begin();
        add_list.clear();
        while (it != combo_list.end()) {
            tmp = (*it)->ExpandByAnyPresent ();
            if (!tmp.empty()) {
                stop = false;
                for (k = 0; k < tmp.size(); k++) {
                    add_list.push_back (new CAutoDefModifierCombo(tmp[k]));
                }
                it = combo_list.erase (it);
            } else {
                it++;
            }
            tmp.clear();
        }
        for (k = 0; k < add_list.size(); k++) {
            combo_list.push_back (new CAutoDefModifierCombo(add_list[k]));
        }
        add_list.clear();
        std::sort (combo_list.begin(), combo_list.end(), SAutoDefModifierComboSort());
        if (combo_list[0]->GetMaxInGroup() == 1) {
            stop = true;
        }
    }

    ITERATE (CAutoDefSourceDescription::TModifierVector, it, combo_list[0]->GetModifiers()) {
        mod_list.push_back (CAutoDefSourceModifierInfo(*it));
    }

    best = combo_list[0];
    combo_list[0] = NULL;
    for (k = 1; k < combo_list.size(); k++) {
       delete combo_list[k];
       combo_list[k] = NULL;
    }
    return best;
}


CAutoDefModifierCombo* CAutoDef::GetAllModifierCombo()
{
    CAutoDefModifierCombo *newm = new CAutoDefModifierCombo(&m_OrigModCombo);
        
    // set all modifiers in combo
    CAutoDefSourceDescription::TAvailableModifierVector modifier_list;
    
    // first, get the list of modifiers that are available
    modifier_list.clear();
    newm->GetAvailableModifiers (modifier_list);
    
    // add any modifier not already in the combo to the combo
    for (unsigned int k = 0; k < modifier_list.size(); k++) {
        if (modifier_list[k].AnyPresent()) {
            if (modifier_list[k].IsOrgMod()) {
                COrgMod::ESubtype subtype = modifier_list[k].GetOrgModType();
                if (!newm->HasOrgMod(subtype)) {
                    newm->AddOrgMod(subtype);
                } 
            } else {
                CSubSource::ESubtype subtype = modifier_list[k].GetSubSourceType();
                if (!newm->HasSubSource(subtype)) {
                    newm->AddSubsource(subtype);
                }
            }
        }
    }
    return newm;
}


CAutoDefModifierCombo* CAutoDef::GetEmptyCombo()
{
    CAutoDefModifierCombo *newm = new CAutoDefModifierCombo(&m_OrigModCombo);
        
    return newm;
}


string CAutoDef::GetOneSourceDescription(CBioseq_Handle bh)
{
    CAutoDefModifierCombo *best = FindBestModifierCombo();
    if (best == NULL) {
        return "";
    }
    
    for (CSeqdesc_CI dit(bh, CSeqdesc::e_Source); dit;  ++dit) {
        const CBioSource& bsrc = dit->GetSource();
        return best->GetSourceDescriptionString(bsrc);
    }
    return "";
}
    

// Some misc_RNA clauses have a comment that actually lists multiple
// features.  These functions create a clause for each element in the
// comment.

bool CAutoDef::x_AddMiscRNAFeatures(CBioseq_Handle bh, const CSeq_feat& cf, const CSeq_loc& mapped_loc, CAutoDefFeatureClause_Base &main_clause)
{
    string comment = "";
    string::size_type pos;
    
    if (cf.GetData().Which() == CSeqFeatData::e_Rna) {
        comment = cf.GetNamedQual("product");
        if (NStr::IsBlank(comment)
            && cf.IsSetData()
            && cf.GetData().IsRna()
            && cf.GetData().GetRna().IsSetExt()) {
            if (cf.GetData().GetRna().GetExt().IsName()) {
                comment = cf.GetData().GetRna().GetExt().GetName();
            } else if (cf.GetData().GetRna().GetExt().IsGen()
                       && cf.GetData().GetRna().GetExt().GetGen().IsSetProduct()) {
                comment = cf.GetData().GetRna().GetExt().GetGen().GetProduct();
            }
        }
    }

    if ((NStr::Equal (comment, "misc_RNA") || NStr::IsBlank (comment)) && cf.CanGetComment()) {
        comment = cf.GetComment();
    }
    if (NStr::IsBlank(comment)) {
        return false;
    }
    
    pos = NStr::Find(comment, "spacer");
    if (pos == NCBI_NS_STD::string::npos) {
        return false;
    }

    bool is_region = false;

    if (NStr::StartsWith (comment, "contains ")) {
        comment = comment.substr(9);
    } else if (NStr::StartsWith (comment, "may contain ")) {
        comment = comment.substr(12);
        is_region = true;
    }

    pos = NStr::Find(comment, ";");
    if (pos != string::npos) {
        comment = comment.substr(0, pos);
    }

    if (is_region) {
        main_clause.AddSubclause(new CAutoDefParsedRegionClause(bh, cf, mapped_loc, comment));
    } else {
        vector<string> elements = CAutoDefFeatureClause_Base::GetMiscRNAElements(comment);
        if (!elements.empty()) {
            ITERATE(vector<string>, s, elements) {
                CAutoDefParsedClause *new_clause = new CAutoDefParsedClause(bh, cf, mapped_loc,
                    (*s == elements.front()), (*s == elements.back()));
                new_clause->SetMiscRNAWord(*s);
                main_clause.AddSubclause(new_clause);
            }
        } else {
            elements = CAutoDefFeatureClause_Base::GetTrnaIntergenicSpacerClausePhrases(comment);
            if (!elements.empty()) {
                ITERATE(vector<string>, s, elements) {
                    size_t pos = NStr::Find(*s, "intergenic spacer");
                    if (pos != string::npos) {
                        CAutoDefParsedIntergenicSpacerClause *spacer =
                            new CAutoDefParsedIntergenicSpacerClause(bh,
                            cf,
                            mapped_loc,
                            (*s),
                            (*s == elements.front()),
                            (*s == elements.back()));
                        main_clause.AddSubclause(spacer);
                    } else {
                        CAutoDefFeatureClause *gene = s_tRNAClauseFromNote(bh, cf, mapped_loc, *s, (*s == elements.front()), (*s == elements.back()));
                        main_clause.AddSubclause(gene);
                    }
                }
            } else {
                CAutoDefParsedIntergenicSpacerClause *spacer =
                    new CAutoDefParsedIntergenicSpacerClause(bh,
                    cf,
                    mapped_loc,
                    comment,
                    true,
                    true);
                main_clause.AddSubclause(spacer);
            }
        }
    }
    return true;
}





bool CAutoDef::x_AddtRNAAndOther(CBioseq_Handle bh, const CSeq_feat& cf, const CSeq_loc& mapped_loc, CAutoDefFeatureClause_Base &main_clause)
{
    if (cf.GetData().GetSubtype() != CSeqFeatData::eSubtype_misc_feature ||
        !cf.IsSetComment()) {
        return false;
    }

    vector<string> phrases = CAutoDefFeatureClause_Base::GetFeatureClausePhrases(cf.GetComment());
    if (phrases.size() < 2) {
        return false;
    }

    bool first = true;
    string last = phrases.back();
    phrases.pop_back();
    ITERATE(vector<string>, it, phrases) {
        main_clause.AddSubclause(CAutoDefFeatureClause_Base::ClauseFromPhrase(*it, bh, cf, mapped_loc, first, false));
        first = false;
    }
    main_clause.AddSubclause(CAutoDefFeatureClause_Base::ClauseFromPhrase(last, bh, cf, mapped_loc, first, true));

    return true;
}


void CAutoDef::x_RemoveOptionalFeatures(CAutoDefFeatureClause_Base *main_clause, CBioseq_Handle bh)
{
    // remove optional features that have not been requested
    if (main_clause == NULL) {
        return;
    }
    
    // keep 5' UTRs only if lonely or requested
    if (!m_Options.GetKeep5UTRs() && !main_clause->IsFeatureTypeLonely(CSeqFeatData::eSubtype_5UTR)) {
        main_clause->RemoveFeaturesByType(CSeqFeatData::eSubtype_5UTR);
    }
    
    // keep 3' UTRs only if lonely or requested
    if (!m_Options.GetKeep3UTRs() && !main_clause->IsFeatureTypeLonely(CSeqFeatData::eSubtype_3UTR)) {
        main_clause->RemoveFeaturesByType(CSeqFeatData::eSubtype_3UTR);
    }
    
    // keep LTRs only if requested or lonely and not in parent
    if (!m_Options.GetKeepLTRs() && !main_clause->IsFeatureTypeLonely(CSeqFeatData::eSubtype_LTR)) {
        main_clause->RemoveFeaturesByType(CSeqFeatData::eSubtype_LTR);
    }
           
    // keep promoters only if requested or lonely and not in mRNA
    if (!m_Options.GetKeepRegulatoryFeatures()) {
        if (!main_clause->IsFeatureTypeLonely(CSeqFeatData::eSubtype_regulatory)) {
            main_clause->RemoveFeaturesByType(CSeqFeatData::eSubtype_regulatory, m_Options.GetUseFakePromoters());
        } else {
            main_clause->RemoveFeaturesInmRNAsByType(CSeqFeatData::eSubtype_regulatory, m_Options.GetUseFakePromoters());
        }
    }
    
    // keep introns only if requested or lonely and not in mRNA
    if (!m_Options.GetKeepIntrons()) {
        if (!main_clause->IsFeatureTypeLonely(CSeqFeatData::eSubtype_intron)) {
            main_clause->RemoveFeaturesByType(CSeqFeatData::eSubtype_intron);
        } else {
            main_clause->RemoveFeaturesInmRNAsByType(CSeqFeatData::eSubtype_intron);
        }
    }
    
    // keep exons only if requested or lonely or in mRNA or in partial CDS or on segment
    if (!m_Options.GetKeepExons() && !IsSegment(bh)) {
        if (main_clause->GetMainFeatureSubtype() != CSeqFeatData::eSubtype_exon) {
            main_clause->RemoveUnwantedExons();
        }
    }
    
    // only keep bioseq precursor RNAs if lonely or requested
    if (!main_clause->IsBioseqPrecursorRNA() && !m_Options.GetKeepPrecursorRNA()) {
        main_clause->RemoveBioseqPrecursorRNAs();
    }

    // keep uORFs if lonely or requested
    if (!m_Options.GetKeepuORFs() && main_clause->GetNumSubclauses() > 1) {
        main_clause->RemoveuORFs();
    }

    // remove "optional" mobile element features unless lonely or requested
    if (!m_Options.GetKeepMobileElements() && main_clause->GetNumSubclauses() > 1) {
        main_clause->RemoveOptionalMobileElements();
    }
    
    // keep misc_recombs only if requested
    if (!m_Options.GetKeepMiscRecomb()) {
        main_clause->RemoveFeaturesByType(CSeqFeatData::eSubtype_misc_recomb);
    }

    // delete subclauses at end, so that loneliness calculations will be correct
    main_clause->RemoveDeletedSubclauses();
}


bool CAutoDef::x_IsFeatureSuppressed(CSeqFeatData::ESubtype subtype)
{
    return m_Options.IsFeatureSuppressed(subtype);
}


void CAutoDef::SuppressFeature(objects::CFeatListItem feat)
{
    if (feat.GetType() == CSeqFeatData::e_not_set) {
        m_Options.SuppressAllFeatures();        
    } else {
        m_Options.SuppressFeature((CSeqFeatData::ESubtype)(feat.GetSubtype()));
    }
}


void CAutoDef::SuppressFeature(objects::CSeqFeatData::ESubtype subtype)
{
    m_Options.SuppressFeature(subtype);
}


bool CAutoDef::IsSegment(CBioseq_Handle bh)
{
    CSeq_entry_Handle seh = bh.GetParentEntry();
    
    seh = seh.GetParentEntry();
    
    if (seh && seh.IsSet()) {
        CBioseq_set_Handle bsh = seh.GetSet();
        if (bsh.CanGetClass() && bsh.GetClass() == CBioseq_set::eClass_parts) {
            return true;
        }
    }
    return false;
}


void CAutoDef::GetMasterLocation(CBioseq_Handle &bh, CRange<TSeqPos>& range)
{
    CSeq_entry_Handle seh = bh.GetParentEntry();
    CBioseq_Handle    master = bh;
    unsigned int      start = 0, stop = bh.GetBioseqLength() - 1;
    unsigned int      offset = 0;
    
    seh = seh.GetParentEntry();
    
    if (seh && seh.IsSet()) {
        CBioseq_set_Handle bsh = seh.GetSet();
        if (bsh.CanGetClass() && bsh.GetClass() == CBioseq_set::eClass_parts) {
            seh = seh.GetParentEntry();
            if (seh.IsSet()) {
                bsh = seh.GetSet();
                if (bsh.CanGetClass() && bsh.GetClass() == CBioseq_set::eClass_segset) {
                    CBioseq_CI seq_iter(seh);
                    for ( ; seq_iter; ++seq_iter ) {
                        if (seq_iter->CanGetInst_Repr()) {
                            if (seq_iter->GetInst_Repr() == CSeq_inst::eRepr_seg) {
                                master = *seq_iter;
                            } else if (seq_iter->GetInst_Repr() == CSeq_inst::eRepr_raw) {
                                if (*seq_iter == bh) {
                                    start = offset;
                                    stop = offset + bh.GetBioseqLength() - 1;
                                } else {
                                    offset += seq_iter->GetBioseqLength();
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    bh = master;
    range.SetFrom(start);
    range.SetTo(stop);
}


bool CAutoDef::x_Is5SList(CFeat_CI feat_ci)
{
    bool is_list = true;
    bool is_single = true;
    bool found_single = false;

    if (!feat_ci) {
        return false;
    }
    ++feat_ci;
    if (feat_ci) {
        is_single = false;
    }
    feat_ci.Rewind();
    
    while (feat_ci && is_list) {
        if (feat_ci->GetData().GetSubtype() == CSeqFeatData::eSubtype_rRNA) {
            if (!feat_ci->GetData().GetRna().IsSetExt()
                || !feat_ci->GetData().GetRna().GetExt().IsName()
                || !NStr::Equal(feat_ci->GetData().GetRna().GetExt().GetName(), "5S ribosomal RNA")) {
                is_list = false;
            }
        } else if (feat_ci->GetData().GetSubtype() == CSeqFeatData::eSubtype_misc_feature) {
            if (!feat_ci->IsSetComment()) {
                is_list = false;
            } else if (NStr::Equal(feat_ci->GetComment(), "contains 5S ribosomal RNA and nontranscribed spacer")) {
                found_single = true;
            } else if (!NStr::Equal(feat_ci->GetComment(), "nontranscribed spacer")) {
                is_list = false;
            } 
        } else {
            is_list = false;
        }
        ++feat_ci;
    }
    if (is_single && !found_single) {
        is_list = false;
    }
    feat_ci.Rewind();
    return is_list;
} 


bool s_HasPromoter(CBioseq_Handle bh)
{
    bool has_promoter = false;
    SAnnotSelector sel(CSeqFeatData::eSubtype_regulatory);
    CFeat_CI f_ci (bh, sel);
    while (f_ci && !has_promoter) {
        has_promoter = CAutoDefFeatureClause::IsPromoter(*(f_ci->GetSeq_feat()));
        ++f_ci;
    }
    return has_promoter;
}


string CAutoDef::x_GetFeatureClauses(CBioseq_Handle bh)
{
    const string& custom = m_Options.GetCustomFeatureClause();
    if (!NStr::IsBlank(custom)) {
        return custom;
    }

    CSeqdesc_CI d(bh, CSeqdesc::e_User);
    while (d) {
        if (x_IsHumanSTR(d->GetUser())) {
            return x_GetHumanSTRFeatureClauses(bh, d->GetUser());
        }
        ++d;
    }


    CAutoDefFeatureClause_Base main_clause;
    CAutoDefFeatureClause *new_clause;
    CRange<TSeqPos> range;
    CBioseq_Handle master_bh = bh;
    
    GetMasterLocation(master_bh, range);

    // if no promoter, and fake promoters are requested, create one
    if (m_Options.GetUseFakePromoters() && !s_HasPromoter(bh)) {
        CRef<CSeq_feat> fake_promoter(new CSeq_feat());
        CRef<CSeq_loc> fake_promoter_loc(new CSeq_loc());
        const CSeq_id* id = FindBestChoice(bh.GetBioseqCore()->GetId(), CSeq_id::BestRank);
        CRef <CSeq_id> new_id(new CSeq_id);
        new_id->Assign(*id);
        fake_promoter_loc->SetInt().SetId(*new_id);
        fake_promoter_loc->SetInt().SetFrom(0);
        fake_promoter_loc->SetInt().SetTo(bh.GetInst_Length() - 1);

        fake_promoter->SetLocation(*fake_promoter_loc);

        main_clause.AddSubclause (new CAutoDefFakePromoterClause (master_bh, 
                                                                    *fake_promoter,
                                                                    *fake_promoter_loc));
    }

    // now create clauses for real features
    CFeat_CI feat_ci(master_bh);

    if (x_Is5SList(feat_ci)) {
        return "5S ribosomal RNA gene region";
    }

    while (feat_ci)
    { 
        const CSeq_feat& cf = feat_ci->GetOriginalFeature();
        const CSeq_loc& mapped_loc = feat_ci->GetMappedFeature().GetLocation();
        unsigned int subtype = cf.GetData().GetSubtype();
        unsigned int stop = mapped_loc.GetStop(eExtreme_Positional);
        new_clause = NULL;
        // unless it's a gene, don't use it unless it ends in the sequence we're looking at
        if ((subtype == CSeqFeatData::eSubtype_gene 
             || subtype == CSeqFeatData::eSubtype_mRNA
             || subtype == CSeqFeatData::eSubtype_cdregion
             || (stop >= range.GetFrom() && stop <= range.GetTo()))
            && !x_IsFeatureSuppressed(cf.GetData().GetSubtype())) {

			// some clauses can be created differently just knowing the subtype
            if (subtype == CSeqFeatData::eSubtype_gene) {
                new_clause = new CAutoDefGeneClause(bh, cf, mapped_loc, m_Options.GetSuppressLocusTags());
            } else if (subtype == CSeqFeatData::eSubtype_ncRNA) {
				new_clause = new CAutoDefNcRNAClause(bh, cf, mapped_loc, m_Options.GetUseNcRNAComment());
            } else if (subtype == CSeqFeatData::eSubtype_mobile_element) {
                new_clause = new CAutoDefMobileElementClause(bh, cf, mapped_loc);
            } else if (CAutoDefFeatureClause::IsSatellite(cf)) {
                new_clause = new CAutoDefSatelliteClause(bh, cf, mapped_loc);
            } else if (subtype == CSeqFeatData::eSubtype_otherRNA
						   || subtype == CSeqFeatData::eSubtype_misc_RNA
						   || subtype == CSeqFeatData::eSubtype_rRNA) {
                if (!x_AddMiscRNAFeatures(bh, cf, mapped_loc, main_clause)) {
                    new_clause = new CAutoDefFeatureClause(bh, cf, mapped_loc);
                }
            } else if (CAutoDefFeatureClause::IsPromoter(cf)) {
                new_clause = new CAutoDefPromoterClause(bh, cf, mapped_loc);
            } else if (CAutoDefFeatureClause::IsGeneCluster(cf)) {
                new_clause = new CAutoDefGeneClusterClause(bh, cf, mapped_loc);
            } else if (CAutoDefFeatureClause::IsControlRegion(cf)) {
                new_clause = new CAutoDefFeatureClause(bh, cf, mapped_loc);
            } else if ((subtype == CSeqFeatData::eSubtype_misc_feature || subtype == CSeqFeatData::eSubtype_otherRNA) &&
                       (x_AddMiscRNAFeatures(bh, cf, mapped_loc, main_clause) || x_AddtRNAAndOther(bh, cf, mapped_loc, main_clause))) {
                // special misc_features and misc_RNA features
            } else if (subtype == CSeqFeatData::eSubtype_misc_feature) {
				// some misc-features may require more parsing
                new_clause = new CAutoDefFeatureClause(bh, cf, mapped_loc);
                if (m_Options.GetMiscFeatRule() == CAutoDefOptions::eDelete
                    || (m_Options.GetMiscFeatRule() == CAutoDefOptions::eNoncodingProductFeat && !new_clause->IsNoncodingProductFeat())) {
                    delete new_clause;
                    new_clause = NULL;
                } else if (m_Options.GetMiscFeatRule() == CAutoDefOptions::eCommentFeat) {
                    delete new_clause;
                    new_clause = NULL;
                    if (cf.CanGetComment() && !NStr::IsBlank(cf.GetComment())) {
                        new_clause = new CAutoDefMiscCommentClause(bh, cf, mapped_loc);
                    }
                }
            } else {
  			    new_clause = new CAutoDefFeatureClause(bh, cf, mapped_loc);
			}
        
            if (new_clause != NULL && 
                (new_clause->IsRecognizedFeature() ||
                 (m_Options.GetKeepRepeatRegion() && 
                  (new_clause->GetMainFeatureSubtype() == CSeqFeatData::eSubtype_repeat_region || 
                   new_clause->GetMainFeatureSubtype() == CSeqFeatData::eSubtype_LTR)))) {
                if (new_clause->GetMainFeatureSubtype() == CSeqFeatData::eSubtype_exon ||
                    new_clause->GetMainFeatureSubtype() == CSeqFeatData::eSubtype_intron) {
                    new_clause->Label(m_Options.GetSuppressAlleles());
                }
                main_clause.AddSubclause(new_clause);
            } else if (new_clause != NULL) {            
                delete new_clause;
            }
        }
        ++feat_ci;
    }

    // optionally remove misc_feature subfeatures
    if (m_Options.GetSuppressMiscFeatureSubfeatures()) {
        main_clause.RemoveFeaturesUnderType(CSeqFeatData::eSubtype_misc_feature);
    }
    
    // Group alt-spliced exons first, so that they will be associated with the correct genes and mRNAs
    main_clause.GroupAltSplicedExons(bh);
    main_clause.RemoveDeletedSubclauses();
    
    // Add mRNAs to other clauses
    main_clause.GroupmRNAs(m_Options.GetSuppressAlleles());
    main_clause.RemoveDeletedSubclauses();
   
    // Add genes to clauses that need them for descriptions/products
    main_clause.GroupGenes(m_Options.GetSuppressAlleles());
        
    main_clause.GroupSegmentedCDSs(m_Options.GetSuppressAlleles());
    main_clause.RemoveDeletedSubclauses();
        
    // Group all features
    main_clause.GroupClauses(m_Options.GetGeneClusterOppStrand());
    main_clause.RemoveDeletedSubclauses();
    
    // now that features have been grouped, can expand lists of spliced exons
    main_clause.ExpandExonLists();
    
    // assign product names for features associated with genes that have products
    main_clause.AssignGeneProductNames(&main_clause, m_Options.GetSuppressAlleles());
    
    // reverse the order of clauses for minus-strand CDSfeatures
    main_clause.ReverseCDSClauseLists();
    
    main_clause.Label(m_Options.GetSuppressAlleles());
    main_clause.CountUnknownGenes();
    main_clause.RemoveDeletedSubclauses();
    
    x_RemoveOptionalFeatures(&main_clause, bh);
    
    // if a gene is listed as part of another clause, they do not need
    // to be listed as there own clause
    main_clause.RemoveGenesMentionedElsewhere();
    main_clause.RemoveDeletedSubclauses();
    
    if (m_Options.GetSuppressMobileElementSubfeatures()) {
        main_clause.SuppressMobileElementAndInsertionSequenceSubfeatures();
    }

    // if this is a segment, remove genes, mRNAs, and CDSs
    // unless they end on this segment or have subclauses that need their protein/gene
    // information
    main_clause.RemoveNonSegmentClauses(range);
    
    main_clause.Label(m_Options.GetSuppressAlleles());

    if (!m_Options.GetSuppressFeatureAltSplice()) {
        main_clause.FindAltSplices(m_Options.GetSuppressAlleles());
        main_clause.RemoveDeletedSubclauses();
    } 
    
    main_clause.ConsolidateRepeatedClauses(m_Options.GetSuppressAlleles());
    main_clause.RemoveDeletedSubclauses();
    
    main_clause.GroupConsecutiveExons(bh);
    main_clause.RemoveDeletedSubclauses();
    
    main_clause.Label(m_Options.GetSuppressAlleles());

    return main_clause.ListClauses(true, false, m_Options.GetSuppressAlleles());
}


string OrganelleByGenome(unsigned int genome_val)
{
    string organelle = "";
    switch (genome_val) {
        case CBioSource::eGenome_macronuclear:
            organelle = "macronuclear";
            break;
        case CBioSource::eGenome_nucleomorph:
            organelle = "nucleomorph";
            break;
        case CBioSource::eGenome_mitochondrion:
            organelle = "mitochondrion";
            break;
        case CBioSource::eGenome_apicoplast:
            organelle = "apicoplast";
            break;
        case CBioSource::eGenome_chloroplast:
            organelle = "chloroplast";
            break;
        case CBioSource::eGenome_chromoplast:
            organelle = "chromoplast";
            break;
        case CBioSource::eGenome_kinetoplast:
            organelle = "kinetoplast";
            break;
        case CBioSource::eGenome_plastid:
            organelle = "plastid";
            break;
        case CBioSource::eGenome_cyanelle:
            organelle = "cyanelle";
            break;
        case CBioSource::eGenome_leucoplast:
            organelle = "leucoplast";
            break;
        case CBioSource::eGenome_proplastid:
            organelle = "proplastid";
            break;
        case CBioSource::eGenome_hydrogenosome:
            organelle = "hydrogenosome";
            break;
    }
    return organelle;
}


static unsigned int s_GetProductFlagFromCDSProductNames (CBioseq_Handle bh)
{
	unsigned int product_flag = CBioSource::eGenome_unknown;
	string::size_type pos;

	SAnnotSelector sel(CSeqFeatData::eSubtype_cdregion);
    CFeat_CI feat_ci(bh, sel);
	while (feat_ci && product_flag == CBioSource::eGenome_unknown) {
        if (feat_ci->IsSetProduct()) {
            string label;
            CConstRef<CSeq_feat> prot
                = sequence::GetBestOverlappingFeat(feat_ci->GetProduct(),
                CSeqFeatData::e_Prot,
                sequence::eOverlap_Simple,
                bh.GetScope());
            if (prot) {
                feature::GetLabel(*prot, &label, feature::fFGL_Content);
                if (NStr::Find(label, "mitochondrion") != NCBI_NS_STD::string::npos
                    || NStr::Find(label, "mitochondrial") != NCBI_NS_STD::string::npos) {
                    product_flag = CBioSource::eGenome_mitochondrion;
                } else if (NStr::Find(label, "apicoplast") != NCBI_NS_STD::string::npos) {
                    product_flag = CBioSource::eGenome_apicoplast;
                } else if (NStr::Find(label, "chloroplast") != NCBI_NS_STD::string::npos) {
                    product_flag = CBioSource::eGenome_chloroplast;
                } else if (NStr::Find(label, "chromoplast") != NCBI_NS_STD::string::npos) {
                    product_flag = CBioSource::eGenome_chromoplast;
                } else if (NStr::Find(label, "kinetoplast") != NCBI_NS_STD::string::npos) {
                    product_flag = CBioSource::eGenome_kinetoplast;
                } else if (NStr::Find(label, "proplastid") != NCBI_NS_STD::string::npos) {
                    product_flag = CBioSource::eGenome_proplastid;
                } else if ((pos = NStr::Find(label, "plastid")) != NCBI_NS_STD::string::npos
                    && (pos == 0 || isspace(label.c_str()[pos]))) {
                    product_flag = CBioSource::eGenome_plastid;
                } else if (NStr::Find(label, "cyanelle") != NCBI_NS_STD::string::npos) {
                    product_flag = CBioSource::eGenome_cyanelle;
                } else if (NStr::Find(label, "leucoplast") != NCBI_NS_STD::string::npos) {
                    product_flag = CBioSource::eGenome_leucoplast;
                }
            }
        }
		++feat_ci;
	}
    return product_flag;
}


string CAutoDef::x_GetFeatureClauseProductEnding(const string& feature_clauses,
                                                 CBioseq_Handle bh)
{
    bool pluralize = false;
	unsigned int product_flag_to_use;
    unsigned int nuclear_copy_flag = CBioSource::eGenome_unknown;
    
	if (m_Options.GetSpecifyNuclearProduct()) {
	    product_flag_to_use = s_GetProductFlagFromCDSProductNames (bh);
	} else {
		product_flag_to_use = m_Options.GetProductFlag();
        nuclear_copy_flag = m_Options.GetNuclearCopyFlag();
	}
    if (NStr::Find(feature_clauses, "genes") != NCBI_NS_STD::string::npos) {
        pluralize = true;
    } else {
        string::size_type pos = NStr::Find(feature_clauses, "gene");
        if (pos != NCBI_NS_STD::string::npos
            && NStr::Find (feature_clauses, "gene", pos + 4) != NCBI_NS_STD::string::npos) {
            pluralize = true;
        }
    }

    unsigned int genome_val = CBioSource::eGenome_unknown;
    string genome_from_mods = "";
 
    for (CSeqdesc_CI dit(bh, CSeqdesc::e_Source); dit;  ++dit) {
        const CBioSource& bsrc = dit->GetSource();
        if (bsrc.CanGetGenome()) {
            genome_val = bsrc.GetGenome();
        }
        if (bsrc.CanGetSubtype()) {
            ITERATE (CBioSource::TSubtype, subSrcI, bsrc.GetSubtype()) {
                if ((*subSrcI)->GetSubtype() == CSubSource::eSubtype_other) {
                    string note = (*subSrcI)->GetName();
                    if (NStr::Equal(note, "macronuclear") || NStr::Equal(note, "micronuclear")) {
                        genome_from_mods = note;
                    }
                }
            }
        }
        break;
    }
    
    string ending = OrganelleByGenome(genome_val);
    if (NStr::Equal(ending, "mitochondrion")) {
        ending = "mitochondrial";
    }
    if (!NStr::IsBlank(ending)) {
        ending = "; " + ending;
    } else {
        if (product_flag_to_use != CBioSource::eGenome_unknown) {
            ending = OrganelleByGenome(product_flag_to_use);
            if (NStr::IsBlank(ending)) {
                if (!NStr::IsBlank(genome_from_mods)) {
                    ending = "; " + genome_from_mods;
                }
            } else {
                if (NStr::Equal(ending, "mitochondrion")) {
                    ending = "mitochondrial";
                }
                if (pluralize) {
                    ending = "; nuclear genes for " + ending + " products";
                } else {
                    ending = "; nuclear gene for " + ending + " product";
                }
            }
        } else if (nuclear_copy_flag != CBioSource::eGenome_unknown) {
            ending = OrganelleByGenome(nuclear_copy_flag);
            if (!NStr::IsBlank(ending)) {
                if (NStr::Equal(ending, "mitochondrion")) {
                    ending = "mitochondrial";
                }
                ending = "; nuclear copy of " + ending + " gene";
            }
        }
    } 
    return ending;
}


string CAutoDef::x_GetNonFeatureListEnding()
{
    string end = "";
    switch (m_Options.GetFeatureListType())
    {
        case CAutoDefOptions::eCompleteSequence:
            end = ", complete sequence.";
            break;
        case CAutoDefOptions::eCompleteGenome:
            end = ", complete genome.";
            break;
        case CAutoDefOptions::ePartialSequence:
            end = ", partial sequence.";
            break;
        case CAutoDefOptions::ePartialGenome:
            end = ", partial genome.";
            break;
        case CAutoDefOptions::eSequence:
            end = " sequence.";
            break;
        default:
            break;
    }
    return end;
}


bool IsBioseqmRNA(CBioseq_Handle bsh)
{
    bool is_mRNA = false;
    for (CSeqdesc_CI desc(bsh, CSeqdesc::e_Molinfo); desc && !is_mRNA; ++desc) {
        if (desc->GetMolinfo().CanGetBiomol()
            && desc->GetMolinfo().GetBiomol() == CMolInfo::eBiomol_mRNA) {
            is_mRNA = true;
        }
    }
    return is_mRNA;
}


bool IsInGenProdSet(CBioseq_Handle bh)
{
    CBioseq_set_Handle parent = bh.GetParentBioseq_set();
    while (parent) {
        if (parent.IsSetClass() && parent.GetClass() == CBioseq_set::eClass_gen_prod_set) {
            return true;
        }
        parent = parent.GetParentBioseq_set();
    }
    return false;
}


string CAutoDef::GetOneFeatureClauseList(CBioseq_Handle bh, unsigned int genome_val)
{
    string feature_clauses = "";
    if (m_Options.GetFeatureListType() == CAutoDefOptions::eListAllFeatures ||
        (IsBioseqmRNA(bh) && IsInGenProdSet(bh))) {
        feature_clauses = x_GetFeatureClauses(bh);
        if (!NStr::IsBlank(feature_clauses)) {
            feature_clauses = " " + feature_clauses;
        }
        string ending = x_GetFeatureClauseProductEnding(feature_clauses, bh);
        if (m_Options.GetAltSpliceFlag()) {
            if (NStr::IsBlank(ending)) {
                ending = "; alternatively spliced";
            } else {
                ending += ", alternatively spliced";
            }
        }
        feature_clauses += ending;
        if (NStr::IsBlank(feature_clauses)) {
            feature_clauses = ".";
        } else {
            feature_clauses += ".";
        }
    } else {
        string organelle = "";
        
        if (m_Options.GetFeatureListType() != CAutoDefOptions::eSequence
            || genome_val == CBioSource::eGenome_apicoplast
            || genome_val == CBioSource::eGenome_chloroplast
            || genome_val == CBioSource::eGenome_kinetoplast
            || genome_val == CBioSource::eGenome_leucoplast
            || genome_val == CBioSource::eGenome_mitochondrion
            || genome_val == CBioSource::eGenome_plastid) {
            organelle = OrganelleByGenome(genome_val);
        }
        if (!NStr::IsBlank(organelle)) {
            feature_clauses = " " + organelle;
        } else if (NStr::IsBlank(organelle) && m_Options.GetFeatureListType() == CAutoDefOptions::eSequence) {
            string biomol = "";
            CSeqdesc_CI mi(bh, CSeqdesc::e_Molinfo);
            if (mi && mi->GetMolinfo().IsSetBiomol()) {
                if (mi->GetMolinfo().GetBiomol() == CMolInfo::eBiomol_mRNA) {
                    biomol = "mRNA";
                } else {
                    biomol = CMolInfo::GetBiomolName(mi->GetMolinfo().GetBiomol());
                }
            }
            if (!NStr::IsBlank(biomol)) {
                feature_clauses = " " + biomol;
            }
        }

        feature_clauses += x_GetNonFeatureListEnding();
    }
    return feature_clauses;
}


string CAutoDef::GetKeywordPrefix(CBioseq_Handle bh)
{
    string keyword = kEmptyStr;

    CSeqdesc_CI gb(bh, CSeqdesc::e_Genbank);
    if (gb) {
        if (gb->GetGenbank().IsSetKeywords()) {
            ITERATE(CGB_block::TKeywords, it, gb->GetGenbank().GetKeywords()) {
                if (NStr::EqualNocase(*it, "TPA:inferential")) {
                    keyword = "TPA_inf: ";
                    break;
                } else if (NStr::EqualNocase(*it, "TPA:experimental")) {
                    keyword = "TPA_exp: ";
                    break;
                }
            }
        }
    } else {
        CSeqdesc_CI mi(bh, CSeqdesc::e_Molinfo);
        if (mi && mi->GetMolinfo().IsSetTech() && mi->GetMolinfo().GetTech() == CMolInfo::eTech_tsa) {
            keyword = "TSA: ";
        }
    }
    return keyword;
}


string CAutoDef::GetOneDefLine(CAutoDefModifierCombo *mod_combo, CBioseq_Handle bh)
{
    // for protein sequences, use sequence::GetTitle
    if (bh.CanGetInst() && bh.GetInst().CanGetMol() && bh.GetInst().GetMol() == CSeq_inst::eMol_aa) {
        return sequence::CDeflineGenerator()
            .GenerateDefline(bh,
                             sequence::CDeflineGenerator::fIgnoreExisting |
                             sequence::CDeflineGenerator::fAllProteinNames);
    }
    string org_desc = "Unknown organism";
    unsigned int genome_val = CBioSource::eGenome_unknown;
    mod_combo->InitOptions(m_Options);
    
    for (CSeqdesc_CI dit(bh, CSeqdesc::e_Source); dit;  ++dit) {
        const CBioSource& bsrc = dit->GetSource();
        org_desc = mod_combo->GetSourceDescriptionString(bsrc);
        if (bsrc.CanGetGenome()) {
            genome_val = bsrc.GetGenome();
        }
        break;
    }
    string feature_clauses = GetOneFeatureClauseList(bh, genome_val);
    
    if (org_desc.length() > 0 && isalpha(org_desc.c_str()[0])) {
        string first_letter = org_desc.substr(0, 1);
        string remainder = org_desc.substr(1);
        NStr::ToUpper(first_letter);
        org_desc = first_letter + remainder;
    }

    string keyword = GetKeywordPrefix(bh);

    return keyword + org_desc + feature_clauses;
}


// use internal settings to create mod combo
string CAutoDef::GetOneDefLine(CBioseq_Handle bh)
{
    // for protein sequences, use sequence::GetTitle
    if (bh.CanGetInst() && bh.GetInst().CanGetMol() && bh.GetInst().GetMol() == CSeq_inst::eMol_aa) {
        return sequence::CDeflineGenerator()
            .GenerateDefline(bh,
            sequence::CDeflineGenerator::fIgnoreExisting |
            sequence::CDeflineGenerator::fAllProteinNames);
    }
    string org_desc = "Unknown organism";
    unsigned int genome_val = CBioSource::eGenome_unknown;

    CAutoDefModifierCombo *mod_combo = GetEmptyCombo();
    mod_combo->InitFromOptions(m_Options);

    for (CSeqdesc_CI dit(bh, CSeqdesc::e_Source); dit; ++dit) {
        const CBioSource& bsrc = dit->GetSource();
        org_desc = mod_combo->GetSourceDescriptionString(bsrc);
        if (bsrc.CanGetGenome()) {
            genome_val = bsrc.GetGenome();
        }
        break;
    }
    string feature_clauses = GetOneFeatureClauseList(bh, genome_val);

    if (org_desc.length() > 0 && isalpha(org_desc.c_str()[0])) {
        string first_letter = org_desc.substr(0, 1);
        string remainder = org_desc.substr(1);
        NStr::ToUpper(first_letter);
        org_desc = first_letter + remainder;
    }

    string keyword = GetKeywordPrefix(bh);

    return keyword + org_desc + feature_clauses;
}


void CAutoDef::GetAvailableModifiers(CAutoDef::TAvailableModifierSet &mod_set)
{    
    mod_set.clear();
    CAutoDefSourceDescription::TAvailableModifierVector modifier_list;
    modifier_list.clear();
    m_OrigModCombo.GetAvailableModifiers (modifier_list);
    for (unsigned int k = 0; k < modifier_list.size(); k++) {
        mod_set.insert(CAutoDefAvailableModifier(modifier_list[k]));
    }
}


void CAutoDef::SetOptionsObject(const CUser_object& user)
{
    m_Options.InitFromUserObject(user);
}


//starting here, remove when separating autodef from taxonomy options
CConstRef<CUser_object> s_GetOptionsForSet(CBioseq_set_Handle set)
{
    CConstRef<CUser_object> options(NULL);
    CBioseq_CI b(set, CSeq_inst::eMol_na);
    while (b && !options) {
        CSeqdesc_CI desc(*b, CSeqdesc::e_User);
        while (desc && desc->GetUser().GetObjectType() != CUser_object::eObjectType_AutodefOptions) {
            ++desc;
        }
        if (desc) {
            options.Reset(&(desc->GetUser()));
        }
    }
    return options;
}


string CAutoDef::RegenerateDefLine(CBioseq_Handle bh)
{
    string defline = kEmptyStr;
    if (bh.IsAa()) {
        return kEmptyStr;
    }
    CSeqdesc_CI desc(bh, CSeqdesc::e_User);
    while (desc && desc->GetUser().GetObjectType() != CUser_object::eObjectType_AutodefOptions) {
        ++desc;
    }
    if (desc) {
        CAutoDef autodef;
        autodef.SetOptionsObject(desc->GetUser());
        CAutoDefModifierCombo mod_combo;
        CAutoDefOptions options;
        options.InitFromUserObject(desc->GetUser());
        mod_combo.SetOptions(options);
        defline = autodef.GetOneDefLine(&mod_combo, bh);
    }
    return defline;
}


bool CAutoDef::RegenerateSequenceDefLines(CSeq_entry_Handle se)
{
    bool any = false;
    CBioseq_CI b_iter(se);
    for (; b_iter; ++b_iter) {
        if (b_iter->IsAa()) {
            continue;
        }
        CSeqdesc_CI desc(*b_iter, CSeqdesc::e_User);
        while (desc && desc->GetUser().GetObjectType() != CUser_object::eObjectType_AutodefOptions) {
            ++desc;
        }
        if (desc) {
            string defline = RegenerateDefLine(*b_iter);

            bool found_existing = false;
            CBioseq_EditHandle beh(*b_iter);
            NON_CONST_ITERATE(CBioseq_EditHandle::TDescr::Tdata, it, beh.SetDescr().Set()) {
                if ((*it)->IsTitle()) {
                    if (!NStr::Equal((*it)->GetTitle(), defline)) {
                        (*it)->SetTitle(defline);
                        any = true;
                    }
                    found_existing = true;
                    break;
                }
            }
            if (!found_existing) {
                CRef<CSeqdesc> new_desc(new CSeqdesc());
                new_desc->SetTitle(defline);
                beh.SetDescr().Set().push_back(new_desc);
                any = true;
            }
        }
    }
    return any;
}


bool CAutoDef::x_IsHumanSTR(const CUser_object& obj)
{
    if (obj.GetObjectType() != CUser_object::eObjectType_StructuredComment) {
        return false;
    }
    if (!obj.IsSetData()) {
        return false;
    }
    ITERATE(CUser_object::TData, f, obj.GetData()) {
        if ((*f)->IsSetLabel() && (*f)->GetLabel().IsStr() &&
            NStr::EqualNocase((*f)->GetLabel().GetStr(), "StructuredCommentPrefix") &&
            (*f)->IsSetData() && (*f)->GetData().IsStr()) {
            if (NStr::EqualNocase((*f)->GetData().GetStr(), "##HumanSTR-START##")) {
                return true;
            } else {
                return false;
            }
        }
    }
    return false;
}


string CAutoDef::x_GetHumanSTRFeatureClauses(CBioseq_Handle bh, const CUser_object& comment)
{
    string locus_name = kEmptyStr;
    string allele = kEmptyStr;
    string repeat = kEmptyStr;

    if (comment.IsSetData()) {
        ITERATE(CUser_object::TData, it, comment.GetData()) {
            if ((*it)->IsSetData() && (*it)->GetData().IsStr() &&
                (*it)->IsSetLabel() && (*it)->GetLabel().IsStr()) {
                const string& label = (*it)->GetLabel().GetStr();
                if (NStr::EqualNocase(label, "STR locus name")) {
                    locus_name = (*it)->GetData().GetStr();
                } else if (NStr::EqualNocase(label, "Length-based allele")) {
                    allele = (*it)->GetData().GetStr();
                } else if (NStr::EqualNocase(label, "Bracketed repeat")) {
                    repeat = (*it)->GetData().GetStr();
                }
            }
        }
    }

    string clause = "microsatellite " + locus_name + " " + allele + " " + repeat;
    CFeat_CI f(bh, CSeqFeatData::eSubtype_variation);
    while (f) {
        if (f->IsSetDbxref()) {
            ITERATE(CSeq_feat::TDbxref, db, f->GetDbxref()) {
                if ((*db)->IsSetDb() && NStr::Equal((*db)->GetDb(), "dbSNP") &&
                    (*db)->IsSetTag()) {
                    if ((*db)->GetTag().IsStr()) {
                        clause += " " + (*db)->GetTag().GetStr();
                    } else if ((*db)->GetTag().IsId()) {
                        clause += " " + NStr::NumericToString((*db)->GetTag().GetId());
                    }
                }
            }             
        }
        ++f;
    }
    clause += " sequence";
    return clause;
}



END_SCOPE(objects)
END_NCBI_SCOPE
