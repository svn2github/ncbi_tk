/* $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's offical duties as a United States Government employee and
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
 * Author: Ilya Dondoshansky
 *
 */

/** @file blast_hits.c
 * BLAST functions for saving hits after the (preliminary) gapped alignment
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_extend.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_util.h>

/********************************************************************************
          Functions manipulating BlastHSP's
********************************************************************************/

BlastHSP* Blast_HSPFree(BlastHSP* hsp)
{
   if (!hsp)
      return NULL;
   hsp->gap_info = GapEditBlockDelete(hsp->gap_info);
   sfree(hsp);
   return NULL;
}

BlastHSP* Blast_HSPNew(void)
{
     BlastHSP* new_hsp = (BlastHSP*) calloc(1, sizeof(BlastHSP));
     return new_hsp;
}

/*
   Comments in blast_hits.h
*/
Int2
Blast_HSPInit(Int4 query_start, Int4 query_end, Int4 subject_start, Int4 subject_end,
          Int4 query_gapped_start, Int4 subject_gapped_start, 
          Int4 query_context, Int2 subject_frame, Int4 score, 
          GapEditBlock* *gap_edit, BlastHSP* *ret_hsp)
{
   BlastHSP* new_hsp = NULL;

   if (!ret_hsp)
      return -1;

   new_hsp = Blast_HSPNew();

   *ret_hsp = NULL;

   if (new_hsp == NULL)
	return -1;


   new_hsp->query.offset = query_start;
   new_hsp->subject.offset = subject_start;
   new_hsp->query.length = query_end - query_start;
   new_hsp->subject.length = subject_end - subject_start;
   new_hsp->query.end = query_end;
   new_hsp->subject.end = subject_end;
   new_hsp->query.gapped_start = query_gapped_start;
   new_hsp->subject.gapped_start = subject_gapped_start;
   new_hsp->context = query_context;
   new_hsp->subject.frame = subject_frame;
   new_hsp->score = score;
   if (gap_edit && *gap_edit)
   { /* If this is non-NULL transfer ownership. */
        new_hsp->gap_info = *gap_edit;
        *gap_edit = NULL;
   }

   *ret_hsp = new_hsp;

   return 0;
}

/*

 Comments in blast_hits.h

*/
Int2
Blast_HSPReset(Int4 query_start, Int4 query_end, Int4 subject_start, Int4 subject_end,
          Int4 score, GapEditBlock* *gap_edit, BlastHSP* hsp)
{

   if (hsp == NULL)
     return -1;

   hsp->score = score;
   hsp->query.offset = query_start;
   hsp->subject.offset = subject_start;
   hsp->query.length = query_end - query_start;
   hsp->subject.length = subject_end - subject_start;
   hsp->query.end = query_end;
   hsp->subject.end = subject_end;
   if (gap_edit && *gap_edit)
   { /* If this is non-NULL transfer ownership. */
        hsp->gap_info = *gap_edit;
        *gap_edit = NULL;
   }
   return 0;
}

void Blast_HSPPHIGetEvalue(BlastHSP* hsp, BlastScoreBlk* sbp)
{
   double paramC;
   double Lambda;
   Int8 pattern_space;
  
   if (!hsp || !sbp)
      return;
   paramC = sbp->kbp[0]->paramC;
   Lambda = sbp->kbp[0]->Lambda;
   pattern_space = sbp->effective_search_sp;
   hsp->evalue = pattern_space*paramC*(1+Lambda*hsp->score)*
                    exp(-Lambda*hsp->score);
}

Boolean Blast_HSPReevaluateWithAmbiguities(BlastHSP* hsp, 
           Uint1* query_start, Uint1* subject_start, 
           const BlastHitSavingParameters* hit_params, 
           const BlastScoringParameters* score_params, 
           BlastQueryInfo* query_info, BlastScoreBlk* sbp)
{
   BlastScoringOptions *score_options = score_params->options;
   Int4 sum, score, gap_open, gap_extend;
   Int4** matrix;
   Uint1* query,* subject;
   Uint1* new_q_start,* new_s_start,* new_q_end,* new_s_end;
   Int4 index;
   Int2 factor = 1;
   Uint1 mask = 0x0f;
   GapEditScript* esp,* last_esp = NULL,* prev_esp,* first_esp = NULL;
   Boolean delete_hsp;
   Int8 searchsp_eff;
   Int4 last_esp_num = 0;
   Int4 align_length;
   Blast_KarlinBlk* kbp;
   Boolean gapped_calculation = score_options->gapped_calculation;

   /* NB: this function is called only for BLASTn, so we know where the 
      Karlin block is */
   kbp = sbp->kbp_std[hsp->context];
   searchsp_eff = query_info->eff_searchsp_array[hsp->context];

   if (score_params->gap_open == 0 && score_params->gap_extend == 0) {
      if (score_params->reward % 2 == 1) 
         factor = 2;
      gap_open = 0;
      gap_extend = 
         (score_params->reward - 2*score_params->penalty) * factor / 2;
   } else {
      gap_open = score_params->gap_open;
      gap_extend = score_params->gap_extend;
   }

   matrix = sbp->matrix;

   query = query_start + hsp->query.offset; 
   subject = subject_start + hsp->subject.offset;
   score = 0;
   sum = 0;
   new_q_start = new_q_end = query;
   new_s_start = new_s_end = subject;
   index = 0;
   
   if (!gapped_calculation) {
      for (index = 0; index < hsp->subject.length; ++index) {
         sum += factor*matrix[*query & mask][*subject];
         query++;
         subject++;
         if (sum < 0) {
            if (score < hit_params->cutoff_score) {
               /* Start from new offset */
               new_q_start = query;
               new_s_start = subject;
               score = sum = 0;
            } else {
               /* Stop here */
               break;
            }
         } else if (sum > score) {
            /* Remember this point as the best scoring end point */
            score = sum;
            new_q_end = query;
            new_s_end = subject;
         }
      }
   } else {
      esp = hsp->gap_info->esp;
      prev_esp = NULL;
      last_esp = first_esp = esp;
      last_esp_num = 0;
      
      while (esp) {
         if (esp->op_type == eGapAlignSub) {
            sum += factor*matrix[*query & mask][*subject];
            query++;
            subject++;
            index++;
         } else if (esp->op_type == eGapAlignDel) {
            sum -= gap_open + gap_extend * esp->num;
            subject += esp->num;
            index += esp->num;
         } else if (esp->op_type == eGapAlignIns) {
            sum -= gap_open + gap_extend * esp->num;
            query += esp->num;
            index += esp->num;
         }
         
         if (sum < 0) {
            if (score < hit_params->cutoff_score) {
               /* Start from new offset */
               new_q_start = query;
               new_s_start = subject;
               score = sum = 0;
               if (index < esp->num) {
                  esp->num -= index;
                  first_esp = esp;
                  index = 0;
               } else {
                  first_esp = esp->next;
               }
               /* Unlink the bad part of the esp chain 
                  so it can be freed later */
               if (prev_esp)
                  prev_esp->next = NULL;
               last_esp = NULL;
            } else {
               /* Stop here */
               break;
            }
         } else if (sum > score) {
            /* Remember this point as the best scoring end point */
            score = sum;
            last_esp = esp;
            last_esp_num = index;
            new_q_end = query;
            new_s_end = subject;
         }
         if (index >= esp->num) {
            index = 0;
            prev_esp = esp;
            esp = esp->next;
         }
      } /* loop on edit scripts */
   } /* if (gapped_calculation) */

   score /= factor;
   
   delete_hsp = FALSE;
   hsp->score = score;
   hsp->evalue = 
      BLAST_KarlinStoE_simple(score, kbp, searchsp_eff);
   if (hsp->score < hit_params->cutoff_score) {
      delete_hsp = TRUE;
   } else {
      hsp->query.length = new_q_end - new_q_start;
      hsp->subject.length = new_s_end - new_s_start;
      hsp->query.offset = new_q_start - query_start;
      hsp->query.end = hsp->query.offset + hsp->query.length;
      hsp->subject.offset = new_s_start - subject_start;
      hsp->subject.end = hsp->subject.offset + hsp->subject.length;
      if (gapped_calculation) {
         /* Make corrections in edit block and free any parts that
            are no longer needed */
         if (first_esp != hsp->gap_info->esp) {
            GapEditScriptDelete(hsp->gap_info->esp);
            hsp->gap_info->esp = first_esp;
         }
         if (last_esp->next != NULL) {
            GapEditScriptDelete(last_esp->next);
            last_esp->next = NULL;
         }
         last_esp->num = last_esp_num;
      }
      Blast_HSPGetNumIdentities(query_start, subject_start, hsp, 
         gapped_calculation, &hsp->num_ident, &align_length);
      /* Check if this HSP passes the percent identity test */
      if (((double)hsp->num_ident) / align_length * 100 < 
          hit_params->options->percent_identity)
         delete_hsp = TRUE;
   }
   
   if (delete_hsp) { /* This HSP is now below the cutoff */
      if (gapped_calculation && first_esp != NULL && 
          first_esp != hsp->gap_info->esp)
         GapEditScriptDelete(first_esp);
   }
  
   return delete_hsp;
} 

Int2
Blast_HSPGetNumIdentities(Uint1* query, Uint1* subject, 
   BlastHSP* hsp, Boolean is_gapped, Int4* num_ident_ptr, 
   Int4* align_length_ptr)
{
   Int4 i, num_ident, align_length, q_off, s_off;
   Uint1* q,* s;
   GapEditBlock* gap_info;
   GapEditScript* esp;

   gap_info = hsp->gap_info;

   if (!gap_info && is_gapped)
      return -1;

   q_off = hsp->query.offset;
   s_off = hsp->subject.offset;

   if (!subject || !query)
      return -1;

   q = &query[q_off];
   s = &subject[s_off];

   num_ident = 0;
   align_length = 0;


   if (!gap_info) {
      /* Ungapped case */
      align_length = hsp->query.length; 
      for (i=0; i<align_length; i++) {
         if (*q++ == *s++)
            num_ident++;
      }
   } else {
      for (esp = gap_info->esp; esp; esp = esp->next) {
         align_length += esp->num;
         switch (esp->op_type) {
         case eGapAlignSub:
            for (i=0; i<esp->num; i++) {
               if (*q++ == *s++)
                  num_ident++;
            }
            break;
         case eGapAlignDel:
            s += esp->num;
            break;
         case eGapAlignIns:
            q += esp->num;
            break;
         default: 
            s += esp->num;
            q += esp->num;
            break;
         }
      }
   }

   *align_length_ptr = align_length;
   *num_ident_ptr = num_ident;
   return 0;
}

