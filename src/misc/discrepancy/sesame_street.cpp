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
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/PCRPrimer.hpp>
#include <objects/seqfeat/PCRPrimerSet.hpp>
#include <objects/seqfeat/PCRReaction.hpp>
#include <objects/seqfeat/PCRReactionSet.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(sesame_street);

// Some animals are more equal than others...


static string OrderQual(const string& s)
{
    static string names[] = {
        "collected-by",
        "collection-date",
        "country",
        "identified-by",
        "fwd-primer-name",
        "fwd-primer-seq",
        "isolate",
        "isolation-source",
        "host",
        "rev-primer-name",
        "rev-primer-seq",
        "culture-collection",
        "plasmid-name",
        "isolation-source"
        "strain",
        "note-subsrc",
        "note-orgmod",
        "specimen-voucher",
        "taxname",
        "tissue-type",
        "taxid",
        "altitude",
        "location"
    };
    const size_t sz = sizeof(names) / sizeof(names[0]);
    size_t n;
    for (n = 0; n < sz; n++) {
        if (names[n] == s) {
            break;
        }
    }
    if (n == sz) {
        return s;
    }
    string r;
    for (size_t i = sz; i > n; i--) {
        r += " ";
    }
    return r + s;
}

static void AddObjToQualMap(const string& qual, const string& val, CReportObj& obj, CReportNode& node)
{
    if (node["all"][qual].Exist(obj)) {
        node["all"][qual]["*"].Add(obj, false); // duplicated
    }
    else {
        node["all"][qual].Add(obj, false);
    }
    node[qual][val].Add(obj);
}

DISCREPANCY_CASE(SOURCE_QUALS, CBioSource, eDisc | eOncaller | eSubmitter | eSmart, "Some animals are more equal than others...")
{
    CConstRef<CSeqdesc> desc = context.GetCurrentSeqdesc();
    if (desc.IsNull()) {
        return;
    }
    CRef<CDiscrepancyObject> disc_obj(context.NewSeqdescObj(desc, context.GetCurrentBioseqLabel()));
    m_Objs["all"].Add(*disc_obj);
    if (obj.CanGetGenome() && obj.GetGenome() != CBioSource::eGenome_unknown) {

        AddObjToQualMap("location", context.GetGenomeName(obj.GetGenome()), *disc_obj, m_Objs);
    }
    if (obj.CanGetOrg()) {
        const COrg_ref& org_ref = obj.GetOrg();
        if (org_ref.CanGetTaxname()) {

            AddObjToQualMap("taxname", org_ref.GetTaxname(), *disc_obj, m_Objs);
        }
        if (org_ref.GetTaxId()) {

            AddObjToQualMap("taxid", NStr::IntToString(org_ref.GetTaxId()), *disc_obj, m_Objs);
        }
    }
    if (obj.CanGetSubtype()) {
        ITERATE (CBioSource::TSubtype, it, obj.GetSubtype()) {
            const CSubSource::TSubtype& subtype = (*it)->GetSubtype();
            if ((*it)->CanGetName()) {

                const string& qual = subtype == CSubSource::eSubtype_other ? "note-subsrc" : (*it)->GetSubtypeName(subtype, CSubSource::eVocabulary_raw);
                AddObjToQualMap(qual, (*it)->GetName(), *disc_obj, m_Objs);
            }
        }
    }
    if (obj.IsSetOrgMod()) {
        ITERATE (list<CRef<COrgMod> >, it, obj.GetOrgname().GetMod()) {
            const COrgMod::TSubtype& subtype = (*it)->GetSubtype();
            if (subtype != COrgMod::eSubtype_old_name &&
                subtype != COrgMod::eSubtype_old_lineage &&
                subtype != COrgMod::eSubtype_gb_acronym &&
                subtype != COrgMod::eSubtype_gb_anamorph &&
                subtype != COrgMod::eSubtype_gb_synonym) {

                const string& qual = subtype == COrgMod::eSubtype_other ? "note-orgmod" : subtype == COrgMod::eSubtype_nat_host ? "host" : (*it)->GetSubtypeName(subtype, COrgMod::eVocabulary_raw);
                AddObjToQualMap(qual, (*it)->GetSubname(), *disc_obj, m_Objs);
            }
        }
    }
    if (obj.CanGetPcr_primers()) {
        ITERATE (list<CRef<CPCRReaction> >, it, obj.GetPcr_primers().Get()) {
            if ((*it)->CanGetForward()) {
                ITERATE (list<CRef<CPCRPrimer> >, pr, (*it)->GetForward().Get()) {
                    if ((*pr)->CanGetName()) {
                        AddObjToQualMap("fwd-primer-name", (*pr)->GetName(), *disc_obj, m_Objs);
                    }

                    if ((*pr)->CanGetSeq()) {
                        AddObjToQualMap("fwd-primer-seq", (*pr)->GetSeq(), *disc_obj, m_Objs);
                    }
                }
            }
            if ((*it)->CanGetReverse()) {
                ITERATE (list<CRef<CPCRPrimer> >, pr, (*it)->GetReverse().Get()) {
                    if ((*pr)->CanGetName()) {
                        AddObjToQualMap("rev-primer-name", (*pr)->GetName(), *disc_obj, m_Objs);
                    }
                    if ((*pr)->CanGetSeq()) {
                        AddObjToQualMap("rev-primer-seq", (*pr)->GetSeq(), *disc_obj, m_Objs);
                    }
                }
            }
        }
    }
}


