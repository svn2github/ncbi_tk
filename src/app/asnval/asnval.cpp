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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko
 *
 * File Description:
 *   validator
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objectio.hpp>

#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>

// Objects includes
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objtools/validator/validator.hpp>
#include <objtools/validator/valid_cmdargs.hpp>
#include <objtools/cleanup/cleanup.hpp>

#include <objects/seqset/Bioseq_set.hpp>

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_descr_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <misc/data_loaders_util/data_loaders_util.hpp>

#include <serial/objostrxml.hpp>
#include <misc/xmlwrapp/xmlwrapp.hpp>
#include <util/compress/stream_util.hpp>
#include <util/format_guess.hpp>

#include <common/test_assert.h>  /* This header must go last */


using namespace ncbi;
using namespace objects;
using namespace validator;

const char * ASNVAL_APP_VER = "10.1";

#define USE_XMLWRAPP_LIBS

/////////////////////////////////////////////////////////////////////////////
//
//  Demo application
//

class CValXMLStream;

class CAsnvalApp : public CNcbiApplication, CReadClassMemberHook
{
public:
    CAsnvalApp(void);
    ~CAsnvalApp(void);

    virtual void Init(void);
    virtual int  Run (void);

    // CReadClassMemberHook override
    void ReadClassMember(CObjectIStream& in,
        const CObjectInfo::CMemberIterator& member);

private:

    void Setup(const CArgs& args);

    auto_ptr<CObjectIStream> OpenFile(const CArgs& args);
    auto_ptr<CObjectIStream> OpenFile(const string& fname);

    CConstRef<CValidError> ProcessCatenated(void);
    CConstRef<CValidError> ProcessSeqEntry(CSeq_entry& se);
    CConstRef<CValidError> ProcessSeqEntry(void);
    CConstRef<CValidError> ProcessSeqSubmit(void);
    CConstRef<CValidError> ProcessSeqAnnot(void);
    CConstRef<CValidError> ProcessSeqFeat(void);
    CConstRef<CValidError> ProcessBioSource(void);
    CConstRef<CValidError> ProcessPubdesc(void);
    CConstRef<CValidError> ProcessBioseqset(void);
    CConstRef<CValidError> ProcessBioseq(void);

    CConstRef<CValidError> ValidateInput (void);
    void ValidateOneDirectory(string dir_name, bool recurse);
    void ValidateOneFile(const string& fname);
    void ProcessReleaseFile(const CArgs& args);

    void ConstructOutputStreams();
    void DestroyOutputStreams();

    CRef<CSeq_entry> ReadSeqEntry(void);
    CRef<CSeq_feat> ReadSeqFeat(void);
    CRef<CBioSource> ReadBioSource(void);
    CRef<CPubdesc> ReadPubdesc(void);

    CRef<CValidError> ReportReadFailure(void);

    CRef<CScope> BuildScope(void);

    void PrintValidError(CConstRef<CValidError> errors, 
        const CArgs& args);

    enum EVerbosity {
        eVerbosity_Normal = 1,
        eVerbosity_Spaced = 2,
        eVerbosity_Tabbed = 3,
        eVerbosity_XML = 4,
        eVerbosity_min = 1, eVerbosity_max = 4
    };

    void PrintValidErrItem(const CValidErrItem& item);

    CRef<CObjectManager> m_ObjMgr;
    auto_ptr<CObjectIStream> m_In;
    unsigned int m_Options;
    bool m_Continue;
    bool m_OnlyAnnots;
    time_t m_Longest;
    string m_CurrentId;
    string m_LongestId;
    size_t m_NumFiles;
    size_t m_NumRecords;

    size_t m_Level;
    size_t m_Reported;
    EDiagSev m_ReportLevel;

    bool m_DoCleanup;
    CCleanup m_Cleanup;

    EDiagSev m_LowCutoff;
    EDiagSev m_HighCutoff;

    EVerbosity m_verbosity;
    string     m_obj_type;

    CNcbiOstream* m_ValidErrorStream;
    CNcbiOstream* m_LogStream;
#ifdef USE_XMLWRAPP_LIBS
    auto_ptr<CValXMLStream> m_ostr_xml;
#endif
};

class CValXMLStream: public CObjectOStreamXml
{
public:
    CValXMLStream(CNcbiOstream& out, bool deleteOut): CObjectOStreamXml(out, deleteOut){};
    void Print(const CValidErrItem& item);
};


