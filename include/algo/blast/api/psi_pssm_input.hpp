#ifndef ALGO_BLAST_API__PSI_PSSM_INPUT__HPP
#define ALGO_BLAST_API__PSI_PSSM_INPUT__HPP

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
 * Author:  Christiam Camacho
 *
 */

/** @file psi_pssm_input.hpp
 * Defines a concrete strategy to obtain PSSM input data for PSI-BLAST.
 */

#include <corelib/ncbiobj.hpp>
#include <algo/blast/api/pssm_input.hpp>

// Forward declaration for unit test classes
class CPssmEngineTest;

BEGIN_NCBI_SCOPE

// Forward declarations in objects scope
BEGIN_SCOPE(objects)
    class CSeq_align_set;
    class CDense_seg;
    class CScope;
END_SCOPE(objects)

BEGIN_SCOPE(blast)

/// This class is a concrete strategy for IPssmInputData, and it
/// implements the traditional PSI-BLAST algorithm for building a multiple
/// sequence alignment from a list of pairwise alignments.
class CPsiBlastInputData : public IPssmInputData
{
public:
    /// Construct a concrete strategy, used to configure the CPssmEngine object
    /// @param query query sequence for the alignment.
    /// @param query_length length of the sequence above.
    /// @param sset pairwise alignment produced by BLAST where query was the
    /// query sequence.
    /// @param opts options to be used in the PSSM engine
    CPsiBlastInputData(const Uint1* query,
                       unsigned int query_length,
                       CRef<objects::CSeq_align_set> sset,
                       CRef<objects::CScope> scope,
                       const PSIBlastOptions& opts);
    virtual ~CPsiBlastInputData();

    void Process();
    PsiAlignmentData* GetData();
    const PSIBlastOptions* GetOptions();

protected:
    /// Populates the multiple alignment data structure
    void x_ExtractAlignmentData();
    // First implementation of use_best_align option from old toolkit. Should
    // be implemented as a subclass of this one?
    //void x_ExtractAlignmentDataUseBestAlign();

    unsigned int GetNumAlignedSequences() const;
    unsigned int GetQueryLength() const;

    /// Iterates over the Dense-seg passed in and extracts alignment 
    /// information to multiple alignment data structure.
    /// @param denseg source alignment segment (HSP) [in]
    /// @param msa_index index of the sequence aligned with the query in the
    ///        desc_matrix field of the m_AlignmentData data member [in]
    /// @param e_value E-value for the HSP corresponding to denseg [in]
    void x_ProcessDenseg(const objects::CDense_seg& denseg, 
                         unsigned int msa_index, double e_value);

private:

    /// Pointer to query sequence
    Uint1*                          m_Query;
    /// Scope where to retrieve the sequences in the aligment from
    CRef<objects::CScope>           m_Scope;
    // Structure representing the multiple sequence alignment
    PsiAlignmentData*               m_AlignmentData;
    /// Multiple sequence alignment dimensions
    PsiInfo                         m_MSA_Dimensions;
    /// Pairwise alignment result of a BLAST search
    CRef<objects::CSeq_align_set>   m_SeqAlignSet;
    /// Algorithm options
    PSIBlastOptions                 m_Opts;
    /// Helps to keep track of the hits to process because they meet the
    /// inclusion e-value threshold criteria
    std::vector<Uint1>              m_ProcessHit;

    /////////////////////////// Auxiliary functions ///////////////////////////

    /// Tries to fetch the sequence data for the subject for the segments 
    /// specified in the Dense-seg. If the sequence cannot be retrieved from the 
    /// scope, a warning is printed and the pair (NULL, 0) is returned.
    typedef pair<AutoPtr<Uint1, ArrayDeleter<Uint1> >, TSeqPos> TSeqPair;
    static TSeqPair 
    x_GetSubjectSequence(const objects::CDense_seg& ds, objects::CScope& scope);

    /// Examines the sequence alignment and keeps track of those hits which
    /// have an HSP with an e-value below the inclusion threshold specified in
    /// the PSIBlastOptions structure.
    /// @return number of hits which qualify for constructing the multiple
    /// sequence alignment structure
    unsigned int
    x_CountAndSelectQualifyingAlignments();

    friend class ::CPssmEngineTest;     // unit test class
};

END_SCOPE(blast)
END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.1  2004/07/29 17:53:47  camacho
 * Initial revision
 *
 *
 * ===========================================================================
 */
#endif  /* ALGO_BLAST_API__PSI_PSSM_INPUT_HPP */
