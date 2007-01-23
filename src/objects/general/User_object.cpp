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
 *   'general.asn'.
 */

// standard includes

// generated includes
#include <ncbi_pch.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbistr.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CUser_object::~CUser_object(void)
{
}


//
// retrieve a named field.  The named field can recurse, depending
// on a set of user-defined delimiters
//
const CUser_field& CUser_object::GetField(const string& str,
                                          const string& delim) const
{
    return *GetFieldRef(str, delim);
}


CConstRef<CUser_field> CUser_object::GetFieldRef(const string& str,
                                                 const string& delim) const
{
    list<string> toks;
    NStr::Split(str, delim, toks);

    CConstRef<CUser_object> obj_ref(this);
    CConstRef<CUser_field>  field_ref;
    ITERATE(list<string>, iter, toks) {
        if (obj_ref) {
            ITERATE(TData, field_iter, obj_ref->GetData()) {
                const CUser_field& field = **field_iter;
                if (field.IsSetLabel()  &&  field.GetLabel().IsStr()  &&
                    field.GetLabel().GetStr() == *iter) {
                    field_ref.Reset(&field);
                    obj_ref.Reset();
                    break;
                }
            }
        } else if (field_ref) {
            switch (field_ref->GetData().Which()) {
            case CUser_field::TData::e_Object:
                obj_ref.Reset(&field_ref->GetData().GetObject());
                field_ref.Reset();
                --iter;
                break;
            case CUser_field::TData::e_Objects:
                break;
            case CUser_field::TData::e_Fields:
                break;
            default:
                NCBI_THROW(CException, eUnknown,
                    "illegal recursion");
            }
        }
    }

    if ( !field_ref ) {
        /// not found in the standard pathway, so recurse the fields themselves
        ITERATE(TData, field_iter, GetData()) {
            field_ref = (*field_iter)->GetFieldRef(str, delim);
            if (field_ref) {
                break;
            }
        }
    }

    return field_ref;
}


bool CUser_object::HasField(const string& str,
                            const string& delim) const
{
    return GetFieldRef(str, delim).GetPointer() ? true : false;
}


//
// retrieve a named field.  The named field can recurse, depending
// on a set of user-defined delimiters
//
CUser_field& CUser_object::SetField(const string& str,
                                    const string& delim,
                                    const string& obj_subtype)
{
    return *SetFieldRef(str, delim, obj_subtype);
}


CRef<CUser_field> CUser_object::SetFieldRef(const string& str,
                                            const string& delim,
                                            const string& /* obj_subtype */)
{
    list<string> toks;
    NStr::Split(str, delim, toks);

    CRef<CUser_field>  field_ref;

    /// pass 1: see if we have a field that starts with this label already
    NON_CONST_ITERATE(TData, field_iter, SetData()) {
        CUser_field& field = **field_iter;
        if (field.GetLabel().GetStr() == toks.front()) {
            field_ref = *field_iter;
            break;
        }
    }

    if ( !field_ref ) {
        field_ref.Reset(new CUser_field());
        field_ref->SetLabel().SetStr(toks.front());
        SetData().push_back(field_ref);
    }

    toks.pop_front();
    if (toks.size()) {
        string s = NStr::Join(toks, delim);
        CRef<CUser_field> f = field_ref->SetFieldRef(s, delim);
        field_ref = f;
    }

    return field_ref;
}


// add a data field to the user object that holds a given value
CUser_object& CUser_object::AddField(const string& label,
                                     const string& value,
                                     EParseField parse)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    switch (parse) {
    case eParse_Number:
        try {
            int i = NStr::StringToInt(value);
            field->SetData().SetInt(i);
            break;
        }
        catch (...) {
        }

        try {
            double d = NStr::StringToDouble(value);
            field->SetData().SetReal(d);
            break;
        }
        catch (...) {
        }
        field->SetData().SetStr(value);
        break;

    default:
    case eParse_String:
        field->SetData().SetStr(value);
        break;
    }

    SetData().push_back(field);
    return *this;
}

CUser_object& CUser_object::AddField(const string& label,
                                     int           value)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    field->SetData().SetInt(value);

    SetData().push_back(field);
    return *this;
}


CUser_object& CUser_object::AddField(const string& label,
                                     double        value)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    field->SetData().SetReal(value);

    SetData().push_back(field);
    return *this;
}


CUser_object& CUser_object::AddField(const string& label,
                                     bool          value)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    field->SetData().SetBool(value);

    SetData().push_back(field);
    return *this;
}