// constructor
CAsnvalApp::CAsnvalApp(void) :
    m_ObjMgr(0), m_In(0), m_Options(0), m_Continue(false), m_OnlyAnnots(false),
    m_Longest(0), m_CurrentId(""), m_LongestId(""), m_NumFiles(0),
    m_NumRecords(0), m_Level(0), m_Reported(0), m_verbosity(eVerbosity_min),
    m_ValidErrorStream(0), m_LogStream(0)
{
    SetVersionByBuild(1);
}


// destructor
CAsnvalApp::~CAsnvalApp (void)

{
}


void CAsnvalApp::Init(void)
{
    // Prepare command line descriptions

    // Create
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddOptionalKey
        ("p", "Directory", "Path to ASN.1 Files",
        CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey
        ("i", "InFile", "Single Input File",
        CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey(
        "o", "OutFile", "Single Output File",
        CArgDescriptions::eOutputFile);
    arg_desc->AddOptionalKey(
        "f", "Filter", "Substring Filter",
        CArgDescriptions::eOutputFile);
    arg_desc->AddDefaultKey
        ("x", "String", "File Selection Substring", CArgDescriptions::eString, ".ent");
    arg_desc->AddFlag("u", "Recurse");
    arg_desc->AddDefaultKey(
        "R", "SevCount", "Severity for Error in Return Code",
        CArgDescriptions::eInteger, "4");
    arg_desc->AddDefaultKey(
        "Q", "SevLevel", "Lowest Severity for Error to Show",
        CArgDescriptions::eInteger, "3");
    arg_desc->AddDefaultKey(
        "P", "SevLevel", "Highest Severity for Error to Show",
        CArgDescriptions::eInteger, "5");
    CArgAllow* constraint = new CArgAllow_Integers(eDiagSevMin, eDiagSevMax);
    arg_desc->SetConstraint("Q", constraint);
    arg_desc->SetConstraint("P", constraint);
    arg_desc->SetConstraint("R", constraint);
    arg_desc->AddOptionalKey(
        "E", "String", "Only Error Code to Show",
        CArgDescriptions::eString);

    arg_desc->AddDefaultKey("a", "a", 
                            "ASN.1 Type (a Automatic, c Catenated, z Any, e Seq-entry, b Bioseq, s Bioseq-set, m Seq-submit, t Batch Bioseq-set, u Batch Seq-submit",
                            CArgDescriptions::eString,
                            "a");

    arg_desc->AddFlag("b", "Input is in binary format");
    arg_desc->AddFlag("c", "Batch File is Compressed");

    CValidatorArgUtil::SetupArgDescriptions(arg_desc.get());
    arg_desc->AddFlag("annot", "Verify Seq-annots only");

    arg_desc->AddOptionalKey(
        "L", "OutFile", "Log File",
        CArgDescriptions::eOutputFile);

    arg_desc->AddDefaultKey("v", "Verbosity", 
                            "Verbosity\n"
                            "\t1 Standard Report\n"
                            "\t2 Accession / Severity / Code(space delimited)\n"
                            "\t3 Accession / Severity / Code(tab delimited)\n"
                            "\t4 XML Report",
                            CArgDescriptions::eInteger, "1");
    CArgAllow* v_constraint = new CArgAllow_Integers(eVerbosity_min, eVerbosity_max);
    arg_desc->SetConstraint("v", v_constraint);

    arg_desc->AddFlag("cleanup", "Perform BasicCleanup before validating (to match C Toolkit)");

    CDataLoadersUtil::AddArgumentDescriptions(*arg_desc,
                                              CDataLoadersUtil::fDefault |
                                              CDataLoadersUtil::fGenbankOffByDefault);

    // Program description
    string prog_description = "ASN Validator\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
        prog_description, false);

    // Pass argument descriptions to the application
    SetupArgDescriptions(arg_desc.release());

}


CConstRef<CValidError> CAsnvalApp::ValidateInput (void)
{
    // Process file based on its content
    // Unless otherwise specifien we assume the file in hand is
    // a Seq-entry ASN.1 file, other option are a Seq-submit or NCBI
    // Release file (batch processing) where we process each Seq-entry
    // at a time.
    CConstRef<CValidError> eval;
    // ASN.1 Type (a Automatic, z Any, e Seq-entry, b Bioseq, s Bioseq-set, m Seq-submit, t Batch Bioseq-set, u Batch Seq-submit",
    string header = m_In->ReadFileHeader();
    if (header.empty() && !m_obj_type.empty())
    {
        switch (m_obj_type[0])
        {
        case 'e':
            header = "Seq-entry";
            break;
        case 'm':
            header = "Seq-submit";
            break;
        case 's':
            header = "Bioseq-set";
            break;
        case 'b':
            header = "Bioseq";
            break;
        }
    }

    if (!m_obj_type.empty() && m_obj_type[0] == 'c') {
        eval = ProcessCatenated();
    } else if (header == "Seq-submit" ) {           // Seq-submit
        eval = ProcessSeqSubmit();
    } else if ( header == "Seq-entry" ) {           // Seq-entry
        eval = ProcessSeqEntry();
    } else if ( header == "Seq-annot" ) {           // Seq-annot
        eval = ProcessSeqAnnot();
    } else if (header == "Seq-feat" ) {             // Seq-feat
        eval = ProcessSeqFeat();
    } else if (header == "BioSource" ) {            // BioSource
        eval = ProcessBioSource();
    } else if (header == "Pubdesc" ) {              // Pubdesc
        eval = ProcessPubdesc();
    } else if (header == "Bioseq-set" ) {           // Bioseq-set
        eval = ProcessBioseqset();
    } else if (header == "Bioseq" ) {               // Bioseq
        eval = ProcessBioseq();
    } else {
        NCBI_THROW(CException, eUnknown, "Unhandled type " + header);
    }

    return eval;
}


void CAsnvalApp::ValidateOneFile(const string& fname)
{
    const CArgs& args = GetArgs();

    if (m_LogStream) {
        *m_LogStream << fname << endl;
    }
    auto_ptr<CNcbiOfstream> local_stream;

    bool close_error_stream = false;

    try {
        if (!m_ValidErrorStream) {
            string path;
            if (fname.empty())  {
                path = "stdin.val";
            } else {
                size_t pos = NStr::Find(fname, ".", 0, string::npos, NStr::eLast);
                if (pos != string::npos)
                    path = fname.substr(0, pos);
                else
                    path = fname;

                path.append(".val");
            }

            local_stream.reset(new CNcbiOfstream(path.c_str()));
            m_ValidErrorStream = local_stream.get();

            ConstructOutputStreams();
            close_error_stream = true;
        }
    }
    catch (CException) {
    }

    m_In = OpenFile(fname);
    if (m_In.get() == 0) {
        PrintValidError(ReportReadFailure(), args);
        NCBI_THROW(CException, eUnknown, "Unable to open " + fname);
    } else {
        try {
            if ( NStr::Equal(args["a"].AsString(), "t")) {          // Release file
                // Open File 
                ProcessReleaseFile(args);
            }
            else {
                size_t num_validated = 0;
                while (true) {
                   time_t start_time = time(NULL);
                   try {
                        CConstRef<CValidError> eval = ValidateInput();

                        if (eval) {
                            PrintValidError(eval, args);
                        }
                        num_validated++;
                    }
                    catch (CException &e) {
                        if (num_validated == 0) {
                            throw(e);
                        }
                        else {
                            break;
                        }
                    }
                    time_t stop_time = time(NULL);
                    time_t elapsed = stop_time - start_time;
                    if (elapsed > m_Longest) {
                        m_Longest = elapsed;
                        m_LongestId = m_CurrentId;
                    }
                }
            }
        } catch (CException &e) {
            // Also log to XML?
            ERR_POST(e);
            ++m_Reported;
        }
    }
    m_NumFiles++;
    if (close_error_stream) {
        DestroyOutputStreams();
    }
    m_In.reset();
}


void CAsnvalApp::ValidateOneDirectory(string dir_name, bool recurse)
{
    const CArgs& args = GetArgs();

    CDir dir(dir_name);

    string suffix = ".ent";
    if (args["x"]) {
        suffix = args["x"].AsString();
    }
    string mask = "*" + suffix;

    CDir::TEntries files (dir.GetEntries(mask, CDir::eFile));
    ITERATE(CDir::TEntries, ii, files) {
        string fname = (*ii)->GetName();
        if ((*ii)->IsFile() &&
            (!args["f"] || NStr::Find (fname, args["f"].AsString()) != string::npos)) {
            string fname = CDirEntry::MakePath(dir_name, (*ii)->GetName());
            ValidateOneFile (fname);
        }
    }
    if (recurse) {
        CDir::TEntries subdirs (dir.GetEntries("", CDir::eDir));
        ITERATE(CDir::TEntries, ii, subdirs) {
            string subdir = (*ii)->GetName();
            if ((*ii)->IsDir() && !NStr::Equal(subdir, ".") && !NStr::Equal(subdir, "..")) {
                string subname = CDirEntry::MakePath(dir_name, (*ii)->GetName());
                ValidateOneDirectory (subname, recurse);
            }
        }
    }
}


int CAsnvalApp::Run(void)
{
    const CArgs& args = GetArgs();
    Setup(args);

    time_t start_time = time(NULL);

    if (args["o"]) {
        m_ValidErrorStream = &(args["o"].AsOutputFile());
    }
            
    m_LogStream = args["L"] ? &(args["L"].AsOutputFile()) : &NcbiCout;

    // note - the C Toolkit uses 0 for SEV_NONE, but the C++ Toolkit uses 0 for SEV_INFO
    // adjust here to make the inputs to asnvalidate match asnval expectations
    m_ReportLevel = static_cast<EDiagSev>(args["R"].AsInteger() - 1);
    m_LowCutoff = static_cast<EDiagSev>(args["Q"].AsInteger() - 1);
    m_HighCutoff = static_cast<EDiagSev>(args["P"].AsInteger() - 1);

    m_DoCleanup = args["cleanup"] && args["cleanup"].AsBoolean();
    m_verbosity = static_cast<EVerbosity>(args["v"].AsInteger());

    // Process file based on its content
    // Unless otherwise specifien we assume the file in hand is
    // a Seq-entry ASN.1 file, other option are a Seq-submit or NCBI
    // Release file (batch processing) where we process each Seq-entry
    // at a time.
    m_Reported = 0;

    m_obj_type = args["a"].AsString();

    if (args["b"] && m_obj_type == "a")
    {
        NCBI_THROW(CException, eUnknown, "Specific argument -a must be used along with -b flags" );
    }

    bool execption_caught = false;
    try {
        ConstructOutputStreams();

        if ( args["p"] ) {
            ValidateOneDirectory (args["p"].AsString(), args["u"]);
        } else if (args["i"]) {
            ValidateOneFile (args["i"].AsString());
        } else {
            ValidateOneFile("");
        }
    } catch (CException& e) {
        ERR_POST(Error << e);
        execption_caught = true;
    }
    if (m_NumFiles == 0) {
       ERR_POST("No matching files found");
    }

    time_t stop_time = time(NULL);
    if (m_LogStream) {
        *m_LogStream << "Finished in " << stop_time - start_time << " seconds" << endl;
        *m_LogStream << "Longest processing time " << m_Longest << " seconds on " << m_LongestId << endl;
        *m_LogStream << "Total number of records " << m_NumRecords << endl;
    }

    DestroyOutputStreams();

    if (m_Reported > 0  ||  execption_caught) {
        return 1;
    } else {
        return 0;
    }
}


CRef<CScope> CAsnvalApp::BuildScope (void)
{
    CRef<CScope> scope(new CScope (*m_ObjMgr));
    scope->AddDefaults();

    return scope;
}


void CAsnvalApp::ReadClassMember
(CObjectIStream& in,
 const CObjectInfo::CMemberIterator& member)
{
    m_Level++;

    if ( m_Level == 1 ) {
        size_t n = 0;
        // Read each element separately to a local TSeqEntry,
        // process it somehow, and... not store it in the container.
        for ( CIStreamContainerIterator i(in, member); i; ++i ) {
            try {
                // Get seq-entry to validate
                CRef<CSeq_entry> se(new CSeq_entry);
                i >> *se;

                // Validate Seq-entry
                CValidator validator(*m_ObjMgr);
                CRef<CScope> scope = BuildScope();
                CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*se);

                if (m_LogStream) {
                    CBioseq_CI bi(seh);
                    if (bi) {
                        m_CurrentId = "";
                        bi->GetId().front().GetSeqId()->GetLabel(&m_CurrentId);
                        *m_LogStream << m_CurrentId << endl;
                    }
                }

                if (m_DoCleanup) {
                    m_Cleanup.SetScope (scope);
                    m_Cleanup.BasicCleanup (*se);
                }

                if ( m_OnlyAnnots ) {
                    for (CSeq_annot_CI ni(seh); ni; ++ni) {
                        const CSeq_annot_Handle& sah = *ni;
                        CConstRef<CValidError> eval = validator.Validate(sah, m_Options);
                        m_NumRecords++;
                        if ( eval ) {
                            PrintValidError(eval, GetArgs());
                        }
                    }
                } else {
                    // CConstRef<CValidError> eval = validator.Validate(*se, &scope, m_Options);
                    CStopWatch sw(CStopWatch::eStart);
                    CConstRef<CValidError> eval = validator.Validate(seh, m_Options);
                    m_NumRecords++;
                    time_t elapsed = sw.Elapsed();
                    if (elapsed > m_Longest) {
                        m_Longest = elapsed;
                        m_LongestId = m_CurrentId;
                   }
                    //if (m_ValidErrorStream) {
                    //    *m_ValidErrorStream << "Elapsed = " << sw.Elapsed() << endl;
                    //}
                    if ( eval ) {
                        PrintValidError(eval, GetArgs());
                    }
                }
                scope->RemoveTopLevelSeqEntry(seh);
                scope->ResetHistory();
                n++;
            } catch (exception&) {
                if ( !m_Continue ) {
                    throw;
                }
                // should we issue some sort of warning?
            }
        }
    } else {
        in.ReadClassMember(member);
    }

    m_Level--;
}


