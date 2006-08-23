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

/// @MSMod.hpp
/// User-defined methods of the data storage class.
///
/// This file was originally generated by application DATATOOL
/// using the following specifications:
/// 'omssa.asn'.
///
/// New methods or data members can be added to it if needed.
/// See also: MSMod_.hpp


#ifndef OBJECTS_OMSSA_MSMOD_HPP
#define OBJECTS_OMSSA_MSMOD_HPP


// generated includes
#include <objects/omssa/MSMod_.hpp>
#include <objects/omssa/MSModType_.hpp>
// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// list out the amino acids using NCBIstdAA
const int kNumUniqueAA = 28;
// U is selenocystine
// this is NCBIstdAA.  Doesn't seem to be a central location to get this.
// all blast protein databases are encoded with this.
// U is selenocystine
const char * const UniqueAA = "-ABCDEFGHIKLMNPQRSTVWXYZU*JO";
//                             0123456789012345678901234567
//                             0123456789abcdef0123456789ab

// used to scale all float masses into ints
#define MSSCALE 1000
#define MSSCALE2INT(x) (static_cast <int> ((x) * MSSCALE + 0.5))
#define MSSCALE2DBL(x) ((x)/ static_cast <double> (MSSCALE))



END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.5  2006/08/23 21:33:04  lewisg
* rearrange file parsing
*
* Revision 1.4  2006/05/25 17:11:56  lewisg
* one filtered spectrum per precursor charge state
*
* Revision 1.3  2005/10/24 21:46:13  lewisg
* exact mass, peptide size limits, validation, code cleanup
*
* Revision 1.2  2005/03/14 22:29:54  lewisg
* add mod file input
*
* Revision 1.1  2004/12/07 23:38:22  lewisg
* add modification handling code
*
*
* ===========================================================================
*/

#endif // OBJECTS_OMSSA_MSMOD_HPP
/* Original file checksum: lines: 65, chars: 1980, CRC32: a05bf8ae */
