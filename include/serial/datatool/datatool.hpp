#ifndef DATATOOL__HPP
#define DATATOOL__HPP

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
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2000/11/27 18:19:30  vasilche
* Datatool now conforms CNcbiApplication requirements.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <serial/datatool/generate.hpp>
#include <list>

BEGIN_NCBI_SCOPE

class CArgs;
class CFileSet;

class CDataTool : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);

private:
    bool ProcessModules(void);
    bool ProcessData(void);
    bool GenerateCode(void);

    void LoadDefinitions(CFileSet& fileSet,
                         const string& modulesDir,
                         const string& names);

    CCodeGenerator generator;
};

//#include <serial/datatool/datatool.inl>

END_NCBI_SCOPE

#endif  /* DATATOOL__HPP */
