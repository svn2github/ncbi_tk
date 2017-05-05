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
* Author:  Eugene Vasilchenko
*
* File Description:
*   Unit tests for CSRA data loader
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <sra/readers/sra/exception.hpp>
#include <sra/data_loaders/csra/csraloader.hpp>
#include <sra/readers/ncbi_traces_path.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/seqres/seqres__.hpp>
#include <corelib/ncbi_system.hpp>
#include <objtools/readers/idmapper.hpp>
#include <serial/iterator.hpp>

#include <corelib/test_boost.hpp>

#include <common/test_data_path.h>
#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

#define PILEUP_NAME_SUFFIX " pileup graphs"

CRef<CObjectManager> sx_GetOM(void)
{
    SetDiagPostLevel(eDiag_Info);
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CObjectManager::TRegisteredNames names;
    om->GetRegisteredNames(names);
    ITERATE ( CObjectManager::TRegisteredNames, it, names ) {
        om->RevokeDataLoader(*it);
    }
    return om;
}

void sx_CheckNames(CScope& scope, const CSeq_loc& loc,
                   const string& name)
{
    SAnnotSelector sel;
    sel.SetSearchUnresolved();
    sel.SetCollectNames();
    CAnnotTypes_CI it(CSeq_annot::C_Data::e_not_set, scope, loc, &sel);
    CAnnotTypes_CI::TAnnotNames names = it.GetAnnotNames();
    ITERATE ( CAnnotTypes_CI::TAnnotNames, i, names ) {
        if ( i->IsNamed() ) {
            NcbiCout << "Named annot: " << i->GetName()
                     << NcbiEndl;
        }
        else {
            NcbiCout << "Unnamed annot"
                     << NcbiEndl;
        }
    }
    //NcbiCout << "Checking for name: " << name << NcbiEndl;
    BOOST_CHECK_EQUAL(names.count(name), 1u);
    if ( names.size() > 1 ) {
        BOOST_CHECK_EQUAL(names.count(name+PILEUP_NAME_SUFFIX), 1u);
    }
    else {
        BOOST_CHECK_EQUAL(names.size(), 1u);
    }
}

string sx_GetPath(const string& dir)
{
    vector<string> reps;
    NStr::Split("traces02:traces04", ":", reps);
    ITERATE ( vector<string>, it, reps ) {
        string path = CFile::MakePath(CFile::MakePath(NCBI_GetTestDataPath(), *it), dir);
        if ( CDirEntry(path).Exists() ) {
            return path;
        }
    }
    return dir;
}

void sx_ReportCSraLoaderName(const string& name)
{
    NcbiCout << "CSRA loader: " << name << NcbiEndl;
    CDataLoader* loader = CObjectManager::GetInstance()->FindDataLoader(name);
    CCSRADataLoader* csra_loader = dynamic_cast<CCSRADataLoader*>(loader);
    BOOST_REQUIRE(csra_loader);
    CCSRADataLoader::TAnnotNames names = csra_loader->GetPossibleAnnotNames();
    ITERATE ( CCSRADataLoader::TAnnotNames, it, names ) {
        NcbiCout << "  annot name: " << it->GetName() << NcbiEndl;
    }
}

void sx_CheckSeq(CScope& scope,
                 const CSeq_id_Handle& main_idh,
                 const CSeq_id& id)
{
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(id);
    if ( idh == main_idh ) {
        return;
    }
    const CBioseq_Handle::TId& ids = scope.GetIds(idh);
    BOOST_REQUIRE(!ids.empty());
    //BOOST_CHECK_EQUAL(ids[0], idh);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
}

