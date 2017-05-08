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
#include <corelib/ncbimisc.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/feature.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Bioseq.hpp>

#include <serial/iterator.hpp>

#include <objmgr/util/autodef_mod_combo.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CAutoDefModifierCombo::CAutoDefModifierCombo() : m_UseModifierLabels(false),
                                                 m_MaxModifiers(-99),
                                                 m_AllowModAtEndOfTaxname(false),
                                                 m_KeepCountryText(false),
                                                 m_ExcludeSpOrgs(true),
                                                 m_ExcludeCfOrgs(false),
                                                 m_ExcludeNrOrgs(false),
                                                 m_ExcludeAffOrgs(false),
                                                 m_KeepParen(true),
                                                 m_KeepAfterSemicolon(false),
                                                 m_HIVCloneIsolateRule(CAutoDefOptions::eWantBoth)

{
    m_SubSources.clear();
    m_OrgMods.clear();

    m_GroupList.clear();
    m_Modifiers.clear();
}


CAutoDefModifierCombo::CAutoDefModifierCombo(CAutoDefModifierCombo *orig) 
{
    _ASSERT (orig);
    m_SubSources.clear();
    m_OrgMods.clear();
    
    m_GroupList.clear();
    m_Modifiers.clear();

    ITERATE (TGroupListVector, it, orig->GetGroupList()) {
        CAutoDefSourceGroup * g = new CAutoDefSourceGroup(*it);
        m_GroupList.push_back (g);
    }
    ITERATE (CAutoDefSourceDescription::TModifierVector, it, orig->GetModifiers()) {
        m_Modifiers.push_back (CAutoDefSourceModifierInfo(*it));
    }
    
    unsigned int k;
    for (k = 0; k < orig->GetNumSubSources(); k++) {
        m_SubSources.push_back(orig->GetSubSource(k));
    }
    
    for (k = 0; k < orig->GetNumOrgMods(); k++) {
        m_OrgMods.push_back(orig->GetOrgMod(k));
    }
    
    m_UseModifierLabels = orig->GetUseModifierLabels();
    m_KeepCountryText = orig->GetKeepCountryText();
    m_ExcludeSpOrgs = orig->GetExcludeSpOrgs();
    m_ExcludeCfOrgs = orig->GetExcludeCfOrgs();
    m_ExcludeNrOrgs = orig->GetExcludeNrOrgs();
    m_ExcludeAffOrgs = orig->GetExcludeAffOrgs();
    m_KeepParen = orig->GetKeepParen();
    m_KeepAfterSemicolon = orig->GetKeepAfterSemicolon();
    m_HIVCloneIsolateRule = orig->GetHIVCloneIsolateRule();
    m_AllowModAtEndOfTaxname = orig->GetAllowModAtEndOfTaxname();
    m_MaxModifiers = orig->GetMaxModifiers();
}


CAutoDefModifierCombo::~CAutoDefModifierCombo()
{
    for (unsigned int k = 0; k < m_GroupList.size(); k++) {
        delete m_GroupList[k];
    }
}


void CAutoDefModifierCombo::InitFromOptions(const CAutoDefOptions& options)
{
    m_UseModifierLabels = options.GetUseLabels();
    m_MaxModifiers = options.GetMaxMods();
    m_AllowModAtEndOfTaxname = options.GetAllowModAtEndOfTaxname();
    m_KeepCountryText = options.GetIncludeCountryText();
    m_ExcludeSpOrgs = options.GetDoNotApplyToSp();
    m_ExcludeCfOrgs = options.GetDoNotApplyToCf();
    m_ExcludeNrOrgs = options.GetDoNotApplyToNr();
    m_ExcludeAffOrgs = options.GetDoNotApplyToAff();
    m_KeepParen = options.GetLeaveParenthetical();
    m_KeepAfterSemicolon = options.GetKeepAfterSemicolon();
    m_HIVCloneIsolateRule = (CAutoDefOptions::EHIVCloneIsolateRule)(options.GetHIVRule());

    const CAutoDefOptions::TSubSources& subsrcs = options.GetSubSources();
    ITERATE(CAutoDefOptions::TSubSources, it, subsrcs) {
        AddQual(false, *it, true);
    }
    const CAutoDefOptions::TOrgMods& orgmods = options.GetOrgMods();
    ITERATE(CAutoDefOptions::TOrgMods, it, orgmods) {
        AddQual(true, *it, true);
    }
}