CUser_object& CUser_object::AddField(const string& label,
                                     CUser_object& object)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    field->SetData().SetObject(object);

    SetData().push_back(field);
    return *this;
}


CUser_object& CUser_object::AddField(const string& label,
                                     const vector<string>& value)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    field->SetNum(value.size());
    field->SetData().SetStrs() = value;

    SetData().push_back(field);
    return *this;
}


CUser_object& CUser_object::AddField(const string& label,
                                     const vector<int>&    value)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    field->SetNum(value.size());
    field->SetData().SetInts() = value;

    SetData().push_back(field);
    return *this;
}


CUser_object& CUser_object::AddField(const string& label,
                                     const vector<double>& value)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    field->SetNum(value.size());
    field->SetData().SetReals() = value;

    SetData().push_back(field);
    return *this;
}


CUser_object&
CUser_object::AddField(const string& label,
                       const vector< CRef<CUser_object> >& objects)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    field->SetNum(objects.size());
    field->SetData().SetObjects() = objects;

    SetData().push_back(field);
    return *this;
}


CUser_object& CUser_object::AddField(const string& label,
                                     const vector<CRef<CUser_field> >& objects)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    field->SetNum(objects.size());
    field->SetData().SetFields() = objects;

    SetData().push_back(field);
    return *this;
}



// static consts here allow us to use reference counting
static const string s_ncbi("NCBI");
static const string s_expres("experimental_results");
static const string s_exp("experiment");
static const string s_sage("SAGE");
static const string s_tag("tag");
static const string s_count("count");


// accessors: classify a given user object
CUser_object::ECategory CUser_object::GetCategory(void) const
{
    if (!IsSetClass()  ||
        GetClass() != s_ncbi) {
        // we fail to recognize non-NCBI classes of user-objects
        return eCategory_Unknown;
    }

    //
    // experimental results
    //
    if (GetType().IsStr()  &&
        NStr::CompareNocase(GetType().GetStr(), s_expres) == 0  &&
        GetData().size() == 1) {

        ITERATE (CUser_object::TData, iter, GetData()) {
            const CUser_field& field       = **iter;
            const CUser_field::TData& data = field.GetData();
            if (data.Which() != CUser_field::TData::e_Object  ||
                !field.IsSetLabel()  ||
                !field.GetLabel().IsStr()  ||
                NStr::CompareNocase(field.GetLabel().GetStr(),
                                    s_exp) != 0) {
                // poorly formed experiment spec
                return eCategory_Unknown;
            }
        }

        return eCategory_Experiment;
    }

    //
    // unrecognized - catch-all
    //
    return eCategory_Unknown;
}


// sub-category accessors:
CUser_object::EExperiment CUser_object::GetExperimentType(void) const
{
    // check to see if we have an experiment
    if ( GetCategory() != eCategory_Experiment) {
        return eExperiment_Unknown;
    }

    // we do - so we have one nested user object that contains the
    // specification of the experimental data
    const CUser_field& field = *GetData().front();
    const CUser_object& obj = field.GetData().GetObject();
    if (obj.GetType().IsStr()  &&
        NStr::CompareNocase(obj.GetType().GetStr(), s_sage) == 0) {
        return eExperiment_Sage;
    }

    //
    // catch-all
    //
    return eExperiment_Unknown;
}


// sub-category accessors:
const CUser_object& CUser_object::GetExperiment(void) const
{
    switch (GetExperimentType()) {
    case eExperiment_Sage:
        // we have one nested user object that contains the
        // specification of the experimental data
        return GetData().front()->GetData().GetObject();

    case eExperiment_Unknown:
    default:
        return *this;
    }
}


//
// format the type specification fo a given user object
//
static string s_GetUserObjectType(const CUser_object& obj)
{
    switch (obj.GetCategory()) {
    case CUser_object::eCategory_Experiment:
        switch (obj.GetExperimentType()) {
        case CUser_object::eExperiment_Sage:
            return "SAGE";

        case CUser_object::eExperiment_Unknown:
        default:
            return "Experiment";
        }
        break;

    case CUser_object::eCategory_Unknown:
    default:
        break;
    }

    return "User";
}


