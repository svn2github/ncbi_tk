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
#include <serial/enumvalues.hpp>

// generated includes
#include <objects/seqfeat/SubSource.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CSubSource::~CSubSource(void)
{
}


void CSubSource::GetLabel(string* str) const
{
    *str += '/';
    string type_name;
    if (GetSubtype() == eSubtype_other) {
        type_name = "other";
    } else {
        try {
            // eVocabulary_insdc has some special cases not (historically)
            // used here.
            type_name = GetSubtypeName(GetSubtype());
            replace(type_name.begin(), type_name.end(), '_', '-');
        } catch (CSerialException&) {
            type_name = "unknown";
        }
    }
    *str += type_name;
    *str += '=';
    *str += GetName();
    if (IsSetAttrib()) {
        *str += " (";
        *str += GetAttrib();
        *str += ")";
    }
}


CSubSource::TSubtype CSubSource::GetSubtypeValue(const string& str,
                                                 EVocabulary vocabulary)
{
    string name = NStr::TruncateSpaces(str);
    NStr::ToLower(name);
    replace(name.begin(), name.end(), '_', '-');

    if (name == "note") {
        return eSubtype_other;
    } else if (vocabulary == eVocabulary_insdc) {
        // consider a table if more special cases arise.
        if (name == "insertion-seq") {
            return eSubtype_insertion_seq_name;
        } else if (name == "plasmid") {
            return eSubtype_plasmid_name;
        } else if (name == "transposon") {
            return eSubtype_transposon_name;
        } else if (name == "sub-clone") {
            return eSubtype_subclone;
        }
    }

    return ENUM_METHOD_NAME(ESubtype)()->FindValue(str);
}


string CSubSource::GetSubtypeName(CSubSource::TSubtype stype,
                                  EVocabulary vocabulary)
{
    if (stype == CSubSource::eSubtype_other) {
        return "note";
    } else if (vocabulary == eVocabulary_insdc) {
        switch (stype) {
        case eSubtype_subclone:           return "sub_clone";
        case eSubtype_plasmid_name:       return "plasmid";
        case eSubtype_transposon_name:    return "transposon";
        case eSubtype_insertion_seq_name: return "insertion_seq";
        default:
            return NStr::Replace
                (ENUM_METHOD_NAME(ESubtype)()->FindName(stype, true),
                 "-", "_");
        }
    } else {
        return ENUM_METHOD_NAME(ESubtype)()->FindName(stype, true);
    }
}




bool CSubSource::NeedsNoText(const TSubtype& subtype)
{
    if (subtype == eSubtype_germline
        || subtype == eSubtype_rearranged
        || subtype == eSubtype_transgenic
        || subtype == eSubtype_environmental_sample
        || subtype == eSubtype_metagenomic) {
        return true;
    } else {
        return false;
    }
}


const string sm_LegalMonths [] = {
  "Jan",
  "Feb",
  "Mar",
  "Apr",
  "May",
  "Jun",
  "Jul",
  "Aug",
  "Sep",
  "Oct",
  "Nov",
  "Dec",
};


const int sm_daysPerMonth [] = {
  31,
  28,
  31,
  30,
  31,
  30,
  31,
  31,
  30,
  31,
  30,
  31
};

CRef<CDate> CSubSource::DateFromCollectionDate (const string& str) THROWS((CException))
{
    if (NStr::IsBlank(str)) {
        NCBI_THROW (CException, eUnknown,
                        "collection-date string is blank");
    }
    size_t pos = NStr::Find(str, "-");
    string year = "";
    string month = "";
    string day = "";
    int dpm = 0;

    if (pos == string::npos) {
        year = str;
    } else {
        size_t pos2 = NStr::Find(str, "-", pos + 1);
        if (pos2 == string::npos) {
            month = str.substr(0, pos);
            year = str.substr(pos + 1);
            if (NStr::IsBlank(month)) {
                NCBI_THROW (CException, eUnknown,
                                "collection-date string is improperly formatted");
            }
        } else {
            day = str.substr(0, pos);
            month = str.substr(pos + 1, pos2 - pos - 1);
            year = str.substr(pos2 + 1);
            if (NStr::IsBlank(month) || NStr::IsBlank(day)) {
                NCBI_THROW (CException, eUnknown,
                                "collection-date string is improperly formatted");
            }
        }
    }

    int day_val = 0;
    if (!NStr::IsBlank(day)) {
        try {
            day_val = NStr::StringToInt (day);
            if (day_val < 1 || day_val > 31) {
                NCBI_THROW (CException, eUnknown,
                                "collection-date string has invalid day value");
            }
        } catch ( const exception& ) {
            // threw exception while converting to int
            NCBI_THROW (CException, eUnknown,
                            "collection-date string is improperly formatted");
        }
    }

    int month_val = 0;
    if (!NStr::IsBlank(month)) {
        for (size_t i = 0; i < sizeof(sm_LegalMonths) / sizeof(string); i++) {
            if (NStr::Equal(month, sm_LegalMonths[i])) {
                dpm = sm_daysPerMonth [i];
                month_val = i + 1;
                break;
            }
        }
        if (month_val == 0) {
            NCBI_THROW (CException, eUnknown,
                            "collection-date string has invalid month");
        }
    }

    if (NStr::IsBlank(year)) {
        NCBI_THROW (CException, eUnknown,
                        "collection-date string is improperly formatted");
    } 

    int year_val = 0;
    try {
        year_val = NStr::StringToInt (year);
    } catch ( const exception& ) {
        // threw exception while converting to int
        NCBI_THROW (CException, eUnknown,
                        "collection-date string is improperly formatted");
    }

    if (year_val < 1700 || year_val >= 2100) {
        NCBI_THROW (CException, eUnknown,
                        "collection-date year is out of range");
    }

    if (day_val > 0 && dpm > 0 && day_val > dpm) {
        if (month_val != 2 || day_val != 29 || (year_val % 4) != 0) {
            NCBI_THROW (CException, eUnknown,
                            "collection-date day is greater than monthly maximum");
        }
    }

    CRef<CDate> date(new CDate);

    date->SetStd().SetYear (year_val);
    if (month_val > 0) {
        date->SetStd().SetMonth (month_val);
    }
    if (day_val > 0) {
        date->SetStd().SetDay (day_val);
    }
    
    return date;
}