class CSourseQualsAutofixData : public CObject
{
public:
    string m_Qualifier;
    mutable string m_Value;
    vector<string> m_Choice;
    mutable bool m_Ask;
    void* m_User;
    CSourseQualsAutofixData() : m_Ask(0), m_User(0) {}
};
typedef map<const CReportObj*, CRef<CReportObj> > TReportObjPtrMap;
typedef map<string, vector<CRef<CReportObj> > > TStringObjVectorMap;
typedef map<string, TStringObjVectorMap > TStringStringObjVectorMap;

static bool GetSubtypeStr(const string& qual, const string& val, const TReportObjectList& objs, string& subtype)
{
    bool unique = objs.size() == 1;
    if (unique) {
        subtype = "[n] source[s] [has] unique value[s] for " + qual;
    }
    else {
        subtype = "[n] source[s] [has] " + qual + " = " + val;
    }

    return unique;
}

static void AddObjectToReport(const string& subtype, const string& qual, const string& val, bool unique, CReportObj& obj, CReportNode& report)
{
    if (unique) {
        report[subtype]["1 source has " + qual + " = " + val].Add(obj);
    }
    else {
        report[subtype].Add(obj);
    }
}

static void AddObjsToReport(const string& diagnosis, CReportNode::TNodeMap& all_objs, const string& qual, CReportNode& report)
{
    NON_CONST_ITERATE(CReportNode::TNodeMap, objs, all_objs) {

        string subtype;
        bool unique = GetSubtypeStr(qual, objs->first, objs->second->GetObjects(), subtype);

        ITERATE(TReportObjectList, obj, objs->second->GetObjects()) {

            AddObjectToReport(subtype, qual, objs->first, unique, obj->GetNCObject(), report[diagnosis]);
        }
    }
}

static void AddObjsToReport(const string& diagnosis, const TStringObjVectorMap& all_objs, const string& qual, CReportNode& report)
{
    ITERATE(TStringObjVectorMap, objs, all_objs) {

        string subtype;
        bool unique = GetSubtypeStr(qual, objs->first, objs->second, subtype);

        ITERATE(TReportObjectList, obj, objs->second) {

            AddObjectToReport(subtype, qual, objs->first, unique, obj->GetNCObject(), report[diagnosis]);
        }
    }
}

static size_t GetNumOfObjects(CReportNode& root)
{
    size_t ret = root.GetObjects().size();

    NON_CONST_ITERATE(CReportNode::TNodeMap, child, root.GetMap()) {
        ret += GetNumOfObjects(*child->second);
    }

    return ret;
}

static size_t GetSortOrderId(const string& subitem, CReportNode& node)
{
    static const size_t CEILING_VALUE = 1000000000;

    size_t ret = NStr::Find(subitem, "[has] missing") != NPOS ||
                 NStr::Find(subitem, "isolate") != NPOS ?
                 GetNumOfObjects(node) : CEILING_VALUE - GetNumOfObjects(node);

    return ret;
}

