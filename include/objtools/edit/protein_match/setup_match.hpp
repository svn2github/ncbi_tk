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
 *  and reliability of the software and data,  the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties,  express or implied,  including
 *  warranties of performance,  merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors:  Justin Foley
 */

#ifndef _SETUP_MATCH_H_
#define _SETUP_MATCH_H_

#include <corelib/ncbistd.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objmgr/scope.hpp>
#include <objects/seqset/Seq_entry.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CMatchSetup {

public:
    CMatchSetup(CRef<CScope> db_scope);
    virtual ~CMatchSetup(void) = default;

    static void GatherNucProtSets(
        CSeq_entry& input_entry,
        list<CRef<CSeq_entry>>& nuc_prot_sets);

    CConstRef<CBioseq_set> GetDBNucProtSet(const CBioseq& nuc_seq);

    bool UpdateNucSeqIds(CRef<CSeq_id> new_id,
        CSeq_entry& nuc_prot_set) const;

    bool GetNucSeqIdFromCDSs(const CSeq_entry& nuc_prot_set,
        CRef<CSeq_id>& id) const;

    bool GetNucSeqId(const CBioseq& nuc_seq, CRef<CSeq_id>& id) const;

    bool GetNucSeqId(const CBioseq_set& nuc_prot_set, CRef<CSeq_id>& id) const;

    bool GetReplacedIdFromHist(const CBioseq& nuc_seq, CRef<CSeq_id>& id) const;
private:
    CBioseq& x_FetchNucSeqRef(CSeq_entry& nuc_prot_set) const;
    CBioseq& x_FetchNucSeqRef(CBioseq_set& nuc_prot_set) const;
    const CBioseq& x_FetchNucSeqRef(const CBioseq_set& nuc_prot_set) const;

    CRef<CScope> m_DBScope;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif
