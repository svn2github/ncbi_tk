/*  $Id$
 * =========================================================================
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
 * =========================================================================
 *
 * Authors: Sema Kachalo
 *
 */

#include <ncbi_pch.hpp>
#include "discrepancy_core.hpp"
#include "utils.hpp"
#include <objects/general/Dbtag.hpp>
#include <objects/macro/Molecule_type.hpp>
#include <objects/macro/Molecule_class_type.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/RNA_gen.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/macro/String_constraint.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <sstream>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(discrepancy_case);


// COUNT_NUCLEOTIDES

DISCREPANCY_CASE(COUNT_NUCLEOTIDES, CSeq_inst, eOncaller | eSubmitter | eSmart, "Count nucleotide sequences")
{
    CSeq_inst::TMol mol = obj.GetMol();
    if (mol != CSeq_inst::eMol_dna && mol != CSeq_inst::eMol_rna && mol != CSeq_inst::eMol_na) {
        return;
    }
    m_Objs["[n] nucleotide Bioseq[s] [is] present"].Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary()));
}


DISCREPANCY_SUMMARIZE(COUNT_NUCLEOTIDES)
{
    m_Objs["[n] nucleotide Bioseq[s] [is] present"]; // If no sequences found still report 0
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// COUNT_PROTEINS

DISCREPANCY_CASE(COUNT_PROTEINS, CSeq_inst, eDisc, "Count Proteins")
{
    if (obj.GetMol() != CSeq_inst::eMol_aa) {
        return;
    }
    m_Objs["[n] protein sequence[s] [is] present"].Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary()));
}


DISCREPANCY_SUMMARIZE(COUNT_PROTEINS)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// MISSING_PROTEIN_ID
DISCREPANCY_CASE(MISSING_PROTEIN_ID, CSeq_inst, eDisc | eSubmitter | eSmart, "Missing Protein ID")
{
    CConstRef<CBioseq> bioseq = context.GetCurrentBioseq();
    if( ! bioseq || ! bioseq->IsAa() ) {
        return;
    }

    const CSeq_id * protein_id = context.GetProteinId();
    if( ! protein_id ) {
        m_Objs["[n] protein[s] [has] invalid ID[s]."].Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary()), false).Fatal();
    }
}

DISCREPANCY_SUMMARIZE(MISSING_PROTEIN_ID)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// INCONSISTENT_PROTEIN_ID
DISCREPANCY_CASE(INCONSISTENT_PROTEIN_ID, CSeq_inst, eDisc | eSubmitter | eSmart, "Inconsistent Protein ID")
{
    CConstRef<CBioseq> bioseq = context.GetCurrentBioseq();
    if( ! bioseq || ! bioseq->IsAa() ) {
        return;
    }

    const CSeq_id * protein_id = context.GetProteinId();
    if( ! protein_id ) {
        return;
    }
    _ASSERT(protein_id->IsGeneral());
    CTempString protein_id_prefix(
        GET_STRING_FLD_OR_BLANK(protein_id->GetGeneral(), Db));
    if( protein_id_prefix.empty() ) {
        return;
    }
    string protein_id_prefix_lowercase = protein_id_prefix;
    NStr::ToLower(protein_id_prefix_lowercase);

    // find (or create if none before) the canonical form of the
    // protein_id_prefix since case-insensitivity means it could have
    // multiple forms.  Here, the canonical form is the way it appears
    // the first time.
    CReportNode& canonical_forms_node = m_Objs["canonical forms"][protein_id_prefix_lowercase];
    string canonical_protein_id_prefix;
    if( canonical_forms_node.empty() ) {
        // haven't seen this protein_id_prefix_lowercase before so we have
        // to set the canonical form.
        canonical_protein_id_prefix = protein_id_prefix;
        canonical_forms_node[protein_id_prefix];
    } else {
        // use previously set canonical form;
        canonical_protein_id_prefix =
            canonical_forms_node.GetMap().begin()->first;
    }
    _ASSERT(NStr::EqualNocase(protein_id_prefix, canonical_protein_id_prefix));

    m_Objs[kEmptyStr]["[n] sequence[s] [has] protein ID prefix " + canonical_protein_id_prefix].Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary()), false);
}

DISCREPANCY_SUMMARIZE(INCONSISTENT_PROTEIN_ID)
{
    // if _all_ are identical, don't report
    CReportNode& reports_collected = m_Objs[kEmptyStr];
    if( reports_collected.GetMap().size() <= 1 ) {
        // if there are no sequences or all sequences have the same
        // canonical protein id, then do not show any discrepancy
        return;
    }

    m_ReportItems = reports_collected.Export(*this)->GetSubitems();
}

// SHORT_SEQUENCES
DISCREPANCY_CASE(SHORT_SEQUENCES, CSeq_inst, eDisc | eSubmitter | eSmart, "Find Short Sequences")
{
    if (obj.IsAa()) {
        return;
    }
    if (obj.IsSetLength() && obj.GetLength() < 50 && !context.IsCurrentRnaInGenProdSet()) {
        m_Objs["[n] sequence[s] [is] shorter than 50 nt"].Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary()));
    }
}


DISCREPANCY_SUMMARIZE(SHORT_SEQUENCES)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// N_RUNS

void FindNRuns(vector<CRange<TSeqPos> >& runs, const CSeq_data& seq_data, const TSeqPos start_pos, const TSeqPos len, const TSeqPos min_run_length) {
    CSeq_data as_iupacna;
    TSeqPos nconv = CSeqportUtil::Convert(seq_data, &as_iupacna, CSeq_data::e_Iupacna, 0, len);
    if (nconv == 0) {
        return;
    }

    CRange<TSeqPos> this_run;

    TSeqPos position = start_pos;

    const string& iupacna_str = as_iupacna.GetIupacna().Get();
    ITERATE (string, base, iupacna_str) {
        TSeqPos curr_length = this_run.GetLength();

        switch (*base) {
            case 'N':
                if (curr_length == 0) {
                    this_run.SetFrom(position);
                }
                this_run.SetToOpen(position + 1);
                break;
            default:
                if (curr_length >= min_run_length) {
                    runs.push_back(this_run);
                }

                if (curr_length != 0) {
                    this_run = CRange<TSeqPos>::GetEmpty();
                }
                break;
        }
        ++position;
    }
    if (this_run.GetLength() >= min_run_length) {
        runs.push_back(this_run);
    }
}