void CAsnvalApp::ProcessReleaseFile
(const CArgs& args)
{
    CRef<CBioseq_set> seqset(new CBioseq_set);

    // Register the Seq-entry hook
    CObjectTypeInfo set_type = CType<CBioseq_set>();
    set_type.FindMember("seq-set").SetLocalReadHook(*m_In, this);

    // Read the CBioseq_set, it will call the hook object each time we 
    // encounter a Seq-entry
    *m_In >> *seqset;
}

CRef<CValidError> CAsnvalApp::ReportReadFailure(void)
{
    CRef<CValidError> errors(new CValidError());
    errors->AddValidErrItem(eDiag_Critical, eErr_GENERIC_InvalidAsn, "Unable to read invalid ASN.1");
    return errors;
}


CRef<CSeq_entry> CAsnvalApp::ReadSeqEntry(void)
{
    CRef<CSeq_entry> se(new CSeq_entry);
    m_In->Read(ObjectInfo(*se), CObjectIStream::eNoFileHeader);

    return se;
}

CConstRef<CValidError> CAsnvalApp::ProcessCatenated(void)
{
    try {
        while (true) {
            // Get seq-entry to validate
            CRef<CSeq_entry> se(new CSeq_entry);

            try {
                m_In->Read(ObjectInfo(*se), CObjectIStream::eNoFileHeader);
            }
            catch (const CSerialException& e) {
                if (e.GetErrCode() == CSerialException::eEOF) {
                    break;
                } else {
                    throw;
                }
            }
            catch (const CException& e) {
                ERR_POST(Error << e);
                return ReportReadFailure();
            }
            try {
                CConstRef<CValidError> eval = ProcessSeqEntry(*se);
                if ( eval ) {
                    PrintValidError(eval, GetArgs());
                }
            }
            catch (const CObjMgrException& om_ex) {
                if (om_ex.GetErrCode() == CObjMgrException::eAddDataError)
                  se->ReassignConflictingIds();
                CConstRef<CValidError> eval = ProcessSeqEntry(*se);
                if ( eval ) {
                    PrintValidError(eval, GetArgs());
                }
            }
            try {
                m_In->SkipFileHeader(CSeq_entry::GetTypeInfo());
            }
            catch (const CEofException&) {
                break;
            }
        }
    }
    catch (const CException& e) {
        ERR_POST(Error << e);
        return ReportReadFailure();
    }

    return CConstRef<CValidError>();
}

