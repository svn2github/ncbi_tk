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
#include <util/static_map.hpp>
#include <util/util_misc.hpp>
#include <util/line_reader.hpp>
#include <serial/enumvalues.hpp>

#include <objects/general/general_macros.hpp>

// generated includes
#include <objects/seqfeat/OrgMod.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
COrgMod::~COrgMod(void)
{
}


COrgMod::TSubtype COrgMod::GetSubtypeValue(const string& str,
                                           EVocabulary vocabulary)
{
    string name = NStr::TruncateSpaces(str);
    NStr::ToLower(name);
    replace(name.begin(), name.end(), '_', '-');
    replace(name.begin(), name.end(), ' ', '-');
    
    if (name == "note" ||
        NStr::EqualNocase(name, "orgmod-note")) {
        return eSubtype_other;
    } else if (vocabulary == eVocabulary_insdc) {
        if (name == "host") {
            return eSubtype_nat_host;
        } else if (name == "sub-strain") {
            return eSubtype_substrain;
        }
    }

    return ENUM_METHOD_NAME(ESubtype)()->FindValue(name);
}


bool COrgMod::IsValidSubtypeName(const string& str, 
                                 EVocabulary vocabulary)
{
    string name = NStr::TruncateSpaces(str);
    NStr::ToLower(name);
    replace(name.begin(), name.end(), '_', '-');
    replace(name.begin(), name.end(), ' ', '-');

    if (name == "note" ||
        name == "orgmod-note") {
        return true;
    } else if (vocabulary == eVocabulary_insdc) {
        if (name == "host" || name == "sub-strain") {
            return true;
        }
    }

    return ENUM_METHOD_NAME(ESubtype)()->IsValidName(name);
}


string COrgMod::GetSubtypeName(COrgMod::TSubtype stype, EVocabulary vocabulary)
{
    if (stype == eSubtype_other) {
        return "note";
    } else if (vocabulary == eVocabulary_insdc) {
        switch (stype) {
        case eSubtype_substrain: return "sub_strain";
        case eSubtype_nat_host:  return "host";
        default:
            return NStr::Replace
                (ENUM_METHOD_NAME(ESubtype)()->FindName(stype, true),
                 "-", "_");
        }
    } else {
        return ENUM_METHOD_NAME(ESubtype)()->FindName(stype, true);
    }
}


bool COrgMod::IsMultipleValuesAllowed(TSubtype subtype)
{
    return subtype != eSubtype_strain;
}


bool COrgMod::IsDiscouraged(const TSubtype subtype)
{
    if (subtype == eSubtype_dosage
        || subtype == eSubtype_gb_acronym
        || subtype == eSubtype_gb_anamorph
        || subtype == eSubtype_gb_synonym
        || subtype == eSubtype_old_lineage
        || subtype == eSubtype_old_name
        || subtype == eSubtype_metagenome_source) {
        return true;
    } else {
        return false;
    }
}


bool COrgMod::HoldsInstitutionCode(const TSubtype stype)
{
    switch(stype) {
    case eSubtype_specimen_voucher:
    case eSubtype_culture_collection:
    case eSubtype_bio_material:
        return true;
    default:
        return false;
    }
}


bool COrgMod::ParseStructuredVoucher(const string& str, string& inst, string& coll, string& id)
{
	if (NStr::IsBlank(str) || str.length() < 3) {
		return false;
	}

	size_t pos = NStr::Find(str, ":");
	if (pos == string::npos) {
		return false;
	}
	inst = str.substr(0, pos);
	id = str.substr(pos + 1);
	coll = "";
	pos = NStr::Find(id, ":");
	if (pos != string::npos) {
		coll = id.substr(0, pos);
		id = id.substr(pos + 1);
	}
	if (NStr::IsBlank(inst) || NStr::IsBlank(id)) {
		return false;
	} else {
		return true;
	}
}


// ===== biomaterial, and culture-collection BioSource subsource modifiers     ================

typedef map<string, string, PNocase> TInstitutionCodeMap;

static TInstitutionCodeMap s_BiomaterialInstitutionCodeMap;
static TInstitutionCodeMap s_SpecimenVoucherInstitutionCodeMap;
static TInstitutionCodeMap s_CultureCollectionInstitutionCodeMap;

