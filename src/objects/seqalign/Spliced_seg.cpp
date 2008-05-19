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
 * Author:  Kamen Todorov
 *
 * File Description:
 *   User-defined methods for Spliced-seg.
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using the following specifications:
 *   'seqalign.asn'.
 */

// standard includes
#include <ncbi_pch.hpp>

// generated includes
#include <objects/seqalign/Spliced_seg.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CSpliced_seg::~CSpliced_seg(void)
{
}


ENa_strand 
CSpliced_seg::GetSeqStrand(TDim row) const 
{
    switch (row) {
    case 0:
        if (CanGetProduct_strand()) {
            return GetProduct_strand();
        } else {
            if ((*GetExons().begin())->CanGetProduct_strand()) {
                return (*GetExons().begin())->GetProduct_strand();
            } else {
                return eNa_strand_unknown;
            }
        }
        break;
    case 1:
        if (CanGetGenomic_strand()) {
            return GetGenomic_strand();
        } else {
            if ((*GetExons().begin())->CanGetGenomic_strand()) {
                return (*GetExons().begin())->GetGenomic_strand();
            } else {
                return eNa_strand_unknown;
            }
        }
        break;
    default:
        NCBI_THROW(CSeqalignException, eInvalidRowNumber,
                   "CSpliced_seg::GetSeqStrand(): Invalid row number");
    }
}

void CSpliced_seg::Validate(bool full_test) const
{
    bool prot = GetProduct_type() == eProduct_type_protein;

    if (GetExons().empty()) {
        NCBI_THROW(CSeqalignException, eInvalidAlignment,
                   "CSpliced_seg::Validate(): Spiced-seg is empty (has no exons)");
    }

    ITERATE (CSpliced_seg::TExons, exon_it, GetExons()) {

        const CSpliced_exon& exon = **exon_it;

        /// Ids
        if ( !(IsSetProduct_id()  ||  exon.IsSetProduct_id()) ) {
             NCBI_THROW(CSeqalignException, eInvalidAlignment,
                        "product-id not set.");
        }
        if (IsSetProduct_id()  ==  exon.IsSetProduct_id()) {
            NCBI_THROW(CSeqalignException, eInvalidAlignment,
                       "product-id should be set on the level of Spliced-seg XOR Spliced-exon.");
        }
        if ( !(IsSetGenomic_id()  ||  exon.IsSetGenomic_id()) ) {
             NCBI_THROW(CSeqalignException, eInvalidAlignment,
                        "genomic-id not set.");
        }
        if (IsSetGenomic_id()  ==  exon.IsSetGenomic_id()) {
            NCBI_THROW(CSeqalignException, eInvalidAlignment,
                       "genomic-id should be set on the level of Spliced-seg XOR Spliced-exon.");
        }


        /// Strands
        if (IsSetProduct_strand()  &&  exon.IsSetProduct_strand()) {
            NCBI_THROW(CSeqalignException, eInvalidAlignment,
                       "product-strand can be set on level of Spliced-seg XOR Spliced-exon.");
        }
        bool product_plus = true;
        if (exon.CanGetProduct_strand()) {
            product_plus = exon.GetProduct_strand() != eNa_strand_minus;
        } else if (CanGetProduct_strand()) {
            product_plus = GetProduct_strand() != eNa_strand_minus;
        }
        if (prot  &&  !product_plus) {
            NCBI_THROW(CSeqalignException, eInvalidAlignment,
                       "Protein product cannot have a negative strand.");
        }

        if (CanGetGenomic_strand()  &&  exon.CanGetGenomic_strand()) {
            NCBI_THROW(CSeqalignException, eInvalidAlignment,
                       "genomic-strand can be set on level of Spliced-seg XOR Spliced-exon.");
        }
        bool genomic_plus = true;
        if (exon.CanGetGenomic_strand()) {
            genomic_plus = exon.GetGenomic_strand() != eNa_strand_minus;
        } else if (CanGetGenomic_strand()) {
            genomic_plus = GetGenomic_strand() != eNa_strand_minus;
        }


        /// Ranges
        if (exon.IsSetParts()) {
            TSeqPos exon_product_len = 0;
            TSeqPos exon_genomic_len = 0;
            ITERATE (CSpliced_exon::TParts, chunk_it, exon.GetParts()) {
                const CSpliced_exon_chunk& chunk = **chunk_it;
                
                TSeqPos chunk_product_len = 0;
                TSeqPos chunk_genomic_len = 0;
            
                switch (chunk.Which()) {
                case CSpliced_exon_chunk::e_Match: 
                    chunk_product_len = chunk_genomic_len = chunk.GetMatch();
                    break;
                case CSpliced_exon_chunk::e_Diag: 
                    chunk_product_len = chunk_genomic_len = chunk.GetDiag();
                    break;
                case CSpliced_exon_chunk::e_Mismatch:
                    chunk_product_len = chunk_genomic_len = chunk.GetMismatch();
                    break;
                case CSpliced_exon_chunk::e_Product_ins:
                    chunk_product_len = chunk.GetProduct_ins();
                    break;
                case CSpliced_exon_chunk::e_Genomic_ins:
                    chunk_genomic_len = chunk.GetGenomic_ins();
                    break;
                default:
                    break;
                }
                exon_product_len += chunk_product_len;
                exon_genomic_len += chunk_genomic_len;
            }
            if (exon_product_len != 
                (prot ? 
                 exon.GetProduct_end().GetProtpos().GetAmin() * 3 + exon.GetProduct_end().GetProtpos().GetFrame() - 1 -
                 (exon.GetProduct_start().GetProtpos().GetAmin() * 3 + exon.GetProduct_start().GetProtpos().GetFrame() - 1) + 1 :
                 exon.GetProduct_end().GetNucpos() - exon.GetProduct_start().GetNucpos() + 1)) {
                NCBI_THROW(CSeqalignException, eInvalidAlignment,
                           "Product exon range length is not consistent with exon chunks.");
            }
            if (exon_genomic_len != 
                exon.GetGenomic_end() - exon.GetGenomic_start() + 1) {
                NCBI_THROW(CSeqalignException, eInvalidAlignment,
                           "Genomic exon range length is not consistent with exon chunks.");
            }
        }
    }

}


