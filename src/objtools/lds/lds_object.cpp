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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:  CLDS_Object implementation.
 *
 */


#include <ncbi_pch.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/general/Object_id.hpp>

#include <bdb/bdb_cursor.hpp>
#include <bdb/bdb_util.hpp>

#include <objtools/readers/fasta.hpp>

#include <objtools/lds/lds_object.hpp>
#include <objtools/lds/lds_set.hpp>
#include <objtools/lds/lds_util.hpp>
#include <objtools/lds/lds.hpp>
#include <objtools/lds/lds_query.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/// @internal
class CLDS_FastaScanner : public IFastaEntryScan
{
public:
    CLDS_FastaScanner(CLDS_Object& obj, 
                      int          file_id, 
                      int          type_id);

    virtual void EntryFound(CRef<CSeq_entry> se, 
                            CNcbiStreampos   stream_position);
private:
    CLDS_Object& m_Obj;
    int          m_FileId;
    int          m_TypeId;
};

CLDS_FastaScanner::CLDS_FastaScanner(CLDS_Object& obj, 
                                     int          file_id,
                                     int          type_id)
 : m_Obj(obj),
   m_FileId(file_id),
   m_TypeId(type_id)
{}

void CLDS_FastaScanner::EntryFound(CRef<CSeq_entry> se, 
                                   CNcbiStreampos   stream_position)
{
    if (!se->IsSeq()) 
        return;

    SFastaFileMap::SFastaEntry  fasta_entry;
    fasta_entry.stream_offset = stream_position;

    // extract sequence info

    const CSeq_entry::TSeq& bioseq = se->GetSeq();
    const CSeq_id* sid = bioseq.GetFirstId();
    fasta_entry.seq_id = sid->AsFastaString();

    fasta_entry.all_seq_ids.resize(0);
    if (bioseq.CanGetId()) {
        const CBioseq::TId& seq_ids = bioseq.GetId();
        string id_str;
        ITERATE(CBioseq::TId, it, seq_ids) {
            const CBioseq::TId::value_type& vt = *it;
            id_str = vt->AsFastaString();
            fasta_entry.all_seq_ids.push_back(id_str);
        }
    }

    if (bioseq.CanGetDescr()) {
        const CSeq_descr& d = bioseq.GetDescr();
        if (d.CanGet()) {
            const CSeq_descr_Base::Tdata& data = d.Get();
            if (!data.empty()) {
                CSeq_descr_Base::Tdata::const_iterator it = 
                                                    data.begin();
                if (it != data.end()) {
                    CRef<CSeqdesc> ref_desc = *it;
                    ref_desc->GetLabel(&fasta_entry.description, 
                                        CSeqdesc::eContent);
                }                                
            }
        }
    }

    // store entry record

    // concatenate all ids
    string seq_ids;
    ITERATE(SFastaFileMap::SFastaEntry::TFastaSeqIds, 
            it_id, fasta_entry.all_seq_ids) {
        seq_ids.append(*it_id);
        seq_ids.append(" ");
    }

    m_Obj.SaveObject(m_FileId,
                     fasta_entry.seq_id,
                     fasta_entry.description,
                     seq_ids,
                     fasta_entry.stream_offset,
                     m_TypeId);
    
}





CLDS_Object::CLDS_Object(CLDS_Database& db, 
                         const map<string, int>& obj_map)
: m_DataBase(db),
  m_db(db.GetTables()),
  m_ObjTypeMap(obj_map),
  m_MaxObjRecId(0),
  m_ControlDupIds(false)
{}


CLDS_Object::~CLDS_Object()
{
}

