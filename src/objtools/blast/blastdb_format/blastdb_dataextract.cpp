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
 * Author: Christiam Camacho
 *
 */

/** @file blastdb_dataextract.cpp
 *  Defines classes which extract data from a BLAST database
 */
#include <ncbi_pch.hpp>
#include <objtools/blast/blastdb_format/invalid_data_exception.hpp>
#include <objtools/blast/blastdb_format/blastdb_dataextract.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <corelib/ncbiutil.hpp>
#include <util/sequtil/sequtil_manip.hpp>
#include <util/checksum.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

#define NOT_AVAILABLE "N/A"
#define SEPARATOR ";"

extern string GetBareId(const CSeq_id& id);

void CBlastDBExtractor::SetSeqId(const CBlastDBSeqId &id, bool get_data) {
    m_Defline.Reset();
    m_Gi = ZERO_GI;
    m_Oid = -1;
    CRef<CSeq_id> seq_id;

    TGi target_gi = ZERO_GI;
    CSeq_id *target_seq_id = NULL;

    if (id.IsOID()) {
        m_Oid = id.GetOID();
    } else if (id.IsGi()) {
        m_Gi = id.GetGi();
        m_BlastDb.GiToOid(m_Gi, m_Oid);
        if (m_TargetOnly || ! get_data) target_gi = m_Gi;
    } else if (id.IsPig()) {
        m_BlastDb.PigToOid(id.GetPig(), m_Oid);
    } else if (id.IsStringId()) {
        string acc(id.GetStringId());
        NStr::ToUpper(acc);
        vector<TOID> oids;
        m_BlastDb.AccessionToOids(acc, oids);
        if (!oids.empty()) {
            m_Oid = oids[0];
            if (m_TargetOnly || ! get_data) {
                // TODO check if id is complete
                seq_id.Reset(new CSeq_id(acc, CSeq_id::fParse_PartialOK | CSeq_id::fParse_Default));
                target_seq_id = &(*seq_id);
            }
        }
    }

    if (m_Oid < 0) {
        NCBI_THROW(CSeqDBException, eArgErr,
                   "Entry not found in BLAST database");
    }

   	TSeqPos length = m_BlastDb.GetSeqLength(m_Oid);
   	if (length <= 0) {
       	NCBI_THROW(CSeqDBException, eArgErr,
           	       "Entry found in BLAST database has invalid length");
   	}

   	m_SeqRange = m_OrigSeqRange;
	if((TSeqPos)length <= m_SeqRange.GetTo())
	{
		m_SeqRange.SetTo(length-1);
	}

    if(TSeqRange::GetPositionMax() == m_OrigSeqRange.GetTo())
    {
		if (m_SeqRange.GetTo() < m_SeqRange.GetFrom()) {
	        NCBI_THROW(CSeqDBException, eArgErr,
	                   "start pos > length of sequence");
	    	}
    }

    try {
        if (get_data) {
            m_Bioseq.Reset(m_BlastDb.GetBioseq(m_Oid, target_gi, target_seq_id));
        }
	else if (m_Gi <= ZERO_GI)
	{  // If no GI, then all the Gi2XMaps will fail.
	    m_Bioseq.Reset(m_BlastDb.GetBioseqNoData(m_Oid, target_gi, target_seq_id));
	}

    } catch (const CSeqDBException& e) {
        // this happens when CSeqDB detects a GI that doesn't belong to a
        // filtered database (e.g.: swissprot as a subset of nr)
        if (e.GetMsg().find("oid headers do not contain target gi")) {
            NCBI_THROW(CSeqDBException, eArgErr,
                       "Entry not found in BLAST database");
        }
    }
}

string CBlastDBExtractor::ExtractOid() {
    return NStr::IntToString(m_Oid);
}

string CBlastDBExtractor::ExtractPig() {
    if (m_Oid2Pig.first != m_Oid)
    {
        CSeqDB::TPIG pig;
    	m_BlastDb.OidToPig(m_Oid, pig);
        m_Oid2Pig.first = m_Oid;
        m_Oid2Pig.second = pig;
    }
    return NStr::IntToString(m_Oid2Pig.second);
}

string CBlastDBExtractor::ExtractGi() {
    x_SetGi();
    return (m_Gi != ZERO_GI ? NStr::NumericToString(m_Gi) : NOT_AVAILABLE);
}

void CBlastDBExtractor::x_SetGi() {
    if (m_Gi != ZERO_GI) return;
    ITERATE(list<CRef<CSeq_id> >, itr, m_Bioseq->GetId()) {
        if ((*itr)->IsGi()) {
            m_Gi = (*itr)->GetGi();
            return;
        }
    }
    return;
}