Int2
Blast_HSPGetOOFNumIdentities(Uint1* query, Uint1* subject, 
   BlastHSP* hsp, EBlastProgramType program, 
   Int4* num_ident_ptr, Int4* align_length_ptr)
{
   Int4 i, num_ident, align_length;
   Uint1* q,* s;
   GapEditScript* esp;

   if (!hsp->gap_info || !subject || !query)
      return -1;


   if (program == eBlastTypeTblastn ||
       program == eBlastTypeRpsTblastn) {
       q = &query[hsp->query.offset];
       s = &subject[hsp->subject.offset];
   } else {
       s = &query[hsp->query.offset];
       q = &subject[hsp->subject.offset];
   }
   num_ident = 0;
   align_length = 0;

   for (esp = hsp->gap_info->esp; esp; esp = esp->next) {
      switch (esp->op_type) {
      case eGapAlignSub: /* Substitution */
         align_length += esp->num;
         for (i=0; i<esp->num; i++) {
            if (*q == *s)
               num_ident++;
            ++q;
            s += CODON_LENGTH;
         }
         break;
      case eGapAlignIns: /* Insertion */
         align_length += esp->num;
         s += esp->num * CODON_LENGTH;
         break;
      case eGapAlignDel: /* Deletion */
         align_length += esp->num;
         q += esp->num;
         break;
      case eGapAlignDel2: /* Gap of two nucleotides. */
         s -= 2;
         break;
      case eGapAlignDel1: /* Gap of one nucleotide. */
         s -= 1;
         break;
      case eGapAlignIns1: /* Insertion of one nucleotide. */
         s += 1;
         break;
      case eGapAlignIns2: /* Insertion of two nucleotides. */
         s += 2;
         break;
      default: 
         s += esp->num * CODON_LENGTH;
         q += esp->num;
         break;
      }
   }

   *align_length_ptr = align_length;
   *num_ident_ptr = num_ident;

   return 0;
}

void 
Blast_HSPCalcLengthAndGaps(BlastHSP* hsp, Int4* length_out,
                           Int4* gaps_out, Int4* gap_opens_out)
{
   Int4 length = hsp->query.length;
   Int4 gap_opens = 0, gaps = 0;

   if (hsp->gap_info) {
      GapEditScript* esp = hsp->gap_info->esp;
      for ( ; esp; esp = esp->next) {
         if (esp->op_type == eGapAlignDel) {
            length += esp->num;
            gaps += esp->num;
            ++gap_opens;
         } else if (esp->op_type == eGapAlignIns) {
            ++gap_opens;
            gaps += esp->num;
         }
      }
   } else if (hsp->subject.length > length) {
      length = hsp->subject.length;
   }

   *length_out = length;
   *gap_opens_out = gap_opens;
   *gaps_out = gaps;
}

/** Adjust start and end of an HSP in a translated sequence segment.
 * @param segment BlastSeg structure (part of BlastHSP) [in]
 * @param seq_length Length of the full sequence [in]
 * @param start Start of the alignment in this segment in nucleotide 
 *              coordinates, 1-offset [out]
 * @param end End of the alignment in this segment in nucleotide 
 *            coordinates, 1-offset [out]
 */
static void 
Blast_SegGetTranslatedOffsets(BlastSeg* segment, Int4 seq_length, 
                              Int4* start, Int4* end)
{
   if (segment->frame < 0)	{
      *start = seq_length - CODON_LENGTH*segment->offset
         + segment->frame;
      *end = seq_length
         - CODON_LENGTH*(segment->offset+segment->length)
         + segment->frame + 1;
   } else if (segment->frame > 0)	{
      *start = CODON_LENGTH*(segment->offset) + segment->frame - 1;
      *end = CODON_LENGTH*(segment->offset+segment->length)
         + segment->frame - 2;
   } else {
      *start = segment->offset + 1;
      *end = segment->offset + segment->length;
   }
}

void 
Blast_HSPGetAdjustedOffsets(BlastHSP* hsp, Int4* q_start, Int4* q_end,
                            Int4* s_start, Int4* s_end)
{
   Int4 query_length, subject_length;

   if (!hsp->gap_info) {
      *q_start = hsp->query.offset + 1;
      *q_end = hsp->query.offset + hsp->query.length;
      *s_start = hsp->subject.offset + 1;
      *s_end = hsp->subject.offset + hsp->subject.length;
      return;
   }

   /* Non-translated lengths are stored in the GapEditBlock */
   query_length = hsp->gap_info->original_length1;
   subject_length = hsp->gap_info->original_length2;
   
   if (!hsp->gap_info->translate1 && !hsp->gap_info->translate2) {
      if (hsp->query.frame != hsp->subject.frame) {
         /* Blastn: if different strands, flip offsets in query; leave 
            offsets in subject as they are, but change order for correct
            correspondence. */
         *q_end = query_length - hsp->query.offset;
         *q_start = *q_end - hsp->query.length + 1;
         *s_end = hsp->subject.offset + 1;
         *s_start = hsp->subject.offset + hsp->subject.length;
      } else {
         *q_start = hsp->query.offset + 1;
         *q_end = hsp->query.offset + hsp->query.length;
         *s_start = hsp->subject.offset + 1;
         *s_end = hsp->subject.offset + hsp->subject.length;
      }
   } else {
      Blast_SegGetTranslatedOffsets(&hsp->query, query_length, q_start, q_end);
      Blast_SegGetTranslatedOffsets(&hsp->subject, subject_length, 
                                    q_start, s_end);
   }
}

/** TRUE if c is between a and b; f between d and f. Determines if the
 * coordinates are already in an HSP that has been evaluated. 
 */
#define CONTAINED_IN_HSP(a,b,c,d,e,f) (((a <= c && b >= c) && (d <= f && e >= f)) ? TRUE : FALSE)

/** Checks if the first HSP is contained in the second. */
static Boolean Blast_HSPContained(BlastHSP* hsp1, BlastHSP* hsp2)
{
   Boolean hsp_start_is_contained=FALSE, hsp_end_is_contained=FALSE;

   if (hsp1->score > hsp2->score)
      return FALSE;

   if (CONTAINED_IN_HSP(hsp2->query.offset, hsp2->query.end, hsp1->query.offset, hsp2->subject.offset, hsp2->subject.end, hsp1->subject.offset) == TRUE) {
      hsp_start_is_contained = TRUE;
   }
   if (CONTAINED_IN_HSP(hsp2->query.offset, hsp2->query.end, hsp1->query.end, hsp2->subject.offset, hsp2->subject.end, hsp1->subject.end) == TRUE) {
      hsp_end_is_contained = TRUE;
   }
   
   return (hsp_start_is_contained && hsp_end_is_contained);
}

/** Comparison callback function for sorting HSPs by score. Assumes that both 
 * HSPs being compared are not NULL.
 */
static int
score_compare_hsps(const void* v1, const void* v2)
{
   BlastHSP* h1,* h2;
   
   h1 = *((BlastHSP**) v1);
   h2 = *((BlastHSP**) v2);

   /* Null HSPs are "greater" than any non-null ones, so they go to the end
      of a sorted list. */
   if (!h1 && !h2)
       return 0;
   else if (!h1)
       return 1;
   else if (!h2)
       return -1;

   if (h1->score > h2->score)
      return -1;
   else if (h1->score < h2->score)
      return 1;
   /* Tie-breakers: decreasing subject offsets; decreasing subject ends, 
      decreasing query offsets, decreasing query ends */
   else if (h1->subject.offset > h2->subject.offset)
      return -1;
   else if (h1->subject.offset < h2->subject.offset)
      return 1;
   else if (h1->subject.end > h2->subject.end)
      return -1;
   else if (h1->subject.end < h2->subject.end)
      return 1;
   else if (h1->query.offset > h2->query.offset)
      return -1;
   else if (h1->query.offset < h2->query.offset)
      return 1;
   else if (h1->query.end > h2->query.end)
      return -1;
   else if (h1->query.end < h2->query.end)
      return 1;

   return 0;
}

Boolean Blast_HSPListIsSortedByScore(const BlastHSPList* hsp_list) 
{
    Int4 index;
    
    if (!hsp_list || hsp_list->hspcnt <= 1)
        return TRUE;

    for (index = 0; index < hsp_list->hspcnt - 1; ++index) {
        if (score_compare_hsps(&hsp_list->hsp_array[index], 
                               &hsp_list->hsp_array[index+1]) > 0) {
            return FALSE;
        }
    }
    return TRUE;
}

void Blast_HSPListSortByScore(BlastHSPList* hsp_list)
{
    if (!hsp_list || hsp_list->hspcnt <= 1)
        return;

    if (!Blast_HSPListIsSortedByScore(hsp_list)) {
        qsort(hsp_list->hsp_array, hsp_list->hspcnt, sizeof(BlastHSP*),
              score_compare_hsps);
    }
}

#define FUZZY_EVALUE_COMPARE_FACTOR 1e-6
/** Compares 2 real numbers up to a fixed precision */
static int fuzzy_evalue_comp(double evalue1, double evalue2)
{
   if (evalue1 < (1-FUZZY_EVALUE_COMPARE_FACTOR)*evalue2)
      return -1;
   else if (evalue1 > (1+FUZZY_EVALUE_COMPARE_FACTOR)*evalue2)
      return 1;
   else 
      return 0;
}

/** Comparison callback function for sorting HSPs by e-value and score, before
    saving BlastHSPList in a BlastHitList. E-value has priority over score, 
    because lower scoring HSPs might have lower e-values, if they are linked
    with sum statistics.
    E-values are compared only up to a certain precision. */
