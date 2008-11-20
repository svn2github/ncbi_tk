#ifndef SEQ_LOC_MAPPER__HPP
#define SEQ_LOC_MAPPER__HPP

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
* Author: Aleksey Grichenko
*
* File Description:
*   Seq-loc mapper
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <util/range.hpp>
#include <util/rangemap.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/general/Int_fuzz.hpp>
#include <objmgr/impl/heap_scope.hpp>
#include <objects/seq/seq_loc_mapper_base.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/** @addtogroup ObjectManagerCore
 *
 * @{
 */


class CScope;
class CBioseq_Handle;
class CSeqMap;
class CSeqMap_CI;
struct SSeqMapSelector;


/////////////////////////////////////////////////////////////////////////////
///
///  CSeq_loc_Mapper --
///
///  Mapping locations and alignments between bioseqs through seq-locs,
///  features, alignments or between parts of segmented bioseqs.


class NCBI_XOBJMGR_EXPORT CSeq_loc_Mapper : public CSeq_loc_Mapper_Base
{
public:
    enum ESeqMapDirection {
        eSeqMap_Up,    ///< map from segments to the top level bioseq
        eSeqMap_Down   ///< map from a segmented bioseq to segments
    };

    /// Mapping through a pre-filled CMappipngRanges. Source(s) and
    /// destination(s) are considered as having the same width.
    /// @param mapping_ranges
    ///  CMappingRanges filled with the desired source and destination
    ///  ranges. Must be a heap object (will be stored in a CRef<>).
    /// @param scope
    ///  Optional scope (required only for mapping alignments).
    CSeq_loc_Mapper(CMappingRanges* mapping_ranges,
                    CScope*         scope = 0);

    /// Mapping through a feature, both location and product must be set.
    /// If scope is set, synonyms are resolved for each source ID.
    CSeq_loc_Mapper(const CSeq_feat&  map_feat,
                    EFeatMapDirection dir,
                    CScope*           scope = 0);

    /// Mapping between two seq_locs. If scope is set, synonyms are resolved
    /// for each source ID.
    CSeq_loc_Mapper(const CSeq_loc&   source,
                    const CSeq_loc&   target,
                    CScope*           scope = 0);

    /// Mapping through an alignment. Need to specify target ID or
    /// target row of the alignment. Any other ID is mapped to the
    /// target one. If scope is set, synonyms are resolved for each source ID.
    /// Only the first row matching target ID is used, all other rows
    /// are considered source.
    CSeq_loc_Mapper(const CSeq_align& map_align,
                    const CSeq_id&    to_id,
                    CScope*           scope = 0,
                    TMapOptions       opts = 0);
    CSeq_loc_Mapper(const CSeq_align& map_align,
                    size_t            to_row,
                    CScope*           scope = 0,
                    TMapOptions       opts = 0);

    /// Mapping between segments and the top level sequence.
    /// @param target_seq
    ///  Top level bioseq
    /// @param direction
    ///  Direction of mapping: up (from segments to master) or down.
    CSeq_loc_Mapper(CBioseq_Handle   target_seq,
                    ESeqMapDirection direction);

    /// Mapping through a seq-map.
    /// @param seq_map
    ///  Sequence map defining the mapping
    /// @param direction
    ///  Direction of mapping: up (from segments to master) or down.
    /// @param top_level_id
    ///  Explicit destination id when mapping up, may be used with
    ///  seq-maps constructed from a seq-loc with multiple ids.
    CSeq_loc_Mapper(const CSeqMap&   seq_map,
                    ESeqMapDirection direction,
                    const CSeq_id*   top_level_id = 0,
                    CScope*          scope = 0);

    /// Mapping between segments and the top level sequence limited by depth.
    /// @param depth
    ///  Mapping depth. Depth of 0 converts synonyms.
    /// @param top_level_seq
    ///  Top level bioseq
    /// @param direction
    ///  Direction of mapping: up (from segments to master) or down.
    CSeq_loc_Mapper(size_t                depth,
                    const CBioseq_Handle& top_level_seq,
                    ESeqMapDirection      direction);

    /// Depth-limited mapping through a seq-map.
    /// @param depth
    ///  Mapping depth. Depth of 0 converts synonyms.
    /// @param seq_map
    ///  Sequence map defining the mapping
    /// @param direction
    ///  Direction of mapping: up (from segments to master) or down.
    /// @param top_level_id
    ///  Explicit destination id when mapping up, may be used with
    ///  seq-maps constructed from a seq-loc with multiple ids.
    CSeq_loc_Mapper(size_t           depth,
                    const CSeqMap&   top_level_seq,
                    ESeqMapDirection direction,
                    const CSeq_id*   top_level_id = 0,
                    CScope*          scope = 0);

    /// Mapping between segments and the top level sequence.
    /// @param target_seq
    ///  Top level bioseq
    /// @param direction
    ///  Direction of mapping: up (from segments to master) or down.
    /// @param selector
    ///  Seq-map selector with additional restrictions (range, strand etc.).
    ///  Some properties of the selector are always adjusted by the mapper.
    CSeq_loc_Mapper(CBioseq_Handle   target_seq,
                    ESeqMapDirection direction,
                    SSeqMapSelector  selector);

    /// Mapping through a seq-map.
    /// @param seq_map
    ///  Sequence map defining the mapping
    /// @param direction
    ///  Direction of mapping: up (from segments to master) or down.
    /// @param selector
    ///  Seq-map selector with additional restrictions (range, strand etc.).
    ///  Some properties of the selector are always adjusted by the mapper.
    /// @param top_level_id
    ///  Explicit destination id when mapping up, may be used with
    ///  seq-maps constructed from a seq-loc with multiple ids.
    CSeq_loc_Mapper(const CSeqMap&   seq_map,
                    ESeqMapDirection direction,
                    SSeqMapSelector  selector,
                    const CSeq_id*   top_level_id = 0,
                    CScope*          scope = 0);

    ~CSeq_loc_Mapper(void);

    // Collect synonyms for the given seq-id
    virtual void CollectSynonyms(const CSeq_id_Handle& id,
                                 TSynonyms&            synonyms) const;

protected:
    // Check molecule type, return character width (3=na, 1=aa, 0=unknown).
    virtual int CheckSeqWidth(const CSeq_id& id,
                              int            width,
                              TSeqPos*       length = 0);

    // Get sequence length for the given seq-id
    virtual TSeqPos GetSequenceLength(const CSeq_id& id);

    // Create CSeq_align_Mapper, add any necessary arguments
    virtual CSeq_align_Mapper_Base*
        InitAlignMapper(const CSeq_align& src_align);

private:
    CSeq_loc_Mapper(const CSeq_loc_Mapper&);
    CSeq_loc_Mapper& operator=(const CSeq_loc_Mapper&);

    void x_InitializeSeqMap(const CSeqMap&   seq_map,
                            const CSeq_id*   top_id,
                            ESeqMapDirection direction);
    void x_InitializeSeqMap(const CSeqMap&   seq_map,
                            size_t           depth,
                            const CSeq_id*   top_id,
                            ESeqMapDirection direction);
    void x_InitializeSeqMap(CSeqMap_CI       seg_it,
                            const CSeq_id*   top_id,
                            ESeqMapDirection direction);
    void x_InitializeBioseq(const CBioseq_Handle& bioseq,
                            const CSeq_id*        top_id,
                            ESeqMapDirection      direction);
    void x_InitializeBioseq(const CBioseq_Handle& bioseq,
                            size_t                depth,
                            const CSeq_id*        top_id,
                            ESeqMapDirection      direction);

private:
    CHeapScope        m_Scope;
};


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // SEQ_LOC_MAPPER__HPP