DISCREPANCY_CASE(N_RUNS, CSeq_inst, eDisc | eSubmitter | eSmart, "More than 10 Ns in a row")
{
    if (obj.IsAa() || context.SequenceHasFarPointers()) {
        return;
    }

    bool found_any = false;
    vector<CRange<TSeqPos> > runs;

    ostringstream sub_key;
    sub_key << context.GetCurrentBioseq()->GetFirstId()->GetSeqIdString();
    sub_key << " has runs of Ns at the following locations: ";

    // CSeqMap, not CSeqVector, because gaps should not count as N runs.
    const CRef<CSeqMap> seq_map = CSeqMap::CreateSeqMapForBioseq(*context.GetCurrentBioseq());
    SSeqMapSelector sel;
    sel.SetFlags(CSeqMap::fFindData);
    CSeqMap_CI seq_iter(seq_map, &context.GetScope(), sel);
    for (; seq_iter; ++seq_iter) {
        FindNRuns(runs, seq_iter.GetData(), seq_iter.GetPosition(), seq_iter.GetLength(), 10);
        ITERATE (vector<CRange<TSeqPos> >, run, runs) {
            if (!found_any) {
                found_any = true;
            } else {
                sub_key << ", ";
            }

            sub_key << (run->GetFrom() + 1) << "-" << (run->GetTo() + 1);
        }
        runs.clear();
    }

    if (found_any) {
        m_Objs["[n] sequence[s] [has] runs of 10 or more Ns"][sub_key.str()].Ext().Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary())).Fatal();
    }
}


DISCREPANCY_SUMMARIZE(N_RUNS)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// PERCENT_N

DISCREPANCY_CASE(PERCENT_N, CSeq_inst, eDisc | eSubmitter | eSmart, "More than 5 percent Ns")
{
    if (obj.IsAa() || context.SequenceHasFarPointers()) {
        return;
    }
    const CSeqSummary& sum = context.GetSeqSummary();
    if (sum.N * 100. / sum.Len > 5) {
        m_Objs["[n] sequence[s] [has] more than 5% Ns"].Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary()));
    }
}


DISCREPANCY_SUMMARIZE(PERCENT_N)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// INTERNAL_TRANSCRIBED_SPACER_RRNA

static const char* kRRNASpacer[] = { "internal", "transcribed", "spacer", "\0" };


DISCREPANCY_CASE(INTERNAL_TRANSCRIBED_SPACER_RRNA, CRNA_ref, eOncaller, "Look for rRNAs that contain either 'internal', 'transcribed' or 'spacer'")
{
    CConstRef<CSeq_feat> feat = context.GetCurrentSeq_feat();
    if (feat->GetData().GetSubtype() != CSeqFeatData::eSubtype_rRNA) {
        return;
    }

    const string rna_name = obj.GetRnaProductName();
    for (size_t i = 0; kRRNASpacer[i][0] != '\0'; ++i) {
        if (NStr::FindNoCase(rna_name, kRRNASpacer[i]) != NPOS) {
            m_Objs["[n] rRNA feature products contain 'internal', 'transcribed', or 'spacer'"].Add(*context.NewDiscObj(context.GetCurrentSeq_feat()));
        }
    }
}


DISCREPANCY_SUMMARIZE(INTERNAL_TRANSCRIBED_SPACER_RRNA)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// OVERLAPPING_CDS

static bool StrandsMatch(ENa_strand strand1, ENa_strand strand2)
{
    return (strand1 == eNa_strand_minus && strand2 == eNa_strand_minus) || (strand1 != eNa_strand_minus && strand2 != eNa_strand_minus);
}


static const char* kSimilarProductWords[] = { "transposase", "integrase" };
static const int   kNumSimilarProductWords = sizeof (kSimilarProductWords) / sizeof (char *);

static const char* kIgnoreSimilarProductWords[] = { "hypothetical protein", "phage", "predicted protein" };
static const int   kNumIgnoreSimilarProductWords = sizeof (kIgnoreSimilarProductWords) / sizeof (char *);


static bool ProductNamesAreSimilar(const string& product1, const string& product2)
{
    bool str1_has_similarity_word = false, str2_has_similarity_word = false;

    size_t i;
    // if both product names contain one of the special case similarity words, the product names are similar.
  
    for (i = 0; i < kNumSimilarProductWords; i++) {
        if (string::npos != NStr::FindNoCase(product1, kSimilarProductWords[i])) {
            str1_has_similarity_word = true;
        }

        if (string::npos != NStr::FindNoCase(product2, kSimilarProductWords[i])) {
            str2_has_similarity_word = true;
        }
    }
    if (str1_has_similarity_word && str2_has_similarity_word) {
        return true;
    }
  
    // otherwise, if one of the product names contains one of special ignore similarity
    // words, the product names are not similar.

    for (i = 0; i < kNumIgnoreSimilarProductWords; i++) {
        if (string::npos != NStr::FindNoCase(product1, kIgnoreSimilarProductWords[i]) || string::npos != NStr::FindNoCase(product2, kIgnoreSimilarProductWords[i])) {
            return false;
        }
    }

    return !NStr::CompareNocase(product1, product2);
}


static bool ShouldIgnore(const string& product)
{
    if (NStr::Find(product, "transposon") != string::npos || NStr::Find(product, "transposase") != string::npos) {
        return true;
    }
    CString_constraint constraint;
    constraint.SetMatch_text("ABC");
    constraint.SetCase_sensitive(true);
    constraint.SetWhole_word(true);
    return constraint.Match(product);
}


static const string kOverlappingCDSNoteText = "overlaps another CDS with the same product name";


static bool HasOverlapNote(const CSeq_feat& feat)
{
    return feat.IsSetComment() && NStr::Find(feat.GetComment(), kOverlappingCDSNoteText) != string::npos;
}