static int
evalue_compare_hsps
(const void* v1, const void* v2)
{
   BlastHSP* h1,* h2;
   int retval = 0;

   h1 = *((BlastHSP**) v1);
   h2 = *((BlastHSP**) v2);
   
   /* Check if one or both of these are null. Those HSPs should go to the end */
   if (!h1 && !h2)
      return 0;
   else if (!h1)
      return 1;
   else if (!h2)
      return -1;

   if ((retval = fuzzy_evalue_comp(h1->evalue, h2->evalue)) != 0)
      return retval;

   return score_compare_hsps(v1, v2);
}

void Blast_HSPListSortByEvalue(BlastHSPList* hsp_list)
{
    if (!hsp_list)
        return;

    if (hsp_list->hspcnt > 1) {
        Int4 index;
        BlastHSP** hsp_array = hsp_list->hsp_array;
        /* First check if HSP array is already sorted. */
        for (index = 0; index < hsp_list->hspcnt - 1; ++index) {
            if (evalue_compare_hsps(&hsp_array[index], &hsp_array[index+1]) > 0) {
                break;
            }
        }
        /* Sort the HSP array if it is not sorted yet. */
        if (index < hsp_list->hspcnt - 1) {
            qsort(hsp_list->hsp_array, hsp_list->hspcnt, sizeof(BlastHSP*),
                  evalue_compare_hsps);
        }
    }
}


/** Comparison callback for sorting HSPs by diagonal. Do not compare
 * diagonals for HSPs from different contexts. The absolute value of the
 * return is the diagonal difference between the HSPs.
 */
static int
diag_compare_hsps(const void* v1, const void* v2)
{
   BlastHSP* h1,* h2;

   h1 = *((BlastHSP**) v1);
   h2 = *((BlastHSP**) v2);
   
   if (h1->context < h2->context)
      return INT4_MIN;
   else if (h1->context > h2->context)
      return INT4_MAX;
   return (h1->query.offset - h1->subject.offset) - 
      (h2->query.offset - h2->subject.offset);
}

/** An auxiliary structure used for merging HSPs */
typedef struct BlastHSPSegment {
   Int4 q_start, q_end;
   Int4 s_start, s_end;
   struct BlastHSPSegment* next;
} BlastHSPSegment;

#define OVERLAP_DIAG_CLOSE 10
/** Merge the two HSPs if they intersect.
 * @param hsp1 The first HSP; also contains the result of merge. [in] [out]
 * @param hsp2 The second HSP [in]
 * @param start The starting offset of the subject coordinates where the 
 *               intersection is possible [in]
*/
static Boolean
Blast_HSPsMerge(BlastHSP* hsp1, BlastHSP* hsp2, Int4 start)
{
   BlastHSPSegment* segments1,* segments2,* new_segment1,* new_segment2;
   GapEditScript* esp1,* esp2,* esp;
   Int4 end = start + DBSEQ_CHUNK_OVERLAP - 1;
   Int4 min_diag, max_diag, num1, num2, dist = 0, next_dist = 0;
   Int4 diag1_start, diag1_end, diag2_start, diag2_end;
   Int4 index;
   Uint1 intersection_found;
   EGapAlignOpType op_type = eGapAlignSub;

   if (!hsp1->gap_info || !hsp2->gap_info) {
      /* Assume that this is an ungapped alignment, hence simply compare 
         diagonals. Do not merge if they are on different diagonals */
      if (diag_compare_hsps(&hsp1, &hsp2) == 0 &&
          hsp1->query.end >= hsp2->query.offset) {
         hsp1->query.end = hsp2->query.end;
         hsp1->subject.end = hsp2->subject.end;
         hsp1->query.length = hsp1->query.end - hsp1->query.offset;
         hsp1->subject.length = hsp1->subject.end - hsp1->subject.offset;
         return TRUE;
      } else
         return FALSE;
   }
   /* Find whether these HSPs have an intersection point */
   segments1 = (BlastHSPSegment*) calloc(1, sizeof(BlastHSPSegment));
   
   esp1 = hsp1->gap_info->esp;
   esp2 = hsp2->gap_info->esp;
   
   segments1->q_start = hsp1->query.offset;
   segments1->s_start = hsp1->subject.offset;
   while (segments1->s_start < start) {
      if (esp1->op_type == eGapAlignIns)
         segments1->q_start += esp1->num;
      else if (segments1->s_start + esp1->num < start) {
         if (esp1->op_type == eGapAlignSub) { 
            segments1->s_start += esp1->num;
            segments1->q_start += esp1->num;
         } else if (esp1->op_type == eGapAlignDel)
            segments1->s_start += esp1->num;
      } else 
         break;
      esp1 = esp1->next;
   }
   /* Current esp is the first segment within the overlap region */
   segments1->s_end = segments1->s_start + esp1->num - 1;
   if (esp1->op_type == eGapAlignSub)
      segments1->q_end = segments1->q_start + esp1->num - 1;
   else
      segments1->q_end = segments1->q_start;
   
   new_segment1 = segments1;
   
   for (esp = esp1->next; esp; esp = esp->next) {
      new_segment1->next = (BlastHSPSegment*)
         calloc(1, sizeof(BlastHSPSegment));
      new_segment1->next->q_start = new_segment1->q_end + 1;
      new_segment1->next->s_start = new_segment1->s_end + 1;
      new_segment1 = new_segment1->next;
      if (esp->op_type == eGapAlignSub) {
         new_segment1->q_end += esp->num - 1;
         new_segment1->s_end += esp->num - 1;
      } else if (esp->op_type == eGapAlignIns) {
         new_segment1->q_end += esp->num - 1;
         new_segment1->s_end = new_segment1->s_start;
      } else {
         new_segment1->s_end += esp->num - 1;
         new_segment1->q_end = new_segment1->q_start;
      }
   }
   
   /* Now create the second segments list */
   
   segments2 = (BlastHSPSegment*) calloc(1, sizeof(BlastHSPSegment));
   segments2->q_start = hsp2->query.offset;
   segments2->s_start = hsp2->subject.offset;
   segments2->q_end = segments2->q_start + esp2->num - 1;
   segments2->s_end = segments2->s_start + esp2->num - 1;
   
   new_segment2 = segments2;
   
   for (esp = esp2->next; esp && new_segment2->s_end < end; 
        esp = esp->next) {
      new_segment2->next = (BlastHSPSegment*)
         calloc(1, sizeof(BlastHSPSegment));
      new_segment2->next->q_start = new_segment2->q_end + 1;
      new_segment2->next->s_start = new_segment2->s_end + 1;
      new_segment2 = new_segment2->next;
      if (esp->op_type == eGapAlignIns) {
         new_segment2->s_end = new_segment2->s_start;
         new_segment2->q_end = new_segment2->q_start + esp->num - 1;
      } else if (esp->op_type == eGapAlignDel) {
         new_segment2->s_end = new_segment2->s_start + esp->num - 1;
         new_segment2->q_end = new_segment2->q_start;
      } else if (esp->op_type == eGapAlignSub) {
         new_segment2->s_end = new_segment2->s_start + esp->num - 1;
         new_segment2->q_end = new_segment2->q_start + esp->num - 1;
      }
   }
   
   new_segment1 = segments1;
   new_segment2 = segments2;
   intersection_found = 0;
   num1 = num2 = 0;
   while (new_segment1 && new_segment2 && !intersection_found) {
      if (new_segment1->s_end < new_segment2->s_start || 
          new_segment1->q_end < new_segment2->q_start) {
         new_segment1 = new_segment1->next;
         num1++;
         continue;
      }
      if (new_segment2->s_end < new_segment1->s_start || 
          new_segment2->q_end < new_segment1->q_start) {
         new_segment2 = new_segment2->next;
         num2++;
         continue;
      }
      diag1_start = new_segment1->s_start - new_segment1->q_start;
      diag2_start = new_segment2->s_start - new_segment2->q_start;
      diag1_end = new_segment1->s_end - new_segment1->q_end;
      diag2_end = new_segment2->s_end - new_segment2->q_end;
      
      if (diag1_start == diag1_end && diag2_start == diag2_end &&  
          diag1_start == diag2_start) {
         /* Both segments substitutions, on same diagonal */
         intersection_found = 1;
         dist = new_segment2->s_end - new_segment1->s_start + 1;
         break;
      } else if (diag1_start != diag1_end && diag2_start != diag2_end) {
         /* Both segments gaps - must intersect */
         intersection_found = 3;

         op_type = eGapAlignIns;
         dist = new_segment2->s_end - new_segment1->s_start + 1;
         next_dist = new_segment2->q_end - new_segment1->q_start - dist + 1;
         if (new_segment2->q_end - new_segment1->q_start < dist) {
            dist = new_segment2->q_end - new_segment1->q_start + 1;
            op_type = eGapAlignDel;
            next_dist = new_segment2->s_end - new_segment1->s_start - dist + 1;
         }
         break;
      } else if (diag1_start != diag1_end) {
         max_diag = MAX(diag1_start, diag1_end);
         min_diag = MIN(diag1_start, diag1_end);
         if (diag2_start >= min_diag && diag2_start <= max_diag) {
            intersection_found = 2;
            dist = diag2_start - min_diag + 1;
            if (new_segment1->s_end == new_segment1->s_start)
               next_dist = new_segment2->s_end - new_segment1->s_end + 1;
            else
               next_dist = new_segment2->q_end - new_segment1->q_end + 1;
            break;
         }
      } else if (diag2_start != diag2_end) {
         max_diag = MAX(diag2_start, diag2_end);
         min_diag = MIN(diag2_start, diag2_end);
         if (diag1_start >= min_diag && diag1_start <= max_diag) {
            intersection_found = 2;
            next_dist = max_diag - diag1_start + 1;
            if (new_segment2->s_end == new_segment2->s_start)
               dist = new_segment2->s_start - new_segment1->s_start + 1;
            else
               dist = new_segment2->q_start - new_segment1->q_start + 1;
            break;
         }
      }
      if (new_segment1->s_end <= new_segment2->s_end) {
         new_segment1 = new_segment1->next;
         num1++;
      } else {
         new_segment2 = new_segment2->next;
         num2++;
      }
   }
   
   sfree(segments1);
   sfree(segments2);

   if (intersection_found) {
      esp = NULL;
      for (index = 0; index < num1-1; index++)
         esp1 = esp1->next;
      for (index = 0; index < num2-1; index++) {
         esp = esp2;
         esp2 = esp2->next;
      }
      if (intersection_found < 3) {
         if (num1 > 0)
            esp1 = esp1->next;
         if (num2 > 0) {
            esp = esp2;
            esp2 = esp2->next;
         }
      }
      switch (intersection_found) {
      case 1:
         esp1->num = dist;
         esp1->next = esp2->next;
         esp2->next = NULL;
         break;
      case 2:
         esp1->num = dist;
         esp2->num = next_dist;
         esp1->next = esp2;
         if (esp)
            esp->next = NULL;
         break;
      case 3:
         esp1->num += dist;
         esp2->op_type = op_type;
         esp2->num = next_dist;
         esp1->next = esp2;
         if (esp)
            esp->next = NULL;
         break;
      default: break;
      }
      hsp1->query.end = hsp2->query.end;
      hsp1->subject.end = hsp2->subject.end;
      hsp1->query.length = hsp1->query.end - hsp1->query.offset;
      hsp1->subject.length = hsp1->subject.end - hsp1->subject.offset;
   }

   return (Boolean) intersection_found;
}

