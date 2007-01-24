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
* Author: Aleksandr Morgulis
*
*/

/// @file blast_dbindex.cpp
/// Functionality for indexed databases

#include <ncbi_pch.hpp>
#include <sstream>
#include <list>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbithr.hpp>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_gapalign.h>
#include <algo/blast/core/blast_util.h>

#include <objtools/readers/seqdb/seqdbcommon.hpp>

#include <algo/blast/api/blast_dbindex.hpp>
#include <algo/blast/dbindex/dbindex.hpp>

#include "algo/blast/core/mb_indexed_lookup.h"

// Comment this out to continue with extensions.
// #define STOP_AFTER_PRESEARCH 1

/** @addtogroup AlgoBlast
 *
 * @{
 */

extern "C" {

/** Construct a new instance of index based subject sequence source.

    @param retval Preallocated instance of BlastSeqSrc structure.
    @return \e retval with filled in fields.
*/
static BlastSeqSrc * s_IDbSrcNew( BlastSeqSrc * retval, void * args );

/** Get the seed search results for a give subject id and chunk number.

    @param idb          [I]   Database and index data.
    @param oid          [I]   Subject id.
    @param chunk        [I]   Chunk number.
    @param init_hitlist [I/O] Results are returned here.
*/
static void s_MB_IdbGetResults(
        void * idb_v,
        Int4 oid_i, Int4 chunk_i,
        BlastInitHitList * init_hitlist );
}

BEGIN_NCBI_SCOPE
BEGIN_SCOPE( blast )

USING_SCOPE( ncbi::objects );
USING_SCOPE( ncbi::blastdbindex );

/** No-op presearch function. Used when index search is not enables.
    @sa DbIndexPreSearchFnType()
*/
static void NullPreSearch( 
        BlastSeqSrc *, LookupTableWrap *, 
        BLAST_SequenceBlk *, BlastSeqLoc *,
        LookupTableOptions *, BlastInitialWordOptions * ) {}

/** Global pointer to the appropriate pre-search function, based
    on whether or not index search is enabled.
*/
static DbIndexPreSearchFnType PreSearchFn = &NullPreSearch;
    
//------------------------------------------------------------------------------
/** This structure is used to transfer arguments to s_IDbSrcNew(). */
struct SIndexedDbNewArgs
{
    string indexname;   /**< Name of the index data file. */
    BlastSeqSrc * db;   /**< BLAST database sequence source instance. */
};

//------------------------------------------------------------------------------
/** This class is responsible for loading indices and doing the actual
    seed search.

    It acts as a middle man between the blast engine and dbindex library.
*/
class CIndexedDb : public CObject
{
    private:

        /** Type used to represent collections of search result sets. */
        typedef vector< CConstRef< CDbIndex::CSearchResults > > TResultSet;

        /** Type used to map loaded indices to subject ids. */
        typedef vector< CDbIndex::TSeqNum > TSeqMap;

        /** Type of the collection of currently loaded indices. */
        typedef vector< CRef< CDbIndex > > TIndexSet;

        /** Data local to a running thread.
        */
        struct SThreadLocal 
        {
            CRef< CIndexedDb > idb_; /**< Pointer to the shared index data. */
        };

        /** Find an index corresponding to the given subject id.

            @param oid The subject sequence id.
            @return Index of the corresponding index data in 
                    \e this->indices_.
        */
        TSeqMap::size_type LocateIndex( CDbIndex::TSeqNum oid ) const
        {
            for( TSeqMap::size_type i = 0; i < seqmap_.size(); ++i ) {
                if( seqmap_[i] > oid ) return i;
            }

            assert( 0 );
            return 0;
        }

        BlastSeqSrc * db_;      /**< Points to the real BLAST database instance. */
        TResultSet results_;    /**< Set of result sets, one per loaded index. */
        TSeqMap seqmap_;        /**< For each element of \e indices_ with index i
                                     seqmap_[i] contains one plus the last oid of
                                     that database index. */

        unsigned long threads_;         /**< Number of threads to use for seed search. */
        vector< string > index_names_;  /**< List of index volume names. */
        bool seq_from_index_;   /**< Indicates if it is OK to serve sequence data
                                     directly from the index. */
        bool index_preloaded_;  /**< True if indices are preloaded in constructor. */
        TIndexSet indices_;     /**< Currently loaded indices. */


    public:

        typedef SThreadLocal TThreadLocal; /**< Type for thread local data. */

        typedef std::list< TThreadLocal * > TThreadDataSet; /** Type of a set of allocated TThreadLocal objects. */

        static TThreadDataSet Thread_Data_Set; /* Set of allocated TThreadLocal objects. */

        /** Object constructor.
            
            @param indexname A string that is a comma separated list of index
                             file prefix, number of threads, first and
                             last chunks of the index.
            @param db        Points to the open BLAST database object.
        */
        explicit CIndexedDb( const string & indexname, BlastSeqSrc * db );

        /** Object destructor. */
        ~CIndexedDb();

        /** Access a wrapped BLAST database object.

            @return The BLAST database object.
        */
        BlastSeqSrc * GetDb() const { return db_; }

        /** Get the data portion of the BLAST database object.

            \e this->db_ encapsulates an actual structure representing 
            a particular implementation of an open BLAST database. This
            function returns a typeless pointer to this data.

            @return Pointer to the data encapsulated by /e this=>db_.
        */
        void * GetSeqDb() const 
        { return _BlastSeqSrcImpl_GetDataStructure( db_ ); }

        /** Check whether any results were reported for a given subject sequence.

            @param oid The subject sequence id.
            @return True if the were seeds found for that subject;
                    false otherwise.
        */
        bool CheckOid( Int4 oid ) const
        {
            TSeqMap::size_type i = LocateIndex( oid );
            const CConstRef< CDbIndex::CSearchResults > & results = results_[i];
            if( i > 0 ) oid -= seqmap_[i-1];
            return results->CheckResults( oid );
        }

        /** Invoke the seed search procedure on each of the loaded indices.

            Each search is run in a separate thread. The function waits until
            all threads are complete before it returns.

            @param queries      Queries descriptor.
            @param locs         Unmasked intervals of queries.
            @param lut_options  Lookup table parameters, like target word size.
            @param word_options Contains window size of two-hits based search.
        */
        void PreSearch( 
                BLAST_SequenceBlk * queries, BlastSeqLoc * locs,
                LookupTableOptions * lut_options,
                BlastInitialWordOptions *  word_options );

        /** Return results corresponding to a given subject sequence and chunk.

            @param oid          [I]   The subject sequence id.
            @param chunk        [I]   The chunk number.
            @param init_hitlist [I/O] The results are returned here.
        */
        void GetResults( 
                CDbIndex::TSeqNum oid,
                CDbIndex::TSeqNum chunk,
                BlastInitHitList * init_hitlist ) const;

        /** Check if sequences can be supplied directly from the index.

            @return true if sequences can be provided from the index;
                    false otherwise.
        */
        bool SeqFromIndex() const { return seq_from_index_; }

        /** Get the length of the subject sequence.

            @param oid Subject ordinal id.

            @return Length of the subject sequence in bases.
        */
        TSeqPos GetSeqLength( CDbIndex::TSeqNum oid ) const
        { return indices_[LocateIndex( oid )]->GetSeqLen( oid ); }

        /** Get the subject sequence data.

            @param oid Subject ordinal id.

            @return Pointer to the start of the subject sequence data.
        */
        Uint1 * GetSeqData( CDbIndex::TSeqNum oid ) const
        { 
            return const_cast< Uint1 * >(
                    indices_[LocateIndex( oid )]->GetSeqData( oid )); 
        }
};

//------------------------------------------------------------------------------
CIndexedDb::TThreadDataSet CIndexedDb::Thread_Data_Set; 

//------------------------------------------------------------------------------
/** Callback that is called for index based seed search.
    @sa For the meaning of parameters see DbIndexPreSearchFnType.
*/
static void IndexedDbPreSearch( 
        BlastSeqSrc * seq_src, 
        LookupTableWrap * lt_wrap,
        BLAST_SequenceBlk * queries, 
        BlastSeqLoc * locs,
        LookupTableOptions * lut_options, 
        BlastInitialWordOptions * word_options )
{
    CIndexedDb::TThreadLocal * idb_tl = 
        static_cast< CIndexedDb::TThreadLocal * >(
                _BlastSeqSrcImpl_GetDataStructure( seq_src ) );
    bool found = false;

    for( CIndexedDb::TThreadDataSet::iterator i = 
            CIndexedDb::Thread_Data_Set.begin();
         i != CIndexedDb::Thread_Data_Set.end(); ++i ) {
        if( *i == idb_tl ) {
            found = true;
            break;
        }
    }

    if( !found ) return;

    CIndexedDb * idb = idb_tl->idb_.GetPointerOrNull();
    lt_wrap->lut = (void *)idb;
    lt_wrap->read_indexed_db = (void *)(&s_MB_IdbGetResults);
    idb->PreSearch( queries, locs, lut_options, word_options );

#ifdef STOP_AFTER_PRESEARCH
    exit( 0 );
#endif
}