static bool SetOverlapNote(CSeq_feat& feat)
{
    if (feat.IsSetComment() && NStr::Find(feat.GetComment(), kOverlappingCDSNoteText) != string::npos) {
        return false;
    }
    AddComment(feat, (string)kOverlappingCDSNoteText);
    return true;
}


static const char* kOverlap0 = "[n] coding region[s] overlap[S] another coding region with a similar or identical name.";
static const char* kOverlap1 = "[n] coding region[s] overlap[S] another coding region with a similar or identical name, but [has] the appropriate note text.";
static const char* kOverlap2 = "[n] coding region[s] overlap[S] another coding region with a similar or identical name and [does] not have the appropriate note text.";


static string GetProdName(const CSeq_feat* feat, map<const CSeq_feat*, string>& products, CDiscrepancyContext& context)
{
    if (products.find(feat) == products.end()) {
        string name = context.GetProdForFeature(*feat);
        products[feat] = name.empty() || ShouldIgnore(name) ? kEmptyStr : name;
    }
    return products[feat];
}


DISCREPANCY_CASE(OVERLAPPING_CDS, COverlappingFeatures, eDisc, "Overlapping CDs")
{
    if (!context.GetCurrentBioseq()->CanGetInst() || !context.GetCurrentBioseq()->GetInst().IsNa()) {
        return;
    }
    const vector<CConstRef<CSeq_feat> >& cds = context.FeatCDS();
    map<const CSeq_feat*, string> products;
    for (size_t i = 0; i < cds.size(); i++) {
        const CSeq_loc& loc_i = cds[i]->GetLocation();
        ENa_strand strand_i = loc_i.GetStrand();
        for (size_t j = i+1; j < cds.size(); j++) {
            const CSeq_loc& loc_j = cds[j]->GetLocation();
            if (!StrandsMatch(strand_i, loc_j.GetStrand()) || context.Compare(loc_i, loc_j) == sequence::eNoOverlap) {
                continue;
            }
            string prod_i = GetProdName(cds[i], products, context);
            if (prod_i.empty()) {
                break;
            }
            string prod_j = GetProdName(cds[j], products, context);
            if (prod_j.empty() || !ProductNamesAreSimilar(prod_i, prod_j)) {
                continue;
            }
            bool has_note = HasOverlapNote(*cds[i]);
            m_Objs[kOverlap0][has_note ? kOverlap1 : kOverlap2].Add(*context.NewDiscObj(cds[i], eNoRef, !has_note));
            has_note = HasOverlapNote(*cds[j]);
            m_Objs[kOverlap0][has_note ? kOverlap1 : kOverlap2].Add(*context.NewDiscObj(cds[j], eNoRef, !has_note));
        }
    }
}