// holds all the data in the specific ones above
static TInstitutionCodeMap s_CompleteInstitutionCodeMap;
static TInstitutionCodeMap s_CompleteInstitutionFullNameMap;
static TInstitutionCodeMap s_InstitutionCodeTypeMap;
static bool                    s_InstitutionCollectionCodeMapInitialized = false;
DEFINE_STATIC_FAST_MUTEX(s_InstitutionCollectionCodeMutex);

#include "institution_codes.inc"

static void s_ProcessInstitutionCollectionCodeLine(const CTempString& line)
{
    vector<string> tokens;
    NStr::Tokenize(line, "\t", tokens);
    if (tokens.size() != 3) {
//        ERR_POST_X(1, Warning << "Bad format in institution_codes.txt entry " << line
//                   << "; disregarding");
    } else {
        NStr::TruncateSpacesInPlace( tokens[0] );
        NStr::TruncateSpacesInPlace( tokens[1] );
        NStr::TruncateSpacesInPlace( tokens[2] );
        string& vouch_types = tokens[1];
        for (size_t i = 0; i < vouch_types.size(); i++) {
            switch (vouch_types[i]) {
                case 'b':
                    s_BiomaterialInstitutionCodeMap[tokens[0]] = tokens[2];
                    break;
                case 'c':
                    s_CultureCollectionInstitutionCodeMap[tokens[0]] = tokens[2];
                    break;
                case 's':
                    s_SpecimenVoucherInstitutionCodeMap[tokens[0]] = tokens[2];
                    break;
                default:
//                  ERR_POST_X(1, Warning << "Bad format in institution_codes.txt entry " << line
//                             << "; unrecognized subtype (" << tokens[1] << "); disregarding");
                    break;
            }
        }
        s_CompleteInstitutionCodeMap[tokens[0]] = tokens[2];
        s_CompleteInstitutionFullNameMap[tokens[2]] = tokens[0];
        s_InstitutionCodeTypeMap[tokens[0]] = tokens[1];
    }
}


static void s_InitializeInstitutionCollectionCodeMaps(void)
{
    CFastMutexGuard GUARD(s_InstitutionCollectionCodeMutex);
    if (s_InstitutionCollectionCodeMapInitialized) {
        return;
    }
    string file = g_FindDataFile("institution_codes.txt");
    CRef<ILineReader> lr;
    if ( !file.empty() ) {
        try {
            lr = ILineReader::New(file);
        } NCBI_CATCH("s_InitializeInstitutionCollectionCodeMaps")
    }

    if (lr.Empty()) {
//        ERR_POST_X(2, Info << "s_InitializeInstitutionCollectionCodeMaps: "
//                   "falling back on built-in data.");
        size_t num_codes = sizeof (kInstitutionCollectionCodeList) / sizeof (char *);
        for (size_t i = 0; i < num_codes; i++) {
            const char *p = kInstitutionCollectionCodeList[i];
            s_ProcessInstitutionCollectionCodeLine(p);
        }
    } else {
        do {
            s_ProcessInstitutionCollectionCodeLine(*++*lr);
        } while ( !lr->AtEOF() );
    }

    s_InstitutionCollectionCodeMapInitialized = true;
}


bool COrgMod::IsInstitutionCodeValid(const string& inst_coll, string &voucher_type, bool& is_miscapitalized, string& correct_cap, bool& needs_country, bool& erroneous_country)
{
    bool rval = false;

    is_miscapitalized = false;
    needs_country = false;
    erroneous_country = false;
    correct_cap = "";

    s_InitializeInstitutionCollectionCodeMaps();

    TInstitutionCodeMap::iterator it = s_InstitutionCodeTypeMap.find(inst_coll);
    if (it != s_InstitutionCodeTypeMap.end()) {
        if (NStr::EqualCase (it->first, inst_coll)) {
        } else if (NStr::EqualNocase (it->first, inst_coll)) {
            is_miscapitalized = true;
        }
        voucher_type = it->second;
        correct_cap = it->first;
        rval = true;
    } else {
        size_t pos = NStr::Find(inst_coll, "<");
        if (pos == string::npos) {
            string check = inst_coll + "<";
            it = s_InstitutionCodeTypeMap.begin();
            while (!rval && it != s_InstitutionCodeTypeMap.end()) {
                if (NStr::StartsWith(it->first, check, NStr::eNocase)) {
                    needs_country = true;
                    rval = true;
                    if (!NStr::StartsWith(it->first, check, NStr::eCase)) {
                        is_miscapitalized = true;
                    }
                    correct_cap = it->first.substr(0, inst_coll.length());
                }
                ++it;
            }
        } else {
            string inst_sub = inst_coll.substr(0, pos);
            it = s_InstitutionCodeTypeMap.find(inst_sub);
            if (it != s_InstitutionCodeTypeMap.end()) {
                erroneous_country = true;
                rval = true;
            }
        }
    }
    return rval;
}