string CSubSource::GetCollectionDateProblem (const string& date_string)
{
    string problem = "";

    try {
        CRef<CDate> coll_date = CSubSource::DateFromCollectionDate (date_string);

        // if there are two dashes, then the first token needs to be the day, and the
        // day has to have two numbers, a leading zero if the day is less than 10
        bool is_bad = false;
        size_t pos = NStr::Find(date_string, "-");
        if (pos != string::npos) {
            size_t pos2 = NStr::Find(date_string, "-", pos + 1);
            if (pos2 != string::npos && pos != 2) {
                problem = "Collection_date format is not in DD-Mmm-YYYY format";
                is_bad = true;
            }
        }

        if (!is_bad) {         
            struct tm *tm;
            time_t t;

            time(&t);
            tm = localtime(&t);

            if (coll_date->GetStd().GetYear() > tm->tm_year + 1900) {
                problem = "Collection_date is in the future";
            } else if (coll_date->GetStd().GetYear() == tm->tm_year + 1900
                       && coll_date->GetStd().IsSetMonth()) {
                if (coll_date->GetStd().GetMonth() > tm->tm_mon + 1) {
                    problem = "Collection_date is in the future";
                } else if (coll_date->GetStd().GetMonth() == tm->tm_mon + 1
                           && coll_date->GetStd().IsSetDay()) {
                    if (coll_date->GetStd().GetDay() > tm->tm_mday) {
                        problem = "Collection_date is in the future";
                    }
                }
            }
        }
    } catch (CException ) {
        problem = "Collection_date format is not in DD-Mmm-YYYY format";
    }
    return problem;
}