/********************************************************************************
          Functions manipulating BlastHSPList's
********************************************************************************/

BlastHSPList* Blast_HSPListFree(BlastHSPList* hsp_list)
{
   Int4 index;

   if (!hsp_list)
      return hsp_list;

   for (index = 0; index < hsp_list->hspcnt; ++index) {
      Blast_HSPFree(hsp_list->hsp_array[index]);
   }
   sfree(hsp_list->hsp_array);

   sfree(hsp_list);
   return NULL;
}

BlastHSPList* Blast_HSPListNew(Int4 hsp_max)
{
   BlastHSPList* hsp_list = (BlastHSPList*) calloc(1, sizeof(BlastHSPList));
   const Int4 k_default_allocated=100;

   /* hsp_max is max number of HSP's allowed in an HSP list; 
      INT4_MAX taken as infinity. */
   hsp_list->hsp_max = INT4_MAX; 
   if (hsp_max > 0)
      hsp_list->hsp_max = hsp_max;

   hsp_list->allocated = MIN(k_default_allocated, hsp_list->hsp_max);

   hsp_list->hsp_array = (BlastHSP**) 
      calloc(hsp_list->allocated, sizeof(BlastHSP*));

   return hsp_list;
}

/** Duplicate HSPList's contents, copying only pointers to individual HSPs */
static BlastHSPList* Blast_HSPListDup(BlastHSPList* hsp_list)
{
   BlastHSPList* new_hsp_list = (BlastHSPList*) 
      BlastMemDup(hsp_list, sizeof(BlastHSPList));
   new_hsp_list->hsp_array = (BlastHSP**) 
      BlastMemDup(hsp_list->hsp_array, hsp_list->allocated*sizeof(BlastHSP*));
   if (!new_hsp_list->hsp_array) {
      sfree(new_hsp_list);
      return NULL;
   }
   new_hsp_list->allocated = hsp_list->allocated;

   return new_hsp_list;
}

/** This is a copy of a static function from ncbimisc.c.
 * Turns array into a heap with respect to a given comparison function.
 */
static void
heapify (char* base0, char* base, char* lim, char* last, size_t width, int (*compar )(const void*, const void* ))
{
   size_t i;
   char   ch;
   char* left_son,* large_son;
   
   left_son = base0 + 2*(base-base0) + width;
   while (base <= lim) {
      if (left_son == last)
         large_son = left_son;
      else
         large_son = (*compar)(left_son, left_son+width) >= 0 ?
            left_son : left_son+width;
      if ((*compar)(base, large_son) < 0) {
         for (i=0; i<width; ++i) {
            ch = base[i];
            base[i] = large_son[i];
            large_son[i] = ch;
         }
         base = large_son;
         left_son = base0 + 2*(base-base0) + width;
      } else
         break;
   }
}

/** Creates a heap of elements based on a comparison function.
 * @param b An array [in] [out]
 * @param nel Number of elements in b [in]
 * @param width The size of each element [in]
 * @param compar Callback to compare two heap elements [in]
 */
static void CreateHeap (void* b, size_t nel, size_t width, 
   int (*compar )(const void*, const void* ))   
{
   char*    base = (char*)b;
   size_t i;
   char*    base0 = (char*)base,* lim,* basef;
   
   if (nel < 2)
      return;
   
   lim = &base[((nel-2)/2)*width];
   basef = &base[(nel-1)*width];
   i = nel/2;
   for (base = &base0[(i - 1)*width]; i > 0; base = base - width) {
      heapify(base0, base, lim, basef, width, compar);
      i--;
   }
}

/** Given a BlastHSPList* with a heapified HSP array, check whether the 
 * new HSP is better than the worst scoring.  If it is, then remove the 
 * worst scoring and insert, otherwise free the new one.
 * HSP and insert the new HSP in the heap. 
 * @param hsp_list Contains all HSPs for a given subject. [in] [out]
 * @param hsp A pointer to new HSP to be inserted into the HSP list [in] [out]
 */
static void 
Blast_HSPListInsertHSPInHeap(BlastHSPList* hsp_list, 
                             BlastHSP** hsp)
{
    BlastHSP** hsp_array = hsp_list->hsp_array;
    if (score_compare_hsps(hsp, &hsp_array[0]) > 0)
    {
         Blast_HSPFree(*hsp);
         return;
    }
    else
         Blast_HSPFree(hsp_array[0]);

    hsp_array[0] = *hsp;
    if (hsp_list->hspcnt >= 2) {
        heapify((char*)hsp_array, (char*)hsp_array, 
                (char*)&hsp_array[hsp_list->hspcnt/2 - 1],
                 (char*)&hsp_array[hsp_list->hspcnt-1],
                 sizeof(BlastHSP*), score_compare_hsps);
    }
}

/* Comments in blast_hits.h
 */
Int2 
Blast_HSPListSaveHSP(BlastHSPList* hsp_list, BlastHSP* new_hsp)
{
   BlastHSP** hsp_array;
   Int4 hspcnt;
   Int4 hsp_allocated; /* how many hsps are in the array. */
   Int2 status = 0;

   hspcnt = hsp_list->hspcnt;
   hsp_allocated = hsp_list->allocated;
   hsp_array = hsp_list->hsp_array;
   

   /* Check if list is already full, then reallocate. */
   if (hspcnt >= hsp_allocated && hsp_list->do_not_reallocate == FALSE)
   {
      Int4 new_allocated = MIN(2*hsp_list->allocated, hsp_list->hsp_max);
      if (new_allocated > hsp_list->allocated) {
         hsp_array = (BlastHSP**)
            realloc(hsp_array, new_allocated*sizeof(BlastHSP*));
         if (hsp_array == NULL)
         {
            hsp_list->do_not_reallocate = TRUE; 
            hsp_array = hsp_list->hsp_array;
            /** Return a non-zero status, because restriction on number
                of HSPs here is a result of memory allocation failure. */
            status = -1;
         } else {
            hsp_list->hsp_array = hsp_array;
            hsp_list->allocated = new_allocated;
            hsp_allocated = new_allocated;
         }
      } else {
         hsp_list->do_not_reallocate = TRUE; 
      }
      /* If it is the first time when the HSP array is filled to capacity,
         create a heap now. */
      if (hsp_list->do_not_reallocate) {
          CreateHeap(hsp_array, hspcnt, sizeof(BlastHSP*), 
                     score_compare_hsps);
      }
   }

   /* If there is space in the allocated HSP array, simply save the new HSP. 
      Othewise, if the new HSP has lower score than the worst HSP in the heap,
      then delete it, else insert it in the heap. */
   if (hspcnt < hsp_allocated)
   {
      hsp_array[hsp_list->hspcnt] = new_hsp;
      (hsp_list->hspcnt)++;
      return status;
   } else {
       /* Insert the new HSP in heap. */
       Blast_HSPListInsertHSPInHeap(hsp_list, &new_hsp);
   }
   
   return status;
}

void 
Blast_HSPListSetFrames(EBlastProgramType program_number, BlastHSPList* hsp_list, 
                 Boolean is_ooframe)
{
   BlastHSP* hsp;
   Int4 index;

   if (!hsp_list)
      return;

   for (index=0; index<hsp_list->hspcnt; index++) {
      hsp = hsp_list->hsp_array[index];
      if (is_ooframe && program_number == eBlastTypeBlastx) {
         /* Query offset is in mixed-frame coordinates */
         hsp->query.frame = hsp->query.offset % CODON_LENGTH + 1;
         if ((hsp->context % NUM_FRAMES) >= CODON_LENGTH)
            hsp->query.frame = -hsp->query.frame;
      } else {
         hsp->query.frame = BLAST_ContextToFrame(program_number, hsp->context);
      }
      
      /* Correct offsets in the edit block too */
      if (hsp->gap_info) {
         hsp->gap_info->frame1 = hsp->query.frame;
         hsp->gap_info->frame2 = hsp->subject.frame;
      }
   }
}

