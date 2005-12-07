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
* Author:  Yuri Kapustin
*
* File Description:  Helper functions
*                   
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include "splign_util.hpp"
#include "splign_hitparser.hpp"
#include "messages.hpp"

#include <algo/align/nw/align_exception.hpp>
#include <algo/align/util/blast_tabular.hpp>

#include <algorithm>
#include <math.h>

BEGIN_NCBI_SCOPE


void CleaveOffByTail(CSplign::THitRefs* phitrefs, size_t polya_start)
{
    const size_t hit_dim = phitrefs->size();

    for(size_t i = 0; i < hit_dim; ++i) {

        CSplign::THitRef& hit = (*phitrefs)[i];
        if(hit->GetQueryStart() >= polya_start) {
            (*phitrefs)[i].Reset(NULL);
        }
        else if(hit->GetQueryStop() >= polya_start) {
            hit->Modify(1, polya_start - 1);
        }
    }

    typedef CSplign::THitRefs::iterator TI;
    TI ii0 = phitrefs->begin();
    for(TI ii = ii0, ie = phitrefs->end(); ii != ie; ++ii) {
        if(ii->NotEmpty()) {
            if(ii0 < ii) {
                *ii0 = *ii;
            }
            ++ii0;
        }
    }

    phitrefs->erase(ii0, phitrefs->end());
}


void BTRefsToHits(const CSplign::THitRefs& src, vector<CHit>* dst)
{
    const size_t dim = src.size();
    dst->resize(dim);
    for(size_t i = 0; i < dim; ++i) {

        CHit& h = (*dst)[i];
        const CSplign::THitRef& bt = src[i];

        h.m_Query = bt->GetQueryId()->AsFastaString();
        h.m_Subj = bt->GetSubjId()->AsFastaString();
        h.m_Idnty = bt->GetIdentity();
        h.m_MM = bt->GetMismatches();
        h.m_Gaps = bt->GetGaps();
        h.m_Value = bt->GetEValue();
        h.m_Score = bt->GetScore();

        h.m_ai[0] = bt->GetQueryMin();
        h.m_ai[1] = bt->GetQueryMax();
        h.m_ai[2] = bt->GetSubjMin();
        h.m_ai[3] = bt->GetSubjMax();

        if(bt->GetQueryStrand()) {
            h.m_an[0] = h.m_ai[0];
            h.m_an[1] = h.m_ai[1];
        }
        else {
            h.m_an[1] = h.m_ai[0];
            h.m_an[0] = h.m_ai[1];
        }

        if(bt->GetSubjStrand()) {
            h.m_an[2] = h.m_ai[2];
            h.m_an[3] = h.m_ai[3];
        }
        else {
            h.m_an[3] = h.m_ai[2];
            h.m_an[2] = h.m_ai[3];
        }
        
        h.m_anLength[0] = 1 + h.m_ai[1] - h.m_ai[0];
        h.m_anLength[1] = 1 + h.m_ai[3] - h.m_ai[2];
        h.m_Length = h.m_anLength[0] < h.m_anLength[1]? 
            h.m_anLength[1]: h.m_anLength[0];
    }
}


CConstRef<objects::CSeq_id> CreateSeqId(const string& strid)
{
    USING_SCOPE(objects);
    CConstRef<objects::CSeq_id> rv;

    try {
        rv.Reset(new CSeq_id(strid));
    }
    catch(CSeqIdException& e) {
        if(e.GetErrCode() == CSeqIdException::eFormat) {
            rv.Reset(new CSeq_id(CSeq_id::e_Local, strid));
        }
        else {
            throw;
        }
    }
    return rv;
}


