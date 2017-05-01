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
* Author: Andrei Gourianov
*
* File Description:
*   JSON Schema parser
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include "exceptions.hpp"
#include "jsdparser.hpp"
#include "tokens.hpp"
#include "module.hpp"
#include <serial/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Serial_Parsers

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
// JSDParser

JSDParser::JSDParser(JSDLexer& lexer)
    : DTDParser(lexer)
{
    m_SrcType = eJson;
}

JSDParser::~JSDParser(void)
{
}

void JSDParser::BeginDocumentTree(void)
{
//    Reset();
}

void JSDParser::BuildDocumentTree(CDataTypeModule& module)
{
    string value;
    TToken tok;
    m_URI.clear();
    m_URI.push_back("#");
    for (;;) {
        tok = GetNextToken();
        switch (tok) {
        case K_BEGIN_OBJECT:
            ParseRoot();
            break;
        case T_EOF:
            return;
        default:
            break;
        }
    }
}

void JSDParser::ParseRoot(void)
{
    string node_id(m_URI.front());
    DTDElement& node = m_MapElement[node_id];
    node.SetName("JsonValue");
    ParseNode(node);
}

void JSDParser::ParseObjectContent(DTDElement* owner)
{
    TToken tok;
    for (tok = GetNextToken(); tok != K_END_OBJECT; tok = GetNextToken()) {
        if (tok == K_KEY) {
            ParseMemberDefinition(owner);
        }
    }
}

void JSDParser::ParseMemberDefinition(DTDElement* owner)
{
    string key = Value();
    m_URI.push_back(key);
    string node_id = NStr::Join(m_URI,"/");
    DTDElement& item = m_MapElement[node_id];
    item.SetName(key);
    item.SetNamed();
    if (owner) {
        item.SetEmbedded();
    }
    TToken tok = GetNextToken();
    if (tok == K_BEGIN_OBJECT) {
        ParseNode(item);
    }
    if (owner) {
	    AddElementContent(*owner,node_id);
    }
    m_URI.pop_back();
}

void JSDParser::ParseArrayContent(DTDElement& node)
{
    string node_id = NStr::Join(m_URI,"/");
    DTDElement& item = m_MapElement[node_id];
    item.SetOccurrence(DTDElement::eZeroOrMore);
    ParseNode(item);
    item.SetEmbedded();
	AddElementContent(node,node_id);
}

