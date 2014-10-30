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
#include <objects/seqfeat/PCRPrimerSeq.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// Erases spaces which are not inside angle brackets ("<" to ">").
// returns true if there was a change
static
void s_EraseSpacesOutsideBrackets( string &str )
{
    string result;
    result.reserve( str.length() );

    bool in_brackets = false;
    string::iterator ch_iter = str.begin();
    while (ch_iter != str.end()) {
        const char ch = *ch_iter;
        switch( ch ) {
            case '<':
                in_brackets = true;
                result += ch;
                break;
            case '>':
                in_brackets = false;
                result += ch;
                break;
            case ' ':
                if( in_brackets ) {
                    result += ch;
                }
                break;
            default:
                result += ch;
                break;
        }
        ch_iter++;
    }

    // swap is faster than assignment
    result.swap( str );
}


void CPCRPrimerSeq::Clean(string& seq)
{
    // first, set sequence to lowercase
    NStr::ToLower(seq);

    // remove any spaces in sequence outside of <modified base>
    s_EraseSpacesOutsideBrackets(seq);

    // upper case modified base <OTHER>
    NStr::ReplaceInPlace(seq, "<other>", "<OTHER>");

}


const char* sm_ValidModifiedPrimerBases[] = {
  "ac4c",
  "chm5u",
  "cm",
  "cmnm5s2u",
  "cmnm5u",
  "d",
  "fm",
  "gal q",
  "gm",
  "i",
  "i6a",
  "m1a",
  "m1f",
  "m1g",
  "m1i",
  "m22g",
  "m2a",
  "m2g",
  "m3c",
  "m4c",
  "m5c",
  "m6a",
  "m7g",
  "mam5u",
  "mam5s2u",
  "man q",
  "mcm5s2u",
  "mcm5u",
  "mo5u",
  "ms2i6a",
  "ms2t6a",
  "mt6a",
  "mv",
  "o5u",
  "osyw",
  "p",
  "q",
  "s2c",
  "s2t",
  "s2u",
  "s4u",
  "t",
  "t6a",
  "tm",
  "um",
  "yw",
  "x",
  "OTHER"
};


bool CPCRPrimerSeq::IsValid(const string& seq, char& bad_ch)
{
    string str = seq;
    bad_ch = 0;
    if (NStr::IsBlank(str)) {
        return false;
    }

    if (NStr::Find(str, ",") == string::npos) {
        if (NStr::Find(str, "(") != string::npos
            || NStr::Find(str, ")") != string::npos) {
            return false;
        }
    } else {
        if (!NStr::StartsWith(str, "(") || !NStr::EndsWith(str, ")")) {
            return false;
        }
    }

    if (NStr::Find(str, ";") != string::npos) {
        return false;
    }

    const char* *list_begin = sm_ValidModifiedPrimerBases;
    const char* *list_end = &(sm_ValidModifiedPrimerBases[sizeof(sm_ValidModifiedPrimerBases) / sizeof(const char*)]);

    size_t pos = 0;
    string::iterator sit = str.begin();
    while (sit != str.end()) {
        if (*sit == '<') {
            size_t pos2 = NStr::Find (str, ">", pos + 1);
            if (pos2 == string::npos) {
                bad_ch = '<';
                return false;
            }
            string match = str.substr(pos + 1, pos2 - pos - 1);
            if (find(list_begin, list_end, match) == list_end) {
                bad_ch = '<';
                return false;
            }
            sit += pos2 - pos + 1;
            pos = pos2 + 1;
        } else {
            if (*sit != '(' && *sit != ')' && *sit != ',' && *sit != ':') {
                if (!isalpha (*sit)) {
                    bad_ch = *sit;
                    return false;
                }
                char ch = toupper(*sit);
                if (strchr ("ABCDGHKMNRSTVWY", ch) == NULL) {
                    bad_ch = tolower (ch);
                    return false;
                }
            }
            ++sit;
            ++pos;
        }
    }

    return true;
}

bool CPCRPrimerSeq::TrimJunk(string& seq)
{
    const char* starts[] = {"5'-","5`-","5-","5'","5`","-",NULL};
    const char* ends[] = {"-3'","-3`","-3","3'","3`","-",NULL};

    string orig(seq);

    for(int i=0; starts[i] != NULL; i++) {
        size_t len = strlen(starts[i]);
        size_t pos = seq.find(starts[i]);
        if (pos == 0 && len < seq.length())
            seq = seq.substr(len);
    }
    
    for(int i=0; ends[i] != NULL; i++) {
        size_t len = strlen(ends[i]);
        size_t pos = seq.rfind(ends[i]);
        if (len < seq.length() && pos == seq.length()-len)
            seq = seq.substr(0,seq.length()-len);
    }
    return (orig != seq);
}

bool CPCRPrimerSeq::Fixi(string& seq)
{
    string orig(seq);
    for (size_t i = 0; i < seq.size(); ++i) {
        if (seq[i] == 'I')
            seq[i] = 'i';
    }

    
    SIZE_TYPE pos = 0;
    while (pos != NPOS && pos < seq.length()) {
        pos = NStr::Find(seq, "i", pos, NPOS, NStr::eFirst, NStr::eCase);
        if (pos != NPOS) {
            string repl;
            
            if(pos==0 || seq[pos-1]!='<')
                repl = "<";
            
            repl += "i";
            
            if (pos==seq.length()-1 || seq[pos+1]!='>')
                repl += ">";
            
            seq = seq.substr(0, pos) + repl + seq.substr(pos + 1, NPOS);
            pos += repl.length();
        }
    }
    
    return (orig != seq);
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 52, chars: 1683, CRC32: dd02975f */
