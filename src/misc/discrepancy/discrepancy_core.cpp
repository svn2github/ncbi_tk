/*  $Id$
 * =========================================================================
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
 * =========================================================================
 *
 * Authors: Sema Kachalo
 *
 */

#include <ncbi_pch.hpp>
#include "discrepancy_core.hpp"
#include "utils.hpp"
#include <sstream>
#include <objmgr/object_manager.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/util/sequence.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

CSafeStatic<map<string, CDiscrepancyConstructor*> > CDiscrepancyConstructor::sm_Table;
CSafeStatic<map<string, string> > CDiscrepancyConstructor::sm_DescrTable;
CSafeStatic<map<string, TGroup> > CDiscrepancyConstructor::sm_GroupTable;
CSafeStatic<map<string, string> > CDiscrepancyConstructor::sm_AliasTable;
CSafeStatic<map<string, vector<string> > > CDiscrepancyConstructor::sm_AliasListTable;


string CDiscrepancyConstructor::GetDiscrepancyCaseName(const string& name)
{
    map<string, CDiscrepancyConstructor*>& Table = GetTable();
    map<string, string>& AliasTable = GetAliasTable();
    if (Table.find(name) != Table.end()) {
        return name;
    }
    if (AliasTable.find(name) != AliasTable.end()) {
        return AliasTable[name];
    }
    if (name.substr(0, 5) == "DISC_") {
        return GetDiscrepancyCaseName(name.substr(5));
    }
    return "";
}


const CDiscrepancyConstructor* CDiscrepancyConstructor::GetDiscrepancyConstructor(const string& name)
{
    string str = GetDiscrepancyCaseName(name);
    return str.empty() ? 0 : GetTable()[str];
}


string GetDiscrepancyCaseName(const string& name)
{
    return CDiscrepancyConstructor::GetDiscrepancyCaseName(name);
}


string GetDiscrepancyDescr(const string& name)
{
    string str = GetDiscrepancyCaseName(name);
    return str.empty() ? "" : CDiscrepancyConstructor::GetDescrTable()[str];
}


TGroup GetDiscrepancyGroup(const string& name)
{
    string str = GetDiscrepancyCaseName(name);
    return str.empty() ? 0 : CDiscrepancyConstructor::GetGroupTable()[str];
}


vector<string> GetDiscrepancyNames(TGroup group)
{
    typedef map<string, CDiscrepancyConstructor*> MyMap;
    map<string, CDiscrepancyConstructor*>& Table = CDiscrepancyConstructor::GetTable();
    map<string, TGroup>& Group = CDiscrepancyConstructor::GetGroupTable();
    vector<string> V;
    ITERATE (MyMap, J, Table) {
        if ((Group[J->first] & group) == group) {
            V.push_back(J->first);
        }
    }
    return V;
}


vector<string> GetDiscrepancyAliases(const string& name)
{
    map<string, vector<string> >& AliasListTable = CDiscrepancyConstructor::GetAliasListTable();
    return AliasListTable.find(name)!=AliasListTable.end() ? AliasListTable[name] : vector<string>();
}


CReportNode& CReportNode::operator[](const string& name)
{
    if (m_Map.find(name) == m_Map.end()) {
        m_Map[name] = CRef<CReportNode>(new CReportNode(name));
    }
    return *m_Map[name];
}


void CReportNode::Add(TReportObjectList& list, TReportObjectSet& hash, CReportObj& obj, bool unique)
{
    if (unique && hash.find(&obj) != hash.end()) {
        return;
    }
    list.push_back(CRef<CReportObj>(&obj));
    hash.insert(&obj);
}


void CReportNode::Add(TReportObjectList& list, TReportObjectSet& hash, TReportObjectList& objs, bool unique)
{
    NON_CONST_ITERATE (TReportObjectList, it, objs) {
        Add(list, hash, **it, unique);
    }
}


void CReportNode::Copy(CRef<CReportNode> other)
{
    m_Map = other->m_Map;
    m_Objs = other->m_Objs;
    m_Hash = other->m_Hash;
    m_Fatal = other->m_Fatal;
    m_Autofix = other->m_Autofix;
    m_Fatal = other->m_Fatal;
    m_Hash = other->m_Hash;
}