void CBlastDBExtractor::x_InitDefline()
{
    if (m_Defline.NotEmpty()) {
        return;
    }
    if (m_Bioseq.NotEmpty()) {
        m_Defline = CSeqDB::ExtractBlastDefline(*m_Bioseq);
    }
    if (m_Defline.Empty()) {
        m_Defline = m_BlastDb.GetHdr(m_Oid);
    }
}

string CBlastDBExtractor::ExtractLinksInteger()
{
    x_InitDefline();
    string retval;

    ITERATE(CBlast_def_line_set::Tdata, itr, m_Defline->Get()) {
        const CRef<CSeq_id> seqid = FindBestChoice((*itr)->GetSeqid(),
                                                   CSeq_id::BestRank);
        _ASSERT(seqid.NotEmpty());
        if ((*itr)->IsSetLinks()) {
            if (seqid->IsGi()) {
                if (seqid->GetGi() == m_Gi) {
                    ITERATE(CBlast_def_line::TLinks, links_int, (*itr)->GetLinks()) {
                        retval += NStr::IntToString(*links_int) + SEPARATOR;
                    }
                    break;
                }
            } else {
                ITERATE(CBlast_def_line::TLinks, links_int, (*itr)->GetLinks()) {
                    retval += NStr::IntToString(*links_int) + SEPARATOR;
                }
            }
        }
    }
    if (retval.size()) {
        retval.erase(retval.size()-1, 1);   // remove the last separator
    }

    return (retval.empty() ? NOT_AVAILABLE : retval);
}

string CBlastDBExtractor::ExtractMembershipInteger()
{
    x_InitDefline();
    int retval = 0;

    if (m_Gi == ZERO_GI) {
        return NStr::IntToString(0);
    }

    ITERATE(CBlast_def_line_set::Tdata, itr, m_Defline->Get()) {
        const CRef<CSeq_id> seqid = FindBestChoice((*itr)->GetSeqid(),
                                                   CSeq_id::BestRank);
        _ASSERT(seqid.NotEmpty());

        if (seqid->IsGi() && (seqid->GetGi() == m_Gi) &&
            (*itr)->IsSetMemberships()) {
            ITERATE(CBlast_def_line::TMemberships, memb_int,
                    (*itr)->GetMemberships()) {
                retval += *memb_int;
            }
            break;
        }
    }

    return NStr::IntToString(retval);
}

string CBlastDBExtractor::ExtractAsn1Defline()
{
    x_InitDefline();
    CNcbiOstrstream oss;
    oss << MSerial_AsnText << *m_Defline << endl;
    return CNcbiOstrstreamToString(oss);
}

string CBlastDBExtractor::ExtractAsn1Bioseq()
{
    _ASSERT(m_Bioseq.NotEmpty());
    CNcbiOstrstream oss;
    oss << MSerial_AsnText << *m_Bioseq << endl;
    return CNcbiOstrstreamToString(oss);
}

void CBlastDBExtractor::x_SetGi2AccMap()
{
	if (m_Gi2AccMap.first == m_Oid)
		return;

    map<TGi, string> gi2acc;
    x_InitDefline();
    ITERATE(CBlast_def_line_set::Tdata, bd, m_Defline->Get()) {
        TGi gi(INVALID_GI);
        ITERATE(CBlast_def_line::TSeqid, id, ((*bd)->GetSeqid())) {
            if ((*id)->IsGi()) {
                gi = (*id)->GetGi();
                break;
            }
        }
        CRef<CSeq_id> theId = FindBestChoice((*bd)->GetSeqid(), CSeq_id::WorstRank);
        string acc;
        theId->GetLabel(&acc, CSeq_id::eContent);
        if (gi != INVALID_GI)
            gi2acc[gi] = acc;
    }
	m_Gi2AccMap.first = m_Oid;
	m_Gi2AccMap.second.swap(gi2acc);
	return;
}

void CBlastDBExtractor::x_SetGi2SeqIdMap()
{
	if (m_Gi2SeqIdMap.first == m_Oid)
		return;

	map<TGi, string> gi2id;
    x_InitDefline();
    ITERATE(CBlast_def_line_set::Tdata, bd, m_Defline->Get()) {
        TGi gi(INVALID_GI);
        ITERATE(CBlast_def_line::TSeqid, id, ((*bd)->GetSeqid())) {
            if ((*id)->IsGi()) {
                gi = (*id)->GetGi();
                break;
            }
        }
        CRef<CSeq_id> theId = FindBestChoice((*bd)->GetSeqid(), CSeq_id::WorstRank);
        if (gi != INVALID_GI) {
            if (m_UseLongSeqIds) {
                gi2id[gi] = theId->AsFastaString();
            }
            else {
                gi2id[gi] = GetBareId(*theId);
            }
        }
    }
	m_Gi2SeqIdMap.first = m_Oid;
	m_Gi2SeqIdMap.second.swap(gi2id);
	return;
}

