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
 * Author:  Viatcheslav Gorelenkov
 *
 */

#include <app/project_tree_builder/stl_msvc_usage.hpp>
#include <app/project_tree_builder/msvc_sln_generator.hpp>
#include <app/project_tree_builder/msvc_prj_utils.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbistr.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>
#include <app/project_tree_builder/msvc_prj_defines.hpp>
#include <app/project_tree_builder/msvc_makefile.hpp>

BEGIN_NCBI_SCOPE

//-----------------------------------------------------------------------------
CMsvcSolutionGenerator::CMsvcSolutionGenerator
                                            (const list<SConfigInfo>& configs)
    :m_Configs(configs)
{
}


CMsvcSolutionGenerator::~CMsvcSolutionGenerator(void)
{
}


void 
CMsvcSolutionGenerator::AddProject(const CProjItem& project)
{
    m_Projects[project.m_ID] = CPrjContext(project);
}


void 
CMsvcSolutionGenerator::AddMasterProject(const string& base_name)
{
    m_MasterProject.first  = base_name;
    m_MasterProject.second = GenerateSlnGUID();
}


void 
CMsvcSolutionGenerator::AddConfigureProject(const string& base_name)
{
    m_ConfigureProject.first  = base_name;
    m_ConfigureProject.second = GenerateSlnGUID();
}


bool 
CMsvcSolutionGenerator::IsSetMasterProject(void) const
{
    return !m_MasterProject.first.empty() && !m_MasterProject.second.empty();
}


bool 
CMsvcSolutionGenerator::IsSetConfigureProject(void) const
{
    return !m_ConfigureProject.first.empty() && 
           !m_ConfigureProject.second.empty();
}

void 
CMsvcSolutionGenerator::SaveSolution(const string& file_path)
{
    CDirEntry::SplitPath(file_path, &m_SolutionDir);

    // Create dir for output sln file
    CDir(m_SolutionDir).CreatePath();

    CNcbiOfstream  ofs(file_path.c_str(), IOS_BASE::out | IOS_BASE::trunc);
    if ( !ofs )
        NCBI_THROW(CProjBulderAppException, eFileCreation, file_path);

    //Start sln file
    ofs << MSVC_SOLUTION_HEADER_LINE << endl;

    ITERATE(TProjects, p, m_Projects) {
        
        WriteProjectAndSection(ofs, p->second);
    }
    if ( IsSetMasterProject() )
        WriteUtilityProject(m_MasterProject,    ofs);
    if ( IsSetConfigureProject() )
        WriteUtilityProject(m_ConfigureProject, ofs);

    //Start "Global" section
    ofs << "Global" << endl;
	
    //Write all configurations
    ofs << '\t' << "GlobalSection(SolutionConfiguration) = preSolution" << endl;
    ITERATE(list<SConfigInfo>, p, m_Configs) {
        const string& config = (*p).m_Name;
        ofs << '\t' << '\t' << config << " = " << config << endl;
    }
    ofs << '\t' << "EndGlobalSection" << endl;
    
    ofs << '\t' 
        << "GlobalSection(ProjectConfiguration) = postSolution" 
        << endl;

    ITERATE(TProjects, p, m_Projects) {
        
        WriteProjectConfigurations(ofs, p->second);
    }
    if ( IsSetMasterProject() )
        WriteUtilityProjectConfiguration(m_MasterProject,   ofs);
    if ( IsSetConfigureProject() )
        WriteUtilityProjectConfiguration(m_ConfigureProject,ofs);

    ofs << '\t' << "EndGlobalSection" << endl;

    //meanless stuff
    ofs << '\t' 
        << "GlobalSection(ExtensibilityGlobals) = postSolution" << endl;
	ofs << '\t' << "EndGlobalSection" << endl;
	ofs << '\t' << "GlobalSection(ExtensibilityAddIns) = postSolution" << endl;
	ofs << '\t' << "EndGlobalSection" << endl;
   
    //End of global section
    ofs << "EndGlobal" << endl;
}


//------------------------------------------------------------------------------
CMsvcSolutionGenerator::CPrjContext::CPrjContext(void)
{
    Clear();
}


CMsvcSolutionGenerator::CPrjContext::CPrjContext(const CPrjContext& context)
{
    SetFrom(context);
}


CMsvcSolutionGenerator::CPrjContext::CPrjContext(const CProjItem& project)
    :m_Project(project)
{
    m_GUID = GenerateSlnGUID();

    CMsvcPrjProjectContext project_context(project);
    m_ProjectName = project_context.ProjectName();
    m_ProjectPath = CDirEntry::ConcatPath(project_context.ProjectDir(),
                                          project_context.ProjectName());
    m_ProjectPath += MSVC_PROJECT_FILE_EXT;
}


