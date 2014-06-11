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
 * Author:  J. Chen
 *
 * File Description:
 *   Evaluate if a string and an object match to CString_constraint
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using the following specifications:
 *   'macro.asn'.
 */

// standard includes
#include <ncbi_pch.hpp>

// generated includes
#include <objects/macro/String_constraint.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Imp_feat.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CString_constraint::~CString_constraint(void)
{
}

bool CString_constraint :: Empty() const
{
   if (GetIs_all_caps() || GetIs_all_lower() || GetIs_all_punct()) {
      return false;
   }
   else if (!CanGetMatch_text() || GetMatch_text().empty()) {
        return true;
   }
   else {
      return false;
   }
};

bool  CString_constraint :: x_IsAllCaps(const string& str) const
{
  string up_str = str;
  up_str = NStr::ToUpper(up_str);
  if (up_str == str) return true;
  else return false;
};

bool CString_constraint :: x_IsAllLowerCase(const string& str) const
{
  string low_str = str;
  low_str = NStr::ToLower(low_str);
  if (low_str == str) return true;
  else return false;
};

bool CString_constraint :: x_IsAllPunctuation(const string& str) const
{
   for (unsigned i=0; i< str.size(); i++) {
     if (!ispunct(str[i])) return false;
   }
   return true;
};

static const string weasels[] = {
  "candidate",
  "hypothetical",
  "novel",
  "possible",
  "potential",
  "predicted",
  "probable",
  "putative",
  "candidate",
  "uncharacterized",
  "unique"
};

bool CString_constraint :: x_IsWeasel(const string& str) const
{
  unsigned sz = ArraySize(weasels);
  const string *begin = weasels;
  const string *end = &(weasels[sz]);

  if (find(begin, end, str) != end) {
        return true;
  } else {
        return false;
  }
};

string CString_constraint :: x_SkipWeasel(const string& str) const
{
  if (str.empty()) {
    return kEmptyStr;
  }
  string ret_str;
  vector <string> arr;
  arr = NStr::Tokenize(str, " ", arr);

  bool found = false;
  while (!arr.empty() && x_IsWeasel(arr[0])) {
     arr.erase(arr.begin());
     found = true;
  }
  if (found) {
      ret_str = NStr::Join(arr, " "); 
      ret_str = CTempString(ret_str).substr(0, ret_str.size()-1); 
  }
  else ret_str = str;

  return ret_str;
};

bool CString_constraint :: x_CaseNCompareEqual(string str1, string str2, unsigned len1, bool case_sensitive) const
{
   if (!len1) return false;
   string comp_str1, comp_str2;
   comp_str1 = CTempString(str1).substr(0, len1);
   comp_str2 = CTempString(str2).substr(0, len1);
   if (case_sensitive) {
       return (comp_str1 == comp_str2);
   }
   else {
     return (NStr::EqualNocase(comp_str1, 0, len1, comp_str2));
   }
};

string CString_constraint :: x_StripUnimportantCharacters(const string& str, bool strip_space, bool strip_punct) const
{
   if (str.empty()) {
      return kEmptyStr;
   }
   string result;
   result.reserve(str.size());
   string::const_iterator it = str.begin();
   do {
      if ((strip_space && isspace(*it)) || (strip_punct && ispunct(*it)));
      else result += *it;
   } while (++it != str.end());

   return result;
};

bool CString_constraint :: x_DisallowCharacter(const char ch, bool disallow_slash) const
{
  if (isalpha (ch) || isdigit (ch) || ch == '_' || ch == '-') return true;
  else if (disallow_slash && ch == '/') return true;
  else return false;
};

