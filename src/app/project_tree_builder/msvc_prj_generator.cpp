#include <app/project_tree_builder/msvc_prj_generator.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>
#include <app/project_tree_builder/msvc_project_context.hpp>

#include "Platforms.hpp"
#include "Platform.hpp"
#include "Configurations.hpp"
#include "Configuration.hpp"
#include "Tool.hpp"
#include "File.hpp"
#include "Files.hpp"
#include "Filter.hpp"

#include <app/project_tree_builder/msvc_prj_utils.hpp>
#include <app/project_tree_builder/msvc_prj_defines.hpp>

#include <algorithm>


BEGIN_NCBI_SCOPE


static void s_CollectRelPathes(const string&       path_from,
                               const list<string>& abs_dirs,
                               const list<string>& file_masks,
                               list<string>*       rel_pathes);


CMsvcProjectGenerator::CMsvcProjectGenerator(const list<string>& configs)
{
    m_Configs = configs;
}


CMsvcProjectGenerator::~CMsvcProjectGenerator()
{
}


bool CMsvcProjectGenerator::Generate(const CProjItem& prj)
{
    CVisualStudioProject xmlprj;
    CMsvcPrjProjectContext project_context(prj);
    
    {{
        //Attributes:
        xmlprj.SetAttlist().SetProjectType (MSVC_PROJECT_PROJECT_TYPE);
        xmlprj.SetAttlist().SetVersion     (MSVC_PROJECT_VERSION);
        xmlprj.SetAttlist().SetName        (project_context.ProjectName());
        xmlprj.SetAttlist().SetKeyword     (MSVC_PROJECT_KEYWORD_WIN32);
    }}
    
    {{
        //Platforms
         CRef<CPlatform> platform(new CPlatform(""));
         platform->SetAttlist().SetName(MSVC_PROJECT_PLATFORM);
         xmlprj.SetPlatforms().SetPlatform().push_back(platform);
    }}

    ITERATE(list<string>, p , m_Configs) {

        const string& config = *p;

        //contexts:
        
        CMsvcPrjGeneralContext general_context(config, project_context);

        //MSVC Tools
        CMsvcTools msvc_tool(general_context, project_context);

        CRef<CConfiguration> conf(new CConfiguration());

#define BIND_TOOLS(tool, msvctool, X) tool->SetAttlist().Set##X(msvctool->##X())

        {{
            //Configuration

            //Name
            BIND_TOOLS(conf, msvc_tool.Configuration(), Name);
            
            //OutputDirectory
            BIND_TOOLS(conf, msvc_tool.Configuration(), OutputDirectory);

            //IntermediateDirectory
            BIND_TOOLS(conf, msvc_tool.Configuration(), IntermediateDirectory);

            //ConfigurationType
            BIND_TOOLS(conf, msvc_tool.Configuration(), ConfigurationType);

            //CharacterSet
            BIND_TOOLS(conf, msvc_tool.Configuration(), CharacterSet);
        }}

        
        {{
            //Compiler
            CRef<CTool> tool(new CTool("")); 

            //Name
            BIND_TOOLS(tool, msvc_tool.Compiler(), Name);
            
            //Optimization
            BIND_TOOLS(tool, msvc_tool.Compiler(), Optimization);

            //AdditionalIncludeDirectories
            {{
                BIND_TOOLS(tool, 
                           msvc_tool.Compiler(), 
                           AdditionalIncludeDirectories);

            }}
            
            //PreprocessorDefinitions
            BIND_TOOLS(tool, msvc_tool.Compiler(), PreprocessorDefinitions);
            
            //MinimalRebuild
            BIND_TOOLS(tool, msvc_tool.Compiler(), MinimalRebuild);

            //BasicRuntimeChecks
            BIND_TOOLS(tool, msvc_tool.Compiler(), BasicRuntimeChecks);

            //RuntimeLibrary
            BIND_TOOLS(tool, msvc_tool.Compiler(), RuntimeLibrary);

            //RuntimeTypeInfo
            BIND_TOOLS(tool, msvc_tool.Compiler(), RuntimeTypeInfo);

            //UsePrecompiledHeader
            BIND_TOOLS(tool, msvc_tool.Compiler(), UsePrecompiledHeader);

            //WarningLevel
            BIND_TOOLS(tool, msvc_tool.Compiler(), WarningLevel);

            //Detect64BitPortabilityProblems
            BIND_TOOLS(tool, 
                       msvc_tool.Compiler(), 
                       Detect64BitPortabilityProblems);

            //DebugInformationFormat
            BIND_TOOLS(tool, msvc_tool.Compiler(), DebugInformationFormat);

            //CompileAs
            BIND_TOOLS(tool, msvc_tool.Compiler(), CompileAs);
            
            //InlineFunctionExpansion
            BIND_TOOLS(tool, msvc_tool.Compiler(), InlineFunctionExpansion);

            //OmitFramePointers
            BIND_TOOLS(tool, msvc_tool.Compiler(), OmitFramePointers);

            //StringPooling
            BIND_TOOLS(tool, msvc_tool.Compiler(), StringPooling);

            //EnableFunctionLevelLinking
            BIND_TOOLS(tool, msvc_tool.Compiler(), EnableFunctionLevelLinking);

            conf->SetTool().push_back(tool);
        }}

        {{
            //Linker
            CRef<CTool> tool(new CTool(""));

            //Name
            BIND_TOOLS(tool, msvc_tool.Linker(), Name);

            //AdditionalOptions
            BIND_TOOLS(tool, msvc_tool.Linker(), AdditionalOptions);
            
            //OutputFile
            BIND_TOOLS(tool, msvc_tool.Linker(), OutputFile);

            //LinkIncremental
            BIND_TOOLS(tool, msvc_tool.Linker(), LinkIncremental);
            
            //GenerateDebugInformation
            BIND_TOOLS(tool, msvc_tool.Linker(), GenerateDebugInformation);
            
            //ProgramDatabaseFile
            BIND_TOOLS(tool, msvc_tool.Linker(), ProgramDatabaseFile);

            //SubSystem
            BIND_TOOLS(tool, msvc_tool.Linker(), SubSystem);

            //ImportLibrary
            BIND_TOOLS(tool, msvc_tool.Linker(), ImportLibrary);

            //TargetMachine
            BIND_TOOLS(tool, msvc_tool.Linker(), TargetMachine);

            //OptimizeReferences
            BIND_TOOLS(tool, msvc_tool.Linker(), OptimizeReferences);

            //EnableCOMDATFolding
            BIND_TOOLS(tool, msvc_tool.Linker(), EnableCOMDATFolding);

            conf->SetTool().push_back(tool);
        }}

        {{
            //Librarian
            CRef<CTool> tool(new CTool(""));

            //Name
            BIND_TOOLS(tool, msvc_tool.Librarian(), Name);

            //AdditionalOptions
            BIND_TOOLS(tool, msvc_tool.Librarian(), AdditionalOptions);

            //OutputFile
            BIND_TOOLS(tool, msvc_tool.Librarian(), OutputFile);

            //IgnoreAllDefaultLibraries
            BIND_TOOLS(tool, msvc_tool.Librarian(), IgnoreAllDefaultLibraries);

            //IgnoreDefaultLibraryNames
            BIND_TOOLS(tool, msvc_tool.Librarian(), IgnoreDefaultLibraryNames);

            conf->SetTool().push_back(tool);
        }}

        {{
            //CustomBuildTool
            CRef<CTool> tool(new CTool(""));

            //Name
            BIND_TOOLS(tool, msvc_tool.CustomBuid(), Name);

            conf->SetTool().push_back(tool);
        }}
        {{
            //MIDL
            CRef<CTool> tool(new CTool(""));

            //Name
            BIND_TOOLS(tool, msvc_tool.MIDL(), Name);

            conf->SetTool().push_back(tool);
        }}
        {{
            //PostBuildEvent
            CRef<CTool> tool(new CTool(""));

            //Name
            BIND_TOOLS(tool, msvc_tool.PostBuildEvent(), Name);

            conf->SetTool().push_back(tool);
        }}
        {{
            //PreBuildEvent
            CRef<CTool> tool(new CTool(""));

            //Name
            BIND_TOOLS(tool, msvc_tool.PreBuildEvent(), Name);

            conf->SetTool().push_back(tool);
        }}
        {{
            //PreLinkEvent
            CRef<CTool> tool(new CTool(""));

            //Name
            BIND_TOOLS(tool, msvc_tool.PreLinkEvent(), Name);

            conf->SetTool().push_back(tool);
        }}
        {{
            //ResourceCompiler
            CRef<CTool> tool(new CTool(""));

            //Name
            BIND_TOOLS(tool, msvc_tool.ResourceCompiler(), Name);
             
            //PreprocessorDefinitions
            BIND_TOOLS(tool, 
                       msvc_tool.ResourceCompiler(), 
                       PreprocessorDefinitions);
            
            //Culture
            BIND_TOOLS(tool, msvc_tool.ResourceCompiler(), Culture);

            //AdditionalIncludeDirectories
            BIND_TOOLS(tool, 
                       msvc_tool.ResourceCompiler(), 
                       AdditionalIncludeDirectories);

            conf->SetTool().push_back(tool);
        }}

        {{
            //WebServiceProxyGenerator
            CRef<CTool> tool(new CTool(""));

            //Name
            BIND_TOOLS(tool, msvc_tool.WebServiceProxyGenerator(), Name);

            conf->SetTool().push_back(tool);
        }}
        {{
            //XMLDataGenerator
            CRef<CTool> tool(new CTool(""));

            //Name
            BIND_TOOLS(tool, msvc_tool.XMLDataGenerator(), Name);

            conf->SetTool().push_back(tool);
        }}
        {{
            //ManagedWrapperGenerator
            CRef<CTool> tool(new CTool(""));

            //Name
            BIND_TOOLS(tool, msvc_tool.ManagedWrapperGenerator(), Name);

            conf->SetTool().push_back(tool);
        }}
        {{
            //AuxiliaryManagedWrapperGenerator
            CRef<CTool> tool(new CTool(""));

            //Name
            BIND_TOOLS(tool, 
                       msvc_tool.AuxiliaryManagedWrapperGenerator(),
                       Name);

            conf->SetTool().push_back(tool);
        }}

        xmlprj.SetConfigurations().SetConfiguration().push_back(conf);
    }
    {{
        //References
        xmlprj.SetReferences("");
    }}

    {{
        //Files - source files
        CRef<CFilter> filter(new CFilter());
        filter->SetAttlist().SetName("Source Files");
        filter->SetAttlist().SetFilter
            ("cpp;c;cxx;def;odl;idl;hpj;bat;asm;asmx");

        list<string> rel_pathes;
        CollectSources(prj,project_context, &rel_pathes); 
        ITERATE(list<string>, p, rel_pathes) {
            //Include collected source files
            CRef< CFFile > file(new CFFile());
            file->SetAttlist().SetRelativePath(*p);

            CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
            ce->SetFile(*file);
            filter->SetFF().SetFF().push_back(ce);
        }
        xmlprj.SetFiles().SetFilter().push_back(filter);
    }}
    {{
        //Files - header files
        CRef<CFilter> filter(new CFilter());
        filter->SetAttlist().SetName("Header Files");
        filter->SetAttlist().SetFilter("h;hpp;hxx;hm;inc;xsd");

        list<string> rel_pathes;
        CollectHeaders(prj, project_context, &rel_pathes);
        ITERATE(list<string>, p, rel_pathes) {
            //Include collected header files
            CRef<CFFile> file(new CFFile());
            file->SetAttlist().SetRelativePath(*p);

            CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
            ce->SetFile(*file);
            filter->SetFF().SetFF().push_back(ce);
        }
        xmlprj.SetFiles().SetFilter().push_back(filter);
    }}
    {{
        //Files - inline files
        CRef<CFilter> filter(new CFilter());
        filter->SetAttlist().SetName("Inline Files");
        filter->SetAttlist().SetFilter("inl");

        list<string> rel_pathes;
        CollectInlines(prj, project_context, &rel_pathes);
        ITERATE(list<string>, p, rel_pathes) {
            //Include collected inline files
            CRef< CFFile > file(new CFFile());
            file->SetAttlist().SetRelativePath(*p);

            CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
            ce->SetFile(*file);
            filter->SetFF().SetFF().push_back(ce);
        }
        xmlprj.SetFiles().SetFilter().push_back(filter);
    }}
    {{
        //Resource Files - header files - empty
        CRef<CFilter> filter(new CFilter());
        filter->SetAttlist().SetName("Resource Files");
        filter->SetAttlist().SetFilter
            ("rc;ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe;resx");

        xmlprj.SetFiles().SetFilter().push_back(filter);
    }}

    {{
        //Globals
        xmlprj.SetGlobals("");
    }}


    string project_path = CDirEntry::ConcatPath(project_context.ProjectDir(), 
                                                project_context.ProjectName());
    project_path += MSVC_PROJECT_FILE_EXT;

    SaveToXmlFile(project_path, xmlprj);

    return true;

}