void CAutoDefModifierCombo::InitOptions(CAutoDefOptions& options) const
{
    options.SetUseLabels(m_UseModifierLabels);
    options.SetMaxMods(m_MaxModifiers);
    options.SetAllowModAtEndOfTaxname(m_AllowModAtEndOfTaxname);
    options.SetIncludeCountryText(m_KeepCountryText);
    options.SetDoNotApplyToSp(m_ExcludeSpOrgs);
    options.SetDoNotApplyToCf(m_ExcludeCfOrgs);
    options.SetDoNotApplyToNr(m_ExcludeNrOrgs);
    options.SetDoNotApplyToAff(m_ExcludeAffOrgs);
    options.SetLeaveParenthetical(m_KeepParen);
    options.SetKeepAfterSemicolon(m_KeepAfterSemicolon);
    options.SetHIVRule(m_HIVCloneIsolateRule);

    // add subsources and orgmods
    ITERATE(CAutoDefSourceDescription::TModifierVector, it, m_Modifiers) {
        if (it->IsOrgMod()) {
            options.AddOrgMod(it->GetSubtype());
        } else {
            options.AddSubSource(it->GetSubtype());
        }
    }
}


unsigned int CAutoDefModifierCombo::GetNumGroups()
{
    return m_GroupList.size();
}


unsigned int CAutoDefModifierCombo::GetNumSubSources()
{
    return m_SubSources.size();
}

   
CSubSource::ESubtype CAutoDefModifierCombo::GetSubSource(unsigned int index)
{
    _ASSERT (index < m_SubSources.size());
    
    return m_SubSources[index];
}


unsigned int CAutoDefModifierCombo::GetNumOrgMods()
{
    return m_OrgMods.size();
}

   
COrgMod::ESubtype CAutoDefModifierCombo::GetOrgMod(unsigned int index)
{
    _ASSERT (index < m_OrgMods.size());
    
    return m_OrgMods[index];
}


bool CAutoDefModifierCombo::HasSubSource(CSubSource::ESubtype st)
{
    for (unsigned int k = 0; k < m_SubSources.size(); k++) {
        if (m_SubSources[k] == st) {
            return true;
        }
    }
    return false;
}


bool CAutoDefModifierCombo::HasOrgMod(COrgMod::ESubtype st)
{
    for (unsigned int k = 0; k < m_OrgMods.size(); k++) {
        if (m_OrgMods[k] == st) {
            return true;
        }
    }
    return false;
}


void CAutoDefModifierCombo::AddSource(const CBioSource& bs, string feature_clauses) 
{
    CAutoDefSourceDescription src(bs, feature_clauses);
    bool found = false;

    NON_CONST_ITERATE (TGroupListVector, it, m_GroupList) {
        if ((*it)->GetSrcList().size() > 0
            && src.Compare (*((*it)->GetSrcList().begin())) == 0) {
            (*it)->AddSource (&src);
            found = true;
        }
    }
    if (!found) {
        CAutoDefSourceGroup * g = new CAutoDefSourceGroup();
        g->AddSource (&src);
        m_GroupList.push_back (g);
    }
}


void CAutoDefModifierCombo::AddSubsource(CSubSource::ESubtype st, bool even_if_not_uniquifying)
{
    AddQual(false, st, even_if_not_uniquifying);
}


void CAutoDefModifierCombo::AddOrgMod(COrgMod::ESubtype st, bool even_if_not_uniquifying)
{
    AddQual (true, st, even_if_not_uniquifying);
}


string CAutoDefModifierCombo::x_GetSubSourceLabel (CSubSource::ESubtype st)
{
    string label = "";
    
    if (st == CSubSource::eSubtype_endogenous_virus_name) {
        label = "endogenous virus";
    } else if (st == CSubSource::eSubtype_transgenic) {
        label = "transgenic";
    } else if (st == CSubSource::eSubtype_plasmid_name) {
        label = "plasmid";
    } else if (st == CSubSource::eSubtype_country) {
        label = "from";
    } else if (st == CSubSource::eSubtype_segment) {
        label = "segment";
    } else if (m_UseModifierLabels) {
        label = CAutoDefAvailableModifier::GetSubSourceLabel (st);
    }
    if (!NStr::IsBlank(label)) {
        label = " " + label;
    }
    return label;
}


string CAutoDefModifierCombo::x_GetOrgModLabel(COrgMod::ESubtype st)
{
    string label = "";
    if (st == COrgMod::eSubtype_nat_host) {
        label = "from";
    } else if (m_UseModifierLabels) {
        label = CAutoDefAvailableModifier::GetOrgModLabel(st);
    }
    if (!NStr::IsBlank(label)) {
        label = " " + label;
    }
    return label;
}


/* This function fixes HIV abbreviations, removes items in parentheses,
 * and trims spaces around the taxonomy name.
 */