CRange<TSeqPos>
CSpliced_seg::GetSeqRange(TDim row) const
{
    if (GetExons().empty()) {
        NCBI_THROW(CSeqalignException, eInvalidAlignment,
                   "CSpliced_seg::GetSeqRange(): Spiced-seg is empty (has no exons)");
    }
    CRange<TSeqPos> result;
    switch (row) {
    case 0:
        switch (GetProduct_type()) {
        case eProduct_type_transcript:
            ITERATE(TExons, exon_it, GetExons()) {
                result.CombineWith
                    (CRange<TSeqPos>
                     ((*exon_it)->GetProduct_start().GetNucpos(),
                      (*exon_it)->GetProduct_end().GetNucpos()));
            }
            break;
        case eProduct_type_protein:
            ITERATE(TExons, exon_it, GetExons()) {
                result.CombineWith
                    (CRange<TSeqPos>
                     ((*exon_it)->GetProduct_start().GetProtpos().GetAmin(),
                      (*exon_it)->GetProduct_end().GetProtpos().GetAmin()));

            }
            break;
        default:
            NCBI_THROW(CSeqalignException, eInvalidAlignment,
                       "Invalid product type");
        }
        break;
    case 1:
        ITERATE(TExons, exon_it, GetExons()) {
            result.CombineWith
                (CRange<TSeqPos>
                 ((*exon_it)->GetGenomic_start(),
                  (*exon_it)->GetGenomic_end()));
        }
        break;
    default:
        NCBI_THROW(CSeqalignException, eInvalidRowNumber,
                   "CSpliced_seg::GetSeqRange(): Invalid row number");
    }
    return result;
}


TSeqPos
CSpliced_seg::GetSeqStart(TDim row) const
{
    return GetSeqRange(row).GetFrom();
}


TSeqPos
CSpliced_seg::GetSeqStop (TDim row) const
{
    return GetSeqRange(row).GetTo();
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 57, chars: 1732, CRC32: fa335290 */
