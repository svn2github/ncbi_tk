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
 */

/// @MSSearchType.hpp
/// User-defined methods of the data storage class.
///
/// This file was originally generated by application DATATOOL
/// using the following specifications:
/// 'omssa.asn'.
///
/// New methods or data members can be added to it if needed.
/// See also: MSSearchType_.hpp


#ifndef OBJECTS_OMSSA_MSSEARCHTYPE_HPP
#define OBJECTS_OMSSA_MSSEARCHTYPE_HPP


// generated includes
#include <objects/omssa/MSSearchType_.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

///
/// the names of the various modifications codified in the asn.1
///
char const * const kSearchType[eMSSearchType_max] = {
    "monoisotopic",
    "average",
    "monoisotopic N15"
};	   

END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.3  2005/01/31 17:30:57  lewisg
* adjustable intensity, z dpendence of precursor mass tolerance
*
* Revision 1.1  2004/06/08 19:46:21  lewisg
* input validation, additional user settable parameters
*
*
* ===========================================================================
*/

#endif // OBJECTS_OMSSA_MSSEARCHTYPE_HPP
/* Original file checksum: lines: 65, chars: 2022, CRC32: c2e493be */
