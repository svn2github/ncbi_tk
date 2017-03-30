#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/object_manager.hpp>

#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seq/Seq_hist_rec.hpp>

#include <objtools/edit/protein_match/prot_match_exception.hpp>
#include <objtools/edit/protein_match/setup_match.hpp>
#include <serial/iterator.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CMatchSetup::CMatchSetup(CRef<CScope> db_scope) : m_DBScope(db_scope)
{
}


void CMatchSetup::GatherNucProtSets(
    CSeq_entry& input_entry,
    list<CRef<CSeq_entry>>& nuc_prot_sets) 
{
    if (!input_entry.IsSet()) {
        return;
    }

    for (CTypeIterator<CSeq_entry> it(input_entry); it; ++it) {
        if (it->IsSet() &&
            it->GetSet().IsSetClass() &&
            it->GetSet().GetClass() == CBioseq_set::eClass_nuc_prot) {
            nuc_prot_sets.push_back(Ref(&*it));
        }
    }

}


CBioseq& CMatchSetup::x_FetchNucSeqRef(CBioseq_set& nuc_prot_set) const 
{
    NON_CONST_ITERATE(CBioseq_set::TSeq_set, seq_it, nuc_prot_set.SetSeq_set()) {
        CSeq_entry& se = **seq_it;
        if (se.IsSeq() && se.GetSeq().IsNa()) {
            return se.SetSeq();
        }
    }

    NCBI_THROW(CProteinMatchException, 
        eInputError,
        "No nucleotide sequence found");
}


const CBioseq& CMatchSetup::x_FetchNucSeqRef(const CBioseq_set& nuc_prot_set) const 
{
    ITERATE(CBioseq_set::TSeq_set, seq_it, nuc_prot_set.GetSeq_set()) {
        const CSeq_entry& se = **seq_it;
        if (se.IsSeq() && se.GetSeq().IsNa()) {
            return se.GetSeq();
        }
    }

    NCBI_THROW(CProteinMatchException, 
        eInputError,
        "No nucleotide sequence found");
}


CBioseq& CMatchSetup::x_FetchNucSeqRef(CSeq_entry& nuc_prot_set) const
{
    return x_FetchNucSeqRef(nuc_prot_set.SetSet());
}


bool CMatchSetup::GetReplacedIdFromHist(const CBioseq& nuc_seq, CRef<CSeq_id>& id) const
{
    if (!nuc_seq.IsSetInst()) {
        return false;
    }

    const auto& seq_inst = nuc_seq.GetInst();
    if (!seq_inst.IsSetHist() ||
        !seq_inst.GetHist().IsSetReplaces() ||
        !seq_inst.GetHist().GetReplaces().IsSetIds()) {
        return false;
    }

    for (auto pReplacesId : seq_inst.GetHist().GetReplaces().GetIds()) {
        if (pReplacesId->IsGenbank() || pReplacesId->IsOther()) {
            id = pReplacesId;
            return true;
        }
    }

    return false;
}


bool CMatchSetup::GetNucSeqId(const CBioseq& nuc_seq, CRef<CSeq_id>& id) const
{
    CRef<CSeq_id> other_id;
    for (auto pNucId : nuc_seq.GetId()) {
        if (pNucId->IsGenbank()) {
            id = pNucId;
            return true;
        }

        if (pNucId->IsOther()) {
            other_id = pNucId;
        }
    }

    if (other_id && !other_id.IsNull()) {
        id = other_id;
        return true;
    }
    return false;
}


bool CMatchSetup::GetNucSeqId(const CBioseq_set& nuc_prot_set, CRef<CSeq_id>& id) const
{
    const CBioseq& nuc_seq = x_FetchNucSeqRef(nuc_prot_set);
    return GetNucSeqId(nuc_seq, id);
}


CConstRef<CBioseq_set> CMatchSetup::GetDBNucProtSet(const CBioseq& nuc_seq) 
{
    CBioseq_Handle db_bsh;
    for (auto pNucId : nuc_seq.GetId()) {
        if (pNucId->IsGenbank() || pNucId->IsOther()) {  // Look at GetBioseqHandle
            db_bsh = m_DBScope->GetBioseqHandle(*pNucId);
            if (db_bsh && 
                db_bsh.GetParentBioseq_set()) {
                return db_bsh.GetParentBioseq_set().GetCompleteBioseq_set();
            }
        }
    }

    CRef<CSeq_id> pReplacedId;
    const bool use_replaced_id = GetReplacedIdFromHist(nuc_seq, pReplacedId);

    if (use_replaced_id) {
        db_bsh = m_DBScope->GetBioseqHandle(*pReplacedId);
        if (db_bsh && 
            db_bsh.GetParentBioseq_set()) {
            return db_bsh.GetParentBioseq_set().GetCompleteBioseq_set();
        }
    }


    NCBI_THROW(CProteinMatchException,
               eInputError,
               "Failed to find a valid database id");

    return CConstRef<CBioseq_set>();
}


struct SIdCompare
{
    bool operator()(const CRef<CSeq_id>& id1,
        const CRef<CSeq_id>& id2) const 
    {
        return id1->CompareOrdered(*id2) < 0;
    }
};


static bool s_InList(const CSeq_id& id, const CBioseq::TId& id_list)
{
    for (auto pId : id_list) {
        if (id.Match(*pId)) {
            return true;
        }
    }
    return false;
}

bool CMatchSetup::GetNucSeqIdFromCDSs(const CSeq_entry& nuc_prot_set,
    CRef<CSeq_id>& id) const
{
    // Set containing distinct ids
    set<CRef<CSeq_id>, SIdCompare> ids;

    for (CTypeConstIterator<CSeq_feat> feat(nuc_prot_set); feat; ++feat) 
    {
        if (!feat->GetData().IsCdregion()) {
            continue;
        }

        CRef<CSeq_id> nucseq_id = Ref(new CSeq_id());
        const CSeq_id* id_ptr = feat->GetLocation().GetId();
        if (!id_ptr) {
            NCBI_THROW(CProteinMatchException,
                eBadInput,
                "Invalid CDS location");
        }
        nucseq_id->Assign(*id_ptr);
        ids.insert(nucseq_id);
    }

    if (ids.empty()) {
        NCBI_THROW(CProteinMatchException,
            eBadInput,
            "CDS locations not specified");
    }


    // Check that the ids point to the nucleotide sequence
    const CBioseq& nuc_seq = nuc_prot_set.GetSet().GetNucFromNucProtSet();

    for ( auto pId : ids) {
        if (!s_InList(*pId, nuc_seq.GetId())) {
            NCBI_THROW(CProteinMatchException,
                eBadInput,
                "Unrecognized CDS location id");
        }
    }

    const bool with_version = true;
    const string local_id_string = nuc_seq.GetFirstId()->GetSeqIdString(with_version);

    if (id.IsNull()) {
        id = Ref(new CSeq_id());
    }
    id->SetLocal().SetStr(local_id_string);

    return true;
}


bool CMatchSetup::UpdateNucSeqIds(CRef<CSeq_id> new_id,
    CSeq_entry& nuc_prot_set) const
{
    if (!nuc_prot_set.IsSet()) {
        return false;
    }

    CBioseq& nuc_seq = x_FetchNucSeqRef(nuc_prot_set);
    nuc_seq.ResetId();
    nuc_seq.SetId().push_back(new_id);

    for (CTypeIterator<CSeq_feat> feat(nuc_prot_set); feat; ++feat) {
        if (feat->GetData().IsCdregion()) {
            feat->SetLocation().SetId(*new_id);
        }
    }

    return true;   
}


END_SCOPE(objects)
END_NCBI_SCOPE