CConstRef<CValidError> CAsnvalApp::ProcessBioseq(void)
{
    // Get seq-entry to validate
    CRef<CSeq_entry> se(new CSeq_entry);
    CBioseq& bioseq = se->SetSeq();

    m_In->Read(ObjectInfo(bioseq), CObjectIStream::eNoFileHeader);

    // Validate Seq-entry
    return ProcessSeqEntry(*se);
}

CConstRef<CValidError> CAsnvalApp::ProcessBioseqset(void)
{
    // Get seq-entry to validate
    CRef<CSeq_entry> se(new CSeq_entry);
    CBioseq_set& bioseqset = se->SetSet();

    m_In->Read(ObjectInfo(bioseqset), CObjectIStream::eNoFileHeader);
    // Validate Seq-entry
    return ProcessSeqEntry(*se);
}


CConstRef<CValidError> CAsnvalApp::ProcessSeqEntry(void)
{
    // Get seq-entry to validate
    CRef<CSeq_entry> se(new CSeq_entry);

    try {
            m_In->Read(ObjectInfo(*se), CObjectIStream::eNoFileHeader);    
    }
    catch (const CException& e) {
        ERR_POST(Error << e);
        return ReportReadFailure();
    }

    try
    {
        return ProcessSeqEntry(*se);
    }
    catch (const CObjMgrException& om_ex)
    {        
        if (om_ex.GetErrCode() == CObjMgrException::eAddDataError)
          se->ReassignConflictingIds();
    }
    // try again
    return ProcessSeqEntry(*se);
}