void CLDS_Object::DeleteCascadeFiles(const CLDS_Set& file_ids, 
                                     CLDS_Set* objects_deleted,
                                     CLDS_Set* annotations_deleted)
{
    if (file_ids.count())
        return;

    //
    //  Delete records from "object" table
    //
    {{
    CBDB_FileCursor cur(m_db.object_db); 
    cur.SetCondition(CBDB_FileCursor::eFirst); 
    while (cur.Fetch() == eBDB_Ok) { 
        int fid = m_db.object_db.file_id;
        if (fid && LDS_SetTest(file_ids, fid)) {
/*
            int object_attr_id = m_db.object_db.object_attr_id;
            
            if (object_attr_id) {  // delete dependent object attr
                m_db.object_attr_db.object_attr_id = object_attr_id;
                m_db.object_attr_db.Delete();
            }
*/
            int object_id = m_db.object_db.object_id;

            objects_deleted->set(object_id);
            m_db.object_db.Delete();
        }
    }

    }}

    //
    // Delete "annot2obj"
    //
    {{
    CBDB_FileCursor cur(m_db.annot2obj_db); 
    cur.SetCondition(CBDB_FileCursor::eFirst); 
    while (cur.Fetch() == eBDB_Ok) { 
        int object_id = m_db.annot2obj_db.object_id;
        if (object_id && LDS_SetTest(*objects_deleted, object_id)) {
            m_db.annot2obj_db.Delete();
        }
    }

    }}

    //
    // Delete "annotation"
    //
    {{
    CBDB_FileCursor cur(m_db.annot_db); 
    cur.SetCondition(CBDB_FileCursor::eFirst); 
    while (cur.Fetch() == eBDB_Ok) { 
        int fid = m_db.object_db.file_id;
        if (fid && LDS_SetTest(file_ids, fid)) {
            int annot_id = m_db.annot_db.annot_id;
            annotations_deleted->set(annot_id);
            m_db.annot_db.Delete();
        }
    }

    }}

    //
    // Delete "seq_id_list"
    //
    {{

    {{
    CLDS_Set::enumerator en = objects_deleted->first();
    for ( ; en.valid(); ++en) {
        int id = *en;
        m_db.seq_id_list.object_id = id;
        m_db.seq_id_list.Delete();
    }
    }}

    CLDS_Set::enumerator en = annotations_deleted->first();
    for ( ; en.valid(); ++en) {
        int id = *en;
        m_db.seq_id_list.object_id = id;
        m_db.seq_id_list.Delete();
    }

    }}
}


void CLDS_Object::UpdateCascadeFiles(const CLDS_Set& file_ids)
{
    if (file_ids.none())
        return;

    CLDS_Set objects_deleted;
    CLDS_Set annotations_deleted;
    DeleteCascadeFiles(file_ids, &objects_deleted, &annotations_deleted);


    CLDS_Set::enumerator en(file_ids.first());
    for ( ; en.valid(); ++en) {
        int fid = *en;
        m_db.file_db.file_id = fid;

        if (m_db.file_db.Fetch() == eBDB_Ok) {
            string fname(m_db.file_db.file_name);
            CFormatGuess::EFormat format = 
                (CFormatGuess::EFormat)(int)m_db.file_db.format;
    
            LOG_POST(Info << "<< Updating file >>: " << fname);

            UpdateFileObjects(fid, fname, format);
        }
    } // ITERATE

    // re-index

    if (file_ids.any()) {
        BuildSeqIdIdx();
    }
}


void CLDS_Object::UpdateFileObjects(int file_id, 
                                    const string& file_name, 
                                    CFormatGuess::EFormat format)
{
    FindMaxObjRecId();

    if (format == CFormatGuess::eBinaryASN ||
        format == CFormatGuess::eTextASN ||
        format == CFormatGuess::eXml) {

        LOG_POST(Info << "Scanning file: " << file_name);

        CLDS_CoreObjectsReader sniffer;
        ESerialDataFormat stream_format = FormatGuess2Serial(format);

        auto_ptr<CObjectIStream> 
          input(CObjectIStream::Open(file_name, stream_format));
        sniffer.Probe(*input);

        CLDS_CoreObjectsReader::TObjectVector& obj_vector 
                                                = sniffer.GetObjectsVector();

        if (obj_vector.size()) {
            for (unsigned int i = 0; i < obj_vector.size(); ++i) {
                CLDS_CoreObjectsReader::SObjectDetails* obj_info = &obj_vector[i];
                // If object is not in the database yet.
                if (obj_info->ext_id == 0) {
                    SaveObject(file_id, &sniffer, obj_info);
                }
            }
            LOG_POST(Info << "LDS: " 
                          << obj_vector.size() 
                          << " object(s) found in:" 
                          << file_name);

            sniffer.ClearObjectsVector();

        } else {
            LOG_POST(Info << "LDS: No objects found in:" << file_name);
        }

    } else if ( format == CFormatGuess::eFasta ){

        int type_id;
        {{
        map<string, int>::const_iterator it = m_ObjTypeMap.find("FastaEntry");
        _ASSERT(it != m_ObjTypeMap.end());
        type_id = it->second;
        }}

        CNcbiIfstream input(file_name.c_str(), 
                            IOS_BASE::in | IOS_BASE::binary);

        CLDS_FastaScanner fscan(*this, file_id, type_id);
        ScanFastaFile(&fscan, 
                      input, 
                      CFastaReader::fAssumeNuc  |
                      CFastaReader::fAllSeqIds  |
                      CFastaReader::fOneSeq     |
                      CFastaReader::fNoSeqData  |
                      CFastaReader::fParseRawID);
    } else {
        LOG_POST(Info << "Unsupported file format: " << file_name);
    }


}


