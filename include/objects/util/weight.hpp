#ifndef WEIGHT__HPP
#define WEIGHT__HPP

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
* Author:  Aaron Ucko
*
* File Description:
*   Molecular weights for protein sequences
*/

#include <corelib/ncbistd.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CBioseq_Handle;
class CSeq_loc;
class CSeqVector;


class CBadResidueException : public runtime_error
{
public:
    CBadResidueException(const string& s) : runtime_error(s) { }
};

// Handles the standard 20 amino acids and Sec; treats Asx as Asp and
// Glx as Glu; throws CBadResidueException on anything else.
double GetProteinWeight(CBioseq_Handle& handle, const CSeq_loc* location = 0)
    THROWS((CBadResidueException));


typedef map<CConstRef<CSeq_loc>, double> TWeights;

// Automatically picks reasonable ranges: in decreasing priority order,
// - Annotated cleavage products (mature peptides)
// - What's left after removing the first signal peptide found
// - The entire sequence (skipping a leading methionine if present)
void GetProteinWeights(CBioseq_Handle& handle, TWeights& weights);

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.5  2002/06/07 18:19:59  ucko
* Reworked to take advantage of CBioseq_Handle::GetSequenceView.
*
* Revision 1.4  2002/05/06 16:11:55  ucko
* Update for new OM; move CVS log to end.
*
* Revision 1.3  2002/05/03 21:28:05  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.2  2002/04/19 17:50:03  ucko
* Add forward declaration for CBioseqHandle.
*
* Revision 1.1  2002/03/06 22:08:39  ucko
* Add code to calculate protein weights.
* ===========================================================================
*/

#endif  /* WEIGHT__HPP */