void CBlastDBExtractor::x_SetGi2TitleMap()
{
	if (m_Gi2TitleMap.first == m_Oid)
		return;

	map<TGi, string> gi2title;
	x_InitDefline();
    ITERATE(CBlast_def_line_set::Tdata, bd, m_Defline->Get()) {
	    TGi gi(INVALID_GI);
        ITERATE(CBlast_def_line::TSeqid, id, ((*bd)->GetSeqid())) {
            if ((*id)->IsGi()) {
                gi = (*id)->GetGi();
                break;
            }
	    }
        if (gi != INVALID_GI) {
            gi2title[gi] = (*bd)->GetTitle();
        }
	}
	m_Gi2TitleMap.first = m_Oid;
	m_Gi2TitleMap.second.swap(gi2title);
	return;
}

string CBlastDBExtractor::ExtractAccession() {
    if (m_Gi != ZERO_GI)
    {
    	x_SetGi2AccMap();
    	return m_Gi2AccMap.second[m_Gi];
    }

    CRef<CSeq_id> theId = FindBestChoice(m_Bioseq->GetId(), CSeq_id::WorstRank);
    if (theId->IsGeneral() && theId->GetGeneral().GetDb() == "BL_ORD_ID") {
        return NOT_AVAILABLE;
    }
    string acc;
    theId->GetLabel(&acc, CSeq_id::eContent);
    return acc;
}

string CBlastDBExtractor::ExtractSeqId() {
    if (m_Gi != ZERO_GI)
    {
        x_SetGi2SeqIdMap();
        return m_Gi2SeqIdMap.second[m_Gi];
    }

    CRef<CSeq_id> theId = FindBestChoice(m_Bioseq->GetId(), CSeq_id::WorstRank);
    if (theId->IsGeneral() && theId->GetGeneral().GetDb() == "BL_ORD_ID") {
        	return NOT_AVAILABLE;
    }
    string retval;

    if (m_UseLongSeqIds) {
        retval = theId->AsFastaString();

        // Remove "lcl|" on local ID.
        if(theId->IsLocal())
            retval = retval.erase(0, 4);
    }
    else {
        retval = GetBareId(*theId);
    }

    return retval;
}

string CBlastDBExtractor::ExtractTitle() {
    if (m_Gi != ZERO_GI)
    {
        x_SetGi2TitleMap();
        return m_Gi2TitleMap.second[m_Gi];
    }

    ITERATE(list <CRef <CSeqdesc> >, itr, m_Bioseq->GetDescr().Get()) {
        if ((*itr)->IsTitle()) {
            return (*itr)->GetTitle();
        }
    }
    return NOT_AVAILABLE;
}

string CBlastDBExtractor::ExtractTaxId() {
    return NStr::IntToString(x_ExtractTaxId());
}

string CBlastDBExtractor::ExtractLeafTaxIds() {
    set<int> taxids;
    x_ExtractLeafTaxIds(taxids);
    if (taxids.empty()) {
        return ExtractTaxId();
    }
    set<int>::iterator taxids_iter = taxids.begin();
    string retval;
    ITERATE(set<int>, taxids_iter, taxids) {
        if (retval.empty()) {
            retval = NStr::IntToString(*taxids_iter);
        } else {
            retval += SEPARATOR + NStr::IntToString(*taxids_iter);
        }
    }
    return retval;
}

string CBlastDBExtractor::ExtractCommonTaxonomicName() {
    const int kTaxID = x_ExtractTaxId();
    SSeqDBTaxInfo tax_info;
    string retval(NOT_AVAILABLE);
    try {
        m_BlastDb.GetTaxInfo(kTaxID, tax_info);
        _ASSERT(kTaxID == tax_info.taxid);
        retval = tax_info.common_name;
    } catch (...) {}
    return retval;
}

string CBlastDBExtractor::ExtractLeafCommonTaxonomicNames() {
    set<int> taxids;
    x_ExtractLeafTaxIds(taxids);
    SSeqDBTaxInfo tax_info;
    string retval;
    ITERATE(set<int>, taxid_iter, taxids) {
        const int kTaxID = *taxid_iter;
        try {
            m_BlastDb.GetTaxInfo(kTaxID, tax_info);
            _ASSERT(kTaxID == tax_info.taxid);
            if (retval.empty()) {
                retval = tax_info.common_name;
            } else {
                retval += SEPARATOR + tax_info.common_name;
            }
        } catch (...) {}
    }
    if (retval.empty()) {
        return ExtractCommonTaxonomicName();
    } else {
        return retval;
    }
}

string CBlastDBExtractor::ExtractScientificName() {
    const int kTaxID = x_ExtractTaxId();
    SSeqDBTaxInfo tax_info;
    string retval(NOT_AVAILABLE);
    try {
        m_BlastDb.GetTaxInfo(kTaxID, tax_info);
        _ASSERT(kTaxID == tax_info.taxid);
        retval = tax_info.scientific_name;
    } catch (...) {}
    return retval;
}