DISCREPANCY_SUMMARIZE(SOURCE_QUALS)
{
    CReportNode report,
                final_report;
    CReportNode::TNodeMap& the_map = m_Objs.GetMap();
    TReportObjectList& all = m_Objs["all"].GetObjects();
    size_t total = all.size();
    TReportObjPtrMap all_missing;
    ITERATE (TReportObjectList, it, all) {
        all_missing[it->GetPointer()] = *it;
    }

    NON_CONST_ITERATE (CReportNode::TNodeMap, it, the_map) {
        if (it->first == "all") {
            continue;
        }
        string qual = it->first;
        size_t bins = 0;
        size_t uniq = 0;
        size_t num = 0;
        size_t pres = m_Objs["all"][qual].GetObjects().size();
        size_t mul = m_Objs["all"][qual]["*"].GetObjects().size();
        TReportObjPtrMap missing = all_missing;
        CReportNode::TNodeMap& sub = it->second->GetMap();
        TStringStringObjVectorMap capital;
        NON_CONST_ITERATE (CReportNode::TNodeMap, jj, sub) {
            TReportObjectList& obj = jj->second->GetObjects();
            bins++;
            num += obj.size();
            uniq += obj.size() == 1 ? 1 : 0;
            string upper = jj->first;
            upper = NStr::ToUpper(upper);
            ITERATE (TReportObjectList, o, obj) {
                missing.erase(o->GetPointer());
                capital[upper][jj->first].push_back(*o);
            }
        }
        string diagnosis = OrderQual(it->first);
        diagnosis += " (";
        diagnosis += pres == total ? "all present" : "some missing";
        diagnosis += ", ";
        diagnosis += uniq == num ? "all unique" : bins == 1 ? "all same" : "some duplicates";
        diagnosis += mul ? ", some multi)" : ")";
        report[diagnosis];

        if ((num != total || bins != 1) && (it->first == "collection-date" || it->first == "country" || it->first == "isolation-source" || it->first == "strain")) {
            report[diagnosis].Fatal();
        }

        if ((bins > capital.size() || (num < total && capital.size() == 1))
            && (it->first == "country" || it->first == "collection-date" || it->first == "isolation-source")) { // autofixable
            CRef<CSourseQualsAutofixData> fix;
            if (bins > capital.size()) { // capitalization
                ITERATE (TStringStringObjVectorMap, cap, capital) {
                    const TStringObjVectorMap& objs = cap->second;
                    if (objs.size() < 2) {

                        AddObjsToReport(diagnosis, objs, it->first, report);
                        continue;
                    }
                    size_t best_count = 0;
                    fix.Reset(new CSourseQualsAutofixData);
                    fix->m_Qualifier = it->first;
                    fix->m_User = context.GetUserData();
                    ITERATE(TStringObjVectorMap, x, objs) {
                        fix->m_Choice.push_back(x->first);
                        if (best_count < x->second.size()) {
                            best_count = x->second.size();
                            fix->m_Value = x->first;
                        }
                    }
                    ITERATE(TStringObjVectorMap, x, objs) {
                        const vector<CRef<CReportObj> >& v = x->second;
                        ITERATE (vector<CRef<CReportObj> >, o, v) {
                            report[diagnosis]["[n] source[s] [has] inconsistent capitalization: " + it->first + " (" + x->first + ")"].Add(*((const CDiscrepancyObject&)**o).Clone(true, CRef<CObject>(fix.GetNCPointer())));
                        }
                    }
                }
            }
            else {
                AddObjsToReport(diagnosis, sub, it->first, report);
            }

            if (num < total) { // some missing
                if (capital.size() == 1 && num / (float)total >= context.GetSesameStreetCutoff()) { // all same and autofixable
                    if (fix.IsNull()) {
                        fix.Reset(new CSourseQualsAutofixData);
                        fix->m_Qualifier = it->first;
                        fix->m_Value = sub.begin()->first;
                        fix->m_User = context.GetUserData();
                    }
                    ITERATE (TReportObjPtrMap, o, missing) {
                        report[diagnosis]["[n] source[s] [has] missing " + it->first + " (" + sub.begin()->first + ")"].Add(*((const CDiscrepancyObject&)*o->second).Clone(true, CRef<CObject>(fix.GetNCPointer())));
                    }
                }
                else {
                    ITERATE(TReportObjPtrMap, o, missing) {
                        CRef<CReportObj> r = o->second;
                        report[diagnosis]["[n] source[s] [has] missing " + it->first].Add(*r);
                    }
                }
            }
        }
        else { // not autofixable
            AddObjsToReport(diagnosis, sub, it->first, report);

            ITERATE(TReportObjPtrMap, o, missing) {
                CRef<CReportObj> r = o->second;
                report[diagnosis]["[n] source[s] [has] missing " + it->first].Add(*r);
            }
        }

        static const size_t MAX_NUM_STR_LEN = 20;

        NON_CONST_ITERATE(CReportNode::TNodeMap, item, report[diagnosis].GetMap()) {

            // It builds a key for map to be looked like "[*00000000000000000123*]<old_key>" to keep a required sort order
            size_t sort_order_id = GetSortOrderId(item->first, *item->second);
            string sort_order_str = NStr::SizetToString(sort_order_id);
            string leading_zeros(MAX_NUM_STR_LEN - sort_order_str.size(), '0');
            string subitem = "[*" + leading_zeros + sort_order_str + "*]" + item->first;

            final_report[diagnosis][subitem].Copy(item->second);
        }
    }
    m_ReportItems = final_report.Export(*this)->GetSubitems();
}