BOOST_AUTO_TEST_CASE(FetchSeq1)
{
    CRef<CObjectManager> om = sx_GetOM();

    CCSRADataLoader::SLoaderParams params;
    string csra_name, id;
    TSeqPos from, to, align_count;

    {
        //string path = NCBI_TRACES01_PATH "/compress/DATA/ASW/NA19909.mapped.illumina.mosaik.ASW.exome.20110411";
        string path = NCBI_TRACES01_PATH "/compress/1KG/ASW/NA19909";
        params.m_DirPath = sx_GetPath(path);
        csra_name = "exome.ILLUMINA.MOSAIK.csra";
        id = "NC_000001.10";
        from = 0;
        to = 100000;
        align_count = 37; // 36 if mapq>=1
    }
    csra_name = CFile::MakePath(params.m_DirPath, csra_name);
    params.m_DirPath.erase();
    params.m_CSRAFiles.push_back(csra_name);
    string annot_name = csra_name; //CDirEntry(csra_name).GetBase();
    string pileup_name = annot_name+PILEUP_NAME_SUFFIX;

    string loader_name =
        CCSRADataLoader::RegisterInObjectManager(*om, params,
                                                 CObjectManager::eDefault)
        .GetLoader()->GetName();
    sx_ReportCSraLoaderName(loader_name);
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CRef<CSeq_loc> loc(new CSeq_loc);
    loc->SetInt().SetId(*seqid);
    loc->SetInt().SetFrom(from);
    loc->SetInt().SetTo(to);
    sx_CheckNames(scope, *loc, annot_name);
    SAnnotSelector sel(CSeq_annot::C_Data::e_Align);
    sel.SetSearchUnresolved();
    sel.ExcludeNamedAnnots(pileup_name);

    if ( 1 ) {
        CGraph_CI git(scope, *loc, sel);
        BOOST_CHECK_EQUAL(git.GetSize(), 1u);
    }

    if ( 1 ) {
        CAlign_CI it(scope, *loc, sel);
        if ( it ) {
            cout << "Align count: "<<it.GetSize()<<endl;
            if ( it.GetAnnot().IsNamed() ) {
                cout << "Annot name: " << it.GetAnnot().GetName()<<endl;
            }
        }
        BOOST_CHECK_EQUAL(align_count, it.GetSize());

        for ( ; it; ++it ) {
            const CSeq_align& align = *it;
            ITERATE(CDense_seg::TIds, j, align.GetSegs().GetDenseg().GetIds()) {
                sx_CheckSeq(scope, idh, **j);
            }
        }
    }

    if ( 1 ) {
        sel.ResetAnnotsNames();
        sel.AddNamedAnnots(pileup_name);
        CGraph_CI git(scope, *loc, sel);
        BOOST_CHECK(git.GetSize() % 6 == 0);
        if ( 0 ) {
            for ( ; git; ++git ) {
                NcbiCout << MSerial_AsnText << git->GetOriginalGraph();
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(FetchSeq2)
{
    CRef<CObjectManager> om = sx_GetOM();

    CCSRADataLoader::SLoaderParams params;
    string csra_name, id;
    TSeqPos from, to, align_count;

    {
        //string path = NCBI_TRACES01_PATH "/compress/DATA/ASW/NA19909.mapped.illumina.mosaik.ASW.exome.20110411";
        string path = NCBI_TRACES01_PATH "/compress/1KG/ASW/NA19909";
        params.m_DirPath = sx_GetPath(path);
        csra_name = "exome.ILLUMINA.MOSAIK.csra";
        id = "NC_000001.10";
        if ( 1 ) {
            CNcbiIfstream mapfile("mapfile");
            BOOST_CHECK(mapfile);
            params.m_IdMapper.reset(new CIdMapperConfig(mapfile, "", false));
            id = "89161185";
            //id = "NC_000001.9";
        }
        from = 0;
        to = 100000;
        align_count = 37; // 36 if mapq>=1
    }
    params.m_CSRAFiles.push_back(csra_name);
    string annot_name = "csra_name"; //CDirEntry(csra_name).GetBase();
    params.m_AnnotName = "csra_name";
    string pileup_name = annot_name+PILEUP_NAME_SUFFIX;

    string loader_name =
        CCSRADataLoader::RegisterInObjectManager(*om, params,
                                                 CObjectManager::eDefault)
        .GetLoader()->GetName();
    sx_ReportCSraLoaderName(loader_name);
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CRef<CSeq_loc> loc(new CSeq_loc);
    loc->SetInt().SetId(*seqid);
    loc->SetInt().SetFrom(from);
    loc->SetInt().SetTo(to);
    sx_CheckNames(scope, *loc, annot_name);
    SAnnotSelector sel(CSeq_annot::C_Data::e_Align);
    sel.SetSearchUnresolved();
    sel.ExcludeNamedAnnots(pileup_name);

    if ( 1 ) {
        CGraph_CI git(scope, *loc, sel);
        BOOST_CHECK_EQUAL(git.GetSize(), 1u);
    }

    if ( 1 ) {
        CAlign_CI it(scope, *loc, sel);
        if ( it ) {
            cout << "Align count: "<<it.GetSize()<<endl;
            if ( it.GetAnnot().IsNamed() ) {
                cout << "Annot name: " << it.GetAnnot().GetName()<<endl;
            }
        }
        BOOST_CHECK_EQUAL(align_count, it.GetSize());

        for ( ; it; ++it ) {
            const CSeq_align& align = *it;
            ITERATE(CDense_seg::TIds, j, align.GetSegs().GetDenseg().GetIds()) {
                sx_CheckSeq(scope, idh, **j);
            }
        }
    }

    if ( 1 ) {
        sel.ResetAnnotsNames();
        sel.AddNamedAnnots(pileup_name);
        CGraph_CI git(scope, *loc, sel);
        BOOST_CHECK(git.GetSize() % 6 == 0);
        if ( 0 ) {
            for ( ; git; ++git ) {
                NcbiCout << MSerial_AsnText << git->GetOriginalGraph();
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(FetchSeq3)
{
    CRef<CObjectManager> om = sx_GetOM();

    CCSRADataLoader::SLoaderParams params;
    string csra_name, id;
    TSeqPos from, to, align_count;

    {
        string path = NCBI_TRACES01_PATH "/compress/1KG/CEU/NA12043";
        params.m_DirPath = sx_GetPath(path);
        csra_name = "genome.LS454.SSAHA2.csra";
        id = "NC_000001.10";
        if ( 0 ) {
            CNcbiIfstream mapfile("mapfile");
            BOOST_CHECK(mapfile);
            params.m_IdMapper.reset(new CIdMapperConfig(mapfile, "", false));
            id = "89161185";
            //id = "NC_000001.9";
        }
        from = 0;
        to = 100000;
        align_count = 617; // 95 if mapq>=1
    }
    params.m_CSRAFiles.push_back(csra_name);
    string annot_name = csra_name; //CDirEntry(csra_name).GetBase();
    string pileup_name = annot_name+PILEUP_NAME_SUFFIX;

    string loader_name =
        CCSRADataLoader::RegisterInObjectManager(*om, params,
                                                 CObjectManager::eDefault)
        .GetLoader()->GetName();
    sx_ReportCSraLoaderName(loader_name);
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CRef<CSeq_loc> loc(new CSeq_loc);
    loc->SetInt().SetId(*seqid);
    loc->SetInt().SetFrom(from);
    loc->SetInt().SetTo(to);
    sx_CheckNames(scope, *loc, annot_name);
    SAnnotSelector sel(CSeq_annot::C_Data::e_Align);
    sel.SetSearchUnresolved();
    sel.ExcludeNamedAnnots(pileup_name);

    if ( 1 ) {
        CGraph_CI git(scope, *loc, sel);
        BOOST_CHECK_EQUAL(git.GetSize(), 1u);
    }

    if ( 1 ) {
        CAlign_CI it(scope, *loc, sel);
        if ( it ) {
            cout << "Align count: "<<it.GetSize()<<endl;
            if ( it.GetAnnot().IsNamed() ) {
                cout << "Annot name: " << it.GetAnnot().GetName()<<endl;
            }
        }
        BOOST_CHECK_EQUAL(align_count, it.GetSize());

        for ( ; it; ++it ) {
            const CSeq_align& align = *it;
            ITERATE(CDense_seg::TIds, j, align.GetSegs().GetDenseg().GetIds()) {
                sx_CheckSeq(scope, idh, **j);
            }
        }
    }

    if ( 1 ) {
        sel.ResetAnnotsNames();
        sel.AddNamedAnnots(pileup_name);
        CGraph_CI git(scope, *loc, sel);
        BOOST_CHECK(git.GetSize() % 6 == 0);
        if ( 0 ) {
            for ( ; git; ++git ) {
                NcbiCout << MSerial_AsnText << git->GetOriginalGraph();
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(FetchSeq4)
{
    CRef<CObjectManager> om = sx_GetOM();

    CCSRADataLoader::SLoaderParams params;
    string csra_name, id;
    TSeqPos from, to, align_count;

    {
        string path = NCBI_TRACES01_PATH "/compress/1KG/CEU/NA12249";
        params.m_DirPath = sx_GetPath(path);
        //csra_name = "genome.ILLUMINA.BWA.csra";
        csra_name = "exome.ILLUMINA.MOSAIK.csra";
        id = "NC_000023.10";
        if ( 0 ) {
            CNcbiIfstream mapfile("mapfile");
            BOOST_CHECK(mapfile);
            params.m_IdMapper.reset(new CIdMapperConfig(mapfile, "", false));
            id = "89161185";
            //id = "NC_000001.9";
        }
        from = 31137345;
        to   = 31157726;
        align_count = 730;
        //to = 33357726;
        //align_count = 129595;
    }
    params.m_CSRAFiles.push_back(csra_name);
    string annot_name = csra_name; //CDirEntry(csra_name).GetBase();
    string pileup_name = annot_name+PILEUP_NAME_SUFFIX;

    CStopWatch sw(CStopWatch::eStart);
    string loader_name =
        CCSRADataLoader::RegisterInObjectManager(*om, params,
                                                 CObjectManager::eDefault)
        .GetLoader()->GetName();
    sx_ReportCSraLoaderName(loader_name);
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CRef<CSeq_loc> loc(new CSeq_loc);
    loc->SetInt().SetId(*seqid);
    loc->SetInt().SetFrom(from);
    loc->SetInt().SetTo(to);
    sx_CheckNames(scope, *loc, annot_name);
    SAnnotSelector sel(CSeq_annot::C_Data::e_Align);
    sel.SetSearchUnresolved();
    sel.ExcludeNamedAnnots(pileup_name);

    if ( 1 ) {
        CGraph_CI git(scope, *loc, sel);
        BOOST_CHECK_EQUAL(git.GetSize(), 1u);
    }

    if ( 1 ) {
        CAlign_CI it(scope, *loc, sel);
        if ( it ) {
            cout << "Align count: "<<it.GetSize()<<endl;
            if ( it.GetAnnot().IsNamed() ) {
                cout << "Annot name: " << it.GetAnnot().GetName()<<endl;
            }
        }
        BOOST_CHECK_EQUAL(align_count, it.GetSize());
        NcbiCout << "Elapsed: " << sw.Elapsed() << NcbiEndl;

        for ( ; it; ++it ) {
            const CSeq_align& align = it.GetOriginalSeq_align();
            //NcbiCout << MSerial_AsnText << align;
            if ( 1 ) {
                ITERATE( CDense_seg::TIds, j,
                         align.GetSegs().GetDenseg().GetIds() ) {
                    sx_CheckSeq(scope, idh, **j);
                    CSeq_id_Handle id2 = CSeq_id_Handle::GetHandle(**j);
                    if ( id2 != idh ) {
                        CBioseq_Handle bh = scope.GetBioseqHandle(id2);
                        BOOST_CHECK(bh);
                        //NcbiCout << MSerial_AsnText << *bh.GetCompleteBioseq();
                    }
                }
            }
        }
        NcbiCout << "Elapsed: " << sw.Elapsed() << NcbiEndl;
    }

    if ( 1 ) {
        sel.ResetAnnotsNames();
        sel.AddNamedAnnots(pileup_name);
        CGraph_CI git(scope, *loc, sel);
        BOOST_CHECK(git.GetSize() % 6 == 0);
        if ( 0 ) {
            for ( ; git; ++git ) {
                NcbiCout << MSerial_AsnText << git->GetOriginalGraph();
            }
        }
        NcbiCout << "Elapsed: " << sw.Elapsed() << NcbiEndl;
    }
}


BOOST_AUTO_TEST_CASE(FetchSeq4l)
{
    CRef<CObjectManager> om = sx_GetOM();

    CCSRADataLoader::SLoaderParams params;
    string csra_name, id;

    {
        string path = NCBI_TRACES01_PATH "/compress/1KG/CEU/NA12249";
        path = sx_GetPath(path);
        csra_name = CDirEntry::MakePath(path, "exome.ILLUMINA.MOSAIK.csra");
        id = csra_name;
        replace(id.begin(), id.end(), '/', '\\');
        id = "gnl|SRA|"+id+".6.1";
    }
    params.m_CSRAFiles.push_back(csra_name);
    params.m_PathInId = true;
    params.m_QualityGraphs = true;

    CStopWatch sw(CStopWatch::eStart);
    string loader_name =
        CCSRADataLoader::RegisterInObjectManager(*om, params,
                                                 CObjectManager::eDefault)
        .GetLoader()->GetName();
    sx_ReportCSraLoaderName(loader_name);

    for ( int t = 0; t < 2; ++t ) {
        CScope scope(*om);
        scope.AddDefaults();

        CRef<CSeq_id> seqid(new CSeq_id(id));
        CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
        CBioseq_Handle bh = scope.GetBioseqHandle(idh);
        BOOST_REQUIRE(bh);
        BOOST_CHECK_EQUAL(bh.GetBioseqLength(), 97u);
        CGraph_CI git(bh);
        BOOST_CHECK_EQUAL(git.GetSize(), 1u);
    }
}


BOOST_AUTO_TEST_CASE(FetchSeq4sx)
{
    CRef<CObjectManager> om = sx_GetOM();

    CCSRADataLoader::SLoaderParams params;
    string csra_name, id;

    {
        string path = NCBI_TRACES01_PATH "/compress/1KG/CEU/NA12249";
        path = sx_GetPath(path);
        csra_name = CDirEntry::MakePath(path, "exome.ILLUMINA.MOSAIK.csra");
        id = csra_name;
        replace(id.begin(), id.end(), '/', '\\');
        id = "gnl|SRA|"+id+".6.1";
    }
    params.m_CSRAFiles.push_back(csra_name);
    params.m_PathInId = true;
    params.m_QualityGraphs = true;

    CStopWatch sw(CStopWatch::eStart);
    string loader_name =
        CCSRADataLoader::RegisterInObjectManager(*om, params,
                                                 CObjectManager::eDefault)
        .GetLoader()->GetName();
    sx_ReportCSraLoaderName(loader_name);

    for ( int t = 0; t < 2; ++t ) {
        CScope scope(*om);
        scope.AddDefaults();

        CRef<CSeq_id> seqid(new CSeq_id(id));
        CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
        CBioseq_Handle bh = scope.GetBioseqHandle(idh);
        BOOST_REQUIRE(bh);
        BOOST_CHECK_EQUAL(bh.GetBioseqLength(), 97u);
        CGraph_CI git(bh);
        BOOST_CHECK_EQUAL(git.GetSize(), 1u);
    }
}


BOOST_AUTO_TEST_CASE(FetchSeq4sd)
{
    CRef<CObjectManager> om = sx_GetOM();

    CCSRADataLoader::SLoaderParams params;
    string csra_name, id;

    {
        string path = NCBI_TRACES01_PATH "/compress/1KG/CEU/NA12249";
        string file;
        CDirEntry::SplitPath(sx_GetPath(path), &params.m_DirPath, &file);
        csra_name = CDirEntry::MakePath(file, "exome.ILLUMINA.MOSAIK.csra");
        id = CDirEntry::MakePath(file, "exome.ILLUMINA.MOSAIK");
        replace(id.begin(), id.end(), '/', '\\');
        id = "gnl|SRA|"+id+".6.1";
    }
    params.m_CSRAFiles.push_back(csra_name);
    params.m_PathInId = false;
    params.m_QualityGraphs = true;

    CStopWatch sw(CStopWatch::eStart);
    string loader_name =
        CCSRADataLoader::RegisterInObjectManager(*om, params,
                                                 CObjectManager::eDefault)
        .GetLoader()->GetName();
    sx_ReportCSraLoaderName(loader_name);

    for ( int t = 0; t < 2; ++t ) {
        CScope scope(*om);
        scope.AddDefaults();

        CRef<CSeq_id> seqid(new CSeq_id(id));
        CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
        CBioseq_Handle bh = scope.GetBioseqHandle(idh);
        BOOST_REQUIRE(bh);
        BOOST_CHECK_EQUAL(bh.GetBioseqLength(), 97u);
        CGraph_CI git(bh);
        BOOST_CHECK_EQUAL(git.GetSize(), 1u);
    }
}


BOOST_AUTO_TEST_CASE(FetchSeq4s)
{
    CRef<CObjectManager> om = sx_GetOM();

    CCSRADataLoader::SLoaderParams params;
    string csra_name, id;

    {
        string path = NCBI_TRACES01_PATH "/compress/1KG/CEU/NA12249";
        params.m_DirPath = sx_GetPath(path);
        csra_name = "exome.ILLUMINA.MOSAIK.csra";
        id = "gnl|SRA|exome.ILLUMINA.MOSAIK.6.1";
    }
    params.m_CSRAFiles.push_back(csra_name);
    params.m_PathInId = false;
    params.m_QualityGraphs = true;

    CStopWatch sw(CStopWatch::eStart);
    string loader_name =
        CCSRADataLoader::RegisterInObjectManager(*om, params,
                                                 CObjectManager::eDefault)
        .GetLoader()->GetName();
    sx_ReportCSraLoaderName(loader_name);
    for ( int t = 0; t < 2; ++t ) {
        CScope scope(*om);
        scope.AddDefaults();

        CRef<CSeq_id> seqid(new CSeq_id(id));
        CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
        CBioseq_Handle bh = scope.GetBioseqHandle(idh);
        BOOST_REQUIRE(bh);
        BOOST_CHECK_EQUAL(bh.GetBioseqLength(), 97u);
        CGraph_CI git(bh);
        BOOST_CHECK_EQUAL(git.GetSize(), 1u);
    }
}


BOOST_AUTO_TEST_CASE(FetchSeq5)
{
    CRef<CObjectManager> om = sx_GetOM();

    CCSRADataLoader::SLoaderParams params;
    string csra_name, id;
    TSeqPos from, to, align_count;

#if 1
    {
        csra_name = "SRR389414";
        params.m_DirPath = csra_name;
        id = "NC_000023.10";
        from = 1000000;
        to = 2000000;
        align_count = 3016; // 3005 if mapq>=1
    }
    string loader_name =
        CCSRADataLoader::RegisterInObjectManager(*om, params,
                                                 CObjectManager::eDefault)
        .GetLoader()->GetName();
#else
    {
        csra_name = "SRR389414";
        id = "NC_000023.10";
        from = 1000000;
        to = 2000000;
        align_count = 3016; // 3005 if mapq>=1
    }
    string loader_name =
        CCSRADataLoader::RegisterInObjectManager(*om, csra_name,
                                                 CObjectManager::eDefault)
        .GetLoader()->GetName();
#endif
    sx_ReportCSraLoaderName(loader_name);
    CScope scope(*om);
    scope.AddDefaults();

    string annot_name = csra_name; //CDirEntry(csra_name).GetBase();
    string pileup_name = annot_name+PILEUP_NAME_SUFFIX;

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CRef<CSeq_loc> loc(new CSeq_loc);
    loc->SetInt().SetId(*seqid);
    loc->SetInt().SetFrom(from);
    loc->SetInt().SetTo(to);
    sx_CheckNames(scope, *loc, annot_name);
    SAnnotSelector sel(CSeq_annot::C_Data::e_Align);
    sel.SetSearchUnresolved();
    sel.ExcludeNamedAnnots(pileup_name);

    if ( 1 ) {
        CGraph_CI git(scope, *loc, sel);
        BOOST_CHECK_EQUAL(git.GetSize(), 1u);
    }

    if ( 1 ) {
        CAlign_CI it(scope, *loc, sel);
        if ( it ) {
            cout << "Align count: "<<it.GetSize()<<endl;
            if ( it.GetAnnot().IsNamed() ) {
                cout << "Annot name: " << it.GetAnnot().GetName()<<endl;
            }
        }
        BOOST_CHECK_EQUAL(align_count, it.GetSize());

        for ( ; it; ++it ) {
            const CSeq_align& align = *it;
            ITERATE(CDense_seg::TIds, j, align.GetSegs().GetDenseg().GetIds()) {
                sx_CheckSeq(scope, idh, **j);
            }
        }
    }

    if ( 1 ) {
        sel.ResetAnnotsNames();
        sel.AddNamedAnnots(pileup_name);
        CGraph_CI git(scope, *loc, sel);
        BOOST_CHECK(git.GetSize() % 6 == 0);
        if ( 0 ) {
            for ( ; git; ++git ) {
                NcbiCout << MSerial_AsnText << git->GetOriginalGraph();
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(FetchSeq6)
{
    CRef<CObjectManager> om = sx_GetOM();

    CCSRADataLoader::SLoaderParams params;
    string csra_name, id;
    TSeqPos from, to, align_count;

    {
        csra_name = "SRR389414";
        id = "gnl|SRA|SRR389414/GL000215.1";
        from = 100000;
        to   = 200000;
        align_count = 83; // 8 if mapq>=1
    }
    string loader_name =
        CCSRADataLoader::RegisterInObjectManager(*om, params,
                                                 CObjectManager::eDefault)
        .GetLoader()->GetName();
    sx_ReportCSraLoaderName(loader_name);
    CScope scope(*om);
    scope.AddDefaults();

    string annot_name = csra_name;
    string pileup_name = annot_name+PILEUP_NAME_SUFFIX;

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CRef<CSeq_loc> loc(new CSeq_loc);
    loc->SetInt().SetId(*seqid);
    loc->SetInt().SetFrom(from);
    loc->SetInt().SetTo(to);
    sx_CheckNames(scope, *loc, annot_name);
    SAnnotSelector sel(CSeq_annot::C_Data::e_Align);
    sel.SetSearchUnresolved();
    sel.ExcludeNamedAnnots(pileup_name);

    BOOST_CHECK(scope.GetBioseqHandle(idh));
    //NcbiCout<<MSerial_AsnText<<*scope.GetBioseqHandle(idh).GetCompleteObject();
    //NcbiCout<<MSerial_AsnText<<*scope.GetBioseqHandle(idh).GetTopLevelEntry().GetCompleteObject();
    if ( 1 ) {
        CGraph_CI git(scope, *loc, sel);
        BOOST_CHECK_EQUAL(git.GetSize(), 1u);
    }

    if ( 1 ) {
        CAlign_CI it(scope, *loc, sel);
        if ( it ) {
            cout << "Align count: "<<it.GetSize()<<endl;
            if ( it.GetAnnot().IsNamed() ) {
                cout << "Annot name: " << it.GetAnnot().GetName()<<endl;
            }
        }
        BOOST_CHECK_EQUAL(align_count, it.GetSize());

        for ( ; it; ++it ) {
            const CSeq_align& align = *it;
            ITERATE(CDense_seg::TIds, j, align.GetSegs().GetDenseg().GetIds()) {
                sx_CheckSeq(scope, idh, **j);
            }
        }
    }

    if ( 1 ) {
        sel.ResetAnnotsNames();
        sel.AddNamedAnnots(pileup_name);
        CGraph_CI git(scope, *loc, sel);
        BOOST_CHECK(git.GetSize() % 6 == 0);
        if ( 0 ) {
            for ( ; git; ++git ) {
                NcbiCout << MSerial_AsnText << git->GetOriginalGraph();
            }
        }
    }
}


BOOST_AUTO_TEST_CASE(FetchSeq7)
{
    CRef<CObjectManager> om = sx_GetOM();

    CCSRADataLoader::SLoaderParams params;
    string csra_name, id;
    TSeqPos from, to, align_count;

    {
        csra_name = "SRR389414";
        id = "gnl|SRA|SRR389414/1";
        from = 1000000;
        to   = 2000000;
        align_count = 6104; // 5400 if mapq>=1
    }
    string loader_name =
        CCSRADataLoader::RegisterInObjectManager(*om, params,
                                                 CObjectManager::eDefault)
        .GetLoader()->GetName();
    sx_ReportCSraLoaderName(loader_name);
    CScope scope(*om);
    scope.AddDefaults();

    string annot_name = csra_name;
    string pileup_name = annot_name+PILEUP_NAME_SUFFIX;

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CRef<CSeq_loc> loc(new CSeq_loc);
    loc->SetInt().SetId(*seqid);
    loc->SetInt().SetFrom(from);
    loc->SetInt().SetTo(to);
    sx_CheckNames(scope, *loc, annot_name);
    SAnnotSelector sel(CSeq_annot::C_Data::e_Align);
    sel.SetSearchUnresolved();
    sel.ExcludeNamedAnnots(pileup_name);

    BOOST_CHECK(scope.GetBioseqHandle(idh));
    //NcbiCout<<MSerial_AsnText<<*scope.GetBioseqHandle(idh).GetCompleteObject();
    if ( 1 ) {
        CGraph_CI git(scope, *loc, sel);
        BOOST_CHECK_EQUAL(git.GetSize(), 1u);
    }

    if ( 1 ) {
        CAlign_CI it(scope, *loc, sel);
        if ( it ) {
            cout << "Align count: "<<it.GetSize()<<endl;
            if ( it.GetAnnot().IsNamed() ) {
                cout << "Annot name: " << it.GetAnnot().GetName()<<endl;
            }
        }
        BOOST_CHECK_EQUAL(align_count, it.GetSize());

        for ( ; it; ++it ) {
            const CSeq_align& align = *it;
            ITERATE(CDense_seg::TIds, j, align.GetSegs().GetDenseg().GetIds()) {
                sx_CheckSeq(scope, idh, **j);
            }
        }
    }

    if ( 1 ) {
        sel.ResetAnnotsNames();
        sel.AddNamedAnnots(pileup_name);
        CGraph_CI git(scope, *loc, sel);
        BOOST_CHECK(git.GetSize() % 6 == 0);
        if ( 0 ) {
            for ( ; git; ++git ) {
                NcbiCout << MSerial_AsnText << git->GetOriginalGraph();
            }
        }
    }
}


BOOST_AUTO_TEST_CASE(FetchSeq8)
{
    // pileup graph test
    CRef<CObjectManager> om = sx_GetOM();

    CCSRADataLoader::SLoaderParams params;
    string csra_name, id;
    TSeqPos from, to, align_count;

    {
        csra_name = "ERR669165";
        id = "GK000001.2";
        from = 922690;
        to   = 922760;
        align_count = 87;
    }
    params.m_CSRAFiles.push_back(csra_name);
    CGBDataLoader::RegisterInObjectManager(*om);
    string loader_name =
        CCSRADataLoader::RegisterInObjectManager(*om, params,
                                                 CObjectManager::eDefault)
        .GetLoader()->GetName();
    sx_ReportCSraLoaderName(loader_name);
    CScope scope(*om);
    scope.AddDefaults();

    string annot_name = csra_name;
    string pileup_name = annot_name+PILEUP_NAME_SUFFIX;

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CRef<CSeq_loc> loc(new CSeq_loc);
    loc->SetInt().SetId(*seqid);
    loc->SetInt().SetFrom(from);
    loc->SetInt().SetTo(to);
    sx_CheckNames(scope, *loc, annot_name);
    SAnnotSelector sel(CSeq_annot::C_Data::e_Align);
    sel.SetSearchUnresolved();
    sel.ExcludeNamedAnnots(pileup_name);

    BOOST_CHECK(scope.GetBioseqHandle(idh));
    if ( 1 ) {
        CGraph_CI git(scope, *loc, sel);
        BOOST_CHECK_EQUAL(git.GetSize(), 1u);
    }

    if ( 1 ) {
        CAlign_CI it(scope, *loc, sel);
        if ( it ) {
            cout << "Align count: "<<it.GetSize()<<endl;
            if ( it.GetAnnot().IsNamed() ) {
                cout << "Annot name: " << it.GetAnnot().GetName()<<endl;
            }
        }
        BOOST_CHECK_EQUAL(align_count, it.GetSize());

        for ( ; it; ++it ) {
            const CSeq_align& align = *it;
            ITERATE(CDense_seg::TIds, j, align.GetSegs().GetDenseg().GetIds()) {
                sx_CheckSeq(scope, idh, **j);
            }
        }
    }

    if ( 1 ) {
        sel.ResetAnnotsNames();
        sel.AddNamedAnnots(pileup_name);
        CGraph_CI git(scope, *loc, sel);
        BOOST_CHECK_EQUAL(git.GetSize(), 6u);
        for ( size_t k = 0; git && k < 6; ++k, ++git ) {
            const CSeq_graph& graph = git->GetOriginalGraph();
            string title = graph.GetTitle();
            NcbiCout << "Pileup graph: " << title << NcbiEndl;
            typedef unsigned TExpectedPair[2];
            const TExpectedPair* expected_pairs = 0;
            size_t expected_count = 0;
            if ( title == "Number of inserts" ) {
                static const TExpectedPair expected_I[] = {
                    { 0, 0 },
                };
                expected_pairs = expected_I;
                expected_count = ArraySize(expected_I);
            }
            else if ( title == "Number of A bases" ) {
                static const TExpectedPair expected_A[] = {
                    { 0, 0 },
                };
                expected_pairs = expected_A;
                expected_count = ArraySize(expected_A);
            }
            else if ( title == "Number of C bases" ) {
                static const TExpectedPair expected_C[] = {
                    { 922695, 1 },
                };
                expected_pairs = expected_C;
                expected_count = ArraySize(expected_C);
            }
            else if ( title == "Number of G bases" ) {
                static const TExpectedPair expected_G[] = {
                    { 922693, 2 },
                };
                expected_pairs = expected_G;
                expected_count = ArraySize(expected_G);
            }
            else if ( title == "Number of T bases" ) {
                static const TExpectedPair expected_T[] = {
                    { 922692, 1 },
                    { 922693, 2 },
                    { 922695, 1 },
                    { 922750, 1 },
                };
                expected_pairs = expected_T;
                expected_count = ArraySize(expected_T);
            }
            else {
                BOOST_REQUIRE_EQUAL(title, "Number of matches");
                static const TExpectedPair expected_M[] = {
                    { 922690, 70 },
                    { 922691, 74 },
                    { 922692, 74 },
                    { 922693, 75 },
                    { 922694, 80 },
                    { 922695, 81 },
                    { 922696, 83 },
                    { 922697, 83 },
                    { 922698, 84 },
                    { 922699, 85 },
                    { 922745, 1 },
                    { 922746, 1 },
                    { 922747, 1 },
                    { 922748, 1 },
                    { 922749, 1 },
                    { 922750, 1 },
                    { 922751, 2 },
                    { 922752, 2 },
                    { 922753, 2 },
                    { 922754, 2 },
                    { 922755, 2 },
                    { 922756, 2 },
                    { 922757, 2 },
                    { 922758, 2 },
                    { 922759, 2 },
                    { 922760, 2 },
                };
                expected_pairs = expected_M;
                expected_count = ArraySize(expected_M);
            }
            map<TSeqPos, unsigned> expected;
            for ( size_t i = 0; i < expected_count; ++i ) {
                expected[expected_pairs[i][0]] = expected_pairs[i][1];
            }
            CRange<TSeqPos> range = graph.GetLoc().GetTotalRange();
            for ( TSeqPos pos = from; pos <= to; ++pos ) {
                TSeqPos i = pos - range.GetFrom();
                unsigned pileup_value =
                    graph.GetGraph().IsByte()?
                    graph.GetGraph().GetByte().GetValues()[i]:
                    graph.GetGraph().GetInt().GetValues()[i];
                unsigned expected_value =
                    expected.count(pos)? expected[pos]: 0;
                if ( false && pileup_value ) {
                    NcbiCout << pos << ": " << pileup_value << NcbiEndl;
                }
                BOOST_CHECK_EQUAL(pileup_value, expected_value);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(FetchSeq9)
{
    // pileup graph test on chunks border in gaps
    CRef<CObjectManager> om = sx_GetOM();

    CCSRADataLoader::SLoaderParams params;
    string csra_name, id;
    TSeqPos from, to, align_count, align_count_over;

    {
        csra_name = "ERR669165";
        id = "GK000001.2";
        from = 9701400;
        to   = 9701420;
        align_count = 0;
        align_count_over = 58;
    }
    params.m_CSRAFiles.push_back(csra_name);
    CGBDataLoader::RegisterInObjectManager(*om);
    string loader_name =
        CCSRADataLoader::RegisterInObjectManager(*om, params,
                                                 CObjectManager::eDefault)
        .GetLoader()->GetName();
    sx_ReportCSraLoaderName(loader_name);
    CScope scope(*om);
    scope.AddDefaults();

    string annot_name = csra_name;
    string pileup_name = annot_name+PILEUP_NAME_SUFFIX;

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CRef<CSeq_loc> loc(new CSeq_loc);
    loc->SetInt().SetId(*seqid);
    loc->SetInt().SetFrom(from);
    loc->SetInt().SetTo(to);
    sx_CheckNames(scope, *loc, annot_name);
    SAnnotSelector sel(CSeq_annot::C_Data::e_Align);
    sel.SetSearchUnresolved();
    sel.ExcludeNamedAnnots(pileup_name);

    BOOST_CHECK(scope.GetBioseqHandle(idh));
    if ( 1 ) {
        CGraph_CI git(scope, *loc, sel);
        BOOST_CHECK_EQUAL(git.GetSize(), 1u);
    }

    if ( 1 ) {
        CAlign_CI it(scope, *loc, sel);
        if ( it ) {
            cout << "Align count: "<<it.GetSize()<<endl;
            if ( it.GetAnnot().IsNamed() ) {
                cout << "Annot name: " << it.GetAnnot().GetName()<<endl;
            }
        }
        BOOST_CHECK_EQUAL(align_count, it.GetSize());

        for ( ; it; ++it ) {
            const CSeq_align& align = *it;
            ITERATE(CDense_seg::TIds, j, align.GetSegs().GetDenseg().GetIds()) {
                sx_CheckSeq(scope, idh, **j);
            }
        }
    }

    if ( 1 ) {
        SAnnotSelector sel2 = sel; sel2.SetOverlapTotalRange();
        CAlign_CI it(scope, *loc, sel2);
        if ( it ) {
            cout << "Align count: "<<it.GetSize()<<endl;
            if ( it.GetAnnot().IsNamed() ) {
                cout << "Annot name: " << it.GetAnnot().GetName()<<endl;
            }
        }
        BOOST_CHECK_EQUAL(align_count_over, it.GetSize());

        for ( ; it; ++it ) {
            const CSeq_align& align = *it;
            ITERATE(CDense_seg::TIds, j, align.GetSegs().GetDenseg().GetIds()) {
                sx_CheckSeq(scope, idh, **j);
            }
        }
    }

    if ( 1 ) {
        sel.ResetAnnotsNames();
        sel.AddNamedAnnots(pileup_name);
        CGraph_CI git(scope, *loc, sel);
        //BOOST_CHECK_EQUAL(git.GetSize(), 12u);
        for ( size_t k = 0; git && k < 12; ++k, ++git ) {
            const CSeq_graph& graph = git->GetOriginalGraph();
            string title = graph.GetTitle();
            NcbiCout << "Pileup graph: " << title << NcbiEndl;
            typedef unsigned TExpectedPair[2];
            const TExpectedPair* expected_pairs = 0;
            size_t expected_count = 0;
            if ( title == "Number of inserts" ) {
                static const TExpectedPair expected_I[] = {
                    { 0, 0 },
                };
                expected_pairs = expected_I;
                expected_count = ArraySize(expected_I);
            }
            else if ( title == "Number of A bases" ) {
                static const TExpectedPair expected_A[] = {
                    { 0, 0 },
                };
                expected_pairs = expected_A;
                expected_count = ArraySize(expected_A);
            }
            else if ( title == "Number of C bases" ) {
                static const TExpectedPair expected_C[] = {
                    { 0, 0 },
                };
                expected_pairs = expected_C;
                expected_count = ArraySize(expected_C);
            }
            else if ( title == "Number of G bases" ) {
                static const TExpectedPair expected_G[] = {
                    { 0, 0 },
                };
                expected_pairs = expected_G;
                expected_count = ArraySize(expected_G);
            }
            else if ( title == "Number of T bases" ) {
                static const TExpectedPair expected_T[] = {
                    { 0, 0 },
                };
                expected_pairs = expected_T;
                expected_count = ArraySize(expected_T);
            }
            else {
                BOOST_REQUIRE_EQUAL(title, "Number of matches");
                static const TExpectedPair expected_M[] = {
                    { 0, 0 },
                };
                expected_pairs = expected_M;
                expected_count = ArraySize(expected_M);
            }
            map<TSeqPos, unsigned> expected;
            for ( size_t i = 0; i < expected_count; ++i ) {
                expected[expected_pairs[i][0]] = expected_pairs[i][1];
            }
            CRange<TSeqPos> graph_range = graph.GetLoc().GetTotalRange();
            CRange<TSeqPos> range =
                graph_range.IntersectionWith(CRange<TSeqPos>(from, to));
            BOOST_CHECK(!range.Empty());
            for ( TSeqPos pos = range.GetFrom(); pos <= range.GetTo(); ++pos ) {
                TSeqPos i = pos - graph_range.GetFrom();
                unsigned pileup_value =
                    graph.GetGraph().IsByte()?
                    graph.GetGraph().GetByte().GetValues()[i]:
                    graph.GetGraph().GetInt().GetValues()[i];
                unsigned expected_value =
                    expected.count(pos)? expected[pos]: 0;
                if ( false && pileup_value ) {
                    NcbiCout << pos << ": " << pileup_value << NcbiEndl;
                }
                BOOST_CHECK_EQUAL(pileup_value, expected_value);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(FetchSeq10)
{
    // pileup graph test on chunks border
    CRef<CObjectManager> om = sx_GetOM();

    CCSRADataLoader::SLoaderParams params;
    string csra_name, id;
    TSeqPos from, to, align_count, align_count_over;

    {
        csra_name = "ERR669165";
        id = "GK000001.2";
        from = 147027820;
        to   = 147027840;
        align_count = 683;
        align_count_over = 683;
    }
    params.m_CSRAFiles.push_back(csra_name);
    CGBDataLoader::RegisterInObjectManager(*om);
    string loader_name =
        CCSRADataLoader::RegisterInObjectManager(*om, params,
                                                 CObjectManager::eDefault)
        .GetLoader()->GetName();
    sx_ReportCSraLoaderName(loader_name);
    CScope scope(*om);
    scope.AddDefaults();

    string annot_name = csra_name;
    string pileup_name = annot_name+PILEUP_NAME_SUFFIX;

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CRef<CSeq_loc> loc(new CSeq_loc);
    loc->SetInt().SetId(*seqid);
    loc->SetInt().SetFrom(from);
    loc->SetInt().SetTo(to);
    sx_CheckNames(scope, *loc, annot_name);
    SAnnotSelector sel(CSeq_annot::C_Data::e_Align);
    sel.SetSearchUnresolved();
    sel.ExcludeNamedAnnots(pileup_name);

    BOOST_CHECK(scope.GetBioseqHandle(idh));
    if ( 1 ) {
        CGraph_CI git(scope, *loc, sel);
        BOOST_CHECK_EQUAL(git.GetSize(), 1u);
    }

    if ( 1 ) {
        CAlign_CI it(scope, *loc, sel);
        if ( it ) {
            cout << "Align count: "<<it.GetSize()<<endl;
            if ( it.GetAnnot().IsNamed() ) {
                cout << "Annot name: " << it.GetAnnot().GetName()<<endl;
            }
        }
        BOOST_CHECK_EQUAL(align_count, it.GetSize());

        for ( ; it; ++it ) {
            const CSeq_align& align = *it;
            ITERATE(CDense_seg::TIds, j, align.GetSegs().GetDenseg().GetIds()) {
                sx_CheckSeq(scope, idh, **j);
            }
        }
    }

    if ( 1 ) {
        SAnnotSelector sel2 = sel; sel2.SetOverlapTotalRange();
        CAlign_CI it(scope, *loc, sel2);
        if ( it ) {
            cout << "Align count: "<<it.GetSize()<<endl;
            if ( it.GetAnnot().IsNamed() ) {
                cout << "Annot name: " << it.GetAnnot().GetName()<<endl;
            }
        }
        BOOST_CHECK_EQUAL(align_count_over, it.GetSize());

        for ( ; it; ++it ) {
            const CSeq_align& align = *it;
            ITERATE(CDense_seg::TIds, j, align.GetSegs().GetDenseg().GetIds()) {
                sx_CheckSeq(scope, idh, **j);
            }
        }
    }

    if ( 1 ) {
        sel.ResetAnnotsNames();
        sel.AddNamedAnnots(pileup_name);
        CGraph_CI git(scope, *loc, sel);
        //BOOST_CHECK_EQUAL(git.GetSize(), 12u);
        for ( size_t k = 0; git && k < 12; ++k, ++git ) {
            const CSeq_graph& graph = git->GetOriginalGraph();
            string title = graph.GetTitle();
            NcbiCout << "Pileup graph: " << title << NcbiEndl;
            typedef unsigned TExpectedPair[2];
            const TExpectedPair* expected_pairs = 0;
            size_t expected_count = 0;
            if ( title == "Number of inserts" ) {
                static const TExpectedPair expected_I[] = {
                    { 0, 0 },
                };
                expected_pairs = expected_I;
                expected_count = ArraySize(expected_I);
            }
            else if ( title == "Number of A bases" ) {
                static const TExpectedPair expected_A[] = {
                    { 147027821, 4 },
                    { 147027822, 6 },
                    { 147027825, 1 },
                    { 147027828, 1 },
                    { 147027829, 4 },
                    { 147027830, 10 },
                    { 147027832, 2 },
                };
                expected_pairs = expected_A;
                expected_count = ArraySize(expected_A);
            }
            else if ( title == "Number of C bases" ) {
                static const TExpectedPair expected_C[] = {
                    { 147027820, 1 },
                    { 147027821, 1 },
                    { 147027827, 2 },
                    { 147027829, 1 },
                    { 147027832, 1 },
                    { 147027837, 1 },
                };
                expected_pairs = expected_C;
                expected_count = ArraySize(expected_C);
            }
            else if ( title == "Number of G bases" ) {
                static const TExpectedPair expected_G[] = {
                    { 147027822, 1 },
                    { 147027826, 1 },
                    { 147027827, 1 },
                };
                expected_pairs = expected_G;
                expected_count = ArraySize(expected_G);
            }
            else if ( title == "Number of T bases" ) {
                static const TExpectedPair expected_T[] = {
                    { 147027821, 1 },
                    { 147027822, 1 },
                    { 147027823, 1 },
                    { 147027826, 2 },
                    { 147027827, 2 },
                    { 147027831, 1 },
                    { 147027832, 1 },
                    { 147027833, 1 },
                    { 147027836, 4 },
                    { 147027837, 1 },
                };
                expected_pairs = expected_T;
                expected_count = ArraySize(expected_T);
            }
            else {
                BOOST_REQUIRE_EQUAL(title, "Number of matches");
                static const TExpectedPair expected_M[] = {
                    { 147027820, 602 },
                    { 147027821, 597 },
                    { 147027822, 600 },
                    { 147027823, 597 },
                    { 147027824, 590 },
                    { 147027825, 598 },
                    { 147027826, 597 },
                    { 147027827, 599 },
                    { 147027828, 603 },
                    { 147027829, 581 },
                    { 147027830, 561 },
                    { 147027831, 561 },
                    { 147027832, 552 },
                    { 147027833, 559 },
                    { 147027834, 547 },
                    { 147027835, 546 },
                    { 147027836, 541 },
                    { 147027837, 545 },
                    { 147027838, 8 },
                    { 147027839, 8 },
                    { 147027840, 8 },
                };
                expected_pairs = expected_M;
                expected_count = ArraySize(expected_M);
            }
            map<TSeqPos, unsigned> expected;
            for ( size_t i = 0; i < expected_count; ++i ) {
                expected[expected_pairs[i][0]] = expected_pairs[i][1];
            }
            CRange<TSeqPos> graph_range = graph.GetLoc().GetTotalRange();
            CRange<TSeqPos> range =
                graph_range.IntersectionWith(CRange<TSeqPos>(from, to));
            BOOST_CHECK(!range.Empty());
            for ( TSeqPos pos = range.GetFrom(); pos <= range.GetTo(); ++pos ) {
                TSeqPos i = pos - graph_range.GetFrom();
                unsigned pileup_value =
                    graph.GetGraph().IsByte()?
                    graph.GetGraph().GetByte().GetValues()[i]:
                    graph.GetGraph().GetInt().GetValues()[i];
                unsigned expected_value =
                    expected.count(pos)? expected[pos]: 0;
                if ( false && pileup_value ) {
                    NcbiCout << pos << ": " << pileup_value << NcbiEndl;
                }
                BOOST_CHECK_EQUAL(pileup_value, expected_value);
            }
        }
    }
}


BOOST_AUTO_TEST_CASE(FetchSeq11)
{
    // pileup graph test on chunks and page border
    CRef<CObjectManager> om = sx_GetOM();

    CCSRADataLoader::SLoaderParams params;
    string csra_name, id;
    TSeqPos from, to, align_count, align_count_over;

    {
        csra_name = "ERR669165";
        id = "GK000001.2";
        from = 59805960;
        to   = 59805980;
        align_count = 6;
        align_count_over = 6;
    }
    params.m_CSRAFiles.push_back(csra_name);
    CGBDataLoader::RegisterInObjectManager(*om);
    string loader_name =
        CCSRADataLoader::RegisterInObjectManager(*om, params,
                                                 CObjectManager::eDefault)
        .GetLoader()->GetName();
    sx_ReportCSraLoaderName(loader_name);
    CScope scope(*om);
    scope.AddDefaults();

    string annot_name = csra_name;
    string pileup_name = annot_name+PILEUP_NAME_SUFFIX;

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CRef<CSeq_loc> loc(new CSeq_loc);
    loc->SetInt().SetId(*seqid);
    loc->SetInt().SetFrom(from);
    loc->SetInt().SetTo(to);
    sx_CheckNames(scope, *loc, annot_name);
    SAnnotSelector sel(CSeq_annot::C_Data::e_Align);
    sel.SetSearchUnresolved();
    sel.ExcludeNamedAnnots(pileup_name);

    BOOST_CHECK(scope.GetBioseqHandle(idh));
    if ( 1 ) {
        CGraph_CI git(scope, *loc, sel);
        BOOST_CHECK_EQUAL(git.GetSize(), 1u);
    }

    if ( 1 ) {
        CAlign_CI it(scope, *loc, sel);
        if ( it ) {
            cout << "Align count: "<<it.GetSize()<<endl;
            if ( it.GetAnnot().IsNamed() ) {
                cout << "Annot name: " << it.GetAnnot().GetName()<<endl;
            }
        }
        BOOST_CHECK_EQUAL(align_count, it.GetSize());

        for ( ; it; ++it ) {
            const CSeq_align& align = *it;
            ITERATE(CDense_seg::TIds, j, align.GetSegs().GetDenseg().GetIds()) {
                sx_CheckSeq(scope, idh, **j);
            }
        }
    }

    if ( 1 ) {
        SAnnotSelector sel2 = sel; sel2.SetOverlapTotalRange();
        CAlign_CI it(scope, *loc, sel2);
        if ( it ) {
            cout << "Align count: "<<it.GetSize()<<endl;
            if ( it.GetAnnot().IsNamed() ) {
                cout << "Annot name: " << it.GetAnnot().GetName()<<endl;
            }
        }
        BOOST_CHECK_EQUAL(align_count_over, it.GetSize());

        for ( ; it; ++it ) {
            const CSeq_align& align = *it;
            ITERATE(CDense_seg::TIds, j, align.GetSegs().GetDenseg().GetIds()) {
                sx_CheckSeq(scope, idh, **j);
            }
        }
    }

    if ( 1 ) {
        sel.ResetAnnotsNames();
        sel.AddNamedAnnots(pileup_name);
        CGraph_CI git(scope, *loc, sel);
        BOOST_CHECK_EQUAL(git.GetSize(), 6u);
        for ( size_t k = 0; git && k < 12; ++k, ++git ) {
            const CSeq_graph& graph = git->GetOriginalGraph();
            string title = graph.GetTitle();
            NcbiCout << "Pileup graph: " << title << NcbiEndl;
            typedef unsigned TExpectedPair[2];
            const TExpectedPair* expected_pairs = 0;
            size_t expected_count = 0;
            if ( title == "Number of inserts" ) {
                static const TExpectedPair expected_I[] = {
                    { 0, 0 },
                };
                expected_pairs = expected_I;
                expected_count = ArraySize(expected_I);
            }
            else if ( title == "Number of A bases" ) {
                static const TExpectedPair expected_A[] = {
                    { 0, 0 },
                };
                expected_pairs = expected_A;
                expected_count = ArraySize(expected_A);
            }
            else if ( title == "Number of C bases" ) {
                static const TExpectedPair expected_C[] = {
                    { 0, 0 },
                };
                expected_pairs = expected_C;
                expected_count = ArraySize(expected_C);
            }
            else if ( title == "Number of G bases" ) {
                static const TExpectedPair expected_G[] = {
                    { 0, 0 },
                };
                expected_pairs = expected_G;
                expected_count = ArraySize(expected_G);
            }
            else if ( title == "Number of T bases" ) {
                static const TExpectedPair expected_T[] = {
                    { 59805974, 1 },
                };
                expected_pairs = expected_T;
                expected_count = ArraySize(expected_T);
            }
            else {
                BOOST_REQUIRE_EQUAL(title, "Number of matches");
                static const TExpectedPair expected_M[] = {
                    { 59805960, 4 },
                    { 59805961, 4 },
                    { 59805962, 4 },
                    { 59805963, 4 },
                    { 59805964, 4 },
                    { 59805965, 3 },
                    { 59805966, 3 },
                    { 59805967, 3 },
                    { 59805968, 3 },
                    { 59805969, 3 },
                    { 59805970, 3 },
                    { 59805971, 3 },
                    { 59805972, 3 },
                    { 59805973, 3 },
                    { 59805974, 3 },
                    { 59805975, 5 },
                    { 59805976, 5 },
                    { 59805977, 5 },
                    { 59805978, 5 },
                    { 59805979, 5 },
                    { 59805980, 5 },
                };
                expected_pairs = expected_M;
                expected_count = ArraySize(expected_M);
            }
            map<TSeqPos, unsigned> expected;
            for ( size_t i = 0; i < expected_count; ++i ) {
                expected[expected_pairs[i][0]] = expected_pairs[i][1];
            }
            CRange<TSeqPos> graph_range = graph.GetLoc().GetTotalRange();
            CRange<TSeqPos> range =
                graph_range.IntersectionWith(CRange<TSeqPos>(from, to));
            BOOST_CHECK(!range.Empty());
            for ( TSeqPos pos = range.GetFrom(); pos <= range.GetTo(); ++pos ) {
                TSeqPos i = pos - graph_range.GetFrom();
                unsigned pileup_value =
                    graph.GetGraph().IsByte()?
                    graph.GetGraph().GetByte().GetValues()[i]:
                    graph.GetGraph().GetInt().GetValues()[i];
                unsigned expected_value =
                    expected.count(pos)? expected[pos]: 0;
                if ( false && pileup_value ) {
                    NcbiCout << pos << ": " << pileup_value << NcbiEndl;
                }
                BOOST_CHECK_EQUAL(pileup_value, expected_value);
            }
        }
    }
}


BOOST_AUTO_TEST_CASE(FetchSeq12)
{
    // pileup graph test on chunks and page border
    CRef<CObjectManager> om = sx_GetOM();

    CCSRADataLoader::SLoaderParams params;
    string csra_name, id;
    TSeqPos from, to, align_count, align_count_over;

    {
        csra_name = "SRR1686476";
        id = "NC_011294.1";
        from = 805960;
        to   = 805980;
        align_count = 81;
        align_count_over = 81;
    }
    params.m_CSRAFiles.push_back(csra_name);
    CGBDataLoader::RegisterInObjectManager(*om);
    string loader_name =
        CCSRADataLoader::RegisterInObjectManager(*om, params,
                                                 CObjectManager::eDefault)
        .GetLoader()->GetName();
    sx_ReportCSraLoaderName(loader_name);
    CScope scope(*om);
    scope.AddDefaults();

    string annot_name = csra_name;
    string pileup_name = annot_name+PILEUP_NAME_SUFFIX;

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CRef<CSeq_loc> loc(new CSeq_loc);
    loc->SetInt().SetId(*seqid);
    loc->SetInt().SetFrom(from);
    loc->SetInt().SetTo(to);
    sx_CheckNames(scope, *loc, annot_name);
    SAnnotSelector sel(CSeq_annot::C_Data::e_Align);
    sel.SetSearchUnresolved();
    sel.ExcludeNamedAnnots(pileup_name);

    BOOST_CHECK(scope.GetBioseqHandle(idh));
    if ( 1 ) {
        CGraph_CI git(scope, *loc, sel);
        BOOST_CHECK_EQUAL(git.GetSize(), 1u);
    }

    if ( 1 ) {
        CAlign_CI it(scope, *loc, sel);
        if ( it ) {
            cout << "Align count: "<<it.GetSize()<<endl;
            if ( it.GetAnnot().IsNamed() ) {
                cout << "Annot name: " << it.GetAnnot().GetName()<<endl;
            }
        }
        BOOST_CHECK_EQUAL(align_count, it.GetSize());

        for ( ; it; ++it ) {
            const CSeq_align& align = *it;
            ITERATE(CDense_seg::TIds, j, align.GetSegs().GetDenseg().GetIds()) {
                sx_CheckSeq(scope, idh, **j);
            }
        }
    }

    if ( 1 ) {
        SAnnotSelector sel2 = sel; sel2.SetOverlapTotalRange();
        CAlign_CI it(scope, *loc, sel2);
        if ( it ) {
            cout << "Align count: "<<it.GetSize()<<endl;
            if ( it.GetAnnot().IsNamed() ) {
                cout << "Annot name: " << it.GetAnnot().GetName()<<endl;
            }
        }
        BOOST_CHECK_EQUAL(align_count_over, it.GetSize());

        for ( ; it; ++it ) {
            const CSeq_align& align = *it;
            ITERATE(CDense_seg::TIds, j, align.GetSegs().GetDenseg().GetIds()) {
                sx_CheckSeq(scope, idh, **j);
            }
        }
    }

    if ( 1 ) {
        sel.ResetAnnotsNames();
        sel.AddNamedAnnots(pileup_name);
        CGraph_CI git(scope, *loc, sel);
        BOOST_CHECK_EQUAL(git.GetSize(), 6u);
        for ( size_t k = 0; git && k < 12; ++k, ++git ) {
            const CSeq_graph& graph = git->GetOriginalGraph();
            string title = graph.GetTitle();
            NcbiCout << "Pileup graph: " << title << NcbiEndl;
            typedef unsigned TExpectedPair[2];
            const TExpectedPair* expected_pairs = 0;
            size_t expected_count = 0;
            if ( title == "Number of inserts" ) {
                static const TExpectedPair expected_I[] = {
                    { 0, 0 },
                };
                expected_pairs = expected_I;
                expected_count = ArraySize(expected_I);
            }
            else if ( title == "Number of A bases" ) {
                static const TExpectedPair expected_A[] = {
                    { 805967, 1 },
                    { 805977, 1 },
                };
                expected_pairs = expected_A;
                expected_count = ArraySize(expected_A);
            }
            else if ( title == "Number of C bases" ) {
                static const TExpectedPair expected_C[] = {
                    { 805960, 1 },
                };
                expected_pairs = expected_C;
                expected_count = ArraySize(expected_C);
            }
            else if ( title == "Number of G bases" ) {
                static const TExpectedPair expected_G[] = {
                    { 805968, 1 },
                };
                expected_pairs = expected_G;
                expected_count = ArraySize(expected_G);
            }
            else if ( title == "Number of T bases" ) {
                static const TExpectedPair expected_T[] = {
                    { 805963, 1 },
                    { 805965, 1 },
                    { 805966, 1 },
                };
                expected_pairs = expected_T;
                expected_count = ArraySize(expected_T);
            }
            else {
                BOOST_REQUIRE_EQUAL(title, "Number of matches");
                static const TExpectedPair expected_M[] = {
                    { 805960, 69 },
                    { 805961, 70 },
                    { 805962, 72 },
                    { 805963, 72 },
                    { 805964, 74 },
                    { 805965, 70 },
                    { 805966, 72 },
                    { 805967, 72 },
                    { 805968, 70 },
                    { 805969, 71 },
                    { 805970, 70 },
                    { 805971, 70 },
                    { 805972, 71 },
                    { 805973, 69 },
                    { 805974, 70 },
                    { 805975, 70 },
                    { 805976, 70 },
                    { 805977, 70 },
                    { 805978, 69 },
                    { 805979, 69 },
                    { 805980, 70 },
                };
                expected_pairs = expected_M;
                expected_count = ArraySize(expected_M);
            }
            map<TSeqPos, unsigned> expected;
            for ( size_t i = 0; i < expected_count; ++i ) {
                expected[expected_pairs[i][0]] = expected_pairs[i][1];
            }
            CRange<TSeqPos> graph_range = graph.GetLoc().GetTotalRange();
            CRange<TSeqPos> range =
                graph_range.IntersectionWith(CRange<TSeqPos>(from, to));
            BOOST_CHECK(!range.Empty());
            for ( TSeqPos pos = range.GetFrom(); pos <= range.GetTo(); ++pos ) {
                TSeqPos i = pos - graph_range.GetFrom();
                unsigned pileup_value =
                    graph.GetGraph().IsByte()?
                    graph.GetGraph().GetByte().GetValues()[i]:
                    graph.GetGraph().GetInt().GetValues()[i];
                unsigned expected_value =
                    expected.count(pos)? expected[pos]: 0;
                if ( false && pileup_value ) {
                    NcbiCout << pos << ": " << pileup_value << NcbiEndl;
                }
                BOOST_CHECK_EQUAL(pileup_value, expected_value);
            }
        }
    }
}

#if 0
// huge cSRA file loading
BOOST_AUTO_TEST_CASE(FetchSeq8)
{
    CCSRADataLoader::SetPileupGraphsParamDefault(true);

    CStopWatch sw(CStopWatch::eStart);
    CRef<CObjectManager> om = sx_GetOM();
    NcbiCout << "Loaded OM in " << sw.Restart() << NcbiEndl;

    CCSRADataLoader::SLoaderParams params;
    string csra_name, id;
    TSeqPos from, to, align_count;

    {
        csra_name = "/panfs/traces01/compress/qa/yaschenk/RICHA/NA12878.richa.hiseq.primary_full.csra";
        id = "NC_000005.9";
        from = 20000;
        to   = 25000;
        align_count = 2636; // 5400 if mapq>=1
    }
    params.m_CSRAFiles.push_back(csra_name);
    CGBDataLoader::RegisterInObjectManager(*om);
    string loader_name =
        CCSRADataLoader::RegisterInObjectManager(*om, params,
                                                 CObjectManager::eDefault)
        .GetLoader()->GetName();
    NcbiCout << "Registered cSRA loader in " << sw.Restart() << NcbiEndl;
    sx_ReportCSraLoaderName(loader_name);
    CScope scope(*om);
    scope.AddDefaults();

    string annot_name = csra_name;
    string pileup_name = annot_name+PILEUP_NAME_SUFFIX;

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CRef<CSeq_loc> loc(new CSeq_loc);
    loc->SetInt().SetId(*seqid);
    loc->SetInt().SetFrom(from);
    loc->SetInt().SetTo(to);

    if ( 1 ) {
        BOOST_CHECK(scope.GetBioseqHandle(idh));
        //NcbiCout<<MSerial_AsnText<<*scope.GetBioseqHandle(idh).GetCompleteObject();
        NcbiCout << "Got reference handle in " << sw.Restart() << NcbiEndl;
    }

    if ( 1 ) {
        sx_CheckNames(scope, *loc, annot_name);
        NcbiCout << "Got names in " << sw.Restart() << NcbiEndl;
    }

    SAnnotSelector sel(CSeq_annot::C_Data::e_Align);
    sel.SetSearchUnresolved();

    if ( 1 ) {
        sel.ResetAnnotsNames();
        sel.AddNamedAnnots(annot_name);
        CGraph_CI git(scope, *loc, sel);
        BOOST_CHECK_EQUAL(git.GetSize(), 1u);
        NcbiCout << "Got coverage graph in " << sw.Restart() << NcbiEndl;
        if ( 0 ) {
            for ( ; git; ++git ) {
                NcbiCout << MSerial_AsnText << git->GetOriginalGraph();
            }
        }
    }

    if ( 1 ) {
        sel.ResetAnnotsNames();
        sel.AddNamedAnnots(pileup_name);
        CGraph_CI git(scope, *loc, sel);
        BOOST_CHECK(git.GetSize() % 6 == 0);
        BOOST_CHECK(git.GetSize());
        NcbiCout << "Got pileup graphs in " << sw.Restart() << NcbiEndl;
        if ( 0 ) {
            for ( ; git; ++git ) {
                NcbiCout << MSerial_AsnText << git->GetOriginalGraph();
            }
        }
    }

    if ( 1 ) {
        sel.ResetAnnotsNames();
        sel.AddNamedAnnots(annot_name);
        CAlign_CI it(scope, *loc, sel);
        if ( it ) {
            cout << "Align count: "<<it.GetSize()<<endl;
            if ( it.GetAnnot().IsNamed() ) {
                cout << "Annot name: " << it.GetAnnot().GetName()<<endl;
            }
        }
        BOOST_CHECK_EQUAL(align_count, it.GetSize());
        NcbiCout << "Got alignments in " << sw.Restart() << NcbiEndl;

        for ( ; it; ++it ) {
            const CSeq_align& align = *it;
            ITERATE(CDense_seg::TIds, j, align.GetSegs().GetDenseg().GetIds()) {
                sx_CheckSeq(scope, idh, **j);
            }
        }
        NcbiCout << "Got short reads in " << sw.Restart() << NcbiEndl;
    }
}
#endif
#if 0 // the SRR494718 is not released yet
BOOST_AUTO_TEST_CASE(ShortSeq1x)
{
    CRef<CObjectManager> om = sx_GetOM();

    //putenv("CSRA_LOADER_QUALITY_GRAPHS=true");

    string loader_name =
        CCSRADataLoader::RegisterInObjectManager(*om, CObjectManager::eDefault)
        .GetLoader()->GetName();
    sx_ReportCSraLoaderName(loader_name);

    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> ref_id(new CSeq_id("gnl|SRA|SRR494718/chr1"));
    CRef<CSeq_id> read_id(new CSeq_id("gnl|SRA|SRR494718.7001.1"));
    TSeqPos prim_pos = 4263026;
    TSeqPos sec_pos = 3996256;

    typedef CRange<TSeqPos> TRange;
    CSeq_id_Handle ref_idh = CSeq_id_Handle::GetHandle(*ref_id);
    CSeq_id_Handle read_idh = CSeq_id_Handle::GetHandle(*read_id);

    CBioseq_Handle ref;
    if ( 1 ) {
        ref = scope.GetBioseqHandle(ref_idh);
        BOOST_REQUIRE(ref);
        if ( 1 ) {
            // scan first 100 alignments
            CAlign_CI ait(ref, SAnnotSelector().SetMaxSize(100));
            BOOST_CHECK_EQUAL(ait.GetSize(), 100u);
        }
        if ( 0 ) {
            // scan alignments around primary position
            int found = 0;
            for ( CAlign_CI ait(ref, TRange(prim_pos, prim_pos)); ait; ++ait ) {
                const CSeq_align& align = *ait;
                for ( CTypeConstIterator<CSeq_id> it(Begin(align)); it; ++it ) {
                    if ( read_id->Equals(*it) ) {
                        ++found;
                    }
                }
            }
            BOOST_CHECK_EQUAL(found, 1);
        }
        if ( 1 ) {
            // scan alignments around secondary position
            int found = 0;
            for ( CAlign_CI ait(ref, TRange(sec_pos, sec_pos)); ait; ++ait ) {
                const CSeq_align& align = *ait;
                for ( CTypeConstIterator<CSeq_id> it(Begin(align)); it; ++it ) {
                    if ( read_id->Equals(*it) ) {
                        ++found;
                    }
                }
            }
            BOOST_CHECK_EQUAL(found, 1);
        }
    }

    BOOST_CHECK(scope.GetIds(ref_idh).size() > 0);
    CBioseq_Handle read = scope.GetBioseqHandle(read_idh);
    BOOST_REQUIRE(read);
    if ( 1 ) {
        // quality graph
        CGraph_CI git(read);
        BOOST_CHECK_EQUAL(git.GetSize(), 0u);
    }
    if ( 1 ) {
        // alignment (primary and secondary)
        CAlign_CI ait(read);
        for ( ; ait; ++ait ) {
            NcbiCout << MSerial_AsnText << *ait << NcbiEndl;
        }
        BOOST_CHECK_EQUAL(ait.GetSize(), 2u);
    }
}
#endif

BOOST_AUTO_TEST_CASE(ShortSeq1)
{
    CRef<CObjectManager> om = sx_GetOM();

    //putenv("CSRA_LOADER_QUALITY_GRAPHS=true");

    string loader_name =
        CCSRADataLoader::RegisterInObjectManager(*om, CObjectManager::eDefault)
        .GetLoader()->GetName();
    sx_ReportCSraLoaderName(loader_name);

    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> ref_id(new CSeq_id("gnl|SRA|SRR505887/chr1"));
    CRef<CSeq_id> read_id(new CSeq_id("gnl|SRA|SRR505887.144261.2"));
    TSeqPos prim_pos = 965627;
    TSeqPos sec_pos = 966293;

    typedef CRange<TSeqPos> TRange;
    CSeq_id_Handle ref_idh = CSeq_id_Handle::GetHandle(*ref_id);
    CSeq_id_Handle read_idh = CSeq_id_Handle::GetHandle(*read_id);

    CBioseq_Handle ref;
    if ( 1 ) {
        ref = scope.GetBioseqHandle(ref_idh);
        BOOST_REQUIRE(ref);
        if ( 1 ) {
            // scan first 100 alignments
            CAlign_CI ait(ref, SAnnotSelector().SetMaxSize(100));
            BOOST_CHECK_EQUAL(ait.GetSize(), 100u);
        }
        if ( 0 ) {
            // scan alignments around primary position
            int found = 0;
            for ( CAlign_CI ait(ref, TRange(prim_pos, prim_pos)); ait; ++ait ) {
                const CSeq_align& align = *ait;
                for ( CTypeConstIterator<CSeq_id> it(Begin(align)); it; ++it ) {
                    if ( read_id->Equals(*it) ) {
                        ++found;
                    }
                }
            }
            BOOST_CHECK_EQUAL(found, 1);
        }
        if ( 1 ) {
            // scan alignments around secondary position
            int found = 0;
            for ( CAlign_CI ait(ref, TRange(sec_pos, sec_pos)); ait; ++ait ) {
                const CSeq_align& align = *ait;
                for ( CTypeConstIterator<CSeq_id> it(Begin(align)); it; ++it ) {
                    if ( read_id->Equals(*it) ) {
                        ++found;
                    }
                }
            }
            BOOST_CHECK_EQUAL(found, 1);
        }
    }

    BOOST_CHECK(scope.GetIds(ref_idh).size() > 0);
    CBioseq_Handle read = scope.GetBioseqHandle(read_idh);
    BOOST_REQUIRE(read);
    if ( 1 ) {
        // quality graph
        CGraph_CI git(read);
        BOOST_CHECK_EQUAL(git.GetSize(), 0u);
    }
    if ( 1 ) {
        // alignment (primary and secondary)
        CAlign_CI ait(read);
        for ( ; ait; ++ait ) {
            //NcbiCout << MSerial_AsnText << *ait << NcbiEndl;
        }
        BOOST_CHECK_EQUAL(ait.GetSize(), 2u);
    }
}


BOOST_AUTO_TEST_CASE(ShortSeq2)
{
    CRef<CObjectManager> om = sx_GetOM();

    //putenv("CSRA_LOADER_QUALITY_GRAPHS=true");

    string loader_name =
        CCSRADataLoader::RegisterInObjectManager(*om, CObjectManager::eDefault)
        .GetLoader()->GetName();
    sx_ReportCSraLoaderName(loader_name);

    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> read_id(new CSeq_id("gnl|SRA|SRR035417.1.1"));

    CSeq_id_Handle read_idh = CSeq_id_Handle::GetHandle(*read_id);

    CBioseq_Handle read = scope.GetBioseqHandle(read_idh);
    BOOST_REQUIRE(read);
    if ( 1 ) {
        // quality graph
        CGraph_CI git(read);
        BOOST_CHECK_EQUAL(git.GetSize(), 0u);
    }
    if ( 1 ) {
        // alignment (primary and secondary)
        CAlign_CI ait(read);
        for ( ; ait; ++ait ) {
            //NcbiCout << MSerial_AsnText << *ait << NcbiEndl;
        }
        BOOST_CHECK_EQUAL(ait.GetSize(), 0u);
    }
}


BOOST_AUTO_TEST_CASE(ShortSeq3)
{
    CRef<CObjectManager> om = sx_GetOM();

    //putenv("CSRA_LOADER_QUALITY_GRAPHS=true");

    string loader_name =
        CCSRADataLoader::RegisterInObjectManager(*om, CObjectManager::eDefault)
        .GetLoader()->GetName();
    sx_ReportCSraLoaderName(loader_name);

    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> read_id(new CSeq_id("gnl|SRA|ERR003990.1.1"));

    CSeq_id_Handle read_idh = CSeq_id_Handle::GetHandle(*read_id);

    CBioseq_Handle read = scope.GetBioseqHandle(read_idh);
    BOOST_REQUIRE(read);
    if ( 1 ) {
        // quality graph
        CGraph_CI git(read);
        BOOST_CHECK_EQUAL(git.GetSize(), 0u);
    }
    if ( 1 ) {
        // alignment (primary and secondary)
        CAlign_CI ait(read);
        for ( ; ait; ++ait ) {
            //NcbiCout << MSerial_AsnText << *ait << NcbiEndl;
        }
        BOOST_CHECK_EQUAL(ait.GetSize(), 0u);
    }
}


BOOST_AUTO_TEST_CASE(ShortSeq4)
{
    CRef<CObjectManager> om = sx_GetOM();

    //putenv("CSRA_LOADER_QUALITY_GRAPHS=true");

    string loader_name =
        CCSRADataLoader::RegisterInObjectManager(*om, CObjectManager::eDefault)
        .GetLoader()->GetName();
    sx_ReportCSraLoaderName(loader_name);

    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> read_id(new CSeq_id("gnl|SRA|SRR749060.1.1"));

    CSeq_id_Handle read_idh = CSeq_id_Handle::GetHandle(*read_id);

    CBioseq_Handle read = scope.GetBioseqHandle(read_idh);
    BOOST_REQUIRE(read);
    if ( 1 ) {
        // quality graph
        CGraph_CI git(read);
        BOOST_CHECK_EQUAL(git.GetSize(), 0u);
    }
    if ( 1 ) {
        // alignment (primary and secondary)
        CAlign_CI ait(read);
        for ( ; ait; ++ait ) {
            //NcbiCout << MSerial_AsnText << *ait << NcbiEndl;
        }
        BOOST_CHECK_EQUAL(ait.GetSize(), 0u);
    }
}

/*
BOOST_AUTO_TEST_CASE(ShortSeq5)
{
    CRef<CObjectManager> om = sx_GetOM();

    bool verbose = 1;
    bool graph = 1;

    if ( graph ) {
        putenv("CSRA_LOADER_QUALITY_GRAPHS=true");
    }

    string loader_name =
        CCSRADataLoader::RegisterInObjectManager(*om, CObjectManager::eDefault)
        .GetLoader()->GetName();
    sx_ReportCSraLoaderName(loader_name);

    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> read_id(new CSeq_id("gnl|SRA|SRR500739.422766.2"));

    CSeq_id_Handle read_idh = CSeq_id_Handle::GetHandle(*read_id);

    CBioseq_Handle read = scope.GetBioseqHandle(read_idh);
    BOOST_REQUIRE(read);
    if ( 1 ) {
        // quality graph
        CGraph_CI git(read);
        BOOST_REQUIRE_EQUAL(git.GetSize(), graph? 1u: 0u);
        if ( graph && verbose ) {
            NcbiCout << MSerial_AsnText << git->GetOriginalGraph();
        }
    }
    if ( 1 ) {
        // alignment (primary and secondary)
        CAlign_CI ait(read);
        for ( ; ait; ++ait ) {
            if ( verbose ) {
                NcbiCout << MSerial_AsnText << *ait << NcbiEndl;
            }
        }
        BOOST_CHECK_EQUAL(ait.GetSize(), 0u);
    }
    if ( verbose ) {
        NcbiCout << MSerial_AsnText << *read.GetCompleteObject();
    }
}
*/

BOOST_AUTO_TEST_CASE(MultipleIds1)
{
    CRef<CObjectManager> om = sx_GetOM();

    CCSRADataLoader::SLoaderParams params;
    string csra_name, id;
    TSeqPos from, to, align_count;

    {
        csra_name = "SRR413273";
        id = "121114303";
        from = 100;
        to   = 2000;
        align_count = 1525; // 5400 if mapq>=1
    }
    params.m_DirPath.erase();
    params.m_CSRAFiles.push_back(csra_name);
    string loader_name =
        CCSRADataLoader::RegisterInObjectManager(*om, params,
                                                 CObjectManager::eDefault)
        .GetLoader()->GetName();
    sx_ReportCSraLoaderName(loader_name);
    CScope scope(*om);
    scope.AddDefaults();

    string annot_name = csra_name;
    string pileup_name = annot_name+PILEUP_NAME_SUFFIX;

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CRef<CSeq_loc> loc(new CSeq_loc);
    loc->SetInt().SetId(*seqid);
    loc->SetInt().SetFrom(from);
    loc->SetInt().SetTo(to);
    sx_CheckNames(scope, *loc, annot_name);
    SAnnotSelector sel(CSeq_annot::C_Data::e_Align);
    sel.SetSearchUnresolved();
    sel.ExcludeNamedAnnots(pileup_name);

    //BOOST_REQUIRE(scope.GetBioseqHandle(idh));
    //NcbiCout<<MSerial_AsnText<<*scope.GetBioseqHandle(idh).GetCompleteObject();
    if ( 1 ) {
        CGraph_CI git(scope, *loc, sel);
        BOOST_CHECK_EQUAL(git.GetSize(), 1u);
    }

    if ( 1 ) {
        CAlign_CI it(scope, *loc, sel);
        if ( it ) {
            cout << "Align count: "<<it.GetSize()<<endl;
            if ( it.GetAnnot().IsNamed() ) {
                cout << "Annot name: " << it.GetAnnot().GetName()<<endl;
            }
        }
        BOOST_CHECK_EQUAL(align_count, it.GetSize());

        for ( ; it; ++it ) {
            const CSeq_align& align = *it;
            ITERATE(CDense_seg::TIds, j, align.GetSegs().GetDenseg().GetIds()) {
                sx_CheckSeq(scope, idh, **j);
            }
        }
    }

    if ( 1 ) {
        sel.ResetAnnotsNames();
        sel.AddNamedAnnots(pileup_name);
        CGraph_CI git(scope, *loc, sel);
        BOOST_CHECK(git.GetSize() % 6 == 0);
        if ( 0 ) {
            for ( ; git; ++git ) {
                NcbiCout << MSerial_AsnText << git->GetOriginalGraph();
            }
        }
    }
}


BOOST_AUTO_TEST_CASE(MultipleIds2)
{
    CRef<CObjectManager> om = sx_GetOM();

    CCSRADataLoader::SLoaderParams params;
    string csra_name, id;
    TSeqPos from, to, align_count;

    {
        csra_name = "SRR413273";
        id = "NM_004119.2";
        from = 100;
        to   = 2000;
        align_count = 1525; // 5400 if mapq>=1
    }
    params.m_DirPath.erase();
    params.m_CSRAFiles.push_back(csra_name);
    string loader_name =
        CCSRADataLoader::RegisterInObjectManager(*om, params,
                                                 CObjectManager::eDefault)
        .GetLoader()->GetName();
    sx_ReportCSraLoaderName(loader_name);
    CGBDataLoader::RegisterInObjectManager(*om);
    CScope scope(*om);
    scope.AddDefaults();

    string annot_name = csra_name;
    string pileup_name = annot_name+PILEUP_NAME_SUFFIX;

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CRef<CSeq_loc> loc(new CSeq_loc);
    loc->SetInt().SetId(*seqid);
    loc->SetInt().SetFrom(from);
    loc->SetInt().SetTo(to);
    sx_CheckNames(scope, *loc, annot_name);
    SAnnotSelector sel(CSeq_annot::C_Data::e_Align);
    sel.SetSearchUnresolved();
    sel.ExcludeNamedAnnots(pileup_name);
    sel.ExcludeNamedAnnots("SNP");

    //BOOST_REQUIRE(scope.GetBioseqHandle(idh));
    //NcbiCout<<MSerial_AsnText<<*scope.GetBioseqHandle(idh).GetCompleteObject();
    if ( 1 ) {
        CGraph_CI git(scope, *loc, sel);
        BOOST_CHECK_EQUAL(git.GetSize(), 1u);
    }

    if ( 1 ) {
        CAlign_CI it(scope, *loc, sel);
        if ( it ) {
            cout << "Align count: "<<it.GetSize()<<endl;
            if ( it.GetAnnot().IsNamed() ) {
                cout << "Annot name: " << it.GetAnnot().GetName()<<endl;
            }
        }
        BOOST_CHECK_EQUAL(align_count, it.GetSize());

        for ( ; it; ++it ) {
            const CSeq_align& align = *it;
            ITERATE(CDense_seg::TIds, j, align.GetSegs().GetDenseg().GetIds()) {
                sx_CheckSeq(scope, idh, **j);
            }
        }
    }

    if ( 1 ) {
        sel.ResetAnnotsNames();
        sel.AddNamedAnnots(pileup_name);
        CGraph_CI git(scope, *loc, sel);
        BOOST_CHECK(git.GetSize() % 6 == 0);
        if ( 0 ) {
            for ( ; git; ++git ) {
                NcbiCout << MSerial_AsnText << git->GetOriginalGraph();
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(CheckPrivate1)
{
    CRef<CObjectManager> om = sx_GetOM();

    CCSRADataLoader::SLoaderParams params;

    string path("http://gapsview11.be-md.ncbi.nlm.nih.gov/49DCAC8956E9BD404735BC18EA2E7986C1674A6F/SRR1219902.sra");
    params.m_CSRAFiles.push_back(path);

    string loader_name;
    bool isError = false;
    try {
        NcbiCout << "Trying to create a loader for: " << path << NcbiEndl;
        loader_name = CCSRADataLoader::RegisterInObjectManager(*om, params)
            .GetLoader()->GetName();
        NcbiCout << "Loader created successfully" << NcbiEndl;
        sx_ReportCSraLoaderName(loader_name);
    }
    catch (CSraException& e) {
        isError = true;
        ERR_POST("Exception: "<<e);
        BOOST_CHECK(e.GetErrCode() == e.eProtectedDb);
    }
    BOOST_CHECK(isError);
}


NCBITEST_INIT_TREE()
{
    NCBITEST_DISABLE(FetchSeq1);
    NCBITEST_DISABLE(FetchSeq2);
    NCBITEST_DISABLE(FetchSeq3);
    NCBITEST_DISABLE(FetchSeq4);
    NCBITEST_DISABLE(FetchSeq4l);
    NCBITEST_DISABLE(FetchSeq4sx);
    NCBITEST_DISABLE(FetchSeq4sd);
    NCBITEST_DISABLE(FetchSeq4s);
}