bool CString_constraint :: x_IsWholeWordMatch(const string& start, const size_t& found, const unsigned& match_len, bool disallow_slash) const
{
  unsigned after_idx;

  if (!match_len) return true;
  else if (start.empty() || found == string::npos) return false;
  else
  {
    if (found) {
      if (x_DisallowCharacter (start[found-1], disallow_slash)) {
         return false;
      }
    }
    after_idx = found + match_len;
    if ( after_idx < start.size()
              && x_DisallowCharacter(start[after_idx], disallow_slash)) {
       return false;
    }
  }
  return true;
};
bool CString_constraint :: x_AdvancedStringCompare(const string& str, const string& str_match, bool is_start, unsigned int * ini_target_match_len)  const
{
  if (str.empty()) return false;
  if (str_match.empty()) return true;

  size_t pos_match = 0, pos_str = 0;
  bool wd_case, whole_wd, word_start_m, word_start_s;
  bool match = true, recursive_match = false;
  unsigned len_m = str_match.size(), len_s = str.size(), target_match_len=0;
  string cp_m, cp_s;
  bool ig_space = GetIgnore_space();
  bool ig_punct = GetIgnore_punct();
  bool str_case = GetCase_sensitive();
  EString_location loc = GetMatch_location();
  unsigned len1, len2;
  char ch1, ch2;
  vector <string> word_word;
  bool has_word = false;
  if (IsSetIgnore_words()) {
      string strtmp;
      ITERATE (list <CRef <CWord_substitution> >, 
                 it, 
                 GetIgnore_words().Get()) {
          strtmp = ((*it)->CanGetWord()) ? (*it)->GetWord() : kEmptyStr;
          word_word.push_back(strtmp);
          if (!strtmp.empty()) {
             has_word = true;
          }
      }
  }

  unsigned i;
  while (match && pos_match < len_m && pos_str < len_s && !recursive_match) {
    cp_m = CTempString(str_match).substr(pos_match);
    cp_s = CTempString(str).substr(pos_str);

    /* first, check to see if we're skipping synonyms */
    i=0;
    if (has_word) {
      ITERATE (list <CRef <CWord_substitution> >, 
               it, 
               GetIgnore_words().Get()) {
        wd_case = (*it)->GetCase_sensitive();
        whole_wd = (*it)->GetWhole_word();
        len1 = word_word[i].size();
        //text match
        if (len1 && x_CaseNCompareEqual(word_word[i], cp_m, len1,wd_case)){
           word_start_m 
               = (!pos_match && is_start) || !isalpha(str_match[pos_match-1]);
           ch1 = (cp_m.size() < len1) ? ' ' : cp_m[len1];
           if (!whole_wd || (!isalpha(ch1) && word_start_m)) { // whole word mch
              if ( !(*it)->CanGetSynonyms() ) { 
                 if (x_AdvancedStringCompare(cp_s, 
                                           CTempString(cp_m).substr(len1), 
                                           word_start_m, 
                                           &target_match_len)) {
                    recursive_match = true;
                 }
              }
              else {
                ITERATE (list <string>, sit, (*it)->GetSynonyms()) {
                  len2 = (*sit).size();

                    // text match
                  if (x_CaseNCompareEqual(*sit, cp_s, len2, wd_case)) {
                    word_start_s 
                          = (!pos_str && is_start) || !isalpha(str[pos_str-1]);
                    ch2 = (cp_s.size() < len2) ? ' ' : cp_s[len2];
                    // whole word match
                    if (!whole_wd || (!isalpha(ch2) && word_start_s)) {
                      if(x_AdvancedStringCompare(CTempString(cp_s).substr(len2),
                                                 CTempString(cp_m).substr(len1),
                                                 word_start_m & word_start_s, 
                                                 &target_match_len)){
                            recursive_match = true;
                      }
                    }
                  }
                }
              }
           }
        }
      }
    }

    if (!recursive_match) {
      if (x_CaseNCompareEqual(cp_m, cp_s, 1, str_case)) {
           pos_match++;
           pos_str++;
           target_match_len++;
      } 
      else if ( ig_space && (isspace(cp_m[0]) || isspace(cp_s[0])) ) {
        if ( isspace(cp_m[0]) ) pos_match++;
        if ( isspace(cp_s[0]) ) {
             pos_str++;
             target_match_len++;
        }
      }
      else if (ig_punct && ( ispunct(cp_m[0]) || ispunct(cp_s[0]) )) {
        if ( ispunct(cp_m[0]) ) pos_match++;
        if ( ispunct(cp_s[0]) ) {
             pos_str++;
             target_match_len++;
        }
      }
      else match = false;
    }
  }

  if (match && !recursive_match) {
    while ( pos_str < str.size() 
             && ((ig_space && isspace(str[pos_str])) 
             || (ig_punct && ispunct(str[pos_str]))) ){
       pos_str++;
       target_match_len++;
    }
    while ( pos_match < str_match.size()
              && ( (ig_space && isspace(str_match[pos_match])) 
                     || (ig_punct && ispunct(str_match[pos_match])) ) ) {
         pos_match++;
    }

    if (pos_match < str_match.size()) {
         match = false;
    }
    else if ( (loc == eString_location_ends || loc == eString_location_equals) 
                 && pos_str < len_s) {
         match = false;
    }
    else if (GetWhole_word() 
                && (!is_start || (pos_str < len_s && isalpha (str[pos_str]))) ){
             match = false;
    }
  }
  if (match && ini_target_match_len) {
         *ini_target_match_len += target_match_len;
  }

  return match;
};

