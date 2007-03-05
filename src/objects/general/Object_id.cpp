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
 *   'general.asn'.
 */

// standard includes

// generated includes
#include <ncbi_pch.hpp>
#include <objects/general/Object_id.hpp>
#include <corelib/ncbistd.hpp>

// generated classes

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::


// destructor
CObject_id::~CObject_id(void)
{
    return;
}


// match for identity
bool CObject_id::Match(const CObject_id& oid2) const
{
    E_Choice type = Which();

    if ( type != oid2.Which() )
        return false;

    switch ( type ) {
    case e_Id:
        return GetId() == oid2.GetId();
    case e_Str:
        return PNocase().Equals(GetStr(), oid2.GetStr());
    default:
        return this == &oid2;
    }
}


// match for identity
int CObject_id::Compare(const CObject_id& oid2) const
{
    E_Choice type = Which();
    E_Choice type2 = oid2.Which();
    if ( type != type2 )
        return type - type2;
    switch ( type ) {
    case e_Id:
        return GetId() - oid2.GetId();
    case e_Str:
        return PNocase().Compare(GetStr(), oid2.GetStr());
    default:
        return 0;
    }
}


// format contents into a stream
ostream& CObject_id::AsString(ostream &s) const
{
    switch ( Which() ) {
    case e_Id:
        s << GetId();
        break;
    case e_Str:
        s << GetStr(); // no Upcase() on output as per Ostell 7/2001 - Karl
        break;
    default:
        break;
    }
    return s;
}


END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE
