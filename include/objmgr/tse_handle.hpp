#ifndef OBJMGR_TSE_HANDLE__HPP
#define OBJMGR_TSE_HANDLE__HPP

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
* Authors:
*           Eugene Vasilchenko
*
* File Description:
*           CTSE_Handle is handle to TSE
*
*/

#include <corelib/ncbiobj.hpp>
#include <objmgr/impl/heap_scope.hpp>
#include <objmgr/impl/tse_scope_lock.hpp>
#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CScope;
class CTSE_ScopeInfo;
class CTSE_Info;
class CTSE_Lock;
class CBioseq_Handle;
class CSeq_entry;
class CSeq_entry_Handle;
class CSeq_id;
class CSeq_id_Handle;
class CBlobIdKey;

class CScopeInfo_Base;
class CScopeInfoLocker;

/////////////////////////////////////////////////////////////////////////////
// CTSE_Handle definition
/////////////////////////////////////////////////////////////////////////////


class NCBI_XOBJMGR_EXPORT CTSE_Handle
{
public:
    /// Default constructor/destructor and assignment
    CTSE_Handle(void);
    CTSE_Handle(const CTSE_Handle& tse);
    CTSE_Handle& operator=(const CTSE_Handle& tse);
    ~CTSE_Handle(void);

    /// Returns scope
    CScope& GetScope(void) const;

    /// State check
    bool IsValid(void) const;
    DECLARE_OPERATOR_BOOL(IsValid());

    bool operator==(const CTSE_Handle& tse) const;
    bool operator!=(const CTSE_Handle& tse) const;
    bool operator<(const CTSE_Handle& tse) const;

    /// Reset to null state
    void Reset(void);

    /// TSE info getters
    CBlobIdKey GetBlobId(void) const;

    bool Blob_IsSuppressed(void) const;
    bool Blob_IsSuppressedTemp(void) const;
    bool Blob_IsSuppressedPerm(void) const;
    bool Blob_IsDead(void) const;

    /// Complete and get const reference to the seq-entry
    CConstRef<CSeq_entry> GetCompleteTSE(void) const;

    /// Get const reference to the seq-entry
    CConstRef<CSeq_entry> GetTSECore(void) const;

    /// Get top level Seq-entry handle
    CSeq_entry_Handle GetTopLevelEntry(void) const;
    
    /// Get Bioseq handle from this TSE
    CBioseq_Handle GetBioseqHandle(const CSeq_id& id) const;
    CBioseq_Handle GetBioseqHandle(const CSeq_id_Handle& id) const;

    /// Register argument TSE as used by this TSE, so it will be
    /// released by scope only after this TSE is released.
    ///
    /// @param tse
    ///  Used TSE handle
    ///
    /// @return
    ///  True if argument TSE was successfully registered as 'used'.
    ///  False if argument TSE was not registered as 'used'.
    ///  Possible reasons:
    ///   Circular reference in 'used' tree.
    bool AddUsedTSE(const CTSE_Handle& tse) const;

    /// Return true if this TSE handle is local to scope and can be edited.
    bool CanBeEdited(void) const;

protected:
    friend class CScope_Impl;
    friend class CTSE_ScopeInfo;

    typedef CTSE_ScopeInfo TObject;

    CTSE_Handle(TObject& object);

private:

    CHeapScope          m_Scope;
    CTSE_ScopeUserLock  m_TSE;

public: // non-public section

    CTSE_Handle(const CTSE_ScopeUserLock& lock);

    TObject& x_GetScopeInfo(void) const;
    const CTSE_Info& x_GetTSE_Info(void) const;
    CScope_Impl& x_GetScopeImpl(void) const;
};


/////////////////////////////////////////////////////////////////////////////
// CTSE_Handle inline methods
/////////////////////////////////////////////////////////////////////////////


inline
CTSE_Handle::CTSE_Handle(void)
{
}


inline
CScope& CTSE_Handle::GetScope(void) const
{
    return m_Scope;
}


inline
CScope_Impl& CTSE_Handle::x_GetScopeImpl(void) const
{
    return *m_Scope.GetImpl();
}


inline
bool CTSE_Handle::operator==(const CTSE_Handle& tse) const
{
    return m_TSE == tse.m_TSE;
}


inline
bool CTSE_Handle::operator!=(const CTSE_Handle& tse) const
{
    return m_TSE != tse.m_TSE;
}


inline
bool CTSE_Handle::operator<(const CTSE_Handle& tse) const
{
    return m_TSE < tse.m_TSE;
}


inline
CTSE_Handle::TObject& CTSE_Handle::x_GetScopeInfo(void) const
{
    return const_cast<TObject&>(*m_TSE);
}


/////////////////////////////////////////////////////////////////////////////
// CScopeInfo classes
/////////////////////////////////////////////////////////////////////////////

class CScopeInfo_Base : public CObject
{
public:
    // creates object with one reference
    NCBI_XOBJMGR_EXPORT CScopeInfo_Base(void);
    NCBI_XOBJMGR_EXPORT CScopeInfo_Base(const CTSE_ScopeUserLock& tse,
                                        const CObject& info);
    NCBI_XOBJMGR_EXPORT CScopeInfo_Base(const CTSE_Handle& tse,
                                        const CObject& info);
    NCBI_XOBJMGR_EXPORT ~CScopeInfo_Base(void);
    
