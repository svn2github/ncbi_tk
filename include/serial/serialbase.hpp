#ifndef SERIALBASE__HPP
#define SERIALBASE__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   File to be included in all headers generated by datatool
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <serial/exception.hpp>
#include <serial/serialdef.hpp>
#include <typeinfo>

#define BITSTRING_AS_VECTOR    0

#if !BITSTRING_AS_VECTOR
#include <util/bitset/ncbi_bitset.hpp>
#include <util/bitset/bmserial.h>
#endif


/** @addtogroup GenClassSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

// CBitString
#if BITSTRING_AS_VECTOR
typedef std::vector< bool > CBitString;
#else
typedef bm::bvector< > CBitString;
#endif

class CTypeInfo;
class CClassTypeInfo;
class CChoiceTypeInfo;
class CEnumeratedTypeValues;

// enum for choice classes generated by datatool
enum EResetVariant {
    eDoResetVariant,
    eDoNotResetVariant
};

typedef void (*TPreReadFunction)(const CTypeInfo* info, void* object);
typedef void (*TPostReadFunction)(const CTypeInfo* info, void* object);
typedef void (*TPreWriteFunction)(const CTypeInfo* info, const void* object);
typedef void (*TPostWriteFunction)(const CTypeInfo* info, const void* object);

NCBI_XSERIAL_EXPORT
void SetPreRead(CClassTypeInfo*  info, TPreReadFunction function);

NCBI_XSERIAL_EXPORT
void SetPostRead(CClassTypeInfo*  info, TPostReadFunction function);

NCBI_XSERIAL_EXPORT
void SetPreRead(CChoiceTypeInfo* info, TPreReadFunction function);

NCBI_XSERIAL_EXPORT
void SetPostRead(CChoiceTypeInfo* info, TPostReadFunction function);

NCBI_XSERIAL_EXPORT
void SetPreWrite(CClassTypeInfo*  info, TPreWriteFunction function);

NCBI_XSERIAL_EXPORT
void SetPostWrite(CClassTypeInfo*  info, TPostWriteFunction function);

NCBI_XSERIAL_EXPORT
void SetPreWrite(CChoiceTypeInfo* info, TPreWriteFunction function);

NCBI_XSERIAL_EXPORT
void SetPostWrite(CChoiceTypeInfo* info, TPostWriteFunction function);

template<class Class>
class CClassPrePostReadWrite
{
public:
    static void PreRead(const CTypeInfo* /*info*/, void* object)
        {
            static_cast<Class*>(object)->PreRead();
        }
    static void PostRead(const CTypeInfo* /*info*/, void* object)
        {
            static_cast<Class*>(object)->PostRead();
        }
    static void PreWrite(const CTypeInfo* /*info*/, const void* object)
        {
            static_cast<const Class*>(object)->PreWrite();
        }
    static void PostWrite(const CTypeInfo* /*info*/, const void* object)
        {
            static_cast<const Class*>(object)->PostWrite();
        }
};

// Base class for all serializable objects
class NCBI_XSERIAL_EXPORT CSerialObject : public CObject
{
public:
    CSerialObject(void);
    virtual ~CSerialObject(void);
    virtual const CTypeInfo* GetThisTypeInfo(void) const = 0;
    virtual void Assign(const CSerialObject& source,
                        ESerialRecursionMode how = eRecursive);
    virtual bool Equals(const CSerialObject& object,
                        ESerialRecursionMode how = eRecursive) const;
    virtual void DebugDump(CDebugDumpContext ddc, unsigned int depth) const;

    void ThrowUnassigned(TMemberIndex index) const;
    // for all GetX() methods called in the current thread
    static  void SetVerifyDataThread(ESerialVerifyData verify);
    // for all GetX() methods called in the current process
    static  void SetVerifyDataGlobal(ESerialVerifyData verify);

    static const char* ms_UnassignedStr;
    static const char  ms_UnassignedByte;

    bool HasNamespaceName(void) const;
    const string& GetNamespaceName(void) const;
    void SetNamespaceName(const string& ns_name);