int CLDS_Object::SaveObject(int file_id,
                            const string& seq_id,
                            const string& description,
                            const string& seq_ids,
                            CNcbiStreampos pos,
                            int type_id)
{
    ++m_MaxObjRecId;
    EBDB_ErrCode err;
/*
    m_db.object_attr_db.object_attr_id = m_MaxObjRecId;
    m_db.object_attr_db.object_title = description;
    EBDB_ErrCode err = m_db.object_attr_db.Insert();
    BDB_CHECK(err, "LDS::ObjectAttribute");
*/
    m_db.object_db.object_id = m_MaxObjRecId;
    m_db.object_db.file_id = file_id;
    m_db.object_db.seqlist_id = 0;
    m_db.object_db.object_type = type_id;
    Int8 ipos = NcbiStreamposToInt8(pos);
    m_db.object_db.file_pos = ipos;
//    m_db.object_db.object_attr_id = m_MaxObjRecId;
    m_db.object_db.TSE_object_id = 0;
    m_db.object_db.parent_object_id = 0;
    m_db.object_db.object_title = description;
    m_db.object_db.seq_ids = seq_ids;

    string ups = seq_id; 
    NStr::ToUpper(ups);
    m_db.object_db.primary_seqid = ups;

    LOG_POST(Info << "Saving Fasta object: " << seq_id);

    err = m_db.object_db.Insert();
    BDB_CHECK(err, "LDS::Object");

    return m_MaxObjRecId;
}


