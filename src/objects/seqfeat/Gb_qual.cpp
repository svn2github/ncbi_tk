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
 *   using the following specifications:
 *   'seqfeat.asn'.
 */

// standard includes
#include <ncbi_pch.hpp>

// generated includes
#include <objects/seqfeat/Gb_qual.hpp>

// other includes
#include <serial/enumvalues.hpp>
#include <serial/serialimpl.hpp>

// generated classes


BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CGb_qual::~CGb_qual(void)
{
}


static const char * const valid_inf_categories [] = {
    "EXISTENCE",
    "COORDINATES",
    "DESCRIPTION"
};

static const char * const valid_inf_prefixes [] = {
    "ab initio prediction",
    "nucleotide motif",
    "profile",
    "protein motif",
    "similar to AA sequence",
    "similar to DNA sequence",
    "similar to RNA sequence",
    "similar to RNA sequence, EST",
    "similar to RNA sequence, mRNA",
    "similar to RNA sequence, other RNA",
    "similar to sequence",
    "alignment"
};


void CGb_qual::ParseExperiment(const string& orig, string& category, string& experiment, string& doi)
{
    experiment = orig;
    category = "";
    doi = "";
    NStr::TruncateSpacesInPlace(experiment);

    for (unsigned int i = 0; i < sizeof (valid_inf_categories) / sizeof (char *); i++) {
        if (NStr::StartsWith (experiment, valid_inf_categories[i])) {
            category = valid_inf_categories[i];
            experiment = experiment.substr(category.length());
            NStr::TruncateSpacesInPlace(experiment);
            if (NStr::StartsWith(experiment, ":")) {
                experiment = experiment.substr(1);
            }
            NStr::TruncateSpacesInPlace(experiment);
            break;
        }
    }
    if (NStr::EndsWith(experiment, "]")) {
        size_t start_doi = NStr::Find(experiment, "[");

        if (start_doi != string::npos) {
            doi = experiment.substr(start_doi + 1);
            doi = doi.substr(0, doi.length() - 1);
            experiment = experiment.substr(0, start_doi);
        }
    }
}


string CGb_qual::BuildExperiment(const string& category, const string& experiment, const string& doi)
{
    string rval = "";
    if (!NStr::IsBlank(category)) {
        rval += category + ":";
    }
    rval += experiment;
    if (!NStr::IsBlank(doi)) {
        rval += "[" + doi + "]";
    }
    return rval;
}


bool CGb_qual::x_CleanupRptAndReplaceSeq(string& val)
{
    if (NStr::IsBlank(val)) {
        return false;
    }
    // do not clean if val contains non-sequence characters
    if (string::npos != val.find_first_not_of("ACGTUacgtu")) {
        return false;
    }
    string orig = val;
    NStr::ToLower(val);
    NStr::ReplaceInPlace(val, "u", "t");
    return !NStr::Equal(orig, val);
}


bool CGb_qual::CleanupRptUnitSeq(string& val)
{
    return x_CleanupRptAndReplaceSeq(val);
}


bool CGb_qual::CleanupReplace(string& val)
{
    return x_CleanupRptAndReplaceSeq(val);
}


// converts dashes to a pair of dots, unless dots are already present
// also makes no change if characters other than digits or dashes are found
bool CGb_qual::CleanupRptUnitRange(string& val)
{
    if (NStr::IsBlank(val)) {
        return false;
    }
    if (NStr::Find(val, ".") != string::npos) {
        return false;
    }
    if (NStr::Find(val, "-") == string::npos) {
        return false;
    }
    if (string::npos != val.find_first_not_of("0123456789-")) {
        return false;
    }
    NStr::ReplaceInPlace(val, "-", "..");
    return true;
}


// constructor
CInferencePrefixList::CInferencePrefixList(void)
{
}

//destructor
CInferencePrefixList::~CInferencePrefixList(void)
{
}


void CInferencePrefixList::GetPrefixAndRemainder (const string& inference, string& prefix, string& remainder)
{
    string category = "";
    prefix = "";
    remainder = "";
    string check = inference;

    for (unsigned int i = 0; i < sizeof (valid_inf_categories) / sizeof (char *); i++) {
        if (NStr::StartsWith (check, valid_inf_categories[i])) {
            category = valid_inf_categories[i];
            check = check.substr(category.length());
            NStr::TruncateSpacesInPlace(check);
            if (NStr::StartsWith(check, ":")) {
                check = check.substr(1);
            }
            if (NStr::StartsWith(check, " ")) {
                check = check.substr(1);
            }
            break;
        }
    }
    for (unsigned int i = 0; i < sizeof (valid_inf_prefixes) / sizeof (char *); i++) {
        if (NStr::StartsWith (check, valid_inf_prefixes[i], NStr::eNocase)) {
            prefix = valid_inf_prefixes[i];
        }
    }

    remainder = check.substr (prefix.length());
    NStr::TruncateSpacesInPlace (remainder);
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 65, chars: 1885, CRC32: 3224da35 */
