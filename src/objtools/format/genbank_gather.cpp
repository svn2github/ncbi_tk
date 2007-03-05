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
* Author:  Aaron Ucko, NCBI
*
* File Description:
*   
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/User_field.hpp>
#include <objmgr/seqdesc_ci.hpp>

#include <objtools/format/item_ostream.hpp>
#include <objtools/format/items/locus_item.hpp>
#include <objtools/format/items/defline_item.hpp>
#include <objtools/format/items/accession_item.hpp>
#include <objtools/format/items/version_item.hpp>
#include <objtools/format/items/dbsource_item.hpp>
#include <objtools/format/items/segment_item.hpp>
#include <objtools/format/items/keywords_item.hpp>
#include <objtools/format/items/source_item.hpp>
#include <objtools/format/items/reference_item.hpp>
#include <objtools/format/items/comment_item.hpp>
#include <objtools/format/items/basecount_item.hpp>
#include <objtools/format/items/sequence_item.hpp>
#include <objtools/format/items/ctrl_items.hpp>
#include <objtools/format/items/feature_item.hpp>
#include <objtools/format/items/primary_item.hpp>
#include <objtools/format/items/wgs_item.hpp>
#include <objtools/format/items/genome_item.hpp>
#include <objtools/format/items/contig_item.hpp>
#include <objtools/format/items/origin_item.hpp>
#include <objtools/format/items/genome_project_item.hpp>
#include <objtools/format/gather_items.hpp>
#include <objtools/format/genbank_gather.hpp>
#include <objtools/format/context.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CGenbankGatherer::CGenbankGatherer(void)
{
}


bool s_ShowBaseCount(const CFlatFileConfig& cfg)
{ 
    return (cfg.IsModeDump()  ||  cfg.IsModeGBench());
}


bool s_ShowContig(CBioseqContext& ctx)
{
    if ( (ctx.IsSegmented()  &&  ctx.HasParts())  ||
         (ctx.IsDelta()  &&  !ctx.IsDeltaLitOnly()) ) {
        return true;
    }
    return false;
}


void CGenbankGatherer::x_DoSingleSection(CBioseqContext& ctx) const
{
    const CFlatFileConfig& cfg = ctx.Config();

    ItemOS() << new CStartSectionItem(ctx);

    ItemOS() << new CLocusItem(ctx);
    ItemOS() << new CDeflineItem(ctx);
    ItemOS() << new CAccessionItem(ctx);
    ItemOS() << new CVersionItem(ctx);
    if ( ctx.IsGbGenomeProject() ) {
        ItemOS() << new CGenomeProjectItem(ctx);
    }
    if ( ctx.IsProt() ) {
        ItemOS() << new CDBSourceItem(ctx);
    }
    ItemOS() << new CKeywordsItem(ctx);
    if ( ctx.IsPart() ) {
        ItemOS() << new CSegmentItem(ctx);
    }
    ItemOS() << new CSourceItem(ctx);
    x_GatherReferences();
    x_GatherComments();
    ItemOS() << new CPrimaryItem(ctx);
    ItemOS() << new CFeatHeaderItem(ctx);
    if ( !cfg.HideSourceFeatures() ) {
        x_GatherSourceFeatures();
    }
    if ( ctx.IsWGSMaster()  &&  ctx.GetTech() == CMolInfo::eTech_wgs ) {
        x_GatherWGS(ctx);
    } else if ( ctx.DoContigStyle() ) {
        if ( cfg.ShowContigFeatures() ) {
            x_GatherFeatures();
        }
        ItemOS() << new CContigItem(ctx);
        if ( cfg.ShowContigAndSeq() ) {
            if ( ctx.IsNuc()  &&  s_ShowBaseCount(cfg) ) {
                ItemOS() << new CBaseCountItem(ctx);
            }
            ItemOS() << new COriginItem(ctx);
            x_GatherSequence();
        }
    } else {
        x_GatherFeatures();
        if ( cfg.ShowContigAndSeq()  &&  s_ShowContig(ctx) ) {
            ItemOS() << new CContigItem(ctx);
        }
        if ( ctx.IsNuc()  &&  s_ShowBaseCount(cfg) ) {
            ItemOS() << new CBaseCountItem(ctx);
        }
        ItemOS() << new COriginItem(ctx);
        x_GatherSequence();
    }
        
    ItemOS() << new CEndSectionItem(ctx);
}


void CGenbankGatherer::x_GatherWGS(CBioseqContext& ctx) const
{
    const string* first = 0;
    const string* last  = 0;

    for (CSeqdesc_CI desc(ctx.GetHandle(), CSeqdesc::e_User);  desc;  ++desc) {
        const CUser_object& uo = desc->GetUser();
        CWGSItem::EWGSType wgs_type = CWGSItem::eWGS_not_set;

        if ( !uo.GetType().IsStr() ) {
            continue;
        }
        const string& type = uo.GetType().GetStr();
        if ( NStr::CompareNocase(type, "WGSProjects") == 0 ) {
            wgs_type = CWGSItem::eWGS_Projects;
        } else if ( NStr::CompareNocase(type, "WGS-Scaffold-List") == 0 ) {
            wgs_type = CWGSItem::eWGS_ScaffoldList;
        } else if ( NStr::CompareNocase(type, "WGS-Contig-List") == 0 ) {
            wgs_type = CWGSItem::eWGS_ContigList;
        }

        if ( wgs_type == CWGSItem::eWGS_not_set ) {
            continue;
        }

        ITERATE (CUser_object::TData, it, uo.GetData()) {
            if ( !(*it)->GetLabel().IsStr() ) {
                continue;
            }
            const string& label = (*it)->GetLabel().GetStr();
            if ( NStr::CompareNocase(label, "WGS_accession_first") == 0  ||
                 NStr::CompareNocase(label, "Accession_first") == 0 ) {
                first = &((*it)->GetData().GetStr());
            } else if ( NStr::CompareNocase(label, "WGS_accession_last") == 0 ||
                        NStr::CompareNocase(label, "Accession_last") == 0 ) {
                last = &((*it)->GetData().GetStr());
            }
        }

        if ( (first != 0)  &&  (last != 0) ) {
            ItemOS() << new CWGSItem(wgs_type, *first, *last, uo, ctx);
        }
    }    
}


END_SCOPE(objects)
END_NCBI_SCOPE