void CAutoDefModifierCombo::x_CleanUpTaxName (string &tax_name)
{
    if (NStr::Equal(tax_name, "Human immunodeficiency virus type 1", NStr::eNocase)
        || NStr::Equal(tax_name, "Human immunodeficiency virus 1", NStr::eNocase)) {
        tax_name = "HIV-1";
    } else if (NStr::Equal(tax_name, "Human immunodeficiency virus type 2", NStr::eNocase)
               || NStr::Equal(tax_name, "Human immunodeficiency virus 2", NStr::eNocase)) {
        tax_name = "HIV-2";
    } else if (!m_KeepParen) {
        string::size_type pos = NStr::Find(tax_name, "(");
        if (pos != NCBI_NS_STD::string::npos) {
            tax_name = tax_name.substr(0, pos);
            NStr::TruncateSpacesInPlace(tax_name);
        }
    }
}

bool CAutoDefModifierCombo::x_BioSourceHasSubSrc(const CBioSource& src, CSubSource::ESubtype subtype)
{
    if (!src.IsSetSubtype()) {
        return false;
    }
    ITERATE(CBioSource::TSubtype, it, src.GetSubtype()) {
        if ((*it)->IsSetSubtype() && (*it)->GetSubtype() == subtype) {
            return true;
        }
    }
    return false;
}


bool CAutoDefModifierCombo::x_BioSourceHasOrgMod(const CBioSource& src, COrgMod::ESubtype subtype)
{
    if (!src.IsSetOrg() || !src.GetOrg().IsSetOrgname() || !src.GetOrg().GetOrgname().IsSetMod()) {
        return false;
    }
    ITERATE(COrgName::TMod, it, src.GetOrg().GetOrgname().GetMod()) {
        if ((*it)->IsSetSubtype() && (*it)->GetSubtype() == subtype) {
            return true;
        }
    }
    return false;

}


bool CAutoDefModifierCombo::x_AddSubsourceString (string &source_description, const CBioSource& bsrc, CSubSource::ESubtype st)
{
    bool         used = false;

    if (!bsrc.IsSetSubtype()) {
        return false;
    }
    ITERATE (CBioSource::TSubtype, subSrcI, bsrc.GetSubtype()) {
        if ((*subSrcI)->GetSubtype() == st) {
            source_description += x_GetSubSourceLabel (st);

            string val = (*subSrcI)->GetName();
            // truncate value at first semicolon
			      if (!m_KeepAfterSemicolon) {
				        string::size_type pos = NStr::Find(val, ";");
				        if (pos != NCBI_NS_STD::string::npos) {
					          val = val.substr(0, pos);
				        }
			      }
                    
            // if country and not keeping text after colon, truncate after colon
            if (st == CSubSource::eSubtype_country
                && ! m_KeepCountryText) {
                string::size_type pos = NStr::Find(val, ":");
                if (pos != NCBI_NS_STD::string::npos) {
                    val = val.substr(0, pos);
                }
            } else if (st == CSubSource::eSubtype_plasmid_name && NStr::EqualNocase(val, "unnamed")) {
                val = "";
            }
            if (!NStr::IsBlank(val)) {
                source_description += " " + val;
            }
            used = true;
        }
    }
    return used;
}

bool CAutoDefModifierCombo::IsModifierInString(const string& find_this, const string& find_in, bool ignore_at_end)
{
    size_t pos = NStr::Find(find_in, find_this);
    if (pos == string::npos) {
        return false;
    }

    bool keep_looking = false;
    // need to make sure it's a whole word and not a partial match
    if (pos != 0 && find_in.c_str()[pos - 1] != '(' && find_in.c_str()[pos - 1] != ' ') {
        // not whole word
        keep_looking = true;
    } else if (find_in.c_str()[pos + find_this.length()] != ')' &&
               find_in.c_str()[pos + find_this.length()] != ' ' &&
               find_in.c_str()[pos + find_this.length()] != 0) {
        // not whole word
        keep_looking = true;
    }

    bool at_end = (pos == find_in.length() - find_this.length());

    if (keep_looking) {
        if (at_end) {
            return false;
        } else {
            return IsModifierInString(find_this, find_in.substr(pos + 1), ignore_at_end);
        }
    } else if (at_end && ignore_at_end) {
        return false;
    } else {
        return true;
    }
}


