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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   Sample test application for BAM reader and comparison NCBI BAM reader
 *   with SAMTOOLS library.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_system.hpp>
#include <sra/readers/bam/bamread.hpp>
#include <sra/readers/ncbi_traces_path.hpp>

#include <objects/general/general__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/seqalign/seqalign__.hpp>

#include <objtools/readers/idmapper.hpp>
#include <objtools/simple/simple_om.hpp>
#include "samtools.hpp"

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////
//  CBAMCompareApp::


class CBAMCompareApp : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test


#define BAM_DIR1 "/1000genomes/ftp/data"
#define BAM_DIR2 "/1000genomes/ftp/phase1/data"
#define BAM_DIR3 "/1kg_pilot_data/data"
#define BAM_DIR4 "/1000genomes2/ftp/data"
#define BAM_DIR5 "/1000genomes3/ftp/data"

#define BAM_FILE1 "NA19240.mapped.SOLID.bfast.YRI.exome.20111114.bam"
#define BAM_FILE2 "NA19240.chromMT.LS454.ssaha2.YRI.high_coverage.20100311.bam"
#define BAM_FILE3 "NA19240.chromMT.SLX.SRP000032.2009_04.bam"
#define BAM_FILE4 "NA19240.chrom3.SLX.SRP000032.2009_04.bam"

void CBAMCompareApp::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "CArgDescriptions demo program");

    arg_desc->AddOptionalKey("dir", "Dir",
                             "BAM files files directory",
                             CArgDescriptions::eString);
    arg_desc->AddDefaultKey("file", "File",
                            "BAM file name",
                            CArgDescriptions::eString,
                            BAM_FILE1);
    arg_desc->AddFlag("no_index", "Access BAM file without index");
    arg_desc->AddFlag("refseq_table", "Dump RefSeq table");
    arg_desc->AddFlag("no_short", "Do not collect short ids");
    arg_desc->AddFlag("range_only", "Collect only the range on RefSeq");
    arg_desc->AddFlag("no_ref_size", "Assume zero ref_size");

    arg_desc->AddOptionalKey("mapfile", "MapFile",
                             "IdMapper config filename",
                             CArgDescriptions::eInputFile);
    arg_desc->AddDefaultKey("genome", "Genome",
                            "UCSC build number",
                            CArgDescriptions::eString, "");

    arg_desc->AddOptionalKey("refseq", "RefSeqId",
                             "RefSeq id",
                             CArgDescriptions::eString);
    arg_desc->AddDefaultKey("refpos", "RefSeqPos",
                            "RefSeq position",
                            CArgDescriptions::eInteger,
                            "0");
    arg_desc->AddOptionalKey("refposs", "RefSeqPoss",
                            "RefSeq positions",
                            CArgDescriptions::eString);
    arg_desc->AddOptionalKey("refwindow", "RefSeqWindow",
                             "RefSeq window",
                             CArgDescriptions::eInteger);
    arg_desc->AddOptionalKey("refend", "RefSeqEnd",
                             "RefSeq last position",
                             CArgDescriptions::eInteger);
    arg_desc->AddOptionalKey("q", "Query",
                             "Query coordinates in form chr1:100-10000",
                             CArgDescriptions::eString);
    arg_desc->AddDefaultKey("limit_count", "LimitCount",
                            "Number of BAM entries to read (0 - unlimited)",
                            CArgDescriptions::eInteger,
                            "100000");
    arg_desc->AddOptionalKey("check_id", "CheckId",
                             "Compare alignments with the specified sequence",
                             CArgDescriptions::eString);
    arg_desc->AddFlag("raw", "Compare with raw index version");
    arg_desc->AddFlag("make_seq_entry", "Generated Seq-entries");
    arg_desc->AddFlag("print_seq_entry", "Print generated Seq-entry");
    arg_desc->AddFlag("ignore_errors", "Ignore errors in individual entries");
    arg_desc->AddFlag("verbose", "Print BAM data");
    arg_desc->AddFlag("short_id_conflicts", "Check for short id conflicts");

    arg_desc->AddDefaultKey("o", "OutputFile",
                            "Output file of ASN.1",
                            CArgDescriptions::eOutputFile,
                            "-");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



/////////////////////////////////////////////////////////////////////////////
//  Run test
/////////////////////////////////////////////////////////////////////////////

inline bool Matches(char ac, char rc)
{
    static int bits[26] = {
        0x1, 0xE, 0x2, 0xD,
        0x0, 0x0, 0x4, 0xB,
        0x0, 0x0, 0xC, 0x0,
        0x3, 0xF, 0x0, 0x0,
        0x0, 0x5, 0x6, 0x8,
        0x0, 0x7, 0x9, 0x0,
        0xA, 0x0
    };
    
    //if ( ac == rc || ac == 'N' || rc == 'N' ) return true;
    if ( ac < 'A' || ac > 'Z' || rc < 'A' || rc > 'Z' ) return false;
    if ( bits[ac-'A'] & bits[rc-'A'] ) return true;
    return false;
}

string GetShortSeqData(const CBamAlignIterator& it)
{
    string ret;
    string src = it.GetShortSequence();
    //bool minus = it.IsSetStrand() && IsReverse(it.GetStrand());
    TSeqPos src_pos = it.GetCIGARPos();

    string CIGAR = it.GetCIGAR();
    const char* ptr = CIGAR.data();
    const char* end = ptr + CIGAR.size();
    char type;
    TSeqPos len;
    while ( ptr != end ) {
        type = *ptr;
        for ( len = 0; ++ptr != end; ) {
            char c = *ptr;
            if ( c >= '0' && c <= '9' ) {
                len = len*10+(c-'0');
            }
            else {
                break;
            }
        }
        if ( type == 'M' || type == '=' || type == 'X' ) {
            // match
            for ( TSeqPos i = 0; i < len; ++i ) {
                ret += src[src_pos++];
            }
        }
        else if ( type == 'I' || type == 'S' ) {
            // insert
            src_pos += len;
        }
        else if ( type == 'D' || type == 'N' ) {
            ret.append(len, 'N');
        }
    }
    return ret;
}


char Complement(char c)
{
    switch ( c ) {
    case 'N': return 'N';
    case 'A': return 'T';
    case 'T': return 'A';
    case 'C': return 'G';
    case 'G': return 'C';
    case 'B': return 'V';
    case 'V': return 'B';
    case 'D': return 'H';
    case 'H': return 'D';
    case 'K': return 'M';
    case 'M': return 'K';
    case 'R': return 'Y';
    case 'Y': return 'R';
    case 'S': return 'S';
    case 'W': return 'W';
    default: Abort();
    }
}


string Reverse(const string& s)
{
    size_t size = s.size();
    string r(size, ' ');
    for ( size_t i = 0; i < size; ++i ) {
        r[i] = Complement(s[size-1-i]);
    }
    return r;
}


