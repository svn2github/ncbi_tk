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
 *   'biblio.asn'.
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.1  2002/01/10 20:07:54  clausen
 * Added GetTitle
 *
 *
 * ===========================================================================
 */

#ifndef OBJECTS_BIBLIO_TITLE_HPP
#define OBJECTS_BIBLIO_TITLE_HPP


// generated includes
#include <objects/biblio/Title_.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class CTitle : public CTitle_Base
{
    typedef CTitle_Base Tparent;
public:
    // constructor
    CTitle(void);
    // destructor
    ~CTitle(void);
    
    // Returns a string title of the type found first in internal list. 
    // Throws runtime_error if title is not set
    const string& GetTitle() const;

private:
    // Prohibit copy constructor and assignment operator
    CTitle(const CTitle& value);
    CTitle& operator=(const CTitle& value);

};



/////////////////// CTitle inline methods

// constructor
inline
CTitle::CTitle(void)
{
}


/////////////////// end of CTitle inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // OBJECTS_BIBLIO_TITLE_HPP
/* Original file checksum: lines: 90, chars: 2332, CRC32: c4619fa8 */
