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

#ifndef QUERY_HISTORY_BASE_HPP
#define QUERY_HISTORY_BASE_HPP

// standard includes
#include <serial/serialbase.hpp>

// generated includes
#include <string>


// forward declarations
class CQuery_Command;
class CTime;


// generated classes

class CQuery_History_Base : public ncbi::CSerialObject
{
    typedef ncbi::CSerialObject Tparent;
public:
    // constructor
    CQuery_History_Base(void);
    // destructor
    virtual ~CQuery_History_Base(void);

    // type info
    DECLARE_INTERNAL_TYPE_INFO();

    // members' types
    typedef std::string TName;
    typedef int TSeqNumber;
    typedef CTime TTime;
    typedef CQuery_Command TCommand;

    // members' getters
    // members' setters
    bool IsSetName(void) const;
    void ResetName(void);
    const std::string& GetName(void) const;
    void SetName(const std::string& value);
    std::string& SetName(void);

    void ResetSeqNumber(void);
    const int& GetSeqNumber(void) const;
    void SetSeqNumber(const int& value);
    int& SetSeqNumber(void);

    void ResetTime(void);
    const CTime& GetTime(void) const;
    void SetTime(CTime& value);
    CTime& SetTime(void);

    void ResetCommand(void);
    const CQuery_Command& GetCommand(void) const;
    void SetCommand(CQuery_Command& value);
    CQuery_Command& SetCommand(void);

    // reset whole object
    virtual void Reset(void);


private:
    // Prohibit copy constructor and assignment operator
    CQuery_History_Base(const CQuery_History_Base&);
    CQuery_History_Base& operator=(const CQuery_History_Base&);

    // members' data
    bool m_set_Name;
    TName m_Name;
    TSeqNumber m_SeqNumber;
    ncbi::CRef< TTime > m_Time;
    ncbi::CRef< TCommand > m_Command;
};






///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////
inline
bool CQuery_History_Base::IsSetName(void) const
{
    return m_set_Name;
}

inline
const CQuery_History_Base::TName& CQuery_History_Base::GetName(void) const
{
    return m_Name;
}

inline
void CQuery_History_Base::SetName(const TName& value)
{
    m_Name = value;
    m_set_Name = true;
}

inline
CQuery_History_Base::TName& CQuery_History_Base::SetName(void)
{
    m_set_Name = true;
    return m_Name;
}

inline
void CQuery_History_Base::ResetSeqNumber(void)
{
    m_SeqNumber = 0;
}

inline
const CQuery_History_Base::TSeqNumber& CQuery_History_Base::GetSeqNumber(void) const
{
    return m_SeqNumber;
}

inline
void CQuery_History_Base::SetSeqNumber(const TSeqNumber& value)
{
    m_SeqNumber = value;
}

inline
CQuery_History_Base::TSeqNumber& CQuery_History_Base::SetSeqNumber(void)
{
    return m_SeqNumber;
}

inline
CQuery_History_Base::TTime& CQuery_History_Base::SetTime(void)
{
    return (*m_Time);
}

inline
CQuery_History_Base::TCommand& CQuery_History_Base::SetCommand(void)
{
    return (*m_Command);
}

///////////////////////////////////////////////////////////
////////////////// end of inline methods //////////////////
///////////////////////////////////////////////////////////






#endif // QUERY_HISTORY_BASE_HPP