Int2 Blast_HSPListGetEvalues(BlastQueryInfo* query_info,
        BlastHSPList* hsp_list, Boolean gapped_calculation, 
        BlastScoreBlk* sbp, double gap_decay_rate)
{
   BlastHSP* hsp;
   BlastHSP** hsp_array;
   Blast_KarlinBlk** kbp;
   Int4 hsp_cnt;
   Int4 index;
   double gap_decay_divisor = 1.;
   
   if (hsp_list == NULL || hsp_list->hspcnt == 0)
      return 0;
   
   if (gapped_calculation)
      kbp = sbp->kbp_gap_std;
   else
      kbp = sbp->kbp_std;
   
   hsp_cnt = hsp_list->hspcnt;
   hsp_array = hsp_list->hsp_array;

   if (gap_decay_rate != 0.)
      gap_decay_divisor = BLAST_GapDecayDivisor(gap_decay_rate, 1);
   
   for (index=0; index<hsp_cnt; index++) {
      hsp = hsp_array[index];

      ASSERT(hsp != NULL);

      /* Get effective search space from the score block, or from the 
         query information block, in order of preference */
      if (sbp->effective_search_sp) {
         hsp->evalue = BLAST_KarlinStoE_simple(hsp->score, kbp[hsp->context],
                          sbp->effective_search_sp);
      } else {
         hsp->evalue = 
            BLAST_KarlinStoE_simple(hsp->score, kbp[hsp->context],
               query_info->eff_searchsp_array[hsp->context]);
      }
      hsp->evalue /= gap_decay_divisor;
   }
   
   /* Assign the best e-value field. Here the best e-value will always be
      attained for the first HSP in the list. Check that the incoming
      HSP list is properly sorted by score. */
   ASSERT(Blast_HSPListIsSortedByScore(hsp_list));
   hsp_list->best_evalue = hsp_list->hsp_array[0]->evalue;

   return 0;
}

Int2 Blast_HSPListGetBitScores(BlastHSPList* hsp_list, 
                               Boolean gapped_calculation, BlastScoreBlk* sbp)
{
   BlastHSP* hsp;
   Blast_KarlinBlk** kbp;
   Int4 index;
   
   if (hsp_list == NULL)
      return 1;
   
   if (gapped_calculation)
      kbp = sbp->kbp_gap_std;
   else
      kbp = sbp->kbp_std;
   
   for (index=0; index<hsp_list->hspcnt; index++) {
      hsp = hsp_list->hsp_array[index];
      ASSERT(hsp != NULL);
      hsp->bit_score = 
         (hsp->score*kbp[hsp->context]->Lambda - kbp[hsp->context]->logK) / 
         NCBIMATH_LN2;
   }
   
   return 0;
}

void Blast_HSPListPHIGetEvalues(BlastHSPList* hsp_list, BlastScoreBlk* sbp)
{
   Int4 index;
   BlastHSP* hsp;

   if (!hsp_list || hsp_list->hspcnt == 0)
       return;

   for (index = 0; index < hsp_list->hspcnt; ++index) {
      hsp = hsp_list->hsp_array[index];
      Blast_HSPPHIGetEvalue(hsp, sbp);
   }
   /* The best e-value is the one for the highest scoring HSP, which 
      must be the first in the list. Check that HSPs are sorted by score
      to make sure this assumption is correct. */
   ASSERT(Blast_HSPListIsSortedByScore(hsp_list));
   hsp_list->best_evalue = hsp_list->hsp_array[0]->evalue;
}

Int2 Blast_HSPListReapByEvalue(BlastHSPList* hsp_list, 
        BlastHitSavingOptions* hit_options)
{
   BlastHSP* hsp;
   BlastHSP** hsp_array;
   Int4 hsp_cnt = 0;
   Int4 index;
   double cutoff;
   
   if (hsp_list == NULL)
      return 0;

   cutoff = hit_options->expect_value;

   hsp_array = hsp_list->hsp_array;
   for (index = 0; index < hsp_list->hspcnt; index++) {
      hsp = hsp_array[index];

      ASSERT(hsp != NULL);
      
      if (hsp->evalue > cutoff) {
         hsp_array[index] = Blast_HSPFree(hsp_array[index]);
      } else {
         if (index > hsp_cnt)
            hsp_array[hsp_cnt] = hsp_array[index];
         hsp_cnt++;
      }
   }
      
   hsp_list->hspcnt = hsp_cnt;

   return 0;
}

/* See description in blast_hits.h */

Int2
Blast_HSPListPurgeNullHSPs(BlastHSPList* hsp_list)

{
        Int4 index, index1; /* loop indices. */
        Int4 hspcnt; /* total number of HSP's to iterate over. */
        BlastHSP** hsp_array;  /* hsp_array to purge. */

	if (hsp_list == NULL || hsp_list->hspcnt == 0)
		return 0;

        hsp_array = hsp_list->hsp_array;
        hspcnt = hsp_list->hspcnt;

	index1 = 0;
	for (index=0; index<hspcnt; index++)
	{
		if (hsp_array[index] != NULL)
		{
			hsp_array[index1] = hsp_array[index];
			index1++;
		}
	}

	for (index=index1; index<hspcnt; index++)
	{
		hsp_array[index] = NULL;
	}

	hsp_list->hspcnt = index1;

        return 0;
}

typedef enum EHSPInclusionStatus {
   eEqual = 0,      /**< Identical */
   eFirstInSecond,  /**< First included in rectangle formed by second */
   eSecondInFirst,  /**< Second included in rectangle formed by first */
   eDiagNear,       /**< Diagonals are near, but neither HSP is included in
                       the other. */
   eDiagDistant     /**< Diagonals are far apart, or different contexts */
} EHSPInclusionStatus;

/** Are the two HSPs within a given diagonal distance of each other? */
#define MB_HSP_CLOSE(q1, q2, s1, s2, c) (ABS(((q1)-(s1)) - ((q2)-(s2))) < (c))
#define MIN_DIAG_DIST 60

/** HSP inclusion criterion for megablast: one HSP must be included in a
 * diagonal strip of a certain width around the other, and also in a rectangle
 * formed by the other HSP's endpoints.
 */
static EHSPInclusionStatus 
Blast_HSPInclusionTest(BlastHSP* hsp1, BlastHSP* hsp2)
{
   if (hsp1->context != hsp2->context || 
       !MB_HSP_CLOSE(hsp1->query.offset, hsp2->query.offset,
                     hsp1->subject.offset, hsp2->subject.offset, 
                     MIN_DIAG_DIST))
      return eDiagDistant;

   if (hsp1->query.offset == hsp2->query.offset && 
       hsp1->query.end == hsp2->query.end &&  
       hsp1->subject.offset == hsp2->subject.offset && 
       hsp1->subject.end == hsp2->subject.end && 
       hsp1->score == hsp2->score) {
      return eEqual;
   } else if (hsp1->query.offset >= hsp2->query.offset && 
       hsp1->query.end <= hsp2->query.end &&  
       hsp1->subject.offset >= hsp2->subject.offset && 
       hsp1->subject.end <= hsp2->subject.end && 
       hsp1->score <= hsp2->score) { 
      return eFirstInSecond;
   } else if (hsp1->query.offset <= hsp2->query.offset &&  
              hsp1->query.end >= hsp2->query.end &&  
              hsp1->subject.offset <= hsp2->subject.offset && 
              hsp1->subject.end >= hsp2->subject.end && 
              hsp1->score >= hsp2->score) { 
      return eSecondInFirst;
   }
   return eDiagNear;
}

/** How many HSPs to check for inclusion for each new HSP? */
#define MAX_NUM_CHECK_INCLUSION 20

/** Sort the HSPs in an HSP list by diagonal and remove redundant HSPs */
Int2
Blast_HSPListUniqSort(BlastHSPList* hsp_list)
{
   Int4 index, new_hspcnt, index1, index2;
   BlastHSP** hsp_array = hsp_list->hsp_array;
   Boolean shift_needed = FALSE;
   EHSPInclusionStatus inclusion_status = eDiagNear;

   if (hsp_list->hspcnt <= 1)
      return 0;

   qsort(hsp_array, hsp_list->hspcnt, sizeof(BlastHSP*), 
         diag_compare_hsps);

   for (index=1, new_hspcnt=0; index<hsp_list->hspcnt; index++) {
      if (!hsp_array[index])
         break;
      inclusion_status = eDiagNear;
      for (index1 = new_hspcnt; inclusion_status != eDiagDistant &&
           index1 >= 0 && new_hspcnt-index1 < MAX_NUM_CHECK_INCLUSION;
           index1--) {
         inclusion_status = 
            Blast_HSPInclusionTest(hsp_array[index], hsp_array[index1]);
         if (inclusion_status == eFirstInSecond || 
             inclusion_status == eEqual) {
            /* Free the new HSP and break out of the inclusion test loop */
            hsp_array[index] = Blast_HSPFree(hsp_array[index]);
            break;
         } else if (inclusion_status == eSecondInFirst) {
            hsp_array[index1] = Blast_HSPFree(hsp_array[index1]);
            shift_needed = TRUE;
         }
      }
      
      /* If some lower indexed HSPs have been removed, shift the subsequent 
         HSPs */
      if (shift_needed) {
         /* Find the first non-NULL HSP, going backwards */
         while (index1 >= 0 && !hsp_array[index1])
            index1--;
         /* Go forward, and shift any non-NULL HSPs */
         for (index2 = ++index1; index1 <= new_hspcnt; index1++) {
            if (hsp_array[index1])
               hsp_array[index2++] = hsp_array[index1];
         }
         new_hspcnt = index2 - 1;
         shift_needed = FALSE;
      }
      if (hsp_array[index] != NULL)
         hsp_array[++new_hspcnt] = hsp_array[index];
   }
   /* Set all HSP pointers that will not be used any more to NULL */
   memset(&hsp_array[new_hspcnt+1], 0, 
	  (hsp_list->hspcnt - new_hspcnt - 1)*sizeof(BlastHSP*));
   hsp_list->hspcnt = new_hspcnt + 1;

   /* Sort the HSP array by score again. */
   Blast_HSPListSortByScore(hsp_list);

   return 0;
}