int CLDS_Object::SaveObject(int file_id, 
                            CLDS_CoreObjectsReader* sniffer,
                            CLDS_CoreObjectsReader::SObjectDetails* obj_info)
{
    int top_level_id, parent_id;

    _ASSERT(obj_info->ext_id == 0);  // Making sure the object is not in the DB yet

    if (obj_info->is_top_level) {
        top_level_id = parent_id = 0;
    } else {
        // Find the direct parent
        {{

            CLDS_CoreObjectsReader::SObjectDetails* parent_obj_info 
                        = sniffer->FindObjectInfo(obj_info->parent_offset);
            _ASSERT(parent_obj_info);
            if (parent_obj_info->ext_id == 0) { // not yet in the database
                // Recursively save the parent
                parent_id = SaveObject(file_id, sniffer, parent_obj_info);
            } else {
                parent_id = parent_obj_info->ext_id;
            }

        }}

        // Find the top level grand parent
        {{

            CLDS_CoreObjectsReader::SObjectDetails* top_obj_info 
                        = sniffer->FindObjectInfo(obj_info->top_level_offset);
            _ASSERT(top_obj_info);
            if (top_obj_info->ext_id == 0) { // not yet in the database
                // Recursively save the parent
                top_level_id = SaveObject(file_id, sniffer, top_obj_info);
            } else {
                top_level_id = top_obj_info->ext_id;
            }

        }}

    }

    const string& type_name = obj_info->info.GetTypeInfo()->GetName();

    map<string, int>::const_iterator it = m_ObjTypeMap.find(type_name);
    if (it == m_ObjTypeMap.end()) {
        LOG_POST(Info << "Unrecognized type: " << type_name);
        return 0;                
    }
    int type_id = it->second;


    string id_str;
    string molecule_title;

    ++m_MaxObjRecId;

    bool is_object = IsObject(*obj_info, &id_str, &molecule_title);
    if (is_object) {
        string all_seq_id; // Space separated list of seq_ids
        const CBioseq* bioseq = CType<CBioseq>().Get(obj_info->info);
        if (bioseq) {
            const CBioseq::TId&  id_list = bioseq->GetId();
            ITERATE(CBioseq::TId, it, id_list) {
                const CSeq_id* seq_id = *it;
                if (seq_id) {
                    all_seq_id.append(seq_id->AsFastaString());
                    all_seq_id.append(" ");
                }
            }
        }

        m_db.object_db.primary_seqid = NStr::ToUpper(id_str);

        obj_info->ext_id = m_MaxObjRecId; // Keep external id for the next scan
        EBDB_ErrCode err;
/*
        m_db.object_attr_db.object_attr_id = m_MaxObjRecId;
        m_db.object_attr_db.object_title = molecule_title;
        m_db.object_attr_db.seq_ids = NStr::ToUpper(all_seq_id);
        EBDB_ErrCode err = m_db.object_attr_db.Insert();
        BDB_CHECK(err, "LDS::ObjectAttr");
*/
        m_db.object_db.object_id = m_MaxObjRecId;
        m_db.object_db.file_id = file_id;
        m_db.object_db.seqlist_id = 0;  // TODO:
        m_db.object_db.object_type = type_id;
        Int8 i8 = NcbiStreamposToInt8(obj_info->offset);
        m_db.object_db.file_pos = i8;
//        m_db.object_db.object_attr_id = m_MaxObjRecId; 
        m_db.object_db.TSE_object_id = top_level_id;
        m_db.object_db.parent_object_id = parent_id;
        m_db.object_db.object_title = molecule_title;
        m_db.object_db.seq_ids = NStr::ToUpper(all_seq_id);


//        LOG_POST(Info << "Saving object: " << type_name << " " << id_str);

        err = m_db.object_db.Insert();
        BDB_CHECK(err, "LDS::Object");

    } else {
                
        // Set of seq ids referenced in the annotation
        //
        set<string> ref_seq_ids;

        // Check for alignment in annotation
        //
        const CSeq_annot* annot = 
            CType<CSeq_annot>().Get(obj_info->info);
        if (annot && annot->CanGetData()) {
            const CSeq_annot_Base::C_Data& adata = annot->GetData();
            if (adata.Which() == CSeq_annot_Base::C_Data::e_Align) {
                const CSeq_annot_Base::C_Data::TAlign& al_list =
                                            adata.GetAlign();
                ITERATE (CSeq_annot_Base::C_Data::TAlign, it, al_list){
                    if (!(*it)->CanGetSegs())
                        continue;

                    const CSeq_align::TSegs& segs = (*it)->GetSegs();
                    switch (segs.Which())
                    {
                    case CSeq_align::C_Segs::e_Std:
                        {
                        const CSeq_align_Base::C_Segs::TStd& std_list =
                                                    segs.GetStd();
                        ITERATE (CSeq_align_Base::C_Segs::TStd, it2, std_list) {
                            const CRef<CStd_seg>& seg = *it2;
                            const CStd_seg::TIds& ids = seg->GetIds();

                            ITERATE(CStd_seg::TIds, it3, ids) {
                                ref_seq_ids.insert((*it3)->AsFastaString());

                            } // ITERATE

                        } // ITERATE
                        }
                        break;
                    case CSeq_align::C_Segs::e_Denseg:
                        {
                        const CSeq_align_Base::C_Segs::TDenseg& denseg =
                                                        segs.GetDenseg();
                        const CDense_seg::TIds& ids = denseg.GetIds();

                        ITERATE (CDense_seg::TIds, it3, ids) {
                            ref_seq_ids.insert((*it3)->AsFastaString());
                        } // ITERATE
                        
                        }
                        break;
                        //                    case CSeq_align::C_Segs::e_Packed:
                        //                    case CSeq_align::C_Segs::e_Disc:
                    default:
                        break;
                    }

                } // ITERATE
            }
        }

        // Save all seq ids referred by the alignment
        //
        ITERATE (set<string>, it, ref_seq_ids) {
            m_db.seq_id_list.object_id = m_MaxObjRecId;
            m_db.seq_id_list.seq_id = it->c_str();

            EBDB_ErrCode err = m_db.seq_id_list.Insert();
            BDB_CHECK(err, "LDS::seq_id_list");
        }
        
        obj_info->ext_id = m_MaxObjRecId; // Keep external id for the next scan
                
        m_db.annot_db.annot_id = m_MaxObjRecId;
        m_db.annot_db.file_id = file_id;
        m_db.annot_db.annot_type = type_id;
        Int8 i8 = NcbiStreamposToInt8(obj_info->offset);
        m_db.annot_db.file_pos = i8;
        m_db.annot_db.TSE_object_id = top_level_id;
        m_db.annot_db.parent_object_id = parent_id;
/*
        LOG_POST(Info << "Saving annotation: " 
                      << type_name 
                      << " " 
                      << id_str
                      << " " 
                      << (const char*)(!top_level_id ? "Top Level. " : " ")
                      << "offs=" 
                      << obj_info->offset
                      );
*/

        EBDB_ErrCode err = m_db.annot_db.Insert();
        BDB_CHECK(err, "LDS::Annotation");

        m_db.annot2obj_db.object_id = parent_id;
        m_db.annot2obj_db.annot_id = m_MaxObjRecId;

        err = m_db.annot2obj_db.Insert();
        BDB_CHECK(err, "LDS::Annot2Obj");

    }

    return obj_info->ext_id;
}


