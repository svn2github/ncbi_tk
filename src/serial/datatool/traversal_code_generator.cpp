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
* Author: Michael Kornbluh
*
* File Description:
*   generates the code from the given specification file.
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbitime.hpp>
#include <serial/error_codes.hpp>
#include "traversal_merger.hpp"
#include "traversal_code_generator.hpp"
#include "traversal_attach_user_funcs_callback.hpp"

#include "blocktype.hpp"
#include "enumtype.hpp"
#include "module.hpp"
#include "moduleset.hpp"
#include "reftype.hpp"
#include "type.hpp"
#include "traversal_spec_file_parser.hpp"
#include "unitype.hpp"
#include "filecode.hpp"

#define NCBI_USE_ERRCODE_X   Serial_DataTool

BEGIN_NCBI_SCOPE

namespace {
    const char *kAutoGenerationNotice = 
        "/// This file was generated by application DATATOOL\n"
        "///\n"
        "/// ATTENTION:\n"
        "///   Don't edit or commit this file into SVN as this file will\n"
        "///   be overridden (by DATATOOL) without warning!\n";
}

// Use with CTraversalNode's DepthFirst func to pretty-print the descendents of a node
// with indenting.
class CPrintTraversalNodeCallback : public CTraversalNode::CDepthFirstCallback {
public:
    CPrintTraversalNodeCallback( CNcbiOstream& ostream ) 
        : m_Ostream(ostream) { }

    bool Call( CTraversalNode& node, const CTraversalNode::TNodeVec &node_path, ECallType is_cyclic ) {

        // print indentation
        const int depth = node_path.size() - 1;
        m_Ostream << string( depth * 2, ' ' );

        if( node.GetType() == CTraversalNode::eType_Reference ) {
            m_Ostream << "--" << endl;
        } else {

            // mark as cyclic if it is
            if( is_cyclic == eCallType_Cyclic ) {
                m_Ostream << "(CYCLIC)";
            }

            m_Ostream << node.GetTypeName() << ":" << node.GetVarName() << ":" << node.GetTypeAsString() << ":" << node.GetInputClassName() << endl;
        }

        // only continue if we're not cyclic (to avoid infinite loops)
        return ( is_cyclic == eCallType_NonCyclic );
    }

private:
    CNcbiOstream& m_Ostream;
};

// Use with CTraversalNode's DepthFirst func to output all the include
// lines needed for the generated code.
// It does not involve generating the includes needed by custom stuff
// from "member" clauses, etc.
class CGenerateIncludesCallback : public CTraversalNode::CDepthFirstCallback {
public:
    CGenerateIncludesCallback( CNcbiOstream& ostream ) 
        : m_Ostream(ostream) { }

    bool Call( CTraversalNode& node, const CTraversalNode::TNodeVec &node_path, ECallType is_cyclic ) {

        // We can't generate an include for an unknown type
        if( node.IsTemplate() ) {
            return true;
        }

        // we don't need to include anything for basic types like "int".
        // For enums, etc. we assume the parent class is included elsewhere
        if( node.GetType() == CTraversalNode::eType_Primitive ||
            node.GetType() == CTraversalNode::eType_Enum ||
            node.GetType() == CTraversalNode::eType_Null ) {
            return true;
        }

        // references to primitive types are not included, since they're generated
        // elsewhere
        if( node.GetType() == CTraversalNode::eType_Reference ) {
            _ASSERT( ! node.GetCallees().empty() );
            switch( (*node.GetCallees().begin())->GetType() ) {
            case CTraversalNode::eType_Primitive:
            case CTraversalNode::eType_Enum:
            case CTraversalNode::eType_Null:
                return true;
                break;
            default:
                // keep going
                break;
            }
        }

        // get class name
        string class_name = node.GetInputClassName();

        // double colon indicates an inner class, and we assume the
        // parent class is included elsewhere
        string::size_type first_double_colon = class_name.find("::");
        if( first_double_colon != string::npos ) {
            return true;
        }

        // chop off leading prefix since it's not part of the include file
        if( class_name[0] == 'C' || class_name[0] == 'E' ) {
            class_name = class_name.substr(1);
        }
        
        // figure out the file name, and include it if we haven't already
        string file_name = node.GetIncludePath() + "/" + class_name + ".hpp";
        if( m_FilesWeHaveIncluded.find(file_name) == m_FilesWeHaveIncluded.end() ) {
            // haven't included this
            m_Ostream << "#include <objects/" << file_name << ">" << endl;
            m_FilesWeHaveIncluded.insert( file_name );
        }

        return true;
    }

private:
    CNcbiOstream& m_Ostream;
    set<string> m_FilesWeHaveIncluded;
};