    bool HasNamespacePrefix(void) const;
    const string& GetNamespacePrefix(void) const;
    void SetNamespacePrefix(const string& ns_prefix);

private:
    static ESerialVerifyData x_GetVerifyData(void);
    static ESerialVerifyData ms_VerifyDataDefault;
};

class NCBI_XSERIAL_EXPORT CSerialAttribInfoItem
{
public:
    CSerialAttribInfoItem(const string& name,
                          const string& ns_name, const string& value);
    CSerialAttribInfoItem(const CSerialAttribInfoItem& other);
    virtual ~CSerialAttribInfoItem(void);

    const string& GetName(void) const;
    const string& GetNamespaceName(void) const;
    const string& GetValue(void) const;
private:
    string m_Name;
    string m_NsName;
    string m_Value;
};

class NCBI_XSERIAL_EXPORT CAnyContentObject : public CSerialObject
{
public:
    CAnyContentObject(void);
    CAnyContentObject(const CAnyContentObject& other);
    virtual ~CAnyContentObject(void);

    virtual const CTypeInfo* GetThisTypeInfo(void) const
    { return GetTypeInfo(); }
    static const CTypeInfo* GetTypeInfo(void);

    void Reset(void);
    CAnyContentObject& operator= (const CAnyContentObject& other);
    bool operator== (const CAnyContentObject& other) const;

    void SetName(const string& name);
    const string& GetName(void) const;
    void SetValue(const string& value);
    const string& GetValue(void) const;

    void SetNamespaceName(const string& ns_name);
    const string& GetNamespaceName(void) const;
    void SetNamespacePrefix(const string& ns_prefix);
    const string& GetNamespacePrefix(void) const;

    void AddAttribute(const string& name,
                      const string& ns_name, const string& value);
    const vector<CSerialAttribInfoItem>& GetAttributes(void) const;

private:
    void x_Copy(const CAnyContentObject& other);
    void x_Decode(const string& value);
    string m_Name;
    string m_Value;
    string m_NsName;
    string m_NsPrefix;
    vector<CSerialAttribInfoItem> m_Attlist;
};

// Base class for user-defined serializable classes
// to allow for objects assignment and comparison.
// EXAMPLE:
//   class CSeq_entry : public CSeq_entry_Base, CSerialUserOp
//
class NCBI_XSERIAL_EXPORT CSerialUserOp
{
    friend class CClassTypeInfo;
    friend class CChoiceTypeInfo;
public:
    virtual ~CSerialUserOp() { }
protected:
    // will be called after copying the datatool-generated members
    virtual void UserOp_Assign(const CSerialUserOp& source) = 0;
    // will be called after comparing the datatool-generated members
    virtual bool UserOp_Equals(const CSerialUserOp& object) const = 0;
};


/////////////////////////////////////////////////////////////////////
//
// Alias wrapper templates
//

template <class TPrim>
class NCBI_XSERIAL_EXPORT CAliasBase
{
public:
    typedef CAliasBase<TPrim> TThis;

    CAliasBase(void) {}
    explicit CAliasBase(const TPrim& value)
        : m_Data(value) {}

    const TPrim& Get(void) const
        {
            return m_Data;
        }
    TPrim& Set(void)
        {
            return m_Data;
        }
    void Set(const TPrim& value)
        {
            m_Data = value;
        }
    operator TPrim(void) const
        {
            return m_Data;
        }

    TThis& operator*(void)
        {
            return *this;
        }
    TThis* operator->(void)
        {
            return this;
        }

    bool operator<(const TPrim& value) const
        {
            return m_Data < value;
        }
    bool operator>(const TPrim& value) const
        {
            return m_Data > value;
        }
    bool operator==(const TPrim& value) const
        {
            return m_Data == value;
        }
    bool operator!=(const TPrim& value) const
        {
            return m_Data != value;
        }

    static TConstObjectPtr GetDataPtr(const TThis* alias)
        {
            return &alias->m_Data;
        }

protected:
    TPrim m_Data;
};