bool CLDS_Object::IsObject(const CLDS_CoreObjectsReader::SObjectDetails& parse_info,
                           string* object_str_id,
                           string* object_title)
{
    *object_title = "";
    *object_str_id = "";

    if (parse_info.is_top_level) {

        CSeq_entry* seq_entry = CType<CSeq_entry>().Get(parse_info.info);

        if (seq_entry) {
            m_Scope.Reset();
            m_TSE_Manager = CObjectManager::GetInstance();
            m_Scope = new CScope(*m_TSE_Manager);

            m_Scope->AddTopLevelSeqEntry(*seq_entry);
            return true;
        } else {
            CBioseq* bioseq = CType<CBioseq>().Get(parse_info.info);
            if (bioseq) {
                m_TSE_Manager = CObjectManager::GetInstance();
                m_Scope = new CScope(*m_TSE_Manager);

                m_Scope->AddBioseq(*bioseq);
            }
        }
    }

    const CBioseq* bioseq = CType<CBioseq>().Get(parse_info.info);
    if (bioseq) {
        const CSeq_id* seq_id = bioseq->GetFirstId();
        if (seq_id) {
            *object_str_id = seq_id->AsFastaString();
        } else {
            *object_str_id = "";
        }

        if (m_Scope) { // we are under OM here
            CBioseq_Handle bio_handle = m_Scope->GetBioseqHandle(*bioseq);
            if (bio_handle) {
                *object_title = sequence::GetTitle(bio_handle);
                //LOG_POST(Info << "object title: " << *object_title);
            } else {
                // the last resort
                bioseq->GetLabel(object_title, CBioseq::eBoth);
            }
            
        } else {  // non-OM controlled object
            bioseq->GetLabel(object_title, CBioseq::eBoth);
        }
    } else {
        const CSeq_annot* annot = 
            CType<CSeq_annot>().Get(parse_info.info);
        if (annot) {
            *object_str_id = "";
            return false;
        } else {
            const CSeq_align* align =  
                CType<CSeq_align>().Get(parse_info.info);
            if (align) {
                *object_str_id = "";
                return false;
            }
        }
    }
    return true;
}


int CLDS_Object::FindMaxObjRecId()
{
    if (m_MaxObjRecId) {
        return m_MaxObjRecId;
    }

    LDS_GETMAXID(m_MaxObjRecId, m_db.object_db, object_id);

    int ann_rec_id = 0;
    LDS_GETMAXID(ann_rec_id, m_db.annot_db, annot_id);

    if (ann_rec_id > m_MaxObjRecId) {
        m_MaxObjRecId = ann_rec_id;
    }

    return m_MaxObjRecId;
}