bool CAutoDefModifierCombo::x_AddOrgModString (string &source_description, const CBioSource& bsrc, COrgMod::ESubtype st)
{
    bool         used = false;
    string       val;

    ITERATE (COrgName::TMod, modI, bsrc.GetOrg().GetOrgname().GetMod()) {
        if ((*modI)->GetSubtype() == st) {

            string val = (*modI)->GetSubname();
            // truncate value at first semicolon
			if (!m_KeepAfterSemicolon) {
				string::size_type pos = NStr::Find(val, ";");
				if (pos != NCBI_NS_STD::string::npos) {
					val = val.substr(0, pos);
				}
			}

            if (st == COrgMod::eSubtype_specimen_voucher && NStr::StartsWith (val, "personal:")) {
                val = val.substr(9);
            }
            // If modifier is one of the following types and the value already appears in the tax Name,
            // don't use in the organism description
            if ((st == COrgMod::eSubtype_strain
                 || st == COrgMod::eSubtype_variety
                 || st == COrgMod::eSubtype_sub_species
                 || st == COrgMod::eSubtype_forma
                 || st == COrgMod::eSubtype_forma_specialis
                 || st == COrgMod::eSubtype_pathovar
                 || st == COrgMod::eSubtype_specimen_voucher
                 || st == COrgMod::eSubtype_isolate)
                 && IsModifierInString(val, bsrc.GetOrg().GetTaxname(), m_AllowModAtEndOfTaxname)) {
                // can't use this
            } else {
                source_description += x_GetOrgModLabel(st);

                source_description += " ";
                source_description += val;
                used = true;
                break;
            }
        }
    }
    return used;
}


bool CAutoDefModifierCombo::HasTrickyHIV()
{
    bool has_tricky = false;

    for (unsigned int k = 0; k < m_GroupList.size() && !has_tricky; k++) {
        has_tricky = m_GroupList[k]->HasTrickyHIV();
    }
    return has_tricky;
}


void CAutoDefModifierCombo::x_AddHIVModifiers(TExtraOrgMods& extra_orgmods, TExtraSubSrcs& extra_subsrcs, const CBioSource& bsrc)
{
    string   clone_text = "";
    string   isolate_text = "";
    bool     src_has_clone = false;
    bool     src_has_isolate = false;
    bool     src_has_strain = false;
    
    if (!bsrc.IsSetOrg() || !bsrc.GetOrg().IsSetTaxname()) {
        return;
    }
    string   source_description = bsrc.GetOrg().GetTaxname();
    x_CleanUpTaxName(source_description);
    if (!NStr::Equal(source_description, "HIV-1") &&
        !NStr::Equal(source_description, "HIV-2")) {
        return;
    }

    if (extra_subsrcs.find(CSubSource::eSubtype_country) == extra_subsrcs.end()) {
        extra_subsrcs.insert(TExtraSubSrc(CSubSource::eSubtype_country, true));
    }

    src_has_clone = x_BioSourceHasSubSrc(bsrc, CSubSource::eSubtype_clone);
    src_has_isolate = x_BioSourceHasOrgMod(bsrc, COrgMod::eSubtype_isolate);
    src_has_strain = x_BioSourceHasOrgMod(bsrc, COrgMod::eSubtype_strain);
        
    if ((HasSubSource (CSubSource::eSubtype_clone) && src_has_clone) || 
        (HasOrgMod (COrgMod::eSubtype_isolate) && src_has_isolate) ||
        (HasOrgMod (COrgMod::eSubtype_strain) && src_has_strain)) {
        // no additional changes - isolate and clone rule taken care of
    } else {
        bool use_isolate = false;
        bool use_strain = false;
        if ( ! HasOrgMod (COrgMod::eSubtype_isolate) && src_has_isolate
            && (m_HIVCloneIsolateRule == CAutoDefOptions::ePreferIsolate
            || m_HIVCloneIsolateRule == CAutoDefOptions::eWantBoth
                || !src_has_clone)) {
            if (extra_orgmods.find(COrgMod::eSubtype_isolate) == extra_orgmods.end()) {
                extra_orgmods.insert(TExtraOrgMod(COrgMod::eSubtype_isolate, true));
                use_isolate = true;
            }
        }
        if (!HasOrgMod(COrgMod::eSubtype_strain) && src_has_strain &&
            !use_isolate) {
            if (extra_orgmods.find(COrgMod::eSubtype_strain) == extra_orgmods.end()) {
                extra_orgmods.insert(TExtraOrgMod(COrgMod::eSubtype_strain, true));
                use_strain = true;
            }
        }
        if (! HasSubSource(CSubSource::eSubtype_clone) && src_has_clone
            && (m_HIVCloneIsolateRule == CAutoDefOptions::ePreferClone
            || m_HIVCloneIsolateRule == CAutoDefOptions::eWantBoth
                || (!src_has_isolate && !src_has_strain))) {
            if (extra_subsrcs.find(CSubSource::eSubtype_clone) == extra_subsrcs.end()) {
                extra_subsrcs.insert(TExtraSubSrc(CSubSource::eSubtype_clone, true));
            }
        }
    }    
}