DISCREPANCY_SUMMARIZE(OVERLAPPING_CDS)
{
    if (m_Objs.Exist(kOverlap0)) {
        m_Objs[kOverlap0].Promote();
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_AUTOFIX(OVERLAPPING_CDS)
{
    unsigned int n = 0;
    TReportObjectList list = item->GetDetails();
    NON_CONST_ITERATE (TReportObjectList, it, list) {
        if ((*it)->CanAutofix()) {
            const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->GetObject().GetPointer());
            if (sf) {
                CRef<CSeq_feat> new_feat(new CSeq_feat());
                new_feat->Assign(*sf);
                if (SetOverlapNote(*new_feat)) {
                    CSeq_feat_EditHandle feh(scope.GetSeq_featHandle(*sf));
                    feh.Replace(*new_feat);
                    ++n;
                    dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->SetFixed();
                }
            }
        }
    }
    return CRef<CAutofixReport>(n ? new CAutofixReport("OVERLAPPING_CDS: Set note for [n] coding region[s]", n) : 0);
}


DISCREPANCY_CASE(PARTIAL_CDS_COMPLETE_SEQUENCE, CSeq_feat, eDisc | eSubmitter | eSmart, "Partial CDSs in Complete Sequences")
{
    if (obj.GetData().GetSubtype() != CSeqFeatData::eSubtype_cdregion) {
        return;
    }

    // leave if this CDS is not at least in some way marked as partial
    if( ! GET_FIELD_OR_DEFAULT(obj, Partial, false) &&
        ! (obj.IsSetLocation() && obj.GetLocation().IsPartialStart(eExtreme_Biological)) &&
        ! (obj.IsSetLocation() && obj.GetLocation().IsPartialStop(eExtreme_Biological) ))
    {
        return;
    }

    // leave if we're not on a complete sequence
    const CMolInfo * mol_info = context.GetCurrentMolInfo();
    if( ! mol_info ||
        ! FIELD_EQUALS(*mol_info, Completeness,
                       CMolInfo::eCompleteness_complete) )
    {
        return;
    }

    // record the issue
    m_Objs["[n] partial CDS[s] in complete sequence[s]"].Add(*context.NewDiscObj(context.GetCurrentSeq_feat()));
}


DISCREPANCY_SUMMARIZE(PARTIAL_CDS_COMPLETE_SEQUENCE)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_CASE(RNA_NO_PRODUCT, CSeq_feat, eDisc | eOncaller | eSubmitter | eSmart, "Find RNAs without Products")
{
    if( ! obj.GetData().IsRna() ) {
        return;
    }

    // for the given RNA subtype, see whether a product is required
    switch(obj.GetData().GetSubtype()) {
    case CSeqFeatData::eSubtype_otherRNA: {
        CTempString comment( obj.IsSetComment() ? obj.GetComment() : kEmptyStr);
        if( NStr::StartsWith(comment, "contains ", NStr::eNocase) ||
            NStr::StartsWith(comment, "may contain", NStr::eNocase) )
        {
            return;
        }
        break;
    }
    case CSeqFeatData::eSubtype_tmRNA:
        // don't require products for tmRNA
        return;
    case CSeqFeatData::eSubtype_ncRNA: {
        // if ncRNA has a class other than "other", don't need a product
        const CRNA_ref & rna_ref = obj.GetData().GetRna();
        if( ! FIELD_IS_SET_AND_IS(rna_ref, Ext, Gen) ) {
            // no RNA-gen, so no class, so needs a product
            break;
        }
        const CTempString gen_class(
            GET_STRING_FLD_OR_BLANK(rna_ref.GetExt().GetGen(), Class));
        if( ! gen_class.empty() && ! NStr::EqualNocase(gen_class, "other") ) {
            // product has a product other than "other", so no
            // explicit product needed.
            return;
        }
        break;
    }
    default:
        // other kinds always need a product
        break;
    }

    const CRNA_ref & rna_ref = obj.GetData().GetRna();

    if( ! rna_ref.IsSetExt() ) {
        // will try other ways farther below
    } else {
        const CRNA_ref::TExt & rna_ext = rna_ref.GetExt();
        switch(rna_ext.Which()) {
        case CRNA_ref::TExt::e_Name: {
            const string & ext_name = rna_ref.GetExt().GetName();
            if( ! ext_name.empty() &&
                ! NStr::EqualNocase(ext_name, "ncRNA") &&
                ! NStr::EqualNocase(ext_name, "tmRNA") &&
                ! NStr::EqualNocase(ext_name, "misc_RNA") )
            {
                // ext.name can considered a product
                return;
            }
            break;
        }
        case CRNA_ref::TExt::e_TRNA:
        case CRNA_ref::TExt::e_Gen:
            if( ! rna_ref.GetRnaProductName().empty() ) {
                // found a product
                return;
            }
            break;
        default:
            _TROUBLE;
            break;
        }
    }

    // try to get it from a "product" qual
    if( ! obj.GetNamedQual("product").empty() ) {
        // gb-qual can be considered a product
        return;
    }

    // could not find a product
    m_Objs["[n] RNA feature[s] [has] no product and [is] not pseudo"].Add(*context.NewDiscObj(context.GetCurrentSeq_feat()), false);  // not unique
}


DISCREPANCY_SUMMARIZE(RNA_NO_PRODUCT)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// CONTAINED_CDS

static bool HasContainedNote(const CSeq_feat& feat)
{
    return feat.IsSetComment() && NStr::EqualNocase(feat.GetComment(), "completely contained in another CDS");
}


static void DeleteProteinSequence(CBioseq_Handle prot)
{
    // Should be a protein!
    if ( prot && prot.IsProtein() && !prot.IsRemoved() ) {
        // Get the protein parent set before you remove the protein
        CBioseq_set_Handle bssh = prot.GetParentBioseq_set();

        // Delete the protein
        CBioseq_EditHandle prot_eh(prot);
        prot_eh.Remove();

        // If lone nuc remains, renormalize the nuc-prot set
        if (bssh && bssh.IsSetClass() 
            && bssh.GetClass() == CBioseq_set::eClass_nuc_prot
            && !bssh.IsEmptySeq_set() 
            && bssh.GetBioseq_setCore()->GetSeq_set().size() == 1) 
        {
            // Renormalize the lone nuc that's inside the nuc-prot set into  
            // a nuc bioseq.  This call will remove annots/descrs from the 
            // set and attach them to the seq.
            bssh.GetParentEntry().GetEditHandle().ConvertSetToSeq();
        }
    }
}


static bool ConvertCDSToMiscFeat(const CSeq_feat& feat, CScope& scope)
{
    if (!feat.GetData().IsCdregion() || feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_misc_feature) {
        return false;
    }
    CRef<CSeq_feat> replacement(new CSeq_feat());
    replacement->Assign(feat);
    replacement->SetData().SetImp().SetKey("misc_feature");
    if (feat.IsSetProduct()) {
        string product = GetProductName(feat, scope);
        if (!NStr::IsBlank(product)) {
            AddComment(*replacement, product);
        }
    }
    try {
        CSeq_feat_EditHandle feh(scope.GetSeq_featHandle(feat));
        feh.Replace(*replacement);
    } catch (...) {
        // feature may have already been removed or converted
        return false;
    }
    return true;
}


static const char* kContained = "[n] coding region[s] [is] completely contained in another coding region.";
static const char* kContainedNote = "[n] coding region[s] [is] completely contained in another coding region, but have note.";
static const char* kContainedSame = "[n] coding region[s] [is] completely contained in another coding region on the same strand.";
static const char* kContainedOpps = "[n] coding region[s] [is] completely contained in another coding region, but on the opposite strand.";


DISCREPANCY_CASE(CONTAINED_CDS, COverlappingFeatures, eDisc | eSubmitter | eSmart, "Contained CDs")
{
    if (!context.GetCurrentBioseq()->CanGetInst() || !context.GetCurrentBioseq()->GetInst().IsNa() || context.IsEukaryotic()) {
        return;
    }
    const vector<CConstRef<CSeq_feat> >& cds = context.FeatCDS();
    for (size_t i = 0; i < cds.size(); i++) {
        const CSeq_loc& loc_i = cds[i]->GetLocation();
        ENa_strand strand_i = loc_i.GetStrand();
        for (size_t j = i+1; j < cds.size(); j++) {
            const CSeq_loc& loc_j = cds[j]->GetLocation();
            sequence::ECompare compare = context.Compare(loc_j, loc_i);
            if (compare == sequence::eContains || compare == sequence::eSame || compare == sequence::eContained) {
                const char* strand = StrandsMatch(strand_i, loc_j.GetStrand()) ? kContainedSame : kContainedOpps;
                bool has_note = HasContainedNote(*cds[i]);
                m_Objs[kContained][has_note ? kContainedNote : strand].Add(*context.NewDiscObj(cds[i], eNoRef, compare == sequence::eContained && !has_note));
                has_note = HasContainedNote(*cds[j]);
                m_Objs[kContained][has_note ? kContainedNote : strand].Add(*context.NewDiscObj(cds[j], eNoRef, compare == sequence::eContains && !has_note));
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(CONTAINED_CDS)
{
    if (m_Objs.Exist(kContained) && m_Objs[kContained].GetMap().size() == 1) {
        m_ReportItems = m_Objs[kContained].Export(*this)->GetSubitems();
    }
    else {
        m_ReportItems = m_Objs.Export(*this)->GetSubitems();
    }
}


DISCREPANCY_AUTOFIX(CONTAINED_CDS)
{
    unsigned int n = 0;
    TReportObjectList list = item->GetDetails();
    NON_CONST_ITERATE (TReportObjectList, it, list) {
        if ((*it)->CanAutofix()) {
            const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->GetObject().GetPointer());
            if (sf) {
                if (ConvertCDSToMiscFeat(*sf, scope)) {
                    ++n;
                    dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->SetFixed();
                }
            }
        }
    }
    return CRef<CAutofixReport>(n ? new CAutofixReport("CONTAINED_CDS: Converted [n] coding region[s] to misc_feat", n) : 0);
}


DISCREPANCY_CASE(ZERO_BASECOUNT, CSeq_inst, eDisc | eOncaller | eSubmitter | eSmart, "Zero Base Counts")
{
    static const char* kMsg = "[n] sequence[s] [has] a zero basecount for a nucleotide";
    if (obj.IsAa() || context.SequenceHasFarPointers()) {
        return;
    }
    const CSeqSummary& sum = context.GetSeqSummary();
    if (!sum.A) {
        m_Objs[kMsg]["[n] sequence[s] [has] no As"].Ext().Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary()));
    }
    if (!sum.C) {
        m_Objs[kMsg]["[n] sequence[s] [has] no Cs"].Ext().Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary()));
    }
    if (!sum.G) {
        m_Objs[kMsg]["[n] sequence[s] [has] no Gs"].Ext().Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary()));
    }
    if (!sum.T) {
        m_Objs[kMsg]["[n] sequence[s] [has] no Ts"].Ext().Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary()));
    }
}