//------------------------------------------------------------------------------
CIndexedDb::CIndexedDb( const string & indexname, BlastSeqSrc * db )
    : db_( db ), threads_( 1 ), 
      seq_from_index_( false ), index_preloaded_( false )
{
    // Parse the indexname as a comma separated list
    string::size_type start = 0, end = 0;

    if( !indexname.empty() ) {
        unsigned long start_vol = 0, stop_vol = 99;
        end = indexname.find_first_of( ",", start );
        string index_base = indexname.substr( start, end );
        start = end + 1;

        if( start < indexname.length() && end != string::npos ) {
            end = indexname.find_first_of( ",", start );
            string threads_str = indexname.substr( start, end );
            
            if( !threads_str.empty() ) {
                threads_ = atoi( threads_str.c_str() );
            }

            start = end + 1;

            if( start < indexname.length() && end != string::npos ) {
                end = indexname.find_first_of( ",", start );
                string start_vol_str = indexname.substr( start, end );

                if( !start_vol_str.empty() ) {
                    start_vol = atoi( start_vol_str.c_str() );
                }

                start = end + 1;

                if( start < indexname.length() && end != string::npos ) {
                    end = indexname.find_first_of( ",", start );
                    string stop_vol_str = indexname.substr( start, end );

                    if( !stop_vol_str.empty() ) {
                        stop_vol = atoi( stop_vol_str.c_str() );
                    }
                }
            }
        }

        if( threads_ > 0 && start_vol <= stop_vol ) {
            long last_i = -1;

            for( long i = start_vol; (unsigned long)i <= stop_vol; ++i ) {
                ostringstream os;
                os << index_base << "." << setw( 2 ) << setfill( '0' )
                   << i << ".idx";
                string name = SeqDB_ResolveDbPath( os.str() );

                if( !name.empty() ){
                    if( i - last_i > 1 ) {
                        for( long j = last_i + 1; j < i; ++j ) {
                            ERR_POST( Error << "Index volume " 
                                            << j << " not resolved." );
                        }
                    }

                    index_names_.push_back( name );
                    last_i = i;
                }
            }
        }
        else if( threads_ == 0 ) threads_ = 1;
    }

    if( index_names_.empty() ) {
        throw std::runtime_error(
                "CIndexedDb: no index file specified" );
    }

    PreSearchFn = &IndexedDbPreSearch;
    
    if( threads_ >= index_names_.size() ) {
        seq_from_index_ = true;
        index_preloaded_ = true;

        for( vector< string >::size_type ind = 0; 
                ind < index_names_.size(); ++ind ) {
            CRef< CDbIndex > index = CDbIndex::Load( index_names_[ind] );

            if( index == 0 ) {
                throw std::runtime_error(
                        (string( "CIndexedDb: could not load index" ) 
                        + index_names_[ind]).c_str() );
            }

            if( index->Version() != 5 ) seq_from_index_ = false;
            indices_.push_back( index );
            results_.push_back( CConstRef< CDbIndex::CSearchResults >( null ) );
            CDbIndex::TSeqNum s = seqmap_.empty() ? 0 : *seqmap_.rbegin();
            seqmap_.push_back( s + (index->StopSeq() - index->StartSeq()) );
        }
    }
}

//------------------------------------------------------------------------------
CIndexedDb::~CIndexedDb()
{
    PreSearchFn = &NullPreSearch;
    BlastSeqSrcFree( db_ );
}

//------------------------------------------------------------------------------
class CPreSearchThread : public CThread
{
    public:

        CPreSearchThread(
                BLAST_SequenceBlk * queries,
                BlastSeqLoc * locs,
                const CDbIndex::SSearchOptions & sopt,
                CRef< CDbIndex > & index,
                CConstRef< CDbIndex::CSearchResults > & results )
            : queries_( queries ), locs_( locs ), sopt_( sopt ),
              index_( index ), results_( results )
        {}