bool CString_constraint :: x_AdvancedStringMatch(const string& str, const string& tmp_match) const
{
  bool rval = false;
  string
    match_text = CanGetMatch_text() ? tmp_match : kEmptyStr;

  if (x_AdvancedStringCompare (str, match_text, true)) {
       return true;
  }
  else if (GetMatch_location() == eString_location_starts
                || GetMatch_location() == eString_location_equals) {
           return false;
  }
  else {
    size_t pos = 1;
    unsigned len = str.size();
    while (!rval && pos < len) {
      if (GetWhole_word()) {
          while (pos < len && isalpha (str[pos-1])) pos++;
      }
      if (pos < len) {
        if (x_AdvancedStringCompare (CTempString(str).substr(pos),
                                      match_text,
                                      true)) {
            rval = true;
        }
        else {
            pos++;
        }
      }
    }
  }
  return rval;
};

bool CString_constraint :: x_GetSpanFromHyphenInString(const string& str, const size_t& hyphen, string& first, string& second) const
{
   if (!hyphen) return false;

   /* find range start */
   size_t cp = str.substr(0, hyphen-1).find_last_not_of(' ');   
   if (cp != string::npos) {
      cp = str.substr(0, cp).find_last_not_of(" ,;"); 
   }
   if (cp == string::npos) {
     cp = 0;
   }

   unsigned len = hyphen - cp;
   first = CTempString(str).substr(cp, len);
   NStr::TruncateSpacesInPlace(first);
 
   /* find range end */
   cp = str.find_first_not_of(' ', hyphen+1);
   if (cp != string::npos) {
      cp = str.find_first_not_of(" ,;");
   }
   if (cp == string::npos) {
      cp = str.size() -1;   
   }

   len = cp - hyphen;
   if (!isspace (str[cp])) {
     len--;
   }
   second = CTempString(str).substr(hyphen+1, len);
   NStr::TruncateSpacesInPlace(second);

   bool rval = true;
   if (first.empty() || second.empty()) {
      rval = false;
   }
   else if (!isdigit (first[first.size() - 1]) 
                 || !isdigit (second[second.size() - 1])) {
      /* if this is a span, then neither end point can end with anything other than a number */
      rval = false;
   }
   if (!rval) {
      first = second = kEmptyStr;
   }
   return rval;
};

bool CString_constraint :: x_StringIsPositiveAllDigits(const string& str) const
{
   if (str.find_first_not_of(m_digit_str) != string::npos) {
      return false;
   }
   else {
      return true;
   }
};

bool CString_constraint :: x_IsStringInSpan(const string& str, const string& first, const string& second) const
{
  string new_first, new_second, new_str;
  if (str.empty()) return false;
  else if (str == first || str == second) return true;
  else if (first.empty() || second.empty()) return false;

  int str_num, first_num, second_num;
  str_num = first_num = second_num = 0;
  bool rval = false;
  size_t prefix_len;
  string comp_str1, comp_str2;
  if (x_StringIsPositiveAllDigits (first)) {
    if (x_StringIsPositiveAllDigits (str) 
             && x_StringIsPositiveAllDigits (second)) {
      str_num = NStr::StringToUInt (str);
      first_num = NStr::StringToUInt (first);
      second_num = NStr::StringToUInt (second);
      if ( (str_num > first_num && str_num < second_num)
               || (str_num > second_num && str_num < first_num) ) {
          rval = true;
      }
    }
  } 
  else if (x_StringIsPositiveAllDigits(second)) {
    prefix_len = first.find_first_of(m_digit_str) + 1;

    new_str = CTempString(str).substr(prefix_len - 1);
    new_first = CTempString(first).substr(prefix_len - 1);
    comp_str1 = CTempString(str).substr(0, prefix_len);
    comp_str2 = CTempString(first).substr(0, prefix_len);
    if (comp_str1 == comp_str2
          && x_StringIsPositiveAllDigits (new_str)
          && x_StringIsPositiveAllDigits (new_first)) {
      first_num = NStr::StringToUInt(new_first);
      second_num = NStr::StringToUInt (second);
      str_num = NStr::StringToUInt (str);
      if ( (str_num > first_num && str_num < second_num)
               || (str_num > second_num && str_num < first_num) ) {
        rval = true;
      }
    }
  } 
  else {
    /* determine length of prefix */
    prefix_len = 0;
    while ( prefix_len < first.size() 
               && prefix_len < second.size() 
               && first[prefix_len] == second[prefix_len]) {
       prefix_len ++;
    }
    prefix_len ++;

    comp_str1 = CTempString(str).substr(0, prefix_len);
    comp_str2 = CTempString(first).substr(0, prefix_len);
    if (prefix_len <= first.size() 
           && prefix_len <= second.size()
           && isdigit (first[prefix_len-1]) 
           && isdigit (second[prefix_len-1])
           && comp_str1 == comp_str2) {
      new_first = CTempString(first).substr(prefix_len);
      new_second = CTempString(second).substr(prefix_len);
      new_str = CTempString(str).substr(prefix_len);
      if (x_StringIsPositiveAllDigits (new_first) 
            && x_StringIsPositiveAllDigits (new_second) 
            && x_StringIsPositiveAllDigits (new_str)) {
        first_num = NStr::StringToUInt(new_first);
        second_num = NStr::StringToUInt (new_second);
        str_num = NStr::StringToUInt (new_str);
        if ( (str_num > first_num && str_num < second_num)
                || (str_num > second_num && str_num < first_num) ) {
          rval = true;
        }
      } else {
        /* determine whether there is a suffix */
        size_t idx1, idx2, idx_str;
        string suf1, suf2, sub_str;
        idx1 = first.find_first_not_of(m_digit_str);
        suf1 = CTempString(first).substr(prefix_len + idx1);
        idx2 = second.find_first_not_of(m_digit_str);
        suf2 = CTempString(second).substr(prefix_len + idx2);
        idx_str = str.find_first_not_of(m_digit_str);
        sub_str = CTempString(str).substr(prefix_len + idx_str);
        if (suf1 == suf2 && suf1 == sub_str) {
          /* suffixes match */
          first_num 
            = NStr::StringToUInt(CTempString(first).substr(prefix_len, idx1));
          second_num 
            =NStr::StringToUInt(CTempString(second).substr(prefix_len, idx2));
          str_num 
            =NStr::StringToUInt(CTempString(str).substr(prefix_len, idx_str));
          if ( (str_num > first_num && str_num < second_num)
                   || (str_num > second_num && str_num < first_num) ) {
            rval = true;
          }
        }
      }
    }
  }
  return rval;
};

