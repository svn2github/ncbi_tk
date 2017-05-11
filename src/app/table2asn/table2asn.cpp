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
* Authors:  Jonathan Kans, Clifford Clausen,
*           Aaron Ucko, Sergiy Gotvyanskyy
*
* File Description:
*   Converter of various files into ASN.1 format, main application function
*
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbi_mask.hpp>

#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

#include <util/line_reader.hpp>
#include <objtools/edit/remote_updater.hpp>

#include "multireader.hpp"
#include "table2asn_context.hpp"
#include "struc_cmt_reader.hpp"
#include "OpticalXML2ASN.hpp"
#include "feature_table_reader.hpp"
#include "fcs_reader.hpp"
#include "src_quals.hpp"

#include <objects/seq/Seq_descr.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/general/Date.hpp>

#include <objects/seq/Linkage_evidence.hpp>
#include <objects/seq/Seq_gap.hpp>

#include <objtools/edit/seq_entry_edit.hpp>
#include <objects/valerr/ValidError.hpp>
#include <objtools/validator/validator.hpp>

#include <objtools/readers/message_listener.hpp>
#include <common/ncbi_source_ver.h>

#include "table2asn_validator.hpp"

#include <objmgr/feat_ci.hpp>
#include "visitors.hpp"

#include <objtools/readers/fasta_exception.hpp>

#include <misc/data_loaders_util/data_loaders_util.hpp>

#include <common/test_assert.h>  /* This header must go last */

using namespace ncbi;
using namespace objects;

#define USE_SCOPE

class CTable2AsnLogger: public CMessageListenerLenient
{
public:
    CTable2AsnLogger(): m_enable_log(false) {}
    bool m_enable_log;
protected:
    virtual void PutProgress(
        const string & sMessage,
        const Uint8 iNumDone = 0,
        const Uint8 iNumTotal = 0)
    {
        if (m_enable_log)
            CMessageListenerLenient::PutProgress(sMessage, iNumDone, iNumTotal);
    }
};

class CTbl2AsnApp : public CNcbiApplication
{
public:
    CTbl2AsnApp(void);

    virtual void Init(void);
    virtual int Run (void);
    virtual int DryRun(void)
    {
        return Run();
    }

private:

    static const CDataLoadersUtil::TLoaders default_loaders = CDataLoadersUtil::fGenbank | CDataLoadersUtil::fVDB | CDataLoadersUtil::fGenbankOffByDefault;
    void Setup(const CArgs& args);

    string GenerateOutputFilename(const CTempString& ext) const;
    void ProcessOneFile();
    void ProcessOneFile(CRef<CSerialObject>& result);
    bool ProcessOneDirectory(const CDir& directory, const CMask& mask, bool recurse);
    void ProcessSecretFiles(CSeq_entry& result);
    void ProcessSRCFileAndQualifiers(const string& pathname, CSeq_entry& result, const string& opt_map_xml);
    void ProcessQVLFile(const string& pathname, CSeq_entry& result);
    void ProcessDSCFile(const string& pathname, CSeq_entry& result);
    void ProcessCMTFile(const string& pathname, CSeq_entry& result, bool byrows);
    void ProcessPEPFile(const string& pathname, CSeq_entry& result);
    void ProcessRNAFile(const string& pathname, CSeq_entry& result);
    void ProcessPRTFile(const string& pathname, CSeq_entry& result);
    void ProcessAnnotFile(const string& pathname, CSeq_entry& result);

    CRef<CScope> GetScope(void);

    auto_ptr<CMultiReader> m_reader;
    CRef<CSeq_entry> m_replacement_proteins;
    CRef<CSeq_entry> m_possible_proteins;
    CRef<CTable2AsnValidator> m_validator;
    CRef<CTable2AsnLogger> m_logger;
    auto_ptr<CForeignContaminationScreenReportReader> m_fcs_reader;
    CTable2AsnContext    m_context;

    //bool m_Continue;
    //bool m_OnlyAnnots;

    //EDiagSev m_LowCutoff;
    //EDiagSev m_HighCutoff;
};

CTbl2AsnApp::CTbl2AsnApp(void)
{
    SetVersionByBuild(1);
}