string CSubSource::ReformatDate (string orig_date, bool month_first, bool& month_ambiguous)
{
    string reformatted_date = "";
    string month = "";
    int year = 0, day = 0;
    string token_delimiters = " ,-/";
    size_t i;

    month_ambiguous = false;
    NStr::TruncateSpacesInPlace (orig_date);
    vector<string> tokens;
    string one_token;    
    for (i = 0; i < 4; i++) {
        one_token = NStr::GetField (orig_date, i, token_delimiters, NStr::eMergeDelims);
        if (NStr::IsBlank (one_token)) {
            break;
        } else {
            tokens.push_back (one_token);
        }
    }
    if (tokens.size() < 1 || tokens.size() > 3) {
        // no tokens or too many tokens
        return "";
    }

    vector<string>::iterator it = tokens.begin();
    while (it != tokens.end()) {
        one_token = *it;
        bool found = false;
        if (isalpha(one_token.c_str()[0])) {
            if (!NStr::IsBlank (month)) {
                // already have month, error
                return "";
            }
            for (size_t i = 0; i < sizeof(sm_LegalMonths) / sizeof(string); i++) {
                if (NStr::StartsWith(one_token, sm_LegalMonths[i], NStr::eNocase)) {
                    month = sm_LegalMonths[i];
                    found = true;
                    break;
                }
            }
        } else {
            try {
                int val = NStr::StringToInt (one_token);
                if (val > 31) {
                    year = val;
                    found = true;
                } else if (val < 1) {
                    // numbers should not be less than 1
                    return "";
                }
            } catch ( const exception& ) {
                // threw exception while converting to int
                return "";
            }
        }
        if (found) {
            it = tokens.erase(it);
        } else {
            it++;
        }
    }

    if (tokens.size() == 0) {
        // good - all tokens assigned to values
    } else if (tokens.size() > 2) {
        // three numbers, none of them are year, can't resolve
        return "";
    } else if (tokens.size() == 1) {
        int val = NStr::StringToInt (tokens[0]);
        if (year == 0) {
            year = val;
        } else {
            if (NStr::IsBlank (month)) {
                if (val > 0 && val < 13) {
                    month = sm_LegalMonths[val - 1];
                } else {
                    // month number out of range
                    return "";
                }
            } else {
                day = val;
            }
        }
    } else if (!NStr::IsBlank (month)) {
        // shouldn't happen
        return "";
    } else {
        int val1 = NStr::StringToInt (tokens[0]); 
        int val2 = NStr::StringToInt (tokens[1]);
        if (val1 > 12 && val2 > 12) {
            // both numbers too big for month
            return "";
        } else if (val1 < 13 && val2 < 13) {
            // both numbers could be month
            month_ambiguous = true;
            if (month_first) {
                month = sm_LegalMonths[val1 - 1];
                day = val2;
            } else {
                month = sm_LegalMonths[val2 - 1];
                day = val1;
            }
        } else if (val1 < 13) {
            month = sm_LegalMonths[val1 - 1];
            day = val2;
        } else {
            month = sm_LegalMonths[val2 - 1];
            day = val1;
        }
    }
                
    if (year > 0) {
        reformatted_date = NStr::NumericToString (year);
        if (!NStr::IsBlank (month)) {
            reformatted_date = month + "-" + reformatted_date;
            if (day > 0) {
                string day_str = NStr::NumericToString (day);
                if (day_str.length() < 2) {
                    day_str = "0" + day_str;
                }
                reformatted_date = day_str + "-" + reformatted_date;
            }
        }
    }

    return reformatted_date;
}


void CSubSource::IsCorrectLatLonFormat (string lat_lon, bool& format_correct, bool& precision_correct,
                                     bool& lat_in_range, bool& lon_in_range,
                                     double& lat_value, double& lon_value)
{
    format_correct = false;
    lat_in_range = false;
    lon_in_range = false;
    precision_correct = false;
    double ns, ew;
    char lon, lat;
    int processed;

    lat_value = 0.0;
    lon_value = 0.0;

    if (NStr::IsBlank(lat_lon)) {
        return;
    } else if (sscanf (lat_lon.c_str(), "%lf %c %lf %c%n", &ns, &lat, &ew, &lon, &processed) != 4
               || processed != lat_lon.length()) {
        return;
    } else if ((lat != 'N' && lat != 'S') || (lon != 'E' && lon != 'W')) {
        return;
    } else {
        // init values found
        if (lat == 'N') {
            lat_value = ns;
        } else {
            lat_value = 0.0 - ns;
        }
        if (lon == 'E') {
            lon_value = ew;
        } else {
            lon_value = 0.0 - ew;
        }

    // make sure format is correct
        vector<string> pieces;
        NStr::Tokenize(lat_lon, " ", pieces);
        if (pieces.size() > 3) {
            int precision_lat = 0;
            size_t pos = NStr::Find(pieces[0], ".");
            if (pos != string::npos) {
                precision_lat = pieces[0].length() - pos - 1;
            }
            int precision_lon = 0;
            pos = NStr::Find(pieces[2], ".");
            if (pos != string::npos) {
                precision_lon = pieces[2].length() - pos - 1;
            }

            char reformatted[1000];
            sprintf (reformatted, "%.*lf %c %.*lf %c", precision_lat, ns, lat,
                                                       precision_lon, ew, lon);

            size_t len = strlen (reformatted);
            if (NStr::StartsWith(lat_lon, reformatted)
                && (len == lat_lon.length() 
                  || (len < lat_lon.length() 
                      && lat_lon.c_str()[len] == ';'))) {
                format_correct = true;
                if (ns <= 90 && ns >= 0) {
                    lat_in_range = true;
                }
                if (ew <= 180 && ew >= 0) {
                    lon_in_range = true;
                }
                if (precision_lat < 3 && precision_lon < 3) {
                    precision_correct = true;
                }
            }
        }
    }
}


static void s_TrimInternalSpaces (string& token)
{
    size_t pos;

    while ((pos = NStr::Find (token, "  ")) != string::npos) {
        string before = token.substr(0, pos);
        string after = token.substr(pos + 1);
        token = before + after;
    }
    while ((pos = NStr::Find (token, " '")) != string::npos) {
        string before = token.substr(0, pos);
        string after = token.substr(pos + 1);
        token = before + after;
    }
}