string 
COrgMod::IsCultureCollectionValid(const string& culture_collection)
{
    if (NStr::Find(culture_collection, ":") == string::npos) {
        return "Culture_collection should be structured, but is not";
    } else {
        return IsStructuredVoucherValid(culture_collection, "c");
    }
}


string 
COrgMod::IsSpecimenVoucherValid(const string& specimen_voucher)
{
    if (NStr::Find(specimen_voucher, ":") == string::npos) {
        return "";
    } else {
        return IsStructuredVoucherValid(specimen_voucher, "s");
    }
}


string 
COrgMod::IsBiomaterialValid(const string& biomaterial)
{
    if (NStr::Find(biomaterial, ":") == string::npos) {
        return "";
    } else {
        return IsStructuredVoucherValid(biomaterial, "b");
    }
}


string 
COrgMod::IsStructuredVoucherValid(const string& val, const string& v_type)
{
    string inst_code = "";
    string coll_code = "";
    string inst_coll = "";
    string id = "";
    if (!ParseStructuredVoucher(val, inst_code, coll_code, id)) {
        if (NStr::IsBlank(inst_code)) {
            return "Voucher is missing institution code";
        }
        if (NStr::IsBlank(id)) {
             return "Voucher is missing specific identifier";
        }
        return "";
    }

    if (NStr::IsBlank (coll_code)) {
        inst_coll = inst_code;
    } else {
        inst_coll = inst_code + ":" + coll_code;
    }    

    // first, check combination of institution and collection (if collection found)
    string voucher_type;
    bool is_miscapitalized;
    bool needs_country;
    bool erroneous_country;
    string correct_cap;
    if (COrgMod::IsInstitutionCodeValid(inst_coll, voucher_type, is_miscapitalized, correct_cap, needs_country, erroneous_country)) {
        if (needs_country) {
            return "Institution code " + inst_coll + " needs to be qualified with a <COUNTRY> designation";
        } else if (erroneous_country) {
            return "Institution code " + inst_coll + " should not be qualified with a <COUNTRY> designation";
        } else if (is_miscapitalized) {
            return "Institution code " + inst_coll + " exists, but correct capitalization is " + correct_cap;
        } else {   
            if (NStr::FindNoCase(voucher_type, v_type) == string::npos) {
                if (NStr::FindNoCase (voucher_type, "b") != string::npos) {
                    return "Institution code " + inst_coll + " should be bio_material";
                } else if (NStr::FindNoCase (voucher_type, "c") != string::npos) {
                    return "Institution code " + inst_coll + " should be culture_collection";
                } else if (NStr::FindNoCase (voucher_type, "s") != string::npos) {
                    return "Institution code " + inst_coll + " should be specimen_voucher";
                }
            }
            return "";
        } 
    } else if (NStr::StartsWith(inst_coll, "personal", NStr::eNocase)) {
        if (NStr::EqualNocase (inst_code, "personal") && NStr::IsBlank (coll_code)) {
            return "Personal collection does not have name of collector";
        }
        return "";
    } else if (NStr::IsBlank(coll_code)) {
        return "Institution code " + inst_coll + " is not in list";
    } else if (IsInstitutionCodeValid(inst_code, voucher_type, is_miscapitalized, correct_cap, needs_country, erroneous_country)) {
        if (needs_country) {
            return "Institution code in " + inst_coll + " needs to be qualified with a <COUNTRY> designation";
        } else if (erroneous_country) {
            return "Institution code " + inst_code + " should not be qualified with a <COUNTRY> designation";
        } else if (is_miscapitalized) {
            return "Institution code " + inst_code + " exists, but correct capitalization is " + correct_cap;
        } else if (NStr::Equal (coll_code, "DNA")) {
            // DNA is a valid collection for any institution (using bio_material)
            if (!NStr::Equal (voucher_type, "b")) {
                return "DNA should be bio_material";
            }
        } else {
            return "Institution code " + inst_code + " exists, but collection "
                        + inst_coll + " is not in list";
        }
    } else {
        return "Institution code " + inst_coll + " is not in list";
    }
    return "";
}


