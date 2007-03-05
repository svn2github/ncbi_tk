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
 * Author:  Eugene Vasilchenko
 *
 * File Description:
 *   .......
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using specifications from the ASN data definition file
 *   'seqloc.asn'.
 */

#include <ncbi_pch.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>


BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

CSeq_loc_mix::CSeq_loc_mix(void)
{
    return;
}


CSeq_loc_mix::~CSeq_loc_mix(void)
{
}

bool CSeq_loc_mix::IsPartialStart(ESeqLocExtremes ext) const
{
    if (!Get().empty()) {
        return (IsReverseStrand()  &&  ext == eExtreme_Positional) ?
            Get().back()->IsPartialStart(ext) : Get().front()->IsPartialStart(ext);
    }
    return false;
}

bool CSeq_loc_mix::IsPartialStop(ESeqLocExtremes ext) const
{
    if (!Get().empty()) {
        return (IsReverseStrand()  &&  ext == eExtreme_Positional) ?
            Get().front()->IsPartialStop(ext) : Get().back()->IsPartialStop(ext);
    }
    return false;
}


void CSeq_loc_mix::SetPartialStart(bool val, ESeqLocExtremes ext)
{
    if (IsPartialStart(ext) == val  ||  Set().empty()) {
        return;
    }
    if ((IsReverseStrand()  &&  ext == eExtreme_Positional)) {
        Set().back()->SetPartialStart(val, ext);
    }else {
        Set().front()->SetPartialStart(val, ext);
    }
}


void CSeq_loc_mix::SetPartialStop(bool val, ESeqLocExtremes ext)
{
    if (IsPartialStop(ext) == val  ||  Set().empty()) {
        return;
    }
    if (IsReverseStrand()  &&  ext == eExtreme_Positional) {
        Set().front()->SetPartialStop(val, ext);
    } else {
        Set().back()->SetPartialStop(val, ext);
    }
}


bool CSeq_loc_mix::IsTruncatedStart(ESeqLocExtremes ext) const
{
    if (!Get().empty()) {
        return (IsReverseStrand()  &&  ext == eExtreme_Positional) ?
            Get().back()->IsTruncatedStart(ext) :
            Get().front()->IsTruncatedStart(ext);
    }
    return false;
}

bool CSeq_loc_mix::IsTruncatedStop(ESeqLocExtremes ext) const
{
    if (!Get().empty()) {
        return (IsReverseStrand()  &&  ext == eExtreme_Positional) ?
            Get().front()->IsTruncatedStop(ext) :
            Get().back()->IsTruncatedStop(ext);
    }
    return false;
}


void CSeq_loc_mix::SetTruncatedStart(bool val, ESeqLocExtremes ext)
{
    if (IsTruncatedStart(ext) == val  ||  Set().empty()) {
        return;
    }
    if ((IsReverseStrand()  &&  ext == eExtreme_Positional)) {
        Set().back()->SetTruncatedStart(val, ext);
    }else {
        Set().front()->SetTruncatedStart(val, ext);
    }
}


void CSeq_loc_mix::SetTruncatedStop(bool val, ESeqLocExtremes ext)
{
    if (IsTruncatedStop(ext) == val  ||  Set().empty()) {
        return;
    }
    if (IsReverseStrand()  &&  ext == eExtreme_Positional) {
        Set().front()->SetTruncatedStop(val, ext);
    } else {
        Set().back()->SetTruncatedStop(val, ext);
    }
}


ENa_strand CSeq_loc_mix::GetStrand(void) const
{
    ENa_strand strand = eNa_strand_unknown;
    bool strand_set = false;
    const CSeq_id* id = NULL;
    ITERATE(Tdata, it, Get()) {
        if ((*it)->IsNull()  ||  (*it)->IsEmpty()) {
            continue;
        }

        // check fro multiple IDs
        const CSeq_id* iid = (*it)->GetId();
        if (iid == NULL) {
            return eNa_strand_other;
        }
        if (id == NULL) {
            id = iid;
        } else {
            if (id->Compare(*iid) != CSeq_id::e_YES) {
                return eNa_strand_other;
            }
        }

        ENa_strand istrand = (*it)->GetStrand();
        if (strand == eNa_strand_unknown  &&  istrand == eNa_strand_plus) {
            strand = istrand;
            strand_set = true;
        } else if (strand == eNa_strand_plus  &&  istrand == eNa_strand_unknown) {
        } else if (!strand_set) {
            strand = istrand;
            strand_set = true;
        } else if (istrand != strand) {
            return eNa_strand_other;
        }
    }
    return strand;
}


TSeqPos CSeq_loc_mix::GetStart(ESeqLocExtremes ext) const
{
    if (!Get().empty()) {
        return (IsReverseStrand()  &&  ext == eExtreme_Positional) ?
            Get().back()->GetStart(ext) : Get().front()->GetStart(ext);
    }
    return kInvalidSeqPos;
}


TSeqPos CSeq_loc_mix::GetStop(ESeqLocExtremes ext) const
{
    if (!Get().empty()) {
        return (IsReverseStrand()  &&  ext == eExtreme_Positional) ?
            Get().front()->GetStop(ext) : Get().back()->GetStop(ext);
    }
    return kInvalidSeqPos;
}


void CSeq_loc_mix::AddSeqLoc(const CSeq_loc& other)
{
    if ( !other.IsMix() ) {
        CRef<CSeq_loc> loc(new CSeq_loc);
        loc->Assign(other);
        Set().push_back(loc);
    } else {
        // "flatten" the other seq-loc
        ITERATE(CSeq_loc_mix::Tdata, li, other.GetMix().Get()) {
            AddSeqLoc(**li);
        }
    }
}


void CSeq_loc_mix::AddSeqLoc(CSeq_loc& other)
{
    if ( !other.IsMix() ) {
        CRef<CSeq_loc> loc(&other);
        Set().push_back(loc);
    } else {
        // "flatten" the other seq-loc
        NON_CONST_ITERATE(CSeq_loc_mix::Tdata, li, other.SetMix().Set()) {
            AddSeqLoc(**li);
        }
    }
}


void CSeq_loc_mix::AddInterval(const CSeq_id& id, TSeqPos from, TSeqPos to,
                               ENa_strand strand)
{
    CRef<CSeq_loc> loc(new CSeq_loc);
    if (from == to) {
        CSeq_point& pnt = loc->SetPnt();
        pnt.SetPoint(from);
        pnt.SetId().Assign(id);
        if (strand != eNa_strand_unknown) {
            pnt.SetStrand(strand);
        }
    } else {
        CSeq_interval& ival = loc->SetInt();
        ival.SetFrom(from);
        ival.SetTo(to);
        ival.SetId().Assign(id);
        if (strand != eNa_strand_unknown) {
            ival.SetStrand(strand);
        }
    }
    Set().push_back(loc);
}


void CSeq_loc_mix::SetStrand(ENa_strand strand)
{
    NON_CONST_ITERATE (Tdata, it, Set()) {
        (*it)->SetStrand(strand);
    }
}


void CSeq_loc_mix::ResetStrand()
{
    NON_CONST_ITERATE (Tdata, it, Set()) {
        (*it)->ResetStrand();
    }
}


void CSeq_loc_mix::FlipStrand(void)
{
    NON_CONST_ITERATE (Tdata, it, Set()) {
        (*it)->FlipStrand();
    }
}

END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE
