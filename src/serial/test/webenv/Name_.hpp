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
 * File Description:
 *   This code is generated by application DATATOOL
 *   using specifications from the ASN data definition file
 *   'twebenv.asn'.
 *
 * ATTENTION:
 *   Don't edit or check-in this file to the CVS as this file will
 *   be overridden (by DATATOOL) without warning!
 * ===========================================================================
 */

#ifndef NAME_BASE_HPP
#define NAME_BASE_HPP

// standard includes
#include <serial/serialbase.hpp>

// generated includes
#include <string>


// generated classes

class CName_Base : public ncbi::CSerialObject
{
    typedef ncbi::CSerialObject Tparent;
public:
    // constructor
    CName_Base(void);
    // destructor
    virtual ~CName_Base(void);

    // type info
    DECLARE_INTERNAL_TYPE_INFO();

    // members' types
    typedef std::string TName;
    typedef std::string TDescription;

    // members' getters
    // members' setters
    void ResetName(void);
    const std::string& GetName(void) const;
    void SetName(const std::string& value);
    std::string& SetName(void);

    bool IsSetDescription(void) const;
    void ResetDescription(void);
    const std::string& GetDescription(void) const;
    void SetDescription(const std::string& value);
    std::string& SetDescription(void);

    // reset whole object
    virtual void Reset(void);


private:
    // Prohibit copy constructor and assignment operator
    CName_Base(const CName_Base&);
    CName_Base& operator=(const CName_Base&);

    // members' data
    bool m_set_Description;
    TName m_Name;
    TDescription m_Description;
};






///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////
inline
const CName_Base::TName& CName_Base::GetName(void) const
{
    return m_Name;
}

inline
void CName_Base::SetName(const TName& value)
{
    m_Name = value;
}

inline
CName_Base::TName& CName_Base::SetName(void)
{
    return m_Name;
}

inline
bool CName_Base::IsSetDescription(void) const
{
    return m_set_Description;
}

inline
const CName_Base::TDescription& CName_Base::GetDescription(void) const
{
    return m_Description;
}

inline
void CName_Base::SetDescription(const TDescription& value)
{
    m_Description = value;
    m_set_Description = true;
}

inline
CName_Base::TDescription& CName_Base::SetDescription(void)
{
    m_set_Description = true;
    return m_Description;
}

///////////////////////////////////////////////////////////
////////////////// end of inline methods //////////////////
///////////////////////////////////////////////////////////






#endif // NAME_BASE_HPP
