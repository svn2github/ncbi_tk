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
 * Authors:  Christiam Camacho
 *
 */

/// @file disc_nucl_options.cpp
/// Implements the CDiscNucleotideOptionsHandle class.

#include <algo/blast/api/disc_nucl_options.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include "blast_setup.hpp"

/** @addtogroup AlgoBlast
 *
 * @{
 */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

CDiscNucleotideOptionsHandle::CDiscNucleotideOptionsHandle(EAPILocality locality)
    : CBlastNucleotideOptionsHandle(locality)
{
    if (m_Opts->GetLocality() == CBlastOptions::eRemote) {
        return;
    }
    SetDefaults();
    m_Opts->SetProgram(eBlastn);
}

void 
CDiscNucleotideOptionsHandle::SetMBLookupTableDefaults()
{
    CBlastNucleotideOptionsHandle::SetMBLookupTableDefaults();
    SetTemplateType(0);
    SetTemplateLength(21);
    SetWordSize(BLAST_WORDSIZE_NUCL);
    SetScanStep(4);
}

void 
CDiscNucleotideOptionsHandle::SetMBInitialWordOptionsDefaults()
{
    SetXDropoff(BLAST_UNGAPPED_X_DROPOFF_NUCL);
    SetWindowSize(BLAST_WINDOW_SIZE_DISC);
    SetSeedContainerType(eDiagArray);
    SetVariableWordSize(BLAST_VARWORD_NUCL);
    SetSeedExtensionMethod(eRight);
    SetUngappedExtension();
}

void
CDiscNucleotideOptionsHandle::SetMBGappedExtensionDefaults()
{
    SetGapXDropoff(BLAST_GAP_X_DROPOFF_NUCL);
    SetGapXDropoffFinal(BLAST_GAP_X_DROPOFF_FINAL_NUCL);
    SetGapTrigger(BLAST_GAP_TRIGGER_NUCL);
    SetGapExtnAlgorithm(EXTEND_DYN_PROG);
}

void
CDiscNucleotideOptionsHandle::SetMBScoringOptionsDefaults()
{
    CBlastNucleotideOptionsHandle::SetScoringOptionsDefaults();
}

void
CDiscNucleotideOptionsHandle::SetTraditionalBlastnDefaults()
{
    NCBI_THROW(CBlastException, eNotSupported, 
   "Blastn uses a seed extension method incompatible with discontiguous nuclotide blast");
}

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2004/03/22 20:14:22  dondosha
 * Make dynamic programming gapped extension default for discontiguous megablast
 *
 * Revision 1.5  2004/03/19 15:13:34  camacho
 * Move to doxygen group AlgoBlast
 *
 * Revision 1.4  2004/03/17 21:48:32  dondosha
 * Added custom SetMBInitialWordOptionsDefaults and SetMBGappedExtensionDefaults methods
 *
 * Revision 1.3  2004/02/03 18:36:10  dondosha
 * Reset the stride to 4 in SetMBLookupTableDefaults
 *
 * Revision 1.2  2004/01/16 21:54:52  bealer
 * - Blast4 API changes.
 *
 * Revision 1.1  2003/11/26 18:24:01  camacho
 * +Blast Option Handle classes
 *
 * ===========================================================================
 */