// Use with CTraversalNode's DepthFirst func to output the code
// generated by each CTraversalNode
class CGenerateCodeCallback : public CTraversalNode::CDepthFirstCallback {
public:
    CGenerateCodeCallback( const string &class_name, CNcbiOstream& ostream, CTraversalNode::EGenerateMode generate_mode ) 
        : m_Ostream(ostream), m_ClassName(class_name), m_GenerateMode(generate_mode) { }

    bool Call( CTraversalNode& node, const CTraversalNode::TNodeVec &node_path, ECallType is_cyclic ) {

        // skip functions we've already output
        if( m_NodesSeen.find( node.Ref() ) != m_NodesSeen.end() ) {
            return true;
        }

        // prototypes get indented
        if( m_GenerateMode == CTraversalNode::eGenerateMode_Prototypes ) {
            m_Ostream << "  ";
        }

        // generate code and mark that we've done this node
        node.GenerateCode( m_ClassName, m_Ostream, m_GenerateMode );
        m_NodesSeen.insert( node.Ref() );
        return true;
    }

private:
    CNcbiOstream& m_Ostream;
    CTraversalNode::TNodeSet m_NodesSeen;
    const string m_ClassName;
    const CTraversalNode::EGenerateMode m_GenerateMode;
};

// Use with CTraversalNode's DepthFirst func to load all the 
// ancestors/descendents of a node into a set
class CAddToNodeSetCallback : public CTraversalNode::CDepthFirstCallback {
public:
    CAddToNodeSetCallback( CTraversalNode::TNodeSet &set_to_add_to )
        : m_SetToAddTo(set_to_add_to) { }

    bool Call( CTraversalNode& node, const CTraversalNode::TNodeVec &node_path, ECallType is_cyclic ) {
        // we've already seen this node, so don't traverse its "children" again
        if( m_SetToAddTo.find( node.Ref() ) != m_SetToAddTo.end() ) {
            return false;
        }

        m_SetToAddTo.insert( node.Ref() );
        return true;
    }

private:
    CTraversalNode::TNodeSet &m_SetToAddTo;
};

// Some functions store their argument every time they're called.
// This generates the private member variable declaration at the bottom of
// the class prototype.
class CGenerateStoredArgVariablesCallback : public CTraversalNode::CDepthFirstCallback {
public:
    CGenerateStoredArgVariablesCallback( CNcbiOstream& ostream ) 
        : m_Ostream(ostream) { }

    bool Call( CTraversalNode& node, const CTraversalNode::TNodeVec &node_path, ECallType is_cyclic ) {
        if( node.GetDoStoreArg() ) {
            m_Ostream << "  " << node.GetInputClassName() << "* " << node.GetStoredArgVariable() << ";" << endl;
        }
        return true;
    }

private:
    CNcbiOstream& m_Ostream;
};

// Some functions store their argument every time they're called.
// This generates the initializer in the constructor.
// It always initializes to NULL.
class CGenerateStoredArgInitializerCallback : public CTraversalNode::CDepthFirstCallback {
public:
    CGenerateStoredArgInitializerCallback( CNcbiOstream& ostream ) 
        : m_Ostream(ostream) { }

    bool Call( CTraversalNode& node, const CTraversalNode::TNodeVec &node_path, ECallType is_cyclic ) {
        if( node.GetDoStoreArg() ) {
            m_Ostream << "    " << node.GetStoredArgVariable() << "(NULL)," << endl;
        }
        return true;
    }

private:
    CNcbiOstream& m_Ostream;
};