bool CString_constraint :: x_IsStringInSpanInList (const string& str, const string& list) const
{
  if (list.empty() || str.empty()) {
      return false;
  }

  size_t idx = str.find_first_not_of(m_alpha_str);
  if (idx == string::npos) {
     return false;
  }

  idx = CTempString(str).substr(idx).find_first_not_of(m_digit_str);

  /* find ranges */
  size_t hyphen = list.find('-');
  bool rval = false;
  string range_start, range_end;
  while (hyphen != string::npos && !rval) {
    if (!hyphen) {
       hyphen = CTempString(list).substr(1).find('-');
    }
    else {
      if (x_GetSpanFromHyphenInString (list, hyphen, range_start, range_end)){
        if (x_IsStringInSpan (str, range_start, range_end)) rval = true;
      }
      hyphen = list.find('-', hyphen + 1);
    }
  }
  return rval;
};

bool CString_constraint :: x_DoesSingleStringMatchConstraint(const string& str) const
{
  bool rval = false;

  string this_str(str);
  if (str.empty()) {
     return false;
  }
  if (Empty()) {
    rval = true;
  }
  else {
    if (GetIgnore_weasel()) {
         this_str = x_SkipWeasel(str);
    }
    if (GetIs_all_caps() && !x_IsAllCaps(this_str)) {
         rval = false;
    }
    else if (GetIs_all_lower() && !x_IsAllLowerCase(this_str)) {
               rval = false;
    }
    else if (GetIs_all_punct() && !x_IsAllPunctuation(this_str)) {
               rval = false;
    }
    else {
      string tmp_match = GetMatch_text();
      if (GetIgnore_weasel()) {
         tmp_match = x_SkipWeasel(tmp_match);
      }
      if ((GetMatch_location() != eString_location_inlist) 
                && CanGetIgnore_words()){
          rval = x_AdvancedStringMatch(str, tmp_match);
      }
      else {
        string search(this_str), pattern(tmp_match);
        bool ig_space = GetIgnore_space();
        bool ig_punct = GetIgnore_punct();
        if ( (GetMatch_location() != eString_location_inlist)
                 && (ig_space || ig_punct)) {
          search = x_StripUnimportantCharacters(search, ig_space, ig_punct);
          pattern = x_StripUnimportantCharacters(pattern, ig_space, ig_punct);
        } 

        size_t pFound;
        pFound = GetCase_sensitive()?
                    search.find(pattern) : NStr::FindNoCase(search, pattern);
        switch (GetMatch_location()) 
        {
          case eString_location_contains:
            if (string::npos == pFound) {
               rval = false;
            }
            else if (GetWhole_word()) {
                rval = x_IsWholeWordMatch (search, pFound, pattern.size());
                while (!rval && pFound != string::npos) {
	          pFound = GetCase_sensitive() ?
                              search.find(pattern, pFound+1):
                                NStr::FindNoCase(search, pattern, pFound+1);
                  rval = (pFound != string::npos)? 
                           x_IsWholeWordMatch(search, pFound, pattern.size()):
                            false;
                }
            }
            else {
                 rval = true;
            }
            break;
          case eString_location_starts:
            if (!pFound) {
              rval = GetWhole_word() ?
                         x_IsWholeWordMatch (search, pFound, pattern.size()):
                         true;
            }
            break;
          case eString_location_ends:
            while (pFound != string::npos && !rval) {
              if ( (pFound + pattern.size()) == search.size()) {
                rval = GetWhole_word()? 
                         x_IsWholeWordMatch (search, pFound, pattern.size()) : 
                         true;
                /* stop the search, we're at the end of the string */
                pFound = string::npos;
              }
              else {
  	              pFound = GetCase_sensitive() ?
                           search.find(pattern, pFound+1):
                              NStr::FindNoCase(search, pattern, pFound+1);
              }
            }
            break;
          case eString_location_equals:
            if (GetCase_sensitive() && (search == pattern) ) {
               rval= true; 
            }
            else if (!GetCase_sensitive() 
                        && NStr::EqualNocase(search, pattern) ) {
                  rval = true;
            }
            break;
          case eString_location_inlist:
            pFound = GetCase_sensitive()?
                       pattern.find(search) : NStr::FindNoCase(pattern, search);
            if (pFound == string::npos) {
                  rval = false;
            }
            else {
              rval = x_IsWholeWordMatch(pattern, pFound, search.size(), true);
              while (!rval && pFound != string::npos) {
                pFound = GetCase_sensitive() ?
                          CTempString(pattern).substr(pFound + 1).find(search):
                            NStr::FindNoCase(
                              CTempString(pattern).substr(pFound + 1), search);
                if (pFound != string::npos) {
                  rval = x_IsWholeWordMatch (pattern, pFound, str.size(), true);
                }
              }
            }
            if (!rval) {
              /* look for spans */
              rval = x_IsStringInSpanInList (search, pattern);
            }
            break;
        }
      }
    }
  }
  return rval;
};

