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
* Author:  Vladimir Soussov
*   
* File Description:  Handlers Stack
*
*
*/

#include <dbapi/driver/util/handle_stack.hpp>
#include <string.h>

BEGIN_NCBI_SCOPE


CDBHandlerStack::CDBHandlerStack(size_t n)
{
    m_Room    = (n < 8) ? 8 : (int) n;
    m_Stack   = new CDBUserHandler* [m_Room];
    m_TopItem = 0;
}


void CDBHandlerStack::Push(CDBUserHandler* h)
{
    if (m_TopItem >= m_Room) {
        CDBUserHandler** t = m_Stack;
        m_Room += m_Room / 2;
        m_Stack = new CDBUserHandler* [m_Room];
        memcpy(m_Stack, t, m_TopItem * sizeof(CDBUserHandler*));
        delete [] t;
    }
    m_Stack[m_TopItem++] = h;
}


void CDBHandlerStack::Pop(CDBUserHandler* h, bool last)
{
    int i;

    if ( last ) {
        for (i = m_TopItem;  --i >= 0; ) {
            if (m_Stack[i] == h) {
                m_TopItem = i;
                break;
            }
        }
    }
    else {
        for (i = 0;  i < m_TopItem;  i++) {
            if (m_Stack[i] == h) {
                m_TopItem = i;
                break;
            }
        }
    }
}


CDBHandlerStack::CDBHandlerStack(const CDBHandlerStack& s)
{
    m_Stack = new CDBUserHandler* [s.m_Room];
    memcpy(m_Stack, s.m_Stack, s.m_TopItem * sizeof(CDBUserHandler*));
    m_TopItem = s.m_TopItem;
    m_Room    = s.m_Room;
}


CDBHandlerStack& CDBHandlerStack::operator= (const CDBHandlerStack& s)
{
    if (m_Room < s.m_TopItem) {
        delete [] m_Stack;
        m_Stack = new CDBUserHandler* [s.m_Room];
        m_Room = s.m_Room;
    }
    memcpy(m_Stack, s.m_Stack, s.m_TopItem * sizeof(CDBUserHandler*));
    m_TopItem = s.m_TopItem;
    return *this;
}


void CDBHandlerStack::PostMsg(const CDB_Exception* ex)
{
    for (int i = m_TopItem;  --i >= 0; ) {
        if ( m_Stack[i]->HandleIt(ex) )
            break;
    }
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2001/09/21 23:39:59  vakatov
 * -----  Initial (draft) revision.  -----
 * This is a major revamp (by Denis Vakatov, with help from Vladimir Soussov)
 * of the DBAPI "driver" libs originally written by Vladimir Soussov.
 * The revamp involved massive code shuffling and grooming, numerous local
 * API redesigns, adding comments and incorporating DBAPI to the C++ Toolkit.
 *
 * ===========================================================================
 */