DISCREPANCY_SUMMARIZE(ZERO_BASECOUNT)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// NONWGS_SETS_PRESENT
DISCREPANCY_CASE(NONWGS_SETS_PRESENT, CBioseq_set, eDisc, "Eco, mut, phy or pop sets present")
{
    _ASSERT(&obj == &*context.GetCurrentBioseq_set());

    if( ! obj.IsSetClass() ) {
        return;
    }

    CBioseq_set::EClass bioseq_set_class = obj.GetClass();
    switch(bioseq_set_class) {
    case CBioseq_set::eClass_eco_set:
    case CBioseq_set::eClass_mut_set:
    case CBioseq_set::eClass_phy_set:
    case CBioseq_set::eClass_pop_set:
        // non-WGS set found
        m_Objs["[n] set[s] [is] of type eco, mut, phy or pop"].Add(*context.NewDiscObj(context.GetCurrentBioseq_set(), eNoRef, true), false);
        break;
    default:
        // other types are fine
        break;
    }
}


DISCREPANCY_SUMMARIZE(NONWGS_SETS_PRESENT)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_AUTOFIX(NONWGS_SETS_PRESENT)
{
    TReportObjectList list = item->GetDetails();
    unsigned int n = 0;
    NON_CONST_ITERATE (TReportObjectList, it, list) {
        if ((*it)->CanAutofix()) {
            const CBioseq_set* set = dynamic_cast<const CBioseq_set*>(dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->GetObject().GetPointer());
            if (set) {
                CBioseq_set_Handle set_h = scope.GetBioseq_setHandle(*set);
                CBioseq_set_EditHandle set_eh(set_h);
                set_eh.SetClass(CBioseq_set::eClass_genbank);
                ++n;
                dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->SetFixed();
            }
        }
    }
    return CRef<CAutofixReport>(n ? new CAutofixReport("NONWGS_SETS_PRESENT: Set class to GenBank for [n] set[s]", n) : 0);
}


//NO_ANNOTATION
DISCREPANCY_CASE(NO_ANNOTATION, CSeq_inst, eDisc | eOncaller | eSubmitter | eSmart, "No annotation")
{
    if (context.HasFeatures()) {
        return;
    }
    m_Objs["[n] bioseq[s] [has] no features"].Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary()));
}


DISCREPANCY_SUMMARIZE(NO_ANNOTATION)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_CASE(LONG_NO_ANNOTATION, CSeq_inst, eDisc | eOncaller | eSubmitter | eSmart, "No annotation for LONG sequence")
{
    const int kSeqLength = 5000;
    if (obj.IsAa() || context.HasFeatures() || !(obj.CanGetLength() && obj.GetLength() > kSeqLength)) {
        return;
    }
    m_Objs["[n] bioseq[s] [is] longer than 5000nt and [has] no features"].Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary()));
}


DISCREPANCY_SUMMARIZE(LONG_NO_ANNOTATION)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// POSSIBLE_LINKER


DISCREPANCY_CASE(POSSIBLE_LINKER, CSeq_inst, eOncaller, "Detect linker sequence after poly-A tail")
{
    if (obj.IsAa() ) {
        return;
    }

    CBioseq_Handle bsh = context.GetScope().GetBioseqHandle(*context.GetCurrentBioseq());
    if ( !bsh.IsSetInst_Mol()  ||
         bsh.GetInst_Mol() != CSeq_inst::eMol_rna ) {
        return;
    }

    CSeqdesc_CI desc_it (bsh, CSeqdesc::e_Molinfo);
    if ( !desc_it ) {
        return;
    }

    if ( ! (desc_it->GetMolinfo().IsSetBiomol())  || 
         (desc_it->GetMolinfo().GetBiomol() != CMolInfo::eBiomol_mRNA) ) {
        return;
    }

    if (bsh.IsSetInst_Length() && bsh.GetInst_Length() < 30) {
        // not long enough to have poly-a tail
        return;
    }
        
    CSeqVector seq_vec(*context.GetCurrentBioseq(), &context.GetScope(), CBioseq_Handle::eCoding_Iupac, eNa_strand_plus);

    static const size_t TAIL = 30;
    string seq_data(kEmptyStr);
    seq_vec.GetSeqData(bsh.GetInst_Length() - TAIL, bsh.GetInst_Length(), seq_data);

    size_t tail_len = 0;
    size_t cut = 0;

    for (size_t i = 0; i < seq_data.length(); i++) {
        if (seq_data[i] == 'A' || seq_data[i] == 'a') {
            tail_len++;
        }
        else {
            if (tail_len > 20) {
                cut = i;
            }
            tail_len = 0;
        }
    }

    if (cut) {
        cut = TAIL - cut;
        m_Objs["[n] bioseq[s] may have linker sequence after the poly-A tail"].Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary(), eNoRef, cut > 0, cut ? new CSimpleTypeObject<size_t>(cut) : 0));
    }
}