void CTbl2AsnApp::Init(void)
{

    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Prepare command line descriptions, inherit them from tbl2asn legacy application

    arg_desc->AddOptionalKey
        ("indir", "Directory", "Path to input files",
        CArgDescriptions::eInputFile);

    arg_desc->AddOptionalKey
        ("outdir", "Directory", "Path to results",
        CArgDescriptions::eOutputFile);

    arg_desc->AddFlag("E", "Recurse");

    arg_desc->AddDefaultKey
        ("x", "String", "Suffix", CArgDescriptions::eString, ".fsa");

    arg_desc->AddOptionalKey
        ("i", "InFile", "Single Input File",
        CArgDescriptions::eInputFile);

    arg_desc->AddOptionalKey(
        "o", "OutFile", "Single Output File",
        CArgDescriptions::eOutputFile);

    arg_desc->AddDefaultKey
        ("out-suffix", "String", "ASN.1 files suffix", CArgDescriptions::eString, ".sqn");

    arg_desc->AddOptionalKey("t", "InFile", "Template File",
        CArgDescriptions::eInputFile);

    arg_desc->AddDefaultKey
        ("a", "String", "File Type\n\
      a Any\n\
      r20u Runs of 20+ Ns are gaps, 100 Ns are unknown length\n\
      r20k Runs of 20+ Ns are gaps, 100 Ns are known length\n\
      r10u Runs of 10+ Ns are gaps, 100 Ns are unknown length\n\
      r10k Runs of 10+ Ns are gaps, 100 Ns are known length\n\
      s FASTA Set (s Batch, s1 Pop, s2 Phy, s3 Mut, s4 Eco,\n\
        s9 Small-genome)\n\
      d FASTA Delta, di FASTA Delta with Implicit Gaps\n\
      l FASTA+Gap Alignment (l Batch, l1 Pop, l2 Phy, l3 Mut, l4 Eco,\n\
        l9 Small-genome)\n\
      z FASTA with Gap Lines\n\
      e PHRAP/ACE\n\
      b ASN.1 for -M flag", CArgDescriptions::eString, "a");

    arg_desc->AddFlag("g", "Genomic Product Set");
    arg_desc->AddFlag("J", "Delayed Genomic Product Set ");                // done
    arg_desc->AddOptionalKey
        ("F", "String", "Feature ID Links\n\
      o By Overlap\n\
      p By Product", CArgDescriptions::eString);

    arg_desc->AddOptionalKey
        ("A", "String", "Accession", CArgDescriptions::eString);           // done
    arg_desc->AddOptionalKey
        ("C", "String", "Genome Center Tag", CArgDescriptions::eString);   // done
    arg_desc->AddOptionalKey
        ("n", "String", "Organism Name", CArgDescriptions::eString);       // done
    arg_desc->AddOptionalKey
        ("j", "String", "Source Qualifiers", CArgDescriptions::eString);   // done
    arg_desc->AddOptionalKey("src-file", "InFile", "Single source qualifiers file", CArgDescriptions::eInputFile); //done
    arg_desc->AddOptionalKey
        ("y", "String", "Comment", CArgDescriptions::eString);             // done
    arg_desc->AddOptionalKey
        ("Y", "InFile", "Comment File", CArgDescriptions::eInputFile);     // done
    arg_desc->AddOptionalKey
        ("D", "InFile", "Descriptors File", CArgDescriptions::eInputFile); // done
    arg_desc->AddOptionalKey
        ("f", "InFile", "Single 5 column table file or other annotations", CArgDescriptions::eInputFile);// done

    arg_desc->AddOptionalKey
        ("k", "String", "CDS Flags (combine any of the following letters)\n\
      c Annotate Longest ORF\n\
      r Allow Runon ORFs\n\
      m Allow Alternative Starts\n\
      k Set Conflict on Mismatch", CArgDescriptions::eString);

    arg_desc->AddOptionalKey
        ("V", "String", "Verification (combine any of the following letters)\n\
      v Validate with Normal Stringency\n\
      r Validate without Country Check\n\
      c BarCode Validation\n\
      b Generate GenBank Flatfile\n\
      g Generate Gene Report\n\
      t Validate with TSA Check", CArgDescriptions::eString);

    arg_desc->AddFlag("q", "Seq ID from File Name");      // done
    arg_desc->AddFlag("u", "GenProdSet to NucProtSet");   // done
    arg_desc->AddFlag("I", "General ID to Note");         // done

    arg_desc->AddOptionalKey("G", "String", "Alignment Gap Flags (comma separated fields, e.g., p,-,-,-,?,. )\n\
      n Nucleotide or p Protein,\n\
      Begin, Middle, End Gap Characters,\n\
      Missing Characters, Match Characters", CArgDescriptions::eString);

    arg_desc->AddFlag("S", "Smart Feature Annotation");

    arg_desc->AddOptionalKey("Q", "String", "mRNA Title Policy\n\
      s Special mRNA Titles\n\
      r RefSeq mRNA Titles", CArgDescriptions::eString);

    arg_desc->AddFlag("U", "Remove Unnecessary Gene Xref");
    arg_desc->AddFlag("L", "Force Local protein_id/transcript_id");
    arg_desc->AddFlag("T", "Remote Taxonomy Lookup");               // done
    arg_desc->AddFlag("P", "Remote Publication Lookup");            // done
    arg_desc->AddFlag("W", "Log Progress");                         // done
    arg_desc->AddFlag("K", "Save Bioseq-set");                      // done

    arg_desc->AddOptionalKey("H", "String", "Hold Until Publish\n\
      y Hold for One Year\n\
      mm/dd/yyyy", CArgDescriptions::eString); // done

    arg_desc->AddOptionalKey(
        "Z", "OutFile", "Discrepancy Report Output File", CArgDescriptions::eOutputFile);

    arg_desc->AddOptionalKey("c", "String", "Cleanup (combine any of the following letters)\n\
      b Basic cleanup (default)\n\
      e Extended cleanup\n\
      f Fix product names\n\
      s Add exception to short introns\n\
      w WGS cleanup\n\
      - avoid cleanup", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("z", "OutFile", "Cleanup Log File", CArgDescriptions::eOutputFile);

    arg_desc->AddOptionalKey("X", "String", "Extra Flags (combine any of the following letters)\n\
      A Automatic definition line generator\n\
      C Apply comments in .cmt files to all sequences\n\
      E Treat like eukaryota in the Discrepancy Report", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("N", "String", "Project Version Number", CArgDescriptions::eString); //done

    arg_desc->AddOptionalKey("w", "InFile", "Single Structured Comment File (overrides the use of -X C)", CArgDescriptions::eInputFile); //done
    arg_desc->AddOptionalKey("M", "String", "Master Genome Flags\n\
      n Normal\n\
      b Big Sequence\n\
      p Power Option\n\
      t TSA", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("l", "String", "Add type of evidence used to assert linkage across assembly gaps. May be used multiple times. Must be one of the following:\n\
      paired-ends\n\
      align-genus\n\
      align-xgenus\n\
      align-trnscpt\n\
      within-clone\n\
      clone-contig\n\
      map\n\
      strobe", CArgDescriptions::eString, CArgDescriptions::fAllowMultiple);  //done

    arg_desc->AddOptionalKey("gap-type", "String", "Set gap type for runs of Ns. Must be one of the following:\n\
      unknown\n\
      fragment\n\
      clone\n\
      short_arm\n\
      heterochromatin\n\
      centromere\n\
      telomere\n\
      repeat\n\
      contig\n\
      scaffold\n\
      other", CArgDescriptions::eString);  //done

    arg_desc->AddOptionalKey("m", "String", "Lineage to use for Discrepancy Report tests", CArgDescriptions::eString);

    // all new options are done
    arg_desc->AddFlag("type-aa", "Treat sequence as DNA");
    arg_desc->AddFlag("type-nuc", "Treat sequence as Nukleotide");
    arg_desc->AddOptionalKey("taxid", "Integer", "Organism taxonomy ID", CArgDescriptions::eInteger);
    arg_desc->AddOptionalKey("taxname", "String", "Taxonomy name", CArgDescriptions::eString);
    arg_desc->AddOptionalKey("strain-name", "String", "Strain name", CArgDescriptions::eString);
    arg_desc->AddOptionalKey("ft-url", "String", "FileTrack URL for the XML file retrieval", CArgDescriptions::eString);
    arg_desc->AddOptionalKey("ft-url-mod", "String", "FileTrack URL for the XML file base modifications", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("gaps-min", "Integer", "minimum run of Ns recognised as a gap", CArgDescriptions::eInteger);
    arg_desc->AddOptionalKey("gaps-unknown", "Integer", "exact number of Ns recognised as a gap with unknown length", CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey("min-threshold", "Integer", "minimum length of sequence", CArgDescriptions::eInteger);
    arg_desc->AddOptionalKey("fcs-file", "FileName", "FCS report file", CArgDescriptions::eInputFile);
    arg_desc->AddFlag("fcs-trim", "Trim FCS regions instead of annotate");
    arg_desc->AddFlag("avoid-submit", "Avoid submit block for optical map");
    arg_desc->AddFlag("map-use-loc", "Optical map: use locations instead of lengths of fragments");
    arg_desc->AddFlag("postprocess-pubs", "Postprocess pubs: convert authors to standard");
    arg_desc->AddOptionalKey("locus-tag-prefix", "String",  "Add prefix to locus tags in annotation files", CArgDescriptions::eString);
    arg_desc->AddFlag("euk", "Assume eukariote");
    arg_desc->AddOptionalKey("suspect-rules", "String", "Path to a file containing suspect rules set. Overrides environment variable PRODUCT_RULES_LIST", CArgDescriptions::eString);


    arg_desc->AddOptionalKey("logfile", "LogFile", "Error Log File", CArgDescriptions::eOutputFile);
    arg_desc->AddFlag("split-logs", "Create unique log file for each output file");

    CDataLoadersUtil::AddArgumentDescriptions(*arg_desc, default_loaders);

    // Program description
    string prog_description = "Converts files of various formats to ASN.1\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
        prog_description, false);

    // Pass argument descriptions to the application
    SetupArgDescriptions(arg_desc.release());
}

int CTbl2AsnApp::Run(void)
{
    const CArgs& args = GetArgs();

    Setup(args);

    m_context.m_split_log_files = args["split-logs"].AsBoolean();
    if (m_context.m_split_log_files && args["logfile"])
    {
        NCBI_THROW(CArgException, eConstraint,
            "-logfile cannot be used with -split-logs");
    }

    m_logger.Reset(new CTable2AsnLogger);
    CNcbiOstream* error_log = args["logfile"] ? &(args["logfile"].AsOutputFile()) : &NcbiCerr;
    m_logger->SetProgressOstream(error_log);
    SetDiagStream(error_log);
    m_context.m_logger = m_logger;
    m_logger->m_enable_log = args["W"].AsBoolean();
    m_validator.Reset(new CTable2AsnValidator(m_context));

    if (args["c"])
    {
        if (args["c"].AsString().find_first_not_of("-befws") != string::npos)
        {
            NCBI_THROW(CArgException, eConvert,
                "Unrecognized cleanup type " + args["c"].AsString());
        }

        m_context.m_cleanup = args["c"].AsString();
    }
    else
       m_context.m_cleanup = "b"; // always cleanup

    if (args["M"])
    {
        m_context.m_master_genome_flag = args["M"].AsString();
        m_context.m_delay_genprodset = true;
        m_context.m_GenomicProductSet = false;
        m_context.m_HandleAsSet = true;
        m_context.m_feature_links = 'p';
        m_context.m_cleanup += "fU";
        m_context.m_validate = "v";       
    }

    m_reader.reset(new CMultiReader(m_context));
    m_context.m_remote_updater.reset(new edit::CRemoteUpdater);

    if (args["fcs-file"])
    {
        m_fcs_reader.reset(new CForeignContaminationScreenReportReader(m_context));
        CRef<ILineReader> reader(ILineReader::New(args["fcs-file"].AsInputFile()));

        m_fcs_reader->LoadFile(*reader);
        m_context.m_fcs_trim = args["fcs-trim"];

        if (args["min-threshold"])
            m_context.m_minimal_sequence_length = args["min-threshold"].AsInteger();
    }

    if (args["n"])
        m_context.m_OrganismName = args["n"].AsString();

    if (args["y"])
        m_context.m_Comment = args["y"].AsString();
    else
    if (args["Y"])
    {
        CRef<ILineReader> reader(ILineReader::New(args["Y"].AsInputFile()));
        while (!reader->AtEOF())
        {
            reader->ReadLine();
            m_context.m_Comment += reader->GetCurrentLine();
            m_context.m_Comment += " ";
        }
    }
    NStr::TruncateSpacesInPlace(m_context.m_Comment);

    m_context.m_GenomicProductSet = args["g"].AsBoolean();
    m_context.m_NucProtSet = args["u"].AsBoolean();
    m_context.m_SetIDFromFile = args["q"].AsBoolean();

    if (args["U"] && args["U"].AsBoolean())
      m_context.m_cleanup += 'U';

    m_context.m_delay_genprodset = args["J"].AsBoolean();
    if (args["X"])
    {
        const string& extra = args["X"].AsString();
        m_context.m_flipped_struc_cmt = extra.find('C') != string::npos;
        m_context.m_disc_eucariote = extra.find('E') != string::npos;
    }

    if (args["m"]) {
        m_context.m_disc_lineage = args["m"].AsString();
    }

    if (m_context.m_delay_genprodset)
        m_context.m_GenomicProductSet = false;

    m_context.m_asn1_suffix = args["out-suffix"].AsString();

    m_context.m_copy_genid_to_note = args["I"].AsBoolean();
    m_context.m_save_bioseq_set = args["K"].AsBoolean();

    if (args["taxname"])
        m_context.m_OrganismName = args["taxname"].AsString();
    if (args["taxid"])
        m_context.m_taxid = args["taxid"].AsInteger();
    if (args["strain-name"])
        m_context.m_strain = args["strain-name"].AsString();
    if (args["ft-url"])
        m_context.m_ft_url = args["ft-url"].AsString();
    if (args["ft-url-mod"])
        m_context.m_ft_url_mod = args["ft-url-mod"].AsString();
    if (args["A"])
        m_context.m_accession.Reset(new CSeq_id(args["A"].AsString()));
    if (args["j"])
    {       
        m_context.m_source_mods = args["j"].AsString();
    }
    if (args["src-file"])
        m_context.m_single_source_qual_file = args["src-file"].AsString();

    if (args["F"])
    {
        char f_arg = args["F"].AsString()[0];

        if (args["F"].AsString().length() != 1 || 
            !(f_arg == 'f' || f_arg == 'p'))
        {
            NCBI_THROW(CArgException, eConvert,
                "Unrecognized feature link type " + args["F"].AsString());
        }
        m_context.m_feature_links = f_arg;
    }
    if (args["f"])
        m_context.m_single_annot_file = args["f"].AsString();

    if (args["w"])
        m_context.m_single_structure_cmt = args["w"].AsString();

    m_context.m_RemotePubLookup = args["P"].AsBoolean();
    if (!m_context.m_RemotePubLookup) // those are always postprocessed
        m_context.m_postprocess_pubs = args["postprocess-pubs"].AsBoolean();

    m_context.m_RemoteTaxonomyLookup = args["T"].AsBoolean();

    m_context.m_avoid_submit_block = args["avoid-submit"].AsBoolean();

    m_context.m_optmap_use_locations = args["map-use-loc"].AsBoolean();

    if (args["type-aa"] && args["type-nuc"])
    {
        NCBI_THROW(CArgException, eConstraint, "type-aa flag cannot be used with type-nuc");
    }
    m_context.m_handle_as_aa = args["type-aa"].AsBoolean();
    m_context.m_handle_as_nuc = args["type-nuc"].AsBoolean();

    if (args["a"])
    {
        const string& a_arg = args["a"].AsString();
        if (a_arg.length() > 2 && a_arg[0] == 'r')
        {
            switch (*(a_arg.end() - 1))
            {
            case 'u':
                m_context.m_gap_Unknown_length = 100; // yes, it's hardcoded value
                // do not put break statement here
            case 'k':
                m_context.m_gapNmin = NStr::StringToUInt(a_arg.substr(1, a_arg.length() - 2));
                break;
            default:
                // error
                break;
            }
        }
        else
        {
            if (a_arg == "s")
            {
                m_context.m_HandleAsSet = true;
            }
            else
            if (a_arg == "s4")
            {
                m_context.m_HandleAsSet = true;
                m_context.m_ecoset = true;
            }
            else
            if (a_arg == "di")
            {
                m_context.m_di_fasta = true;
            }
        }
    }
    if (args["gaps-min"])
        m_context.m_gapNmin = args["gaps-min"].AsInteger();
    if (args["gaps-unknown"])
        m_context.m_gap_Unknown_length = args["gaps-unknown"].AsInteger();

    if (args["l"])
    {
        const CEnumeratedTypeValues::TNameToValue&
            linkage_evidence_to_value_map = CLinkage_evidence::GetTypeInfo_enum_EType()->NameToValue();

        ITERATE(CArgValue::TStringArray, arg_it, args["l"].GetStringList())
        {
            CEnumeratedTypeValues::TNameToValue::const_iterator it = linkage_evidence_to_value_map.find(*arg_it);
            if (it == linkage_evidence_to_value_map.end())
            {
                NCBI_THROW(CArgException, eConvert,
                    "Unrecognized linkage evidence " + *arg_it);
            }
            else
            {
                m_context.m_gap_evidences.insert(it->second);
                m_context.m_gap_type = CSeq_gap::eType_scaffold; // for compatibility with tbl2asn
            }
        }
    }

    if (args["gap-type"])
    {
        const CEnumeratedTypeValues::TNameToValue&
            linkage_evidence_to_value_map = CSeq_gap::GetTypeInfo_enum_EType()->NameToValue();

        CEnumeratedTypeValues::TNameToValue::const_iterator it = linkage_evidence_to_value_map.find(args["gap-type"].AsString());
        if (it == linkage_evidence_to_value_map.end())
        {
            NCBI_THROW(CArgException, eConvert,
                "Unrecognized gap type " + args["gap-type"].AsString());
        }
        else
        {
            m_context.m_gap_type = it->second;
        }

    }

    if (m_context.m_gapNmin < 0)
        m_context.m_gapNmin = 0;

    if (m_context.m_gapNmin == 0 || m_context.m_gap_Unknown_length < 0)
        m_context.m_gap_Unknown_length = 0;

    if (args["k"])
        m_context.m_find_open_read_frame = args["k"].AsString();

    if (args["H"])
    {
        try
        {
            string sdate = args["H"].AsString();
            if (sdate[0] == '\'' && sdate.length() > 0 && sdate[sdate.length() - 1] == '\'')
            {
                sdate.erase(0, 1);
                sdate.erase(sdate.length() - 1, 1);
            }

            m_context.m_HoldUntilPublish = CTime(sdate, "M/D/Y");
        }
        catch (const CException &)
        {
            int years = NStr::StringToInt(args["H"].AsString());
            m_context.m_HoldUntilPublish.SetCurrent();
            m_context.m_HoldUntilPublish.SetYear(m_context.m_HoldUntilPublish.Year() + years);
        }
    }

    if (args["N"])
        m_context.m_ProjectVersionNumber = args["N"].AsString();

    if (args["C"])
    {
        m_context.m_genome_center_id = args["C"].AsString();
        if (!m_context.m_ProjectVersionNumber.empty())
            m_context.m_genome_center_id += m_context.m_ProjectVersionNumber;
    }

    if (args["V"])
        m_context.m_validate = args["V"].AsString();

    if (args["locus-tag-prefix"])
        m_context.m_locus_tag_prefix = args["locus-tag-prefix"].AsString();

    if (m_context.m_HandleAsSet)
    {
        if (m_context.m_GenomicProductSet)
            NCBI_THROW(CArgException, eConstraint, "-s flag cannot be used with -d, -e, -g, -l or -z");
    }

    // Designate where do we output files: local folder, specified folder or a specific single output file
    if (args["outdir"])
            m_context.m_ResultsDirectory = CDir::AddTrailingPathSeparator(args["outdir"].AsString());

    if (args["o"])
    {
        m_context.m_output_filename = args["o"].AsString();
        m_context.m_output = &args["o"].AsOutputFile();
    }
    else
    {
        if (args["outdir"])
        {
            CDir outputdir(m_context.m_ResultsDirectory);
            if (!IsDryRun())
                if (!outputdir.Exists())
                    outputdir.Create();
        }
    }

    if (args["Z"])
    {
        m_context.m_discrepancy_file = args["Z"].AsString();
    }

    m_context.m_eukariote = args["euk"].AsBoolean();

    if (m_context.m_cleanup.find('f') != string::npos)
        m_context.m_use_hypothetic_protein = true;

    if (args["suspect-rules"])
        m_context.m_suspect_rules.SetFilename(args["suspect-rules"].AsString());

    try
    {
        if (args["t"])
        {
            m_reader->LoadTemplate(m_context, args["t"].AsString());
        }
    }
    catch (const CException&)
    {
        m_logger->PutError(*auto_ptr<CLineError>(
            CLineError::Create(CLineError::eProblem_GeneralParsingError, eDiag_Error,
            "", 0, "", "", "",
            "Error loading template file")));
    }

    try
    {
        if (args["D"])
        {
            m_reader->LoadDescriptors(args["D"].AsString(), m_context.m_descriptors);
        }
    }
    catch (const CException&)
    {
        m_logger->PutError(*auto_ptr<CLineError>(
            CLineError::Create(CLineError::eProblem_GeneralParsingError, eDiag_Error,
            "", 0, "", "", "",
            "Error loading descriptors file")));
    }

    if (m_logger->Count() == 0)
    try
    {
        // Designate where do we get input: single file or a folder or folder structure
        if (args["i"])
        {
            m_context.m_current_file = args["i"].AsString();
            ProcessOneFile();
        }
        else
        if (args["indir"])
        {
            CDir directory(args["indir"].AsString());
            if (directory.Exists())
            {
                CMaskFileName masks;
                masks.Add("*" + args["x"].AsString());

                ProcessOneDirectory(directory, masks, args["E"].AsBoolean());
            }
        }
        if (m_validator->TotalErrors() > 0)
        {
            string outputfile = GenerateOutputFilename(".stats");
            CNcbiOfstream val_stats(outputfile.c_str());
            m_validator->ReportErrorStats(val_stats);
        }
    }
    catch (const CBadResiduesException& e)
    {
        int line = 0;
        ILineError::TVecOfLines lines;
        if (e.GetBadResiduePositions().m_BadIndexMap.size() == 1)
        {
            line = e.GetBadResiduePositions().m_BadIndexMap.begin()->first;
        }
        else
        {
            lines.reserve(e.GetBadResiduePositions().m_BadIndexMap.size());
            ITERATE(CBadResiduesException::SBadResiduePositions::TBadIndexMap, it, e.GetBadResiduePositions().m_BadIndexMap)
            {
                lines.push_back(it->first);
            }
        }
        auto_ptr<CLineError> le(
            CLineError::Create(CLineError::eProblem_GeneralParsingError, eDiag_Error,
            e.GetBadResiduePositions().m_SeqId->AsFastaString(),
            line, "", "", "", e.GetMsg(), lines));
        m_logger->PutError(*le);
    }
    catch (const CException& e)
    {
        m_logger->PutError(*auto_ptr<CLineError>(
            CLineError::Create(CLineError::eProblem_GeneralParsingError, eDiag_Error,
            "", 0, "", "", "",
            e.GetMsg())));
    }

    if (m_logger->Count() == 0)
        return 0;
    else
    {
        m_logger->Dump();
        //m_logger->DumpAsXML(NcbiCout);

        size_t errors = m_logger->LevelCount(eDiag_Critical) +
            m_logger->LevelCount(eDiag_Error) +
            m_logger->LevelCount(eDiag_Fatal);
        // all errors reported as failure
        if (errors > 0)
            return 1;

        // only warnings reported as 2
        if (m_logger->LevelCount(eDiag_Warning)>0)
            return 2;

        // otherwise it's ok
        return 0;
    }
}


CRef<CScope> CTbl2AsnApp::GetScope(void)
{
    return m_context.m_scope;
}

void CTbl2AsnApp::ProcessOneFile(CRef<CSerialObject>& result)
{
    CRef<CSeq_entry> entry;
    CRef<CSeq_submit> submit;

    m_context.m_avoid_orf_lookup = false;
    bool avoid_submit_block = false;
    bool do_dates = false;

    if (m_context.m_current_file.substr(m_context.m_current_file.length() - 4).compare(".xml") == 0)
    {
        avoid_submit_block = m_context.m_avoid_submit_block;
        COpticalxml2asnOperator op;
        entry = op.LoadXML(m_context.m_current_file, m_context);
        if (entry.IsNull())
            return;
    }
    else
    {
		CFormatGuess::EFormat format = m_reader->LoadFile(m_context.m_current_file, entry, submit);
        if (m_fcs_reader.get())
        {
            m_fcs_reader->PostProcess(*entry);
        }
        entry->Parentize();

        if (m_context.m_SetIDFromFile)
        {
            m_context.SetSeqId(*entry);
        }

        switch (format)
        {
        case CFormatGuess::eTextASN:
        case CFormatGuess::eBinaryASN:
            do_dates = true;
            break;
        default:
            break;
        }
    }

    m_context.ApplyAccession(*entry);

    if (!IsDryRun())
    {
        m_context.m_suspect_rules.m_report_ostream.reset();
        m_context.m_suspect_rules.m_fixed_product_report_filename = GenerateOutputFilename(".fixedproducts");
        CFile(m_context.m_suspect_rules.m_fixed_product_report_filename).Remove();
    }
    m_context.ApplyFileTracks(*entry);

    if (m_context.m_descriptors.NotNull())
        m_reader->ApplyDescriptors(*entry, *m_context.m_descriptors);

    m_reader->ApplyAdditionalProperties(*entry);

    ProcessSecretFiles(*entry);

    CFeatureTableReader fr(m_context);
    // this may convert seq into seq-set
    if (!m_context.m_avoid_orf_lookup && !m_context.m_find_open_read_frame.empty())
        fr.FindOpenReadingFrame(*entry);

    if (m_context.m_RemoteTaxonomyLookup)
    {
        m_context.m_remote_updater->UpdateOrgFromTaxon(m_context.m_logger, *entry);
    }
    else
    {
        VisitAllBioseqs(*entry, CTable2AsnContext::UpdateTaxonFromTable);
    }

    fr.m_replacement_protein = m_replacement_proteins;
    //if (!m_context.m_ecoset) 
       fr.MergeCDSFeatures(*entry);

    entry->Parentize();
    
    if (m_possible_proteins.NotEmpty())
        fr.AddProteins(*m_possible_proteins, *entry);

    if (m_context.m_copy_genid_to_note)
        m_context.CopyFeatureIdsToComments(*entry);

    if (m_context.m_GenomicProductSet)
    {
        //fr.ConvertSeqIntoSeqSet(*entry, false);
    }

    if (m_context.m_HandleAsSet)
    {
        fr.ConvertNucSetToSet(entry);
    }

    if (do_dates)
    {
        // if create-date exists apply update date
        m_context.ApplyCreateUpdateDates(*entry);
    }

    // this methods do not remove entry nor change it. But create 'result' object which either
    // equal to 'entry' or contain reference to 'entry'.
    if (avoid_submit_block)
        result = m_context.CreateSeqEntryFromTemplate(entry);
    else
        result = m_context.CreateSubmitFromTemplate(entry, submit);

    //m_context.MakeGenomeCenterId(*entry);

    CSeq_entry_EditHandle entry_edit_handle = m_context.m_scope->AddTopLevelSeqEntry(*entry).GetEditHandle();

    fr.MakeGapsFromFeatures(entry_edit_handle);

    if (m_context.m_delay_genprodset)
    {
        VisitAllFeatures(entry_edit_handle, &m_context.RenameProteinIdsQuals);
    }
    else
    {
        VisitAllFeatures(entry_edit_handle, &m_context.RemoveProteinIdsQuals);
    }

    if (m_context.m_RemotePubLookup)
    {
        m_context.m_remote_updater->UpdatePubReferences(*entry);
    }
    if (m_context.m_postprocess_pubs)
    {
        edit::CRemoteUpdater::PostProcessPubs(*entry);
    }

    if (avoid_submit_block)
    {
        // we need to fix cit-sub date
        //COpticalxml2asnOperator::UpdatePubDate(*result);
    }

    // make asn.1 look nicier
    edit::SortSeqDescr(*entry);

    if (m_context.m_cleanup.find('-') == string::npos)
    {
       m_validator->Cleanup(entry_edit_handle, m_context.m_cleanup);
    }

    CFeatureTableReader ftr(m_context);
    ftr.ChangeDeltaProteinToRawProtein(*entry);

    if (!IsDryRun())
    {
        m_validator->UpdateECNumbers(entry_edit_handle, GenerateOutputFilename(".ecn"), m_context.m_ecn_numbers_ostream);

        if (!m_context.m_validate.empty())
        {
            m_validator->Validate(submit, entry, m_context.m_validate, GenerateOutputFilename(".val"));
        }

        if (!m_context.m_discrepancy_file.empty())
        {
            m_validator->ReportDiscrepancies(submit.Empty() ? (CSerialObject&)*entry : (CSerialObject&)*submit, *m_context.m_scope, m_context.m_disc_eucariote, m_context.m_disc_lineage);
        }
    }
}

string CTbl2AsnApp::GenerateOutputFilename(const CTempString& ext) const
{
    string dir;
    string outputfile;
    string base;

    if (m_context.m_output_filename.empty())
    {
        CDirEntry::SplitPath(m_context.m_current_file, &dir, &base, 0);

        outputfile = m_context.m_ResultsDirectory.empty() ? dir : m_context.m_ResultsDirectory;
    }
    else
    {
        CDirEntry::SplitPath(m_context.m_output_filename, &dir, &base, 0);
        if (m_context.m_output_filename == "-" || dir == "/dev" ) {
           CDirEntry::SplitPath(m_context.m_current_file, &dir, &base, 0);
           outputfile = m_context.m_ResultsDirectory.empty() ? dir : m_context.m_ResultsDirectory;
        }
        else {
          outputfile = dir;
        }
    }
    outputfile += base;
    outputfile += ext;

    return outputfile;
}

void CTbl2AsnApp::ProcessOneFile()
{
    m_context.m_scope->ResetDataAndHistory();
    if (m_context.m_split_log_files)
        m_context.m_logger->ClearAll();

    CFile file(m_context.m_current_file);
    if (!file.Exists())
    {
        m_logger->PutError(*auto_ptr<CLineError>(
            CLineError::Create(ILineError::eProblem_GeneralParsingError, eDiag_Error, "", 0,
                "File " + m_context.m_current_file + " does not exists")));
            return;
        }

    CFile log_name;
    if (!IsDryRun() && m_context.m_split_log_files)
    {
        log_name = GenerateOutputFilename(".log");
        CNcbiOstream* error_log = new CNcbiOfstream(log_name.GetPath().c_str());
        m_logger->SetProgressOstream(error_log);
        SetDiagStream(error_log);
    }

    try
    {
        CRef<CSerialObject> obj;
        ProcessOneFile(obj);

        if (!IsDryRun() && obj.NotEmpty())
        {
            const CSerialObject* to_write = obj;
            if (m_context.m_save_bioseq_set)
            {
                if (obj->GetThisTypeInfo()->IsType(CSeq_entry::GetTypeInfo()))
                {
                    const CSeq_entry* se = (const CSeq_entry*)obj.GetPointer();
                    if (se->IsSet())
                        to_write = &se->GetSet();
                }
            }

            CFile local_file;
            CNcbiOstream* output(0);

            if (m_context.m_output == 0)
            {
                local_file = GenerateOutputFilename(m_context.m_asn1_suffix);
                output = new CNcbiOfstream(local_file.GetPath().c_str());
                m_context.m_ecn_numbers_ostream.release();
            }
            else
            {
                output = m_context.m_output;
            }

            try
            {
                m_reader->WriteObject(*to_write, *output);
                if (m_context.m_output == 0)
                    delete output;
                output = 0;
            }
            catch (...)
            {
                // if something goes wrong - remove the partial output to avoid confuse
                if (m_context.m_output == 0)
                {
                    local_file.Remove();
                    delete output;
                }
                throw;
            }
        }

        if (!log_name.GetPath().empty())
        {
            m_logger->SetProgressOstream(&NcbiCout);
        }
    }
    catch (...)
    {
        if (!log_name.GetPath().empty())
        {
            m_logger->SetProgressOstream(&NcbiCout);
        }

        throw;
    }
}

bool CTbl2AsnApp::ProcessOneDirectory(const CDir& directory, const CMask& mask, bool recurse)
{
    CDir::TEntries* e = directory.GetEntriesPtr("*", CDir::fCreateObjects | CDir::fIgnoreRecursive);
    auto_ptr<CDir::TEntries> entries(e);

    for (CDir::TEntries::const_iterator it = e->begin(); it != e->end(); it++)
    {
        // first process files and then recursivelly access other folders
        if (!(*it)->IsDir())
        {
            if (mask.Match((*it)->GetPath()))
            {
                m_context.m_current_file = (*it)->GetPath();
                ProcessOneFile();
            }
        }
        else
            if (recurse)
            {
            ProcessOneDirectory(**it, mask, recurse);
            }
    }

    return true;
}

void CTbl2AsnApp::Setup(const CArgs& args)
{
    // Setup application registry and logs for CONNECT library
    CORE_SetLOG(LOG_cxx2c());
    CORE_SetREG(REG_cxx2c(&GetConfig(), false));
    // Setup MT-safety for CONNECT library
    // CORE_SetLOCK(MT_LOCK_cxx2c());

    // Create object manager and scope

    m_context.m_ObjMgr = CObjectManager::GetInstance();
    CDataLoadersUtil::SetupObjectManager(args, *m_context.m_ObjMgr, default_loaders);

    m_context.m_scope.Reset(new CScope(*m_context.m_ObjMgr));
    m_context.m_scope->AddDefaults();
}

/*
tbl��� 5-column Feature Table
.src�� tab-delimited table with source qualifiers
.qvl�� PHRAP/PHRED/consed quality scores
.dsc� One or more descriptors in ASN.1 format
.cmt� Tab-delimited file for structured comment
.pep� Replacement proteins for coding regions on this sequence, use to mark conflicts
.rna�� Replacement mRNA sequences for RNA editing
.prt��� Proteins for suggest intervals
*/
void CTbl2AsnApp::ProcessSecretFiles(CSeq_entry& result)
{
    string dir;
    string base;
    string ext;
    CDirEntry::SplitPath(m_context.m_current_file, &dir, &base, &ext);

    string name = dir + base;

    ProcessSRCFileAndQualifiers(name + ".src", result, ext == ".xml" ? (name + ".xml") : "");

    ProcessQVLFile(name + ".qvl", result);
    ProcessDSCFile(name + ".dsc", result);
    ProcessCMTFile(name + ".cmt", result, m_context.m_flipped_struc_cmt);
    ProcessCMTFile(m_context.m_single_structure_cmt, result, m_context.m_flipped_struc_cmt);
    ProcessPEPFile(name + ".pep", result);
    ProcessRNAFile(name + ".rna", result);
    ProcessPRTFile(name + ".prt", result);

    if (!m_context.m_single_annot_file.empty())
    {
        ProcessAnnotFile(m_context.m_single_annot_file, result);
    }
    else
    {
        ProcessAnnotFile(name + ".tbl", result);
        ProcessAnnotFile(name + ".gff", result);
        ProcessAnnotFile(name + ".gff3", result);
        ProcessAnnotFile(name + ".gff2", result);
        ProcessAnnotFile(name + ".gtf", result);
    }
}

void CTbl2AsnApp::ProcessSRCFileAndQualifiers(const string& pathname, CSeq_entry& result, const string& opt_map_xml)
{ 
    CSourceQualifiersReader src_reader(&m_context);
    if (src_reader.LoadSourceQualifiers(pathname, opt_map_xml) || !m_context.m_source_mods.empty())
       src_reader.ProcessSourceQualifiers(result, opt_map_xml);
}

void CTbl2AsnApp::ProcessQVLFile(const string& pathname, CSeq_entry& result)
{
    CFile file(pathname);
    if (!file.Exists() || file.GetLength() == 0) return;
}

void CTbl2AsnApp::ProcessDSCFile(const string& pathname, CSeq_entry& result)
{
    CFile file(pathname);
    if (!file.Exists() || file.GetLength() == 0) return;

    CRef<CSeq_descr> descr;
    m_reader->LoadDescriptors(pathname, descr);
    m_reader->ApplyDescriptors(result, *descr);
}

void CTbl2AsnApp::ProcessCMTFile(const string& pathname, CSeq_entry& result, bool byrows)
{
    CFile file(pathname);
    if (!file.Exists() || file.GetLength() == 0) return;

    CRef<ILineReader> reader(ILineReader::New(pathname));

    CTable2AsnStructuredCommentsReader cmt_reader(m_logger);

    if (byrows)
        cmt_reader.ProcessCommentsFileByRows(*reader, result);
    else
        cmt_reader.ProcessCommentsFileByCols(*reader, result);
}

void CTbl2AsnApp::ProcessPEPFile(const string& pathname, CSeq_entry& entry)
{
    CFile file(pathname);
    if (!file.Exists() || file.GetLength() == 0) return;

    CRef<ILineReader> reader(ILineReader::New(pathname));

    CFeatureTableReader peps(m_context);

    m_replacement_proteins = peps.ReadProtein(*reader);
}

void CTbl2AsnApp::ProcessRNAFile(const string& pathname, CSeq_entry& entry)
{
    CFile file(pathname);
    if (!file.Exists() || file.GetLength() == 0) return;
}

void CTbl2AsnApp::ProcessPRTFile(const string& pathname, CSeq_entry& entry)
{
    CFile file(pathname);
    if (!file.Exists() || file.GetLength() == 0) return;

    CRef<ILineReader> reader(ILineReader::New(pathname));

    CFeatureTableReader prts(m_context);

    m_possible_proteins = prts.ReadProtein(*reader);
}

void CTbl2AsnApp::ProcessAnnotFile(const string& pathname, CSeq_entry& entry)
{
    CFile file(pathname);
    if (!file.Exists() || file.GetLength() == 0) return;

	m_reader->LoadAnnot(entry, pathname);
}

/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[])
{
    return CTbl2AsnApp().AppMain(argc, argv, 0, eDS_Default, "table2asn.conf");
}