CConstRef<CValidError> CAsnvalApp::ProcessSeqEntry(CSeq_entry& se)
{
    // Validate Seq-entry
    CValidator validator(*m_ObjMgr);
    CRef<CScope> scope = BuildScope();
    if (m_DoCleanup) {        
        m_Cleanup.SetScope (scope);
        m_Cleanup.BasicCleanup (se);
    }
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(se);
    if (m_LogStream) {
        CBioseq_CI bi(seh);
        if (bi) {
            m_CurrentId = "";
            bi->GetId().front().GetSeqId()->GetLabel(&m_CurrentId);
            *m_LogStream << m_CurrentId << endl;
        }
    }

    if ( m_OnlyAnnots ) {
        for (CSeq_annot_CI ni(seh); ni; ++ni) {
            const CSeq_annot_Handle& sah = *ni;
            CConstRef<CValidError> eval = validator.Validate(sah, m_Options);
            m_NumRecords++;
            if ( eval ) {
                PrintValidError(eval, GetArgs());
            }
        }
        return CConstRef<CValidError>();
    }
    CConstRef<CValidError> eval = validator.Validate(se, scope, m_Options);
    m_NumRecords++;
    return eval;
    }


CRef<CSeq_feat> CAsnvalApp::ReadSeqFeat(void)
{
    CRef<CSeq_feat> feat(new CSeq_feat);
    m_In->Read(ObjectInfo(*feat), CObjectIStream::eNoFileHeader);

    return feat;
}