CTraversalCodeGenerator::CTraversalCodeGenerator( 
    CFileSet& mainModules, 
    CNcbiIstream& traversal_spec_file )
{
    TNameToASNMap nameToASNMap;
    // Need to build this map because mainModules.ResolveInAnyModule
    // is just too slow.
    x_BuildNameToASNMap( mainModules, nameToASNMap );

    // parse spec file and extract some basic info
    CTraversalSpecFileParser spec_file_parser( traversal_spec_file ) ;
    string headerFileName = spec_file_parser.GetOutputFileHeader();
    string sourceFileName = spec_file_parser.GetOutputFileSource();
    CNcbiOfstream traversal_header_file( headerFileName.c_str() );
    CNcbiOfstream traversal_source_file( sourceFileName.c_str() );

    CTraversalNode::TNodeSet nodesWithFunctions;
    CTraversalNode::TNodeVec rootTraversalNodes;
    
    ITERATE( CTraversalSpecFileParser::TRootInfoRefVec, root_iter, spec_file_parser.GetRootTypes() ) {
        CDataType *a_asn_root = NULL;

        TNameToASNMap::iterator root_find_iter = nameToASNMap.find( (*root_iter)->m_Root_type_name );
        if( root_find_iter != nameToASNMap.end() ) {
            a_asn_root = root_find_iter->second;
        }

        if( NULL == a_asn_root ) {
            throw runtime_error( "could not resolve root type: " + (*root_iter)->m_Root_type_name );
        }

        // recurse to create the traversal node
        {
            TASNToTravMap asn_nodes_seen; // to prevent infinite recursion
            CRef<CTraversalNode> a_traversal_root = x_CreateNode( nameToASNMap, asn_nodes_seen, "x_" + (*root_iter)->m_Root_func_name, a_asn_root, CRef<CTraversalNode>() );
            rootTraversalNodes.push_back( a_traversal_root );
            // remove "x_" from root node's function name since it's public
            a_traversal_root->RemoveXFromFuncName();
        }

        // uncomment this code if you want to print out all the nodes
        // CPrintTraversalNodeCallback printTraversalNodes(std::cerr);
        // a_traversal_root->DepthFirst( printTraversalNodes, CTraversalNode::fTraversalOpts_AllowCycles );
    }

    // This will attach functions to all nodes that should get them, and
    // fill in nodesWithFunctions
    // ( The constructor does all the work )
    CTraversalAttachUserFuncsCallback( spec_file_parser, nodesWithFunctions );

    // remove empty nodes (or nodes that only call empty calls)
    // otherwise we might generate a huge number of functions
    if( spec_file_parser.IsPruningAllowed() ) {
        x_PruneEmptyNodes( rootTraversalNodes, nodesWithFunctions );
    }

    // This merges functions that are completely identical.
    // This also tremendously reduces the number of functions we output.
    // ( The constructor does all the work )
    if( spec_file_parser.IsMergingAllowed() ) {
        CTraversalMerger merger( rootTraversalNodes, nodesWithFunctions );
    }

    // Finally, generate the files
    x_GenerateHeaderFile( spec_file_parser.GetOutputClassName(), headerFileName, traversal_header_file, 
        rootTraversalNodes, spec_file_parser.GetMembers(), spec_file_parser.GetHeaderIncludes(),
        spec_file_parser.GetHeaderForwardDeclarations() );
    x_GenerateSourceFile( spec_file_parser.GetOutputClassName(), headerFileName, traversal_source_file, 
        rootTraversalNodes, spec_file_parser.GetSourceIncludes() );
}

void CTraversalCodeGenerator::x_PruneEmptyNodes( 
        vector< CRef<CTraversalNode> > &rootTraversalNodes, 
        CTraversalNode::TNodeSet &nodesWithFunctions )
{
    CTraversalNode::TNodeSet usefulNodes;

    // we traverse to find all callers of useful nodes and 
    // add those to the useful nodes set
    CAddToNodeSetCallback add_to_set_callback( usefulNodes );
    NON_CONST_SET_ITERATE( CTraversalNode::TNodeSet, node_iter, nodesWithFunctions ) {
        (*node_iter)->DepthFirst( add_to_set_callback, 
            (CTraversalNode::fTraversalOpts_UpCallers) );
    }

    // force root nodes to be considered useful, since the user may call
    // them even if they don't do anything
    usefulNodes.insert( rootTraversalNodes.begin(), rootTraversalNodes.end() );

    // delete all nodes which are not useful
    CTraversalNode::TNodeRawSet::const_iterator every_node_iter = CTraversalNode::GetEveryNode().begin();
    while( every_node_iter != CTraversalNode::GetEveryNode().end() ) {
        // holds a reference so we don't delete the node until we've
        // incremented the iterator.
        CRef<CTraversalNode> node = const_cast<CTraversalNode*>(*every_node_iter)->Ref();

        if( usefulNodes.find(node) == usefulNodes.end() ) {
            node->Clear();
        }
        // increment before the "node" CRef goes out of scope, in case it's destroyed
        ++every_node_iter;
    }
}