void LDS_GetSequenceBase(const CSeq_id&   seq_id, 
                         SLDS_SeqIdBase*  seqid_base)
{
    _ASSERT(seqid_base);

    const CObject_id* obj_id_ptr = 0;
    int   obj_id_int = 0;
    const CTextseq_id* obj_id_txt = 0;
    const CGiimport_id* obj_id_gii = 0;
    const CPatent_seq_id* obj_id_patent = 0;
    const CDbtag*         obj_id_dbtag = 0;
    const CPDB_seq_id*    obj_id_pdb = 0;

    switch (seq_id.Which()) {
    case CSeq_id::e_Local:
        obj_id_ptr = &seq_id.GetLocal();
        break;
    case CSeq_id::e_Gibbsq:
        obj_id_int = seq_id.GetGibbsq();
        break;
    case CSeq_id::e_Gibbmt:
        obj_id_int = seq_id.GetGibbmt();
        break;
    case CSeq_id::e_Giim:
        obj_id_gii = &seq_id.GetGiim();
        break;
    case CSeq_id::e_Genbank:
        obj_id_txt = &seq_id.GetGenbank();
        break;
    case CSeq_id::e_Embl:
        obj_id_txt = &seq_id.GetEmbl();
        break;
    case CSeq_id::e_Pir:
        obj_id_txt = &seq_id.GetPir();
        break;
    case CSeq_id::e_Swissprot:
        obj_id_txt = &seq_id.GetSwissprot();
        break;
    case CSeq_id::e_Patent:
        obj_id_patent = &seq_id.GetPatent();
        break;
    case CSeq_id::e_Other:
        obj_id_txt = &seq_id.GetOther();
        break;
    case CSeq_id::e_General:
        obj_id_dbtag = &seq_id.GetGeneral();
        break;
    case CSeq_id::e_Gi:
        obj_id_int = seq_id.GetGi();
        break;
    case CSeq_id::e_Ddbj:
        obj_id_txt = &seq_id.GetDdbj();
        break;
    case CSeq_id::e_Prf:
        obj_id_txt = &seq_id.GetPrf();
        break;
    case CSeq_id::e_Pdb:
        obj_id_pdb = &seq_id.GetPdb();
        break;
    case CSeq_id::e_Tpg:
        obj_id_txt = &seq_id.GetTpg();
        break;
    case CSeq_id::e_Tpe:
        obj_id_txt = &seq_id.GetTpe();
        break;
    case CSeq_id::e_Tpd:
        obj_id_txt = &seq_id.GetTpd();
        break;
    case CSeq_id::e_Gpipe:
        obj_id_txt = &seq_id.GetGpipe();
        break;
    default:
        _ASSERT(0);
        break;
    }

    const string* id_str = 0;

    if (obj_id_ptr) {
        switch (obj_id_ptr->Which()) {
        case CObject_id::e_Id:
            obj_id_int = obj_id_ptr->GetId();
            break;
        case CObject_id::e_Str:
            id_str = &obj_id_ptr->GetStr();
            break;
        case CObject_id::e_not_set:
            break;
        default:
            break;
        }
    }

    if (obj_id_int) {
        seqid_base->int_id = obj_id_int;
        seqid_base->str_id.erase();
        return;
    }

    if (obj_id_txt) {
        if (obj_id_txt->CanGetAccession()) {
            const CTextseq_id::TAccession& acc = 
                                obj_id_txt->GetAccession();
            id_str = &acc;
        } else {
            if (obj_id_txt->CanGetName()) {
                const CTextseq_id::TName& name =
                    obj_id_txt->GetName();
                id_str = &name;
            }
        }
    }
    if (id_str) {
        seqid_base->int_id = 0;
        seqid_base->str_id = *id_str;
        return;
    }


    LOG_POST(Warning 
             << "SeqId indexer: unsupported type " 
             << seq_id.AsFastaString());

    seqid_base->Init();

}

bool LDS_GetSequenceBase(const string&   seq_id_str, 
                         SLDS_SeqIdBase* seqid_base,
                         CSeq_id*        conv_seq_id)
{
    _ASSERT(seqid_base);

    CRef<CSeq_id> tmp_seq_id;

    if (conv_seq_id == 0) {
        tmp_seq_id.Reset((conv_seq_id = new CSeq_id));

    }

    bool can_convert = true;

    try {
        conv_seq_id->Set(seq_id_str);
    } catch (CSeqIdException&) {
        try {
            conv_seq_id->Set(CSeq_id::e_Local, 
                                seq_id_str);
        } catch (CSeqIdException&) {
            can_convert = false;
            LOG_POST(Error 
                << "Cannot convert seq id string: "
                << seq_id_str);
            seqid_base->Init();
        }
    }

    if (can_convert) {
        LDS_GetSequenceBase(*conv_seq_id, seqid_base);
    }

    return can_convert;
}


