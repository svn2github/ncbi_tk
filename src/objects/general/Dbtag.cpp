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
 * $Log$
 * Revision 6.7  2003/07/22 16:34:39  shomrat
 * Added ZFIN to approved DB list
 *
 * Revision 6.6  2003/06/27 16:22:25  shomrat
 * Changed initialization of kApprovedDbXrefs
 *
 * Revision 6.5  2003/06/27 15:40:09  shomrat
 * Implemented IsApproved
 *
 * Revision 6.4  2002/01/10 19:45:57  clausen
 * Added GetLabel
 *
 * Revision 6.3  2001/12/07 18:52:04  grichenk
 * Updated "#include"-s and forward declarations to work with the
 * new datatool version.
 *
 * Revision 6.2  2000/12/15 19:22:10  ostell
 * made AsString do Upcase, and switched to using PNocase().Equals()
 *
 * Revision 6.1  2000/11/21 18:58:20  vasilche
 * Added Match() methods for CSeq_id, CObject_id and CDbtag.
 *
 *
 * ===========================================================================
 */

// standard includes
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbireg.hpp>

#include <algorithm>

// generated includes
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <corelib/ncbistd.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::


// List of approved DB names.
// NOTE: these must stay in ASCIIbetical order!
static const string kApprovedDbXrefs[] = {
    "ATCC",
    "ATCC(dna)",
    "ATCC(in host)",
    "AceView/WormGenes",
    "BDGP_EST",
    "BDGP_INS",
    "CDD",
    "CK",
    "COG",
    "ENSEMBL",
    "ESTLIB",
    "FANTOM_DB"         ,
    "FLYBASE",
    "GABI",
    "GDB",
    "GI",
    "GO",
    "GOA",
    "GeneDB",
    "GeneID",
    "IFO",
    "IMGT/HLA",
    "IMGT/LIGM",
    "ISFinder",
    "InterimID",
    "Interpro",
    "JCM",
    "LocusID",
    "MGD",
    "MGI",
    "MIM",
    "MaizeDB",
    "NextDB",
    "PID",
    "PIDd",
    "PIDe",
    "PIDg",
    "PIR",
    "PSEUDO",
    "RATMAP",
    "REMTREMBL",
    "RGD",
    "RZPD",
    "RiceGenes",
    "SGD",
    "SPTREMBL",
    "SWISS-PROT",
    "SoyBase",
    "UniGene",
    "UniSTS",
    "WorfDB",
    "WormBase",
    "ZFIN",
    "dbEST",
    "dbSNP",
    "dbSTS",
    "niaEST",
    "taxon"
};
static const string* kApprovedDbXrefsEnd
    = kApprovedDbXrefs + sizeof(kApprovedDbXrefs)/sizeof(string);


// destructor
CDbtag::~CDbtag(void)
{
}

bool CDbtag::Match(const CDbtag& dbt2) const
{
	if (! PNocase().Equals(GetDb(), dbt2.GetDb()))
		return false;
	return ((GetTag()).Match((dbt2.GetTag())));
}


// Appends a label to "label" based on content of CDbtag 
void CDbtag::GetLabel(string* label) const
{
    const CObject_id& id = GetTag();
    switch (id.Which()) {
    case CObject_id::e_Str:
        *label += GetDb() + ": " + id.GetStr();
        break;
    case CObject_id::e_Id:
        *label += GetDb() + ": " + NStr::IntToString(id.GetId());
        break;
    default:
        *label += GetDb();
    }
}


// Test if CDbtag.dbis in the approved databases list.
// NOTE: 'GenBank', 'EMBL' and 'DDBJ' are approved only in 
//        the context of a RefSeq record.
bool CDbtag::IsApproved(bool refseq) const
{
    if ( !CanGetDb() ) {
        return false;
    }
    const string& db = GetDb();

    // if refseq, first test if GenBank, EMBL or DDBJ
    if ( refseq ) {
        if ( db == "GenBank"  ||  db == "EMBL"  ||  db == "DDBJ" ) {
            return true;
        }
    }

    // Check the rest of approved databases
    return binary_search(kApprovedDbXrefs, kApprovedDbXrefsEnd, db);
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