bool CReportNode::Promote()
{
    if (m_Map.size() == 1) {
        Copy(m_Map.begin()->second);
        return true;
    }
    return false;
}


CRef<CReportItem> CReportNode::Export(CDiscrepancyCase& test, bool unique)
{
    TReportObjectList objs = m_Objs;
    TReportObjectSet hash = m_Hash;
    TReportItemList subs;
    bool autofix = false;
    bool fatal = m_Fatal;
    NON_CONST_ITERATE(TNodeMap, it, m_Map) {
        fatal |= it->second->m_Fatal;
        CRef<CReportItem> sub = it->second->Export(test, unique);
        autofix |= sub->CanAutofix();
        subs.push_back(sub);
        if (!m_NoRec) {
            TReportObjectList details = sub->GetDetails();
            NON_CONST_ITERATE (TReportObjectList, ob, details) {
                Add(objs, hash, **ob, unique);
            }
        }
    }
    if (!autofix) {
        NON_CONST_ITERATE (TReportObjectList, ob, objs) {
            if ((*ob)->CanAutofix()) {
                autofix = true;
                break;
            }
        }
    }
	CRef<CDiscrepancyItem> item(new CDiscrepancyItem(test, CDiscrepancySet::Format(m_Name, m_Count ? m_Count : objs.size()), m_Name));
    item->m_Autofix = autofix;
    item->m_Fatal = fatal;
    item->m_Ext = m_Ext;
    item->m_Summ = m_Summ;
    item->m_Subs = subs;
    item->m_Objs = objs;
    return CRef<CReportItem>((CReportItem*)item);
}


CRef<CReportItem> CReportItem::CreateReportItem(const string& test, const string& msg, bool autofix)
{
    CRef<CDiscrepancyCase> t = CDiscrepancyConstructor::GetDiscrepancyConstructor(test)->Create();
	CRef<CDiscrepancyItem> item(new CDiscrepancyItem(*t, msg, msg));
    item->m_Autofix = autofix;
    return CRef<CReportItem>((CReportItem*)item);
}


void CDiscrepancyItem::PushReportObj(CReportObj& obj)
{
    m_Objs.push_back(CRef<CReportObj>(&obj));
}


CReportObj* CDiscrepancyObject::Clone(bool autofixable, CConstRef<CObject> data) const
{
    CDiscrepancyObject* obj = new CDiscrepancyObject(*this);
    obj->m_Autofix = autofixable;
    obj->m_More = data;
    return obj;
}


template<typename T> void CDiscrepancyVisitor<T>::Call(const T& obj, CDiscrepancyContext& context)
{
    try {
        Visit(obj, context);
    }
    catch (CException& e) {
        string ss = "EXCEPTION caught: "; ss += e.what();
        m_Objs[ss].Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary()));
    }
}


CRef<CDiscrepancySet> CDiscrepancySet::New(CScope& scope){ return CRef<CDiscrepancySet>(new CDiscrepancyContext(scope));}


string CDiscrepancySet::Format(const string& s, unsigned int count)
{
    string str = s;
    NStr::TruncateSpacesInPlace(str);
    NStr::ReplaceInPlace(str, "[n]", NStr::Int8ToString(count));
    NStr::ReplaceInPlace(str, "[n/2]", NStr::Int8ToString(count / 2));
    NStr::ReplaceInPlace(str, "[s]", count == 1 ? "" : "s");  // nouns
    NStr::ReplaceInPlace(str, "[S]", count == 1 ? "s" : "");  // verbs
    NStr::ReplaceInPlace(str, "[is]", count == 1 ? "is" : "are");
    NStr::ReplaceInPlace(str, "[does]", count == 1 ? "does" : "do");
    NStr::ReplaceInPlace(str, "[has]", count == 1 ? "has" : "have");
    for (size_t n = NStr::Find(str, "[*"); n != NPOS; n = NStr::Find(str, "[*")) {
        size_t k = NStr::Find(str, "*]");
        if (k != NPOS) {
            str.erase(n, k - n + 2);
        }
        else {
            str.erase(n);
        }
    }
    return str;
}


bool CDiscrepancyContext::AddTest(const string& name)
{
    string str = GetDiscrepancyCaseName(name);
    if (str.empty()) {
        return false; // no such test
    }
    if (m_Tests.find(str) != m_Tests.end()) {
        return false;  // already there
    }
    CRef<CDiscrepancyCase> test = CDiscrepancyConstructor::GetDiscrepancyConstructor(str)->Create();
    m_Tests[str] = test;

#define REGISTER_DISCREPANCY_TYPE(type) \
    if (dynamic_cast<CDiscrepancyVisitor<type>* >(test.GetPointer())) {                         \
        m_All_##type.push_back(dynamic_cast<CDiscrepancyVisitor<type>* >(test.GetPointer()));   \
        m_Enable_##type = true;                                                                 \
        return true;                                                                            \
    }
    REGISTER_DISCREPANCY_TYPE(CSeq_inst)
    REGISTER_DISCREPANCY_TYPE(CSeqdesc)
    REGISTER_DISCREPANCY_TYPE(CSeq_feat)
    REGISTER_DISCREPANCY_TYPE(CSubmit_block)
    REGISTER_DISCREPANCY_TYPE(CSeqFeatData)
    REGISTER_DISCREPANCY_TYPE(CSeq_feat_BY_BIOSEQ)
    REGISTER_DISCREPANCY_TYPE(CSeqdesc_BY_BIOSEQ)
    REGISTER_DISCREPANCY_TYPE(COverlappingFeatures)
    REGISTER_DISCREPANCY_TYPE(CBioSource)
    REGISTER_DISCREPANCY_TYPE(COrgName)
    REGISTER_DISCREPANCY_TYPE(CRNA_ref)
    REGISTER_DISCREPANCY_TYPE(CSeq_annot)
    REGISTER_DISCREPANCY_TYPE(CPubdesc)
    REGISTER_DISCREPANCY_TYPE(CAuth_list)
    REGISTER_DISCREPANCY_TYPE(CPerson_id)
    REGISTER_DISCREPANCY_TYPE(CBioseq_set)
    return false;
}


bool CDiscrepancyContext::SetAutofixHook(const string& name, TAutofixHook func)
{
    string str = GetDiscrepancyCaseName(name);
    if (str.empty()) {
        return false; // no such test
    }
    if (m_Tests.find(str) == m_Tests.end()) {
        return false;
    }
    return ((CDiscrepancyCore*)m_Tests[str].GetPointer())->SetHook(func);
}


void CDiscrepancyContext::Update_Bioseq_set_Stack(CTypesConstIterator& it)
{
    size_t n = 0;
    CTypesConstIterator::TIteratorContext ctx = it.GetContextData();
    ITERATE (CTypesConstIterator::TIteratorContext, p, ctx) {
        if (p->first.GetName() == "Bioseq-set") {
            n++;
        }
    }
    if (CType<CBioseq_set>::Match(it)) {
        m_Bioseq_set_Stack.resize(n - 1);
        m_Bioseq_set_Stack.push_back(m_Scope->GetBioseq_setHandle(*CType<CBioseq_set>::Get(it)).GetCompleteBioseq_set());
    }
    else {
        m_Bioseq_set_Stack.resize(n);
    }

}


void CDiscrepancyContext::Parse(const CSerialObject& root)
{
    CTypesConstIterator i;
    CType<CBioseq>::AddTo(i);
    CType<CBioseq_set>::AddTo(i);
    CType<CSubmit_block>::AddTo(i);
    CType<CSeqdesc>::AddTo(i);
    CType<CSeq_feat>::AddTo(i);
    CType<CSeq_annot>::AddTo(i);
    CType<CPub>::AddTo(i);
    CType<CPub_equiv>::AddTo(i);
#define ENABLE_DISCREPANCY_TYPE(type) if (m_Enable_##type) CType<type>::AddTo(i);
    ENABLE_DISCREPANCY_TYPE(CSeq_inst)
    // ENABLE_DISCREPANCY_TYPE(CSeqdesc) - don't need!
    // ENABLE_DISCREPANCY_TYPE(CSeq_feat) - don't need!
    ENABLE_DISCREPANCY_TYPE(CSeqFeatData)
    // Don't ENABLE_DISCREPANCY_TYPE(CSeq_feat_BY_BIOSEQ), it is handled separately!
    // Don't ENABLE_DISCREPANCY_TYPE(CSeqdesc_BY_BIOSEQ), it is handled separately!
    // Don't ENABLE_DISCREPANCY_TYPE(COverlappingFeatures), it is handled separately!
    ENABLE_DISCREPANCY_TYPE(CBioSource)
    ENABLE_DISCREPANCY_TYPE(COrgName)
    ENABLE_DISCREPANCY_TYPE(CRNA_ref)
    ENABLE_DISCREPANCY_TYPE(CPubdesc)
    ENABLE_DISCREPANCY_TYPE(CAuth_list)
    ENABLE_DISCREPANCY_TYPE(CPerson_id)
    // Don't ENABLE_DISCREPANCY_TYPE(CSeq_annot), it is handled separately!
    // Don't ENABLE_DISCREPANCY_TYPE(CBioseq_set), it is handled separately!

    m_TextMap.clear();
    m_TextMapShort.clear();
    m_IsPseudoMap.clear();
    m_GeneForFeatureMap.clear();
    m_ProdForFeatureMap.clear();
    m_Current_Submit_block.Reset();
    m_Current_Submit_block_StringObj.Reset();
    m_Current_Cit_sub_StringObj.Reset();

    CSeqdesc_CI::TDescChoices desc_choices = {CSeqdesc::e_Source};

    for (i = CConstBeginInfo(root); i; ++i) {
        CTypesConstIterator::TIteratorContext ctx = i.GetContextData();
        if (CType<CBioseq>::Match(i)) {
            m_Current_Bioseq_Handle = m_Scope->GetBioseqHandle(*CType<CBioseq>::Get(i));
            m_Current_Bioseq.Reset(m_Current_Bioseq_Handle.GetCompleteBioseq());
            m_Count_Bioseq++;
            Update_Bioseq_set_Stack(i);
            if (m_Current_Bioseq->GetInst().IsNa()) {
                m_NaSeqs.push_back(CRef<CReportObj>(NewBioseqObj(m_Current_Bioseq, &GetSeqSummary())));
            }
            if (m_Current_Submit_block_StringObj && m_Current_Submit_block_StringObj->Value.empty()) {
                string label;
                string citsub;
                m_Current_Bioseq->GetLabel(&label, CBioseq::eContent);
                citsub = "Cit-sub for Set containing " + label;
                label = "Set containing " + label;
                if (!m_File.empty()) {
                    label = m_File + ":" + label;
                    citsub = m_File + ":" + citsub;
                }
                m_Current_Submit_block_StringObj->Value = label;
                m_Current_Cit_sub_StringObj->Value = citsub;
                m_Current_Submit_block_StringObj.Reset();
                m_Current_Cit_sub_StringObj.Reset();
            }
            // CSeq_feat_BY_BIOSEQ cycle
            CFeat_CI feat_ci(m_Current_Bioseq_Handle);
            m_Feat_CI = !!feat_ci;
            ClearFeatureList();
            if (m_Enable_CSeq_feat_BY_BIOSEQ || m_Enable_COverlappingFeatures) {
                m_Current_Seqdesc.Reset();
                for (; feat_ci; ++feat_ci) {
                    const CSeq_feat *feat = feat_ci->GetSeq_feat();
                    CollectFeature(*feat);
                    m_Current_Seq_feat.Reset(feat);
                    m_Count_Seq_feat++;
                    NON_CONST_ITERATE(vector<CDiscrepancyVisitor<CSeq_feat_BY_BIOSEQ>* >, it, m_All_CSeq_feat_BY_BIOSEQ) {
                        Call(**it, (const CSeq_feat_BY_BIOSEQ&)*feat);
                    }
                }
            }
            CSeqdesc_CI desc_ci(m_Current_Bioseq_Handle, desc_choices);
            if (m_Enable_CSeqdesc_BY_BIOSEQ) {
                for (; desc_ci; ++desc_ci) {
                    const CSeqdesc& desc = *desc_ci;
                    NON_CONST_ITERATE(vector<CDiscrepancyVisitor<CSeqdesc_BY_BIOSEQ>* >, it, m_All_CSeqdesc_BY_BIOSEQ) {
                        Call(**it, (const CSeqdesc_BY_BIOSEQ&)desc);
                    }
                }
            }
            // COverlappingFeatures
            if (m_Enable_COverlappingFeatures) {
                const COverlappingFeatures& obj = (const COverlappingFeatures&)*m_Current_Bioseq;
                NON_CONST_ITERATE(vector<CDiscrepancyVisitor<COverlappingFeatures>* >, it, m_All_COverlappingFeatures) {
                    Call(**it, obj);
                }
            }
        }
        else if (CType<CBioseq_set>::Match(i)) {
            m_Current_Bioseq.Reset();
            m_Count_Bioseq++;
            Update_Bioseq_set_Stack(i);
            if (m_Enable_CBioseq_set) {
                const CBioseq_set & obj = *CType<CBioseq_set>::Get(i);
                NON_CONST_ITERATE (vector<CDiscrepancyVisitor<CBioseq_set>* >, it, m_All_CBioseq_set) {
                    Call(**it, obj);
                }
            }
        }
        else if (CType<CSeq_annot>::Match(i)) {
            // Seq-annots are special because they are the only part of a
            // Bioseq-set that could appear _after_ its descendents are
            // traversed.
            const CSeq_annot & obj = *CType<CSeq_annot>::Get(i);
            const CSeq_annot_Handle annot_h = m_Scope->GetSeq_annotHandle(obj);
            if (!annot_h.GetParentEntry().IsSeq()) {
                m_Current_Bioseq.Reset();
                m_Count_Bioseq++;
                // if this annot is not on a Bioseq, then we need to check
                // that the bioseq-set stack is correct in case
                // we've left any bioseq-sets
                Update_Bioseq_set_Stack(i);
            }
            if (m_Enable_CSeq_annot) {
                NON_CONST_ITERATE (vector<CDiscrepancyVisitor<CSeq_annot>* >, it, m_All_CSeq_annot) {
                    Call(**it, obj);
                }
            }
        }
        else if (CType<CSubmit_block>::Match(i)) {
            m_Current_Submit_block.Reset(CType<CSubmit_block>::Get(i));
            m_Current_Submit_block_StringObj.Reset(new CSimpleTypeObject<string>);
            m_Current_Cit_sub_StringObj.Reset(new CSimpleTypeObject<string>);
            m_Current_Pub.Reset();
            m_Current_Pub_equiv.Reset();
            const CSubmit_block & obj = *CType<CSubmit_block>::Get(i);
            NON_CONST_ITERATE (vector<CDiscrepancyVisitor<CSubmit_block>* >, it, m_All_CSubmit_block) {
                Call(**it, obj);
            }
        }
        else if (CType<CPub>::Match(i)) {
            m_Current_Pub.Reset(CType<CPub>::Get(i));
            m_Count_Pub++;
        }
        else if (CType<CPub_equiv>::Match(i)) {
            m_Current_Pub_equiv.Reset(CType<CPub_equiv>::Get(i));
            m_Count_Pub_equiv++;
        }
        else if (CType<CSeqdesc>::Match(i)) {
            m_Current_Seq_feat.Reset();
            m_Current_Seqdesc.Reset(CType<CSeqdesc>::Get(i));
            m_Count_Seqdesc++;
            if (m_Enable_CSeqdesc) {
                const CSeqdesc& obj = *CType<CSeqdesc>::Get(i);
                NON_CONST_ITERATE (vector<CDiscrepancyVisitor<CSeqdesc>* >, it, m_All_CSeqdesc) {
                    Call(**it, obj);
                }
            }
        }
        else if (CType<CSeq_feat>::Match(i)) {
            //m_Current_Seq_feat.Reset(m_Scope->GetSeq_featHandle(*CType<CSeq_feat>::Get(i)).GetSeq_feat());
            m_Current_Seq_feat.Reset(CType<CSeq_feat>::Get(i));
            m_Current_Seqdesc.Reset();
            m_Count_Seq_feat++;
            if (m_Enable_CSeq_feat) {
                const CSeq_feat& obj = *CType<CSeq_feat>::Get(i);
                NON_CONST_ITERATE (vector<CDiscrepancyVisitor<CSeq_feat>* >, it, m_All_CSeq_feat) {
                    Call(**it, obj);
                }
            }
        }
#define HANDLE_DISCREPANCY_TYPE(type) \
        else if (m_Enable_##type && CType<type>::Match(i)) {                                    \
            const type& obj = *CType<type>::Get(i);                                             \
            NON_CONST_ITERATE (vector<CDiscrepancyVisitor<type>* >, it, m_All_##type) {         \
                Call(**it, obj);                                                                \
            }                                                                                   \
        }
        HANDLE_DISCREPANCY_TYPE(CSeq_inst)  // no semicolon!
        HANDLE_DISCREPANCY_TYPE(CSeqFeatData)
        HANDLE_DISCREPANCY_TYPE(CBioSource)
        HANDLE_DISCREPANCY_TYPE(COrgName)
        HANDLE_DISCREPANCY_TYPE(CRNA_ref)
        HANDLE_DISCREPANCY_TYPE(CPubdesc)
        HANDLE_DISCREPANCY_TYPE(CAuth_list)
        HANDLE_DISCREPANCY_TYPE(CPerson_id)
    }
}