template <class TStd>
class NCBI_XSERIAL_EXPORT CStdAliasBase : public CAliasBase<TStd>
{
    typedef CAliasBase<TStd> TParent;
    typedef CStdAliasBase<TStd> TThis;
public:
    CStdAliasBase(void)
        : TParent(0) {}
    explicit CStdAliasBase(const TStd& value)
        : TParent(value) {}
};


template <class TString>
class NCBI_XSERIAL_EXPORT CStringAliasBase : public CAliasBase<TString>
{
    typedef CAliasBase<TString> TParent;
    typedef CStringAliasBase<TString> TThis;
public:
    CStringAliasBase(void)
        : TParent() {}
    explicit CStringAliasBase(const TString& value)
        : TParent(value) {}
};


template<typename T>
class CUnionBuffer
{   // char buffer support, used in choices
public:
    typedef T    TObject;                  // object type
    typedef char TBuffer[sizeof(TObject)]; // char buffer type

    // cast to object type
    TObject& operator*(void)
        {
            return *reinterpret_cast<TObject*>(m_Buffer);
        }
    const TObject& operator*(void) const
        {
            return *reinterpret_cast<const TObject*>(m_Buffer);
        }

    // construct/destruct object
    void Construct(void)
        {
            ::new(static_cast<void*>(m_Buffer)) TObject();
        }
    void Destruct(void)
        {
            (**this).~TObject();
        }
    
private:
    TBuffer m_Buffer;
};


/////////////////////////////////////////////////////////////////////
//
//  Assignment and comparison for serializable objects
//

template <class C>
C& SerialAssign(C& dest, const C& src, ESerialRecursionMode how = eRecursive)
{
    if ( typeid(src) != typeid(dest) ) {
        ERR_POST(Fatal <<
                 "SerialAssign() -- Assignment of incompatible types: " <<
                 typeid(dest).name() << " = " << typeid(src).name());
    }
    C::GetTypeInfo()->Assign(&dest, &src, how);
    return dest;
}

template <class C>
bool SerialEquals(const C& object1, const C& object2,
                  ESerialRecursionMode how = eRecursive)
{
    if ( typeid(object1) != typeid(object2) ) {
        ERR_POST(Fatal <<
                 "SerialAssign() -- Can not compare types: " <<
                 typeid(object1).name() << " == " << typeid(object2).name());
    }
    return C::GetTypeInfo()->Equals(&object1, &object2, how);
}

// create on heap a clone of the source object
template <typename C>
C* SerialClone(const C& src)
{
    typename C::TTypeInfo type = C::GetTypeInfo();
    TObjectPtr obj = type->Create();
    type->Assign(obj, &src);
    return static_cast<C*>(obj);
}

/////////////////////////////////////////////////////////////////////////////
//
//  I/O stream manipulators and helpers for serializable objects
//

// Helper base class
class NCBI_XSERIAL_EXPORT MSerial_Flags
{
protected:
    MSerial_Flags(unsigned long all, unsigned long flags);
private:
    MSerial_Flags(void) {}
    MSerial_Flags(const MSerial_Flags&) {}
    MSerial_Flags& operator= (const MSerial_Flags&) {return *this;}

    void SetFlags(CNcbiIos& io) const;
    unsigned long m_All;
    unsigned long m_Flags;

    friend CNcbiOstream& operator<< (CNcbiOstream& io, const MSerial_Flags& obj);
    friend CNcbiIstream& operator>> (CNcbiIstream& io, const MSerial_Flags& obj);
};

inline
CNcbiOstream& operator<< (CNcbiOstream& io, const MSerial_Flags& obj)
{
    obj.SetFlags(io);
    return io;
}
inline
CNcbiIstream& operator>> (CNcbiIstream& io, const MSerial_Flags& obj)
{
    obj.SetFlags(io);
    return io;
}

// Formatting
class NCBI_XSERIAL_EXPORT MSerial_Format : public MSerial_Flags
{
public:
    explicit MSerial_Format(ESerialDataFormat fmt);
};