string COrgMod::MakeStructuredVoucher(const string& inst, const string& coll, const string& id)
{
    string rval = "";
    if (NStr::IsBlank(inst) && NStr::IsBlank(coll) && NStr::IsBlank(id)) {
        rval = "";
    } else if (NStr::IsBlank(inst) && NStr::IsBlank(coll)) {
        rval = id;
    } else if (NStr::IsBlank(coll)) {
        rval = inst + ":" + id;
    } else {
        rval = inst + ":" + coll + ":" + id;
    }
    return rval;
}


// As described in SQD-1655, we can only rescue an unstructured
// structured voucher if it consists of a series of three or
// more letters followed by a series of digits, optionally separated
// by space, and if the series of letters looks up as a valid
// institution code.
bool FindInstCodeAndSpecID(TInstitutionCodeMap& code_map, string& val)
{
    // nothing to do if value is blank
    if (NStr::IsBlank(val)) {
        return false;
    }
    
    // find first non-letter position
    size_t len = 0;
    string::iterator sit = val.begin();
    while (sit != val.end() && isalpha(*sit)) {
        len++;
        sit++;
    }
    if (len < 3 || len == val.length()) {
        // institution code too short or no second token
        return false;
    }
    string inst_code = val.substr(0, len);
    string remainder = val.substr(len);
    NStr::TruncateSpacesInPlace(remainder); 
    if (NStr::IsBlank(remainder)) {
        // no second token
        return false;
    }
    // remainder must be all digits
    sit = remainder.begin();
    while (sit != remainder.end()) {
        if (!isdigit(*sit)) {
            return false;
        }
        sit++;
    }

    bool rval = false;
    TInstitutionCodeMap::iterator it = code_map.find(inst_code);
    if (it != code_map.end()) {
        val = inst_code + ":" + remainder;
        rval = true;
    }

    return rval;
}


bool COrgMod::AddStructureToVoucher(string& val, const string& v_type)
{
    // nothing to do if value is blank
    if (NStr::IsBlank(val)) {
        return false;
    }    

    s_InitializeInstitutionCollectionCodeMaps();
    if (NStr::Find(v_type, "b") != string::npos && FindInstCodeAndSpecID(s_BiomaterialInstitutionCodeMap, val)) {
        return true;
    } else if (NStr::Find(v_type, "c") != string::npos && FindInstCodeAndSpecID(s_CultureCollectionInstitutionCodeMap, val)) {
        return true;
    } else if (NStr::Find(v_type, "s") != string::npos && FindInstCodeAndSpecID(s_SpecimenVoucherInstitutionCodeMap, val)) {
        return true;
    } else {
        return false;
    }            
}


bool 
COrgMod::FixStructuredVoucher(string& val, const string& v_type)
{
    string inst_code = "";
    string coll_code = "";
    string id = "";
    if (!ParseStructuredVoucher(val, inst_code, coll_code, id)
        || NStr::IsBlank(inst_code)) {
        return AddStructureToVoucher(val, v_type);
    }
    bool rval = false;
    bool found = false;
    s_InitializeInstitutionCollectionCodeMaps();

    TInstitutionCodeMap::iterator it = s_InstitutionCodeTypeMap.begin();

    string new_inst_code = inst_code;
    while ((!found) && (it != s_InstitutionCodeTypeMap.end())) {
        if (NStr::Find(it->second, v_type) != string::npos) {
            if (NStr::EqualNocase (it->first, inst_code)) {
                if (!NStr::Equal (it->first, inst_code)) {
                    new_inst_code = it->first;
                    rval = true;
                }
                found = true;
            } else if (NStr::StartsWith(inst_code, it->first)
                       && inst_code.c_str()[it->first.length()] == '<') {
                /*
                new_inst_code = it->first;
                rval = true;
                */
            }
        }
        ++it;
    }


    if (rval) {
        val = MakeStructuredVoucher(new_inst_code, coll_code, id);
    }
    return rval;
}


