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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors: Peter Meric
 *
 * File Description:  NetSchedule grid client for TrackManager switch-context request/reply
 *
 */

#include <ncbi_pch.hpp>
#include <objects/trackmgr/trackmgr__.hpp>
#include <internal/trackmgr/gridcli/switch_context_client.hpp>


BEGIN_NCBI_SCOPE


CTMS_SwitchContext_Client::CTMS_SwitchContext_Client(const string& NS_service,
                                                   const string& NS_queue,
                                                   const string& client_name,
                                                   const string& NC_registry_section
                                                  )
    : TBaseClient(NS_service, NS_queue, client_name, NC_registry_section)
{
}

CTMS_SwitchContext_Client::CTMS_SwitchContext_Client(const string& NS_registry_section,
                                                   const string& NC_registry_section
                                                  )
    : TBaseClient(NS_registry_section, NC_registry_section)
{
}

CTMS_SwitchContext_Client::~CTMS_SwitchContext_Client()
{
}

CTMS_SwitchContext_Client::TReplyRef
CTMS_SwitchContext_Client::Fetch(const TRequest& request) const
{
    TReplyRef reply(new TReply());
    TBaseClient::Ask(request, *reply);
    return reply;
}


END_NCBI_SCOPE