// Class member assignment verification
class NCBI_XSERIAL_EXPORT MSerial_VerifyData : public MSerial_Flags
{
public:
    explicit MSerial_VerifyData(ESerialVerifyData fmt);
};

// Default string encoding in XML streams
class NCBI_XSERIAL_EXPORT MSerialXml_DefaultStringEncoding : public MSerial_Flags
{
public:
    explicit MSerialXml_DefaultStringEncoding(EEncoding fmt);
};

// Formatting
NCBI_XSERIAL_EXPORT CNcbiIos& MSerial_AsnText(CNcbiIos& io);
NCBI_XSERIAL_EXPORT CNcbiIos& MSerial_AsnBinary(CNcbiIos& io);
NCBI_XSERIAL_EXPORT CNcbiIos& MSerial_Xml(CNcbiIos& io);

// Class member assignment verification
NCBI_XSERIAL_EXPORT CNcbiIos& MSerial_VerifyDefault(CNcbiIos& io);
NCBI_XSERIAL_EXPORT CNcbiIos& MSerial_VerifyNo(CNcbiIos& io);
NCBI_XSERIAL_EXPORT CNcbiIos& MSerial_VerifyYes(CNcbiIos& io);
NCBI_XSERIAL_EXPORT CNcbiIos& MSerial_VerifyDefValue(CNcbiIos& io);

class CConstObjectInfo;
class CObjectInfo;
// Input/output
NCBI_XSERIAL_EXPORT CNcbiOstream& operator<< (CNcbiOstream& str, const CSerialObject& obj);
NCBI_XSERIAL_EXPORT CNcbiIstream& operator>> (CNcbiIstream& str, CSerialObject& obj);
NCBI_XSERIAL_EXPORT CNcbiOstream& operator<< (CNcbiOstream& str, const CConstObjectInfo& obj);
NCBI_XSERIAL_EXPORT CNcbiIstream& operator>> (CNcbiIstream& str, const CObjectInfo& obj);


END_NCBI_SCOPE

// these methods must be defined in root namespace so they have prefix NCBISER

// default functions do nothing
template<class CInfo>
inline
void NCBISERSetPreRead(const void* /*object*/, CInfo* /*info*/)
{
}

template<class CInfo>
inline
void NCBISERSetPostRead(const void* /*object*/, CInfo* /*info*/)
{
}

template<class CInfo>
inline
void NCBISERSetPreWrite(const void* /*object*/, CInfo* /*info*/)
{
}

template<class CInfo>
inline
void NCBISERSetPostWrite(const void* /*object*/, CInfo* /*info*/)
{
}

// define for declaring specific function
#define NCBISER_HAVE_PRE_READ(Class) \
template<class CInfo> \
inline \
void NCBISERSetPreRead(const Class* /*object*/, CInfo* info) \
{ \
    NCBI_NS_NCBI::SetPreRead \
        (info, &NCBI_NS_NCBI::CClassPrePostReadWrite<Class>::PreRead);\
}

#define NCBISER_HAVE_POST_READ(Class) \
template<class CInfo> \
inline \
void NCBISERSetPostRead(const Class* /*object*/, CInfo* info) \
{ \
    NCBI_NS_NCBI::SetPostRead \
        (info, &NCBI_NS_NCBI::CClassPrePostReadWrite<Class>::PostRead);\
}

#define NCBISER_HAVE_PRE_WRITE(Class) \
template<class CInfo> \
inline \
void NCBISERSetPreWrite(const Class* /*object*/, CInfo* info) \
{ \
    NCBI_NS_NCBI::SetPreWrite \
        (info, &NCBI_NS_NCBI::CClassPrePostReadWrite<Class>::PreWrite);\
}

#define NCBISER_HAVE_POST_WRITE(Class) \
template<class CInfo> \
inline \
void NCBISERSetPostWrite(const Class* /*object*/, CInfo* info) \
{ \
    NCBI_NS_NCBI::SetPostWrite \
        (info, &NCBI_NS_NCBI::CClassPrePostReadWrite<Class>::PostWrite);\
}