const string &
COrgMod::GetInstitutionFullName( const string &short_name )
{
    s_InitializeInstitutionCollectionCodeMaps();
    TInstitutionCodeMap::const_iterator iter = s_CompleteInstitutionCodeMap.find( short_name );
    if( iter != s_CompleteInstitutionCodeMap.end() ) {
        return iter->second;
    } else {
        return kEmptyStr;
    }
}

const string &
COrgMod::GetInstitutionShortName( const string &full_name )
{
    s_InitializeInstitutionCollectionCodeMaps();
    TInstitutionCodeMap::const_iterator iter = s_CompleteInstitutionFullNameMap.find( full_name );
    if( iter != s_CompleteInstitutionFullNameMap.end() ) {
        return iter->second;
    } else {
        return kEmptyStr;
    }
}


// look for multiple source vouchers
string COrgMod::CheckMultipleVouchers(const vector<string>& vouchers)
{
    ITERATE(vector<string>, it, vouchers) {
        string inst1 = "", coll1 = "", id1 = "";
        if (!COrgMod::ParseStructuredVoucher(*it, inst1, coll1, id1)) continue;
        if (NStr::EqualNocase(inst1, "personal") || NStr::EqualCase(coll1, "DNA")) continue;

        vector<string>::const_iterator it_next = it;
        for (++it_next; it_next != vouchers.end(); ++it_next) {
            string inst2 = "", coll2 = "", id2 = "";
            if (!COrgMod::ParseStructuredVoucher(*it_next, inst2, coll2, id2)) continue;
            if (NStr::EqualNocase(inst2, "personal") || NStr::EqualCase(coll2, "DNA")) continue;
            if (!NStr::EqualNocase (inst1, inst2) || NStr::IsBlank(inst1)) continue;
            return NStr::EqualNocase(coll1, coll2) && !NStr::IsBlank(coll1) ? "Multiple vouchers with same institution:collection" : "Multiple vouchers with same institution";
        }
    }
    return "";
}


bool s_IsAllDigits(string str)
{
    return (str.find_first_not_of("0123456789") == NPOS);
}


bool s_FixStrainForPrefix(const string& prefix, string& strain)
{
    bool rval = false;

    if (NStr::StartsWith(strain, prefix, NStr::eNocase)) {
        string tmp = strain.substr(prefix.length());
        NStr::TruncateSpacesInPlace(tmp);
        if (NStr::StartsWith(tmp, ":") || NStr::StartsWith(tmp, "/")) {
            tmp = tmp.substr(1);
        }
        NStr::TruncateSpacesInPlace(tmp);
        if (!NStr::IsBlank(tmp) && s_IsAllDigits(tmp)) {
            strain = prefix + " " + tmp;
            rval = true;
        }
    }
    return rval;
}


string s_FixOneStrain( const string& strain)
{
    string new_val = strain;
    if (s_FixStrainForPrefix("ATCC", new_val)) {
        // fixed for ATCC
    } else if (s_FixStrainForPrefix("DSM", new_val)) {
        // fixed for DSM
    } else {
        // no fix
        new_val = "";
    }
    return new_val;
}


string COrgMod::FixStrain( const string& strain)
{
    string new_val = strain;
    vector<string> words;
    vector<string> results;
    NStr::Tokenize(strain, ";", words);
    FOR_EACH_STRING_IN_VECTOR(itr, words) {
        string str = *itr;
        NStr::TruncateSpacesInPlace(str);
        string fixed = s_FixOneStrain(str);
        if (fixed.empty()) {
            results.push_back (str);
        } else {
            results.push_back (fixed);
        }
    }
    return NStr::Join(results,"; ");
}