static string s_GetUserObjectContent(const CUser_object& obj)
{
    switch (obj.GetCategory()) {
    case CUser_object::eCategory_Experiment:
        switch (obj.GetExperimentType()) {
        case CUser_object::eExperiment_Sage:
            {{
                string label;
                const CUser_object& nested_obj =
                    obj.GetData().front()->GetData().GetObject();

                // grab the tag and count fields
                const CUser_field* tag = NULL;
                const CUser_field* count = NULL;
                ITERATE (CUser_object::TData, iter, nested_obj.GetData()) {
                    const CUser_field& field = **iter;
                    if (!field.GetLabel().IsStr()) {
                        continue;
                    }

                    const string& lbl = field.GetLabel().GetStr();
                    if (NStr::CompareNocase(lbl, s_tag) == 0) {
                        tag = &field;
                    } else if (NStr::CompareNocase(lbl, s_count) == 0) {
                        count = &field;
                    }
                }

                if (tag  &&  tag->GetData().IsStr()) {
                    if ( !label.empty() ) {
                        label += " ";
                    }
                    label += s_tag + "=" + tag->GetData().GetStr();
                }

                if (count  &&  count->GetData().IsInt()) {
                    if ( !label.empty() ) {
                        label += " ";
                    }
                    label += s_count + "=" +
                        NStr::IntToString(count->GetData().GetInt());
                }

                return label;
            }}

        case CUser_object::eExperiment_Unknown:
        default:
            break;
        }
        return "[experiment]";

    case CUser_object::eCategory_Unknown:
    default:
        break;
    }
    return "[User]";
}


//
// append a formatted string to a label describing this object
//
void CUser_object::GetLabel(string* label, ELabelContent mode) const
{
    // Check the label is not null
    if (!label) {
        return;
    }

    switch (mode) {
    case eType:
        *label += s_GetUserObjectType(*this);
        break;
    case eContent:
        *label += s_GetUserObjectContent(*this);
        break;
    case eBoth:
        *label += s_GetUserObjectType(*this) + ": " +
            s_GetUserObjectContent(*this);
        break;
    }
}


CUser_object& CUser_object::SetCategory(ECategory category)
{
    Reset();
    SetClass(s_ncbi);
    switch (category) {
    case eCategory_Experiment:
        SetType().SetStr(s_expres);
        {{
            CRef<CUser_object> subobj(new CUser_object());
            AddField(s_exp, *subobj);
            SetClass(s_ncbi);
            return *subobj;
        }}

    case eCategory_Unknown:
    default:
        break;
    }

    return *this;
}


CUser_object& CUser_object::SetExperiment(EExperiment category)
{
    Reset();
    SetClass(s_ncbi);
    switch (category) {
    case eExperiment_Sage:
        SetType().SetStr(s_sage);
        break;

    case eExperiment_Unknown:
    default:
        break;
    }

    return *this;
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 6.13  2005/02/02 19:49:54  grichenk
* Fixed more warnings
*
* Revision 6.12  2004/11/24 15:56:04  dicuccio
* GetField(): added recursion of CUser_field if recursion in CUser_object fails.
* SetField(): make equivalent to GetField()
*
* Revision 6.11  2004/11/08 17:20:37  dicuccio
* Added GetFieldRef() and SetFieldRef() - returns the CConstRef<>/CRef<>
* corresponding to the named field.  White space changes.
*
* Revision 6.10  2004/10/28 18:40:54  dicuccio
* Corrected field recursion.  Added optional type string to classes
*
* Revision 6.9  2004/09/21 15:08:11  kans
* added EParseField parameter to AddField so it can optionally interpret the string value as numeric
*
* Revision 6.8  2004/07/27 15:12:03  ucko
* Use vectors for User-field.data SEQUENCE-OF choices.
*
* Revision 6.7  2004/05/19 17:21:39  gorelenk
* Added include of PCH - ncbi_pch.hpp
*
* Revision 6.6  2004/01/20 20:38:09  vasilche
* Added required includes.
*
* Revision 6.5  2003/11/21 14:45:01  grichenk
* Replaced runtime_error with CException
*
* Revision 6.4  2003/11/14 16:12:45  shomrat
* fixed GetLabel so it will append to string and not replace it
*
* Revision 6.3  2003/09/29 15:57:15  dicuccio
* Fleshed out CUser_object API.  Added function to retrieve fields based on
* delimited keys.  Added functions to format a CUser_object as a known category
* (the only supported category is experiment)
*
* Revision 6.2  2003/06/19 01:05:51  dicuccio
* Added interfaces for adding key/value items to a user object as a nested
* CUser_field object
*
* Revision 6.1  2002/10/03 17:02:45  clausen
* Added GetLabel()
*
*
* ===========================================================================
*/
/* Original file checksum: lines: 64, chars: 1893, CRC32: f7a70d1d */