static string s_GetNumFromLatLonToken (string token)
{
    NStr::TruncateSpacesInPlace(token);
    string dir = "";
    if (NStr::StartsWith (token, "N") || NStr::StartsWith (token, "S") || NStr::StartsWith (token, "E") || NStr::StartsWith (token, "W")) {
        dir = token.substr(0, 1);
        token = token.substr(1);
    } else {
        dir = token.substr(token.length() - 1, 1);
        token = token.substr(0, token.length() - 1);
    }
    NStr::TruncateSpacesInPlace(token);
    // clean up double spaces, spaces between numbers and '
    s_TrimInternalSpaces(token);

    size_t pos = 0;
    double val = 0;
    size_t num_sep = 0, prev_start = 0;
    int prec = 0;
    while (pos < token.length()) {
        char ch = token.c_str()[pos];
        if (ch == ' ' || ch == ':' || ch == '-') {
            string num_str = token.substr(prev_start, pos - prev_start);
            double this_val = NStr::StringToDouble (num_str);
            if (num_sep == 0) {
                val += this_val;
            } else if (num_sep == 1) {
                val += (this_val) / (60.0);
                prec += 2;
            } else if (num_sep == 2) {
                val += (this_val) / (3600.0);
                prec += 2;
            } else {
                // too many separators
                return "";
            }
            size_t p_pos = NStr::Find (num_str, ".");
            if (p_pos != string::npos) {
                prec += num_str.substr(p_pos + 1).length();
            }
            num_sep++;
            pos++;
            prev_start = pos;
        } else if (ch == '\'') {
            string num_str = token.substr(prev_start, pos - prev_start);
            double this_val = NStr::StringToDouble (num_str);
            if (token.c_str()[pos + 1] == '\'') {
                if (num_sep == 2) {
                    // already found seconds
                    return "";
                }
                // seconds
                val += (this_val) / (3600.0);
                prec += 2;
                if (NStr::Find (token, "'") == pos) {
                    // seconds specified without minutes
                    prec += 2;
                }
                pos++;
                num_sep = 2;
            } else {
                if (num_sep > 1) {
                    // already found seconds
                    return "";
                }
                val += (this_val) / (60.0);
                prec += 2;
                num_sep = 1;
            }
            size_t p_pos = NStr::Find (num_str, ".");
            if (p_pos != string::npos) {
                prec += num_str.substr(p_pos + 1).length();
            }
            pos++;
            while (isspace (token.c_str()[pos])) {
                pos++;
            }
            prev_start = pos;
        } else if (isdigit(ch) || ch == '.') {
            pos++;
        } else {
            return "";
        }
    }

    if (prev_start == 0) {
        return token + " " + dir;
    } else {
        if (prev_start < pos) {
            string num_str = token.substr(prev_start, pos - prev_start);
            double this_val = NStr::StringToDouble (num_str);
            if (num_sep == 0) {
                val += this_val;
            } else if (num_sep == 1) {
                val += (this_val) / (60.0);
                prec += 2;
            } else if (num_sep == 2) {
                val += (this_val) / (3600.0);
                prec += 2;
            } else {
                // too many separators
                return "";
            }
            size_t p_pos = NStr::Find (num_str, ".");
            if (p_pos != string::npos) {
                prec += num_str.substr(p_pos + 1).length();
            }
        }
        if (prec > 0) {
            double mult = pow ((double)10.0, prec);
            bool round_down = true;
            double remainder = (val * mult) - floor (val * mult);
            if (remainder > 0.5) {
                round_down = false;
            }
            double tmp;
            if (round_down) {
                tmp = floor(val * mult);
            } else {
                tmp = ceil(val * mult);
            }
            val = tmp / mult;
        }
        string val_str = NStr::NumericToString (val, NStr::fDoubleFixed);
        pos = NStr::Find (val_str, ".");
        if (pos != string::npos && prec > 0) {
            while (val_str.substr(pos + 1).length() < prec) {
                val_str += "0";
            }
            if (val_str.substr(pos + 1).length() > prec) {
                val_str = val_str.substr (0, pos + prec + 1);
            }
        }
        return val_str + " " + dir;
    }            
}


static void s_RemoveLeadingCommaOrSemicolon(string& token)
{
    NStr::TruncateSpacesInPlace(token);
    if (NStr::StartsWith (token, ",") || NStr::StartsWith (token, ";")) {
        token = token.substr(1);
        NStr::TruncateSpacesInPlace(token);
    }
}