CMsvcSolutionGenerator::CPrjContext& 
CMsvcSolutionGenerator::CPrjContext::operator= (const CPrjContext& context)
{
    if (this != &context) {
        Clear();
        SetFrom(context);
    }
    return *this;
}


CMsvcSolutionGenerator::CPrjContext::~CPrjContext(void)
{
    Clear();
}


void 
CMsvcSolutionGenerator::CPrjContext::Clear(void)
{
    //TODO
}


void 
CMsvcSolutionGenerator::CPrjContext::SetFrom
    (const CPrjContext& project_context)
{
    m_Project     = project_context.m_Project;

    m_GUID        = project_context.m_GUID;
    m_ProjectName = project_context.m_ProjectName;
    m_ProjectPath = project_context.m_ProjectPath;
}


void 
CMsvcSolutionGenerator::WriteProjectAndSection(CNcbiOfstream&     ofs, 
                                               const CPrjContext& project)
{
    ofs << "Project(\"" 
        << MSVC_SOLUTION_ROOT_GUID 
        << "\") = \"" 
        << project.m_ProjectName 
        << "\", \"";

    ofs << CDirEntry::CreateRelativePath(m_SolutionDir, project.m_ProjectPath)
        << "\", \"";

    ofs << project.m_GUID 
        << "\"" 
        << endl;

    ofs << '\t' << "ProjectSection(ProjectDependencies) = postProject" << endl;

    ITERATE(list<string>, p, project.m_Project.m_Depends) {

        const string& id = *p;

        TProjects::const_iterator n = m_Projects.find(id);
        if (n != m_Projects.end()) {

            const CPrjContext& prj_i = n->second;

            ofs << '\t' << '\t' 
                << prj_i.m_GUID 
                << " = " 
                << prj_i.m_GUID << endl;
        } else {

            LOG_POST(Warning << "Project: " + 
                      project.m_ProjectName + " is dependend of " + id + 
                      ". But no such project");
        }
    }
    ofs << '\t' << "EndProjectSection" << endl;
    ofs << "EndProject" << endl;
}


void 
CMsvcSolutionGenerator::WriteUtilityProject(const TUtilityProject& project,
                                            CNcbiOfstream& ofs)
{
    ofs << "Project(\"" 
        << MSVC_SOLUTION_ROOT_GUID
        << "\") = \"" 
        << project.first //basename
        << "\", \"";

    ofs << project.first + MSVC_PROJECT_FILE_EXT 
        << "\", \"";

    ofs << project.second //m_GUID 
        << "\"" 
        << endl;

    ofs << '\t' << "ProjectSection(ProjectDependencies) = postProject" << endl;

    ofs << '\t' << "EndProjectSection" << endl;
    ofs << "EndProject" << endl;
}



void 
CMsvcSolutionGenerator::WriteProjectConfigurations(CNcbiOfstream&     ofs, 
                                                   const CPrjContext& project)
{
    ITERATE(list<SConfigInfo>, p, m_Configs) {

        const string& config = (*p).m_Name;
        ofs << '\t' 
            << '\t' 
            << project.m_GUID 
            << '.' 
            << config 
            << ".ActiveCfg = " 
            << ConfigName(config)
            << endl;

        ofs << '\t' 
            << '\t' 
            << project.m_GUID 
            << '.' 
            << config 
            << ".Build.0 = " 
            << ConfigName(config)
            << endl;
    }
}


void 
CMsvcSolutionGenerator::WriteUtilityProjectConfiguration(const TUtilityProject& project,
                                                        CNcbiOfstream& ofs)
{
    ITERATE(list<SConfigInfo>, p, m_Configs) {

        const string& config = (*p).m_Name;
        ofs << '\t' 
            << '\t' 
            << project.second // project.m_GUID 
            << '.' 
            << config 
            << ".ActiveCfg = " 
            << ConfigName(config)
            << endl;

        ofs << '\t' 
            << '\t' 
            << project.second // project.m_GUID 
            << '.' 
            << config 
            << ".Build.0 = " 
            << ConfigName(config)
            << endl;
    }
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.9  2004/02/12 16:27:58  gorelenk
 * Changed generation of command line for datatool.
 *
 * Revision 1.8  2004/02/10 18:20:16  gorelenk
 * Changed LOG_POST messages.
 *
 * Revision 1.7  2004/02/05 00:00:48  gorelenk
 * Changed log messages generation.
 *
 * Revision 1.6  2004/01/28 17:55:49  gorelenk
 * += For msvc makefile support of :
 *                 Requires tag, ExcludeProject tag,
 *                 AddToProject section (SourceFiles and IncludeDirs),
 *                 CustomBuild section.
 * += For support of user local site.
 *
 * Revision 1.5  2004/01/26 19:27:29  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * Revision 1.4  2004/01/22 17:57:54  gorelenk
 * first version
 *
 * ===========================================================================
 */