const char* sm_KnownHostWords[] = {
  "alfalfa",
  "almond",
  "apple",
  "asparagus",
  "badger",
  "bean",
  "bitter melon",
  "blackberry",
  "blossoms",
  "blueberry",
  "bovine",
  "brinjal",
  "broad bean",
  "cabbage",
  "canine",
  "cantaloupe",
  "caprine",
  "carrot",
  "cassava",
  "cat",
  "catfish",
  "cattle",
  "cauliflower",
  "Channel catfish",
  "chestnut",
  "chicken",
  "chimpanzee",
  "clover",
  "corn",
  "cotton",
  "cow",
  "cowpea",
  "crab",
  "cucumber",
  "curd",
  "dairy cow",
  "dog",
  "duck",
  "equine",
  "feline",
  "fish",
  "fox",
  "goat",
  "goldfish",
  "goose",
  "guanabana",
  "honeydew",
  "horse",
  "ice cream",
  "juniper",
  "larva",
  "laurel",
  "leek",
  "lentil",
  "lilac",
  "lily",
  "maize",
  "mamey",
  "mamey sapote",
  "mango",
  "mangrove",
  "mangroves",
  "marigold",
  "marine sponge",
  "melon",
  "mosquito",
  "mulberry",
  "mungbean",
  "nematode",
  "oat",
  "ornamental pear",
  "ovine",
  "papaya",
  "pea",
  "peach",
  "peacock",
  "pear",
  "pepper",
  "pig",
  "pomegranate",
  "porcine",
  "potato",
  "raccoon dog",
  "red fox",
  "rhizospheric soil",
  "rice",
  "salmon",
  "seagrass",
  "sesame",
  "sheep",
  "shrimp",
  "sorghum",
  "sour cherry",
  "sourdough",
  "soybean",
  "sponge",
  "squash",
  "strawberry",
  "sugar beet",
  "sunflower",
  "sweet cherry",
  "swine",
  "tobacco",
  "tomato",
  "turf",
  "turfgrass",
  "turkey",
  "turtle",
  "watermelon",
  "wheat",
  "white clover",
  "willow",
  "wolf",
  "yak",
};


string COrgMod::FixHostCapitalization(const string& value)
{
    string fix = value;

    size_t max = sizeof(sm_KnownHostWords) / sizeof(const char*);
    for (size_t i = 0; i < max; i++) {
        if (NStr::EqualNocase(fix, sm_KnownHostWords[i])) {
            fix = sm_KnownHostWords[i];
            break;
        }
    }
    return fix;
}


string COrgMod::FixHost(const string& value)
{
    string fix = value;
    if (NStr::EqualNocase(fix, "human")) {
        fix = "Homo sapiens";
    }

    return fix;
}


string COrgMod::FixCapitalization(TSubtype subtype, const string& value)
{
    string new_val = value;
    switch (subtype) {
        case COrgMod::eSubtype_nat_host:
            new_val = FixHostCapitalization(value);
            break;
        default:
            new_val = value;
            break;
    }
    return new_val;
}


void COrgMod::FixCapitalization()
{
    if (!IsSetSubtype() || !IsSetSubname()) {
        return;
    }

    string new_val = FixCapitalization(GetSubtype(), GetSubname());

    if (!NStr::IsBlank(new_val)) {
        SetSubname(new_val);
    }

}


string COrgMod::AutoFix(TSubtype subtype, const string& value)
{
    string new_val = "";
    switch (subtype) {
        case COrgMod::eSubtype_strain:
            new_val = FixStrain(value);
            break;
        case COrgMod::eSubtype_nat_host:
            new_val = FixHost(value);
            break;
        default:
            break;
    }
    return new_val;
}


void COrgMod::AutoFix()
{
    if (!IsSetSubtype() || !IsSetSubname()) {
        return;
    }

    string new_val = AutoFix(GetSubtype(), GetSubname());

    if (!NStr::IsBlank(new_val)) {
        SetSubname(new_val);
    }

}


void s_HarmonizeString(string& s) 
{
    NStr::ReplaceInPlace (s, " ", "");
    NStr::ReplaceInPlace (s, "_", "");
    NStr::ReplaceInPlace (s, "-", "");
    NStr::ReplaceInPlace (s, ":", "");
    NStr::ReplaceInPlace (s, "/", "");
}


bool COrgMod::FuzzyStrainMatch( const string& strain1, const string& strain2 )
{
    string s1 = strain1;
    string s2 = strain2;

    s_HarmonizeString(s1);
    s_HarmonizeString(s2);
    return NStr::EqualNocase(s1, s2);    
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 65, chars: 1882, CRC32: efba64e1 */
