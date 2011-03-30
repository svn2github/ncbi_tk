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
 */

#ifndef OBJECTS_SEQLOC_PDB_SEQ_ID_HPP
#define OBJECTS_SEQLOC_PDB_SEQ_ID_HPP


// generated includes
#include <objects/seqloc/PDB_seq_id_.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class NCBI_SEQLOC_EXPORT CPDB_seq_id : public CPDB_seq_id_Base
{
    typedef CPDB_seq_id_Base Tparent;
public:
    // constructor
    CPDB_seq_id(void);
    // destructor
    ~CPDB_seq_id(void);

    // comaprison function
    bool Match(const CPDB_seq_id& psip2) const;
    int Compare(const CPDB_seq_id& psip2) const;

    // format a FASTA style string
    ostream& AsFastaString(ostream& s) const;
private:
    // Prohibit copy constructor & assignment operator
    CPDB_seq_id(const CPDB_seq_id&);
    CPDB_seq_id& operator= (const CPDB_seq_id&);
};



/////////////////// CPDB_seq_id inline methods

// constructor
inline
CPDB_seq_id::CPDB_seq_id(void)
{
}


/////////////////// end of CPDB_seq_id inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // OBJECTS_SEQLOC_PDB_SEQ_ID_HPP
/* Original file checksum: lines: 85, chars: 2258, CRC32: 8edce8b4 */