string CBlastDBExtractor::ExtractLeafScientificNames() {
    set<int> taxids;
    x_ExtractLeafTaxIds(taxids);
    SSeqDBTaxInfo tax_info;
    string retval;
    ITERATE(set<int>, taxid_iter, taxids) {
        const int kTaxID = *taxid_iter;
        try {
            m_BlastDb.GetTaxInfo(kTaxID, tax_info);
            _ASSERT(kTaxID == tax_info.taxid);
            if (retval.empty()) {
                retval = tax_info.scientific_name;
            } else {
                retval += SEPARATOR + tax_info.scientific_name;
            }
        } catch (...) {}
    }
    if (retval.empty()) {
        return ExtractScientificName();
    } else {
        return retval;
    }
}

string CBlastDBExtractor::ExtractBlastName() {
    const int kTaxID = x_ExtractTaxId();
    SSeqDBTaxInfo tax_info;
    string retval(NOT_AVAILABLE);
    try {
        m_BlastDb.GetTaxInfo(kTaxID, tax_info);
        _ASSERT(kTaxID == tax_info.taxid);
        retval = tax_info.blast_name;
    } catch (...) {}
    return retval;
}

//string CBlastDBExtractor::ExtractBlastName() {
//    set<int> taxids;
//    x_ExtractTaxIds(taxids);
//    SSeqDBTaxInfo tax_info;
//    string retval;
//    ITERATE(set<int>, taxid_iter, taxids) {
//        const int kTaxID = *taxid_iter;
//        try {
//            m_BlastDb.GetTaxInfo(kTaxID, tax_info);
//            _ASSERT(kTaxID == tax_info.taxid);
//            if (retval.empty()) {
//                retval = tax_info.blast_name;
//            } else {
//                retval += SEPARATOR + tax_info.blast_name;
//            }
//        } catch (...) {}
//    }
//    if (retval.empty()) {
//        return string(NOT_AVAILABLE);
//    } else {
//        return retval;
//    }
//}

string CBlastDBExtractor::ExtractSuperKingdom() {
    const int kTaxID = x_ExtractTaxId();
    SSeqDBTaxInfo tax_info;
    string retval(NOT_AVAILABLE);
    try {
        m_BlastDb.GetTaxInfo(kTaxID, tax_info);
        _ASSERT(kTaxID == tax_info.taxid);
        retval = tax_info.s_kingdom;
    } catch (...) {}
    return retval;
}

//string CBlastDBExtractor::ExtractSuperKingdom() {
//    set<int> taxids;
//    x_ExtractTaxIds(taxids);
//    SSeqDBTaxInfo tax_info;
//    string retval;
//    ITERATE(set<int>, taxid_iter, taxids) {
//        const int kTaxID = *taxid_iter;
//        try {
//            m_BlastDb.GetTaxInfo(kTaxID, tax_info);
//            _ASSERT(kTaxID == tax_info.taxid);
//            if (retval.empty()) {
//                retval = tax_info.s_kingdom;
//            } else {
//                retval += SEPARATOR + tax_info.s_kingdom;
//            }
//        } catch (...) {}
//    }
//    if (retval.empty()) {
//        return string(NOT_AVAILABLE);
//    } else {
//        return retval;
//    }
//}

    static const string kNoMasksFound = "none";
string CBlastDBExtractor::ExtractMaskingData() {
#if ((defined(NCBI_COMPILER_WORKSHOP) && (NCBI_COMPILER_VERSION <= 550))  ||  \
     defined(NCBI_COMPILER_MIPSPRO))
    return kNoMasksFound;
#else
    CSeqDB::TSequenceRanges masked_ranges;
    x_ExtractMaskingData(masked_ranges, m_FmtAlgoId);
    if (masked_ranges.empty())  return kNoMasksFound;

    CNcbiOstrstream out;
    ITERATE(CSeqDB::TSequenceRanges, range, masked_ranges) {
        out << range->first << "-" << range->second << SEPARATOR;
    }
    return CNcbiOstrstreamToString(out);
#endif
}

string CBlastDBExtractor::ExtractSeqData() {
    string seq;
    try {
        m_BlastDb.GetSequenceAsString(m_Oid, seq, m_SeqRange);
        CSeqDB::TSequenceRanges masked_ranges;
        x_ExtractMaskingData(masked_ranges, m_FiltAlgoId);
        ITERATE(CSeqDB::TSequenceRanges, mask, masked_ranges) {
            transform(&seq[mask->first], &seq[mask->second],
                      &seq[mask->first], (int (*)(int))tolower);
        }
        if (m_Strand == eNa_strand_minus) {
            CSeqManip::ReverseComplement(seq, CSeqUtil::e_Iupacna,
                                         0, seq.size());
        }
    } catch (CSeqDBException& e) {
        //FIXME: change the enumeration when it's availble
        if (e.GetErrCode() == CSeqDBException::eArgErr ||
            e.GetErrCode() == CSeqDBException::eFileErr/*eOutOfRange*/) {
            NCBI_THROW(CInvalidDataException, eInvalidRange, e.GetMsg());
        }
        throw;
    }
    return seq;
}