bool CAutoDefModifierCombo::GetDefaultExcludeSp ()
{
    bool default_exclude = true;

    for (unsigned int k = 0; k < m_GroupList.size() && default_exclude; k++) {
        default_exclude = m_GroupList[k]->GetDefaultExcludeSp ();
    }
    return default_exclude;
}


void CAutoDefModifierCombo::x_AddRequiredSubSourceModifiers(TExtraOrgMods& extra_orgmods, TExtraSubSrcs& extra_subsrcs, const CBioSource& bsrc)
{
    if (extra_subsrcs.find(CSubSource::eSubtype_transgenic) == extra_subsrcs.end()) {
        extra_subsrcs.insert(TExtraSubSrc(CSubSource::eSubtype_transgenic, true));
    }

    if (extra_subsrcs.find(CSubSource::eSubtype_plasmid_name) == extra_subsrcs.end()) {
        extra_subsrcs.insert(TExtraSubSrc(CSubSource::eSubtype_plasmid_name, true));
    }

    if (extra_subsrcs.find(CSubSource::eSubtype_endogenous_virus_name) == extra_subsrcs.end()) {
        extra_subsrcs.insert(TExtraSubSrc(CSubSource::eSubtype_endogenous_virus_name, true));
    }

    if (extra_subsrcs.find(CSubSource::eSubtype_segment) == extra_subsrcs.end() &&
        bsrc.IsSetOrg() && bsrc.GetOrg().IsSetTaxname() && NStr::StartsWith(bsrc.GetOrg().GetTaxname(), "Influenza ")) {
        extra_subsrcs.insert(TExtraSubSrc(CSubSource::eSubtype_segment, true));
    }
}


bool CAutoDefModifierCombo::x_HasTypeStrainComment(const CBioSource& bsrc)
{
    if (!bsrc.IsSetOrg() || !bsrc.GetOrg().IsSetOrgname() || !bsrc.GetOrg().GetOrgname().IsSetMod()) {
        return false;
    }

    ITERATE(COrgName::TMod, it, bsrc.GetOrg().GetOrgname().GetMod()) {
        if ((*it)->IsSetSubtype() && (*it)->GetSubtype() == COrgMod::eSubtype_other &&
            (*it)->IsSetSubname() && NStr::FindNoCase((*it)->GetSubname(), "type strain of") != string::npos) {
            return true;
        }
    }
    return false;
}


void CAutoDefModifierCombo::x_AddTypeStrainModifiers(TExtraOrgMods& extra_orgmods, TExtraSubSrcs& extra_subsrcs, const CBioSource& bsrc)
{
    if (x_HasTypeStrainComment(bsrc)) {
        if (extra_orgmods.find(COrgMod::eSubtype_strain) == extra_orgmods.end()) {
            extra_orgmods.insert(TExtraOrgMod(COrgMod::eSubtype_strain, true));
        }
    }    
}


typedef struct {
    size_t subtype;
    bool is_orgmod;
} SPreferredQual;

static const SPreferredQual s_PreferredList[] = {
    { CSubSource::eSubtype_transgenic, false } ,
    { COrgMod::eSubtype_culture_collection, true },
    { COrgMod::eSubtype_strain, true },
    { COrgMod::eSubtype_isolate, true },
    { COrgMod::eSubtype_cultivar, true },
    { COrgMod::eSubtype_specimen_voucher, true },
    { COrgMod::eSubtype_ecotype, true },
    { COrgMod::eSubtype_serotype, true},
    { COrgMod::eSubtype_breed, true},

    { COrgMod::eSubtype_bio_material, true },
    { COrgMod::eSubtype_biotype, true },
    { COrgMod::eSubtype_biovar, true },
    { COrgMod::eSubtype_chemovar, true },
    { COrgMod::eSubtype_pathovar, true },
    { COrgMod::eSubtype_serogroup, true },
    { COrgMod::eSubtype_serovar, true },
    { COrgMod::eSubtype_substrain, true },

    { CSubSource::eSubtype_plasmid_name, false } ,
    { CSubSource::eSubtype_endogenous_virus_name, false } ,
    { CSubSource::eSubtype_clone, false } ,
    { CSubSource::eSubtype_haplotype, false } ,

    { CSubSource::eSubtype_cell_line, false } ,
    { CSubSource::eSubtype_chromosome, false } ,
    { CSubSource::eSubtype_country, false } ,
    { CSubSource::eSubtype_dev_stage, false } ,
    { CSubSource::eSubtype_genotype, false } ,
    { CSubSource::eSubtype_haplogroup, false } ,

    { CSubSource::eSubtype_linkage_group, false } ,
    { CSubSource::eSubtype_map, false } ,
    { CSubSource::eSubtype_pop_variant, false } ,
    { CSubSource::eSubtype_segment, false } ,
    { CSubSource::eSubtype_subclone, false } ,
    { CSubSource::eSubtype_other, false } ,
    { COrgMod::eSubtype_other, true } ,
};