void JSDParser::ParseNode(DTDElement& node)
{
    string key;
    TToken tok;
    bool is_object = false;
    bool has_additional_prop = false;//true;
    node.SetSourceLine( Lexer().CurrentLine());
    for (tok = GetNextToken(); tok != K_END_OBJECT; tok = GetNextToken()) {
        if (tok == K_KEY) {
            key = Value();
            tok = GetNextToken();
            if (tok == K_VALUE) {
                if (key == "$schema") {
                } else if (key == "title") {
                    if (!node.IsNamed()) {
                        node.SetName(Value());
                    }
                } else if (key == "type") {
                    if (Value() == "string") {
                        node.SetType(DTDElement::eString);
                    } else if (Value() == "integer") {
                        node.SetType(DTDElement::eInteger);
                    } else if (Value() == "number") {
                        node.SetType(DTDElement::eDouble);
                    } else if (Value() == "boolean") {
                        node.SetType(DTDElement::eBoolean);
                    } else if (Value() == "null") {
                        node.SetType(DTDElement::eEmpty);
                    } else if (Value() == "object") {
                        node.SetType(DTDElement::eSequence);
                    } else if (Value() == "array") {
                        node.SetType(DTDElement::eSequence);
                    }
                } else if (key == "default") {
                    node.SetDefault(Value());
                    AdjustMinOccurence(node, 0);
                } else if (key == "description") {
                    node.Comments().Add(Value());
                } else if (key == "$ref") {
                    node.SetTypeName(Value());
                    node.SetType(DTDElement::eAlias);
                } else {
                    ERR_POST_X(8, Warning << GetLocation() << "Unsupported property: " << key);
                }
            } else if (tok == T_NUMBER) {
                if (key == "minItems") {
                    AdjustMinOccurence(node, NStr::StringToInt(Value()));
                } else {
                    ERR_POST_X(8, Warning << GetLocation() << "Unsupported property: " << key);
                }
            } else if (tok == K_BEGIN_OBJECT) {
                if (key == "properties") {
                    is_object = true;
                    ParseObjectContent(&node);
                    node.SetDefaultRefsOccurence(DTDElement::eZeroOrOne);
                } else if (key == "items") {
                    m_URI.push_back(key);
                    ParseArrayContent(node);
                    m_URI.pop_back();
                } else if (key == "definitions") {
                    m_URI.push_back(key);
                    ParseObjectContent(nullptr);
                    m_URI.pop_back();
                } else if (key == "dependencies") {
                    ParseDependencies(node);
                } else {
                    ERR_POST_X(8, Warning << GetLocation() << "Unsupported property: " << key);
                    SkipUnknown(K_END_OBJECT);
                }
            } else if (tok == K_BEGIN_ARRAY) {
                if (key == "required") {
                    ParseRequired(node);
                } else if (key == "enum") {
                    ParseEnumeration(node);
                } else if (key == "oneOf") {
                    ParseOneOf(node);
                } else if (key == "anyOf") {
                    ParseAnyOf(node);
                } else if (key == "allOf") {
                    ParseAllOf(node);
                } else if (key == "type") {
                    ParseError("type arrays not supported", "string");
                } else if (key == "items") {
                    ParseError("tuple validation not supported", "{");
                } else {
                    ERR_POST_X(8, Warning << GetLocation() << "Unsupported property: " << key);
                    SkipUnknown(K_END_ARRAY);
                }
#if 0
            } else if (tok == K_TRUE || tok == K_FALSE) {
                if (key == "additionalProperties") {
                    has_additional_prop = tok == K_TRUE;
                } else {
                    ERR_POST_X(8, Warning << GetLocation() << "Unsupported property: " << key);
                }
#endif
            } else {
                ERR_POST_X(8, Warning << GetLocation() << "Unsupported property: " << key);
            }
        }
    }
    if (is_object && has_additional_prop) {
        string item_id = NStr::Join(m_URI,"/") + "*";
        DTDElement& item = m_MapElement[item_id];
        item.SetType(DTDElement::eAny);
        item.SetOccurrence(DTDElement::eZeroOrMore);
        item.SetEmbedded();
        item.SetName(CreateEmbeddedName(item,0));
        AddElementContent(node, item_id);
    }
}

void JSDParser::AdjustMinOccurence(DTDElement& node, int occ)
{
    DTDElement::EOccurrence occNow = node.GetOccurrence();
    DTDElement::EOccurrence occNew = occNow;
    if (occ == 0) {
        if (occNow == DTDElement::eOne) {
            occNew = DTDElement::eZeroOrOne;
        } else if (occNow == DTDElement::eOneOrMore) {
            occNew = DTDElement::eZeroOrMore;
        }
    } else {
        if (occ != 1) {
            ERR_POST_X(8, Warning << GetLocation() << "Unsupported property minItems= " << occ);
        }
        if (occNow == DTDElement::eZeroOrOne) {
            occNew = DTDElement::eOne;
        } else if (occNow == DTDElement::eZeroOrMore) {
            occNew = DTDElement::eOneOrMore;
        }
    }
    node.SetOccurrence(occNew);
}

void JSDParser::ParseRequired(DTDElement& node)
{
    TToken tok;
    for (tok = GetNextToken(); tok != K_END_ARRAY; tok = GetNextToken()) {
        if (tok == K_VALUE) {
            m_URI.push_back(Value());
            string node_id = NStr::Join(m_URI,"/");
            m_URI.pop_back();
            if (m_MapElement[node_id].GetDefault().empty()) {
                node.SetOccurrence(node_id, DTDElement::eOne);
            }
        }
    }
}