string CBlastDBExtractor::ExtractSeqLen() {
    return NStr::IntToString(m_BlastDb.GetSeqLength(m_Oid));
}

string CBlastDBExtractor::ExtractHash() {
    string seq;
    m_BlastDb.GetSequenceAsString(m_Oid, seq);
    return NStr::IntToString(CBlastSeqUtil::GetSeqHash(seq.c_str(), seq.size()));
}

#define CTRL_A "\001"

static void s_ReplaceCtrlAsInTitle(CRef<CBioseq> bioseq)
{
    static const string kTarget(" >gi|");
    static const string kCtrlA = string(CTRL_A) + string("gi|");
    NON_CONST_ITERATE(CSeq_descr::Tdata, desc, bioseq->SetDescr().Set()) {
        if ((*desc)->Which() == CSeqdesc::e_Title) {
            NStr::ReplaceInPlace((*desc)->SetTitle(), kTarget, kCtrlA);
            break;
        }
    }
}

static string s_GetTitle(const CBioseq & bioseq)
{
    _ASSERT(bioseq.CanGetDescr());
    ITERATE(CSeq_descr::Tdata, desc, bioseq.GetDescr().Get()) {
        if ((*desc)->Which() == CSeqdesc::e_Title) {
            return (*desc)->GetTitle();
        }
    }
    return string();
}

/// Auxiliary function to format the defline for FASTA output format
static string
s_ConfigureDeflineTitle(const string& title, bool use_ctrl_a)
{
    static const string kStandardSeparator(" >");
    const string kSeparator(use_ctrl_a ? CTRL_A : kStandardSeparator);
    string retval;
    list<string> tokens;
    NStr::Split(title, kStandardSeparator, tokens, NStr::fSplit_ByPattern);
    int idx = 0;
    for (auto token : tokens) {
        if (idx++ == 0) {
            retval += token;
            continue;
        }
        SIZE_TYPE pos = token.find(' ');
        const string kPossibleId(token, 0, pos != NPOS ? pos : token.length());
        CBioseq::TId seqids;
        
        try { 
            CSeq_id::ParseIDs(seqids, kPossibleId, CSeq_id::fParse_PartialOK);
        } catch (const CException&) {} 

        if (!seqids.empty()) {
            retval += kSeparator;
            CRef<CSeq_id> id = FindBestChoice(seqids, CSeq_id::Score);
            retval += GetBareId(*id);
            if (pos != NPOS)
                retval += token.substr(pos, token.length() - pos);
        } else {
            retval += kStandardSeparator + token;
        }
    }
    return retval;
}

string CBlastDBExtractor::ExtractFasta(const CBlastDBSeqId &id) {
    stringstream out("");

    CFastaOstream fasta(out);
    fasta.SetWidth(m_LineWidth);
    fasta.SetAllFlags(CFastaOstream::fKeepGTSigns|CFastaOstream::fEnableGI);

    SetSeqId(id, true);

    if (m_UseCtrlA && m_UseLongSeqIds) {
        s_ReplaceCtrlAsInTitle(m_Bioseq);
    }

    CRef<CSeq_id> seqid = FindBestChoice(m_Bioseq->GetId(), CSeq_id::BestRank);

    // Handle the case when a sequence range is provided
    CRef<CSeq_loc> range;
    if (m_SeqRange.NotEmpty() || m_Strand != eNa_strand_other) {
        if (m_SeqRange.NotEmpty()) {
            range.Reset(new CSeq_loc(*seqid, m_SeqRange.GetFrom(),
                                     m_SeqRange.GetTo(), m_Strand));
            fasta.ResetFlag(CFastaOstream::fSuppressRange);
        } else {
            TSeqPos length = m_Bioseq->GetLength();
            range.Reset(new CSeq_loc(*seqid, 0, length-1, m_Strand));
            fasta.SetFlag(CFastaOstream::fSuppressRange);
        }
    }
    // Handle any requests for masked FASTA
    static const CFastaOstream::EMaskType kMaskType = CFastaOstream::eSoftMask;
    CSeqDB::TSequenceRanges masked_ranges;
    x_ExtractMaskingData(masked_ranges, m_FiltAlgoId);
    if (!masked_ranges.empty()) {
        CRef<CSeq_loc> masks(new CSeq_loc);
        ITERATE(CSeqDB::TSequenceRanges, itr, masked_ranges) {
            CRef<CSeq_loc> mask(new CSeq_loc(*seqid, itr->first, itr->second -1));
            masks->SetMix().Set().push_back(mask);
        }
        fasta.SetMask(kMaskType, masks);
    }

    try { 
        if (m_UseLongSeqIds) {
            if (seqid->IsLocal()) {
                string lcl_tmp = seqid->AsFastaString();
                lcl_tmp = lcl_tmp.erase(0, 4);
                out << ">" << lcl_tmp << " " << s_GetTitle(*m_Bioseq) << '\n';
                CScope scope(*CObjectManager::GetInstance());
                fasta.WriteSequence(scope.AddBioseq(*m_Bioseq), range); 
            }
            else  {
                fasta.Write(*m_Bioseq, range); 
            }
        }
        else {

            out << '>';
            CRef<CSeq_id> id = FindBestChoice(m_Bioseq->GetId(), CSeq_id::Score);
            out << GetBareId(*id);

            string title = s_GetTitle(*m_Bioseq.GetNonNullPointer());
            out << ' ' << s_ConfigureDeflineTitle(title, m_UseCtrlA);
            out << endl;

            CScope scope(*CObjectManager::GetInstance());
            fasta.WriteSequence(scope.AddBioseq(*m_Bioseq), range); 
        }
    }
    catch (const CObjmgrUtilException& e) {
        if (e.GetErrCode() == CObjmgrUtilException::eBadLocation) {
            NCBI_THROW(CInvalidDataException, eInvalidRange,
                       "Invalid sequence range");
        }
    }
    return out.str();
}

