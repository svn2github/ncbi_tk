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
* Authors:  Kamen Todorov
*
* File Description:
*   Alignment builders
*
* ===========================================================================
*/


#include <ncbi_pch.hpp>

#include <objtools/alnmgr/aln_builders.hpp>
#include <objtools/alnmgr/aln_rng_coll_oper.hpp>
#include <objtools/alnmgr/aln_serial.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


void
MergePairwiseAlns(CPairwiseAln& existing,
                  const CPairwiseAln& addition,
                  const CAlnUserOptions::TMergeFlags& flags)
{
    CPairwiseAln difference(existing.GetFirstId(),
                            existing.GetSecondId(),
                            existing.GetPolicyFlags());
    SubtractAlnRngCollections(addition, // minuend
                              existing, // subtrahend
                              difference);
#ifdef _TRACE_MergeAlnRngColl
    cerr << endl;
    cerr << "existing:" << endl << existing << endl;
    cerr << "addition:" << endl << addition << endl;
    cerr << "difference = addition - existing:" << endl << difference << endl;
#endif
    ITERATE(CPairwiseAln, rng_it, difference) {
        existing.insert(*rng_it);
    }
#ifdef _TRACE_MergeAlnRngColl
    cerr << "result = existing + difference:" << endl << existing << endl;
#endif
}


class CMergedPairwiseAln : public CObject
{
public:
    typedef CAnchoredAln::TPairwiseAlnVector TPairwiseAlnVector;

    CMergedPairwiseAln(const CAlnUserOptions::TMergeFlags& merge_flags)
        : m_MergeFlags(merge_flags),
          m_NumberOfDirectAlns(0)
    {
    };

    void insert(const CRef<CPairwiseAln>& pairwise) {
        CRef<CPairwiseAln> addition(pairwise);

        if (m_MergeFlags & CAlnUserOptions::fTruncateOverlaps) {
            x_TruncateOverlaps(addition);
        }
        x_AddPairwise(*addition);
    }

    const TPairwiseAlnVector& GetPairwiseAlns() const {
        return m_PairwiseAlns;
    };

    const CAlnUserOptions::TMergeFlags& GetMergedFlags() const {
        return m_MergeFlags;
    }

private:
    void x_TruncateOverlaps(CRef<CPairwiseAln>& addition)
    {
        /// Truncate, if requested
        ITERATE(TPairwiseAlnVector, aln_it, m_PairwiseAlns) {
            const CPairwiseAln& existing = **aln_it;
            CRef<CPairwiseAln> truncated
                (new CPairwiseAln(addition->GetFirstId(),
                                  addition->GetSecondId(),
                                  addition->GetPolicyFlags()));
            SubtractAlnRngCollections(*addition,  // minuend
                                      existing,   // subtrahend
                                      *truncated);// difference
#ifdef _TRACE_MergeAlnRngColl
            cerr << endl;
            cerr << "existing:" << endl << existing << endl;
            cerr << "addition:" << endl << *addition << endl;
            cerr << "truncated = addition - existing:" << endl << *truncated << endl;
#endif
            addition = truncated;
        }
    }


    bool x_ValidNeighboursOnFirstDim(const CPairwiseAln::TAlnRng& left,
                                     const CPairwiseAln::TAlnRng& right) {
        if (left.GetFirstToOpen() > right.GetFirstFrom()) {
            // Overlap on first dimension
            return false;
        }
        return true;
    }

    bool x_ValidNeighboursOnSecondDim(const CPairwiseAln::TAlnRng& left,
                                      const CPairwiseAln::TAlnRng& right) {
        _ASSERT(left.IsDirect()  ==  right.IsDirect());
        if (left.GetSecondToOpen() > right.GetSecondFrom()) {
            if (m_MergeFlags & CAlnUserOptions::fAllowTranslocation) {
                if (left.GetSecondFrom() < right.GetSecondToOpen()) {
                    // Overlap on second dimension
                    return false;
                }
                // Allowed translocation
            } else {
                // Either overlap or 
                // unallowed translocation (unsorted on second dimension)
                return false;
            }
        }
        return true;
    }


    bool x_CanInsertRng(CPairwiseAln& a, const CPairwiseAln::TAlnRng& r) {
        PAlignRangeFromLess<CPairwiseAln::TAlnRng> p;
        CPairwiseAln::const_iterator it = lower_bound(a.begin(), a.end(), r.GetFirstFrom(), p);

        if (it != a.begin())   { // Check left
            const CPairwiseAln::TAlnRng& left = *(it - 1);
            if ( !x_ValidNeighboursOnFirstDim(left, r)  ||
                 !x_ValidNeighboursOnSecondDim(r.IsDirect() ? left : r,
                                               r.IsDirect() ? r : left) ) {
                return false;
            }
        }
        if (it != a.end()) { // Check right
            const CPairwiseAln::TAlnRng& right = *it;
            if ( !x_ValidNeighboursOnFirstDim(r, right)  ||
                 !x_ValidNeighboursOnSecondDim(r.IsDirect() ? r : right,
                                               r.IsDirect() ? right : r) ) {
                return false;
            }
        }
        return true;
    }