void JSDParser::ParseDependencies(DTDElement& node)
{
    string key, node_id;
    TToken tok;
    int i=0;
    for (tok = GetNextToken(); tok != K_END_OBJECT; tok = GetNextToken()) {
        if (tok == K_KEY) {
            key = Value();
            m_URI.push_back(Value());
            node_id = NStr::Join(m_URI,"/");
            m_URI.pop_back();
            if (node.RemoveContent(node_id)) {
                DTDElement::EOccurrence occ = node.GetOccurrence(node_id);
                string seq_id = NStr::Join(m_URI,"/") + "/" + NStr::NumericToString(i++);
                DTDElement& seq = m_MapElement[seq_id];
                seq.SetType(DTDElement::eSequence);
                seq.SetName(seq_id);
                seq.SetEmbedded();
                seq.SetOccurrence(occ);
                seq.SetOccurrence(node_id, occ);
                AddElementContent(seq, node_id);
                AddElementContent(node, seq_id);
                tok = GetNextToken();
                if (tok == K_BEGIN_OBJECT) {
                    ParseNode(seq);
                } else {
                    for (tok = GetNextToken(); tok != K_END_ARRAY; tok = GetNextToken()) {
                        if (tok == K_VALUE) {
                            key = Value();
                            m_URI.push_back(Value());
                            node_id = NStr::Join(m_URI,"/");
                            m_URI.pop_back();
                            node.RemoveContent(node_id);
                            DTDElement::EOccurrence occ = node.GetOccurrence(node_id);
                            seq.SetOccurrence(node_id, occ);
                            AddElementContent(seq, node_id);
                        }
                    }
                }
            } else {
                tok = GetNextToken();
                if (tok == K_BEGIN_OBJECT) {
                    SkipUnknown(K_END_OBJECT);
                } else if (tok == K_BEGIN_ARRAY) {
                    SkipUnknown(K_END_ARRAY);
                } else {
                    ParseError("Invalid schema", "{ or [");
                }
            }
        } else if (tok != T_SYMBOL) {
            ParseError("Invalid schema", "element name");
        }
    }
    FixEmbeddedNames(node);
}

void JSDParser::ParseEnumeration(DTDElement& node)
{
    bool isknown = true;
    if (node.GetType() == DTDElement::eInteger) {
        node.ResetType(DTDElement::eUnknown);
        node.SetType(DTDElement::eIntEnum);
    } else if (node.GetType() == DTDElement::eString) {
        node.ResetType(DTDElement::eUnknown);
        node.SetType(DTDElement::eEnum);
    } else if (node.GetType() == DTDElement::eUnknown) {
        isknown = false;
    } else {
        ParseError("enum restriction not supported", "string or integer type");
    }
    bool isint = node.GetType() == DTDElement::eIntEnum;
    TToken tok;
    for (tok = GetNextToken(); tok != K_END_ARRAY; tok = GetNextToken()) {
        if (tok != T_SYMBOL) {
            if (!isknown) {
                isknown = true;
                if (tok == T_NUMBER) {
                    node.SetType(DTDElement::eIntEnum);
                    isint = true;
                } else if (tok == K_VALUE) {
                    node.SetType(DTDElement::eEnum);
                    isint = false;
                }
            }
            if ((isint && tok == T_NUMBER) || (!isint && tok == K_VALUE)) {
                node.AddContent(Value());
            } else {
                ParseError("enum restriction not supported", isint? "integer" : "string");
            }
        }
    }
}

