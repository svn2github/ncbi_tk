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
 *   using specifications from the data definition file
 *   'twebenv.asn'.
 *
 * ATTENTION:
 *   Don't edit or check-in this file to the CVS as this file will
 *   be overridden (by DATATOOL) without warning!
 * ===========================================================================
 */

#ifndef WEB_SAVED_BASE_HPP
#define WEB_SAVED_BASE_HPP

// standard includes
#include <serial/serialbase.hpp>

// generated includes
#include <list>


// forward declarations
class CNamed_Item_Set;
class CNamed_Query;


// generated classes

class CWeb_Saved_Base : public ncbi::CSerialObject
{
    typedef ncbi::CSerialObject Tparent;
public:
    // constructor
    CWeb_Saved_Base(void);
    // destructor
    virtual ~CWeb_Saved_Base(void);

    // type info
    DECLARE_INTERNAL_TYPE_INFO();

    // types
    typedef std::list< ncbi::CRef< CNamed_Query > > TQueries;
    typedef std::list< ncbi::CRef< CNamed_Item_Set > > TItem_Sets;

    // getters
    // setters

    // optional
    // typedef std::list< ncbi::CRef< CNamed_Query > > TQueries
    bool IsSetQueries(void) const;
    bool CanGetQueries(void) const;
    void ResetQueries(void);
    const TQueries& GetQueries(void) const;
    TQueries& SetQueries(void);

    // optional
    // typedef std::list< ncbi::CRef< CNamed_Item_Set > > TItem_Sets
    bool IsSetItem_Sets(void) const;
    bool CanGetItem_Sets(void) const;
    void ResetItem_Sets(void);
    const TItem_Sets& GetItem_Sets(void) const;
    TItem_Sets& SetItem_Sets(void);

    // reset whole object
    virtual void Reset(void);


private:
    // Prohibit copy constructor and assignment operator
    CWeb_Saved_Base(const CWeb_Saved_Base&);
    CWeb_Saved_Base& operator=(const CWeb_Saved_Base&);

    // data
    Uint4 m_set_State[1];
    TQueries m_Queries;
    TItem_Sets m_Item_Sets;
};






///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////
inline
bool CWeb_Saved_Base::IsSetQueries(void) const
{
    return ((m_set_State[0] & 0x3) != 0);
}

inline
bool CWeb_Saved_Base::CanGetQueries(void) const
{
    return true;
}

inline
const std::list< ncbi::CRef< CNamed_Query > >& CWeb_Saved_Base::GetQueries(void) const
{
    return m_Queries;
}

inline
std::list< ncbi::CRef< CNamed_Query > >& CWeb_Saved_Base::SetQueries(void)
{
    m_set_State[0] |= 0x1;
    return m_Queries;
}

inline
bool CWeb_Saved_Base::IsSetItem_Sets(void) const
{
    return ((m_set_State[0] & 0xc) != 0);
}

inline
bool CWeb_Saved_Base::CanGetItem_Sets(void) const
{
    return true;
}

inline
const std::list< ncbi::CRef< CNamed_Item_Set > >& CWeb_Saved_Base::GetItem_Sets(void) const
{
    return m_Item_Sets;
}

inline
std::list< ncbi::CRef< CNamed_Item_Set > >& CWeb_Saved_Base::SetItem_Sets(void)
{
    m_set_State[0] |= 0x4;
    return m_Item_Sets;
}

///////////////////////////////////////////////////////////
////////////////// end of inline methods //////////////////
///////////////////////////////////////////////////////////






#endif // WEB_SAVED_BASE_HPP
