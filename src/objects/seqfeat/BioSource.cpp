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
 *   using specifications from the data definition file
 *   'seqfeat.asn'.
 */

// standard includes

// generated includes
#include <ncbi_pch.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <algorithm>
#include <set>
#include <util/static_map.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CBioSource::~CBioSource(void)
{
}


int CBioSource::GetGenCode(void) const
{
    try {
        int genome = 0;

        if ( IsSetGenome() ) {
            genome = GetGenome();
        }

        const COrgName& orn = GetOrg().GetOrgname();

        switch ( genome ) {
        case eGenome_kinetoplast:
        case eGenome_mitochondrion:
            // bacteria and plant organelle code
            return orn.GetMgcode();
        case eGenome_chloroplast:
        case eGenome_chromoplast:
        case eGenome_plastid:
        case eGenome_cyanelle:
        case eGenome_apicoplast:
        case eGenome_leucoplast:
        case eGenome_proplastid:
            // bacteria and plant plastids are code 11.
            return 11;
        default:
            return orn.GetGcode();
        }
    } catch ( CCoreException exp ) {
        return 0;
    }
}

typedef pair <const char *, const CBioSource::EGenome> TGenomeKey;

static const TGenomeKey genome_key_to_subtype [] = {
    TGenomeKey ( "apicoplast",                CBioSource::eGenome_apicoplast       ),
    TGenomeKey ( "chloroplast",               CBioSource::eGenome_chloroplast      ),
    TGenomeKey ( "chromoplast",               CBioSource::eGenome_chromoplast      ),
    TGenomeKey ( "cyanelle",                  CBioSource::eGenome_cyanelle         ),
    TGenomeKey ( "endogenous_virus",          CBioSource::eGenome_endogenous_virus ),
    TGenomeKey ( "extrachrom",                CBioSource::eGenome_extrachrom       ),
    TGenomeKey ( "genomic",                   CBioSource::eGenome_genomic          ),
    TGenomeKey ( "hydrogenosome",             CBioSource::eGenome_hydrogenosome    ),
    TGenomeKey ( "insertion_seq",             CBioSource::eGenome_insertion_seq    ),
    TGenomeKey ( "kinetoplast",               CBioSource::eGenome_kinetoplast      ),
    TGenomeKey ( "leucoplast",                CBioSource::eGenome_leucoplast       ),
    TGenomeKey ( "macronuclear",              CBioSource::eGenome_macronuclear     ),
    TGenomeKey ( "mitochondrion",             CBioSource::eGenome_mitochondrion    ),
    TGenomeKey ( "mitochondrion:kinetoplast", CBioSource::eGenome_kinetoplast      ),
    TGenomeKey ( "nucleomorph",               CBioSource::eGenome_nucleomorph      ),
    TGenomeKey ( "plasmid",                   CBioSource::eGenome_plasmid          ),
    TGenomeKey ( "plastid",                   CBioSource::eGenome_plastid          ),
    TGenomeKey ( "plastid:apicoplast",        CBioSource::eGenome_apicoplast       ),
    TGenomeKey ( "plastid:chloroplast",       CBioSource::eGenome_chloroplast      ),
    TGenomeKey ( "plastid:chromoplast",       CBioSource::eGenome_chromoplast      ),
    TGenomeKey ( "plastid:cyanelle",          CBioSource::eGenome_cyanelle         ),
    TGenomeKey ( "plastid:leucoplast",        CBioSource::eGenome_leucoplast       ),
    TGenomeKey ( "plastid:proplastid",        CBioSource::eGenome_proplastid       ),
    TGenomeKey ( "proplastid",                CBioSource::eGenome_proplastid       ),
    TGenomeKey ( "proviral",                  CBioSource::eGenome_proviral         ),
    TGenomeKey ( "transposon",                CBioSource::eGenome_transposon       ),
    TGenomeKey ( "unknown",                   CBioSource::eGenome_unknown          ),
    TGenomeKey ( "virion",                    CBioSource::eGenome_virion           )
};


typedef CStaticArrayMap <const char*, const CBioSource::EGenome, PNocase_CStr> TGenomeMap;
static const TGenomeMap sm_GenomeKeys (genome_key_to_subtype, sizeof (genome_key_to_subtype));

CBioSource::EGenome CBioSource::GetGenomeByOrganelle (string organelle, NStr::ECase use_case, bool starts_with)
{
    CBioSource::EGenome gtype = CBioSource::eGenome_unknown;
    
    if (use_case == NStr::eCase && !starts_with) {
        TGenomeMap::const_iterator g_iter = sm_GenomeKeys.find (organelle.c_str ());
        if (g_iter != sm_GenomeKeys.end ()) {
            gtype = g_iter->second;
        }
    } else {
        TGenomeMap::const_iterator g_iter = sm_GenomeKeys.begin();
        if (starts_with) {
            string match;
            while (g_iter != sm_GenomeKeys.end() && gtype == CBioSource::eGenome_unknown) {
                match = g_iter->first;
                if (NStr::StartsWith(organelle, match.c_str(), use_case)) {
                    if (organelle.length() == match.length()
                        || (match.length() < organelle.length() && isspace (organelle[match.length()]))) {
                        gtype = g_iter->second;
                    }
                }
                ++g_iter;
            }
        } else {
            while (g_iter != sm_GenomeKeys.end() && gtype == CBioSource::eGenome_unknown) {
                if (NStr::Equal(organelle, g_iter->first, use_case)) {
                    gtype = g_iter->second;
                }
                ++g_iter;
            }
        }
    }
    return gtype;
}


string CBioSource::GetOrganelleByGenome (unsigned int genome)
{
    string organelle = "";
    TGenomeMap::const_iterator g_iter = sm_GenomeKeys.begin();
    while (g_iter != sm_GenomeKeys.end() && g_iter->second != genome) {
        ++g_iter;
    }
    if (g_iter != sm_GenomeKeys.end()) {
        organelle = g_iter->first;
    }
    return organelle;
}

END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 6.8  2006/10/18 17:37:42  bollin
* Added functions for getting organelle name from genome value and genome value
* from organelle name.
*
* Revision 6.7  2006/03/14 20:21:52  rsmith
* Move BasicCleanup functionality from objects to objtools/cleanup
*
* Revision 6.6  2005/06/03 16:52:28  lavr
* Explicit (unsigned char) casts in ctype routines
*
* Revision 6.5  2005/05/20 20:57:57  ucko
* Rework x_SubtypeCleanup to build against WorkShop's STL implementation,
* which doesn't allow sorting lists via custom comparators.
*
* Revision 6.4  2005/05/20 13:36:54  shomrat
* Added BasicCleanup()
*
* Revision 6.3  2004/05/19 17:26:04  gorelenk
* Added include of PCH - ncbi_pch.hpp
*
* Revision 6.2  2002/11/26 19:01:11  shomrat
* Bug fix in GetGenCode
*
* Revision 6.1  2002/11/26 18:50:31  shomrat
* Add GetGenCode
*
*
* ===========================================================================
*/
/* Original file checksum: lines: 64, chars: 1883, CRC32: e1194deb */