void CTraversalCodeGenerator::x_GenerateHeaderFile( 
    const string &output_class_name,
    const string &headerFileName,
    CNcbiOstream& traversal_header_file, 
    vector< CRef<CTraversalNode> > &rootTraversalNodes,
    const CTraversalSpecFileParser::TMemberRefVec & members,
    const std::vector<std::string> &header_includes,
    const std::vector<std::string> &header_forward_declarations )
{
    // begin include guard
    string include_guard_define;
    x_GetIncludeGuard( include_guard_define, headerFileName );
    traversal_header_file << "#ifndef " << include_guard_define << endl;
    traversal_header_file << "#define " << include_guard_define << endl;
    traversal_header_file << endl;

    // Add copyright notice to the top
    CFileCode::WriteCopyrightHeader(traversal_header_file);
    traversal_header_file << " */ " << endl; // close copyright notice
    traversal_header_file << kAutoGenerationNotice;
    traversal_header_file << endl;

    // generate include directives at top
    CGenerateIncludesCallback generateIncludesCallback( traversal_header_file );
    NON_CONST_ITERATE( vector< CRef<CTraversalNode> >, root_iter, rootTraversalNodes ) {
        (*root_iter)->DepthFirst( generateIncludesCallback );
    }
    traversal_header_file << endl;

    // generate explicitly requested includes
    ITERATE( std::vector<std::string>, include_iter, header_includes ) {
        traversal_header_file << "#include " << *include_iter << endl;
    }
    traversal_header_file << endl;

    // generate forward declarations
    ITERATE( std::vector<std::string>, decl_iter, header_forward_declarations ) {
        traversal_header_file << "class " << *decl_iter << ";" << endl;
    }

    traversal_header_file << endl;
    traversal_header_file << "BEGIN_NCBI_SCOPE" << endl;
    traversal_header_file << "BEGIN_SCOPE(objects)" << endl;
    traversal_header_file << endl;

    traversal_header_file << "class " << output_class_name << " { " <<endl;
    traversal_header_file << "public: " << endl;

    // generate constructor, if we're required to initialize members:
    if( ! members.empty() ) {
        traversal_header_file << "  " << output_class_name << "(" << endl;
        // constructor params
        ITERATE( CTraversalSpecFileParser::TMemberRefVec, member_iter, members ) {
            if( member_iter != members.begin() ) {
                traversal_header_file << "," << endl;
            }
            traversal_header_file << "    " << (*member_iter)->m_Type_name << " " << 
                x_MemberVarNameToArg((*member_iter)->m_Variable_name);
        }
        traversal_header_file << " ) : " << endl;

        // constructor initializers that initialize from external args
        ITERATE( CTraversalSpecFileParser::TMemberRefVec, member_iter, members ) {
            traversal_header_file << "    " << (*member_iter)->m_Variable_name << "(" << 
                x_MemberVarNameToArg((*member_iter)->m_Variable_name) << "), " << endl;
        }

        // constructor initializers that initialize stored args to NULL
        CGenerateStoredArgInitializerCallback generateStoredArgInitializerCallback( traversal_header_file );
        NON_CONST_ITERATE( vector< CRef<CTraversalNode> >, root_iter, rootTraversalNodes ) {
            (*root_iter)->DepthFirst( generateStoredArgInitializerCallback );
        }
        // m_Dummy is used to make it easier to generate the constructor code.
        // It lets us not worry about comma usage and whether or not to put the
        // initializer colon.
        traversal_header_file << "    m_Dummy(0)" << endl;

        traversal_header_file << "  { } " << endl;
        traversal_header_file << endl;
    }

    // generate prototypes of root functions, which are public
    CGenerateCodeCallback generateCodeCallback( output_class_name, traversal_header_file, 
        CTraversalNode::eGenerateMode_Prototypes );
    NON_CONST_ITERATE( vector< CRef<CTraversalNode> >, root_iter, rootTraversalNodes ) {
        // no recursion since we only want the roots
        generateCodeCallback.Call( **root_iter, CTraversalNode::TNodeVec(), CTraversalNode::CDepthFirstCallback::eCallType_NonCyclic );
    }
    
    // generate prototypes of non-root functions, which are private:
    traversal_header_file << endl;
    traversal_header_file << "private: " << endl;
    NON_CONST_ITERATE( vector< CRef<CTraversalNode> >, root_iter, rootTraversalNodes ) {
        // recurse to get non-root functions.  generateCodeCallback automatically skips duplicates,
        // so we won't print the root functions twice.
        (*root_iter)->DepthFirst( generateCodeCallback, CTraversalNode::fTraversalOpts_Post ) ;
    }

    // generate member variables specified in the description file
    if( ! members.empty() ) {
        traversal_header_file << endl;
        ITERATE( CTraversalSpecFileParser::TMemberRefVec, member_iter, members ) {
            traversal_header_file << "  " << (*member_iter)->m_Type_name << " " << 
                (*member_iter)->m_Variable_name << ";" << endl;
        }
    }

    // generate member variables created by functions which store their last value
    traversal_header_file << endl;
    CGenerateStoredArgVariablesCallback generateStoredArgVariablesCallback( traversal_header_file );
    NON_CONST_ITERATE( vector< CRef<CTraversalNode> >, root_iter, rootTraversalNodes ) {
        (*root_iter)->DepthFirst( generateStoredArgVariablesCallback ) ;
    }

    // generate dummy variable
    traversal_header_file << endl;
    traversal_header_file << "  int m_Dummy;" << endl;

    traversal_header_file << "}; // end of " << output_class_name << endl;
    traversal_header_file << endl;
    traversal_header_file << "END_SCOPE(objects)" << endl;
    traversal_header_file << "END_NCBI_SCOPE" << endl;
    traversal_header_file << endl;

    // end include guard
    traversal_header_file << "#endif /* " << include_guard_define << " */" << endl;
}

