/* $Id$
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
 * Author:  .......
 *
 * File Description:
 *   .......
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using specifications from the ASN data definition file
 *   'seqloc.asn'.
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.4  2001/01/03 18:59:09  vasilche
 * Added missing include.
 *
 * Revision 6.3  2001/01/03 16:39:05  vasilche
 * Added CAbstractObjectManager - stub for object manager.
 * CRange extracted to separate file.
 *
 * Revision 6.2  2000/12/26 17:28:55  vasilche
 * Simplified and formatted code.
 *
 * Revision 6.1  2000/11/17 21:35:10  vasilche
 * Added GetLength() method to CSeq_loc class.
 *
 *
 * ===========================================================================
 */

// standard includes

// generated includes
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Packed_seqint.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CSeq_loc::~CSeq_loc(void)
{
}

// returns length, in residues, of Seq_loc
// return = -1 = couldn't calculate due to error
// return = -2 = couldn't calculate because of data type
int CSeq_loc::GetLength(void) const
{
	switch (Which()) {
    case e_not_set:      /* this should never be */
        return eError;
    case e_Bond:         /* can't calculate length */
    case e_Feat:
        return eUndefined;
    case e_Pnt:
        return 1;
    case e_Int:
        return GetInt().GetLength();
    case e_Empty:
    case e_Whole:
        return GetWhole().Resolve()->GetInst().GetLength();
    case e_Packed_int:
        return GetPacked_int().GetLength();
    case e_Mix:
        return GetMix().GetLength();
    case e_Packed_pnt:
        return eUndefined;  /* this is a bunch of points */
    case e_Equiv:
        return eUndefined;  /* unless they are all equal */
    default:
        break;
    }
	return eError;   /* new unsupported type */
}

// returns enclosing location range
CSeq_loc::TRange CSeq_loc::GetTotalRange(void) const
{
	switch (Which()) {
    case e_not_set:      /* this should never be */
        THROW1_TRACE(runtime_error,
                     "CSeq_loc::GetTotalRange(): unset CSeq_loc");
    case e_Bond:         /* can't calculate length */
    case e_Feat:
        break; // undefined
    case e_Pnt:
        {
            int point = GetPnt().GetPoint();
            return TRange(point, point);
        }
    case e_Int:
        {
            const CSeq_interval& interval = GetInt();
            return TRange(interval.GetFrom(), interval.GetTo());
        }
    case e_Empty:
        return TRange::GetEmpty();
    case e_Whole:
        return TRange::GetWhole();
    case e_Packed_int:
        break; // undefined
    case e_Mix:
        return GetMix().GetTotalRange();
    case e_Packed_pnt:
        break; // undefined
    case e_Equiv:
        break; // undefined
    default:
        break;
    }
    THROW1_TRACE(runtime_error,
                 "CSeq_loc::GetTotalRange(): undefined range");
}

END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