int CBlastDBExtractor::x_ExtractTaxId()
{
    x_SetGi();

    if (m_Gi != ZERO_GI) {
        if (m_Gi2TaxidMap.first != m_Oid)
        {
            m_Gi2TaxidMap.first = m_Oid;
            m_BlastDb.GetTaxIDs(m_Oid, m_Gi2TaxidMap.second);
        }
        return m_Gi2TaxidMap.second[m_Gi];
    }
    // for database without Gi:
    vector<int> taxid;
    m_BlastDb.GetTaxIDs(m_Oid, taxid);
    return taxid.size() ? taxid[0] : 0;
}

void CBlastDBExtractor::x_ExtractLeafTaxIds(set<int>& taxids)
{
    x_SetGi();

    if (m_Gi != ZERO_GI) {
        if (m_Gi2TaxidSetMap.first != m_Oid)
        {
            m_Gi2TaxidSetMap.first = m_Oid;
            m_BlastDb.GetLeafTaxIDs(m_Oid, m_Gi2TaxidSetMap.second);
        }
        taxids.clear();
        const set<int>& taxid_set = m_Gi2TaxidSetMap.second[m_Gi];
        taxids.insert(taxid_set.begin(), taxid_set.end());
        return;
    }
    // for database without Gi:
    vector<int> taxid;
    m_BlastDb.GetLeafTaxIDs(m_Oid, taxid);
    taxids.clear();
    taxids.insert(taxid.begin(), taxid.end());
}

void
CBlastDBExtractor::x_ExtractMaskingData(CSeqDB::TSequenceRanges &ranges,
                                        int algo_id)
{
    ranges.clear();
    if (algo_id != -1) {
        m_BlastDb.GetMaskData(m_Oid, algo_id, ranges);
    }
}

void CBlastDBExtractor::SetConfig(TSeqRange range, objects::ENa_strand strand,
							      int filt_algo_id)
{
	m_OrigSeqRange = range;
	m_Strand = strand;
	m_FiltAlgoId = filt_algo_id;
}

static bool s_MatchPDBId(const CSeq_id & target_id, const CSeq_id & defline_id)
{
	if(defline_id.IsPdb()) {
		if(target_id.GetPdb().IsSetChain_id()) {
			if(defline_id.GetPdb().IsSetChain_id()) {
				return ((target_id.GetPdb().GetChain() == defline_id.GetPdb().GetChain())  &&
				        PNocase().Equals(target_id.GetPdb().GetMol(), defline_id.GetPdb().GetMol()));
			}
		}
		else {
			return PNocase().Equals(target_id.GetPdb().GetMol(), defline_id.GetPdb().GetMol());
		}
	}
	return false;
}