        virtual void * Main( void );
        virtual void OnExit( void ) {}

    private:

        BLAST_SequenceBlk * queries_;
        BlastSeqLoc * locs_;
        const CDbIndex::SSearchOptions & sopt_;
        CRef< CDbIndex > & index_;
        CConstRef< CDbIndex::CSearchResults > & results_;
};

//------------------------------------------------------------------------------
void * CPreSearchThread::Main( void )
{
    results_ = index_->Search( queries_, locs_, sopt_ );
    index_->Remap();
    return 0;
}

//------------------------------------------------------------------------------
void CIndexedDb::PreSearch( 
        BLAST_SequenceBlk * queries, BlastSeqLoc * locs,
        LookupTableOptions * lut_options , 
        BlastInitialWordOptions * word_options )
{
    CDbIndex::SSearchOptions sopt;
    sopt.word_size = lut_options->word_size;
    sopt.template_type = lut_options->mb_template_type;
    sopt.two_hits = (word_options->window_size > 0);

    for( vector< string >::size_type v = 0; 
            v < index_names_.size(); v += threads_ ) {
        if( !index_preloaded_ ) {
            indices_.clear();

            for( vector< string >::size_type ind = 0; 
                ind < threads_ && v + ind < index_names_.size(); ++ind ) {
                CRef< CDbIndex > index = CDbIndex::Load( index_names_[v+ind] );

                if( index == 0 ) {
                    throw std::runtime_error(
                            (string( "CIndexedDb: could not load index" ) 
                            + index_names_[v+ind]).c_str() );
                }

                if( index->Version() != 5 ) seq_from_index_ = false;
                indices_.push_back( index );
                results_.push_back( CConstRef< CDbIndex::CSearchResults >( null ) );
                CDbIndex::TSeqNum s = seqmap_.empty() ? 0 : *seqmap_.rbegin();
                seqmap_.push_back( s + (index->StopSeq() - index->StartSeq()) );
            }
        }

        if( indices_.size() > 1 ) {
            vector< CPreSearchThread * > threads( indices_.size(), 0 );

            for( vector< CPreSearchThread * >::size_type i = 0;
                    i < threads.size(); ++i ) {
                threads[i] = new CPreSearchThread(
                        queries, locs, sopt, indices_[i], results_[v+i] );
                threads[i]->Run();
            }

            for( vector< CPreSearchThread * >::iterator i = threads.begin();
                    i != threads.end(); ++i ) {
                void * result;
                (*i)->Join( &result );
                *i = 0;
            }
        }
        else {
            CRef< CDbIndex > & index = indices_[0];
            CConstRef< CDbIndex::CSearchResults > & results = results_[v];
            results = index->Search( queries, locs, sopt );
            index->Remap();
        }
    }
}

//------------------------------------------------------------------------------
void CIndexedDb::GetResults( 
        CDbIndex::TSeqNum oid, CDbIndex::TSeqNum chunk, 
        BlastInitHitList * init_hitlist ) const
{
    BlastInitHitList * res = 0;
    TSeqMap::size_type i = LocateIndex( oid );
    const CConstRef< CDbIndex::CSearchResults > & results = results_[i];
    if( i > 0 ) oid -= seqmap_[i-1];

    if( (res = results->GetResults( oid, chunk )) != 0 ) {
        BlastInitHitListMove( init_hitlist, res );
    }else {
        BlastInitHitListReset( init_hitlist );
    }
}

//------------------------------------------------------------------------------
BlastSeqSrc * DbIndexSeqSrcInit( const string & indexname, BlastSeqSrc * db )
{
    BlastSeqSrcNewInfo bssn_info;
    SIndexedDbNewArgs idb_args = { indexname, db };
    bssn_info.constructor = &s_IDbSrcNew;
    bssn_info.ctor_argument = (void *)&idb_args;
    return BlastSeqSrcNew( &bssn_info );
}

//------------------------------------------------------------------------------
DbIndexPreSearchFnType GetDbIndexPreSearchFn() { return PreSearchFn; }

END_SCOPE( blast )
END_NCBI_SCOPE

USING_SCOPE( ncbi );
USING_SCOPE( ncbi::blast );