void HitsToBTRefs(const vector<CHit>& src, CSplign::THitRefs* dst)
{
    const size_t dim = src.size();
    dst->resize(dim);

    for(size_t i = 0; i < dim; ++i) {

        const CHit& h = src[i];
        CSplign::THitRef& bt = (*dst)[i];
        bt.Reset(new CSplign::THit);

        bt->SetQueryId(CreateSeqId(h.m_Query));
        bt->SetSubjId(CreateSeqId(h.m_Subj));

        bt->SetQueryStart(h.m_an[0]);
        bt->SetQueryStop(h.m_an[1]);
        bt->SetSubjStart(h.m_an[2]);
        bt->SetSubjStop(h.m_an[3]);

        bt->SetLength(h.m_Length);
        bt->SetMismatches(h.m_MM);
        bt->SetGaps(h.m_Gaps);
        bt->SetEValue(h.m_Value);
        bt->SetIdentity(h.m_Idnty);
        bt->SetScore(h.m_Score);
    }
}


void XFilter(CSplign::THitRefs* phitrefs) 
{
    if(phitrefs->size() == 0) return;

    vector<CHit> hits;
    BTRefsToHits(*phitrefs, &hits);
  
    int group_id = 0;
    CHitParser hp (hits, group_id);
    hp.m_Strand = CHitParser::eStrand_Both;
    hp.m_SameOrder = false;
    hp.m_Method = CHitParser::eMethod_MaxScore;
    hp.m_CombinationProximity_pre  = -1;
    hp.m_CombinationProximity_post = -1;
    hp.m_MinQueryCoverage = 0;
    hp.m_MinSubjCoverage = 0;
    hp.m_MaxHitDistQ = kMax_Int;
    hp.m_MaxHitDistS = kMax_Int;
    hp.m_Prot2Nucl = false;
    hp.m_SplitQuery = CHitParser::eSplitMode_Clear;
    hp.m_SplitSubject = CHitParser::eSplitMode_Clear;
    hp.m_group_identification_method = CHitParser::eNone;
    hp.m_Query = hits[0].m_Query;
    hp.m_Subj =  hits[0].m_Subj;
    
    hp.Run( CHitParser::eMode_Normal );

    HitsToBTRefs(hp.m_Out, phitrefs);
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.18  2005/12/07 15:49:30  kapustin
 * Use FASTA-style IDs when converting to strings
 *
 * Revision 1.17  2005/11/21 16:06:38  kapustin
 * Move gpipe-sensitive items to the app level
 *
 * Revision 1.16  2005/10/19 17:56:35  kapustin
 * Switch to using ObjMgr+LDS to load sequence data
 *
 * Revision 1.15  2005/09/27 17:58:34  kapustin
 * Fix CleaveOffByTail()
 *
 * Revision 1.14  2005/09/12 16:24:00  kapustin
 * Move compartmentization to xalgoalignutil.
 *
 * Revision 1.13  2005/05/24 19:36:08  kapustin
 * -RLE()
 *
 * Revision 1.12  2005/01/26 21:33:12  kapustin
 * ::IsConsensusSplce ==> CSplign::SSegment::s_IsConsensusSplice
 *
 * Revision 1.11  2004/12/16 23:12:26  kapustin
 * algo/align rearrangement
 *
 * Revision 1.10  2004/11/29 15:55:55  kapustin
 * -ScoreByTranscript
 *
 * Revision 1.9  2004/11/29 14:37:16  kapustin
 * CNWAligner::GetTranscript now returns TTranscript and direction can be 
 * specified. x_ScoreByTanscript renamed to ScoreFromTranscript with two 
 * additional parameters to specify starting coordinates.
 *
 * Revision 1.8  2004/05/24 16:13:57  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.7  2004/05/17 14:50:57  kapustin
 * Add/remove/rearrange some includes and object declarations
 *
 * Revision 1.6  2004/04/23 20:33:32  ucko
 * Fix remaining use of sprintf.
 *
 * Revision 1.5  2004/04/23 18:43:58  ucko
 * <cmath> -> <math.h>, since some older compilers (MIPSpro) lack the wrappers.
 *
 * Revision 1.4  2004/04/23 16:50:48  kapustin
 * sprintf->CNcbiOstrstream
 *
 * Revision 1.3  2004/04/23 14:37:44  kapustin
 * *** empty log message ***
 *
 *
 * ===========================================================================
 */