DISCREPANCY_SUMMARIZE(POSSIBLE_LINKER)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_AUTOFIX(POSSIBLE_LINKER)
{
    TReportObjectList list = item->GetDetails();
    size_t num_fixed = 0;
    NON_CONST_ITERATE (TReportObjectList, it, list) {
        if ((*it)->CanAutofix()) {
            CDiscrepancyObject& obj = *dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer());
            size_t cut_from_end = dynamic_cast<const CSimpleTypeObject<size_t>*>(obj.GetMoreInfo().GetPointer())->Value;
            const CBioseq* seq = dynamic_cast<const CBioseq*>(obj.GetObject().GetPointer());
            _ASSERT(seq);
            CBioseq_EditHandle besh(scope.GetBioseqEditHandle(*seq));
            SSeqMapSelector selector;
            selector.SetFlags(CSeqMap::fFindData); 
            CSeqMap_I seqmap_i(besh, selector);
            size_t start = 0;
            size_t stop = besh.GetInst_Length() - cut_from_end;
            while (seqmap_i) {
                TSeqPos len = seqmap_i.GetLength();
                if (start < stop && start + len > stop) {
                    string seq_in;
                    seqmap_i.GetSequence(seq_in, CSeqUtil::e_Iupacna);
                    string seq_out = seq_in.substr(0, stop - start);
                    seqmap_i.SetSequence(seq_out, CSeqUtil::e_Iupacna, seqmap_i.GetData().Which());
                    ++seqmap_i;
                }
                else if (start >= stop) {
                    seqmap_i = seqmap_i.Remove(); 
                }
                else {
                    ++seqmap_i;
                }
                start += len;
            }
            num_fixed++;
            dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->SetFixed();
        }
    }
    return CRef<CAutofixReport>(num_fixed ? new CAutofixReport("POSSIBLE_LINKER: [n] sequence[s] trimmed", num_fixed) : 0);
}


// ORDERED_LOCATION
DISCREPANCY_CASE(ORDERED_LOCATION, CSeq_feat, eDisc | eOncaller | eSmart, "Location is ordered (intervals interspersed with gaps)")
{
    if( ! obj.IsSetLocation() ) {
        return;
    }

    CSeq_loc_CI loc_ci(obj.GetLocation(), CSeq_loc_CI::eEmpty_Allow);
    for( ; loc_ci; ++loc_ci) {
        if( loc_ci.GetEmbeddingSeq_loc().IsNull() ) {
            CReportNode & message_report_node = m_Objs["[n] feature[s] [has] ordered location[s]"];
            message_report_node.Add(*context.NewDiscObj(context.GetCurrentSeq_feat(), eNoRef, true), false);
            return;
        }
    }
}


DISCREPANCY_SUMMARIZE(ORDERED_LOCATION)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_AUTOFIX(ORDERED_LOCATION)
{
    TReportObjectList list = item->GetDetails();
    size_t num_fixed = 0;
    NON_CONST_ITERATE (TReportObjectList, it, list) {
        if ((*it)->CanAutofix()) {
            CDiscrepancyObject& obj = *dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer());
            const CSeq_feat* orig_seq_feat = dynamic_cast<const CSeq_feat*>(obj.GetObject().GetPointer());
            _ASSERT(orig_seq_feat);
            // no need to check CanAutofix because ordered locations are
            // always fixable
            _ASSERT(orig_seq_feat->IsSetLocation());
            // rebuild new loc but without the NULL parts
            CSeq_loc_I new_loc_creator(*SerialClone(orig_seq_feat->GetLocation()));
            while( new_loc_creator ) {
                if( new_loc_creator.GetEmbeddingSeq_loc().IsNull() ) {
                    new_loc_creator.Delete();
                } else {
                    ++new_loc_creator;
                }
            }
            if( ! new_loc_creator.HasChanges()) {
                // inefficient, but I suppose it's possible if something
                // elsewhere already fixed this
                continue;
            }

            // replace the Seq-feat
            CRef<CSeq_loc> new_seq_feat_loc = new_loc_creator.MakeSeq_loc(CSeq_loc_I::eMake_PreserveType);
            CRef<CSeq_feat> replacement_seq_feat(SerialClone(*orig_seq_feat));
            replacement_seq_feat->SetLocation(*new_seq_feat_loc);
            try {
                CSeq_feat_EditHandle seq_feat_h(scope.GetSeq_featHandle(*orig_seq_feat));
                seq_feat_h.Replace(*replacement_seq_feat);
                ++num_fixed;
                dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->SetFixed();
            } catch (...) {
                // feature may have already been removed
            }
        }
    }
    return CRef<CAutofixReport>(num_fixed ? new CAutofixReport("ORDERED_LOCATION: [n] features with ordered locations fixed", num_fixed) : 0);
}


const string kHasLocusTags = "has locus tags";

DISCREPANCY_CASE(MISSING_LOCUS_TAGS, CSeq_feat, eDisc | eSubmitter | eSmart, "Missing locus tags")
{
    if (!obj.IsSetData() || !obj.GetData().IsGene()) {
        return;
    }

    const CGene_ref& gene_ref = obj.GetData().GetGene();

    // Skip pseudo-genes
    if (gene_ref.CanGetPseudo() && gene_ref.GetPseudo() == true) {
        return;
    }

    // Report missing or empty locus tags
    if (!gene_ref.CanGetLocus_tag() || NStr::TruncateSpaces(gene_ref.GetLocus_tag()).empty()) {
        m_Objs["[n] gene[s] [has] no locus tag[s]."].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)));
    } else if (!m_Objs.Exist(kHasLocusTags)) {
        m_Objs[kHasLocusTags].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)));
    }
}