Int2 
Blast_HSPListReevaluateWithAmbiguities(BlastHSPList* hsp_list,
   BLAST_SequenceBlk* query_blk, BLAST_SequenceBlk* subject_blk, 
   const BlastHitSavingParameters* hit_params, BlastQueryInfo* query_info, 
   BlastScoreBlk* sbp, const BlastScoringParameters* score_params, 
   const BlastSeqSrc* seq_src)
{
   BlastHSP** hsp_array,* hsp;
   Uint1* query_start,* subject_start = NULL;
   Int4 index, context, hspcnt;
   Boolean purge, delete_hsp;
   Int2 status = 0;

   if (!hsp_list)
      return status;

   hspcnt = hsp_list->hspcnt;
   hsp_array = hsp_list->hsp_array;

   if (hsp_list->hspcnt == 0)
      /* All HSPs have been deleted */
      return status;

   /* Retrieve the unpacked subject sequence and save it in the 
      sequence_start element of the subject structure.
      NB: for the BLAST 2 Sequences case, the uncompressed sequence must 
      already be there */
   if (seq_src) {
      /* Wrap subject sequence block into a GetSeqArg structure, which is 
         needed by the BlastSeqSrc API. */
      GetSeqArg seq_arg;
      seq_arg.oid = subject_blk->oid;
      seq_arg.encoding = BLASTNA_ENCODING;
      seq_arg.seq = subject_blk;
      /* Return the packed sequence to the database */
      BLASTSeqSrcRetSequence(seq_src, (void*) &seq_arg);
      /* Get the unpacked sequence */
      BLASTSeqSrcGetSequence(seq_src, (void*) &seq_arg);
   }
   /* The sequence in blastna encoding is now stored in sequence_start */
   subject_start = subject_blk->sequence_start + 1;

   purge = FALSE;
   for (index=0; index<hspcnt; index++) {
      if (hsp_array[index] == NULL)
         continue;
      else
         hsp = hsp_array[index];

      context = hsp->context;

      query_start = query_blk->sequence + query_info->context_offsets[context];

      delete_hsp = 
         Blast_HSPReevaluateWithAmbiguities(hsp, query_start, subject_start,
            hit_params, score_params, query_info, sbp);
      
      if (delete_hsp) { /* This HSP is now below the cutoff */
         hsp_array[index] = Blast_HSPFree(hsp_array[index]);
         purge = TRUE;
      }
   }
    
   if (purge) {
      Blast_HSPListPurgeNullHSPs(hsp_list);
   }

   /* Sort the HSP array by score (scores may have changed!) */
   Blast_HSPListSortByScore(hsp_list);

   return status;
}

/** Combine two HSP lists, without altering the individual HSPs, and without 
 * reallocating the HSP array. 
 * @param hsp_list New HSP list [in]
 * @param combined_hsp_list Old HSP list, to which new HSPs are added [in] [out]
 * @param new_hspcnt How many HSPs to save in the combined list? The extra ones
 *                   are freed. The best scoring HSPs are saved. This argument
 *                   cannot be greater than the allocated size of the 
 *                   combined list's HSP array. [in]
 */
static void 
Blast_HSPListsCombineByScore(BlastHSPList* hsp_list, 
                             BlastHSPList* combined_hsp_list,
                             Int4 new_hspcnt)
{
   Int4 index, index1, index2;
   BlastHSP** new_hsp_array;

   ASSERT(new_hspcnt <= combined_hsp_list->allocated);

   if (new_hspcnt >= hsp_list->hspcnt + combined_hsp_list->hspcnt) {
      /* All HSPs from both arrays are saved */
      for (index=combined_hsp_list->hspcnt, index1=0; 
           index1<hsp_list->hspcnt; index1++) {
         if (hsp_list->hsp_array[index1] != NULL)
            combined_hsp_list->hsp_array[index++] = hsp_list->hsp_array[index1];
      }
      combined_hsp_list->hspcnt = new_hspcnt;
      Blast_HSPListSortByScore(combined_hsp_list);
   } else {
      /* Not all HSPs are be saved; sort both arrays by score and save only
         the new_hspcnt best ones. 
         For the merged set of HSPs, allocate array the same size as in the 
         old HSP list. */
      new_hsp_array = (BlastHSP**) 
         malloc(combined_hsp_list->allocated*sizeof(BlastHSP*));

      Blast_HSPListSortByScore(combined_hsp_list);
      Blast_HSPListSortByScore(hsp_list);
      index1 = index2 = 0;
      for (index = 0; index < new_hspcnt; ++index) {
         if (index1 < combined_hsp_list->hspcnt &&
             (index2 >= hsp_list->hspcnt ||
              score_compare_hsps(&combined_hsp_list->hsp_array[index1],
                                 &hsp_list->hsp_array[index2]) <= 0)) {
            new_hsp_array[index] = combined_hsp_list->hsp_array[index1];
            ++index1;
         } else {
            new_hsp_array[index] = hsp_list->hsp_array[index2];
            ++index2;
         }
      }
      /* Free the extra HSPs that could not be saved */
      for ( ; index1 < combined_hsp_list->hspcnt; ++index1) {
         combined_hsp_list->hsp_array[index1] = 
            Blast_HSPFree(combined_hsp_list->hsp_array[index1]);
      }
      for ( ; index2 < hsp_list->hspcnt; ++index2) {
         hsp_list->hsp_array[index2] = 
            Blast_HSPFree(hsp_list->hsp_array[index2]);
      }
      /* Point combined_hsp_list's HSP array to the new one */
      sfree(combined_hsp_list->hsp_array);
      combined_hsp_list->hsp_array = new_hsp_array;
      combined_hsp_list->hspcnt = new_hspcnt;
   }
   
   /* Second HSP list now does not own any HSPs */
   hsp_list->hspcnt = 0;
}

Int2 Blast_HSPListAppend(BlastHSPList* hsp_list,
        BlastHSPList** combined_hsp_list_ptr, Int4 hsp_num_max)
{
   BlastHSPList* combined_hsp_list = *combined_hsp_list_ptr;
   BlastHSP** new_hsp_array;
   Int4 new_hspcnt;

   if (!hsp_list || hsp_list->hspcnt == 0)
      return 0;

   /* If no previous HSP list, just return a copy of the new one. */
   if (!combined_hsp_list) {
      if (!(combined_hsp_list = Blast_HSPListDup(hsp_list)))
         return 1;
      *combined_hsp_list_ptr = combined_hsp_list;
      hsp_list->hspcnt = 0;

      return 0;
   }
   /* Just append new list to the end of the old list, in case of 
      multiple frames of the subject sequence */
   new_hspcnt = MIN(combined_hsp_list->hspcnt + hsp_list->hspcnt, 
                    hsp_num_max);
   if (new_hspcnt > combined_hsp_list->allocated && 
       !combined_hsp_list->do_not_reallocate) {
      Int4 new_allocated = MIN(2*new_hspcnt, hsp_num_max);
      new_hsp_array = (BlastHSP**) 
         realloc(combined_hsp_list->hsp_array, 
                 new_allocated*sizeof(BlastHSP*));
      
      if (new_hsp_array) {
         combined_hsp_list->allocated = new_allocated;
         combined_hsp_list->hsp_array = new_hsp_array;
      } else {
         combined_hsp_list->do_not_reallocate = TRUE;
         new_hspcnt = combined_hsp_list->allocated;
      }
   }
   if (combined_hsp_list->allocated == hsp_num_max)
      combined_hsp_list->do_not_reallocate = TRUE;

   Blast_HSPListsCombineByScore(hsp_list, combined_hsp_list, new_hspcnt);

   return 0;
}

Int2 Blast_HSPListsMerge(BlastHSPList* hsp_list, 
                   BlastHSPList** combined_hsp_list_ptr,
                   Int4 hsp_num_max, Int4 start, Boolean merge_hsps)
{
   BlastHSPList* combined_hsp_list = *combined_hsp_list_ptr;
   BlastHSP* hsp, *hsp_var;
   BlastHSP** hspp1,** hspp2;
   Int4 index, index1;
   Int4 hspcnt1, hspcnt2, new_hspcnt = 0;
   BlastHSP** new_hsp_array;
  
   if (!hsp_list || hsp_list->hspcnt == 0)
      return 0;

   /* If no previous HSP list, just return a copy of the new one. */
   if (!combined_hsp_list) {
      *combined_hsp_list_ptr = Blast_HSPListDup(hsp_list);
      hsp_list->hspcnt = 0;
      return 0;
   }

   /* Merge the two HSP lists for successive chunks of the subject sequence */
   hspcnt1 = hspcnt2 = 0;

   /* Put all HSPs that intersect the overlap region at the front of the
      respective HSP arrays. */
   for (index = 0; index < combined_hsp_list->hspcnt; index++) {
      hsp = combined_hsp_list->hsp_array[index];
      if (hsp->subject.end > start) {
         /* At least part of this HSP lies in the overlap strip. */
         hsp_var = combined_hsp_list->hsp_array[hspcnt1];
         combined_hsp_list->hsp_array[hspcnt1] = hsp;
         combined_hsp_list->hsp_array[index] = hsp_var;
         ++hspcnt1;
      }
   }
   for (index = 0; index < hsp_list->hspcnt; index++) {
      hsp = hsp_list->hsp_array[index];
      if (hsp->subject.offset < start + DBSEQ_CHUNK_OVERLAP) {
         /* At least part of this HSP lies in the overlap strip. */
         hsp_var = hsp_list->hsp_array[hspcnt2];
         hsp_list->hsp_array[hspcnt2] = hsp;
         hsp_list->hsp_array[index] = hsp_var;
         ++hspcnt2;
      }
   }

   hspp1 = combined_hsp_list->hsp_array;
   hspp2 = hsp_list->hsp_array;
   /* Sort HSPs from in the overlap region by diagonal */
   qsort(hspp1, hspcnt1, sizeof(BlastHSP*), diag_compare_hsps);
   qsort(hspp2, hspcnt2, sizeof(BlastHSP*), diag_compare_hsps);

   for (index=0; index<hspcnt1; index++) {
      for (index1 = 0; index1<hspcnt2; index1++) {
         /* Skip already deleted HSPs */
         if (!hspp2[index1])
            continue;
         if (hspp1[index]->context == hspp2[index1]->context && 
             ABS(diag_compare_hsps(&hspp1[index], &hspp2[index1])) < 
             OVERLAP_DIAG_CLOSE) {
            if (merge_hsps) {
               if (Blast_HSPsMerge(hspp1[index], hspp2[index1], start)) {
                  /* Free the second HSP. */
                  hspp2[index1] = Blast_HSPFree(hspp2[index1]);
               }
            } else { /* No gap information available */
               if (Blast_HSPContained(hspp1[index], hspp2[index1])) {
                  sfree(hspp1[index]);
                  /* Point the first HSP to the new HSP; free the old HSP. */
                  hspp1[index] = hspp2[index1];
                  hspp2[index1] = NULL;
                  /* This HSP has been removed, so break out of the inner 
                     loop */
                  break;
               } else if (Blast_HSPContained(hspp2[index1], hspp1[index])) {
                  /* Just free the second HSP */
                  hspp2[index1] = Blast_HSPFree(hspp2[index1]);
               }
            }
         } else {
            /* This and remaining HSPs are too far from the one being 
               checked */
            break;
         }
      }
   }

   /* Purge the nulled out HSPs from the new HSP list */
   Blast_HSPListPurgeNullHSPs(hsp_list);

   /* The new number of HSPs is now the sum of the remaining counts in the 
      two lists, but if there is a restriction on the number of HSPs to keep,
      it might have to be reduced. */
   new_hspcnt = 
      MIN(hsp_list->hspcnt + combined_hsp_list->hspcnt, hsp_num_max);
   
   if (new_hspcnt >= combined_hsp_list->allocated-1 && 
       combined_hsp_list->do_not_reallocate == FALSE) {
      Int4 new_allocated = MIN(2*new_hspcnt, hsp_num_max);
      if (new_allocated > combined_hsp_list->allocated) {
         new_hsp_array = (BlastHSP**) 
            realloc(combined_hsp_list->hsp_array, 
                    new_allocated*sizeof(BlastHSP*));
         if (new_hsp_array == NULL) {
            combined_hsp_list->do_not_reallocate = TRUE; 
         } else {
            combined_hsp_list->hsp_array = new_hsp_array;
            combined_hsp_list->allocated = new_allocated;
         }
      } else {
         combined_hsp_list->do_not_reallocate = TRUE;
      }
      new_hspcnt = MIN(new_hspcnt, combined_hsp_list->allocated);
   }

   Blast_HSPListsCombineByScore(hsp_list, combined_hsp_list, new_hspcnt);

   return 0;
}