void JSDParser::ParseOneOf(DTDElement& node)
{
    string node_id_base = NStr::Join(m_URI,"/");
    DTDElement::EType type = node.GetType();
    vector<DTDElement> contents;
    TToken tok;
    for (tok = GetNextToken(); tok != K_END_ARRAY; tok = GetNextToken()) {
        if (tok == K_BEGIN_OBJECT) {
            contents.push_back(DTDElement());
            DTDElement& item = contents.back();
            item.SetType(type);
            item.SetEmbedded();
            ParseNode(item);
        }
    }
    // now merge
    bool hasnil = false;
    DTDElement::EType nexttype = DTDElement::eUnknown;
    for(const DTDElement& c : contents) {
        const DTDElement& e = (c.GetType() == DTDElement::eAlias) ? m_MapElement[c.GetTypeName()] : c;
        if (e.GetType() == DTDElement::eEmpty && !e.IsNamed()) {
            hasnil = true;
        }
        if (e.GetType() != DTDElement::eEmpty && type == DTDElement::eUnknown) {
            if (nexttype == DTDElement::eUnknown) {
                nexttype = e.GetType();
            } else {
                nexttype = DTDElement::eChoice;
            }
        }
    }
    if (nexttype == DTDElement::eUnknown) {
        nexttype = DTDElement::eChoice;
    }
    if (type == DTDElement::eUnknown) {
        node.SetType(nexttype);
    } else if (type == DTDElement::eSequence) {
        node.ResetType(DTDElement::eUnknown);
        node.SetType(nexttype);
    }
    type = node.GetType();
    if (type == DTDElement::eSequence || type == DTDElement::eChoice) {
        int i = 0;
        for(DTDElement& c : contents) {
            string item_id = node_id_base + "/" + NStr::NumericToString(i++);
            if (c.GetType() == DTDElement::eAlias) {
                item_id = c.GetTypeName();
            } else {
                if (!c.IsNamed()) {
                    c.SetName(item_id);
                }
                m_MapElement[item_id] = c;
            }
    	    AddElementContent(node,item_id);
        }
        FixEmbeddedNames(node);
    }
    node.SetNillable(hasnil);
}

void JSDParser::ParseAnyOf(DTDElement& node)
{
    ParseOneOf(node);
//    node.SetOccurrence(DTDElement::eZeroOrMore);
}

void JSDParser::ParseAllOf(DTDElement& node)
{
    string node_id_base = NStr::Join(m_URI,"/");
    DTDElement::EType type = node.GetType();
    vector<DTDElement> contents;
    TToken tok;
    for (tok = GetNextToken(); tok != K_END_ARRAY; tok = GetNextToken()) {
        if (tok == K_BEGIN_OBJECT) {
            contents.push_back(DTDElement());
            DTDElement& item = contents.back();
            item.SetType(type);
            item.SetEmbedded();
            ParseNode(item);
        }
    }
    for(const DTDElement& c : contents) {
        const DTDElement& e = (c.GetType() == DTDElement::eAlias) ? m_MapElement[c.GetTypeName()] : c;
        node.SetType(e.GetType());
        string item_id;
        if (c.GetType() != DTDElement::eAlias && e.IsNamed()) {
    	    item_id = e.GetName();
            AddElementContent(node,item_id);
        } else {
            for(const string& ref : e.GetContent()) {
                item_id = ref;
    	        AddElementContent(node,item_id);
            }
        }
    }
}
void JSDParser::SkipUnknown(TToken tokend)
{
    TToken tok;
    for (tok = GetNextToken(); tok != tokend; tok = GetNextToken()) {
        if (tok == K_BEGIN_ARRAY) {
            SkipUnknown(K_END_ARRAY);
        } else if (tok == K_BEGIN_OBJECT) {
            SkipUnknown(K_END_OBJECT);
        }
    }
}

const string& JSDParser::Value(void)
{
    return m_StringValue;
}

TToken JSDParser::GetNextToken(void)
{
    TToken tok = Next();
    if (tok == T_STRING) {
        m_StringValue = NextToken().GetText();
        Consume();
        tok = Next();
        if (tok != K_KEY) {
            return K_VALUE;
        }
    } else {
        m_StringValue = NextToken().GetText();
    }
    Consume();
    return tok;
}

END_NCBI_SCOPE