class CNotAlnum {
public:
    bool operator()( const char &ch ) { return ! isalnum(ch); }
};

void CTraversalCodeGenerator::x_GetIncludeGuard( string& include_guard_define, const string& headerFileName )
{
    include_guard_define = headerFileName;

    // erase path, if any
    include_guard_define = x_StripPath(include_guard_define);

    // strip off extension, if any
    const string::size_type last_period = include_guard_define.find_last_of(".");
    if( last_period != string::npos ) {
        include_guard_define.resize( last_period );
    }

    // remove all non-alphanumeric characters
    string::iterator remove_iter = 
        remove_if( include_guard_define.begin(), include_guard_define.end(), CNotAlnum() );
    include_guard_define.erase( remove_iter, include_guard_define.end() );

    // make all caps
    NStr::ToUpper( include_guard_define );

    // add the standard ending
    include_guard_define += "__HPP";
}

void CTraversalCodeGenerator::x_GenerateSourceFile(
    const string &output_class_name,
    const string &headerFileName,
    CNcbiOstream& traversal_source_file,
    vector< CRef<CTraversalNode> > &rootTraversalNodes,
    const std::vector<std::string> &source_includes )
{
    // Add copyright notice to the top
    CFileCode::WriteCopyrightHeader(traversal_source_file);
    traversal_source_file << " */ " << endl; // close copyright notice
    traversal_source_file << kAutoGenerationNotice;
    traversal_source_file << endl;

    // generate include directives at top
    if (!CFileCode::GetPchHeader().empty()) {
        traversal_source_file <<
            "#include <" << CFileCode::GetPchHeader() << ">\n";
    }
    ITERATE( vector<string>, include_iter, source_includes ) {
        traversal_source_file << "#include " << (*include_iter) << endl;
    }

    traversal_source_file << endl;
    traversal_source_file << "BEGIN_NCBI_SCOPE" << endl;
    traversal_source_file << "BEGIN_SCOPE(objects)" << endl;
    traversal_source_file << endl;

    // generate main body of functions
    CGenerateCodeCallback generateCodeCallback( output_class_name, traversal_source_file, 
        CTraversalNode::eGenerateMode_Definitions );
    NON_CONST_ITERATE( vector< CRef<CTraversalNode> >, root_iter, rootTraversalNodes ) {
        (*root_iter)->DepthFirst( generateCodeCallback, CTraversalNode::fTraversalOpts_Post ) ;
    }

    traversal_source_file << endl;
    traversal_source_file << "END_SCOPE(objects)" << endl;
    traversal_source_file << "END_NCBI_SCOPE" << endl;
    traversal_source_file << endl;
}

