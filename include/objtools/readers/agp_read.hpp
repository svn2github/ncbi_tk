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
 * Authors: Josh Cherry
 *
 * File Description:  Read agp file
 */


#ifndef OBJTOOLS_READERS___AGP_READ__HPP
#define OBJTOOLS_READERS___AGP_READ__HPP

#include <corelib/ncbistd.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

BEGIN_NCBI_SCOPE

enum EAgpRead_IdRule
{
    eAgpRead_ParseId,         // Try to parse, but make local if this fails
    eAgpRead_ForceLocalId     // Always make a local id
};

/// Read an agp file from a stream, constructing delta sequences
NCBI_XOBJREAD_EXPORT
void AgpRead(CNcbiIstream& is,
             vector<CRef<objects::CBioseq> >& bioseqs,
             EAgpRead_IdRule component_id_rule = eAgpRead_ParseId);

/// Same thing, but wrap bioseqs in Seq-entry's.
NCBI_XOBJREAD_EXPORT
void AgpRead(CNcbiIstream& is,
             vector<CRef<objects::CSeq_entry> >& entries,
             EAgpRead_IdRule component_id_rule = eAgpRead_ParseId);

/// Return a Bioseq-set containing everything.
NCBI_XOBJREAD_EXPORT
CRef<objects::CBioseq_set>
AgpRead(CNcbiIstream& is, EAgpRead_IdRule component_id_rule = eAgpRead_ParseId);

END_NCBI_SCOPE

#endif  // OBJTOOLS_READERS___AGP_READ__HPP

/*
 * =========================================================================
 * $Log$
 * Revision 1.3  2005/01/26 20:58:48  jcherry
 * More robust and controllable handling of component ids
 *
 * Revision 1.2  2004/08/19 13:07:53  dicuccio
 * Repositioned export specifier to precede function declaration's return value
 *
 * Revision 1.1  2003/12/08 15:49:33  jcherry
 * Initial version
 *
 * =========================================================================
 */