    bool IsRemoved(void) const
        {
            return !m_TSE_ScopeInfo;
        }
    bool HasObject(void) const
        {
            return m_ObjectInfo.NotNull();
        }
    bool IsValid(void) const
        {
            return m_ObjectInfo.NotNull();
        }

    typedef CSeq_id_Handle TIndexId;
    typedef vector<TIndexId> TIndexIds;

    virtual NCBI_XOBJMGR_EXPORT const TIndexIds* GetIndexIds(void) const;

    bool IsTemporary(void) const
        {
            const TIndexIds* ids = GetIndexIds();
            return !ids || ids->empty();
        }
    
    CTSE_ScopeInfo& x_GetTSE_ScopeInfo(void) const
        {
            CTSE_ScopeInfo* info = m_TSE_ScopeInfo;
            _ASSERT(info);
            return *info;
        }

    const CTSE_Handle& GetTSE_Handle(void) const
        {
            return m_TSE_Handle;
        }

    NCBI_XOBJMGR_EXPORT CScope_Impl& x_GetScopeImpl(void) const;

    const CObject& GetObjectInfo_Base(void) const
        {
            return *m_ObjectInfo;
        }

protected:
    friend class CTSE_ScopeInfo;
    friend class CScopeInfoLocker;

    // attached new tse and object info
    virtual NCBI_XOBJMGR_EXPORT void x_SetLock(const CTSE_ScopeUserLock& tse,
                                               const CObject& info);
    virtual NCBI_XOBJMGR_EXPORT void x_ResetLock(void);

    // disconnect from TSE
    virtual NCBI_XOBJMGR_EXPORT void x_AttachTSE(CTSE_ScopeInfo* tse);
    virtual NCBI_XOBJMGR_EXPORT void x_DetachTSE(CTSE_ScopeInfo* tse);
    virtual NCBI_XOBJMGR_EXPORT void x_ForgetTSE(CTSE_ScopeInfo* tse);

    enum ECheckFlags {
        fAllowZero  = 0x00,
        fForceZero  = 0x01,
        fForbidZero = 0x02,
        fAllowInfo  = 0x00,
        fForceInfo  = 0x10,
        fForbidInfo = 0x20
    };
    typedef int TCheckFlags;

    bool NCBI_XOBJMGR_EXPORT x_Check(TCheckFlags zero_counter_mode) const;
    void NCBI_XOBJMGR_EXPORT x_RemoveLastInfoLock(void);

    void AddInfoLock(void)
        {
            _ASSERT(x_Check(fForceInfo));
            m_LockCounter.Add(1);
            _ASSERT(x_Check(fForbidZero));
        }
    void RemoveInfoLock(void)
        {
            _ASSERT(x_Check(fForbidZero));
            if ( m_LockCounter.Add(-1) <= 0 ) {
                x_RemoveLastInfoLock();
            }
        }

private: // data members

    CTSE_ScopeInfo*         m_TSE_ScopeInfo; // null if object is removed.
    CAtomicCounter          m_LockCounter; // counts all referencing handles.
    // The following members are not null when handle is locked (counter > 0)
    // and not removed.
    CTSE_Handle             m_TSE_Handle; // locks TSE from releasing.
    CConstRef<CObject>      m_ObjectInfo; // current object info.

private: // to prevent copying
    CScopeInfo_Base(const CScopeInfo_Base&);
    void operator=(const CScopeInfo_Base&);
};


class CScopeInfoLocker : protected CObjectCounterLocker
{
public:
    void Lock(CScopeInfo_Base* info) const
        {
            CObjectCounterLocker::Lock(info);
            info->AddInfoLock();
        }
    void Unlock(CScopeInfo_Base* info) const
        {
            info->RemoveInfoLock();
            CObjectCounterLocker::Unlock(info);
        }
};


typedef CRef<CScopeInfo_Base, CScopeInfoLocker> CScopeInfo_RefBase;


template<class Info>
class CScopeInfo_Ref : public CScopeInfo_RefBase
{
public:
    typedef Info TScopeInfo;

    CScopeInfo_Ref(void)
        {
        }
    explicit CScopeInfo_Ref(TScopeInfo& info)
        : CScopeInfo_RefBase(toBase(&info))
        {
        }

    bool IsValid(void) const
        {
            return NotNull() && GetPointerOrNull()->IsValid();
        }

    void Reset(void)
        {
            CScopeInfo_RefBase::Reset();
        }

    void Reset(TScopeInfo* info)
        {
            CScopeInfo_RefBase::Reset(toBase(info));
        }

    TScopeInfo& operator*(void)
        {
            return *toInfo(GetNonNullPointer());
        }
    const TScopeInfo& operator*(void) const
        {
            return *toInfo(GetNonNullPointer());
        }
    TScopeInfo& GetNCObject(void) const
        {
            return *toInfo(GetNonNullNCPointer());
        }

    TScopeInfo* operator->(void)
        {
            return toInfo(GetNonNullPointer());
        }
    const TScopeInfo* operator->(void) const
        {
            return toInfo(GetNonNullPointer());
        }

protected:
    static CScopeInfo_Base* toBase(TScopeInfo* info)
        {
            return reinterpret_cast<CScopeInfo_Base*>(info);
        }
    static TScopeInfo* toInfo(CScopeInfo_Base* base)
        {
            return reinterpret_cast<TScopeInfo*>(base);
        }
    static const TScopeInfo* toInfo(const CScopeInfo_Base* base)
        {
            return reinterpret_cast<const TScopeInfo*>(base);
        }
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//OBJMGR_TSE_HANDLE__HPP
