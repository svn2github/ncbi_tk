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

#ifndef ITEM_SET_BASE_HPP
#define ITEM_SET_BASE_HPP

// standard includes
#include <serial/serialbase.hpp>

// generated includes
#include <vector>


// generated classes

class CItem_Set_Base : public ncbi::CSerialObject
{
    typedef ncbi::CSerialObject Tparent;
public:
    // constructor
    CItem_Set_Base(void);
    // destructor
    virtual ~CItem_Set_Base(void);

    // type info
    DECLARE_INTERNAL_TYPE_INFO();

    // members' types
    typedef std::vector< char > TItems;
    typedef int TCount;

    // members' getters
    // members' setters
    void ResetItems(void);
    const std::vector< char >& GetItems(void) const;
    std::vector< char >& SetItems(void);

    void ResetCount(void);
    const int& GetCount(void) const;
    void SetCount(const int& value);
    int& SetCount(void);

    // reset whole object
    virtual void Reset(void);

private:
    // Prohibit copy constructor and assignment operator
    CItem_Set_Base(const CItem_Set_Base&);
    CItem_Set_Base& operator=(const CItem_Set_Base&);

    // members' data
    TItems m_Items;
    TCount m_Count;
};






///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////
inline
const CItem_Set_Base::TItems& CItem_Set_Base::GetItems(void) const
{
    return m_Items;
}

inline
CItem_Set_Base::TItems& CItem_Set_Base::SetItems(void)
{
    return m_Items;
}

inline
void CItem_Set_Base::ResetCount(void)
{
    m_Count = 0;
}

inline
const CItem_Set_Base::TCount& CItem_Set_Base::GetCount(void) const
{
    return m_Count;
}

inline
void CItem_Set_Base::SetCount(const TCount& value)
{
    m_Count = value;
}

inline
CItem_Set_Base::TCount& CItem_Set_Base::SetCount(void)
{
    return m_Count;
}

///////////////////////////////////////////////////////////
////////////////// end of inline methods //////////////////
///////////////////////////////////////////////////////////






#endif // ITEM_SET_BASE_HPP