    void x_AddPairwise(const CPairwiseAln& addition) {
        TPairwiseAlnVector::iterator aln_it, aln_end;
        ITERATE(CPairwiseAln, rng_it, addition) {

            // What alignments can we possibly insert it to?
            if (m_MergeFlags & CAlnUserOptions::fAllowMixedStrand) {
                aln_it = m_PairwiseAlns.begin();
                aln_end = m_PairwiseAlns.end();
            } else {
                if (rng_it->IsDirect()) {
                    aln_it = m_PairwiseAlns.begin();
                    aln_end = aln_it + m_NumberOfDirectAlns;
                } else {
                    aln_it = m_PairwiseAlns.begin() + m_NumberOfDirectAlns;
                    aln_end = m_PairwiseAlns.end();
                }
            }

            // Which alignment do we insert it to?
            while (aln_it != aln_end) {
#ifdef _TRACE_MergeAlnRngColl
                cerr << endl;
                cerr << *rng_it << endl;
                cerr << **aln_it << endl;
                cerr << "m_MergeFlags: " << m_MergeFlags << endl;
#endif
                _ASSERT(m_MergeFlags & CAlnUserOptions::fAllowMixedStrand  ||
                        rng_it->IsDirect() && (*aln_it)->IsSet(CPairwiseAln::fDirect)  ||
                        rng_it->IsReversed() && (*aln_it)->IsSet(CPairwiseAln::fReversed));
                if (x_CanInsertRng(**aln_it, *rng_it)) {
                    break;
                }
                ++aln_it;
            }
            if (aln_it == aln_end) {
                CRef<CPairwiseAln> new_aln
                    (new CPairwiseAln(addition.GetFirstId(),
                                      addition.GetSecondId(),
                                      addition.GetPolicyFlags()));
                /* adjust policy flags here? */

                aln_it = m_PairwiseAlns.insert(aln_it, new_aln);

                if (rng_it->IsDirect()  &&
                    !(m_MergeFlags & CAlnUserOptions::fAllowMixedStrand)) {
                    ++m_NumberOfDirectAlns;
                }
            }
            (*aln_it)->insert(*rng_it);
#ifdef _TRACE_MergeAlnRngColl
            cerr << *rng_it;
            cerr << **aln_it;
#endif
            _ASSERT(m_MergeFlags & CAlnUserOptions::fAllowMixedStrand  ||
                    rng_it->IsDirect() && (*aln_it)->IsSet(CPairwiseAln::fDirect)  ||
                    rng_it->IsReversed() && (*aln_it)->IsSet(CPairwiseAln::fReversed));
        }
    }

    const CAlnUserOptions::TMergeFlags m_MergeFlags;
    TPairwiseAlnVector m_PairwiseAlns;
    size_t m_NumberOfDirectAlns;
};



ostream& operator<<(ostream& out, const CMergedPairwiseAln& merged)
{
    out << "MergedPairwiseAln contains: " << endl;
    out << "  TMergeFlags: " << merged.GetMergedFlags() << endl;
    ITERATE(CMergedPairwiseAln::TPairwiseAlnVector, aln_it, merged.GetPairwiseAlns()) {
        out << **aln_it;
    };
    return out;
}


typedef vector<CRef<CMergedPairwiseAln> > TMergedVec;

void BuildAln(const TMergedVec& merged_vec,
              CAnchoredAln& out_aln)
{
    typedef CAnchoredAln::TDim TDim;

    // Determine the size
    size_t total_number_of_rows = 0;
    ITERATE(TMergedVec, merged_i, merged_vec) {
        total_number_of_rows += (*merged_i)->GetPairwiseAlns().size();
    }

    // Resize the container
    out_aln.SetPairwiseAlns().resize(total_number_of_rows);
    

    // Copy pairwises
    TDim row = 0;
    ITERATE(TMergedVec, merged_i, merged_vec) {
        ITERATE(CAnchoredAln::TPairwiseAlnVector, pairwise_i, (*merged_i)->GetPairwiseAlns()) {
            out_aln.SetPairwiseAlns()[row] = *pairwise_i;
            ++row;
        }
    }
}


void 
SortAnchoredAlnVecByScore(TAnchoredAlnVec& anchored_aln_vec)
{
    sort(anchored_aln_vec.begin(),
         anchored_aln_vec.end(), 
         PScoreGreater<CAnchoredAln>());
}


