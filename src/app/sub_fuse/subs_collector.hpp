/*
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
* Author:  Alexey Dobronadezhdin
*
* File Description:
*
* ===========================================================================
*/

#ifndef SUBS_COLLECTOR_H
#define SUBS_COLLECTOR_H

#include <corelib/ncbistre.hpp>
#include <serial/objostrasn.hpp>

namespace ncbi
{
    namespace objects
    {
        class CSeq_entry;
        class CSeq_annot;
        class CSeq_feat;
    }
}

USING_NCBI_SCOPE;
USING_SCOPE(objects);

namespace subfuse
{

class CSubmissionCollector
{
public:

    CSubmissionCollector(CNcbiOstream& out);

    bool ProcessFile(const string& path);
    bool CompleteProcessing();

private:

    void AdjustLocalIds(CSeq_entry& entry, map<int, int>& substitution, bool xrefs = false);
    void AdjustLocalIds(CSeq_annot& annot, map<int, int>& substitution, bool xrefs);
    void AdjustLocalIds(CSeq_feat& feat, map<int, int>& substitution);

    void AdjustXrefLocalIds(CSeq_feat& feat, map<int, int>& substitution);

    CObjectOStreamAsn m_out;
    bool m_header_not_set;

    int m_cur_id;
};

}

#endif // SUBS_COLLECTOR_H