CConstRef<CValidError> CAsnvalApp::ProcessSeqFeat(void)
{
    CRef<CSeq_feat> feat(ReadSeqFeat());

    CRef<CScope> scope = BuildScope();
    if (m_DoCleanup) {
        m_Cleanup.SetScope (scope);
        m_Cleanup.BasicCleanup (*feat);
    }

    CValidator validator(*m_ObjMgr);
    CConstRef<CValidError> eval = validator.Validate(*feat, scope, m_Options);
    m_NumRecords++;
    return eval;
}


CRef<CBioSource> CAsnvalApp::ReadBioSource(void)
{
    CRef<CBioSource> src(new CBioSource);
    m_In->Read(ObjectInfo(*src), CObjectIStream::eNoFileHeader);

    return src;
}


CConstRef<CValidError> CAsnvalApp::ProcessBioSource(void)
{
    CRef<CBioSource> src(ReadBioSource());

    CValidator validator(*m_ObjMgr);
    CRef<CScope> scope = BuildScope();
    CConstRef<CValidError> eval = validator.Validate(*src, scope, m_Options);
    m_NumRecords++;
    return eval;
}


CRef<CPubdesc> CAsnvalApp::ReadPubdesc(void)
{
    CRef<CPubdesc> pd(new CPubdesc());
    m_In->Read(ObjectInfo(*pd), CObjectIStream::eNoFileHeader);

    return pd;
}


CConstRef<CValidError> CAsnvalApp::ProcessPubdesc(void)
{
    CRef<CPubdesc> pd(ReadPubdesc());

    CValidator validator(*m_ObjMgr);
    CRef<CScope> scope = BuildScope();
    CConstRef<CValidError> eval = validator.Validate(*pd, scope, m_Options);
    m_NumRecords++;
    return eval;
}



CConstRef<CValidError> CAsnvalApp::ProcessSeqSubmit(void)
{
    CRef<CSeq_submit> ss(new CSeq_submit);

    // Get seq-submit to validate
    try {
        m_In->Read(ObjectInfo(*ss), CObjectIStream::eNoFileHeader);
    }
    catch (CException& e) {
        ERR_POST(Error << e);
        return ReportReadFailure();
    }

    // Validae Seq-submit
    CValidator validator(*m_ObjMgr);
    CRef<CScope> scope = BuildScope();
    if (ss->GetData().IsEntrys()) {
        NON_CONST_ITERATE(CSeq_submit::TData::TEntrys, se, ss->SetData().SetEntrys()) {
            scope->AddTopLevelSeqEntry(**se);
        }
    }
    if (m_DoCleanup) {
        m_Cleanup.SetScope (scope);
        m_Cleanup.BasicCleanup (*ss);
    }

    CConstRef<CValidError> eval = validator.Validate(*ss, scope, m_Options);
    m_NumRecords++;
    return eval;
}


CConstRef<CValidError> CAsnvalApp::ProcessSeqAnnot(void)
{
    CRef<CSeq_annot> sa(new CSeq_annot);

    // Get seq-annot to validate
    m_In->Read(ObjectInfo(*sa), CObjectIStream::eNoFileHeader);

    // Validae Seq-annot
    CValidator validator(*m_ObjMgr);
    CRef<CScope> scope = BuildScope();
    if (m_DoCleanup) {
        m_Cleanup.SetScope (scope);
        m_Cleanup.BasicCleanup (*sa);
    }
    CSeq_annot_Handle sah = scope->AddSeq_annot(*sa);
    CConstRef<CValidError> eval = validator.Validate(sah, m_Options);
    m_NumRecords++;
    return eval;
}