static void s_RemoveExtraText (string& token, string& extra_text)
{
    size_t comma_pos = NStr::Find (token, ",");
    size_t semicolon_pos = NStr::Find (token, ";");
    size_t sep_pos = string::npos;

    if (comma_pos == string::npos) {
        sep_pos = semicolon_pos;
    } else if (semicolon_pos != string::npos) {
        if (semicolon_pos < comma_pos) {
            sep_pos = semicolon_pos;
        } else {
            sep_pos = comma_pos;
        }
    }
    if (sep_pos != string::npos) {
        extra_text = token.substr(sep_pos + 1);
        NStr::TruncateSpacesInPlace(extra_text);
        token = token.substr(0, sep_pos - 1);
        NStr::TruncateSpacesInPlace(token);
    }        
}


string CSubSource::FixLatLonFormat (string orig_lat_lon)
{
    string cpy;
    size_t pos;

    if (NStr::IsBlank (orig_lat_lon))
    {
        return "";
    }

    cpy = orig_lat_lon;
    
    // replace all 'O' (capital o) following non-alpha characters with '0' (zero)
    pos = NStr::Find (cpy, "O");
    while (pos != string::npos) {
        if (pos > 0 && !isalpha(cpy.c_str()[pos - 1])) {
            string before = cpy.substr(0, pos);
            string after = cpy.substr(pos + 1);
            cpy = before + "0" + after;
        }
        pos = NStr::Find (cpy, "O", pos + 1);
    }
    // replace all 'o' with non-alpha characters before and after with space
    pos = NStr::Find (cpy, "o");
    while (pos != string::npos) {
        if ((pos == 0 || !isalpha(cpy.c_str()[pos - 1]))
            && !isalpha (cpy.c_str()[pos + 1])) {
            string before = cpy.substr(0, pos);
            string after = cpy.substr(pos + 1);
            cpy = before + " " + after;
        }
        pos = NStr::Find (cpy, "o", pos + 1);
    }

    // replace all '#' with ' '
    NStr::ReplaceInPlace (cpy, "#", " ");

    // now make all letters uppercase (note, do not do this before converting 'o' and 'O'(
    cpy = NStr::ToUpper (cpy);

    // replace commas that should be periods
    pos = NStr::Find (cpy, ",");
    while (pos != string::npos) {
        if (pos > 0 && isdigit (cpy.c_str()[pos + 1]) && isdigit (cpy.c_str()[pos - 1])) {
            // follow digits all the way back, make sure character before digits is not '.'
            size_t dpos = pos - 2;
            while (dpos > 0 && isdigit (cpy.c_str()[dpos])) {
                dpos--;
            }
            if (cpy.c_str()[dpos] != '.') {
                string before = cpy.substr(0, pos);
                string after = cpy.substr(pos + 1);
                cpy = before + " " + after;
            }
        }
        pos = NStr::Find (cpy, "o", pos + 1);
    }

    // replace typo semicolon with colon
    pos = NStr::Find (cpy, ";");
    while (pos != string::npos) {
        if (pos > 0 && isdigit (cpy.c_str()[pos - 1]) && isdigit (cpy.c_str()[pos + 1])) {
            string before = cpy.substr(0, pos);
            string after = cpy.substr(pos + 1);
            cpy = before + ":" + after;
            pos = NStr::Find (cpy, ";");
        } else {
          pos = NStr::Find (cpy, ";", pos + 1);
        }
    }

    NStr::ReplaceInPlace (cpy, "LONGITUDE", "LONG");
    NStr::ReplaceInPlace (cpy, "LON.",      "LONG");
    NStr::ReplaceInPlace (cpy, "LONG",      "LO");
    NStr::ReplaceInPlace (cpy, "LO:",       "LO");
    NStr::ReplaceInPlace (cpy, "LATITUDE",  "LAT");
    NStr::ReplaceInPlace (cpy, "LAT.",      "LAT");
    NStr::ReplaceInPlace (cpy, "LAT:",      "LAT");
    NStr::ReplaceInPlace (cpy, "DEGREES",   " " );
    NStr::ReplaceInPlace (cpy, "DEGREE",    " ");
    NStr::ReplaceInPlace (cpy, "DEG.",      " ");
    NStr::ReplaceInPlace (cpy, "DEG",       " " );
    NStr::ReplaceInPlace (cpy, "MASCULINE", " "); // masculine ordinal indicator U+00BA often confused with degree sign U+00B0
    NStr::ReplaceInPlace (cpy, "MIN.",      "'");
    NStr::ReplaceInPlace (cpy, "MINUTES",   "'");
    NStr::ReplaceInPlace (cpy, "MINUTE",    "'");
    NStr::ReplaceInPlace (cpy, "MIN",       "'");
    NStr::ReplaceInPlace (cpy, "SEC.",      "''");
    NStr::ReplaceInPlace (cpy, "SEC",       "''");
    NStr::ReplaceInPlace (cpy, "NORTH",     "N");
    NStr::ReplaceInPlace (cpy, "SOUTH",     "S");
    NStr::ReplaceInPlace (cpy, "EAST",      "E");
    NStr::ReplaceInPlace (cpy, "WEST",      "W");
    NStr::ReplaceInPlace (cpy, "  ", " "); // double-spaces become single spaces

    size_t lat_pos = NStr::Find (cpy, "LAT");
    size_t lon_pos = NStr::Find (cpy, "LO");
    if ((lat_pos == string::npos && lon_pos != string::npos)
        || (lat_pos != string::npos && lon_pos == string::npos)) {
        // must specify both lat and lon or neither
        return "";
    }
    if (lat_pos != string::npos && NStr::Find (cpy, "LAT", lat_pos + 1) != string::npos) {
        // better not find two
        return "";
    }
    if (lon_pos != string::npos && NStr::Find (cpy, "LO", lon_pos + 1) != string::npos) {
        // better not find two
        return "";
    }

    size_t ns_pos = NStr::Find (cpy, "N");
    if (ns_pos == string::npos) {
        ns_pos = NStr::Find (cpy, "S");
        if (ns_pos != string::npos && NStr::Find(cpy, "S", ns_pos + 1) != string::npos) {
            // better not find two
            return "";
        }
    } else if (NStr::Find (cpy, "S") != string::npos || NStr::Find (cpy, "N", ns_pos + 1) != string::npos) {
        // better not find two
        return "";
    }

    size_t ew_pos = NStr::Find (cpy, "E");
    if (ew_pos == string::npos) {
        ew_pos = NStr::Find (cpy, "W");
        if (ew_pos != string::npos && NStr::Find(cpy, "W", ew_pos + 1) != string::npos) {
            // better not find two
            return "";
        }
    } else if (NStr::Find (cpy, "W") != string::npos || NStr::Find (cpy, "E", ew_pos + 1) != string::npos) {
        // better not find two
        return "";
    }

    // todo - figure out how to use degrees as ew_pos markers

    // need to have both or neither
    if ((ns_pos == string::npos && ew_pos != string::npos)
        || (ns_pos != string::npos && ew_pos == string::npos)) {
        return "";
    }

    string extra_text = "";
    string la_token = "";
    string lo_token = "";
    bool la_first = true;

    if (lat_pos == string::npos) {
        if (ns_pos == string::npos) {
            return "";
        }
        if (ns_pos < ew_pos) {
            // as it should be
            if (ns_pos == 0) {
                // letter is first, token ends with ew_pos
                la_token = cpy.substr(0, ew_pos);
                lo_token = cpy.substr(ew_pos);
            } else {
                // letter is last, token ends here
                la_token = cpy.substr(0, ns_pos + 1);
                lo_token = cpy.substr(ns_pos + 1);
            }
        } else {
            // positions reversed
            la_first = false;
            if (ew_pos == 0) {
                // letter is first, token ends with ns_pos
                lo_token = cpy.substr(0, ns_pos);
                la_token = cpy.substr(ns_pos);
            } else {
                // letter is last, token ends here
                lo_token = cpy.substr(0, ew_pos + 1);
                la_token = cpy.substr(ew_pos + 1);
            }
        }            
    } else {
        if (lat_pos < lon_pos) {
            // as it should be
            if (lat_pos == 0) {
                // letter is first, token ends with lon_pos
                la_token = cpy.substr(3, lon_pos - 3);
                lo_token = cpy.substr(lon_pos + 2);
            } else {
                // letter is last, token ends here
                la_token = cpy.substr(0, lat_pos);
                lo_token = cpy.substr(lat_pos + 3);
            }
        } else {
            // positions reversed
            la_first = false;
            if (lon_pos == 0) {
                // letter is first, token ends with lat_pos
                lo_token = cpy.substr(2, lat_pos);
                la_token = cpy.substr(lat_pos + 3);
            } else {
                // letter is last, token ends here
                lo_token = cpy.substr(0, lon_pos);
                la_token = cpy.substr(lon_pos + 2);
            }
        }
        NStr::ReplaceInPlace (la_token, "LAT", "");
        NStr::ReplaceInPlace (lo_token, "LO", "");
        if (NStr::Find (la_token, "E") != string::npos || NStr::Find (la_token, "W") != string::npos) {
            return "";
        } else if (NStr::Find (lo_token, "N") != string::npos || NStr::Find (lo_token, "S") != string::npos) {
            return "";
        }
    }
    if (la_first) {
        NStr::ReplaceInPlace (la_token, ",", "");
        NStr::ReplaceInPlace (la_token, ";", "");
        s_RemoveLeadingCommaOrSemicolon (lo_token);
        s_RemoveExtraText (lo_token, extra_text);
    } else {
        NStr::ReplaceInPlace (lo_token, ",", "");
        NStr::ReplaceInPlace (lo_token, ";", "");
        s_RemoveLeadingCommaOrSemicolon (la_token);
        s_RemoveExtraText (lo_token, extra_text);
    }

    la_token = s_GetNumFromLatLonToken (la_token);
    if (NStr::IsBlank (la_token)) {
        return "";
    }

    lo_token = s_GetNumFromLatLonToken (lo_token);
    if (NStr::IsBlank (lo_token)) {
        return "";
    }

    string fix = la_token + " " + lo_token;
    if (!NStr::IsBlank (extra_text)) {
        fix += ",";
        fix += " " + extra_text;
    }
    return fix;
}


