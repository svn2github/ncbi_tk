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
 * Author: Christiam Camacho
 *
 */

/** @file local_db_adapter.hpp
 * Declares class which provides internal BLAST database representations to the
 * internal BLAST APIs
 */

#ifndef ALGO_BLAST_API___LOCAL_DB_ADAPTER_HPP
#define ALGO_BLAST_API___LOCAL_DB_ADAPTER_HPP

#include <algo/blast/core/blast_seqsrc.h>
#include <algo/blast/api/uniform_search.hpp> // for CSearchDatabase
#include <objtools/readers/seqdb/seqdb.hpp>  // for CSeqDB

/** @addtogroup AlgoBlast
 *
 * @{
 */

// Forward declaration
struct BlastSeqSrc;

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

// Forward declaration
class IBlastSeqInfoSrc;

/// Interface to create a BlastSeqSrc suitable for use in CORE BLAST from a
/// a variety of BLAST database/subject representations
class NCBI_XBLAST_EXPORT CLocalDbAdapter : public CObject
{
public:
    /// Constructor
    /// @param dbinfo 
    ///     Description of BLAST database
    CLocalDbAdapter(const CSearchDatabase& dbinfo);
    
    /// Constructor
    /// @param seqdb 
    ///     CSeqDB object for an already opened BLAST database
    CLocalDbAdapter(CRef<CSeqDB> seqdb);
    
    /// Constructor
    /// @param subject_sequences
    ///     Set of sequences which should be used as subjects
    /// @param opts_handle
    ///     Options to be used (needed to create the ILocalQueryData)
    CLocalDbAdapter(CRef<IQueryFactory> subject_sequences,
                    CConstRef<CBlastOptionsHandle> opts_handle);

    /// Destructor
    virtual ~CLocalDbAdapter();

    /// This method should be called so that if the implementation has an
    /// internal "bookmark" of the chunks of the database it has assigned to
    /// different threads, this can be reset at the start of a PSI-BLAST
    /// iteration (or when reusing the same object to iterate over the 
    /// database/subjects when the query is split).
    /// This method should be called before calling MakeSeqSrc() to
    /// ensure proper action on retrieving the BlastSeqSrc (in some cases it
    /// might cause a re-construction of the underlying BlastSeqSrc
    /// implementation). Note that in some cases, this operation might not apply
    void ResetBlastSeqSrcIteration();

    /// Retrieves or constructs the BlastSeqSrc
    /// @note ownership of the constructed object is handled by this class
    BlastSeqSrc* MakeSeqSrc();

    /// Retrieves or constructs the IBlastSeqInfoSrc
    /// @note ownership of the constructed object is handled by this class
    IBlastSeqInfoSrc* MakeSeqInfoSrc();

private:
    /// True if the BlastSeqSrc object below is owned by this class
    //bool m_OwnSeqSrc;

    /// Pointer to the BlastSeqSrc
    BlastSeqSrc* m_SeqSrc;

    /// True if the IBlastSeqInfoSrc object below is owned by this class
    //bool m_OwnSeqInfoSrc;

    /// Pointer to the IBlastSeqInfoSrc
    CRef<IBlastSeqInfoSrc> m_SeqInfoSrc;

    /// Object containing BLAST database description
    CRef<CSearchDatabase> m_DbInfo;

    /// BLAST database handle
    CRef<CSeqDB> m_SeqDb;

    /// IQueryFactory containing the subject sequences
    CRef<IQueryFactory> m_QueryFactory;

    /// Options to be used when instantiating the subject sequences
    CConstRef<CBlastOptionsHandle> m_OptsHandle;
    
    /// Initialize the internal CSeqDB object from a CSearchDatabase object
    void x_InitSeqDB();

    /// Prohibit copy-constructor
    CLocalDbAdapter(const CLocalDbAdapter&);
    /// Prohibit assignment operator
    CLocalDbAdapter & operator=(const CLocalDbAdapter&);
};


END_SCOPE(BLAST)
END_NCBI_SCOPE

/* @} */

#endif /* ALGO_BLAST_API___LOCAL_DB_ADAPTER__HPP */