void CAsnvalApp::Setup(const CArgs& args)
{
    // Setup application registry and logs for CONNECT library
    // CORE_SetLOG(LOG_cxx2c());
    // CORE_SetREG(REG_cxx2c(&GetConfig(), false));
    // Setup MT-safety for CONNECT library
    // CORE_SetLOCK(MT_LOCK_cxx2c());

    // Create object manager
    m_ObjMgr = CObjectManager::GetInstance();
    CDataLoadersUtil::SetupObjectManager(args, *m_ObjMgr,
                                         CDataLoadersUtil::fDefault |
                                         CDataLoadersUtil::fGenbankOffByDefault);

    m_OnlyAnnots = args["annot"];

    // Set validator options
    m_Options = CValidatorArgUtil::ArgsToValidatorOptions(args);
}


auto_ptr<CObjectIStream> CAsnvalApp::OpenFile(const CArgs& args)
{
    // file name
    return OpenFile(args["i"].AsString());
}


auto_ptr<CObjectIStream> OpenUncompressedStream(const string& fname)
{
    ENcbiOwnership own = fname.empty() ? eNoOwnership :eTakeOwnership;

    auto_ptr<CNcbiIstream> hold_stream(
         fname.empty()? 0: new CNcbiIfstream (fname.c_str(), ios::binary));

    CNcbiIstream* InputStream = fname.empty() ? &cin : hold_stream.get();

    CCompressStream::EMethod method;
    
    CFormatGuess::EFormat format = CFormatGuess::Format(*InputStream);
    switch (format)
    {
        case CFormatGuess::eGZip:  method = CCompressStream::eGZipFile;  break;
        case CFormatGuess::eBZip2: method = CCompressStream::eBZip2;     break;
        case CFormatGuess::eLzo:   method = CCompressStream::eLZO;       break;
        default:                   method = CCompressStream::eNone;      break;
    }
    if (method != CCompressStream::eNone)
    {
        CDecompressIStream* decompress(new CDecompressIStream(*InputStream, method, CCompressStream::fDefault, own));
        hold_stream.release();
        hold_stream.reset(decompress);
        InputStream = hold_stream.get();
        own = eTakeOwnership;
        format = CFormatGuess::Format(*InputStream);
    }

    auto_ptr<CObjectIStream> objectStream;
    switch (format)
    {
        case CFormatGuess::eBinaryASN:
        case CFormatGuess::eTextASN:
            objectStream.reset(CObjectIStream::Open(format==CFormatGuess::eBinaryASN ? eSerial_AsnBinary : eSerial_AsnText, *InputStream, own));
            hold_stream.release();
            break;
        default:
            break;
    }
    return objectStream;
}


auto_ptr<CObjectIStream> CAsnvalApp::OpenFile(const string& fname)
{
    return OpenUncompressedStream(fname);
}

void CAsnvalApp::PrintValidError
(CConstRef<CValidError> errors, 
 const CArgs& args)
{
    if ( errors->TotalSize() == 0 ) {
        return;
    }

    for ( CValidError_CI vit(*errors); vit; ++vit) {
        if (vit->GetSeverity() >= m_ReportLevel) {
            ++m_Reported;
        }
        if ( vit->GetSeverity() < m_LowCutoff || vit->GetSeverity() > m_HighCutoff) {
            continue;
        }
        if (args["E"] && !(NStr::EqualNocase(args["E"].AsString(), vit->GetErrCode()))) {
            continue;
        }
        PrintValidErrItem(*vit);
    }
    m_ValidErrorStream->flush();
}


static string s_GetSeverityLabel (EDiagSev sev, bool is_xml = false)
{
    static const string str_sev[] = {
        "NOTE", "WARNING", "ERROR", "REJECT", "FATAL", "MAX"
    };
    if (sev < 0 || sev > eDiagSevMax) {
        return "NONE";
    }
    if (sev == 0 && is_xml) {
        return "INFO";
    }

    return str_sev[sev];
}