int CBAMCompareApp::Run(void)
{
    // Get arguments
    const CArgs& args = GetArgs();

    int ret_code = 0;

    string path;

    vector<string> dirs;
    if ( args["dir"] ) {
        dirs.push_back(args["dir"].AsString());
    }
    else {
        vector<string> reps;
        NStr::Split(NCBI_TEST_BAM_FILE_PATH, ":", reps);
        ITERATE ( vector<string>, it, reps ) {
            dirs.push_back(CFile::MakePath(*it, BAM_DIR1));
            dirs.push_back(CFile::MakePath(*it, BAM_DIR2));
            dirs.push_back(CFile::MakePath(*it, BAM_DIR3));
            dirs.push_back(CFile::MakePath(*it, BAM_DIR4));
            dirs.push_back(CFile::MakePath(*it, BAM_DIR5));
        }
    }

    ITERATE ( vector<string>, it, dirs ) {
        string dir = *it;
        if ( !dir.empty() && !NStr::EndsWith(dir, "/") ) {
            dir += '/';
        }
        string file = args["file"].AsString();
        path = file;
        if ( CFile(path).Exists() ) {
            break;
        }
        path = dir + file;
        if ( CFile(path).Exists() ) {
            break;
        }
        SIZE_TYPE p1 = file.rfind('/');
        if ( p1 == NPOS ) {
            p1 = 0;
        }
        else {
            p1 += 1;
        }
        SIZE_TYPE p2 = file.find('.', p1);
        if ( p2 != NPOS ) {
            path = dir + file.substr(p1, p2-p1) + "/alignment/" + file;
            if ( CFile(path).Exists() ) {
                break;
            }
            path = dir + file.substr(p1, p2-p1) + "/exome_alignment/" + file;
            if ( CFile(path).Exists() ) {
                break;
            }
        }
        path.clear();
    }
#if 0
    if ( !CFile(path).Exists() ) {
        NcbiCerr << "\nNCBI_UNITTEST_SKIPPED "
                 << "Data+file+"<<args["file"].AsString()<<"+not+found."
                 << NcbiEndl;
        return 1;
    }
#endif
    if ( !CFile(path).Exists() ) {
        ERR_POST("Data file "<<args["file"].AsString()<<" not found.");
    }

    CSeqVector sv;
    if ( args["check_id"] ) {
        sv = CSimpleOM::GetSeqVector(args["check_id"].AsString());
        if ( sv.empty() ) {
            ERR_POST(Fatal<<"Cannot get check sequence: "<<args["check_id"].AsString());
        }
        sv.SetCoding(CSeq_data::e_Iupacna);
    }

    CNcbiOstream& out = args["o"].AsOutputFile();

    out << "File: " << path << NcbiEndl;
    CBamMgr mgr;
    CBamDb bam_db, bam_db2;
    if ( args["no_index"] ) {
        if ( !args["raw"] ) {
            bam_db = CBamDb(mgr, path);
        }
        else {
            bam_db = CBamDb(mgr, path, CBamDb::eUseAlignAccess);
            bam_db2 = CBamDb(mgr, path, CBamDb::eUseRawIndex);
        }
    }
    else {
        if ( !args["raw"] ) {
            bam_db = CBamDb(mgr, path, path+".bai");
        }
        else {
            bam_db = CBamDb(mgr, path, path+".bai", CBamDb::eUseAlignAccess);
            bam_db2 = CBamDb(mgr, path, path+".bai", CBamDb::eUseRawIndex);
        }
    }

    AutoPtr<IIdMapper> mapper;
    if ( args["mapfile"] ) {
        mapper.reset(new CIdMapperConfig(args["mapfile"].AsInputFile(),
                                         args["genome"].AsString(),
                                         false));
    }
    else if ( !args["genome"].AsString().empty() ) {
        LOG_POST("Genome: "<<args["genome"].AsString());
        mapper.reset(new CIdMapperBuiltin(args["genome"].AsString(),
                                          false));
    }
    if ( mapper ) {
        if ( bam_db2 ) {
            bam_db2.SetIdMapper(mapper.get(), eNoOwnership);
        }
        bam_db.SetIdMapper(mapper.release(), eTakeOwnership);
    }

    bool print_refseqs = args["refseq_table"];
    if ( bam_db2 ) {
        if ( bam_db.GetHeaderText() != bam_db2.GetHeaderText() ) {
            ret_code = 1;
            out << "Raw: Header: " << bam_db2.GetHeaderText()
                << endl
                << "VDB: Header: " << bam_db.GetHeaderText()
                << endl;
        }
    }
    if ( bam_db2 || print_refseqs ) {
        CBamRefSeqIterator it2;
        if ( bam_db2 ) {
            it2 = CBamRefSeqIterator(bam_db2);
        }
        for ( CBamRefSeqIterator it(bam_db); it; ++it ) {
            if ( print_refseqs ) {
                out << "RefSeq: " << it.GetRefSeqId()
                    << " = " << it.GetRefSeq_id()->AsFastaString()
                    << " " << it.GetLength()
                    << endl;
            }
            if ( bam_db2 ) {
                if ( !it2 ) {
                    ret_code = 1;
                    out << "Raw: no reference"
                        << endl;
                }
                else {
                    if ( it2.GetRefSeqId() != it.GetRefSeqId() ||
                         !it2.GetRefSeq_id()->Equals(*it.GetRefSeq_id()) ||
                         it2.GetLength() != it.GetLength() ) {
                        ret_code = 1;
                        out << "Raw: RefSeq: " << it2.GetRefSeqId()
                            << " = " << it2.GetRefSeq_id()->AsFastaString()
                            << " " << it2.GetLength()
                            << endl;
                    }
                    ++it2;
                }
            }
        }
        if ( it2 ) {
            ret_code = 1;
            out << "Raw: has more refs: " << it2.GetRefSeqId()
                << endl;
        }
    }
    typedef map<string, int> TRefSeqIndex;
    TRefSeqIndex ref_seq_index;
    {
        int index = 0;
        CBamDb bam_db(mgr, path);
        for ( CBamRefSeqIterator it(bam_db); it; ++it ) {
            ref_seq_index[it.GetRefSeqId()] = index++;
        }
    }

    size_t limit_count = size_t(max(0, args["limit_count"].AsInteger()));
    bool ignore_errors = args["ignore_errors"];
    bool verbose = args["verbose"];
    bool make_seq_entry = args["make_seq_entry"];
    bool print_seq_entry = args["print_seq_entry"];
    vector<CRef<CSeq_entry> > entries;
    typedef map<string, int> TConflicts;
    TConflicts conflicts;
    bool short_id_conflicts = args["short_id_conflicts"];
    if ( 1 ) {
        size_t count = 0;
        CBamAlignIterator it, it2;
        bool collect_short = false;
        bool range_only = args["range_only"];
        bool no_ref_size = args["no_ref_size"];

        string query_id;
        CRange<TSeqPos> query_range = CRange<TSeqPos>::GetWhole();

        if ( args["refseq"] ) {
            query_id = args["refseq"].AsString();
            query_range.SetFrom(args["refpos"].AsInteger());
            if ( args["refwindow"] ) {
                TSeqPos window = args["refwindow"].AsInteger();
                if ( window != 0 ) {
                    query_range.SetLength(window);
                }
                else {
                    query_range.SetTo(kInvalidSeqPos);
                }
            }
            if ( args["refend"] ) {
                query_range.SetTo(args["refend"].AsInteger());
            }
        }
        if ( args["q"] ) {
            string q = args["q"].AsString();
            SIZE_TYPE colon_pos = q.find(':');
            if ( colon_pos == NPOS || colon_pos == 0 ) {
                ERR_POST(Fatal << "Invalid query format: " << q);
            }
            query_id = q.substr(0, colon_pos);
            SIZE_TYPE dash_pos = q.find('-', colon_pos+1);
            if ( dash_pos == NPOS ) {
                ERR_POST(Fatal << "Invalid query format: " << q);
            }
            TSeqPos from =
                NStr::StringToNumeric<TSeqPos>(q.substr(colon_pos+1,
                                                        dash_pos-colon_pos-1));
            TSeqPos to = NStr::StringToNumeric<TSeqPos>(q.substr(dash_pos+1));
            query_range.SetFrom(from).SetTo(to);
        }

        if ( !query_id.empty() ) {
            collect_short = !args["no_short"];
            if ( args["refposs"] ) {
                string str = args["refposs"].AsString();
                CNcbiIstrstream in(str.data(), str.size());
                TSeqPos pos;
                while ( in >> pos ) {
                    NcbiCout << "Trying "<<pos<<NcbiFlush;
                    CBamAlignIterator ait(bam_db, query_id, pos);
                    if ( ait ) {
                        NcbiCout << " -> " << ait.GetRefSeqPos() << NcbiEndl;
                    }
                    else {
                        NcbiCout << " none" << NcbiEndl;
                    }
                }
            }
            it = CBamAlignIterator(bam_db, query_id,
                                   query_range.GetFrom(),
                                   query_range.GetLength());
            if ( bam_db2 ) {
                it2 = CBamAlignIterator(bam_db2, query_id,
                                        query_range.GetFrom(),
                                        query_range.GetLength());
            }
        }
        else {
            it = CBamAlignIterator(bam_db);
            if ( bam_db2 ) {
                it2 = CBamAlignIterator(bam_db2);
            }
        }
        string p_ref_seq_id, p_short_seq_id, p_short_seq_acc, p_data, p_cigar;
        TSeqPos p_ref_pos = 0, p_ref_size = 0;
        TSeqPos p_short_pos = 0, p_short_size = 0;
        Uint8 p_qual = 0;
        int p_strand = -1;
        typedef map<string, vector<COpenRange<TSeqPos> > > TRefIds;
        typedef vector<pair<string, TSeqPos> > TShortIds;
        TRefIds ref_ids;
        TSeqPos ref_min = 0, ref_max = 0;
        if ( range_only ) {
            ref_min = it.GetRefSeqPos();
        }
        TShortIds short_ids;
        vector<samtools::SBamAlignment> alns1, alnsout1;
        size_t unaligned_count = 0;
        for ( ; it; ++it ) {
            try {
                ++count;
                TSeqPos ref_pos;
                try {
                    ref_pos = it.GetRefSeqPos();
                }
                catch ( CBamException& exc ) {
                    if ( exc.GetErrCode() != exc.eNoData ) {
                        throw;
                    }
                    ref_pos = kInvalidSeqPos;
                    ++unaligned_count;
                }
                if ( bam_db2 ) {
                    if ( !it2 ) {
                        ret_code = 1;
                        out << "Raw: no more alignments" << endl;
                    }
                    else if ( it2.GetRefSeqPos() != ref_pos ) {
                        ret_code = 1;
                        out << "Raw: different ref pos: "
                            << it2.GetRefSeqPos() << " vs " << ref_pos
                            << endl;
                    }
                }
                if ( range_only ) {
                    TSeqPos ref_end = ref_pos;
                    if ( !no_ref_size ) {
                        ref_end += it.GetCIGARRefSize();
                    }
                    if ( ref_end > ref_max ) {
                        ref_max = ref_end;
                    }
                    if ( count == limit_count ) {
                        break;
                    }
                    if ( it2 ) {
                        ++it2;
                    }
                    continue;
                }
                TSeqPos ref_size = it.GetCIGARRefSize();
                Uint8 qual = it.GetMapQuality();
                string ref_seq_id;
                if ( ref_pos == kInvalidSeqPos ) {
                    _ASSERT(ref_size == 0 || ref_size == it.GetShortSequence().size());
                    _ASSERT(qual == 0);
                    if ( verbose ) {
                        out << count << ": Unaligned\n";
                    }
                }
                else {
                    ref_seq_id = it.GetRefSeqId();
                    if ( verbose ) {
                        out << count << ": Ref: " << ref_seq_id
                            << " at [" << ref_pos
                            << " - " << (ref_pos+ref_size-1)
                            << "] = " << ref_size
                            << " Qual = " << qual
                            << '\n';
                    }
                    _ASSERT(!ref_seq_id.empty());
                }
                string short_seq_id = it.GetShortSeqId();
                string short_seq_acc = it.GetShortSeqAcc();
                TSeqPos short_pos = it.GetCIGARPos();
                TSeqPos short_size = it.GetCIGARShortSize();
                int strand = -1;
                const char* strand_str = "";
                bool minus = false;
                if ( it.IsSetStrand() ) {
                    strand = it.GetStrand();
                    if ( strand == eNa_strand_plus ) {
                        strand_str = " plus";
                    }
                    else if ( strand == eNa_strand_minus ) {
                        strand_str = " minus";
                        //minus = true;
                    }
                    else {
                        strand_str = " unknown";
                    }
                }
                Uint2 flags = it.GetFlags();
                bool is_paired = it.IsPaired();
                bool is_first = it.IsFirstInPair();
                bool is_second = it.IsSecondInPair();
                if ( verbose ) {
                    out << "Seq: " << short_seq_id << " " << short_seq_acc
                        << " at [" << short_pos
                        << " - " << (short_pos+short_size-1)
                        << "] = " << short_size
                        << " 0x" << hex << flags << dec
                        << strand_str << " "
                        << (is_paired? "P": "")
                        << (is_first? "1": "")
                        << (is_second? "2": "")
                        << '\n';
                }

                {{
                    samtools::SBamAlignment aln;
                    if ( ref_pos == kInvalidSeqPos ) {
                        aln.ref_seq_index = -1;
                    }
                    else {
                        aln.ref_seq_index = ref_seq_index[ref_seq_id];
                    }
                    aln.ref_range.SetFrom(ref_pos).SetLength(ref_size);
                    aln.short_seq_id = short_seq_id;
                    aln.short_range.SetFrom(short_pos).SetLength(short_size);
                    aln.short_sequence = it.GetShortSequence();
                    {{
                        string cigar = it.GetCIGAR();
                        const char* ptr = cigar.data();
                        const char* end = ptr + cigar.size();
                        char type;
                        TSeqPos len;
                        while ( ptr != end ) {
                            type = *ptr;
                            for ( len = 0; ++ptr != end; ) {
                                char c = *ptr;
                                if ( c >= '0' && c <= '9' ) {
                                    len = len*10+(c-'0');
                                }
                                else {
                                    break;
                                }
                            }
                            const char* types = "MIDNSHP=X";
                            const char* ptr = strchr(types, type);
                            if ( !ptr ) {
                                ERR_POST(Fatal<<"Bad CIGAR char: "<<type);
                            }
                            aln.CIGAR.push_back((len<<4)|(ptr-types));
                        }
                    }}
                    aln.flags = flags;
                    aln.quality = qual;
                    if ( query_range == CRange<TSeqPos>::GetWhole() ||
                         aln.ref_range.IntersectingWith(query_range) ) {
                        alns1.push_back(aln);
                    }
                    else {
                        alnsout1.push_back(aln);
                    }
                }}
                
                if ( ref_pos != kInvalidSeqPos &&
                     !(ref_seq_id != p_ref_seq_id || 
                       ref_pos != p_ref_pos ||
                       ref_size != p_ref_size ||
                       short_seq_id != p_short_seq_id ||
                       short_pos != p_short_pos ||
                       short_size != p_short_size ||
                       strand != p_strand ||
                       qual != p_qual ) ) {
                    out << "Error: match info is the same"
                        << NcbiEndl;
                }
                if ( verbose || print_seq_entry || make_seq_entry ) {
                    string data = it.GetShortSequence();
                    if ( verbose ) {
                        out << "Sequence[" << data.size() << "]"
                            << ": " << data
                            << '\n';
                    }
                    string cigar;
                    if ( ref_pos != kInvalidSeqPos ) {
                        cigar = it.GetCIGAR();
                        if ( verbose ) {
                            out << "CIGAR: " << cigar
                                << '\n';
                        }
                        if ( cigar.empty() ) {
                            out << "Error: Empty CIGAR"
                                << NcbiEndl;
                        }
                        else if ( !cigar[cigar.size()-1] ) {
                            out << "Error: Bad CIGAR: "
                                << NStr::PrintableString(cigar)
                                << NcbiEndl;
                        }
                    }
                    if ( print_seq_entry || make_seq_entry ) {
                        CRef<CSeq_entry> entry = it.GetMatchEntry();
                        if ( make_seq_entry ) {
                            entries.push_back(entry);
                        }
                        if ( print_seq_entry ) {
                            out << MSerial_AsnText << *entry << '\n';
                        }
                    }
                    if ( ref_pos == p_ref_pos && strand == p_strand &&
                         cigar == p_cigar ) {
                        if ( data != p_data ) {
                            out << "Error: match data at the same place "
                                << "is not the same"
                                << NcbiEndl;
                        }
                    }
                    else {
                        if ( !(data != p_data ||
                               cigar != p_cigar) ) {
                            out << "Error: match data is the same"
                                << NcbiEndl;
                        }
                    }
                    p_data = data;
                    p_cigar = cigar;
                }
                if ( !sv.empty() ) {
                    const string& s = GetShortSeqData(it);
                    string av = minus? Reverse(s): s;
                    size_t match = 0, error = 0;
                    for ( size_t i = 0; i < s.size(); ++i ) {
                        char ac = av[i];
                        char sc = sv[ref_pos+i];
                        if ( Matches(ac, sc) ) {
                            ++match;
                        }
                        else if ( ac != 'N' ) {
                            ++error;
                        }
                    }
                    out << "Strand:" << strand_str << " ";
                    if ( error ) {
                        if ( error > match ) {
                            if ( qual == 0 ) {
                                out << "Mismatch: bad"
                                    << NcbiEndl;
                            }
                            else {
                                out << "Mismatch: bad, qual: " << qual
                                    << NcbiEndl;
                            }
                        }
                        else {
                            out << "Mismatch: weak"
                                << NcbiEndl;
                        }
                    }
                    else {
                        out << "Matches perfectly"
                            << NcbiEndl;
                    }
                    if ( 1 || error ) {
                        out << "Matched: " << match << " error: " << error
                            << NcbiEndl;
                        string r;
                        sv.GetSeqData(ref_pos, ref_pos+ref_size, r);
                        out << "Short data: " << s
                            << NcbiEndl;
                        if ( minus ) {
                            out << "Align data: " << av
                                << NcbiEndl;
                        }
                        out << "Check data: " << r
                            << NcbiEndl;
                    }
                }
                if ( short_id_conflicts ) {
                    conflicts[it.GetShortSeq_id()->AsFastaString()] += 1;
                }
                p_ref_seq_id = ref_seq_id;
                p_ref_pos = ref_pos;
                p_ref_size = ref_size;
                p_short_seq_id = short_seq_id;
                p_short_pos = short_pos;
                p_short_size = short_size;
                p_qual = qual;
                p_strand = strand;
                ref_ids[ref_seq_id]
                    .push_back(COpenRange<TSeqPos>(ref_pos, ref_pos+ref_size));
                if ( collect_short ) {
                    short_ids.push_back(make_pair(short_seq_id, ref_pos));
                }
                if ( it2 ) {
                    if ( ref_pos != kInvalidSeqPos &&
                         (it2.GetRefSeqId() != it.GetRefSeqId() ||
                          !it2.GetRefSeq_id()->Equals(*it.GetRefSeq_id())) ) {
                        ret_code = 1;
                        out << "Raw: different ref id "
                            << it2.GetRefSeqId()
                            << " vs "
                            << it.GetRefSeqId()
                            << endl;
                    }
                    if ( it2.GetShortSeqId() != it.GetShortSeqId() ||
                         !it2.GetShortSeq_id()->Equals(*it.GetShortSeq_id()) ) {
                        ret_code = 1;
                        out << "Raw: different read id "
                            << it2.GetShortSeqId()
                            << " vs "
                            << it.GetShortSeqId()
                            << endl;
                    }
                    if ( it2.GetShortSeqAcc() != it.GetShortSeqAcc() ) {
                        ret_code = 1;
                        out << "Raw: different read acc "
                            << it2.GetShortSeqAcc()
                            << " vs "
                            << it.GetShortSeqAcc()
                            << endl;
                    }
                    if ( it2.GetFlags() != it.GetFlags() ) {
                        ret_code = 1;
                        out << "Raw: different flags "
                            << it2.GetFlags()
                            << " vs "
                            << it.GetFlags()
                            << endl;
                    }
                    if ( it2.IsPaired() != it.IsPaired() ) {
                        ret_code = 1;
                        out << "Raw: different IsPaired "
                            << it2.IsPaired()
                            << " vs "
                            << it.IsPaired()
                            << endl;
                    }
                    if ( it2.IsFirstInPair() != it.IsFirstInPair() ) {
                        ret_code = 1;
                        out << "Raw: different IsFirstInPair "
                            << it2.IsFirstInPair()
                            << " vs "
                            << it.IsFirstInPair()
                            << endl;
                    }
                    if ( it2.IsSecondInPair() != it.IsSecondInPair() ) {
                        ret_code = 1;
                        out << "Raw: different IsSecondInPair "
                            << it2.IsSecondInPair()
                            << " vs "
                            << it.IsSecondInPair()
                            << endl;
                    }
                    if ( ref_pos != kInvalidSeqPos ) {
                        if ( it2.GetCIGARRefSize() != it.GetCIGARRefSize() ) {
                            ret_code = 1;
                            out << "Raw: different ref size "
                                << it2.GetCIGARRefSize()
                                << " vs "
                                << it.GetCIGARRefSize()
                                << endl;
                        }
                        if ( it2.GetCIGARPos() != it.GetCIGARPos() ||
                             it2.GetCIGARShortSize() != it.GetCIGARShortSize() ) {
                            ret_code = 1;
                            out << "Raw: different read pos "
                                << it2.GetCIGARPos() << "+" << it2.GetCIGARShortSize()
                                << " vs "
                                << it.GetCIGARPos() << "+" << it.GetCIGARShortSize()
                                << endl;
                        }
                        if ( it2.IsSetStrand() != it.IsSetStrand() ||
                             it2.GetStrand() != it.GetStrand() ) {
                            ret_code = 1;
                            out << "Raw: different strands "
                                << it2.GetStrand()
                                << " vs "
                                << it.GetStrand()
                                << endl;
                        }
                        if ( it2.GetMapQuality() != it.GetMapQuality() ) {
                            ret_code = 1;
                            out << "Raw: different qualities "
                                << it2.GetMapQuality()
                                << " vs "
                                << it.GetMapQuality()
                                << endl;
                        }
                        if ( it2.GetCIGAR() != it.GetCIGAR() ) {
                            ret_code = 1;
                            out << "Raw: different CIGAR "
                                << it2.GetCIGAR()
                                << " vs "
                                << it.GetCIGAR()
                                << endl;
                        }
                    }
                    if ( !it2.GetMatchEntry()->Equals(*it.GetMatchEntry()) ) {
                        ret_code = 1;
                        out << "Raw: different entries " << MSerial_AsnText
                            << *it2.GetMatchEntry()
                            << " vs "
                            << *it.GetMatchEntry()
                            << endl;
                    }
                }
            }
            catch ( CException& exc ) {
                ERR_POST("Error: "<<exc);
                if ( !ignore_errors ) {
                    throw;
                }
            }
            if ( limit_count > 0 && count == limit_count ) break;
            if ( bam_db2 ) {
                if ( !it2 ) {
                    ret_code = 1;
                    out << "Raw: no more alignments" << endl;
                }
                else {
                    ++it2;
                }
            }
        }
        out << "Loaded: " << count << " alignments." << NcbiEndl;
        if ( bam_db2 ) {
            if ( bool(it) != bool(it2) ) {
                ret_code = 1;
                out << "Raw: different number of alignments" << endl;
            }
        }
        if ( range_only ) {
            out << "Range: " << ref_min << "-" << ref_max-1 << NcbiEndl;
        }
        else {
            NON_CONST_ITERATE ( TRefIds, it, ref_ids ) {
                if ( it->first.empty() ) {
                    out << "Unmapped alignments: " << it->second.size() << NcbiEndl;
                }
                else {
                    out << "Ref " << it->first << ": " << it->second.size() << NcbiFlush;
                    sort(it->second.begin(), it->second.end());
                    out << "    " << it->second[0].GetFrom() << "-"
                        << it->second.back().GetToOpen()-1 << NcbiEndl;
                }
            }
            if ( collect_short && !short_ids.empty() ) {
                sort(short_ids.begin(), short_ids.end());
                out << "Short: " << short_ids[0].first << "-"
                    << short_ids.back().first << NcbiEndl;
            }
            out << "Sorted." << NcbiEndl;
        }
        if ( 1 ) {
            vector<samtools::SBamAlignment> alns2, alnsout2;

            samtools::CBamFile bam(path,
                                   args["no_index"]?
                                   samtools::CBamFile::eWithoutIndex:
                                   samtools::CBamFile::eWithIndex);
            if ( !query_id.empty() ) {
                bam.Fetch(query_id,
                          query_range.GetFrom(),
                          query_range.GetTo(),
                          alns2, limit_count);
            }
            else {
                ITERATE ( TRefSeqIndex, it, ref_seq_index ) {
                    vector<samtools::SBamAlignment> alns2t;
                    size_t cur_limit = 0;
                    if ( limit_count ) {
                        if ( alns2.size() >= limit_count ) {
                            break;
                        }
                        cur_limit = limit_count-alns2.size();
                    }
                    bam.Fetch(it->first, 0, kInvalidSeqPos, alns2t, cur_limit);
                    alns2.insert(alns2.end(), alns2t.begin(), alns2t.end());
                }
            }
            if ( 1 ) {
                // remove hard breaks
                for ( auto& a : alns2 ) {
                    if ( !a.CIGAR.empty() && (a.CIGAR.back() & 0xf) == 5 ) {
                        a.CIGAR.pop_back();
                    }
                    if ( !a.CIGAR.empty() && (a.CIGAR.front() & 0xf) == 5 ) {
                        a.CIGAR.erase(a.CIGAR.begin());
                    }
                }
            }
            if ( 1 ) {
                // reset quality of unmapped sequences
                for ( auto& a : alns2 ) {
                    if ( a.ref_seq_index == -1 ) {
                        a.quality = 0;
                    }
                }
            }

            out << "SamTools count: " << alns2.size() << NcbiEndl;
            sort(alns1.begin(), alns1.end());
            sort(alns2.begin(), alns2.end());
            size_t diff_count = 0;
            if ( alns1 != alns2 ) {
                size_t size1 = alns1.size(), i1 = 0;
                size_t size2 = alns2.size(), i2 = 0;
                out << "SRA SDK and SamTools results differ:" << NcbiEndl;
                while ( i1 < size1 || i2 < size2 ) {
                    if ( i2 < size2 && query_range != CRange<TSeqPos>::GetWhole() &&
                         !alns2[i2].ref_range.IntersectingWith(query_range) ) {
                        alnsout2.push_back(alns2[i2]);
                        ++i2;
                        continue;
                    }
                    if ( i1 < size1 &&
                         (i2 == size2 || alns1[i1] < alns2[i2]) ) {
                        out << " aln1["<<i1<<"]: "<<alns1[i1]<<NcbiEndl;
                        ++i1;
                    }
                    else if ( i2 < size2 &&
                              (i1 == size1 || alns2[i2] < alns1[i1]) ) {
                        out << " aln2["<<i2<<"]: "<<alns2[i2]<<NcbiEndl;
                        ++i2;
                    }
                    else {
                        ++i1;
                        ++i2;
                    }
                }
            }
            sort(alnsout1.begin(), alnsout1.end());
            diff_count = alnsout1.size() + alnsout2.size();
            if ( !alnsout1.empty() ) {
                if ( alnsout1 == alnsout2 ) {
                    out << "Both API returned the same "<<alnsout1.size()
                        << " non-overlapping alignments." << NcbiEndl;
                    alnsout2.clear();
                }
                else {
                    out << "SRA SDK returned "<<alnsout1.size()
                        << " non-overlapping alignments." << NcbiEndl;
                    ITERATE ( vector<samtools::SBamAlignment>, it, alnsout1 ) {
                        out << " OUT: " << *it << NcbiEndl;
                    }
                }
                ret_code = 1;
            }
            if ( !alnsout2.empty() ) {
                out << "SamTools API returned "<<alnsout1.size()
                    << " non-overlapping alignments." << NcbiEndl;
                ret_code = 1;
            }
            if ( unaligned_count != 0 ) {
                out << "SRA SDK returned "
                    << unaligned_count << " unaligned reads."
                    << NcbiEndl;
            }
            if ( diff_count == 0 ) {
                out << "SRA SDK and SamTools results are exactly the same."
                    << NcbiEndl;
            }
            else {
                ret_code = 1;
            }
        }
        if ( short_id_conflicts ) {
            int conflict_count = 0;
            ITERATE ( TConflicts, it, conflicts ) {
                if ( it->second > 1 ) {
                    out << "Conflict id: " << it->first << NcbiEndl;
                    ++conflict_count;
                }
            }
            if ( conflict_count ) {
                out << "Conflicts: " << conflict_count << NcbiEndl;
            }
        }
    }
    out << "Exiting" << NcbiEndl;
    if ( ret_code ) {
        out << "***** Errors were found *****" << NcbiEndl;
    }
    return ret_code;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CBAMCompareApp::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CBAMCompareApp().AppMain(argc, argv);
}