void 
CMsvcProjectGenerator::CollectSources (const CProjItem&              project,
                                       const CMsvcPrjProjectContext& context,
                                       list<string>*                 rel_pathes)
{
    rel_pathes->clear();

    ITERATE(list<string>, p, project.m_Sources) {

        const string& abs_path = *p; // whithout ext.

        string ext = SourceFileExt(abs_path);
        if ( !ext.empty() ) {
            // add ext to file
            string source_file_abs_path = abs_path + ext;
            rel_pathes->push_back(
                CDirEntry::CreateRelativePath(context.ProjectDir(), 
                                              source_file_abs_path));
        } else {
            LOG_POST(">>>>> Can not resolve/find source file : " + abs_path);
        }
    }
}



void 
CMsvcProjectGenerator::CollectHeaders(const CProjItem&              project,
                                      const CMsvcPrjProjectContext& context,
                                      list<string>*                 rel_pathes)
{
    rel_pathes->clear();

    // .h and .hpp files may be in include or source dirs:
    list<string> abs_dirs(context.IncludeDirsAbs());
    copy(context.SourcesDirsAbs().begin(), 
         context.SourcesDirsAbs().end(), 
         back_inserter(abs_dirs));

    //collect *.h and *.hpp files
    list<string> exts;
    exts.push_back(".h");
    exts.push_back(".hpp");
    s_CollectRelPathes(context.ProjectDir(), abs_dirs, exts, rel_pathes);
}