extern "C" {

//------------------------------------------------------------------------------
/** C language wrapper around CIndexedDb::GetDb(). */
static BlastSeqSrc * s_GetForwardSeqSrc( void * handle )
{
    CIndexedDb::TThreadLocal * idb_handle = (CIndexedDb::TThreadLocal *)handle;
    return idb_handle->idb_->GetDb();
}

//------------------------------------------------------------------------------
/** C language wrapper around CIndexedDb::GetSeqDb(). */
void * s_GetForwardSeqDb( void * handle )
{
    CIndexedDb::TThreadLocal * idb_handle = (CIndexedDb::TThreadLocal *)handle;
    return idb_handle->idb_->GetSeqDb();
}

//------------------------------------------------------------------------------
/** Forwards the call to CIndexedDb::db_. */
static Int4 s_IDbGetNumSeqs( void * handle, void * x )
{
    BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
    void * fw_handle = s_GetForwardSeqDb( handle );
    return _BlastSeqSrcImpl_GetGetNumSeqs( fw_seqsrc )( fw_handle, x );
}

//------------------------------------------------------------------------------
/** Forwards the call to CIndexedDb::db_. */
static Int4 s_IDbGetMaxLength( void * handle, void * x )
{
    BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
    void * fw_handle = s_GetForwardSeqDb( handle );
    return _BlastSeqSrcImpl_GetGetMaxSeqLen( fw_seqsrc )( fw_handle, x );
}

//------------------------------------------------------------------------------
/** Forwards the call to CIndexedDb::db_. */
static Int4 s_IDbGetAvgLength( void * handle, void * x )
{
    BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
    void * fw_handle = s_GetForwardSeqDb( handle );
    return _BlastSeqSrcImpl_GetGetAvgSeqLen( fw_seqsrc )( fw_handle, x );
}

//------------------------------------------------------------------------------
/** Forwards the call to CIndexedDb::db_. */
static Int8 s_IDbGetTotLen( void * handle, void * x )
{
    BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
    void * fw_handle = s_GetForwardSeqDb( handle );
    return _BlastSeqSrcImpl_GetGetTotLen( fw_seqsrc )( fw_handle, x );
}

//------------------------------------------------------------------------------
/** Forwards the call to CIndexedDb::db_. */
static const char * s_IDbGetName( void * handle, void * x )
{
    BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
    void * fw_handle = s_GetForwardSeqDb( handle );
    return _BlastSeqSrcImpl_GetGetName( fw_seqsrc )( fw_handle, x );
}

//------------------------------------------------------------------------------
/** Forwards the call to CIndexedDb::db_. */
static Boolean s_IDbGetIsProt( void * handle, void * x )
{
    BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
    void * fw_handle = s_GetForwardSeqDb( handle );
    return _BlastSeqSrcImpl_GetGetIsProt( fw_seqsrc )( fw_handle, x );
}

//------------------------------------------------------------------------------
/** Forwards the call to CIndexedDb::db_. */
static Int2 s_IDbGetSequence( void * handle, void * x )
{
    CIndexedDb::TThreadLocal * idb_handle = (CIndexedDb::TThreadLocal *)handle;
    CRef< CIndexedDb > idb = idb_handle->idb_;
    BlastSeqSrcGetSeqArg * seq_arg = (BlastSeqSrcGetSeqArg *)x;


    if( !idb->SeqFromIndex() || seq_arg->encoding != 0 ) {
        BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
        void * fw_handle = s_GetForwardSeqDb( handle );
        return _BlastSeqSrcImpl_GetGetSequence( fw_seqsrc )( fw_handle, x );
    }
    else {
        if( seq_arg->seq == 0 ) {
            BLAST_SequenceBlk * new_seq_blk;
            if( BlastSeqBlkNew( &new_seq_blk ) < 0 ) return -1;
            seq_arg->seq = new_seq_blk;
        }

        BlastSequenceBlkClean( seq_arg->seq );
        seq_arg->seq->oid = seq_arg->oid;
        seq_arg->seq->sequence = idb->GetSeqData( seq_arg->oid );
        seq_arg->seq->length = idb->GetSeqLength( seq_arg->oid );
        return 0;
    }
}

//------------------------------------------------------------------------------
/** Forwards the call to CIndexedDb::db_. */
static Int4 s_IDbGetSeqLen( void * handle, void * x )
{
    BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
    void * fw_handle = s_GetForwardSeqDb( handle );
    return _BlastSeqSrcImpl_GetGetSeqLen( fw_seqsrc )( fw_handle, x );
}

//------------------------------------------------------------------------------
/** Forwards the call to CIndexedDb::db_ but skip over the ones for
    which no results were poduces by pre-search. 
*/
static Int4 s_IDbIteratorNext( void * handle, BlastSeqSrcIterator * itr )
{
    CIndexedDb::TThreadLocal * idb_handle = (CIndexedDb::TThreadLocal *)handle;
    CIndexedDb * idb = idb_handle->idb_.GetPointerOrNull();
    BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
    void * fw_handle = s_GetForwardSeqDb( handle );
    Int4 oid = BLAST_SEQSRC_EOF;

    do {
        oid = _BlastSeqSrcImpl_GetIterNext( fw_seqsrc )( fw_handle, itr );
    }while( oid != BLAST_SEQSRC_EOF && !idb->CheckOid( oid ) );

    return oid;
}

//------------------------------------------------------------------------------
/** Forwards the call to CIndexedDb::db_. */
static void s_IDbReleaseSequence( void * handle, void * x )
{
    CIndexedDb::TThreadLocal * idb_handle = (CIndexedDb::TThreadLocal *)handle;
    CRef< CIndexedDb > idb = idb_handle->idb_;
    BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
    void * fw_handle = s_GetForwardSeqDb( handle );
    _BlastSeqSrcImpl_GetReleaseSequence( fw_seqsrc )( fw_handle, x );
}

//------------------------------------------------------------------------------
/** Forwards the call to CIndexedDb::db_. */
static void s_IDbResetChunkIterator( void * handle )
{
    BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
    void * fw_handle = s_GetForwardSeqDb( handle );
    _BlastSeqSrcImpl_GetResetChunkIterator( fw_seqsrc )( fw_handle );
}

//------------------------------------------------------------------------------
/** Destroy the sequence source.

    Destroys and frees CIndexedDb instance and the wrapped BLAST database.

    @param seq_src BlastSeqSrc wrapper around CIndexedDb object.
    @return NULL.
*/
static BlastSeqSrc * s_IDbSrcFree( BlastSeqSrc * seq_src )
{
    if( seq_src ) {
        CIndexedDb::TThreadLocal * idb = 
            static_cast< CIndexedDb::TThreadLocal * >( 
                    _BlastSeqSrcImpl_GetDataStructure( seq_src ) );

        for( CIndexedDb::TThreadDataSet::iterator i = 
                CIndexedDb::Thread_Data_Set.begin();
             i != CIndexedDb::Thread_Data_Set.end(); ++i ) {
            if( *i == idb ) {
                CIndexedDb::Thread_Data_Set.erase( i );
                break;
            }
        }

        delete idb;
        sfree( seq_src );
    }

    return 0;
}

//------------------------------------------------------------------------------
/** Fill the BlastSeqSrc data with a copy of its own contents.

    @param seq_src A BlastSeqSrc instance to fill.
    @return seq_src if copy was successfull, NULL otherwise.
*/
static BlastSeqSrc * s_IDbSrcCopy( BlastSeqSrc * seq_src )
{
    if( !seq_src ) {
        return 0;
    }else {
        CIndexedDb::TThreadLocal * idb =
            static_cast< CIndexedDb::TThreadLocal * >(
                    _BlastSeqSrcImpl_GetDataStructure( seq_src ) );
        try {
            CIndexedDb::TThreadLocal * idb_copy = 
                new CIndexedDb::TThreadLocal( *idb );
            CIndexedDb::Thread_Data_Set.push_back( idb_copy );
            _BlastSeqSrcImpl_SetDataStructure( seq_src, (void *)idb_copy );
            return seq_src;
        }catch( ... ) {
            return 0;
        }
    }
}

//------------------------------------------------------------------------------
/** Initialize the BlastSeqSrc data structure with the appropriate callbacks. */
static void s_IDbSrcInit( BlastSeqSrc * retval, CIndexedDb::TThreadLocal * idb )
{
    ASSERT( retval );
    ASSERT( idb );

    _BlastSeqSrcImpl_SetDeleteFnPtr   (retval, & s_IDbSrcFree);
    _BlastSeqSrcImpl_SetCopyFnPtr     (retval, & s_IDbSrcCopy);
    _BlastSeqSrcImpl_SetDataStructure (retval, (void*) idb);
    _BlastSeqSrcImpl_SetGetNumSeqs    (retval, & s_IDbGetNumSeqs);
    _BlastSeqSrcImpl_SetGetMaxSeqLen  (retval, & s_IDbGetMaxLength);
    _BlastSeqSrcImpl_SetGetAvgSeqLen  (retval, & s_IDbGetAvgLength);
    _BlastSeqSrcImpl_SetGetTotLen     (retval, & s_IDbGetTotLen);
    _BlastSeqSrcImpl_SetGetName       (retval, & s_IDbGetName);
    _BlastSeqSrcImpl_SetGetIsProt     (retval, & s_IDbGetIsProt);
    _BlastSeqSrcImpl_SetGetSequence   (retval, & s_IDbGetSequence);
    _BlastSeqSrcImpl_SetGetSeqLen     (retval, & s_IDbGetSeqLen);
    _BlastSeqSrcImpl_SetIterNext      (retval, & s_IDbIteratorNext);
    _BlastSeqSrcImpl_SetReleaseSequence   (retval, & s_IDbReleaseSequence);
    _BlastSeqSrcImpl_SetResetChunkIterator (retval, & s_IDbResetChunkIterator);
}

//------------------------------------------------------------------------------
static BlastSeqSrc * s_IDbSrcNew( BlastSeqSrc * retval, void * args )
{
    ASSERT( retval );
    ASSERT( args );
    SIndexedDbNewArgs * idb_args = (SIndexedDbNewArgs *)args;
    CIndexedDb::TThreadLocal * idb = 0;
    bool success = false;

    try {
        idb = new CIndexedDb::TThreadLocal;
        idb->idb_.Reset( new CIndexedDb( idb_args->indexname, idb_args->db ) );
        success = true;
    } catch (const ncbi::CException& e) {
        _BlastSeqSrcImpl_SetInitErrorStr(retval, 
                        strdup(e.ReportThis(eDPF_ErrCodeExplanation).c_str()));
    } catch (const std::exception& e) {
        _BlastSeqSrcImpl_SetInitErrorStr(retval, strdup(e.what()));
    } catch (...) {
        _BlastSeqSrcImpl_SetInitErrorStr(retval, 
             strdup("Caught unknown exception from CSeqDB constructor"));
    }

    if( success ) {
        CIndexedDb::Thread_Data_Set.push_back( idb );
        s_IDbSrcInit( retval, idb );
    }else {
        ERR_POST( "Index load operation failed." );
        delete idb;
        sfree( retval );
    }

    return retval;
}

//------------------------------------------------------------------------------
/** Get the seeds corresponding to the given subject sequence and chunk.

    The function is a C language wrapper around CIndexedDb::GetResults().
*/
static void s_MB_IdbGetResults(
        void * idb_v,
        Int4 oid_i, Int4 chunk_i,
        BlastInitHitList * init_hitlist )
{
    ASSERT( idb_v != 0 );
    ASSERT( oid_i >= 0 );
    ASSERT( chunk_i >= 0 );
    ASSERT( init_hitlist != 0 );

    CIndexedDb * idb = (CIndexedDb *)idb_v;
    CDbIndex::TSeqNum oid = (CDbIndex::TSeqNum)oid_i;
    CDbIndex::TSeqNum chunk = (CDbIndex::TSeqNum)chunk_i;

    idb->GetResults( oid, chunk, init_hitlist );
}

} /* extern "C" */

/* @} */

/*
 * ===========================================================================
 * $Log: blast_dbindex.cpp,v $
 * Revision 1.7  2007/01/08 19:54:06  morgulis
 * Umapping unused portions of index after search is complete.
 *
 * Revision 1.5  2006/12/07 20:58:32  morgulis
 * Fixed CheckOid().
 *
 * Revision 1.4  2006/11/20 20:20:02  papadopo
 * from Alex Morgulis: remove compiler warning
 *
 * Revision 1.3  2006/11/15 23:24:10  papadopo
 * From Alex Morgulis:
 * 1. Allow individual volumes of an indexed database to be searched
 * 2. Add utility routines
 * 3. Add doxygen
 *
 * Revision 1.2  2006/10/05 15:59:21  papadopo
 * GetResults is static now
 *
 * Revision 1.1  2006/10/04 19:19:45  papadopo
 * interface for indexed blast databases
 *
 * ===========================================================================
 */