void CDiscrepancyContext::Summarize()
{
    NON_CONST_ITERATE (TDiscrepancyCaseMap, it, m_Tests) {
        static_cast<CDiscrepancyCore&>(*it->second).Summarize(*this);
    }
}


void CDiscrepancyContext::AutofixAll()
{
    NON_CONST_ITERATE (TDiscrepancyCaseMap, it, m_Tests) {
        const TReportItemList& list = it->second->GetReport();
        ITERATE (TReportItemList, it, list) {
            (*it)->Autofix(*m_Scope);
        }
    }
}


TReportItemList CDiscrepancyGroup::Collect(TDiscrepancyCaseMap& tests, bool all) const
{
    TReportItemList out;
    ITERATE (vector<CRef<CDiscrepancyGroup> >, it, m_List) {
        TReportItemList tmp = (*it)->Collect(tests, false);
        ITERATE(TReportItemList, tt, tmp) {
            out.push_back(*tt);
        }
    }
    if (!m_Test.empty() && tests.find(m_Test) != tests.end()) {
        TReportItemList tmp = tests[m_Test]->GetReport();
        ITERATE(TReportItemList, tt, tmp) {
            out.push_back(*tt);
        }
        tests.erase(m_Test);
    }
    if (!m_Name.empty()) {
        TReportObjectList objs;
        TReportObjectSet hash;
        CRef<CDiscrepancyItem> di(new CDiscrepancyItem(m_Name));
        di->m_Subs = out;
        ITERATE (TReportItemList, tt, out) {
            TReportObjectList details = (*tt)->GetDetails();
            NON_CONST_ITERATE(TReportObjectList, ob, details) {
                CReportNode::Add(objs, hash, **ob);
            }
            if ((*tt)->CanAutofix()) {
                di->m_Autofix = true;
            }
            if ((*tt)->IsFatal()) {
                di->m_Fatal = true;
            }
        }
        di->m_Objs = objs;
        out.clear();
        if (di->m_Objs.size()) {
            out.push_back(CRef<CReportItem>(di));
        }
    }
    if (all) {
        ITERATE(TDiscrepancyCaseMap, it, tests) {
            TReportItemList list = it->second->GetReport();
            ITERATE(TReportItemList, it, list) {
                out.push_back(*it);
            }
        }
    }
    return out;
}


DISCREPANCY_LINK_MODULE(discrepancy_case);
DISCREPANCY_LINK_MODULE(suspect_product_names);
DISCREPANCY_LINK_MODULE(division_code_conflicts);
DISCREPANCY_LINK_MODULE(feature_per_bioseq);
DISCREPANCY_LINK_MODULE(seqdesc_per_bioseq);
DISCREPANCY_LINK_MODULE(gene_names);
DISCREPANCY_LINK_MODULE(rna_names);
DISCREPANCY_LINK_MODULE(spell_check);
DISCREPANCY_LINK_MODULE(transl_too_long);
DISCREPANCY_LINK_MODULE(overlapping_features);
DISCREPANCY_LINK_MODULE(sesame_street);
DISCREPANCY_LINK_MODULE(transl_note);
DISCREPANCY_LINK_MODULE(feature_tests);
DISCREPANCY_LINK_MODULE(sequence_tests);
DISCREPANCY_LINK_MODULE(pub_tests);
DISCREPANCY_LINK_MODULE(biosource_tests);

END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
