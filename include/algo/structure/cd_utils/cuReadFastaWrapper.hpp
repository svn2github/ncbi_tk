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
* Authors:  Chris Lanczycki
*
* File Description:
*           An interface to using various possible mechanisms
*           for reading MFasta-formatted input, with concrete implementations.
*
* ===========================================================================
*/

#ifndef CU_FASTA_IO_WRAPPER__HPP
#define CU_FASTA_IO_WRAPPER__HPP

#include <corelib/ncbistd.hpp>
#include <objtools/readers/fasta.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

// class for standalone application
class CFastaIOWrapper
{

public:

    CFastaIOWrapper(bool cacheRawString = false) {
        m_cacheRawFasta = cacheRawString;
        m_rawFastaString = "";
        m_activeFastaString = "";
        m_seqEntry.Reset();
    }

    virtual ~CFastaIOWrapper() {};

    virtual bool ReadFile(CNcbiIstream& iStream) = 0;
    virtual bool Write(CNcbiOstream& ostream) {return false;};

    //  Subclasses have option to fill in a CSeq_entry.
    bool HasSeqEntry() const {return m_seqEntry.NotEmpty();}
    const CRef<CSeq_entry>& GetSeqEntry() const {return m_seqEntry;}

    virtual bool ParseString(const string& fastaString) {
        if (m_cacheRawFasta) m_rawFastaString = fastaString;
        m_activeFastaString = fastaString;
        return m_cacheRawFasta;
    }

    //  Allows users to manipulate the active fasta string.
    virtual string& GetActiveFastaString() {
        return m_activeFastaString;
    }

    //  If m_cacheRawFasta is false, this returns an empty string.
    const string& GetRawFastaString() {
        return m_rawFastaString;
    }
    
    void ClearActiveFasta() {m_activeFastaString.erase();}

    //  Acts as 'ClearActiveFasta' if m_cacheRawFasta is false.
    void ResetActiveFasta() {
        if (m_cacheRawFasta) 
            m_activeFastaString = m_rawFastaString;
        else 
            ClearActiveFasta();
    }

    string GetError() {return m_error;}

protected:

    bool m_cacheRawFasta;
    string m_rawFastaString;
    string m_activeFastaString;

    string m_error;

    CRef< CSeq_entry > m_seqEntry;

};


/// Concrete implementations of the CFastaIOWrapper virtual class


//  Uses the 'ReadFasta' C++-Toolkit function as a reader.
//  The CSeq_entry has a CBioseq_set unless there's only one bioseq.
class CBasicFastaWrapper : public CFastaIOWrapper {

public:

    CBasicFastaWrapper(TReadFastaFlags fastaFlags, bool cacheRawFasta) : CFastaIOWrapper(cacheRawFasta) {
        m_readFastaFlags = fastaFlags;
        m_seqEntry.Reset();
    }

    virtual ~CBasicFastaWrapper() {};

    bool ReadAsSeqEntry(CNcbiIstream& iStream, CRef< CSeq_entry >& seqEntry);

    //  Use 'ReadFasta' with the flags currently set.
    //  Saves the Fasta from the file as 'm_activeFastaString'.  If m_cacheRawFasta is
    //  true then it is also saved as 'm_rawFastaString' which will not be manipulated.
    virtual bool ReadFile(CNcbiIstream& iStream);

    //  Use default implementation for these virtuals...
//    virtual bool Write(CNcbiOstream& ostream, string& err) {};
//    virtual bool ParseString(const string& fastaString, string& err);
//    virtual string GetFastaString(bool processedOrRaw = true);

    void SetFastaFlags(TReadFastaFlags flagsToSet) {
        m_readFastaFlags |= flagsToSet;
    }
    void UnsetFastaFlags(TReadFastaFlags flagsToUnset) {
        m_readFastaFlags &= (0xFFFFFFFF^flagsToUnset);
    }

private:

    TReadFastaFlags m_readFastaFlags;
};


END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif // CU_FASTA_IO_WRAPPER__HPP

/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.1  2006/03/29 15:35:59  lanczyck
 * add files for fasta->cd converter
 *
 * ---------------------------------------------------------------------------
 */
