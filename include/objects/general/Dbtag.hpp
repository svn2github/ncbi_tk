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
 *   'general.asn'.
 *
 * ---------------------------------------------------------------------------
 */

#ifndef OBJECTS_GENERAL_DBTAG_HPP
#define OBJECTS_GENERAL_DBTAG_HPP


// generated includes
#include <objects/general/Dbtag_.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class NCBI_GENERAL_EXPORT CDbtag : public CDbtag_Base
{
    typedef CDbtag_Base Tparent;
public:

    // this must be kept in sync with the (private) list in Dbtag.cpp!
    enum EDbtagType {
        eDbtagType_bad,
        
        eDbtagType_AFTOL,
        eDbtagType_APHIDBASE,
        eDbtagType_ASAP,
        eDbtagType_ATCC,
        eDbtagType_ATCC_dna,
        eDbtagType_ATCC_in_host,
        eDbtagType_AceView_WormGenes,
        eDbtagType_AntWeb,
        eDbtagType_ApiDB,
        eDbtagType_ApiDB_CryptoDB,
        eDbtagType_ApiDB_PlasmoDB,
        eDbtagType_ApiDB_ToxoDB,
        eDbtagType_BB,
        eDbtagType_BDGP_EST,
        eDbtagType_BDGP_INS,
        eDbtagType_BEETLEBASE,
        eDbtagType_BGD,
        eDbtagType_BoLD,
        eDbtagType_CCDS,
        eDbtagType_CDD,
        eDbtagType_CGNC,
        eDbtagType_CK,
        eDbtagType_COG,
        eDbtagType_CloneID,
        eDbtagType_CollecTF,
        eDbtagType_DDBJ,
        eDbtagType_ECOCYC,
        eDbtagType_EMBL,
        eDbtagType_ENSEMBL,
        eDbtagType_ESTLIB,
        eDbtagType_EcoGene,
        eDbtagType_FANTOM_DB,
        eDbtagType_FBOL,
        eDbtagType_FLYBASE,
        eDbtagType_Fungorum,
        eDbtagType_GABI,
        eDbtagType_GDB,
        eDbtagType_GEO,
        eDbtagType_GI,
        eDbtagType_GO,
        eDbtagType_GOA,
        eDbtagType_GRIN,
        eDbtagType_GeneDB,
        eDbtagType_GeneID,
        eDbtagType_GrainGenes,
        eDbtagType_Greengenes,
        eDbtagType_HGNC,
        eDbtagType_HMP,
        eDbtagType_HOMD,
        eDbtagType_HPM,
        eDbtagType_HPRD,
        eDbtagType_HSSP,
        eDbtagType_H_InvDB,
        eDbtagType_IFO,
        eDbtagType_IMGT_GENEDB,
        eDbtagType_IMGT_HLA,
        eDbtagType_IMGT_LIGM,
        eDbtagType_IRD,
        eDbtagType_ISD,
        eDbtagType_ISFinder,
        eDbtagType_InterimID,
        eDbtagType_Interpro,
        eDbtagType_IntrepidBio,
        eDbtagType_JCM,
        eDbtagType_JGIDB,
        eDbtagType_LRG,
        eDbtagType_LocusID,
        eDbtagType_MGI,
        eDbtagType_MIM,
        eDbtagType_MaizeGDB,
        eDbtagType_MycoBank,
        eDbtagType_NMPDR,
        eDbtagType_NRESTdb,
        eDbtagType_NextDB,
        eDbtagType_OrthoMCL,
        eDbtagType_Osa1,
        eDbtagType_PBR,
        eDbtagType_PBmice,
        eDbtagType_PDB,
        eDbtagType_PFAM,
        eDbtagType_PGN,
        eDbtagType_PIR,
        eDbtagType_PSEUDO,
        eDbtagType_Pathema,
        eDbtagType_Phytozome,
        eDbtagType_PomBase,
        eDbtagType_PseudoCap,
        eDbtagType_RAP_DB,
        eDbtagType_RATMAP,
        eDbtagType_RBGE_garden,
        eDbtagType_RBGE_herbarium,
        eDbtagType_REBASE,
        eDbtagType_RFAM,
        eDbtagType_RGD,
        eDbtagType_RZPD,
        eDbtagType_RiceGenes,
        eDbtagType_SEED,
        eDbtagType_SGD,
        eDbtagType_SGN,
        eDbtagType_SK_FST,
        eDbtagType_SRPDB,
        eDbtagType_SoyBase,
        eDbtagType_SubtiList,
        eDbtagType_TAIR,
        eDbtagType_TIGRFAM,
        eDbtagType_UNILIB,
        eDbtagType_UNITE,
        eDbtagType_UniGene,
        eDbtagType_UniProt_SwissProt,
        eDbtagType_UniProt_TrEMBL,
        eDbtagType_UniSTS,
        eDbtagType_VBASE2,
        eDbtagType_VBRC,
        eDbtagType_VectorBase,
        eDbtagType_Vega,
        eDbtagType_WorfDB,
        eDbtagType_WormBase,
        eDbtagType_Xenbase,
        eDbtagType_ZFIN,
        eDbtagType_axeldb,
        eDbtagType_dbClone,
        eDbtagType_dbCloneLib,
        eDbtagType_dbEST,
        eDbtagType_dbProbe,
        eDbtagType_dbSNP,
        eDbtagType_dbSTS,
        eDbtagType_dictyBase,
        eDbtagType_miRBase,
        eDbtagType_niaEST,
        eDbtagType_taxon,
        eDbtagType_MGD,
        eDbtagType_PID,
        eDbtagType_BEEBASE,
        eDbtagType_NASONIABASE,
        eDbtagType_BioProject,
        eDbtagType_IKMC,
        eDbtagType_ViPR,
        eDbtagType_PubChem,
        eDbtagType_SRA,
        eDbtagType_Trace,
        eDbtagType_RefSeq,
        eDbtagType_EnsemblGenomes,
        eDbtagType_TubercuList,
        eDbtagType_MedGen,
        eDbtagType_CGD,
        eDbtagType_Assembly,
        eDbtagType_GenBank,
        eDbtagType_BioSample,
        eDbtagType_ISHAM_ITS
    };

    enum EDbtagGroup {
        fNone    = 0,
        fGenBank = 1 << 0,
        fRefSeq  = 1 << 1,
        fSrc     = 1 << 2,
        fProbe   = 1 << 3
    };

    typedef int TDbtagGroup; ///< holds bitwise OR of "EDbtagGroup"

    // constructor
    CDbtag(void);
    // destructor
    ~CDbtag(void);

    // Comparison functions
    bool Match(const CDbtag& dbt2) const;
    int Compare(const CDbtag& dbt2) const;
    
    // Appends a label to "label" based on content of CDbtag
    void GetLabel(string* label) const;

    // Test if DB is approved by the consortium.
    // 'GenBank', 'EMBL' and 'DDBJ' are approved only in the
    // context of a RefSeq record.
    enum EIsRefseq {
        eIsRefseq_No = 0,
        eIsRefseq_Yes
    };
    enum EIsSource {
        eIsSource_No = 0,
        eIsSource_Yes
    };
    enum EIsEstOrGss {
        eIsEstOrGss_No = 0,
        eIsEstOrGss_Yes,
    };
    bool IsApproved(EIsRefseq refseq = eIsRefseq_No, EIsSource is_source = eIsSource_No, EIsEstOrGss is_est_or_gss = eIsEstOrGss_No ) const;
    // Test if DB is approved (case insensitive).
    // Returns the case sensetive DB name if approved, NULL otherwise.
    const char* IsApprovedNoCase(EIsRefseq refseq = eIsRefseq_No, EIsSource is_source = eIsSource_No) const;
    // Extended version allows many DB categories
    bool IsApproved(TDbtagGroup group) const;

    // Test if appropriate to omit from displays, formatting, etc.
    bool IsSkippable(void) const;

    // Retrieve the enumerated type for the dbtag
    EDbtagType GetType(void) const;

    // determine the situations where the dbtag would be appropriate
    bool GetDBFlags (bool& is_refseq, bool& is_src, string& correct_caps) const;
    // Extended version allows many DB categories
    TDbtagGroup GetDBFlags (string& correct_caps) const;

    // Force a refresh of the internal type
    void InvalidateType(void);
    
    // Get a URL to the resource (if available)
    // @return
    //   the URL or an empty string if non is available
    string GetUrl(void) const;

    // Get a URL to the resource (if available)
    // @param taxid organism taxid for URL generation
    // @return
    //   the URL or an empty string if non is available
    string GetUrl(int taxid) const;

    // Get a URL to the resource (if available)
    // @param taxname organism taxname (e.g. "Danio rerio") for URL generation
    // @return
    //   the URL or an empty string if non is available
    string GetUrl(const string &taxname) const;

    // Get a URL to the resource (if available).  This function assumes
    // that only 1 of the following 3 states can occur: 1. all parameters
    // empty.  2. genus and species set, but not subspecies.
    // 3. all parameters set.
    // Any other state of parameters may give an unexpected result, but
    // no crash or exception will be thrown.
    // @param genus       organism genus for URL generation
    // @param species     organism species for URL generation
    // @param subspecies  organism subspecies for URL generation
    // @return
    //   the URL or an empty string if non is available
    string GetUrl(const string & genus,
                  const string & species,
                  const string & subspecies = kEmptyStr ) const;

private:

    // our enumerated (parsed) type
    mutable EDbtagType m_Type;

    // Prohibit copy constructor & assignment operator
    CDbtag(const CDbtag&);
    CDbtag& operator= (const CDbtag&);

    // returns true if the given tag looks like an accession and
    // it also gives the number of alpha, digit and underscores found in it
    static bool x_LooksLikeAccession(const string &tag, 
        int &out_num_alpha, 
        int &out_num_digit, 
        int &out_num_unscr);
};



/////////////////// CDbtag inline methods

// constructor
inline
CDbtag::CDbtag(void)
    : m_Type(eDbtagType_bad)
{
}


/////////////////// end of CDbtag inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

#endif // OBJECTS_GENERAL_DBTAG_HPP