void CBlastDeflineUtil::ExtractDataFromBlastDeflineSet(const CBlast_def_line_set & dl_set,
        											   vector<string> & results,
        											   BlastDeflineFields fields,
        											   string target_id,
        											   bool use_long_id)
{
	CSeq_id target_seq_id (target_id, CSeq_id::fParse_PartialOK | CSeq_id::fParse_Default);
	Int8 num_id = NStr::StringToNumeric<Int8>(target_id, NStr::fConvErr_NoThrow);
	bool can_be_gi = errno ? false: true;
	bool isPDBId = target_seq_id.IsPdb();
	ITERATE(CBlast_def_line_set::Tdata, itr, dl_set.Get()) {
		 ITERATE(CBlast_def_line::TSeqid, id, (*itr)->GetSeqid()) {
			 if ((*id)->Match(target_seq_id) || (can_be_gi && (*id)->IsGi() && ((*id)->GetGi() == num_id))
				 || (isPDBId && s_MatchPDBId(target_seq_id, **id))) {
				 CBlastDeflineUtil::ExtractDataFromBlastDefline( **itr, results, fields, use_long_id);
				 return;
			 }
		 }
	}

	NCBI_THROW(CException, eInvalid, "Failed to find target id " + target_id);
}

static string s_CheckName(const string & name)
{
        if(name == "-") return NOT_AVAILABLE;
        if(name == "unclassified") return NOT_AVAILABLE;

        return name;
}

void CBlastDeflineUtil::ExtractDataFromBlastDefline(const CBlast_def_line & dl,
				                            	    vector<string> & results,
				                                    BlastDeflineFields fields,
				                                    bool use_long_id)
{
	results.clear();
	results.resize(CBlastDeflineUtil::max_index, kEmptyStr);
	if (fields.gi == 1) {
         results[CBlastDeflineUtil::gi] = NOT_AVAILABLE;
		 ITERATE(CBlast_def_line::TSeqid, id, dl.GetSeqid()) {
			 if ((*id)->IsGi()) {
				 TGi gi = (*id)->GetGi();
				 results[CBlastDeflineUtil::gi] = NStr::NumericToString(gi);
				 break;
			 }
	     }
	}
	if ((fields.accession == 1) || (fields.seq_id == 1)) {
		 CRef<CSeq_id> theId = FindBestChoice(dl.GetSeqid(), CSeq_id::WorstRank);
		 if(fields.seq_id == 1) {
        	 results[CBlastDeflineUtil::seq_id] = theId->AsFastaString();
		 }
		 if(fields.accession == 1) {
			 results[CBlastDeflineUtil::accession] = GetBareId(*theId);
		 }
	}
	if(fields.title == 1) {
		if(dl.IsSetTitle()) {
        	 results[CBlastDeflineUtil::title] = dl.GetTitle();
		}
		else {
        	 results[CBlastDeflineUtil::title] = NOT_AVAILABLE;
		}
	}
	if ((fields.tax_id == 1) || (fields.tax_names == 1)) {
		unsigned int tax_id = 0;
		if (dl.IsSetTaxid()) {
			tax_id = dl.GetTaxid();
		}

		if (fields.tax_id == 1) {
			results[CBlastDeflineUtil::tax_id] = NStr::NumericToString(tax_id);
		}

		if (fields.tax_names == 1) {
			try {
				SSeqDBTaxInfo taxinfo;
			    CSeqDB::GetTaxInfo(tax_id, taxinfo);
			    results[CBlastDeflineUtil::scientific_name] = taxinfo.scientific_name;
			    results[CBlastDeflineUtil::common_name] = taxinfo.common_name;
			    results[CBlastDeflineUtil::blast_name] = s_CheckName(taxinfo.blast_name);
			    results[CBlastDeflineUtil::super_kingdom] = s_CheckName(taxinfo.s_kingdom);
	        } catch (const CException&) {
			    results[CBlastDeflineUtil::scientific_name] = NOT_AVAILABLE;
			    results[CBlastDeflineUtil::common_name] = NOT_AVAILABLE;
			    results[CBlastDeflineUtil::blast_name] = NOT_AVAILABLE;
			    results[CBlastDeflineUtil::super_kingdom] = NOT_AVAILABLE;
			}
		}
	}

	if ((fields.leaf_node_tax_ids == 1) || (fields.leaf_node_tax_names == 1)) {
		set<int>  tax_id_set = dl.GetLeafTaxIds();
		if (tax_id_set.empty()) {
			if (dl.IsSetTaxid()) {
				tax_id_set.insert(dl.GetTaxid());
			}
			else {
				tax_id_set.insert(0);
			}
		}

		string separator = kEmptyStr;
		ITERATE(set<int>, itr, tax_id_set) {
			if (fields.leaf_node_tax_names == 1) {
				try {
					SSeqDBTaxInfo taxinfo;
					CSeqDB::GetTaxInfo(*itr, taxinfo);
					results[CBlastDeflineUtil::leaf_node_scientific_names] += separator + taxinfo.scientific_name;
					results[CBlastDeflineUtil::leaf_node_common_names] += separator + taxinfo.common_name;
				} catch (const CException&) {
				    results[CBlastDeflineUtil::leaf_node_scientific_names] += separator + NOT_AVAILABLE;
				    results[CBlastDeflineUtil::leaf_node_common_names] += separator + NOT_AVAILABLE;
				}
			}
		    results[CBlastDeflineUtil::leaf_node_tax_ids] += separator + NStr::NumericToString(*itr);
			separator = SEPARATOR;
		}
	}

	if (fields.membership == 1) {
		int membership = 0;
		if(dl.IsSetMemberships()) {
			 ITERATE(CBlast_def_line::TMemberships, memb_int, dl.GetMemberships()) {
				 membership += *memb_int;
			 }
		}
		results[CBlastDeflineUtil::membership] = NStr::NumericToString(membership);
	}

	if (fields.pig == 1) {
		int pig = -1;
        if (dl.IsSetOther_info()) {
        	ITERATE(CBlast_def_line::TOther_info, itr, dl.GetOther_info()) {
        		if (*itr != -1) {
        			pig = *itr;
        			break;
		        }
		    }
		}
        results[CBlastDeflineUtil::pig] = NStr::NumericToString(pig);
	}
	if(fields.links == 1) {
		if (dl.IsSetLinks()) {
			ITERATE(CBlast_def_line::TLinks, links_int, dl.GetLinks()) {
				results[CBlastDeflineUtil::links] += NStr::IntToString(*links_int) + SEPARATOR;
			}
		}
		else {
			results[CBlastDeflineUtil::links] = NOT_AVAILABLE;
		}
	}

	if(fields.asn_defline == 1) {
		CNcbiOstrstream tmp;
		tmp << MSerial_AsnText << dl;
		results[CBlastDeflineUtil::asn_defline] = CNcbiOstrstreamToString(tmp);
	}
}