DISCREPANCY_SUMMARIZE(MISSING_LOCUS_TAGS)
{
    if (!m_Objs.Exist(kHasLocusTags)) {
        m_Objs.clear();
        return;
    }
    m_Objs.GetMap().erase(kHasLocusTags);
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_CASE(INCONSISTENT_LOCUS_TAG_PREFIX, CSeqFeatData, eDisc | eSubmitter | eSmart, "Inconsistent locus tag prefix")
{
    if (obj.Which() != CSeqFeatData::e_Gene) {
        return;
    }

    const CGene_ref& gene_ref = obj.GetGene();

    // Skip pseudo-genes
    if (gene_ref.CanGetPseudo() && gene_ref.GetPseudo() == true) {
        return;
    }

    // Skip missing locus tags
    if (!gene_ref.CanGetLocus_tag()) {
        return;
    }

    // Report on good locus tags - are they consistent?
    string locus_tag = gene_ref.GetLocus_tag();
    if (!locus_tag.empty() && !context.IsBadLocusTagFormat(locus_tag)) {
        // Store each unique prefix in a bin
        // If there is more than 1 bin, the prefixes are inconsistent
        string prefix;
        string tagvalue;
        NStr::SplitInTwo(locus_tag, "_", prefix, tagvalue);

        stringstream ss;
        ss << "[n] feature[s] [has] locus tag prefix " << prefix << ".";
        m_Objs[ss.str()].Add(*context.NewDiscObj(context.GetCurrentSeq_feat()));
    }
}


DISCREPANCY_SUMMARIZE(INCONSISTENT_LOCUS_TAG_PREFIX)
{
    // If there is more than 1 bin, the prefixes are inconsistent
    if (m_Objs.GetMap().size() > 1) {
        m_ReportItems = m_Objs.Export(*this)->GetSubitems();
    }
}


static const string kInconsistent_Moltype = "[n] sequences have inconsistent moltypes";

DISCREPANCY_CASE(INCONSISTENT_MOLTYPES, CSeq_inst, eDisc | eOncaller | eSmart, "Inconsistent molecule types")
{
    if (obj.IsAa() ) {
        return;
    }
    // Report on nucs only

    // Initialize moltype string with MolInfo.biomol 
    string moltype;
    const CMolInfo * mol_info = context.GetCurrentMolInfo();
    if( mol_info && mol_info->CanGetBiomol() ) {
        CMolInfo::TBiomol biomol = mol_info->GetBiomol();
        moltype = CMolInfo::GetBiomolName(biomol);
    }

    // If MolInfo.biomol is empty or all spaces, use "genomic" by default
    if (NStr::IsBlank(moltype)) {
        moltype = "genomic";
    }

    // Append Seq-inst.mol to moltype
    if (context.GetCurrentBioseq()->CanGetInst() &&
        context.GetCurrentBioseq()->GetInst().CanGetMol())
    {
        CSeq_inst::TMol mol = context.GetCurrentBioseq()->GetInst().GetMol();
        moltype += string(" ") + CSeq_inst::GetMoleculeClass(mol);
    }

    // Add each nuc bioseq, regardless of moltype, to get a total count
    m_Objs[kInconsistent_Moltype].Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary()));

    // Add each unique moltype as a key to the Extended Output
    stringstream ss;
    ss << "[n] sequence[s] [has] moltype " << moltype;
    m_Objs[kInconsistent_Moltype][ss.str()].Ext().Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary()));
}


DISCREPANCY_SUMMARIZE(INCONSISTENT_MOLTYPES)
{
    // If there is more than 1 key, the moltypes are inconsistent
    if (m_Objs[kInconsistent_Moltype].GetMap().size() > 1) {
        m_ReportItems = m_Objs.Export(*this)->GetSubitems();
    }
}


DISCREPANCY_CASE(BAD_LOCUS_TAG_FORMAT, CSeqFeatData, eDisc | eSubmitter | eSmart, "Bad locus tag format")
{
    if (obj.Which() != CSeqFeatData::e_Gene) {
        return;
    }

    const CGene_ref& gene_ref = obj.GetGene();

    // Skip pseudo-genes
    if (gene_ref.CanGetPseudo() && gene_ref.GetPseudo() == true) {
        return;
    }

    // Skip missing locus tags
    if (!gene_ref.CanGetLocus_tag()) {
        return;
    }

    // Report non-empty, bad-format locus tags
    string locus_tag = gene_ref.GetLocus_tag();
    if (!locus_tag.empty() && context.IsBadLocusTagFormat(locus_tag)) {
        m_Objs["[n] locus tag[s] [is] incorrectly formatted."].Add(*context.NewDiscObj(context.GetCurrentSeq_feat()));
    }
}


DISCREPANCY_SUMMARIZE(BAD_LOCUS_TAG_FORMAT)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// SEGSETS_PRESENT
DISCREPANCY_CASE(SEGSETS_PRESENT, CBioseq_set, eDisc | eSmart, "Segsets present")
{
    if( GET_FIELD_OR_DEFAULT(obj, Class, CBioseq_set::eClass_not_set) !=
        CBioseq_set::eClass_segset )
    {
        return;
    }

    m_Objs["[n] segset[s] [is] present"].Add(*context.NewDiscObj(context.GetCurrentBioseq_set()), false);
}


DISCREPANCY_SUMMARIZE(SEGSETS_PRESENT)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// QUALITY_SCORES
DISCREPANCY_CASE(QUALITY_SCORES, CSeq_annot, eDisc | eSmart, "Check for quality scores")
{
    if (!context.GetCurrentBioseq() || !context.GetCurrentBioseq()->IsSetInst() || context.GetCurrentBioseq()->IsAa()) {
        return;
    }

    if (m_Count != context.GetCountBioseq()) { // stepping onto a new sequence
        m_Count = context.GetCountBioseq();
        if (obj.IsGraph()) {
            m_Objs["has qual scores"].Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary()));
        }
    }
}