static void SetSubsource(CRef<CBioSource> bs, CSubSource::ESubtype st, const string& s, size_t& added, size_t& changed)
{
    ITERATE (CBioSource::TSubtype, it, bs->GetSubtype()) {
        if ((*it)->GetSubtype() == st) {
            CRef<CSubSource> ss(*it);
            if (ss->GetName() != s) {
                ss->SetName(s);
                changed++;
            }
            return;
        }
    }
    bs->SetSubtype().push_back(CRef<CSubSource>(new CSubSource(st, s)));
    added++;
}


static void SetOrgMod(CRef<CBioSource> bs, COrgMod::ESubtype st, const string& s, size_t& added, size_t& changed)
{
    ITERATE (COrgName::TMod, it, bs->GetOrgname().GetMod()) {
        if ((*it)->GetSubtype() == st) {
            CRef<COrgMod> ss(*it);
            if (ss->GetSubname() != s) {
                ss->SetSubname(s);
                changed++;
            }
            return;
        }
    }
    bs->SetOrg().SetOrgname().SetMod().push_back(CRef<COrgMod>(new COrgMod(st, s)));
    added++;
}


DISCREPANCY_AUTOFIX(SOURCE_QUALS)
{
    TReportObjectList list = item->GetDetails();
    const CSourseQualsAutofixData* fix = 0;
    size_t added = 0;
    size_t changed = 0;
    string qual;
    string val;
    NON_CONST_ITERATE (TReportObjectList, it, list) {
        if ((*it)->CanAutofix()) {
            CDiscrepancyObject& obj = *dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer());
            CSeqdesc* desc = const_cast<CSeqdesc*>(dynamic_cast<const CSeqdesc*>(obj.GetObject().GetPointer()));
            CRef<CBioSource> bs(&desc->SetSource());
            fix = dynamic_cast<const CSourseQualsAutofixData*>(obj.GetMoreInfo().GetPointer());
            if (!fix) {
                continue;
            }
            qual = fix->m_Qualifier;
            val = fix->m_Value;

            //CAutofixHookRegularArguments arg;
            //arg.m_User = fix->m_User;
            //if (m_Hook) {
            //    m_Hook(&arg);
            //}
        
            if (qual == "host") {
                SetOrgMod(bs, COrgMod::eSubtype_nat_host, val, added, changed);
                dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->SetFixed();
            }
            else if (qual == "strain") {
                SetOrgMod(bs, COrgMod::eSubtype_strain, val, added, changed);
                dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->SetFixed();
            }
            else if (qual == "country") {
                SetSubsource(bs, CSubSource::eSubtype_country, val, added, changed);
                dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->SetFixed();
            }
            else if (qual == "isolation-source") {
                SetSubsource(bs, CSubSource::eSubtype_isolation_source, val, added, changed);
                dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->SetFixed();
            }
            else if (qual == "collection-date") {
                SetSubsource(bs, CSubSource::eSubtype_collection_date, val, added, changed);
                dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->SetFixed();
            }
        }
    }
    if (changed) {
        return CRef<CAutofixReport>(new CAutofixReport("SOURCE_QUALS: [n] qualifier[s] " + qual + " (" + val + ") fixed", added + changed));
    }
    else {
        return CRef<CAutofixReport>(new CAutofixReport("SOURCE_QUALS: [n] missing qualifier[s] " + qual + " (" + val + ") added", added));
    }
}


DISCREPANCY_ALIAS(SOURCE_QUALS, SOURCE_QUALS_ASNDISC)
DISCREPANCY_ALIAS(SOURCE_QUALS, SRC_QUAL_PROBLEM)
DISCREPANCY_ALIAS(SOURCE_QUALS, MISSING_SRC_QUAL)


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