bool CString_constraint :: Match(const string& str) const
{
  bool rval = x_DoesSingleStringMatchConstraint (str);
  if (GetNot_present()) { 
     return (!rval);
  }
  else {
     return rval;
  }
};

bool CString_constraint::x_ReplaceContains(string& val, const string& replace) const
{
    bool rval = false;

    size_t offset = 0;
    while (offset < val.length()) {
        unsigned int match_len = 0;
        if (x_AdvancedStringCompare(val.substr(offset), GetMatch_text(),
                                    (offset == 0 || !isalpha(val.c_str()[offset - 1])),
                                    &match_len)) {
            val = val.substr(0, offset) + replace + val.substr(offset + match_len);
            rval = true;
            offset += replace.length();
        } else {
            offset++;
        }
    }
    return rval;
}


bool CString_constraint::ReplaceStringConstraintPortionInString(string& val, const string& replace) const
{
    bool rval = false;
    
    if (val.empty()) {
        if (Empty() || (IsSetNot_present() && GetNot_present())) {
            val = replace;
            rval = true;
        }
    } else if (Empty()) {
        val = replace;
        rval = true;
    } else {
        if (IsSetMatch_location()) {
            switch (GetMatch_location()) {
                case eString_location_inlist:
                case eString_location_equals:
                    val = replace;
                    rval = true;
                    break;
                case eString_location_starts:
                    {{
                       unsigned int match_len = 0;
                       if (x_AdvancedStringCompare(val, GetMatch_text(), true, &match_len)) {
                           val = replace + val.substr(match_len);
                           rval = true;
                       }
                    }}
                    break;
                case eString_location_contains:
                    rval = x_ReplaceContains(val, replace);
                    break;
                case eString_location_ends:
                    {{
                        size_t offset = 0;
                        while (!rval && offset < val.length()) {
                            unsigned int match_len = 0;
                            if (x_AdvancedStringCompare(val.substr(offset), 
                                                         GetMatch_text(),
                                                         (offset == 0), 
                                                          &match_len)
                                && offset + match_len == val.length()) {
                                val = val.substr(0, offset) + replace;
                                rval = true;
                            } else {
                                offset++;
                            }
                        }
                    }}
                    break;
            } 
        } else {
            rval = x_ReplaceContains(val, replace);                    
        }
    }
    return rval;
}

END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 57, chars: 1744, CRC32: 7f791d1c */
