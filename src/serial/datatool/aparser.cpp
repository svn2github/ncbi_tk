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
*   Abstract parser class
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  2000/02/01 21:47:52  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.4  1999/11/15 19:36:12  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <serial/tool/aparser.hpp>

USING_NCBI_SCOPE;

AbstractParser::AbstractParser(AbstractLexer& lexer)
    : m_Lexer(lexer)
{
}

AbstractParser::~AbstractParser(void)
{
}

void AbstractParser::ParseError(const char* error, const char* expected,
                                const AbstractToken& token)
{
    throw runtime_error(NStr::IntToString(token.GetLine()) +
                        ": Parse error: " + error + ": " +
                        expected + " expected");
}

string AbstractParser::Location(void) const
{
    const AbstractToken& token = NextToken();
    return NStr::IntToString(token.GetLine()) + ':';
}
