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
 *   'seqset.asn'.
 */

#ifndef OBJECTS_SEQSET_SEQ_ENTRY_HPP
#define OBJECTS_SEQSET_SEQ_ENTRY_HPP


// generated includes
#include <objects/seqset/Seq_entry_.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class NCBI_SEQSET_EXPORT CSeq_entry : public CSeq_entry_Base, public CSerialUserOp
{
    typedef CSeq_entry_Base Tparent;
public:
    // constructor
    CSeq_entry(void);
    // destructor
    ~CSeq_entry(void);

    // Manage Seq-entry tree structure
    // recursive update of parent Seq-entries (will not change parent of this)
    void Parentize(void);
    // non-recursive update of direct childrent parents (will not change parent of this)
    void ParentizeOneLevel(void);
    // reset parent entry to NULL
    void ResetParentEntry(void);

    // get parent of this.
    // NULL means that either this is top level Seq-entry,
    // or Parentize() was never called.
    CSeq_entry* GetParentEntry(void) const;

protected:
    // From CSerialUserOp
    virtual void UserOp_Assign(const CSerialUserOp& source);
    virtual bool UserOp_Equals(const CSerialUserOp& object) const;
private:
    // Prohibit copy constructor and assignment operator
    CSeq_entry(const CSeq_entry& value);
    CSeq_entry& operator= (const CSeq_entry& value);

    // Upper-level Seq-entry
    void SetParentEntry(CSeq_entry* entry);
    CSeq_entry* m_ParentEntry;
};


enum EReadFastaFlags {
    fReadFasta_AssumeNuc  = 0x1,  // type to use if no revealing accn found
    fReadFasta_AssumeProt = 0x2,
    fReadFasta_ForceType  = 0x4,  // force type regardless of accession
    fReadFasta_NoParseID  = 0x8,  // treat name as local ID regardless of |s
    fReadFasta_ParseGaps  = 0x10, // make a delta sequence if gaps found
    fReadFasta_OneSeq     = 0x20, // just read the first sequence found
    fReadFasta_AllSeqIds  = 0x40, // read Seq-ids past the first ^A (see note)
    fReadFasta_NoSeqData  = 0x80  // parse the deflines but skip the data
};
typedef int TReadFastaFlags; // binary OR of EReadFastaFlags

// Note on fReadFasta_AllSeqIds: some databases (notably nr) have
// merged identical sequences, stringing their deflines together with
// control-As.  Normally, the reader stops at the first control-A;
// however, this flag makes it parse all the IDs.

// keeps going until EOF or parse error (-> CParseException) unless
// fReadFasta_OneSeq is set
// see also CFastaOstream in <objects/util/sequence.hpp> (-lxobjutil)
NCBI_SEQ_EXPORT
CRef<CSeq_entry> ReadFasta(CNcbiIstream& in, TReadFastaFlags flags = 0,
                           CSeq_loc* lowercase = 0);



//////////////////////////////////////////////////////////////////
//
// Class - description of multi-entry FASTA file,
// to keep list of offsets on all molecules in the file.
//
struct SFastaFileMap
{
    struct SFastaEntry
    {
        string  seq_id;        // Sequence Id
        string  description;   // Molecule description
        size_t  stream_offset; // Molecule offset in file
    };

    typedef vector<SFastaEntry>  TMapVector;

    TMapVector   file_map; // vector keeps list of all molecule entries
};

// Function reads input stream (assumed that it is FASTA format) one
// molecule entry after another filling the map structure describing and
// pointing on molecule entries. Fasta map can be used later for quick
// CSeq_entry retrival
void NCBI_SEQSET_EXPORT ReadFastaFileMap(SFastaFileMap* fasta_map, 
                                         CNcbiIfstream& input);



/////////////////// CSeq_entry inline methods

// constructor
inline
CSeq_entry::CSeq_entry(void)
    : m_ParentEntry(0)
{
}

inline
CSeq_entry* CSeq_entry::GetParentEntry(void) const
{
    return m_ParentEntry;
}

/////////////////// end of CSeq_entry inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.15  2003/05/23 20:28:11  ucko
 * Give ReadFasta an optional argument for reporting lowercase
 * characters' location.
 *
 * Revision 1.14  2003/05/16 13:31:20  kuznets
 * Fixed comments, added _dllexport.
 *
 * Revision 1.13  2003/05/15 18:50:15  kuznets
 * Implemented ReadFastaFileMap function. Function reads multientry FASTA
 * file filling SFastaFileMap structure(seq_id, sequence offset, description)
 *
 * Revision 1.12  2003/05/09 16:08:06  ucko
 * Rename fReadFasta_Redund to fReadFasta_AllSeqIds.
 *
 * Revision 1.11  2003/05/09 15:47:11  ucko
 * +fReadFasta_{Redund,NoSeqData} (suggested by Michel Dumontier)
 *
 * Revision 1.10  2003/04/24 16:14:12  vasilche
 * Fixed Parentize().
 *
 * Revision 1.9  2002/12/26 12:44:06  dicuccio
 * Added Win32 export specifiers
 *
 * Revision 1.8  2002/10/29 22:08:55  ucko
 * +fReadFasta_OneSeq
 *
 * Revision 1.7  2002/10/23 19:23:08  ucko
 * Move the FASTA reader from objects/util/sequence.?pp to
 * objects/seqset/Seq_entry.?pp because it doesn't need the OM.
 * Move the CVS log to the end of the file per current practice.
 *
 * Revision 1.6  2002/05/22 14:03:36  grichenk
 * CSerialUserOp -- added prefix UserOp_ to Assign() and Equals()
 *
 * Revision 1.5  2001/07/25 19:11:11  grichenk
 * Equals() and Assign() re-declared as protected
 *
 * Revision 1.4  2001/07/16 16:22:45  grichenk
 * Added CSerialUserOp class to create Assign() and Equals() methods for
 * user-defind classes.
 * Added SerialAssign<>() and SerialEquals<>() functions.
 *
 * Revision 1.3  2001/06/25 18:52:03  grichenk
 * Prohibited copy constructor and assignment operator
 *
 * Revision 1.2  2001/06/21 19:47:37  grichenk
 * Copy constructor and operator=() moved to "private" section
 *
 * Revision 1.1  2001/06/13 14:51:18  grichenk
 * Initial revision - Seq-entry tree structure support
 *
 * ===========================================================================
 */

#endif // OBJECTS_SEQSET_SEQ_ENTRY_HPP
/* Original file checksum: lines: 85, chars: 2245, CRC32: 986b11b7 */
