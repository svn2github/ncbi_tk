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
 * Authors: Rafael Sadyrov
 *
 * File Description: NetStorage automation implementation.
 *
 */

#include <ncbi_pch.hpp>

#include "nst_automation.hpp"

USING_NCBI_SCOPE;

string s_GetInitString(CArgArray& arg_array)
{
    const string service_name(arg_array.NextString(kEmptyStr));
    const string domain_name(arg_array.NextString(kEmptyStr));
    const string client_name(arg_array.NextString(kEmptyStr));
    const string metadata(arg_array.NextString(kEmptyStr));

    return "nst=" + service_name + "&domain=" + domain_name +
        "&client=" + client_name + "&metadata=" + metadata;
}

static void s_RemoveStdReplyFields(CJsonNode& server_reply)
{
    server_reply.DeleteByKey("Type");
    server_reply.DeleteByKey("Status");
    server_reply.DeleteByKey("RE");
    server_reply.DeleteByKey("Warnings");
}

SNetStorageServiceAutomationObject::SNetStorageServiceAutomationObject(
        CAutomationProc* automation_proc, CArgArray& arg_array) :
    SNetServiceAutomationObject(automation_proc,
            CNetService::eLoadBalancedService),
    m_NetStorageAdmin(CNetStorage(s_GetInitString(arg_array)))
{
    m_Service = m_NetStorageAdmin.GetService();
    // TODO:
    // m_NetStorageAdmin.SetEventHandler(
    //         new CEventHandler(automation_proc, m_NetStorageAdmin));
}

void SNetStorageServiceAutomationObject::CEventHandler::OnWarning(
        const string& warn_msg, CNetServer server)
{
    // TODO:
    // m_AutomationProc->SendWarning(warn_msg, m_AutomationProc->
    //         ReturnNetStorageServerObject(m_NetStorageAdmin, server));
}

const string& SNetStorageServiceAutomationObject::GetType() const
{
    static const string object_type("nstsvc");

    return object_type;
}

const void* SNetStorageServiceAutomationObject::GetImplPtr() const
{
    return m_NetStorageAdmin;
}

#if 0
SNetStorageServerAutomationObject::SNetStorageServerAutomationObject(
        CAutomationProc* automation_proc, CArgArray& arg_array) :
    SNetStorageServiceAutomationObject(automation_proc,
            CNetStorageAdmin(CNetStorage(s_GetInitString(arg_array)))
{
    if (m_Service.IsLoadBalanced()) {
        NCBI_THROW(CAutomationException, eCommandProcessingError,
                "NetStorageServer constructor: "
                "'server_address' must be a host:port combination");
    }

    m_NetServer = m_Service.Iterate().GetServer();
    // TODO:
    // m_NetStorageAdmin.SetEventHandler(
    //         new CEventHandler(automation_proc, m_NetStorageAdmin));
}

const string& SNetStorageServerAutomationObject::GetType() const
{
    static const string object_type("nstsrv");

    return object_type;
}

const void* SNetStorageServerAutomationObject::GetImplPtr() const
{
    return m_NetServer;
}

TAutomationObjectRef CAutomationProc::ReturnNetStorageServerObject(
        CNetStorageAdmin::TInstance ns_api,
        CNetServer::TInstance server)
{
    TAutomationObjectRef object(FindObjectByPtr(server));
    if (!object) {
        object = new SNetStorageServerAutomationObject(this, ns_api, server);
        AddObject(object, server);
    }
    return object;
}
#endif

bool SNetStorageServiceAutomationObject::Call(const string& method,
        CArgArray& arg_array, CJsonNode& reply)
{
    if (method == "client_objects") {
        string client_name(arg_array.NextString());
        string client_ns(arg_array.NextString(kEmptyStr));
        Int8 limit(arg_array.NextInteger(0));

        CJsonNode request(m_NetStorageAdmin.MkNetStorageRequest("GETCLIENTOBJECTS"));
        request.SetString("ClientName", client_name);
        if (!client_ns.empty()) request.SetString("ClientNamespace", client_ns);
        if (limit) request.SetInteger("Limit", limit);

        CJsonNode response(m_NetStorageAdmin.ExchangeJson(request));
        s_RemoveStdReplyFields(response);
        reply.Append(response);
    } else if (method == "user_objects") {
        string user_name(arg_array.NextString());
        string user_ns(arg_array.NextString(kEmptyStr));
        Int8 limit(arg_array.NextInteger(0));

        CJsonNode request(m_NetStorageAdmin.MkNetStorageRequest("GETUSEROBJECTS"));
        request.SetString("ClientName", user_name);
        if (!user_ns.empty()) request.SetString("ClientNamespace", user_ns);
        if (limit) request.SetInteger("Limit", limit);

        CJsonNode response(m_NetStorageAdmin.ExchangeJson(request));
        s_RemoveStdReplyFields(response);
        reply.Append(response);
    }
#if 0
    if (method == "get_servers") {
        CJsonNode object_ids(CJsonNode::NewArrayNode());
        for (CNetServiceIterator it = m_NetStorageAdmin.GetService().Iterate(
                CNetService::eIncludePenalized); it; ++it)
            object_ids.AppendInteger(m_AutomationProc->
                    ReturnNetStorageServerObject(m_NetStorageAdmin, *it)->GetID());
        reply.Append(object_ids);
    } else
        return SNetServiceAutomationObject::Call(method, arg_array, reply);
#endif

    return true;
}

#if 0
bool SNetStorageServerAutomationObject::Call(const string& method,
        CArgArray& arg_array, CJsonNode& reply)
{
    return SNetStorageServiceAutomationObject::Call(method, arg_array, reply);
}
#endif