static const size_t kNumPreferred = sizeof(s_PreferredList) / sizeof (SPreferredQual);

void CAutoDefModifierCombo::GetAvailableModifiers(CAutoDefSourceDescription::TAvailableModifierVector &modifier_list)
{
    size_t k;

    // first, set up modifier list with blanks
    modifier_list.clear();

    for (k = 0; k < kNumPreferred; k++) {
        if (s_PreferredList[k].is_orgmod) {
            modifier_list.push_back(CAutoDefAvailableModifier((COrgMod::ESubtype)s_PreferredList[k].subtype, true));
        } else {
            modifier_list.push_back(CAutoDefAvailableModifier((CSubSource::ESubtype)s_PreferredList[k].subtype, false));
        }
    }

    for (k = 0; k < m_GroupList.size(); k++) {
        m_GroupList[k]->GetAvailableModifiers(modifier_list);
    }
}


bool CAutoDefModifierCombo::IsUsableInDefline(CSubSource::ESubtype subtype)
{
    size_t k;
    for (k = 0; k < kNumPreferred; k++) {
        if (!s_PreferredList[k].is_orgmod && (CSubSource::ESubtype)s_PreferredList[k].subtype == subtype) {
            return true;
        }
    }
    return false;
}


bool CAutoDefModifierCombo::IsUsableInDefline(COrgMod::ESubtype subtype)
{
    size_t k;
    for (k = 0; k < kNumPreferred; k++) {
        if (s_PreferredList[k].is_orgmod && (COrgMod::ESubtype)s_PreferredList[k].subtype == subtype) {
            return true;
        }
    }
    return false;
}


bool CAutoDefModifierCombo::IsModifierRequiredByDefault(bool is_orgmod, int subtype)
{
    bool rval = false;
if (is_orgmod) {
    rval = false;
} else {
    if (subtype == CSubSource::eSubtype_endogenous_virus_name ||
        subtype == CSubSource::eSubtype_plasmid_name ||
        subtype == CSubSource::eSubtype_transgenic) {
        rval = true;
    } else {
        rval = false;
    }
}
return rval;
}


string CAutoDefModifierCombo::GetSourceDescriptionString(const CBioSource& bsrc)
{
    unsigned int k;
    string       source_description = "";
    map<COrgMod::ESubtype, bool> orgmods;
    map<CSubSource::ESubtype, bool> subsrcs;
    bool no_extras = false;

    /* start with tax name */
    source_description += bsrc.GetOrg().GetTaxname();
    x_CleanUpTaxName(source_description);

    x_AddRequiredSubSourceModifiers(orgmods, subsrcs, bsrc);

    x_AddHIVModifiers(orgmods, subsrcs, bsrc);
    x_AddTypeStrainModifiers(orgmods, subsrcs, bsrc);

    /* should this organism be excluded? */
    if (m_ExcludeSpOrgs) {
        string::size_type pos = NStr::Find(source_description, " sp. ");
        if (pos != NCBI_NS_STD::string::npos
            && (pos < 2 || !NStr::StartsWith(source_description.substr(pos - 2), "f."))) {
            no_extras = true;
            // but add plasmid name anyway
            if (subsrcs.find(CSubSource::eSubtype_plasmid_name) == subsrcs.end()) {
                subsrcs.insert(TExtraSubSrc(CSubSource::eSubtype_plasmid_name, true));
            }
        }
    }
    if (m_ExcludeCfOrgs) {
        string::size_type pos = NStr::Find(source_description, " cf. ");
        if (pos != NCBI_NS_STD::string::npos) {
            no_extras = true;
        }
    }
    if (m_ExcludeNrOrgs) {
        string::size_type pos = NStr::Find(source_description, " nr. ");
        if (pos != NCBI_NS_STD::string::npos) {
            no_extras = true;
        }
    }
    if (m_ExcludeAffOrgs) {
        string::size_type pos = NStr::Find(source_description, " aff. ");
        if (pos != NCBI_NS_STD::string::npos) {
            no_extras = true;
        }
    }

    if (!no_extras) {
        if (bsrc.CanGetOrigin() && bsrc.GetOrigin() == CBioSource::eOrigin_mut) {
            source_description = "Mutant " + source_description;
        }

        // add requested orgmods
        for (unsigned int k = 0; k < m_OrgMods.size(); k++) {
            if (orgmods.find(m_OrgMods[k]) == orgmods.end()) {
                orgmods.insert(TExtraOrgMod(m_OrgMods[k], true));
            }
        }

        // add requested subsources
        for (unsigned int k = 0; k < m_SubSources.size(); k++) {
            if (subsrcs.find(m_SubSources[k]) == subsrcs.end()) {
                subsrcs.insert(TExtraSubSrc(m_SubSources[k], true));
            }
        }
    }


    for (k = 0; k < kNumPreferred; k++) {
        if (s_PreferredList[k].is_orgmod) {
            if (orgmods.find((COrgMod::ESubtype)s_PreferredList[k].subtype) != orgmods.end()) {
                x_AddOrgModString(source_description, bsrc, (COrgMod::ESubtype)s_PreferredList[k].subtype);
            }
        } else {
            if (subsrcs.find((CSubSource::ESubtype)s_PreferredList[k].subtype) != subsrcs.end()) {
                x_AddSubsourceString(source_description, bsrc, (CSubSource::ESubtype)s_PreferredList[k].subtype);
            }
        }
    }

    // add maxicircle/minicircle
    x_AddMinicircle(source_description, bsrc);
    
    return source_description;
}