void 
BuildAln(TAnchoredAlnVec& in_alns,
         CAnchoredAln& out_aln,
         const CAlnUserOptions& options)
{
    // Types
    typedef CAnchoredAln::TDim TDim;
    typedef CAnchoredAln::TPairwiseAlnVector TPairwiseAlnVector;

    /// 1. Build a single anchored_aln
    _ASSERT(out_aln.GetDim() == 0);
    switch (options.m_MergeAlgo) {
    case CAlnUserOptions::eQuerySeqMergeOnly:
        ITERATE(TAnchoredAlnVec, anchored_it, in_alns) {
            const CAnchoredAln& anchored = **anchored_it;
            if (anchored_it == in_alns.begin()) {
                out_aln = anchored;
                continue;
            }
            // assumption is that anchor row is the last
            _ASSERT(anchored.GetAnchorRow() == anchored.GetDim()-1);
            for (TDim row = 0; row < anchored.GetDim(); ++row) {
                if (row == anchored.GetAnchorRow()) {
                    MergePairwiseAlns(*out_aln.SetPairwiseAlns().back(),
                                      *anchored.GetPairwiseAlns()[row],
                                      CAlnUserOptions::fTruncateOverlaps);
                } else {
                    // swap the anchor row with the new one
                    CRef<CPairwiseAln> anchor_pairwise(out_aln.GetPairwiseAlns().back());
                    out_aln.SetPairwiseAlns().back().Reset
                        (new CPairwiseAln(*anchored.GetPairwiseAlns()[row]));
                    out_aln.SetPairwiseAlns().push_back(anchor_pairwise);
                }
            }
        }
        break;
    case CAlnUserOptions::ePreserveRows:
        if ( !in_alns.empty() ) {
            if ( ! options.m_MergeFlags & CAlnUserOptions::fSkipSortByScore ) {
                SortAnchoredAlnVecByScore(in_alns);
            }
            TMergedVec merged_vec;
            const CAnchoredAln& first_anchored = *in_alns.front();
            merged_vec.resize(first_anchored.GetDim());
            ITERATE(TAnchoredAlnVec, anchored_it, in_alns) {
                const CAnchoredAln& anchored = **anchored_it;
                _ASSERT(anchored.GetDim() == first_anchored.GetDim());
                if (anchored.GetDim() != first_anchored.GetDim()) {
                    string errstr = "All input alignments need to have "
                        "the same dimension when using ePreserveRows.";
                    NCBI_THROW(CAlnException, eInvalidRequest, errstr);
                }
                _ASSERT(anchored.GetAnchorRow() == first_anchored.GetAnchorRow());
                if (anchored.GetAnchorRow() != first_anchored.GetAnchorRow()) {
                    string errstr = "All input alignments need to have "
                        "the same anchor row when using ePreserveRows.";
                    NCBI_THROW(CAlnException, eInvalidRequest, errstr);
                }
                for (TDim row = 0; row < anchored.GetDim(); ++row) {
                    CRef<CMergedPairwiseAln>& merged = merged_vec[row];
                    if (merged.Empty()) {
                        merged.Reset
                            (new CMergedPairwiseAln(row == anchored.GetAnchorRow() ?
                                                    CAlnUserOptions::fTruncateOverlaps :
                                                    options.m_MergeFlags));
                    }
                    merged->insert(anchored.GetPairwiseAlns()[row]);
                }
            }
            BuildAln(merged_vec, out_aln);
        }
        break;
    case CAlnUserOptions::eMergeAllSeqs:
    default: 
        {
            if ( ! options.m_MergeFlags & CAlnUserOptions::fSkipSortByScore ) {
                SortAnchoredAlnVecByScore(in_alns);
            }
            typedef map<TAlnSeqIdIRef, CRef<CMergedPairwiseAln>, SAlnSeqIdIRefComp> TIdMergedMap;
            TIdMergedMap id_merged_map;
            TMergedVec merged_vec;

            CRef<CMergedPairwiseAln> merged_anchor;
#ifdef _TRACE_MergeAlnRngColl
                static int aln_idx;
#endif
            ITERATE(TAnchoredAlnVec, anchored_it, in_alns) {
                for (TDim row = 0; row < (*anchored_it)->GetDim(); ++row) {

                    CRef<CMergedPairwiseAln>& merged = id_merged_map[(*anchored_it)->GetId(row)];
                    if (merged.Empty()) {
                        // first time we are dealing with this id.
                        merged.Reset
                            (new CMergedPairwiseAln(row == (*anchored_it)->GetAnchorRow() ?
                                                    CAlnUserOptions::fTruncateOverlaps :
                                                    options.m_MergeFlags));
                        if (row == (*anchored_it)->GetAnchorRow()) {
                            // if anchor
                            if (anchored_it == in_alns.begin()) {
                                merged_anchor.Reset(merged);
                            }
                        } else {
                            // else add to the out vectors
                            merged_vec.push_back(merged);
                        }

                    }
#ifdef _TRACE_MergeAlnRngColl
                    cerr << endl;
                    cerr << "anchor row " << (*anchored_it)->GetAnchorRow() << endl;
                    cerr << *merged << endl;
                    cerr << "inserting aln " << aln_idx << ", row " << row << endl;
                    cerr << *(*anchored_it)->GetPairwiseAlns()[row] << endl;
#endif
                    merged->insert((*anchored_it)->GetPairwiseAlns()[row]);
                }
#ifdef _TRACE_MergeAlnRngColl
                ++aln_idx;
#endif
            }
            // finally, add the anchor
            merged_vec.push_back(merged_anchor);
            
            BuildAln(merged_vec, out_aln);
        }
        break;
    }
    out_aln.SetAnchorRow(out_aln.GetPairwiseAlns().size() - 1);


    /// 2. Sort the ids and alns according to score, how to collect score?
}


END_NCBI_SCOPE