void 
CMsvcProjectGenerator::CollectInlines(const CProjItem&              project,
                                      const CMsvcPrjProjectContext& context,
                                      list<string>*                 rel_pathes)
{
    rel_pathes->clear();

    // .inl files may be in include or source dirs:
    list<string> abs_dirs(context.IncludeDirsAbs());
    copy(context.SourcesDirsAbs().begin(), 
         context.SourcesDirsAbs().end(), 
         back_inserter(abs_dirs));

    //collect *.inl files
    list<string> exts(1, ".inl");
    s_CollectRelPathes(context.ProjectDir(), abs_dirs, exts, rel_pathes);
}


// Collect all files from specified dirs having specified exts
static void s_CollectRelPathes(const string&        path_from,
                               const list<string>&  abs_dirs,
                               const list<string>&  file_masks,
                               list<string>*        rel_pathes)
{
    rel_pathes->clear();

    list<string> pathes;
    ITERATE(list<string>, p, file_masks) {
        ITERATE(list<string>, n, abs_dirs) {
            CDir dir(*n);
            if ( !dir.Exists() )
                continue;
            CDir::TEntries contents = dir.GetEntries("*" + *p);
            ITERATE(CDir::TEntries, i, contents) {
                if ( (*i)->IsFile() ) {
                    string path  = (*i)->GetPath();
                    pathes.push_back(path);
                }
            }
        }
    }

    ITERATE(list<string>, p, pathes)
        rel_pathes->push_back(CDirEntry::CreateRelativePath(path_from, *p));
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2004/01/22 17:57:54  gorelenk
 * first version
 *
 * ===========================================================================
 */