bool CAutoDefModifierCombo::x_AddMinicircle(string& source_description, const string& note_text)
{
    bool any_change = false;
    vector<CTempString> tokens;
    NStr::Split(note_text, ";", tokens, NStr::fSplit_Tokenize);
    ITERATE(vector<CTempString>, t, tokens) {
        if (NStr::Find(*t, "maxicircle") != string::npos ||
            NStr::Find(*t, "minicircle") != string::npos) {
            string add = *t;
            NStr::TruncateSpacesInPlace(add);
            source_description += " " + add;
            any_change = true;
        }
    }
    return any_change;
}


bool CAutoDefModifierCombo::x_AddMinicircle(string& source_description, const CBioSource& bsrc)
{
    bool any_change = false;
    if (bsrc.IsSetSubtype()) {
        ITERATE(CBioSource::TSubtype, it, bsrc.GetSubtype()) {
            if ((*it)->IsSetSubtype() && (*it)->IsSetName() &&
                (*it)->GetSubtype() == CSubSource::eSubtype_other) {
                any_change |= x_AddMinicircle(source_description, (*it)->GetName());
            }
        }
    }
    if (bsrc.IsSetOrg() && bsrc.GetOrg().IsSetOrgname() && bsrc.GetOrg().GetOrgname().IsSetMod()) {
        ITERATE(COrgName::TMod, it, bsrc.GetOrg().GetOrgname().GetMod()) {
            if ((*it)->IsSetSubtype() && (*it)->IsSetSubname() &&
                (*it)->GetSubtype() == COrgMod::eSubtype_other) {
                any_change |= x_AddMinicircle(source_description, (*it)->GetSubname());
            }
        }
    }
    return any_change;
}


unsigned int CAutoDefModifierCombo::GetNumUnique () const
{
    unsigned int num = 0;

    ITERATE (TGroupListVector, it, m_GroupList) {
        if ((*it)->GetSrcList().size() == 1) {
            num++;
        }
    }
    return num;
}


unsigned int CAutoDefModifierCombo::GetMaxInGroup () const
{
    unsigned int num = 0;

    ITERATE (TGroupListVector, it, m_GroupList) {
        if ((*it)->GetSrcList().size() > num) {
            num = (*it)->GetSrcList().size();
        }
    }
    return num;
}


/* NOTE - we want to sort combos from most unique organisms to least unique organisms */
/* secondary sort - most groups to least groups */
/* tertiary sort - fewer max orgs in group to most max orgs in group */
/* fourth sort - least mods to most mods */
int CAutoDefModifierCombo::Compare(const CAutoDefModifierCombo& other) const
{
    int rval = 0;
    unsigned int num_this, num_other;

    num_this = GetNumUnique();
    num_other = other.GetNumUnique();
    if (num_this > num_other) {
        rval = -1;
    } else if (num_this < num_other) {
        rval = 1;
    } else {
        num_this = m_GroupList.size();
        num_other = other.GetGroupList().size();
        if (num_this > num_other) {
            rval = -1;
        } else if (num_this < num_other) {
            rval = 1;
        } else {
            num_this = GetMaxInGroup ();
            num_other = other.GetMaxInGroup();
            if (num_this < num_other) {
                rval = -1;
            } else if (num_this > num_other) {
                rval = 1;
            } else {
                num_this = m_Modifiers.size();
                num_other = other.GetModifiers().size();
                if (num_this < num_other) {
                    rval = -1;
                } else if (num_this > num_other) {
                    rval = 1;
                }
            }
        }
    }
    return rval;
}