void CAsnvalApp::PrintValidErrItem(const CValidErrItem& item)
{
    CNcbiOstream& os = *m_ValidErrorStream;
    switch (m_verbosity) {
        case eVerbosity_Normal:
            os << s_GetSeverityLabel(item.GetSeverity())
                << ": valid [" << item.GetErrGroup() << "." << item.GetErrCode() << "] "
                << item.GetMsg();
            if (item.IsSetObjDesc()) {
                os << " " << item.GetObjDesc();
            }
            os << endl;
            break;
        case eVerbosity_Spaced:
            {
                string spacer = "                    ";
                string msg = item.GetAccnver() + spacer;
                msg = msg.substr(0, 15);
                msg += s_GetSeverityLabel(item.GetSeverity());
                msg += spacer;
                msg = msg.substr(0, 30);
                msg += item.GetErrGroup() + "_" + item.GetErrCode();
                os << msg << endl;
            }
            break;
        case eVerbosity_Tabbed:
            os << item.GetAccnver() << "\t"
               << s_GetSeverityLabel(item.GetSeverity()) << "\t"
               << item.GetErrGroup() << "_" << item.GetErrCode() << endl;
            break;
#ifdef USE_XMLWRAPP_LIBS
        case eVerbosity_XML:
            {
                m_ostr_xml->Print(item);
            }
#else
        case eVerbosity_XML:
            {
                string msg = NStr::XmlEncode(item.GetMsg());
                if (item.IsSetFeatureId()) {
                    os << "  <message severity=\"" << s_GetSeverityLabel(item.GetSeverity())
                       << "\" seq-id=\"" << item.GetAccnver() 
                       << "\" feat-id=\"" << item.GetFeatureId()
                       << "\" code=\"" << item.GetErrGroup() << "_" << item.GetErrCode()
                       << "\">" << msg << "</message>" << endl;
                } else {
                    os << "  <message severity=\"" << s_GetSeverityLabel(item.GetSeverity())
                       << "\" seq-id=\"" << item.GetAccnver() 
                       << "\" code=\"" << item.GetErrGroup() << "_" << item.GetErrCode()
                       << "\">" << msg << "</message>" << endl;
                }
            }
#endif
            break;
    }
}

void CValXMLStream::Print(const CValidErrItem& item)
{
#if 0
    TTypeInfo info = item.GetThisTypeInfo();    
    WriteObject(&item, info);
#else
    m_Output.PutString("  <message severity=\"");
    m_Output.PutString(s_GetSeverityLabel(item.GetSeverity(), true));
    if (item.IsSetAccnver()) {
        m_Output.PutString("\" seq-id=\"");
        WriteString(item.GetAccnver(), eStringTypeVisible);
    }

    if (item.IsSetFeatureId()) {
       m_Output.PutString("\" feat-id=\"");
       WriteString(item.GetFeatureId(), eStringTypeVisible);
    }

    m_Output.PutString("\" code=\"");
    WriteString(item.GetErrGroup(), eStringTypeVisible);
    m_Output.PutString("_");
    WriteString(item.GetErrCode(), eStringTypeVisible);
    m_Output.PutString("\">");

    WriteString(item.GetMsg(), eStringTypeVisible);

    m_Output.PutString("</message>");
    m_Output.PutEol();
#endif
}

void CAsnvalApp::ConstructOutputStreams()
{
    if (m_ValidErrorStream && m_verbosity == eVerbosity_XML)
    {
#ifdef USE_XMLWRAPP_LIBS
        m_ostr_xml.reset(new CValXMLStream(*m_ValidErrorStream, false));
        m_ostr_xml->SetEncoding(eEncoding_UTF8);
        m_ostr_xml->SetReferenceDTD(false);
        m_ostr_xml->SetEnforcedStdXml(true);
        m_ostr_xml->WriteFileHeader(CValidErrItem::GetTypeInfo());
        m_ostr_xml->SetUseIndentation(true);
        m_ostr_xml->Flush();

        *m_ValidErrorStream << endl << "<asnvalidate version=\"" << ASNVAL_APP_VER << "\" severity_cutoff=\""
        << s_GetSeverityLabel(m_LowCutoff, true) << "\">" << endl;
        m_ValidErrorStream->flush();
#else
        *m_ValidErrorStream << "<asnvalidate version=\"" << ASNVAL_APP_VER << "\" severity_cutoff=\""
        << s_GetSeverityLabel(m_LowCutoff, true) << "\">" << endl;
#endif
    }
}

void CAsnvalApp::DestroyOutputStreams()
{
#ifdef USE_XMLWRAPP_LIBS
    if (m_ostr_xml.get())
    {
        m_ostr_xml.reset();
        *m_ValidErrorStream << endl << "</asnvalidate>" << endl;
    }
#endif
    m_ValidErrorStream = 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    return CAsnvalApp().AppMain(argc, argv);
}

// don't commit this
void mk(const CSerialObject *obj)
{
    if( obj ) {
        cerr << MSerial_AsnText << *obj << endl;
    } else {
        cerr << "(NULL)" << endl;
    }
}