// =============================================================================
//                                 Country Names
// =============================================================================


// legal country names
const string CCountries::sm_Countries[] = {
    "Afghanistan",
    "Albania",
    "Algeria",
    "American Samoa",
    "Andorra",
    "Angola",
    "Anguilla",
    "Antarctica",
    "Antigua and Barbuda",
    "Arctic Ocean",
    "Argentina",
    "Armenia",
    "Aruba",
    "Ashmore and Cartier Islands",
    "Atlantic Ocean",
    "Australia",
    "Austria",
    "Azerbaijan",
    "Bahamas",
    "Bahrain",
    "Baker Island",
    "Baltic Sea",
    "Bangladesh",
    "Barbados",
    "Bassas da India",
    "Belarus",
    "Belgium",
    "Belize",
    "Benin",
    "Bermuda",
    "Bhutan",
    "Bolivia",
    "Borneo",
    "Bosnia and Herzegovina",
    "Botswana",
    "Bouvet Island",
    "Brazil",
    "British Virgin Islands",
    "Brunei",
    "Bulgaria",
    "Burkina Faso",
    "Burundi",
    "Cambodia",
    "Cameroon",
    "Canada",
    "Cape Verde",
    "Cayman Islands",
    "Central African Republic",
    "Chad",
    "Chile",
    "China",
    "Christmas Island",
    "Clipperton Island",
    "Cocos Islands",
    "Colombia",
    "Comoros",
    "Cook Islands",
    "Coral Sea Islands",
    "Costa Rica",
    "Cote d'Ivoire",
    "Croatia",
    "Cuba",
    "Curacao",
    "Cyprus",
    "Czech Republic",
    "Democratic Republic of the Congo",
    "Denmark",
    "Djibouti",
    "Dominica",
    "Dominican Republic",
    "East Timor",
    "Ecuador",
    "Egypt",
    "El Salvador",
    "Equatorial Guinea",
    "Eritrea",
    "Estonia",
    "Ethiopia",
    "Europa Island",
    "Falkland Islands (Islas Malvinas)",
    "Faroe Islands",
    "Fiji",
    "Finland",
    "France",
    "French Guiana",
    "French Polynesia",
    "French Southern and Antarctic Lands",
    "Gabon",
    "Gambia",
    "Gaza Strip",
    "Georgia",
    "Germany",
    "Ghana",
    "Gibraltar",
    "Glorioso Islands",
    "Greece",
    "Greenland",
    "Grenada",
    "Guadeloupe",
    "Guam",
    "Guatemala",
    "Guernsey",
    "Guinea-Bissau",
    "Guinea",
    "Guyana",
    "Haiti",
    "Heard Island and McDonald Islands",
    "Honduras",
    "Hong Kong",
    "Howland Island",
    "Hungary",
    "Iceland",
    "India",
    "Indian Ocean",
    "Indonesia",
    "Iran",
    "Iraq",
    "Ireland",
    "Isle of Man",
    "Israel",
    "Italy",
    "Jamaica",
    "Jan Mayen",
    "Japan",
    "Jarvis Island",
    "Jersey",
    "Johnston Atoll",
    "Jordan",
    "Juan de Nova Island",
    "Kazakhstan",
    "Kenya",
    "Kerguelen Archipelago",
    "Kingman Reef",
    "Kiribati",
    "Kosovo",
    "Kuwait",
    "Kyrgyzstan",
    "Laos",
    "Latvia",
    "Lebanon",
    "Lesotho",
    "Liberia",
    "Libya",
    "Liechtenstein",
    "Lithuania",
    "Luxembourg",
    "Macau",
    "Macedonia",
    "Madagascar",
    "Malawi",
    "Malaysia",
    "Maldives",
    "Mali",
    "Malta",
    "Marshall Islands",
    "Martinique",
    "Mauritania",
    "Mauritius",
    "Mayotte",
    "Mediterranean Sea",
    "Mexico",
    "Micronesia",
    "Midway Islands",
    "Moldova",
    "Monaco",
    "Mongolia",
    "Montenegro",
    "Montserrat",
    "Morocco",
    "Mozambique",
    "Myanmar",
    "Namibia",
    "Nauru",
    "Navassa Island",
    "Nepal",
    "Netherlands",
    "New Caledonia",
    "New Zealand",
    "Nicaragua",
    "Niger",
    "Nigeria",
    "Niue",
    "Norfolk Island",
    "North Korea",
    "North Sea",
    "Northern Mariana Islands",
    "Norway",
    "Oman",
    "Pacific Ocean",
    "Pakistan",
    "Palau",
    "Palmyra Atoll",
    "Panama",
    "Papua New Guinea",
    "Paracel Islands",
    "Paraguay",
    "Peru",
    "Philippines",
    "Pitcairn Islands",
    "Poland",
    "Portugal",
    "Puerto Rico",
    "Qatar",
    "Republic of the Congo",
    "Reunion",
    "Romania",
    "Ross Sea",
    "Russia",
    "Rwanda",
    "Saint Helena",
    "Saint Kitts and Nevis",
    "Saint Lucia",
    "Saint Pierre and Miquelon",
    "Saint Vincent and the Grenadines",
    "Samoa",
    "San Marino",
    "Sao Tome and Principe",
    "Saudi Arabia",
    "Senegal",
    "Serbia",
    "Seychelles",
    "Sierra Leone",
    "Singapore",
    "Sint Maarten",
    "Slovakia",
    "Slovenia",
    "Solomon Islands",
    "Somalia",
    "South Africa",
    "South Georgia and the South Sandwich Islands",
    "South Korea",
    "Southern Ocean",
    "Spain",
    "Spratly Islands",
    "Sri Lanka",
    "Sudan",
    "Suriname",
    "Svalbard",
    "Swaziland",
    "Sweden",
    "Switzerland",
    "Syria",
    "Taiwan",
    "Tajikistan",
    "Tanzania",
    "Tasman Sea",
    "Thailand",
    "Togo",
    "Tokelau",
    "Tonga",
    "Trinidad and Tobago",
    "Tromelin Island",
    "Tunisia",
    "Turkey",
    "Turkmenistan",
    "Turks and Caicos Islands",
    "Tuvalu",
    "Uganda",
    "Ukraine",
    "United Arab Emirates",
    "United Kingdom",
    "Uruguay",
    "USA",
    "Uzbekistan",
    "Vanuatu",
    "Venezuela",
    "Viet Nam",
    "Virgin Islands",
    "Wake Island",
    "Wallis and Futuna",
    "West Bank",
    "Western Sahara",
    "Yemen",
    "Zambia",
    "Zimbabwe"
};