#define DECLARE_INTERNAL_TYPE_INFO() \
    typedef const NCBI_NS_NCBI::CTypeInfo* TTypeInfo; \
    virtual TTypeInfo GetThisTypeInfo(void) const { return GetTypeInfo(); } \
    static  TTypeInfo GetTypeInfo(void)

#define ENUM_METHOD_NAME(EnumName) \
    NCBI_NAME2(GetTypeInfo_enum_,EnumName)
#define DECLARE_ENUM_INFO(EnumName) \
    const NCBI_NS_NCBI::CEnumeratedTypeValues* ENUM_METHOD_NAME(EnumName)(void)
#define DECLARE_INTERNAL_ENUM_INFO(EnumName) \
    static DECLARE_ENUM_INFO(EnumName)

#define DECLARE_STD_ALIAS_TYPE_INFO() \
    static const NCBI_NS_NCBI::CTypeInfo* GetTypeInfo(void)

#if HAVE_NCBI_C

#define ASN_STRUCT_NAME(AsnStructName) NCBI_NAME2(struct_, AsnStructName)
#define ASN_STRUCT_METHOD_NAME(AsnStructName) \
    NCBI_NAME2(GetTypeInfo_struct_,AsnStructName)

#define DECLARE_ASN_TYPE_INFO(AsnStructName) \
    const NCBI_NS_NCBI::CTypeInfo* ASN_STRUCT_METHOD_NAME(AsnStructName)(void)
#define DECLARE_ASN_STRUCT_INFO(AsnStructName) \
    struct ASN_STRUCT_NAME(AsnStructName); \
    DECLARE_ASN_TYPE_INFO(AsnStructName); \
    inline \
    const NCBI_NS_NCBI::CTypeInfo* \
    GetAsnStructTypeInfo(const ASN_STRUCT_NAME(AsnStructName)* ) \
    { \
        return ASN_STRUCT_METHOD_NAME(AsnStructName)(); \
    } \
    struct ASN_STRUCT_NAME(AsnStructName)

#define DECLARE_ASN_CHOICE_INFO(AsnChoiceName) \
    DECLARE_ASN_TYPE_INFO(AsnChoiceName)

#endif

/* @} */

