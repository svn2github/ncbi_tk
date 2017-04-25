
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
 * Authors:  Justin Foley
 *
 * File Description:  Write alignment
 *
 */

#include <ncbi_pch.hpp>

#include <objects/seqalign/Seq_align.hpp>

#include <objmgr/scope.hpp>

#include <objtools/writers/writer_exception.hpp>
#include <objtools/writers/write_util.hpp>
#include <objtools/writers/aln_writer.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
bool CAlnWriter::WriteAlign(
    const CSeq_align& align,
    const string& name,
    const string& descr) 
{
    return true;
}
//  ----------------------------------------------------------------------------
END_NCBI_SCOPE