void Blast_HSPListAdjustOffsets(BlastHSPList* hsp_list, Int4 offset)
{
   Int4 index;
   BlastHSP* hsp;

   for (index=0; index<hsp_list->hspcnt; index++) {
      hsp = hsp_list->hsp_array[index];
      hsp->subject.offset += offset;
      hsp->subject.end += offset;
      hsp->subject.gapped_start += offset;
      if (hsp->gap_info)
         hsp->gap_info->start2 += offset;
   }
}

/** Callback for sorting hsp lists by their best evalue/score;
 * Evalues are compared only up to a relative error of 
 * FUZZY_EVALUE_COMPARE_FACTOR. 
 * It is assumed that the HSP arrays in each hit list are already sorted by 
 * e-value/score.
*/
static int
evalue_compare_hsp_lists(const void* v1, const void* v2)
{
   BlastHSPList* h1,* h2;
   int retval = 0;
   
   h1 = *(BlastHSPList**) v1;
   h2 = *(BlastHSPList**) v2;
   
   /* If any of the HSP lists is empty, it is considered "worse" than the 
      other, unless the other is also empty. */
   if (h1->hspcnt == 0 && h2->hspcnt == 0)
      return 0;
   else if (h1->hspcnt == 0)
      return 1;
   else if (h2->hspcnt == 0)
      return -1;

   if ((retval = fuzzy_evalue_comp(h1->best_evalue, 
                                   h2->best_evalue)) != 0)
      return retval;

   if (h1->hsp_array[0]->score > h2->hsp_array[0]->score)
      return -1;
   if (h1->hsp_array[0]->score < h2->hsp_array[0]->score)
      return 1;
   
   /* In case of equal best E-values and scores, order will be determined
      by ordinal ids of the subject sequences */
   if (h1->oid > h2->oid)
      return -1;
   if (h1->oid < h2->oid)
      return 1;

   return 0;
}

/** Callback for sorting hsp lists by their best e-value/score, in 
 * reverse order - from higher e-value to lower (lower score to higher).
*/
static int
evalue_compare_hsp_lists_rev(const void* v1, const void* v2)
{
   return evalue_compare_hsp_lists(v2, v1);
}

/********************************************************************************
          Functions manipulating BlastHitList's
********************************************************************************/

/*
   description in blast_hits.h
 */
BlastHitList* Blast_HitListNew(Int4 hitlist_size)
{
   BlastHitList* new_hitlist = (BlastHitList*) 
      calloc(1, sizeof(BlastHitList));
   new_hitlist->hsplist_max = hitlist_size;
   new_hitlist->low_score = INT4_MAX;
   return new_hitlist;
}

/*
   description in blast_hits.h
*/
BlastHitList* Blast_HitListFree(BlastHitList* hitlist)
{
   if (!hitlist)
      return NULL;

   Blast_HitListHSPListsFree(hitlist);
   sfree(hitlist);
   return NULL;
}

/* description in blast_hits.h */
Int2 Blast_HitListHSPListsFree(BlastHitList* hitlist)
{
   Int4 index;
   
   if (!hitlist)
	return 0;

   for (index = 0; index < hitlist->hsplist_count; ++index)
      hitlist->hsplist_array[index] =
         Blast_HSPListFree(hitlist->hsplist_array[index]);

   sfree(hitlist->hsplist_array);

   hitlist->hsplist_count = 0;

   return 0;
}

static void Blast_HitListPurge(BlastHitList* hit_list)
{
   Int4 index, hsplist_count;
   
   if (!hit_list)
      return;
   
   hsplist_count = hit_list->hsplist_count;
   for (index = 0; index < hsplist_count && 
           hit_list->hsplist_array[index]->hspcnt > 0; ++index);
   
   hit_list->hsplist_count = index;
   /* Free all empty HSP lists */
   for ( ; index < hsplist_count; ++index) {
      Blast_HSPListFree(hit_list->hsplist_array[index]);
   }
}

/** Given a BlastHitList* with a heapified HSP list array, remove
 *  the worst scoring HSP list and insert the new HSP list in the heap
 * @param hit_list Contains all HSP lists for a given query [in] [out]
 * @param hsp_list A new HSP list to be inserted into the hit list [in]
 */
static void 
Blast_HitListInsertHSPListInHeap(BlastHitList* hit_list, 
                                 BlastHSPList* hsp_list)
{
      Blast_HSPListFree(hit_list->hsplist_array[0]);
      hit_list->hsplist_array[0] = hsp_list;
      if (hit_list->hsplist_count >= 2) {
         heapify((char*)hit_list->hsplist_array, (char*)hit_list->hsplist_array, 
                 (char*)&hit_list->hsplist_array[hit_list->hsplist_count/2 - 1],
                 (char*)&hit_list->hsplist_array[hit_list->hsplist_count-1],
                 sizeof(BlastHSPList*), evalue_compare_hsp_lists);
      }
      hit_list->worst_evalue = 
         hit_list->hsplist_array[0]->best_evalue;
      hit_list->low_score = hit_list->hsplist_array[0]->hsp_array[0]->score;
}

Int2 Blast_HitListUpdate(BlastHitList* hit_list, 
                         BlastHSPList* hsp_list)
{
   if (hit_list->hsplist_count < hit_list->hsplist_max) {
      /* If the array of HSP lists for this query is not yet allocated, 
         do it here */
      if (!hit_list->hsplist_array)
         hit_list->hsplist_array = (BlastHSPList**)
            malloc(hit_list->hsplist_max*sizeof(BlastHSPList*));
      /* Just add to the end; sort later */
      hit_list->hsplist_array[hit_list->hsplist_count++] = hsp_list;
      hit_list->worst_evalue = 
         MAX(hsp_list->best_evalue, hit_list->worst_evalue);
      hit_list->low_score = 
         MIN(hsp_list->hsp_array[0]->score, hit_list->low_score);
   } else {
      /* Compare e-values only with a certain precision */
      int evalue_order = fuzzy_evalue_comp(hsp_list->best_evalue,
                                           hit_list->worst_evalue);
      if (evalue_order > 0 || 
          (evalue_order == 0 &&
           (hsp_list->hsp_array[0]->score < hit_list->low_score))) {
         /* This hit list is less significant than any of those already saved;
            discard it. Note that newer hits with score and e-value both equal
            to the current worst will be saved, at the expense of some older 
            hit.
         */
         Blast_HSPListFree(hsp_list);
      } else {
         if (!hit_list->heapified) {
            CreateHeap(hit_list->hsplist_array, hit_list->hsplist_count, 
                       sizeof(BlastHSPList*), evalue_compare_hsp_lists);
            hit_list->heapified = TRUE;
         }
         Blast_HitListInsertHSPListInHeap(hit_list, hsp_list);
      }
   }
   return 0;
}

static Int2 Blast_HitListPurgeNullHSPLists(BlastHitList* hit_list)
{
   Int4 index, index1; /* loop indices. */
   Int4 hsplist_count; /* total number of HSPList's to iterate over. */
   BlastHSPList** hsplist_array;  /* hsplist_array to purge. */

   if (hit_list == NULL || hit_list->hsplist_count == 0)
      return 0;

   hsplist_array = hit_list->hsplist_array;
   hsplist_count = hit_list->hsplist_count;

   index1 = 0;
   for (index=0; index<hsplist_count; index++) {
      if (hsplist_array[index]) {
         hsplist_array[index1] = hsplist_array[index];
         index1++;
      }
   }

   for (index=index1; index<hsplist_count; index++) {
      hsplist_array[index] = NULL;
   }

   hit_list->hsplist_count = index1;

   return 0;
}

/********************************************************************************
          Functions manipulating BlastHSPResults
********************************************************************************/

BlastHSPResults* Blast_HSPResultsNew(Int4 num_queries)
{
   BlastHSPResults* retval = NULL;

   retval = (BlastHSPResults*) malloc(sizeof(BlastHSPResults));
   if ( !retval ) {
       return NULL;
   }

   retval->num_queries = num_queries;
   retval->hitlist_array = (BlastHitList**) 
      calloc(num_queries, sizeof(BlastHitList*));

   if ( !retval->hitlist_array ) {
       return Blast_HSPResultsFree(retval);
   }
   
   return retval;
}