void CBlastDeflineUtil::ProcessFastaDeflines(CBioseq & bioseq, string & out, bool use_ctrla)
{
	out = kEmptyStr;
    const CSeq_id* id = bioseq.GetFirstId();
    if (!id) {
        return;
    }
     if (id->IsGeneral() &&
         id->GetGeneral().GetDb() == "BL_ORD_ID") {
         out = ">"  + s_GetTitle(bioseq) + '\n';
     }
     else if (id->IsLocal()) {
         string lcl_tmp = id->AsFastaString();
         lcl_tmp = lcl_tmp.erase(0,4);
         out = ">" + lcl_tmp + " " + s_GetTitle(bioseq) + '\n';
     }
     else {

        out = '>';
        id = FindBestChoice(bioseq.GetId(), CSeq_id::Score);
        out += GetBareId(*id) + ' ';

        string title = s_GetTitle(bioseq);
        out += s_ConfigureDeflineTitle(title, use_ctrla);
        out += '\n';
     }
}

// Calculates hash for a buffer in IUPACna (NCBIeaa for proteins) format.
// NOTE: if sequence is in a different format, the function below can be modified to convert
// each byte into IUPACna encoding on the fly.
Uint4 CBlastSeqUtil::GetSeqHash(const char* buffer, int length)
{
    CChecksum crc(CChecksum::eCRC32ZIP);

    for(int ii = 0; ii < length; ii++) {
        if (buffer[ii] != '\n')
            crc.AddChars(buffer+ii,1);
    }
    return (crc.GetChecksum() ^ (0xFFFFFFFFL));
}

void CBlastSeqUtil::ApplySeqMask(string & seq, const CSeqDB::TSequenceRanges & masks, const TSeqRange r)
{
	if(r.Empty()) {
		ITERATE(CSeqDB::TSequenceRanges, itr, masks) {
			transform(&seq[itr->first], &seq[itr->second],
			                      &seq[itr->first], (int (*)(int))tolower);
		}
	}
	else {
		const TSeqPos r_from = r.GetFrom();
		ITERATE(CSeqDB::TSequenceRanges, itr, masks) {
			TSeqRange mask (*itr);
			if(mask.GetFrom() > r.GetTo()) {
				break;
			}
			TSeqRange  tmp = r.IntersectionWith(mask);
			if(!tmp.Empty()) {
				transform(&seq[tmp.GetFrom() -r_from], &seq[tmp.GetToOpen() - r_from],
		                  &seq[tmp.GetFrom() -r_from], (int (*)(int))tolower);
			}
		}
	}
}

void CBlastSeqUtil::GetReverseStrandSeq(string & seq)
{
	CSeqManip::ReverseComplement(seq, CSeqUtil::e_Iupacna, 0, seq.size());
}

string CBlastSeqUtil::GetMasksString(const CSeqDB::TSequenceRanges & masks)
{
	if (masks.empty()) {
		return kNoMasksFound;
	}
	CNcbiOstrstream out;
	ITERATE(CSeqDB::TSequenceRanges, range, masks) {
		out << range->first << "-" << range->second << SEPARATOR;
	}
	return CNcbiOstrstreamToString(out);
}

END_NCBI_SCOPE