struct SAutoDefSourceGroupByStrings {
    bool operator()(const CAutoDefSourceGroup& s1,
                    const CAutoDefSourceGroup& s2) const
    {
        return (s1 < s2);
    }
};


bool CAutoDefModifierCombo::AddQual (bool IsOrgMod, int subtype, bool even_if_not_uniquifying)
{
    bool added = false, rval = false;
    vector <CAutoDefSourceGroup *> new_groups;

    new_groups.clear();
    NON_CONST_ITERATE (TGroupListVector, it, m_GroupList) {
        added |= (*it)->AddQual (IsOrgMod, subtype, m_KeepAfterSemicolon);
    }

    if (added) {
        NON_CONST_ITERATE (TGroupListVector, it, m_GroupList) {
            vector <CAutoDefSourceGroup *> tmp = (*it)->RemoveNonMatchingDescriptions();
            while (!tmp.empty()) {
                new_groups.push_back (tmp[tmp.size() - 1]);
                tmp.pop_back();
                rval = true;
            }
        }
    }
    // NOTE - need to put groups from non-matching descriptions and put them in a new_groups list
    // in order to avoid processing them twice
    while (!new_groups.empty()) {
        m_GroupList.push_back (new_groups[new_groups.size() - 1]);
        new_groups.pop_back();
    }

    if (rval || even_if_not_uniquifying) {
        m_Modifiers.push_back (CAutoDefSourceModifierInfo (IsOrgMod, subtype, ""));
        std::sort (m_GroupList.begin(), m_GroupList.end(), SAutoDefSourceGroupByStrings());
        if (IsOrgMod) {
            m_OrgMods.push_back ((COrgMod_Base::ESubtype)subtype);
        } else {
            m_SubSources.push_back ((CSubSource_Base::ESubtype)subtype);
        }
    }
    return rval;
}



bool CAutoDefModifierCombo::RemoveQual (bool IsOrgMod, int subtype)
{
    bool rval = false;

    NON_CONST_ITERATE (TGroupListVector, it, m_GroupList) {
        rval |= (*it)->RemoveQual (IsOrgMod, subtype);
    }
    return rval;
}


vector<CAutoDefModifierCombo *> CAutoDefModifierCombo::ExpandByAnyPresent()
{
    CAutoDefSourceDescription::TModifierVector mods;
    vector<CAutoDefModifierCombo *> expanded;

    expanded.clear();
    NON_CONST_ITERATE (TGroupListVector, it, m_GroupList) {
        if ((*it)->GetSrcList().size() == 1) {
            continue;
        }
        mods = (*it)->GetModifiersPresentForAny();
        ITERATE (CAutoDefSourceDescription::TModifierVector, mod_it, mods) {
            expanded.push_back (new CAutoDefModifierCombo (this));
            if (!expanded[expanded.size() - 1]->AddQual (mod_it->IsOrgMod(), mod_it->GetSubtype())) {
                expanded.pop_back ();
                RemoveQual(mod_it->IsOrgMod(), mod_it->GetSubtype());
            }
        }
        if (!expanded.empty()) {
            break;
        }
    }
    return expanded;
}


bool CAutoDefModifierCombo::AreFeatureClausesUnique()
{
    vector<string> clauses;

    ITERATE (TGroupListVector, g, m_GroupList) {
        CAutoDefSourceGroup::TSourceDescriptionVector src_list = (*g)->GetSrcList();
        CAutoDefSourceGroup::TSourceDescriptionVector::iterator s = src_list.begin();
        while (s != src_list.end()) {
            clauses.push_back((*s)->GetFeatureClauses());
            s++;
        }
    }
    if (clauses.size() < 2) {
        return true;
    }
    sort (clauses.begin(), clauses.end());
    bool unique = true;
    vector<string>::iterator sit = clauses.begin();
    string prev = *sit;
    sit++;
    while (sit != clauses.end() && unique) {
        if (NStr::Equal(prev, *sit)) {
            unique = false;
        } else {
            prev = *sit;
        }
        sit++;
    }
    return unique;
}


END_SCOPE(objects)
END_NCBI_SCOPE
