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
*   Type description of 'SET OF' and 'SEQUENCE OF'
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  1999/12/03 21:42:14  vasilche
* Fixed conflict of enums in choices.
*
* Revision 1.4  1999/12/01 17:36:29  vasilche
* Fixed CHOICE processing.
*
* Revision 1.3  1999/11/16 15:41:18  vasilche
* Added plain pointer choice.
* By default we use C pointer instead of auto_ptr.
* Start adding initializers.
*
* Revision 1.2  1999/11/15 19:36:21  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <serial/stltypes.hpp>
#include <serial/autoptrinfo.hpp>
#include "unitype.hpp"
#include "blocktype.hpp"
#include "code.hpp"
#include "typestr.hpp"
#include "value.hpp"

CUniSequenceDataType::CUniSequenceDataType(const AutoPtr<CDataType>& element)
{
    SetElementType(element);
}

const char* CUniSequenceDataType::GetASNKeyword(void) const
{
    return "SEQUENCE";
}

void CUniSequenceDataType::SetElementType(const AutoPtr<CDataType>& type)
{
    if ( GetElementType() )
        THROW1_TRACE(runtime_error, "double element type " + LocationString());
    m_ElementType = type;
}

void CUniSequenceDataType::PrintASN(CNcbiOstream& out, int indent) const
{
    out << GetASNKeyword() << " OF ";
    GetElementType()->PrintASN(out, indent);
}

void CUniSequenceDataType::FixTypeTree(void) const
{
    CParent::FixTypeTree();
    m_ElementType->SetParent(this, "_E");
    m_ElementType->SetInSet(this);
}

bool CUniSequenceDataType::CheckType(void) const
{
    return m_ElementType->Check();
}

bool CUniSequenceDataType::CheckValue(const CDataValue& value) const
{
    const CBlockDataValue* block =
        dynamic_cast<const CBlockDataValue*>(&value);
    if ( !block ) {
        value.Warning("block of values expected");
        return false;
    }
    bool ok = true;
    iterate ( CBlockDataValue::TValues, i, block->GetValues() ) {
        if ( !m_ElementType->CheckValue(**i) )
            ok = false;
    }
    return ok;
}

TObjectPtr CUniSequenceDataType::CreateDefault(const CDataValue& ) const
{
    THROW1_TRACE(runtime_error, "SET/SEQUENCE OF default not implemented");
}

CTypeInfo* CUniSequenceDataType::CreateTypeInfo(void)
{
    return new CAutoPointerTypeInfo(
        new CStlClassInfoList<AnyType>(
            new CAnyTypeSource(m_ElementType.get())));
}

void CUniSequenceDataType::GetFullCType(CTypeStrings& tType,
                                    CClassCode& code) const
{
    string templ = GetVar("_type");
    CTypeStrings tData;
    GetElementType()->GetFullCType(tData, code);
    if ( !tData.CanBeInSTL() )
        tData.ToPointer();
    if ( templ.empty() )
        templ = "list";
    tType.AddHPPInclude(GetTemplateHeader(templ));
    tType.SetTemplate(GetTemplateNamespace(templ) + "::" + templ,
                      GetTemplateMacro(templ), tData);
}

CUniSetDataType::CUniSetDataType(const AutoPtr<CDataType>& elementType)
    : CParent(elementType)
{
}

const char* CUniSetDataType::GetASNKeyword(void) const
{
    return "SET";
}

CTypeInfo* CUniSetDataType::CreateTypeInfo(void)
{
    CStlClassInfoList<AnyType>* l =
        new CStlClassInfoList<AnyType>(new CAnyTypeSource(GetElementType()));
    l->SetRandomOrder();
    return new CAutoPointerTypeInfo(l);
}

void CUniSetDataType::GetFullCType(CTypeStrings& tType, CClassCode& code) const
{
    string templ = GetVar("_type");
    const CDataSequenceType* seq =
        dynamic_cast<const CDataSequenceType*>(GetElementType());
    if ( seq && seq->GetMembers().size() == 2 ) {
        CTypeStrings tKey;
        seq->GetMembers().front()->GetType()->GetFullCType(tKey, code);
        if ( tKey.CanBeKey() ) {
            CTypeStrings tValue;
            seq->GetMembers().back()->GetType()->GetFullCType(tValue, code);
            if ( !tValue.CanBeInSTL() )
                tValue.ToPointer();
            if ( templ.empty() )
                templ = "multimap";

            tType.AddHPPInclude(GetTemplateHeader(templ));
            tType.SetTemplate(GetTemplateNamespace(templ) + "::" + templ,
                              GetTemplateMacro(templ), tKey, tValue);
            return;
        }
    }
    CTypeStrings tData;
    GetElementType()->GetFullCType(tData, code);
    if ( !tData.CanBeInSTL() )
        tData.ToPointer();
    if ( templ.empty() )
        templ = "multiset";
    tType.AddHPPInclude(GetTemplateHeader(templ));
    tType.SetTemplate(GetTemplateNamespace(templ) + "::" + templ,
                      GetTemplateMacro(templ), tData);
}