BlastHSPResults* Blast_HSPResultsFree(BlastHSPResults* results)
{
   Int4 index;

   if (!results)
       return NULL;

   for (index = 0; index < results->num_queries; ++index)
      Blast_HitListFree(results->hitlist_array[index]);
   sfree(results->hitlist_array);
   sfree(results);
   return NULL;
}

Int2 Blast_HSPResultsSortByEvalue(BlastHSPResults* results)
{
   Int4 index;
   BlastHitList* hit_list;

   for (index = 0; index < results->num_queries; ++index) {
      hit_list = results->hitlist_array[index];
      if (hit_list && hit_list->hsplist_count > 1) {
         qsort(hit_list->hsplist_array, hit_list->hsplist_count, 
                  sizeof(BlastHSPList*), evalue_compare_hsp_lists);
      }
      Blast_HitListPurge(hit_list);
   }
   return 0;
}

Int2 Blast_HSPResultsReverseSort(BlastHSPResults* results)
{
   Int4 index;
   BlastHitList* hit_list;

   for (index = 0; index < results->num_queries; ++index) {
      hit_list = results->hitlist_array[index];
      if (hit_list && hit_list->hsplist_count > 1) {
         qsort(hit_list->hsplist_array, hit_list->hsplist_count, 
               sizeof(BlastHSPList*), evalue_compare_hsp_lists_rev);
      }
      Blast_HitListPurge(hit_list);
   }
   return 0;
}

Int2 Blast_HSPResultsReverseOrder(BlastHSPResults* results)
{
   Int4 index;
   BlastHitList* hit_list;

   for (index = 0; index < results->num_queries; ++index) {
      hit_list = results->hitlist_array[index];
      if (hit_list && hit_list->hsplist_count > 1) {
	 BlastHSPList* hsplist;
	 Int4 index1;
	 /* Swap HSP lists: first with last; second with second from the end,
	    etc. */
	 for (index1 = 0; index1 < hit_list->hsplist_count/2; ++index1) {
	    hsplist = hit_list->hsplist_array[index1];
	    hit_list->hsplist_array[index1] = 
	       hit_list->hsplist_array[hit_list->hsplist_count-index1-1];
	    hit_list->hsplist_array[hit_list->hsplist_count-index1-1] =
	       hsplist;
	 }
      }
   }
   return 0;
}

Int2 Blast_HSPResultsSaveRPSHSPList(EBlastProgramType program, BlastHSPResults* results, 
        BlastHSPList* hsplist_in, const BlastHitSavingOptions* hit_options)
{
   Int4 index, oid;
   BlastHitList* hit_list;
   BlastHSPList* hsp_list;
   BlastHSP* hsp;
   Int2 status = 0;

   if (!hsplist_in)
      return 0;

   /* Query index should be filled before saving the HSP list for RPS BLAST. 
      The hitlist size for RPS BLAST should be available in the preliminary
      hitlist size option. Because of the database concatenation, there is
      no hitlist size restriction in the preliminary phase of the search. */
   hit_list = results->hitlist_array[hsplist_in->query_index];
   if (!hit_list) {
       results->hitlist_array[hsplist_in->query_index] = 
           hit_list = Blast_HitListNew(hit_options->prelim_hitlist_size);
   }

   /* Initialize the HSPList array, if necessary. */
   if (hsplist_in->hspcnt > 0 && !hit_list->hsplist_array) {
      hit_list->hsplist_array = (BlastHSPList**)
         calloc(hit_list->hsplist_max, sizeof(BlastHSPList*));
   }

   /* Save all HSPs into HSPList's corresponding to correct database 
      sequences. */
   for (index = 0; index < hsplist_in->hspcnt; ++index) {
      hsp = hsplist_in->hsp_array[index];
      oid = Blast_GetQueryIndexFromContext(hsp->context, program);
      /* HSP context is no longer needed; set it to 0. */
      hsp->context = 0;
      ASSERT(oid < hit_list->hsplist_max);
      hsp_list = hit_list->hsplist_array[oid];
      if (!hsp_list) {
         hsp_list = hit_list->hsplist_array[oid] = 
            Blast_HSPListNew(hit_options->hsp_num_max);
         hsp_list->oid = oid;
      }
      status = Blast_HSPListSaveHSP(hsp_list, hsp);
   }
   /* Purge the NULL HSPLists from the resulting HitList. */
   hit_list->hsplist_count = hit_list->hsplist_max;
   Blast_HitListPurgeNullHSPLists(hit_list);

   /* Check that all HSP lists are sorted by score, as they should be. */
   for (index = 0; index < hit_list->hsplist_count; ++index) {
       ASSERT(Blast_HSPListIsSortedByScore(hit_list->hsplist_array[index]));
   }
   /* Sort the HSPList's by score - e-values are not yet available, but
      the evalue comparison function looks at scores as tie-breakers,
      so it can still be used here. */
   qsort(hit_list->hsplist_array, hit_list->hsplist_count,
         sizeof(BlastHSPList*), evalue_compare_hsp_lists);
   /* Leave only the number of HSPList's allowed by the hitlist size 
      option. */
   for (index = hit_options->prelim_hitlist_size; 
        index < hit_list->hsplist_count; ++index) {
      hit_list->hsplist_array[index] = 
         Blast_HSPListFree(hit_list->hsplist_array[index]);
   }
   hit_list->hsplist_count = 
      MIN(hit_list->hsplist_count, hit_options->prelim_hitlist_size);

   /* All HSPs from the input HSP list have been moved to the results 
      structure, so make sure there is no attempt to free them now. */
   sfree(hsplist_in->hsp_array);
   hsplist_in->hspcnt = 0;
   hsplist_in = Blast_HSPListFree(hsplist_in);

   return 0;
}

Int2 Blast_HSPResultsSaveHSPList(EBlastProgramType program, BlastHSPResults* results, 
        BlastHSPList* hsp_list, const BlastHitSavingOptions* hit_options)
{
   Int2 status = 0;
   BlastHSPList** hsp_list_array;
   BlastHSP* hsp;
   Int4 index;

   if (!hsp_list)
      return 0;

   if (!results || !hit_options)
      return -1;

   /* The HSP list should already be sorted by score coming into this function.
    * Still check that this assumption is true.
    */
   ASSERT(Blast_HSPListIsSortedByScore(hsp_list));
   
   /* Rearrange HSPs into multiple hit lists if more than one query */
   if (results->num_queries > 1) {
      BlastHSPList* tmp_hsp_list;
      Int4 query_index;
      hsp_list_array = calloc(results->num_queries, sizeof(BlastHSPList*));
      for (index = 0; index < hsp_list->hspcnt; index++) {
         hsp = hsp_list->hsp_array[index];
         query_index = Blast_GetQueryIndexFromContext(hsp->context, program);
         tmp_hsp_list = hsp_list_array[query_index];

         if (!tmp_hsp_list) {
            hsp_list_array[query_index] = tmp_hsp_list = 
               Blast_HSPListNew(hit_options->hsp_num_max);
            tmp_hsp_list->oid = hsp_list->oid;
         }

         if (!tmp_hsp_list || tmp_hsp_list->do_not_reallocate) {
            tmp_hsp_list = NULL;
         } else if (tmp_hsp_list->hspcnt >= tmp_hsp_list->allocated) {
            BlastHSP** new_hsp_array;
            Int4 new_size = 
               MIN(2*tmp_hsp_list->allocated, tmp_hsp_list->hsp_max);
            if (new_size == tmp_hsp_list->hsp_max)
               tmp_hsp_list->do_not_reallocate = TRUE;
            
            new_hsp_array = realloc(tmp_hsp_list->hsp_array, 
                                    new_size*sizeof(BlastHSP*));
            if (!new_hsp_array) {
               tmp_hsp_list->do_not_reallocate = TRUE;
               tmp_hsp_list = NULL;
            } else {
               tmp_hsp_list->hsp_array = new_hsp_array;
               tmp_hsp_list->allocated = new_size;
            }
         }
         if (tmp_hsp_list) {
            tmp_hsp_list->hsp_array[tmp_hsp_list->hspcnt++] = hsp;
         } else {
            /* Cannot add more HSPs; free the memory */
            hsp_list->hsp_array[index] = Blast_HSPFree(hsp);
         }
      }

      /* Insert the hit list(s) into the appropriate places in the results 
         structure */
      for (index = 0; index < results->num_queries; index++) {
         if (hsp_list_array[index]) {
            if (!results->hitlist_array[index]) {
               results->hitlist_array[index] = 
                  Blast_HitListNew(hit_options->prelim_hitlist_size);
            }
            Blast_HitListUpdate(results->hitlist_array[index], 
                                hsp_list_array[index]);
         }
      }
      sfree(hsp_list_array);
      /* All HSPs from the hsp_list structure are now moved to the results 
         structure, so set the HSP count back to 0 */
      hsp_list->hspcnt = 0;
      Blast_HSPListFree(hsp_list);
   } else if (hsp_list->hspcnt > 0) {
      /* Single query; save the HSP list directly into the results 
         structure */
      if (!results->hitlist_array[0]) {
         results->hitlist_array[0] = 
            Blast_HitListNew(hit_options->prelim_hitlist_size);
      }
      Blast_HitListUpdate(results->hitlist_array[0], hsp_list);
   }
   
   return status; 
}

Int2 Blast_HSPResultsInsertHSPList(BlastHSPResults* results, 
        BlastHSPList* hsp_list, Int4 hitlist_size)
{
   if (!hsp_list || hsp_list->hspcnt == 0)
      return 0;

   ASSERT(hsp_list->query_index < results->num_queries);

   if (!results->hitlist_array[hsp_list->query_index]) {
      results->hitlist_array[hsp_list->query_index] = 
         Blast_HitListNew(hitlist_size);
   }
   Blast_HitListUpdate(results->hitlist_array[hsp_list->query_index], 
                       hsp_list);
   return 0;
}
