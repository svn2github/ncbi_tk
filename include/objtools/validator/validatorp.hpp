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
 *`
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko......
 *
 * File Description:
 *   Privae classes and definition for the validator
 *   .......
 *
 */

#ifndef VALIDATOR___VALIDATORP__HPP
#define VALIDATOR___VALIDATORP__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_autoinit.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>  // for CMappedFeat
#include <objmgr/util/seq_loc_util.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/GIBB_mol.hpp>
#include <util/strsearch.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Packed_seg.hpp>
#include <objects/valid/Comment_set.hpp>
#include <objects/valid/Comment_rule.hpp>

#include <objtools/validator/validator.hpp>
#include <objtools/validator/tax_validation_and_cleanup.hpp>
#include <objtools/validator/utilities.hpp>
#include <objtools/validator/feature_match.hpp>

#include <objtools/alnmgr/sparse_aln.hpp>

#include <objmgr/util/create_defline.hpp>

#include <objmgr/util/feature.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CCit_sub;
class CCit_art;
class CCit_gen;
class CSeq_feat;
class CBioseq;
class CSeqdesc;
class CSeq_annot;
class CTrna_ext;
class CProt_ref;
class CSeq_loc;
class CFeat_CI;
class CPub_set;
class CAuth_list;
class CTitle;
class CMolInfo;
class CUser_object;
class CSeqdesc_CI;
class CSeq_graph;
class CMappedGraph;
class CDense_diag;
class CDense_seg;
class CSeq_align_set;
class CPubdesc;
class CBioSource;
class COrg_ref;
class CByte_graph;
class CDelta_seq;
class CGene_ref;
class CCdregion;
class CRNA_ref;
class CImp_feat;
class CSeq_literal;
class CBioseq_Handle;
class CSeq_feat_Handle;
class CCountries;
class CInferencePrefixList;
class CComment_set;
class CTaxon3_reply;
class ITaxon3;

BEGIN_SCOPE(validator)

class CValidError_imp;
class CTaxValidationAndCleanup;
class CGeneCache;

// =============================================================================
//                            Caching classes
// =============================================================================

// for convenience
typedef CValidator::CCache CCache;
typedef Char (& TSpliceSite)[2];

/// Cache various information for one validation run.
/// 
/// This is easy because the validator won't
/// change the contents of what it's validating.
/// 
/// Not thread-safe within one CCacheImpl, but it's
/// fine for each thread to have its own CCacheImpl.
class NCBI_VALIDATOR_EXPORT CValidator::CCacheImpl
{
public:

    //////////
    // cache information about pubdescs
    class NCBI_VALIDATOR_EXPORT CPubdescInfo : public CObject
    {
    public:
        vector<int> m_pmids;
        vector<int> m_muids;
        vector<int> m_serials;
        vector<string> m_published_labels;
        vector<string> m_unpublished_labels;
    };
    const CPubdescInfo & GetPubdescToInfo(CConstRef<CPubdesc> pub);

    // TODO: add a function for accessing the cache that fills it in if it's empty

    //////////
    // cache the features on each bioseq
    struct SFeatKey {
        SFeatKey(
        CSeqFeatData::E_Choice feat_type_arg,
            CSeqFeatData::ESubtype feat_subtype_arg,
            const CBioseq_Handle & bioseq_h_arg)
            : feat_type(feat_type_arg),
              feat_subtype(feat_subtype_arg),
              bioseq_h(bioseq_h_arg) { }

        // use AutoPtr so that NULL can represent
        // the concept of "doesn't matter"
        CSeqFeatData::E_Choice feat_type;
        CSeqFeatData::ESubtype feat_subtype;
        CBioseq_Handle bioseq_h;

        bool operator<(const SFeatKey & rhs) const;
        bool operator==(const SFeatKey & rhs) const;
    };
    typedef std::vector<CMappedFeat> TFeatValue;

    // user should go through here since it loads the cache if it's empty
    static const CSeqFeatData::E_Choice kAnyFeatType;
    static const CSeqFeatData::ESubtype kAnyFeatSubtype;
    const TFeatValue & GetFeatFromCache(const SFeatKey & featKey);

    // user should go through here since it loads the cache if it's empty, but:
    // slower than GetFeatFromCache because it iterates over all the
    // features on the bioseq, so try to avoid when possible.
    // All featkeys must point to the same bioseq.
    AutoPtr<TFeatValue> GetFeatFromCacheMulti(
        const vector<SFeatKey> &featKeys);

    // this should be used for any kind of "string -> feats" mapping
    enum EFeatKeyStr{
        eFeatKeyStr_Label,
        eFeatKeyStr_LocusTag,
        // more could be added
    };
    struct SFeatStrKey
    {
        // set bioseq to kAnyBioseq to get data on all bioseqs
        SFeatStrKey(EFeatKeyStr eFeatKeyStr,
                  const CBioseq_Handle & bioseq,
                  const string & feat_str) :
            m_eFeatKeyStr(eFeatKeyStr), m_bioseq(bioseq),
            m_feat_str(feat_str) { }

        EFeatKeyStr m_eFeatKeyStr;
        CBioseq_Handle m_bioseq;
        string m_feat_str;

        bool operator<(const SFeatStrKey & rhs) const;
        bool operator==(const SFeatStrKey & rhs) const;
    };

    // The "tse" is used to load the cache if it's empty
    // (for now just indexes genes, but more may be added in the future)
    static const CBioseq_Handle kAnyBioseq;
    const TFeatValue & GetFeatStrKeyToFeats(
        const SFeatStrKey & feat_str_key, const CTSE_Handle & tse);

    //////////
    // cache the bioseq(s) that each feature is on

    typedef CMappedFeat TFeatToBioseqKey;
    typedef set<CBioseq_Handle> TFeatToBioseqValue;

    // user should go through here since it loads the cache if it's empty
    const TFeatToBioseqValue & GetBioseqsOfFeatCache(
        const TFeatToBioseqKey & feat_to_bioseq_key,
        const CTSE_Handle & tse);

    //////////
    // cache the bioseq that each CSeq-id points to, since the
    // native implementation is too slow for our purposes
    typedef CConstRef<CSeq_id> TIdToBioseqKey;
    typedef CBioseq_Handle TIdToBioseqValue;

    const TIdToBioseqValue & GetIdToBioseq(
        const TIdToBioseqKey & key, const CTSE_Handle & tse);

    // feel free to add more types of caching.  Maybe bioseqs?  Bioseq sets?
    // etc.

    ////////////////////
    // helper functions on top of the basic cache functions above
    ////////////////////

    CBioseq_Handle GetBioseqHandleFromLocation(
        CScope *scope, const CSeq_loc& loc, const CTSE_Handle & tse);

    static const CTSE_Handle kEmptyTSEHandle;

private:
    // values that do not correspond to any legitimate value
    // of the given enum to let us express the concept of "any"
    static const CBioseq_Handle kEmptyBioseqHandle;
    static const TFeatValue kEmptyFeatValue;

    typedef map<CConstRef<CPubdesc>,
                CConstRef<CPubdescInfo> > TPubdescCache;
    TPubdescCache m_pubdescCache;

    typedef map<SFeatKey, TFeatValue> TFeatCache;
    TFeatCache m_featCache;

    typedef map<SFeatStrKey, TFeatValue> TFeatStrKeyToFeatsCache;
    TFeatStrKeyToFeatsCache m_featStrKeyToFeatsCache;

    typedef map<TFeatToBioseqKey, TFeatToBioseqValue> TFeatToBioseqCache;
    TFeatToBioseqCache m_featToBioseqCache;

    typedef map<TIdToBioseqKey, TIdToBioseqValue> TIdToBioseqCache;
    TIdToBioseqCache m_IdToBioseqCache;
};

typedef CValidator::CCacheImpl CCacheImpl;

class CGeneCache {
public:
    CGeneCache() {};
    ~CGeneCache() {};

    CConstRef<CSeq_feat> GetGeneFromCache(const CSeq_feat* feat, CScope& scope);
    CConstRef<CSeq_feat> GetmRNAFromCache(const CSeq_feat* feat, CScope& scope);
    CRef<feature::CFeatTree> GetFeatTreeFromCache(const CSeq_loc& loc, CScope& scope);
    CRef<feature::CFeatTree> GetFeatTreeFromCache(const CSeq_feat& feat, CScope& scope);
    CRef<feature::CFeatTree> GetFeatTreeFromCache(CBioseq_Handle bh);
    void Clear() { m_FeatGeneMap.clear(); m_SeqTreeMap.clear(); }

private:

    typedef map<const CSeq_feat*, CConstRef<CSeq_feat> > TFeatGeneMap;
    TFeatGeneMap            m_FeatGeneMap;
    
    typedef map<CBioseq_Handle, CRef<feature::CFeatTree> > TSeqTreeMap;
    TSeqTreeMap m_SeqTreeMap;

};


// =============================================================================
//                            Validation classes                          
// =============================================================================
class CValidError_desc;
class CValidError_descr;


class CBioSourceKind
{
public:
    CBioSourceKind(const CBioSource& bsrc);
    CBioSourceKind();
    CBioSourceKind& operator=(const CBioSource& bsrc);

    bool IsOrganismBacteria() const;
    bool IsOrganismEukaryote() const;
    bool IsOrganismArchaea() const;
    bool IsSourceOrganelle() const;
    bool IsGood() const;
    void SetNotGood();
protected:
    bool m_bacteria:  1;
    bool m_eukaryote: 1;
    bool m_archaea:   1;
    bool m_organelle: 1;
};

inline
CBioSourceKind::CBioSourceKind()
{
    SetNotGood();
}

inline
CBioSourceKind::CBioSourceKind(const CBioSource& bsrc)
{
    operator=(bsrc);
}

inline
void CBioSourceKind::SetNotGood()
{
   m_bacteria = false;
   m_eukaryote = false;
   m_archaea = false;
   m_organelle = false; 
}

inline
bool CBioSourceKind::IsGood() const
{
    return m_bacteria | m_eukaryote | m_archaea | m_organelle; 
}

inline
bool CBioSourceKind::IsOrganismBacteria() const
{
    return m_bacteria;
}
inline
bool CBioSourceKind::IsOrganismEukaryote() const
{
    return m_eukaryote;
}
inline
bool CBioSourceKind::IsOrganismArchaea() const
{
    return m_archaea;
}
inline
bool CBioSourceKind::IsSourceOrganelle() const
{
    return m_organelle;
}

// ===========================  Central Validation  ==========================

// CValidError_imp provides the entry point to the validation process.
// It calls upon the various validation classes to perform validation of
// each part.
// The class holds all the data for the validation process. 
class NCBI_VALIDATOR_EXPORT CValidError_imp
{
public:
    typedef map<int, int> TCount;

    // Interface to be used by the CValidError class

    CValidError_imp(CObjectManager& objmgr, CValidError* errors,
        Uint4 options = 0);

    // Constructor allowing over-ride of Services
    // Namely, the taxonomy service.
    // NB: ITaxon is owned by CValidator.
    CValidError_imp(CObjectManager& objmgr, CValidError* errors,
        ITaxon3* taxon, Uint4 options = 0);

    // Destructor
    virtual ~CValidError_imp(void);

    void SetOptions (Uint4 options);
    void SetErrorRepository (CValidError* errors);
    void Reset(void);

    // Validation methods
    bool Validate(const CSeq_entry& se, const CCit_sub* cs = 0,
                  CScope* scope = 0);
    bool Validate(
        const CSeq_entry_Handle& seh, const CCit_sub* cs = 0);
    void Validate(
        const CSeq_submit& ss, CScope* scope = 0);
    void Validate(const CSeq_annot_Handle& sa);

    void Validate(const CSeq_feat& feat, CScope* scope = 0);
    void Validate(const CBioSource& src, CScope* scope = 0);
    void Validate(const CPubdesc& pubdesc, CScope* scope = 0);
    void ValidateSubAffil(const CAffil::TStd& std, const CSerialObject& obj, const CSeq_entry *ctx);
    void ValidateAffil(const CAffil::TStd& std, const CSerialObject& obj, const CSeq_entry *ctx);

    bool GetTSANStretchErrors(const CSeq_entry_Handle& se); 
    bool GetTSACDSOnMinusStrandErrors (const CSeq_entry_Handle& se);
    bool GetTSAConflictingBiomolTechErrors (const CSeq_entry_Handle& se);
    bool GetTSANStretchErrors(const CBioseq& seq); 
    bool GetTSACDSOnMinusStrandErrors (const CSeq_feat& f, const CBioseq& seq);
    bool GetTSAConflictingBiomolTechErrors (const CBioseq& seq);


    void SetProgressCallback(CValidator::TProgressCallback callback,
        void* user_data);
public:
    // interface to be used by the various validation classes

    // typedefs:
    typedef const CSeq_feat& TFeat;
    typedef const CBioseq& TBioseq;
    typedef const CBioseq_set& TSet;
    typedef const CSeqdesc& TDesc;
    typedef const CSeq_annot& TAnnot;
    typedef const CSeq_graph& TGraph;
    typedef const CSeq_align& TAlign;
    typedef const CSeq_entry& TEntry;
    typedef map < const CSeq_feat*, const CSeq_annot* >& TFeatAnnotMap;

    // Posts errors.
    void PostErr(EDiagSev sv, EErrType et, const string& msg,
        const CSerialObject& obj);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TDesc ds);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TFeat ft);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TBioseq sq);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TEntry ctx,
        TDesc ds);
    /*void PostErr(EDiagSev sv, EErrType et, const string& msg, TBioseq sq,
        TDesc ds);*/
    /*void PostErr(EDiagSev sv, EErrType et, const string& msg, TSet set, 
        TDesc ds);*/
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TSet set);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TAnnot annot);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TGraph graph);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TBioseq sq,
        TGraph graph);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TAlign align);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TEntry entry);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, const CBioSource& src);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, const COrg_ref& org);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, const CPubdesc& src);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, const CSeq_submit& ss);
    void PostObjErr (EDiagSev sv, EErrType et, const string& msg, const CSerialObject& obj, const CSeq_entry *ctx = 0);
    void PostBadDateError (EDiagSev sv, const string& msg, int flags, const CSerialObject& obj, const CSeq_entry *ctx = 0);

    void HandleTaxonomyError(const CT3Error& error, const string& host, const COrg_ref& orf);
    void HandleTaxonomyError(const CT3Error& error, const EErrType type, const CSeq_feat& feat);
    void HandleTaxonomyError(const CT3Error& error, const EErrType type, const CSeqdesc& desc, const CSeq_entry* entry);

    bool RaiseGenomeSeverity(EErrType et);

    // General use validation methods
    void ValidatePubdesc(const CPubdesc& pub, const CSerialObject& obj, const CSeq_entry *ctx = 0);
    void ValidateBioSource(const CBioSource& bsrc, const CSerialObject& obj, const CSeq_entry *ctx = 0);
    void ValidatePCRReactionSet(const CPCRReactionSet& pcrset, const CSerialObject& obj, const CSeq_entry *ctx = 0);
    void ValidateSubSource(const CSubSource& subsrc, const CSerialObject& obj, const CSeq_entry *ctx = 0);
    void ValidateOrgRef(const COrg_ref& orgref, const CSerialObject& obj, const CSeq_entry *ctx);
    void ValidateOrgName(const COrgName& orgname, const bool has_taxon, const CSerialObject& obj, const CSeq_entry *ctx);
    void ValidateOrgModVoucher(const COrgMod& orgmod, const CSerialObject& obj, const CSeq_entry *ctx);
    void ValidateBioSourceForSeq(const CBioSource& bsrc, const CSerialObject& obj, const CSeq_entry *ctx, const CBioseq_Handle& bsh);
    void ValidateLatLonCountry(string countryname, string lat_lon, const CSerialObject& obj, const CSeq_entry *ctx);

    static bool IsSyntheticConstruct (const CBioSource& src);
    bool IsArtificial (const CBioSource& src);
    static bool IsOrganelle (int genome);
    bool IsOrganelle (CBioseq_Handle seq);
    bool IsOtherDNA(const CBioseq_Handle& bsh) const;
    void ValidateSeqLoc(const CSeq_loc& loc, const CBioseq_Handle& seq, bool report_abutting,
                        const string& prefix, const CSerialObject& obj);
    void ValidateSeqLocIds(const CSeq_loc& loc, const CSerialObject& obj);
    static bool IsInOrganelleSmallGenomeSet(const CSeq_id& id, CScope& scope);
    static bool BadMultipleSequenceLocation(const CSeq_loc& loc, CScope& scope);
    void CheckMultipleIds(const CSeq_loc& loc, const CSerialObject& obj);
    void ValidateDbxref(const CDbtag& xref, const CSerialObject& obj,
    bool biosource = false, const CSeq_entry *ctx = 0);
    void ValidateDbxref(TDbtags& xref_list, const CSerialObject& obj,
    bool biosource = false, const CSeq_entry *ctx = 0);
    void ValidateCitSub(const CCit_sub& cs, const CSerialObject& obj, const CSeq_entry *ctx = 0);
    void ValidateTaxonomy(const CSeq_entry& se); 
    void ValidateSpecificHost(CTaxValidationAndCleanup& tval);
    void ValidateStrain(CTaxValidationAndCleanup& tval);
    void ValidateSpecificHost (const CSeq_entry& se);
    void ValidateTentativeName(const CSeq_entry& se);
    void ValidateTaxonomy(const COrg_ref& org, int genome = CBioSource::eGenome_unknown);
    void ValidateMultipleTaxIds(const CSeq_entry_Handle& seh);
    void ValidateCitations (const CSeq_entry_Handle& seh);
    bool x_IsFarFetchFailure (const CSeq_loc& loc);
		    
    // getters
    inline CScope* GetScope(void) { return m_Scope; }
    inline CCacheImpl & GetCache(void) { return m_cache; }

    inline CConstRef<CSeq_feat> GetCachedGene(const CSeq_feat* f) { return m_GeneCache.GetGeneFromCache(f, *m_Scope); }
    inline CGeneCache& GetGeneCache() { return m_GeneCache; }

    // flags derived from options parameter
    bool IsNonASCII(void)             const { return m_NonASCII; }
    bool IsSuppressContext(void)      const { return m_SuppressContext; }
    bool IsValidateAlignments(void)   const { return m_ValidateAlignments; }
    bool IsValidateExons(void)        const { return m_ValidateExons; }
    bool IsOvlPepErr(void)            const { return m_OvlPepErr; }
    bool IsRequireTaxonID(void)       const { return !m_SeqSubmitParent; }
    bool IsSeqSubmitParent(void)      const { return m_SeqSubmitParent; }
    bool IsRequireISOJTA(void)        const { return m_RequireISOJTA; }
    bool IsValidateIdSet(void)        const { return m_ValidateIdSet; }
    bool IsRemoteFetch(void)          const { return m_RemoteFetch; }
    bool IsFarFetchMRNAproducts(void) const { return m_FarFetchMRNAproducts; }
    bool IsFarFetchCDSproducts(void)  const { return m_FarFetchCDSproducts; }
    bool IsLocusTagGeneralMatch(void) const { return m_LocusTagGeneralMatch; }
    bool DoRubiscoTest(void)          const { return m_DoRubiscoText; }
    bool IsIndexerVersion(void)       const { return m_IndexerVersion; }
    bool IsGenomeSubmission(void)     const { return m_genomeSubmission; }
    bool UseEntrez(void)              const { return m_UseEntrez; }
    bool DoTaxLookup(void)            const { return m_DoTaxLookup; }
    bool ValidateInferenceAccessions(void) const { return m_ValidateInferenceAccessions; }
    bool IgnoreExceptions(void) const { return m_IgnoreExceptions; }
    bool ReportSpliceAsError(void) const { return m_ReportSpliceAsError; }
    bool IsLatLonCheckState(void)     const { return m_LatLonCheckState; }
    bool IsLatLonIgnoreWater(void)    const { return m_LatLonIgnoreWater; }


    // flags calculated by examining data in record
    inline bool IsStandaloneAnnot(void) const { return m_IsStandaloneAnnot; }
    inline bool IsNoPubs(void) const { return m_NoPubs; }
    inline bool IsNoCitSubPubs(void) const { return m_NoCitSubPubs; }
    inline bool IsNoBioSource(void) const { return m_NoBioSource; }
    inline bool IsGPS(void) const { return m_IsGPS; }
    inline bool IsGED(void) const { return m_IsGED; }
    inline bool IsPDB(void) const { return m_IsPDB; }
    inline bool IsPatent(void) const { return m_IsPatent; }
    inline bool IsRefSeq(void) const { return m_IsRefSeq; }
    inline bool IsEmbl(void) const { return m_IsEmbl; }
    inline bool IsDdbj(void) const { return m_IsDdbj; }
    inline bool IsTPE(void) const { return m_IsTPE; }
    inline bool IsNC(void) const { return m_IsNC; }
    inline bool IsNG(void) const { return m_IsNG; }
    inline bool IsNM(void) const { return m_IsNM; }
    inline bool IsNP(void) const { return m_IsNP; }
    inline bool IsNR(void) const { return m_IsNR; }
    inline bool IsNS(void) const { return m_IsNS; }
    inline bool IsNT(void) const { return m_IsNT; }
    inline bool IsNW(void) const { return m_IsNW; }
    inline bool IsWP(void) const { return m_IsWP; }
    inline bool IsXR(void) const { return m_IsXR; }
    inline bool IsGI(void) const { return m_IsGI; }
    inline bool IsGpipe(void) const { return m_IsGpipe; }
    inline bool IsLocalGeneralOnly(void) const { return m_IsLocalGeneralOnly; }
    inline bool HasGiOrAccnVer(void) const { return m_HasGiOrAccnVer; }
    inline bool IsGenomic(void) const { return m_IsGenomic; }
    inline bool IsSeqSubmit(void) const { return m_IsSeqSubmit; }
    inline bool IsSmallGenomeSet(void) const { return m_IsSmallGenomeSet; }
    bool IsNoncuratedRefSeq(const CBioseq& seq, EDiagSev& sev);
    inline bool IsGenbank(void) const { return m_IsGB; }
    inline bool DoesAnyFeatLocHaveGI(void) const { return m_FeatLocHasGI; }
    inline bool DoesAnyProductLocHaveGI(void) const { return m_ProductLocHasGI; }
    inline bool DoesAnyGeneHaveLocusTag(void) const { return m_GeneHasLocusTag; }
    inline bool DoesAnyProteinHaveGeneralID(void) const { return m_ProteinHasGeneralID; }
    inline bool IsINSDInSep(void) const { return m_IsINSDInSep; }
    inline bool IsGeneious(void) const { return m_IsGeneious; }
    inline const CBioSourceKind& BioSourceKind() const { return m_biosource_kind; }

    // counting number of misplaced features
    inline void ResetMisplacedFeatureCount (void) { m_NumMisplacedFeatures = 0; }
    inline void IncrementMisplacedFeatureCount (void) { m_NumMisplacedFeatures++; }
    inline void AddToMisplacedFeatureCount (SIZE_TYPE num) { m_NumMisplacedFeatures += num; }

    // counting number of small genome set misplaced features
    inline void ResetSmallGenomeSetMisplacedCount (void) { m_NumSmallGenomeSetMisplaced = 0; }
    inline void IncrementSmallGenomeSetMisplacedCount (void) { m_NumSmallGenomeSetMisplaced++; }
    inline void AddToSmallGenomeSetMisplacedCount (SIZE_TYPE num) { m_NumSmallGenomeSetMisplaced += num; }

    // counting number of misplaced graphs
    inline void ResetMisplacedGraphCount (void) { m_NumMisplacedGraphs = 0; }
    inline void IncrementMisplacedGraphCount (void) { m_NumMisplacedGraphs++; }
    inline void AddToMisplacedGraphCount (SIZE_TYPE num) { m_NumMisplacedGraphs += num; }

    // counting number of genes and gene xrefs
    inline void ResetGeneCount (void) { m_NumGenes = 0; }
    inline void IncrementGeneCount (void) { m_NumGenes++; }
    inline void AddToGeneCount (SIZE_TYPE num) { m_NumGenes += num; }
    inline void ResetGeneXrefCount (void) { m_NumGeneXrefs = 0; }
    inline void IncrementGeneXrefCount (void) { m_NumGeneXrefs++; }
    inline void AddToGeneXrefCount (SIZE_TYPE num) { m_NumGeneXrefs += num; }

    // counting sequences with and without TPA history
    inline void ResetTpaWithHistoryCount (void) { m_NumTpaWithHistory = 0; }
    inline void IncrementTpaWithHistoryCount (void) { m_NumTpaWithHistory++; }
    inline void AddToTpaWithHistoryCount (SIZE_TYPE num) { m_NumTpaWithHistory += num; }
    inline void ResetTpaWithoutHistoryCount (void) { m_NumTpaWithoutHistory = 0; }
    inline void IncrementTpaWithoutHistoryCount (void) { m_NumTpaWithoutHistory++; }
    inline void AddToTpaWithoutHistoryCount (SIZE_TYPE num) { m_NumTpaWithoutHistory += num; }

    // counting number of Pseudos and Pseudogenes
    inline void ResetPseudoCount (void) { m_NumPseudo = 0; }
    inline void IncrementPseudoCount (void) { m_NumPseudo++; }
    inline void AddToPseudoCount (SIZE_TYPE num) { m_NumPseudo += num; }
    inline void ResetPseudogeneCount (void) { m_NumPseudogene = 0; }
    inline void IncrementPseudogeneCount (void) { m_NumPseudogene++; }
    inline void AddToPseudogeneCount (SIZE_TYPE num) { m_NumPseudogene += num; }

    // set flag for farfetchfailure
    inline void SetFarFetchFailure (void) { m_FarFetchFailure = true; }

    const CSeq_entry& GetTSE(void) { return *m_TSE; }
    const CSeq_entry_Handle & GetTSEH(void) { return m_TSEH; }
    const CTSE_Handle & GetTSE_Handle(void) { return
            (m_TSEH ? m_TSEH.GetTSE_Handle() : CCacheImpl::kEmptyTSEHandle); }

    void AddBioseqWithNoPub(const CBioseq& seq);
    void AddBioseqWithNoBiosource(const CBioseq& seq);
    void AddProtWithoutFullRef(const CBioseq_Handle& seq);
    static bool IsWGSIntermediate(const CBioseq& seq);
    static bool IsWGSIntermediate(const CSeq_entry& se);
    static bool IsTSAIntermediate(const CBioseq& seq);
    static bool IsTSAIntermediate(const CSeq_entry& se);
    void ReportMissingPubs(const CSeq_entry& se, const CCit_sub* cs);
    void ReportMissingBiosource(const CSeq_entry& se);

    bool IsNucAcc(const string& acc);
    CConstRef<CSeq_feat> GetCDSGivenProduct(const CBioseq& seq);
    CConstRef<CSeq_feat> GetmRNAGivenProduct(const CBioseq& seq);
    const CSeq_entry* GetAncestor(const CBioseq& seq, CBioseq_set::EClass clss);
    bool IsSerialNumberInComment(const string& comment);

    bool CheckSeqVector(const CSeqVector& vec);
    bool IsSequenceAvaliable(const CSeqVector& vec);

    bool IsTransgenic(const CBioSource& bsrc);

private:

    // Setup common options during consturction;
    void x_Init(Uint4 options);

    // This is so we can temporarily set m_Scope in a function
    // and be sure that it will be set to its old value when we're done
    class CScopeRestorer {
    public:
        CScopeRestorer( CRef<CScope> &scope ) : 
          m_scopeToRestore(scope), m_scopeOriginalValue(scope) { }

        ~CScopeRestorer(void) { m_scopeToRestore = m_scopeOriginalValue; }
    private:
        CRef<CScope> &m_scopeToRestore;
        CRef<CScope> m_scopeOriginalValue;
    };

    // Prohibit copy constructor & assignment operator
    CValidError_imp(const CValidError_imp&);
    CValidError_imp& operator= (const CValidError_imp&);

    void Setup(const CSeq_entry_Handle& seh);
    void Setup(const CSeq_annot_Handle& sa);
    CSeq_entry_Handle Setup(const CBioseq& seq);
    void SetScope(const CSeq_entry& se);

    void ValidateSubmitBlock(const CSubmit_block& block, const CSeq_submit& ss);

    void InitializeSourceQualTags();
    void ValidateSourceQualTags(const string& str, const CSerialObject& obj, const CSeq_entry *ctx = 0);

    bool IsMixedStrands(const CSeq_loc& loc);

    void ValidatePubGen(const CCit_gen& gen, const CSerialObject& obj, const CSeq_entry *ctx = 0);
    void ValidatePubArticle(const CCit_art& art, int uid, const CSerialObject& obj, const CSeq_entry *ctx = 0);
    void ValidatePubArticleNoPMID(const CCit_art& art, const CSerialObject& obj, const CSeq_entry *ctx = 0);
    void x_ValidatePages(const string& pages, const CSerialObject& obj, const CSeq_entry *ctx = 0);
    void ValidateAuthorList(const CAuth_list::C_Names& names, const CSerialObject& obj, const CSeq_entry *ctx = 0);
    void ValidateAuthorsInPubequiv (const CPub_equiv& pe, const CSerialObject& obj, const CSeq_entry *ctx = 0);
    void ValidatePubHasAuthor(const CPubdesc& pubdesc, const CSerialObject& obj, const CSeq_entry *ctx = 0);
        
    bool HasName(const CAuth_list& authors);
    bool HasTitle(const CTitle& title);
    bool HasIsoJTA(const CTitle& title);

    bool ValidateDescriptorInSeqEntry (const CSeq_entry& se, CValidError_desc * descval);
    bool ValidateDescriptorInSeqEntry (const CSeq_entry& se);

    bool ValidateSeqDescrInSeqEntry (const CSeq_entry& se, CValidError_descr *descr_val);
    bool ValidateSeqDescrInSeqEntry (const CSeq_entry& se);

    void x_CheckSingleStrandedRNAViruses
        (const CBioSource& source,
        const CSerialObject& obj,
        const CSeq_entry    *ctx,
        const CBioseq_Handle& bsh,
        const CMolInfo& molinfo);

    void FindEmbeddedScript(const CSerialObject& obj);
    void FindNonAsciiText (const CSerialObject& obj);
    void FindCollidingSerialNumbers (const CSerialObject& obj);


    void GatherTentativeName (const CSeq_entry& se, vector<CConstRef<CSeqdesc> >& usr_descs, vector<CConstRef<CSeq_entry> >& desc_ctxs, vector<CConstRef<CSeq_feat> >& usr_feats);

    bool x_CheckPackedInt(const CPacked_seqint& packed_int,
                          CConstRef<CSeq_id>& id_cur,
                          CConstRef<CSeq_id>& id_prv,
                          ENa_strand& strand_cur,
                          ENa_strand& strand_prv,
                          CConstRef<CSeq_interval>& int_cur,
                          CConstRef<CSeq_interval>& int_prv,
                          const CSerialObject& obj);
    bool x_CheckSeqInt(CConstRef<CSeq_id>& id_cur,
                       const CSeq_interval * int_cur,
                       ENa_strand& strand_cur,
                       const CSerialObject& obj);
    void x_ReportInvalidFuzz(const CPacked_seqint& packed_int, const CSerialObject& obj);
    void x_ReportInvalidFuzz(const CSeq_interval& interval, const CSerialObject& obj);
    void x_ReportInvalidFuzz(const CSeq_point& point, const CSerialObject& obj);
    void x_ReportInvalidFuzz(const CSeq_loc& loc, const CSerialObject& obj);
    void x_ReportPCRSeqProblem(const string& primer_kind,
                               char badch,
                               const CSerialObject& obj,
                               const CSeq_entry *ctx);
    void x_CheckPCRPrimer(const CPCRPrimer& primer,
                          const string& primer_kind,
                          const CSerialObject& obj,
                          const CSeq_entry *ctx);

    void x_DoBarcodeTests(CSeq_entry_Handle seh);

    bool x_IsRepliconSubSource(CSubSource::TSubtype subtype) const;
    bool x_IsRepliconSubSourceValid(const string& val) const;

    CRef<CObjectManager>    m_ObjMgr;
    CRef<CScope>            m_Scope;
    CConstRef<CSeq_entry>   m_TSE;
    CSeq_entry_Handle       m_TSEH;

    CCacheImpl              m_cache;
    CGeneCache              m_GeneCache;

    // error repoitory
    CValidError*       m_ErrRepository;

    // flags derived from options parameter
    bool m_NonASCII;             // User sets if Non ASCII char found
    bool m_SuppressContext;      // Include context in errors if true
    bool m_ValidateAlignments;   // Validate Alignments if true
    bool m_ValidateExons;        // Check exon feature splice sites
    bool m_OvlPepErr;            // Peptide overlap error if true, else warn
    bool m_RequireISOJTA;        // Journal requires ISO JTA
    bool m_ValidateIdSet;        // validate update against ID set in database
    bool m_RemoteFetch;          // Remote fetch enabled?
    bool m_FarFetchMRNAproducts; // Remote fetch mRNA products
    bool m_FarFetchCDSproducts;  // Remote fetch proteins
    bool m_LatLonCheckState;
    bool m_LatLonIgnoreWater;
    bool m_LocusTagGeneralMatch;
    bool m_DoRubiscoText;
    bool m_IndexerVersion;
    bool m_genomeSubmission;
    bool m_UseEntrez;
    bool m_IgnoreExceptions;             // ignore exceptions when validating translation
    bool m_ValidateInferenceAccessions;  // check that accessions in inferences are valid
    bool m_ReportSpliceAsError;
    bool m_DoTaxLookup;
    bool m_DoBarcodeTests;
    bool m_SeqSubmitParent; // some errors are suppressed if this is run on a newly created submission

    // flags calculated by examining data in record
    bool m_IsStandaloneAnnot;
    bool m_NoPubs;                  // Suppress no pub error if true
    bool m_NoCitSubPubs;            // Suppress no cit-sub pub error if true
    bool m_NoBioSource;             // Suppress no organism error if true
    bool m_IsGPS;
    bool m_IsGED;
    bool m_IsPDB;
    bool m_IsPatent;
    bool m_IsRefSeq;
    bool m_IsEmbl;
    bool m_IsDdbj;
    bool m_IsTPE;
    bool m_IsNC;
    bool m_IsNG;
    bool m_IsNM;
    bool m_IsNP;
    bool m_IsNR;
    bool m_IsNS;
    bool m_IsNT;
    bool m_IsNW;
    bool m_IsWP;
    bool m_IsXR;
    bool m_IsGI;
    bool m_IsGB;
    bool m_IsGpipe;
    bool m_IsLocalGeneralOnly;
    bool m_HasGiOrAccnVer;
    bool m_IsGenomic;
    bool m_IsSeqSubmit;
    bool m_IsSmallGenomeSet;
    bool m_FeatLocHasGI;
    bool m_ProductLocHasGI;
    bool m_GeneHasLocusTag;
    bool m_ProteinHasGeneralID;
    bool m_IsINSDInSep;
    bool m_FarFetchFailure;
    bool m_IsGeneious;

    CBioSourceKind m_biosource_kind;

    bool m_IsTbl2Asn;

    // seq ids contained within the orignal seq entry. 
    // (used to check for far location)
    vector< CConstRef<CSeq_id> >    m_InitialSeqIds;
    // Bioseqs without source (should be considered only if m_NoSource is false)
    vector< CConstRef<CBioseq> >    m_BioseqWithNoSource;

    // list of publication serial numbers
    vector< int > m_PubSerialNumbers;

    // legal dbxref database strings
    static const string legalDbXrefs[];
    static const string legalRefSeqDbXrefs[];

    // source qulalifiers prefixes
    static const string sm_SourceQualPrefixes[];
    static auto_ptr<CTextFsa> m_SourceQualTags;

    CValidator::TProgressCallback m_PrgCallback;
    CValidator::CProgressInfo     m_PrgInfo;
    SIZE_TYPE   m_NumAlign;
    SIZE_TYPE   m_NumAnnot;
    SIZE_TYPE   m_NumBioseq;
    SIZE_TYPE   m_NumBioseq_set;
    SIZE_TYPE   m_NumDesc;
    SIZE_TYPE   m_NumDescr;
    SIZE_TYPE   m_NumFeat;
    SIZE_TYPE   m_NumGraph;

    SIZE_TYPE   m_NumMisplacedFeatures;
    SIZE_TYPE   m_NumSmallGenomeSetMisplaced;
    SIZE_TYPE   m_NumMisplacedGraphs;
    SIZE_TYPE   m_NumGenes;
    SIZE_TYPE   m_NumGeneXrefs;

    SIZE_TYPE   m_NumTpaWithHistory;
    SIZE_TYPE   m_NumTpaWithoutHistory;

    SIZE_TYPE   m_NumPseudo;
    SIZE_TYPE   m_NumPseudogene;

    // Taxonomy service interface.
    ITaxon3* m_taxon;
    ITaxon3* x_GetTaxonService();

};


// =============================================================================
//                         Specific validation classes
// =============================================================================



class CValidError_base
{
protected:
    // typedefs:
    typedef CValidError_imp::TFeat TFeat;
    typedef CValidError_imp::TBioseq TBioseq;
    typedef CValidError_imp::TSet TSet;
    typedef CValidError_imp::TDesc TDesc;
    typedef CValidError_imp::TAnnot TAnnot;
    typedef CValidError_imp::TGraph TGraph;
    typedef CValidError_imp::TAlign TAlign;
    typedef CValidError_imp::TEntry TEntry;

    CValidError_base(CValidError_imp& imp);
    virtual ~CValidError_base();

    void PostErr(EDiagSev sv, EErrType et, const string& msg,
        const CSerialObject& obj);
    //void PostErr(EDiagSev sv, EErrType et, const string& msg, TDesc ds);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TFeat ft);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TBioseq sq);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TEntry ctx,
        TDesc ds);
    /*void PostErr(EDiagSev sv, EErrType et, const string& msg, TBioseq sq,
        TDesc ds);*/
    /*void PostErr(EDiagSev sv, EErrType et, const string& msg, TSet set, 
        TDesc ds);*/
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TSet set);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TAnnot annot);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TGraph graph);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TBioseq sq,
        TGraph graph);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TAlign align);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TEntry entry);

    CCacheImpl & GetCache(void);

    CValidError_imp& m_Imp;
    CScope* m_Scope;
};


// ============================  Validate SeqGraph  ============================


class CValidError_graph : private CValidError_base
{
public:
    CValidError_graph(CValidError_imp& imp);
    virtual ~CValidError_graph(void);

    void ValidateSeqGraph(const CSeq_graph& graph);
    void ValidateSeqGraph(CSeq_entry_Handle seh);
    void ValidateSeqGraph(const CBioseq& seq);
    void ValidateSeqGraph(const CBioseq_set& set);

    void ValidateSeqGraphContext (const CSeq_graph& graph, const CBioseq_set& set);
    void ValidateSeqGraphContext (const CSeq_graph& graph, const CBioseq& seq);
   

private:

};


// ============================  Validate SeqAlign  ============================


class CValidError_align : private CValidError_base
{
public:
    CValidError_align(CValidError_imp& imp);
    virtual ~CValidError_align(void);

    void ValidateSeqAlign(const CSeq_align& align, int order = -1);

    struct TSegmentGap {
        size_t seg_num; size_t align_pos; string label;
        TSegmentGap(size_t _seg_num, size_t _align_pos, string _label)
            : seg_num(_seg_num), align_pos(_align_pos), label(_label) {}
    };
    typedef vector<TSegmentGap> TSegmentGapV;

    typedef CSeq_align::C_Segs::TDendiag    TDendiag;
    typedef CSeq_align::C_Segs::TDenseg     TDenseg;
    typedef CSeq_align::C_Segs::TPacked     TPacked;
    typedef CSeq_align::C_Segs::TStd        TStd;

    static TSegmentGapV FindSegmentGaps(const TPacked& packed, CScope* scope);
    static TSegmentGapV FindSegmentGaps(const TDenseg& std_segs, CScope* scope);
    static TSegmentGapV FindSegmentGaps(const TStd& denseg, CScope* scope);
    static TSegmentGapV FindSegmentGaps(const TDendiag& dendiags, CScope* scope);

private:
    typedef CSeq_align::C_Segs::TDisc       TDisc;

    void x_ValidateAlignPercentIdentity (const CSeq_align& align, bool internal_gaps);
    static bool AlignmentScorePercentIdOk(const CSeq_align& align);
    static bool IsTpaAlignment(const CDense_seg& denseg, CScope& scope);
    static bool IsTpaAlignment(const CSparseAln& sparse_aln, CScope& scope);
    void x_ValidateDendiag(const TDendiag& dendiags, const CSeq_align& align);
    void x_ValidateDenseg(const TDenseg& denseg, const CSeq_align& align);
    void x_ValidateStd(const TStd& stdsegs, const CSeq_align& align);
    void x_ValidatePacked(const TPacked& packed, const CSeq_align& align);

    // Check if dimension is valid
    template <typename T>
    bool x_ValidateDim(T& obj, const CSeq_align& align, size_t part = 0);

    // Check if the  strand is consistent in SeqAlignment of global 
    // or partial type
    void x_ValidateStrand(const TDenseg& denseg, const CSeq_align& align);
    void x_ValidateStrand(const TStd& std_segs, const CSeq_align& align);

    // Check if an alignment is FASTA-like. 
    // Alignment is FASTA-like if all gaps are at the end with dimensions > 2.
    void x_ValidateFastaLike(const TDenseg& denseg, const CSeq_align& align);

    // Check if there is a gap for all sequences in a segment.
    void x_ReportSegmentGaps(const TSegmentGapV& seggaps, const CSeq_align& align);
    void x_ValidateSegmentGap(const TDenseg& denseg, const CSeq_align& align);
    void x_ValidateSegmentGap(const TPacked& packed, const CSeq_align& align);
    void x_ValidateSegmentGap(const TStd& std_segs, const CSeq_align& align);
    void x_ValidateSegmentGap(const TDendiag& dendiags, const CSeq_align& align);

    // Validate SeqId in sequence alignment.
    void x_ValidateSeqId(const CSeq_align& align);
    void x_GetIds(const CSeq_align& align, vector< CRef< CSeq_id > >& ids);

    // Check segment length, start and end point in Dense_seg, Dense_diag 
    // and Std_seg
    void x_ReportAlignErr(const CSeq_align& align, const CSeq_id& id, const CSeq_id& id_context,
                          size_t segment, size_t pos,
                          EErrType et, EDiagSev sev, const string& prefix, const string& message);
    void x_ReportSumLenStart(const CSeq_align& align, const CSeq_id& id, const CSeq_id& id_context, size_t segment, size_t pos);
    void x_ReportStartMoreThanBiolen(const CSeq_align& align, const CSeq_id& id, const CSeq_id& id_context, size_t segment, size_t pos);
    void x_ValidateSeqLength(const TDenseg& denseg, const CSeq_align& align);
    void x_ValidateSeqLength(const TPacked& packed, const CSeq_align& align);
    void x_ValidateSeqLength(const TStd& std_segs, const CSeq_align& align);
    void x_ValidateSeqLength(const CDense_diag& dendiag, size_t dendiag_num,
        const CSeq_align& align);
};


// =============================  Validate SeqFeat  ============================


class NCBI_VALIDATOR_EXPORT CValidError_feat : private CValidError_base
{
public:
    CValidError_feat(CValidError_imp& imp);
    virtual ~CValidError_feat(void);

    void ValidateSeqFeat(
        const CSeq_feat& feat);
    void ValidateSeqFeatContext(const CSeq_feat& feat, const CBioseq& seq);

    enum EInferenceValidCode {
        eInferenceValidCode_valid = 0,
        eInferenceValidCode_empty,
        eInferenceValidCode_bad_prefix,
        eInferenceValidCode_bad_body,
        eInferenceValidCode_single_field,
        eInferenceValidCode_spaces,
        eInferenceValidCode_comment,
        eInferenceValidCode_same_species_misused,
        eInferenceValidCode_bad_accession,
        eInferenceValidCode_bad_accession_version,
        eInferenceValidCode_accession_version_not_public,
        eInferenceValidCode_bad_accession_type,
        eInferenceValidCode_unrecognized_database
    };

    static vector<string> GetAccessionsFromInferenceString (string inference, string &prefix, string &remainder, bool &same_species);
    static bool GetPrefixAndAccessionFromInferenceAccession (string inf_accession, string &prefix, string &accession);
    static EInferenceValidCode ValidateInferenceAccession (string accession, bool fetch_accession, bool is_similar_to);
    static EInferenceValidCode ValidateInference(string inference, bool fetch_accession);

    // functions expected to be used in Discrepancy Report
    bool DoesCDSHaveShortIntrons(const CSeq_feat& feat);
    bool IsIntronShort(const CSeq_feat& feat);
    void ValidatemRNAGene (const CSeq_feat &feat);
    bool GetTSACDSOnMinusStrandErrors(const CSeq_feat& feat, const CBioseq& seq);
    static void TranslateTripletIntrons (const CSeq_feat& feat, const CCdregion& cdr, bool &nonsense_intron, CSeq_loc& nonsense_intron_loc, CScope *scope);

private:
    void x_ValidateSeqFeatLoc(const CSeq_feat& feat);
    void x_CheckLocForGaps(const CSeq_loc& loc);
    size_t x_FindStartOfGap (CBioseq_Handle bsh, int pos);
    void ValidateSeqFeatData(const CSeqFeatData& data, const CSeq_feat& feat);
    void ValidateSeqFeatProduct(const CSeq_loc& prod, const CSeq_feat& feat);
    void ValidateGene(const CGene_ref& gene, const CSeq_feat& feat);
    void ValidateGeneXRef(const CSeq_feat& feat);
    void ValidateGeneFeaturePair(const CSeq_feat& feat, const CSeq_feat& gene);
    bool x_FindProteinGeneXrefByKey(CBioseq_Handle bsh, const string& key);
    bool FindGeneToMatchGeneXref(const CGene_ref& xref, CSeq_entry_Handle seh);
    void ValidateOperon(const CSeq_feat& feat);
    void ValidateGeneCdsPair(const CSeq_feat& gene);

    void ValidateCdregion(const CCdregion& cdregion, const CSeq_feat& obj);
    void ValidateCdTrans(const CSeq_feat& feat, bool &nonsense_intron);
    void x_ReportUnnecessaryAlternativeStartCodonException(const CSeq_feat& feat);
    static void CheckForThreeBaseNonsense (const CSeq_feat& feat, const CSeq_id& id, const CCdregion& cdr, TSeqPos start, TSeqPos stop, ENa_strand strand, bool &nonsense_intron, CSeq_loc& intron_loc, CScope *scope);
    void ValidateCdRegionTranslation (const CSeq_feat& feat, const string& transl_prot, bool report_errors, bool unclassified_except, bool& has_errors, bool& other_than_mismatch, bool &nonsense_intron);
    void x_GetExceptionFlags
        (const string& except_text,
         bool& unclassified_except,
         bool& mismatch_except,
         bool& frameshift_except,
         bool& rearrange_except,
         bool& product_replaced,
         bool& mixed_population,
         bool& low_quality,
         bool& rna_editing,
         bool& transcript_or_proteomic);
    void x_CheckCDSFrame
        (const CSeq_feat& feat,
         const bool report_errors,
         bool& has_errors,
         bool& other_than_mismatch);
    CBioseq_Handle x_GetCDSProduct
        (const CSeq_feat& feat,
         bool report_errors,
         size_t translation_length,
         string& farstr,
         bool& has_errors,
         bool& other_than_mismatch);

    size_t x_CountTerminalXs(const string& transl_prot, bool skip_stop);
    size_t x_CountTerminalXs(const CSeqVector& prot_vec);

    void x_CheckTranslationMismatches
        (const CSeq_feat& feat,
         CBioseq_Handle prot_handle,
         const string& transl_prot,
         const string& gccode,
         const string& farstr,
         bool report_errors,
         bool got_stop,
         bool rna_editing,
         bool mismatch_except,
         bool unclassified_except,
         size_t& len,
         size_t& num_mismatches,
         bool& no_product,
         bool& show_stop,
         bool& has_errors,
         bool& other_than_mismatch);
    void ValidateCdsProductId(const CSeq_feat& feat);
    void ValidateCdConflict(const CCdregion& cdregion, const CSeq_feat& feat);
    void ReportCdTransErrors(const CSeq_feat& feat,
        bool show_stop, bool got_stop,
        bool report_errors, bool& has_errors);

    EDiagSev x_SeverityForConsensusSplice(void);
    void ValidateDonor (ENa_strand strand, TSeqPos stop, const CSeqVector& vec, TSeqPos seq_len,
                        bool rare_consensus_not_expected, 
                        const string& label, bool report_errors, bool &has_errors, const CSeq_feat& feat, bool is_last);
    void ValidateAcceptor (ENa_strand strand, TSeqPos start, const CSeqVector& vec, TSeqPos seq_len,
                           bool rare_consensus_not_expected, 
                           const string& label, bool report_errors, bool &has_errors, const CSeq_feat& feat, bool is_first);
    void ValidateSplice(const CSeq_feat& feat, bool check_all = false);
    void ValidateBothStrands(const CSeq_feat& feat);
    void ValidateCommonCDSProduct(const CSeq_feat& feat);
    void ValidateBadMRNAOverlap(const CSeq_feat& feat);
    void ValidateCDSPartial(const CSeq_feat& feat);
    bool x_ValidateCodeBreakNotOnCodon(const CSeq_feat& feat,const CSeq_loc& loc,
        const CCdregion& cdregion, bool report_erros);
    void x_ValidateCdregionCodebreak(const CCdregion& cds, const CSeq_feat& feat);

    void ValidateProt(const CProt_ref& prot, const CSeq_feat& feat);
    void x_ValidateProteinName(const string& prot_name, const CSeq_feat& feat);
    void x_ReportUninformativeNames(const CProt_ref& prot, const CSeq_feat& feat);
    void x_ValidateProtECNumbers(const CProt_ref& prot, const CSeq_feat& feat);

    void ValidateRna(const CRNA_ref& rna, const CSeq_feat& feat);
    void ValidateAnticodon(const CSeq_loc& anticodon, const CSeq_feat& feat);
    void ValidateTrnaCodons(const CTrna_ext& trna, const CSeq_feat& feat, bool mustbemethionine);
    void ValidateMrnaTrans(const CSeq_feat& feat);
    void ValidateCommonMRNAProduct(const CSeq_feat& feat);
    void ValidateRnaProductType(const CRNA_ref& rna, const CSeq_feat& feat);
    void ValidateIntron(const CSeq_feat& feat);

    void ValidateImp(const CImp_feat& imp, const CSeq_feat& feat);
    void ValidateNonImpFeat (const CSeq_feat& feat);
    void ValidateImpGbquals(const CImp_feat& imp, const CSeq_feat& feat);
    void ValidateNonImpFeatGbquals(const CSeq_feat& feat);

    // these functions are for validating individual genbank qualifier values
    void ValidateRptUnitVal (const string& val, const string& key, const CSeq_feat& feat);
    void ValidateRptUnitSeqVal (const string& val, const string& key, const CSeq_feat& feat);
    void ValidateRptUnitRangeVal (const string& val, const CSeq_feat& feat);
    void ValidateLabelVal (const string& val, const CSeq_feat& feat);
    void ValidateCompareVal (const string& val, const CSeq_feat& feat);

    void ValidateGapFeature (const CSeq_feat& feat);

    void ValidatePeptideOnCodonBoundry(const CSeq_feat& feat, 
        const string& key);

    bool SplicingNotExpected(const CSeq_feat& feat);
    void ValidateFeatPartialness(const CSeq_feat& feat);
    void ValidateExcept(const CSeq_feat& feat);
    void ValidateExceptText(const string& text, const CSeq_feat& feat);
    void ValidateSeqFeatXref (const CSeqFeatXref& xref, const CSeq_feat& feat);
    // does feat have an xref to a feature other than the one specified by id with the same subtype
    static bool HasNonReciprocalXref(const CSeq_feat& feat, 
                                     const CFeat_id& id, CSeqFeatData::ESubtype subtype,
                                     const CTSE_Handle& tse);
    void ValidateOneFeatXrefPair(const CSeq_feat& feat, const CSeq_feat& far_feat, const CSeqFeatXref& xref);
    void ValidateExtUserObject (const CUser_object& user_object, const CSeq_feat& feat);
    void ValidateGoTerms (CUser_object::TData field_list, const CSeq_feat& feat, vector<pair<string, string> >& id_terms);

    void ValidateFeatCit(const CPub_set& cit, const CSeq_feat& feat);
    void ValidateFeatComment(const string& comment, const CSeq_feat& feat);
    void ValidateFeatBioSource(const CBioSource& bsrc, const CSeq_feat& feat);

    void x_ValidateGbQual(const CGb_qual& qual, const CSeq_feat& feat);

    bool IsPlastid(int genome);
    bool IsOverlappingGenePseudo(const CSeq_feat& feat, CScope* scope);
    unsigned char Residue(unsigned char res);

    static int CheckForRaggedEnd(const CSeq_feat& feat, CScope* scope);
    static int CheckForRaggedEnd(const CSeq_loc&, const CCdregion& cdr, CScope* scope);
    bool SuppressCheck(const string& except_text);
    string MapToNTCoords(const CSeq_feat& feat, const CSeq_loc& product,
        TSeqPos pos);

    bool Is5AtEndSpliceSiteOrGap(const CSeq_loc& loc);

    void ValidateCharactersInField (string value, string field_name, const CSeq_feat& feat);
    void x_ReportECNumFileStatus(const CSeq_feat& feat);

    void ValidateSpliceCdregion(const CSeq_feat& feat, const CBioseq_Handle& bsh, ENa_strand strand, int num_parts, bool report_errors, bool& has_errors);
    void ValidateSpliceMrna(const CSeq_feat& feat, const CBioseq_Handle& bsh, ENa_strand strand, int num_parts, bool report_errors, bool& has_errors);
    void ValidateSpliceExon(const CSeq_feat& feat, const CBioseq_Handle& bsh, ENa_strand strand, int num_parts, bool report_errors, bool& has_errors);

    void ValidateDonorAcceptorPair(ENa_strand strand, TSeqPos stop, const CSeqVector& vec_donor, TSeqPos seq_len_donor,
                                     TSeqPos start, const CSeqVector& vec_acceptor,  TSeqPos seq_len_acceptor,
                                     bool rare_consensus_not_expected, const string& label, bool report_errors, bool& has_errors, const CSeq_feat& feat);
    bool ReadDonorSpliceSite(ENa_strand strand, TSeqPos stop, const CSeqVector& vec, TSeqPos seq_len, const string& label, bool report_errors, bool& has_errors, const CSeq_feat& feat, TSpliceSite site);
    bool ReadAcceptorSpliceSite(ENa_strand strand, TSeqPos stop, const CSeqVector& vec, TSeqPos seq_len, const string& label, bool report_errors, bool& has_errors, const CSeq_feat& feat, TSpliceSite site);
};


// ============================  Validate SeqAnnot  ============================


class CValidError_annot : private CValidError_base
{
public:
    CValidError_annot(CValidError_imp& imp);
    virtual ~CValidError_annot(void);

    void ValidateSeqAnnot(const CSeq_annot_Handle& annot);

    void ValidateSeqAnnot (const CSeq_annot& annot);
    void ValidateSeqAnnotContext(const CSeq_annot& annot, const CBioseq& seq);
    void ValidateSeqAnnotContext(const CSeq_annot& annot, const CBioseq_set& set);
    bool IsLocationUnindexed(const CSeq_loc& loc);
    void ReportLocationGI0(const CSeq_feat& f, const string& label);

private:
    CValidError_graph m_GraphValidator;
    CValidError_align m_AlignValidator;
    CValidError_feat m_FeatValidator;

};


// =============================  Validate SeqDesc  ============================

class NCBI_VALIDATOR_EXPORT CValidError_desc : private CValidError_base
{
public:
    CValidError_desc(CValidError_imp& imp);
    virtual ~CValidError_desc(void);

    void ValidateSeqDesc(const CSeqdesc& desc, const CSeq_entry& ctx);

    bool ValidateStructuredComment(const CUser_object& usr, const CSeqdesc& desc, bool report = true);
    bool ValidateDblink(const CUser_object& usr, const CSeqdesc& desc, bool report = true);

    void ResetModifCounters(void);
private:

    void ValidateComment(const string& comment, const CSeqdesc& desc);
    void ValidateTitle(const string& title, const CSeqdesc& desc, const CSeq_entry& ctx);
    bool ValidateStructuredComment(const CUser_object& usr, const CSeqdesc& desc, const CComment_rule& rule, bool report);
    bool ValidateStructuredCommentGeneric(const CUser_object& usr, const CSeqdesc& desc, bool report);
    void x_ReportStructuredCommentErrors(const CSeqdesc& desc, const CComment_rule::TErrorList& errors);
    void ValidateUser(const CUser_object& usr, const CSeqdesc& desc);
    void ValidateMolInfo(const CMolInfo& minfo, const CSeqdesc& desc);

    CConstRef<CSeq_entry> m_Ctx;
};


// ============================  Validate SeqDescr  ============================


class CValidError_descr : private CValidError_base
{
public:
    CValidError_descr(CValidError_imp& imp);
    virtual ~CValidError_descr(void);

    void ValidateSeqDescr(const CSeq_descr& descr, const CSeq_entry& ctx);
    bool ValidateStructuredComment(const CUser_object& usr, const CSeqdesc& desc, bool report);
private:

    CValidError_desc m_DescValidator;
};


// =============================  Validate Bioseq  =============================

// internal structures
class CCdsMatchInfo;
class CMrnaMatchInfo;
class CValidError_bioseq : private CValidError_base
{
public:
    CValidError_bioseq(CValidError_imp& imp);
    virtual ~CValidError_bioseq(void);

    void ValidateBioseq(const CBioseq& seq);
    void ValidateSeqIds(const CBioseq& seq);
    void ValidateSeqId(const CSeq_id& id, const CBioseq& ctx);
    void ValidateInst(const CBioseq& seq);
    void ValidateBioseqContext(const CBioseq& seq);
    void ValidateHistory(const CBioseq& seq);
    bool GetTSANStretchErrors(const CBioseq& seq);
    bool GetTSAConflictingBiomolTechErrors(const CBioseq& seq);
    static bool IsSelfReferential(const CBioseq& seq);
    static bool IsAllNs(CBioseq_Handle bsh);
    static int PctNs(CBioseq_Handle bsh);

    static bool IsMaster(const CBioseq& seq);
    static bool IsWGSMaster(const CBioseq& seq, CScope& scope);
    static bool IsWGS(const CBioseq& seq);
    static bool IsWGS(CBioseq_Handle bsh);
    static bool IsWGSAccession(const CSeq_id& id);
    static bool IsWGSAccession(const CBioseq& seq);
    static bool IsTSAAccession(const CSeq_id& id);
    static bool IsTSAAccession(const CBioseq& seq);
    static bool IsWp(CBioseq_Handle bsh);
    static bool IsEmblOrDdbj(const CBioseq& seq);
    static bool IsGenbank(const CBioseq& seq);
    static bool IsRefSeq(const CBioseq& seq);
    static bool IsPdb(const CBioseq& seq);
    static bool IsPartial(const CBioseq& seq, CScope& scope);

    // DBLink user object counters
    int m_dblink_count;
    int m_taa_count;
    int m_bs_count;
    int m_as_count;
    int m_pdb_count;
    int m_sra_count;
    int m_bp_count;
    int m_unknown_count;

private:
    typedef multimap<string, const CSeq_feat*, PNocase> TStrFeatMap;
    typedef vector<CMappedFeat>                         TMappedFeatVec;
    
    void ValidateSeqLen(const CBioseq& seq);
    void ValidateSegRef(const CBioseq& seq);
    void ValidateDelta(const CBioseq& seq);
    static bool x_IgnoreEndGap(CBioseq_Handle bsh, CSeq_gap::TType gap_type);
    void ValidateSeqGap(const CSeq_gap& gap, const CBioseq& seq);
    void ValidateDeltaLoc(const CSeq_loc& loc, const CBioseq& seq, TSeqPos& len);
    bool ValidateRepr(const CSeq_inst& inst, const CBioseq& seq);
    void ValidateSeqParts(const CBioseq& seq);
    void x_ValidateTitle(const CBioseq& seq);
    void x_ValidateBarcode(const CBioseq& seq);
    void ValidateRawConst(const CBioseq& seq);
    void x_CalculateNsStretchAndTotal(const CBioseq& seq, TSeqPos& num_ns, TSeqPos& max_stretch, bool& n5, bool& n3);
    void ValidateNsAndGaps(const CBioseq& seq);
    void ReportBadAssemblyGap (const CBioseq& seq);
    static bool HasBadWGSGap(const CBioseq& seq);
    void ReportBadWGSGap(const CBioseq& seq);
    void ReportBadTSAGap(const CBioseq& seq);
    void ReportBadGenomeGap(const CBioseq& seq);
    void ValidateWGSMaster(CBioseq_Handle bsh);
    
    void ValidateMultiIntervalGene (const CBioseq& seq);
    void ValidateMultipleGeneOverlap (const CBioseq_Handle& bsh);
    void ValidateBadGeneOverlap(const CSeq_feat& feat);
    void x_ReportGeneOverlapError(const CSeq_feat& feat, const string& gene_label);
    void ValidateCDSAndProtPartials (const CMappedFeat& feat);
    void x_ReportImproperPartial(const CSeq_feat& feat);
    void x_ReportInternalPartial(const CSeq_feat& feat);
    bool x_PartialAdjacentToIntron(const CSeq_loc& loc);
    void ValidateFeatPartialInContext (const CMappedFeat& feat);
    void x_ValidateCodingRegionParentPartialness(const CSeq_feat& cds, const CSeq_loc& parent_loc, const string& parent_name);
    void x_ValidateCodingRegionParentPartialness(const CSeq_feat& cds);
    bool x_IsPartialAtSpliceSiteOrGap (const CSeq_loc& loc, unsigned int tag, bool& bad_seq, bool& is_gap);
    bool x_SplicingNotExpected(const CMappedFeat& feat);
    bool x_MatchesOverlappingFeaturePartial (const CMappedFeat& feat, unsigned int partial_type);
    bool x_IsSameAsCDS(const CMappedFeat& feat);
    void ValidateSeqFeatContext(const CBioseq& seq);
    EDiagSev x_DupFeatSeverity (const CSeq_feat& curr, const CSeq_feat& prev, bool viral, bool htgs, bool same_annot, bool same_label);
    bool x_ReportDupOverlapFeaturePair (const CSeq_feat_Handle & f1, const CSeq_feat_Handle & f2, bool fruit_fly, bool viral, bool htgs);
    void x_ReportOverlappingPeptidePair (CSeq_feat_Handle f1, CSeq_feat_Handle f2, const CBioseq& bioseq, bool& reported_last_peptide);
    void ValidateDupOrOverlapFeats(const CBioseq& seq);
    void ValidateTwintrons(const CBioseq& seq);
    void ValidateCollidingGenes(const CBioseq& seq);
    void ValidateCompleteGenome(const CBioseq& seq);
    void x_CompareStrings(const TStrFeatMap& str_feat_map, const string& type,
        EErrType err, EDiagSev sev);
    void x_ValidateCompletness(const CBioseq& seq, const CMolInfo& mi);
    void x_ReportSuspiciousUseOfComplete(const CBioseq& seq, EDiagSev sev);
    void x_ValidateAbuttingUTR(const CBioseq_Handle& seq);
    bool x_IsRangeGap (const CBioseq_Handle& seq, int start, int stop);
    void x_ValidateAbuttingRNA(const CBioseq_Handle& seq);
    void x_ValidateGeneCDSmRNACounts (const CBioseq_Handle& seq);
    void x_ValidateCDSmRNAmatch(const CBioseq_Handle& seq, int numgene, int numcds, int nummrna);
    void x_CheckForMultiplemRNAs(const CCdsMatchInfo& cds_match, const list<CRef<CMrnaMatchInfo>>& unmatched_mrnas);
    void x_CheckMrnaProteinLink(const CCdsMatchInfo& cds_match);
    unsigned int x_IdXrefsNotReciprocal (const CSeq_feat &cds, const CSeq_feat &mrna);
    bool x_IdXrefsAreReciprocal (const CSeq_feat &cds, const CSeq_feat &mrna);
    void x_ValidateLocusTagGeneralMatch(const CBioseq_Handle& seq);

    void ValidateSeqDescContext(const CBioseq& seq);
    void CheckForMissingChromosome(CBioseq_Handle bsh);
    void CheckForMultipleStructuredComments(const CBioseq& seq);
    void x_CheckForMultipleComments(CBioseq_Handle bsh);
    void x_ValidateStructuredCommentContext(const CSeqdesc& desc, const CBioseq& seq);
    void ValidateGBBlock (const CGB_block& gbblock, const CBioseq& seq, const CSeqdesc& desc);
    void ValidateMolInfoContext(const CMolInfo& minfo, int& seq_biomol, int& tech, int& completeness,
        const CBioseq& seq, const CSeqdesc& desc);
    void ValidateMolTypeContext(const EGIBB_mol& gibb, EGIBB_mol& seq_biomol,
        const CBioseq& seq, const CSeqdesc& desc);
    void ValidateUpdateDateContext(const CDate& update,const CDate& create,
        const CBioseq& seq, const CSeqdesc& desc);
    void ValidateOrgContext(const CSeqdesc_CI& iter, const COrg_ref& this_org,
        const COrg_ref& org, const CBioseq& seq, const CSeqdesc& desc);
    void ReportModifInconsistentError (int new_mod, int& old_mod, const CSeqdesc& desc, const CSeq_entry& ctx);
    void ValidateModifDescriptors (const CBioseq& seq);
    void ValidateMoltypeDescriptors (const CBioseq& seq);

    void ValidateGraphsOnBioseq(const CBioseq& seq);
    void ValidateGraphOrderOnBioseq (const CBioseq& seq, vector <CRef <CSeq_graph> > graph_list);
    void ValidateByteGraphOnBioseq(const CSeq_graph& graph, const CBioseq& seq);
    void ValidateGraphOnDeltaBioseq(const CBioseq& seq);
    bool ValidateGraphLocation (
        const CSeq_graph& graph);
    void ValidateGraphValues(const CSeq_graph& graph, const CBioseq& seq,
        int& first_N, int& first_ACGT, size_t& num_bases, size_t& Ns_with_score, size_t& gaps_with_score,
        size_t& ACGTs_without_score, size_t& vals_below_min, size_t& vals_above_max);
    void ValidateMinValues(const CByte_graph& bg, const CSeq_graph& graph);
    void ValidateMaxValues(const CByte_graph& bg, const CSeq_graph& graph);
    bool GetLitLength(const CDelta_seq& delta, TSeqPos& len);
    bool IsSupportedGraphType(const CSeq_graph& graph) const;
    SIZE_TYPE GetSeqLen(const CBioseq& seq);

    void ValidateSecondaryAccConflict(const string& primary_acc,
        const CBioseq& seq, int choice);
    void ValidateIDSetAgainstDb(const CBioseq& seq);
    void x_ValidateSourceFeatures(const CBioseq_Handle& bsh);
    void x_ValidatePubFeatures(const CBioseq_Handle& bsh);
    void x_ReportDuplicatePubLabels (const CBioseq& seq, const vector<CTempString>& labels);
    void x_ValidateMultiplePubs(
        const CBioseq_Handle& bsh);

    void CheckForPubOnBioseq(const CBioseq& seq);
    void CheckSoureDescriptor(const CBioseq_Handle& bsh);
    void CheckForMolinfoOnBioseq(const CBioseq& seq);
    void CheckTpaHistory(const CBioseq& seq);

    TSeqPos GetDataLen(const CSeq_inst& inst);
    bool CdError(const CBioseq_Handle& bsh);
    bool IsMrna(const CBioseq_Handle& bsh);
    bool IsPrerna(const CBioseq_Handle& bsh);
    size_t NumOfIntervals(const CSeq_loc& loc);
    bool LocOnSeg(const CBioseq& seq, const CSeq_loc& loc);
    //bool NotPeptideException(const CFeat_CI& curr, const CFeat_CI& prev);
    //bool IsSameSeqAnnot(const CFeat_CI& fi1, const CFeat_CI& fi2);
    bool x_IsSameSeqAnnotDesc(const CSeq_feat_Handle& f1, const CSeq_feat_Handle& f2);
    bool IsIdIn(const CSeq_id& id, const CBioseq& seq);
    bool SuppressTrailingXMsg(const CBioseq& seq);
    CRef<CSeq_loc> GetLocFromSeq(const CBioseq& seq);
    bool IsHistAssemblyMissing(const CBioseq& seq);
    bool IsFlybaseDbxrefs(const TDbtags& dbxrefs);
    bool GraphsOnBioseq(const CBioseq& seq) const;
    bool IsSynthetic(const CBioseq& seq) const;
    bool x_IsArtificial(const CBioseq& seq) const;
    bool x_IsActiveFin(const CBioseq& seq) const;
    bool x_IsMicroRNA(const CBioseq& seq) const;
    bool x_IsDeltaLitOnly(const CSeq_inst& inst) const;
    bool x_ShowBioProjectWarning(const CBioseq& seq);
    bool x_AllowOrphanedProtein(const CBioseq& seq) const;
    bool x_HasCitSub(CBioseq_Handle bsh) const;
    static bool x_HasCitSub(const CPub_equiv& pub);
    static bool x_HasCitSub(const CPub& pub);

    void ValidatemRNAGene (const CBioseq& seq);
    void ValidateCDSUTR(const CBioseq& seq);
    bool x_ReportUTRPair(const CSeq_feat& utr5, const CSeq_feat& utr3);

    size_t x_CountAdjacentNs(const CSeq_literal& lit);

    //internal validators
    CValidError_annot m_AnnotValidator;
    CValidError_descr m_DescrValidator;
    CValidError_feat  m_FeatValidator;

    // BioseqHandle for bioseq currently being validated - to cut down on overhead
    CBioseq_Handle m_CurrentHandle;

    // feature iterator for genes on bioseq - to cut down on overhead
    // (This class does *not* own this)
    const CCacheImpl::TFeatValue * m_GeneIt;

    // feature iterator for all features on bioseq (again, trying to cut down on overhead
    // (This class does *not* own this)
    const CCacheImpl::TFeatValue * m_AllFeatIt;

    class CmRNACDSIndex 
    {
    public:
        CmRNACDSIndex();
        ~CmRNACDSIndex();
        void SetBioseq(const CCacheImpl::TFeatValue * feat_list, const CBioseq_Handle & bioseq);

        CMappedFeat GetmRNAForCDS(const CMappedFeat & cds);
        CMappedFeat GetCDSFormRNA(const CMappedFeat & cds);

    private:
        typedef pair <CMappedFeat, CMappedFeat> TmRNACDSPair;
        typedef vector < TmRNACDSPair > TPairList;
        TPairList m_PairList;

        typedef vector< CMappedFeat > TFeatList;
        TFeatList m_CDSList;
        TFeatList m_mRNAList;
    };

    CmRNAAndCDSIndex m_mRNACDSIndex;


};


// ===========================  Validate Bioseq_set  ===========================


class CValidError_bioseqset : private CValidError_base
{
public:
    CValidError_bioseqset(CValidError_imp& imp);
    virtual ~CValidError_bioseqset(void);

    void ValidateBioseqSet(const CBioseq_set& seqset);

private:

    void ValidateNucProtSet(const CBioseq_set& seqset, int nuccnt, int protcnt, int segcnt);
    void ValidateSegSet(const CBioseq_set& seqset, int segcnt);
    void ValidatePartsSet(const CBioseq_set& seqset);
    void ValidateGenbankSet(const CBioseq_set& seqset);
    void ValidateSetTitle(const CBioseq_set& seqset);
    void ValidateSetElements(const CBioseq_set& seqset);
    void ValidatePopSet(const CBioseq_set& seqset);
    void ValidatePhyMutEcoWgsSet(const CBioseq_set& seqset);
    void ValidateGenProdSet(const CBioseq_set& seqset);
    void CheckForInconsistentBiomols (const CBioseq_set& seqset);
    void SetShouldNotHaveMolInfo(const CBioseq_set& seqset);
    void CheckForImproperlyNestedSets (const CBioseq_set& seqset);
    void ShouldHaveNoDblink (const CBioseq_set& seqset);

    bool IsMrnaProductInGPS(const CBioseq& seq); 
    bool IsCDSProductInGPS(const CBioseq& seq, const CBioseq_set& gps); 

    //internal validators
    CValidError_annot m_AnnotValidator;
    CValidError_descr m_DescrValidator;
    CValidError_bioseq m_BioseqValidator;
};


// ===========================  for handling PCR primer subtypes on BioSource ==

class CPCRSet
{
public:
    CPCRSet(size_t pos);
    virtual ~CPCRSet(void);

    string GetFwdName(void)            const { return m_FwdName; }
    string GetFwdSeq(void)             const { return m_FwdSeq; }
    string GetRevName(void)            const { return m_RevName; }
    string GetRevSeq(void)             const { return m_RevSeq; }
    size_t GetOrigPos(void)            const { return m_OrigPos; }

    void SetFwdName(string fwd_name) { m_FwdName = fwd_name; }
    void SetFwdSeq(string fwd_seq)   { m_FwdSeq = fwd_seq; }
    void SetRevName(string rev_name) { m_RevName = rev_name; }
    void SetRevSeq(string rev_seq)   { m_RevSeq = rev_seq; }

private:
    string m_FwdName;
    string m_FwdSeq;
    string m_RevName;
    string m_RevSeq;
    size_t m_OrigPos;
};

class CPCRSetList
{
public:
    CPCRSetList(void);
    ~CPCRSetList(void);

    void AddFwdName (string name);
    void AddRevName (string name);
    void AddFwdSeq (string name);
    void AddRevSeq (string name);

    bool AreSetsUnique(void);

private:
    vector <CPCRSet *> m_SetList;
};



END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* VALIDATOR___VALIDATORP__HPP */