string CTraversalCodeGenerator::x_StripPath( const string &file_name )
{
    const string::size_type last_slash = file_name.find_last_of("/\\");
    if( last_slash == string::npos ) {
        return file_name;
    } else {
        return file_name.substr( last_slash + 1 );
    }
}

std::string CTraversalCodeGenerator::x_MemberVarNameToArg(const std::string &member_var_name )
{
    // remove initial m_ and make first letter lowercase
    _ASSERT( NStr::StartsWith(member_var_name, "m_") );
    string result = member_var_name.substr(2);
    result[0] = tolower(result[0]);
    return result;
}

void CTraversalCodeGenerator::x_BuildNameToASNMap( CFileSet& mainModules, TNameToASNMap &nameToASNMap )
{
    ITERATE( CFileSet::TModuleSets, mod_set_iter, mainModules.GetModuleSets() ) {
        ITERATE(  CFileModules::TModules, file_mod_iter, (*mod_set_iter)->GetModules() ) {
            ITERATE( CDataTypeModule::TDefinitions, def_iter, (*file_mod_iter)->GetDefinitions() ) {

                CDataType *data_type = &*(*def_iter).second;
                const string &full_name = data_type->GetFullName();

                if( nameToASNMap.find(full_name) != nameToASNMap.end() ) {
                    throw runtime_error( "Tried to add CDataType name multiple times: '" + full_name + "'" );
                }

                nameToASNMap[full_name] = data_type;
            }
        }
    }
}

CRef<CTraversalNode> 
CTraversalCodeGenerator::x_CreateNode( 
    const TNameToASNMap &nameToASNMap,
    TASNToTravMap &asn_nodes_seen,
    const string &var_name,
    CDataType *asn_node, 
    CRef<CTraversalNode> parent )
{
    // To prevent infinite recursion, we check if we've seen this node already and
    // return it if so.
    TASNToTravMap::iterator node_location = asn_nodes_seen.find(asn_node);
    if( node_location != asn_nodes_seen.end() ) {
        CRef<CTraversalNode> child = node_location->second;
        // we still have to link parent/child, though
        if( parent ) {
            child->AddCaller( parent );
        }
        return child;
    }


    const string &member_name = asn_node->GetMemberName();

    CRef<CTraversalNode> result = CTraversalNode::Create( parent, var_name, asn_node );
    asn_nodes_seen.insert( TASNToTravMap::value_type(asn_node, result) );

    // recurse
    if( asn_node->IsReference() ) {
        CReferenceDataType* ref = dynamic_cast<CReferenceDataType*>(asn_node);
        const string &user_type_name = ref->GetUserTypeName();
        TNameToASNMap::const_iterator type_name_iter = nameToASNMap.find(user_type_name);
        if( type_name_iter == nameToASNMap.end() ) {
            throw runtime_error("Could not find user type name '" + user_type_name + "'");
        }
        CDataType *memberType = type_name_iter->second;
        x_CreateNode( nameToASNMap, asn_nodes_seen, asn_node->GetMemberName(), memberType, result );
    } else if( asn_node->IsContainer() ) {
        CDataMemberContainerType * container_type = dynamic_cast<CDataMemberContainerType*>(asn_node);
        ITERATE( CDataMemberContainerType::TMembers, member_iter, container_type->GetMembers() ) {
            CDataType *member_type = (*member_iter)->GetType();
            x_CreateNode( nameToASNMap, asn_nodes_seen, member_type->GetMemberName(), member_type, result );
        }
    } else if( asn_node->IsUniSeq() ) {
        CUniSequenceDataType *uni_seq = dynamic_cast<CUniSequenceDataType*>(asn_node);
        CDataType *member_type = uni_seq->GetElementType();
        x_CreateNode( nameToASNMap, asn_nodes_seen, member_type->GetMemberName(), member_type, result );
    }

    // we're done with this path of traversal, so we remove this so we can find
    // the same datatype on other paths.
    asn_nodes_seen.erase( asn_node );

    return result;
}

END_NCBI_SCOPE