const string CCountries::sm_Former_Countries[] = {
    "Belgian Congo",
    "British Guiana",
    "Burma",
    "Czechoslovakia",
    "Netherlands Antilles",
    "Korea",
    "Serbia and Montenegro",
    "Siam",
    "USSR",
    "Yugoslavia",
    "Zaire"
};

bool CCountries::IsValid(const string& country)
{
    string name = country;
    size_t pos = country.find(':');

    if ( pos != string::npos ) {
        name = country.substr(0, pos);
    }

    // try current countries
    const string *begin = sm_Countries;
    const string *end = &(sm_Countries[sizeof(sm_Countries) / sizeof(string)]);

    return find(begin, end, name) != end;
}


bool CCountries::IsValid(const string& country, bool& is_miscapitalized)
{
    string name = country;
    size_t pos = country.find(':');

    if ( pos != string::npos ) {
        name = country.substr(0, pos);
    }

    is_miscapitalized = false;
    // try current countries
    size_t num_countries = sizeof(sm_Countries) / sizeof(string);
    for (size_t i = 0; i < num_countries; i++) {
        if (NStr::EqualNocase (name, sm_Countries[i])) {
            if (!NStr::EqualCase(name, sm_Countries[i])) {
                is_miscapitalized = true;
            }
            return true;
        }
    }
    return false;
}


bool CCountries::WasValid(const string& country)
{
    string name = country;
    size_t pos = country.find(':');

    if ( pos != string::npos ) {
        name = country.substr(0, pos);
    }

    // try formerly-valid countries
    const string *begin = sm_Former_Countries;
    const string *end = &(sm_Former_Countries[sizeof(sm_Former_Countries) / sizeof(string)]);

    return find(begin, end, name) != end;
}


bool CCountries::WasValid(const string& country, bool& is_miscapitalized)
{
    string name = country;
    size_t pos = country.find(':');

    if ( pos != string::npos ) {
        name = country.substr(0, pos);
    }

    is_miscapitalized = false;
    // try formerly-valid countries
    size_t num_countries = sizeof(sm_Former_Countries) / sizeof(string);
    for (size_t i = 0; i < num_countries; i++) {
        if (NStr::EqualNocase (name, sm_Former_Countries[i])) {
            if (!NStr::EqualCase(name, sm_Former_Countries[i])) {
                is_miscapitalized = true;
            }
            return true;
        }
    }
    return false;
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 65, chars: 1891, CRC32: 7724f0c5 */