#endif  /* SERIALBASE__HPP */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.44  2006/12/07 18:59:30  gouriano
* Reviewed doxygen groupping, added documentation
*
* Revision 1.43  2006/01/19 18:22:34  gouriano
* Added possibility to save bit string data in compressed format
*
* Revision 1.42  2005/12/13 21:12:05  gouriano
* Corrected definition of bit string class
*
* Revision 1.41  2005/12/05 20:10:18  gouriano
* Added i/o stream manipulators with parameters for serializable objects
*
* Revision 1.40  2005/11/29 17:42:49  gouriano
* Added CBitString class
*
* Revision 1.39  2005/05/09 18:45:08  ucko
* Ensure that widely-included classes with virtual methods have virtual dtors.
*
* Revision 1.38  2005/04/26 14:17:51  vasilche
* Implemented CUnionBuffer for inlined objects in choices.
*
* Revision 1.37  2005/02/24 14:38:44  gouriano
* Added PreRead/PostWrite hooks
*
* Revision 1.36  2004/08/24 16:42:03  vasilche
* Removed TAB symbols in sources.
*
* Revision 1.35  2004/08/05 18:32:12  vasilche
* Added EXPORT to CStdAliasBase<> and CStringAliasBase<>.
*
* Revision 1.34  2004/08/04 14:44:48  vasilche
* Added EXPORT to CAliasInfo to workaround bug in MSVC 7.
*
* Revision 1.33  2004/07/29 19:57:08  vasilche
* Added operators to read/write CObjectInfo.
*
* Revision 1.32  2004/05/22 20:19:17  jcherry
* Made CAliasBase::TThis typedef public
*
* Revision 1.31  2004/04/26 16:40:59  ucko
* Tweak for GCC 3.4 compatibility.
*
* Revision 1.30  2004/03/25 15:56:28  gouriano
* Added possibility to copy and compare serial object non-recursively
*
* Revision 1.29  2004/02/02 14:39:52  vasilche
* Removed call to string::resize(0) in constructor - it's empty by default.
*
* Revision 1.28  2004/01/20 14:58:46  dicuccio
* FIxed use of export specifiers - located before return type of function
*
* Revision 1.27  2004/01/16 21:50:41  gouriano
* added export specifiers to i/o manipulators
*
* Revision 1.25  2003/11/13 14:06:44  gouriano
* Elaborated data verification on read/write/get to enable skipping mandatory class data members
*
* Revision 1.24  2003/10/21 13:48:47  grichenk
* Redesigned type aliases in serialization library.
* Fixed the code (removed CRef-s, added explicit
* initializers etc.)
*
* Revision 1.23  2003/09/22 20:57:07  gouriano
* Changed base type of AnyContent object to CSerialObject
*
* Revision 1.22  2003/09/16 14:49:15  gouriano
* Enhanced AnyContent objects to support XML namespaces and attribute info items.
*
* Revision 1.21  2003/08/25 15:58:32  gouriano
* added possibility to use namespaces in XML i/o streams
*
* Revision 1.20  2003/08/13 15:47:02  gouriano
* implemented serialization of AnyContent objects
*
* Revision 1.19  2003/04/29 18:29:06  gouriano
* object data member initialization verification
*
* Revision 1.18  2003/04/15 16:18:48  siyan
* Added doxygen support
*
* Revision 1.17  2003/03/28 18:52:04  dicuccio
* Added Win32 exports for postread/postwrite set hooks
*
* Revision 1.16  2003/03/25 13:08:42  dicuccio
* Added missing NCBI_NS_NCBI:: in PostRead()/PostWrite() macros
*
* Revision 1.15  2003/03/11 18:00:08  gouriano
* reimplement CInvalidChoiceSelection exception
*
* Revision 1.14  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.13  2002/05/29 21:18:43  gouriano
* added debug dump
*
* Revision 1.12  2002/05/22 14:03:37  grichenk
* CSerialUserOp -- added prefix UserOp_ to Assign() and Equals()
*
* Revision 1.11  2002/05/15 20:22:02  grichenk
* Added CSerialObject -- base class for all generated ASN.1 classes
*
* Revision 1.10  2001/07/25 19:15:27  grichenk
* Added comments. Added type checking before dynamic cast.
*
* Revision 1.9  2001/07/16 16:22:47  grichenk
* Added CSerialUserOp class to create Assign() and Equals() methods for
* user-defind classes.
* Added SerialAssign<>() and SerialEquals<>() functions.
*
* Revision 1.8  2000/12/15 21:28:49  vasilche
* Moved some typedefs/enums from corelib/ncbistd.hpp.
* Added flags to CObjectIStream/CObjectOStream: eFlagAllowNonAsciiChars.
* TByte typedef replaced by Uint1.
*
* Revision 1.7  2000/07/12 13:30:56  vasilche
* Typo in function prototype.
*
* Revision 1.6  2000/07/11 20:34:51  vasilche
* File included in all generated headers made lighter.
* Nonnecessary code moved to serialimpl.hpp.
*
* Revision 1.5  2000/07/10 17:59:30  vasilche
* Moved macros needed in headers to serialbase.hpp.
* Use DECLARE_ENUM_INFO in generated code.
*
* Revision 1.4  2000/06/16 16:31:07  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.3  2000/05/04 16:21:36  vasilche
* Fixed bug in choice reset.
*
* Revision 1.2  2000/04/28 16:58:03  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* Revision 1.1  2000/04/03 18:47:09  vasilche
* Added main include file for generated headers.
* serialimpl.hpp is included in generated sources with GetTypeInfo methods
*
* ===========================================================================
*/