/// Scanner functor to build id index
///
/// @internal
///
class CLDS_BuildIdIdx
{
public:
    CLDS_BuildIdIdx(CLDS_Database& db, bool control_dups)
    : m_db(db),
        m_coll(db.GetTables()),
        m_SeqId(new CSeq_id),
        m_ControlDups(control_dups),
        m_Query(new CLDS_Query(db)),
        m_ObjIds(bm::BM_GAP)
    {
        if (m_ControlDups) {
            m_SequenceFind.reset(new CLDS_Query::CSequenceFinder(*m_Query));
        }
    }

    void operator()(SLDS_ObjectDB& dbf)
    {
        int object_id = dbf.object_id; // PK

        if (!dbf.primary_seqid.IsNull()) {
            dbf.primary_seqid.ToString(m_PriSeqId_Str);

            x_AddToIdx(m_PriSeqId_Str, object_id);

            dbf.seq_ids.ToString(m_SeqId_Str);
            vector<string> seq_id_arr;
            NStr::Tokenize(m_SeqId_Str, " ", seq_id_arr, NStr::eMergeDelims);
            ITERATE (vector<string>, it, seq_id_arr) {
                const string& seq_id_str = *it;
                if (NStr::CompareNocase(seq_id_str,m_PriSeqId_Str)==0) {
                    continue;
                }
                x_AddToIdx(seq_id_str, object_id);
            }
        }
    }

private:
    void x_AddToIdx(const string& seq_id_str, int rec_id)
    {
        bool can_convert = 
            LDS_GetSequenceBase(seq_id_str, &m_SBase, &*m_SeqId);
        if (can_convert) {
            if (m_ControlDups) {
                _ASSERT(m_SequenceFind.get());
                CLDS_Set& cand = m_SequenceFind->GetCandidates();
                cand.clear();
                m_SequenceFind->Screen(m_SBase);
                if (cand.any()) {
                    CLDS_Set dup_ids(bm::BM_GAP);
                    m_SequenceFind->FindInCandidates(seq_id_str, &dup_ids);

                    if (dup_ids.any()) {
                        unsigned id = dup_ids.get_first();
                        m_Query->ReportDuplicateObjectSeqId(seq_id_str,
                                                            id, 
                                                            rec_id);
                    }
                }
            }

            x_AddToIdx(m_SBase, rec_id);
        }
    }

    void x_AddToIdx(const SLDS_SeqIdBase& sbase, int rec_id)
    {
        if (sbase.int_id) {
            m_coll.obj_seqid_int_idx.id = sbase.int_id;
            m_coll.obj_seqid_int_idx.row_id = rec_id;
            m_coll.obj_seqid_int_idx.Insert();
        } 
        else if (!sbase.str_id.empty()) {
            m_coll.obj_seqid_txt_idx.id = sbase.str_id;
            m_coll.obj_seqid_txt_idx.row_id = rec_id;
            m_coll.obj_seqid_txt_idx.Insert();
        }
    }

private:
    CLDS_BuildIdIdx(const CLDS_BuildIdIdx&);
    CLDS_BuildIdIdx& operator=(const CLDS_BuildIdIdx&);

private:
    CLDS_Database&         m_db;
    SLDS_TablesCollection& m_coll;
    string                 m_PriSeqId_Str;
    string                 m_SeqId_Str;
    CRef<CSeq_id>          m_SeqId;
    SLDS_SeqIdBase         m_SBase;
    bool                   m_ControlDups; ///< Control id duplicates
    auto_ptr<CLDS_Query>                  m_Query;
    auto_ptr<CLDS_Query::CSequenceFinder> m_SequenceFind;
    CLDS_Set               m_ObjIds;  ///< id set for duplicate search
};

void CLDS_Object::BuildSeqIdIdx()
{
    m_db.obj_seqid_int_idx.Truncate();
    m_db.obj_seqid_txt_idx.Truncate();

    LOG_POST(Info << "Building sequence id index on objects...");

    CLDS_BuildIdIdx func(m_DataBase, m_ControlDupIds);
    BDB_iterate_file(m_db.object_db, func);
}


END_SCOPE(objects)
END_NCBI_SCOPE
