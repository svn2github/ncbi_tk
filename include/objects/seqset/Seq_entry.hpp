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

class CSeq_loc;
class CSeq_descr;
class CSeq_annot;

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

    // convenience functions to get descriptor chain from underlying Bioseq or Bioseq-set
    bool IsSetDescr(void) const;
    const CSeq_descr& GetDescr(void) const;

    // convenience functions to get annot list from underlying Bioseq or Bioseq-set
    typedef list< CRef< CSeq_annot > > TAnnot;
    bool IsSetAnnot(void) const;
    const TAnnot& GetAnnot(void) const;

    enum ELabelType {
        eType,
        eContent,
        eBoth
    };

    // Append a label to label based on type or content of CSeq_entry
    void GetLabel(string* label, ELabelType type) const;

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

#endif // OBJECTS_SEQSET_SEQ_ENTRY_HPP
/* Original file checksum: lines: 85, chars: 2245, CRC32: 986b11b7 */
