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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:  Implemented methods to identify file formats.
 *
 */

#include <util/format_guess.hpp>
#include <corelib/ncbifile.hpp>

BEGIN_NCBI_SCOPE


CFormatGuess::EFormat CFormatGuess::Format(const string& path)
{
    EFormat format = eUnknown;

    CNcbiIfstream input(path.c_str(), IOS_BASE::in | IOS_BASE::binary);

    if (!input.is_open()) 
        return eUnknown;

    unsigned char buf[1024];

    input.read((char*)buf, sizeof(buf));
    size_t count = input.gcount();

    if (!count) 
        return eUnknown;

    // Buffer analysis (completely ad-hoc heuristics).

    // Check for XML signature...
    {{
        if (count > 5) {
            const char* xml_sig = "<?XML";
            bool xml_flag = true;
            for (unsigned i = 0; i < 5; ++i) {
                unsigned char ch = buf[i];
                char upch = toupper(ch);
                if (upch != xml_sig[i]) {
                    xml_flag = false;
                    break;
                }
            }
            if (xml_flag) {
                return eXml;
            }
        }

    }}

    
    unsigned ATGC_content = 0;
    unsigned URY_content = 0;

    unsigned alpha_content = 0;

    for (unsigned i = 0; i < count; ++i) {
        unsigned char ch = buf[i];
        char upch = toupper(ch);

        if (isalnum(ch) || isspace(ch)) {
            ++alpha_content;
        }
        if (upch == 'A' || upch == 'T' || upch == 'G' || upch == 'C') {
            ++ATGC_content;
        }
        if (upch == 'U' || 
            upch == 'R' || 
            upch == 'Y' || 
            upch == 'K' ||
            upch == 'M' ||
            upch == 'S' ||
            upch == 'W' ||
            upch == 'B' ||
            upch == 'D' ||
            upch == 'H' ||
            upch == 'V' ||
            upch == 'N' ) 
        {
            ++URY_content;
        }
    }

    double dna_content = (double)ATGC_content / (double)count;
    double prot_content = (double)URY_content / (double)count;
    double a_content = (double)alpha_content / (double)count;
    if (buf[0] == '>') {
        if (dna_content > 0.7 && a_content > 0.91) {
            return eFasta;  // DNA fasta file
        }
        if (prot_content > 0.7 && a_content > 0.91) {
            return eFasta;  // Protein fasta file
        }
    }

    if (a_content > 0.80) {  // Text ASN ?
        // extract first line
        char line[1024] = {0,};
        char* ptr = line;
        for (unsigned i = 0; i < count; ++i) {
            if (buf[i] == '\n') {
                break;
            }
            *ptr = buf[i];
            ++ptr;
        }
        // roll it back to last non-space character...
        while (ptr > line) {
            --ptr;
            if (!isspace(*ptr)) break;
        }
        if (*ptr == '{') {  // "{" simbol says it's most likely ASN text
            return eTextASN;
        }
    }

    if (a_content < 0.80) {
        return eBinaryASN;
    }

    input.close();
    return format;

}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2003/05/09 14:08:28  ucko
 * ios_base:: -> IOS_BASE:: for gcc 2.9x compatibility
 *
 * Revision 1.1  2003/05/08 19:46:34  kuznets
 * Initial revision
 *
 * ===========================================================================
 */