DISCREPANCY_SUMMARIZE(QUALITY_SCORES)
{
    size_t all_na = context.GetNaSeqs().size();
    size_t quals = m_Objs["has qual scores"].GetObjects().size();
    m_Objs.clear();

    string some = NStr::NumericToString(all_na - quals);
    if (all_na == quals) {
        m_Objs["Quality scores are present on all sequences"];
    }
    else if (quals == 0) {
        m_Objs["Quality scores are missing on all sequences"];
    }
    else {
        m_Objs["Quality scores are missing on some(" + some + ") sequences"];
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_CASE(BACTERIA_SHOULD_NOT_HAVE_MRNA, CSeqFeatData, eDisc | eOncaller | eSubmitter | eSmart, "Bacterial sequences should not have mRNA features")
{
    if (!context.IsBacterial() || obj.GetSubtype() != CSeqFeatData::eSubtype_mRNA) {
        return;
    }
    m_Objs["[n] bacterial sequence[s] [has] mRNA features"].Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary()));
}


DISCREPANCY_SUMMARIZE(BACTERIA_SHOULD_NOT_HAVE_MRNA)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// BAD_BGPIPE_QUALS

static const string kDiscMessage = "[n] feature[s] contain[S] invalid BGPIPE qualifiers";

DISCREPANCY_CASE(BAD_BGPIPE_QUALS, CSeq_feat, eDisc | eSmart, "Bad BGPIPE qualifiers")
{
    if (context.IsRefseq() || !context.IsBGPipe()) {
        return;
    }
    if(STRING_FIELD_NOT_EMPTY(obj, Except_text)) {
        m_Objs[kDiscMessage].Add(*context.NewDiscObj(context.GetCurrentSeq_feat()), false);
        return;
    }
    if (FIELD_IS_SET_AND_IS(obj, Data, Cdregion)) {
        const CCdregion & cdregion = obj.GetData().GetCdregion();
        if(RAW_FIELD_IS_EMPTY_OR_UNSET(cdregion, Code_break)) {
            return;
        }
        if(GET_STRING_FLD_OR_BLANK(obj, Comment) == "ambiguity in stop codon") {
            // check if any code break is a stop codon
            FOR_EACH_CODEBREAK_ON_CDREGION(code_break_it, cdregion) {
                const CCode_break & code_break = **code_break_it;
                if(FIELD_IS_SET_AND_IS(code_break, Aa, Ncbieaa) && code_break.GetAa().GetNcbieaa() == 42 /* *:Stop codon */) {
                    return;
                }
            }
            m_Objs[kDiscMessage].Add(*context.NewDiscObj(context.GetCurrentSeq_feat()), false);
            return;
        }
        FOR_EACH_CODEBREAK_ON_CDREGION(code_break_it, cdregion) {
            const CCode_break & code_break = **code_break_it;
            if( FIELD_IS_SET_AND_IS(code_break, Aa, Ncbieaa) && code_break.GetAa().GetNcbieaa() == 85 /* U:Sec */) {
                return;
            }
        }
        m_Objs[kDiscMessage].Add(*context.NewDiscObj(context.GetCurrentSeq_feat()), false);
    }
}


DISCREPANCY_SUMMARIZE(BAD_BGPIPE_QUALS)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// GENE_PRODUCT_CONFLICT

static const string kGeneProductConflict = "[n] coding region[s] [has] the same gene name as another coding region but a different product";

DISCREPANCY_CASE(GENE_PRODUCT_CONFLICT, CSeq_feat, eDisc | eSubmitter | eSmart, "Gene Product Conflict")
{
    if (obj.IsSetData() && obj.GetData().IsCdregion()) {
        CConstRef<CSeq_feat> gene_feat = sequence::GetBestGeneForCds(obj, context.GetScope());
        if (gene_feat && gene_feat->IsSetData() && gene_feat->GetData().IsGene()) {
            const CGene_ref& gene = gene_feat->GetData().GetGene();
            if (gene.IsSetLocus()) {
                m_Objs.Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj), eKeepRef), false);
            }
        }
    }
}

typedef list<pair<CConstRef<CSeq_feat>, string>> TGenesList;
typedef map<string, TGenesList> TGeneLocusMap;

static void CollectGenesByLocusTag(CDiscrepancyContext& context, TReportObjectList& objs, TGeneLocusMap& genes)
{
    NON_CONST_ITERATE (TReportObjectList, obj, objs) {
        const CSeq_feat* cds = dynamic_cast<const CSeq_feat*>(dynamic_cast<CDiscrepancyObject*>((*obj).GetNCPointer())->GetObject().GetPointer());
        if (cds) {
            CScope& scope = context.GetScope();
            CConstRef<CSeq_feat> gene = sequence::GetBestGeneForCds(*cds, scope);
            if (gene) {
                const string& locus = gene->GetData().GetGene().GetLocus();
                string product = context.GetProdForFeature(*cds);
                genes[locus].push_back(make_pair(CConstRef<CSeq_feat>(cds), product));
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(GENE_PRODUCT_CONFLICT)
{
    if (m_Objs.empty()) {
        return;
    }

    CReportNode report;
    TGeneLocusMap genes;

    CollectGenesByLocusTag(context, m_Objs.GetObjects(), genes);

    ITERATE (TGeneLocusMap, gene, genes) {
        if (gene->second.size() > 1) {
            TGenesList::const_iterator cur_gene = gene->second.begin();
            const string& product = cur_gene->second;
            bool diff = false;
            for (++cur_gene; cur_gene != gene->second.end(); ++cur_gene) {
                const string& cur_product = cur_gene->second;
                if (product != cur_product) {
                    diff = true;
                    break;
                }
            }
            if (diff) {
                string sub = "[n] coding regions have the same gene name (" + gene->first + ") as another coding region but a different product";
                ITERATE (TGenesList, cur_gene, gene->second) {
                    report[kGeneProductConflict][sub].Ext().Add(*context.NewDiscObj(cur_gene->first), false);
                }
            }
        }
    }
    m_ReportItems = report.Export(*this)->GetSubitems();
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
