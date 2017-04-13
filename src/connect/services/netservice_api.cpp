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
 * Author:  Maxim Didenko, Dmitry Kazimirov
 *
 * File Description:
 */

#include <ncbi_pch.hpp>

#include "../ncbi_lbsmd.h"
#include "../ncbi_servicep.h"
#include "../ncbi_comm.h"

#include "netservice_api_impl.hpp"

#include <connect/services/error_codes.hpp>
#include <connect/services/srv_connections_expt.hpp>
#include <connect/services/netcache_api_expt.hpp>
#include <connect/services/netschedule_api_expt.hpp>

#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_conn_exception.hpp>
#include <connect/ncbi_core_cxx.hpp>

#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_config.hpp>
#include <corelib/ncbi_message.hpp>

#include <util/random_gen.hpp>
#include <util/checksum.hpp>

#define NCBI_USE_ERRCODE_X   ConnServ_Connection

#define LBSMD_PENALIZED_RATE_BOUNDARY -0.01

BEGIN_NCBI_SCOPE

// The purpose of this class is to execute commands suppressing possible errors and avoiding retries
class SNetServiceImpl::CTry
{
    struct SHandler : public INetEventHandler
    {
        bool OnError(CException::TErrCode) { return true; }
    };

public:
    CTry(SNetServiceImpl* service) :
        m_Service(service)
    {
        _ASSERT(m_Service);
        Swap(new SHandler);
    }

    ~CTry()
    {
        Swap(m_OriginalHandler);
    }

private:
    void Swap(INetEventHandler* handler)
    {
        m_OriginalHandler = m_Service->SetEventHandler(handler);
        swap(m_MaxRetries, m_Service->m_ConnectionMaxRetries);
    }

    CNetRef<SNetServiceImpl> m_Service;
    CRef<INetEventHandler> m_OriginalHandler;
    unsigned m_MaxRetries = 0;
};

void SDiscoveredServers::DeleteThis()
{
    CNetService service(m_Service);

    if (!service)
        return;

    // Before resetting the m_Service pointer, verify that no other object
    // has acquired a reference to this server group object yet (between
    // the time the reference counter went to zero, and the current moment
    // when m_Service is about to be reset).
    CFastMutexGuard discovery_mutex_lock(service->m_DiscoveryMutex);

    service = NULL;

    if (!Referenced() && m_Service) {
        if (m_Service->m_DiscoveredServers != this) {
            m_NextGroupInPool = m_Service->m_ServerGroupPool;
            m_Service->m_ServerGroupPool = this;
        }
        m_Service = NULL;
    }
}

CNetServer CNetServiceIterator::GetServer()
{
    return new SNetServerImpl(m_Impl->m_ServerGroup->m_Service,
            m_Impl->m_ServerGroup->m_Service->m_ServerPool->
            ReturnServer(m_Impl->m_Position->first));
}

bool CNetServiceIterator::Next()
{
    if (m_Impl->Next())
        return true;

    m_Impl.Reset(NULL);
    return false;
}

bool CNetServiceIterator::Prev()
{
    if (m_Impl->Prev())
        return true;

    m_Impl.Reset(NULL);
    return false;
}

double CNetServiceIterator::GetRate() const
{
    return m_Impl->m_Position->second;
}

bool SNetServiceIteratorImpl::Next()
{
    return ++m_Position != m_ServerGroup->m_Servers.end();
}

bool SNetServiceIteratorImpl::Prev()
{
    if (m_Position == m_ServerGroup->m_Servers.begin())
        return false;
    --m_Position;
    return true;
}

bool SNetServiceIterator_OmitPenalized::Next()
{
    return ++m_Position != m_ServerGroup->m_SuppressedBegin;
}

DEFINE_STATIC_FAST_MUTEX(s_RndLock);
static CRandom s_RandomIteratorGen((CRandom::TValue) time(NULL));

static CRandom::TValue
s_GetRand(CRandom::TValue max_value)
{
    CFastMutexGuard guard(s_RndLock);
    return s_RandomIteratorGen.GetRand(0, max_value);
}

SNetServiceIterator_RandomPivot::SNetServiceIterator_RandomPivot(
        SDiscoveredServers* server_group_impl) :
    SNetServiceIteratorImpl(server_group_impl,
        server_group_impl->m_Servers.begin() + s_GetRand(
            CRandom::TValue((server_group_impl->m_SuppressedBegin -
                server_group_impl->m_Servers.begin()) - 1)))
{
}

bool SNetServiceIterator_RandomPivot::Next()
{
    if (m_RandomIterators.empty()) {
        TNetServerList::const_iterator it = m_ServerGroup->m_Servers.begin();
        size_t number_of_servers = m_ServerGroup->m_SuppressedBegin - it;
        if (number_of_servers <= 1)
            return false; // There are no servers to advance to.
        m_RandomIterators.reserve(number_of_servers);
        m_RandomIterators.push_back(m_Position);
        --number_of_servers;
        do {
            if (it != m_Position) {
                m_RandomIterators.push_back(it);
                --number_of_servers;
            }
            ++it;
        } while (number_of_servers > 0);
        // Shuffle m_RandomIterators starting from the element with index '1'.
        if (m_RandomIterators.size() > 2) {
            TRandomIterators::iterator rnt_it = m_RandomIterators.begin();
            while (++rnt_it != m_RandomIterators.end())
                swap(*rnt_it, m_RandomIterators[s_RandomIteratorGen.GetRand(1,
                    CRandom::TValue(m_RandomIterators.size() - 1))]);
        }
        m_RandomIterator = m_RandomIterators.begin();
        ++m_RandomIterator;
    } else
        if (++m_RandomIterator == m_RandomIterators.end())
            return false;

    m_Position = *m_RandomIterator;

    return true;
}

bool SNetServiceIterator_RandomPivot::Prev()
{
    if (m_RandomIterators.empty() ||
            m_RandomIterator == m_RandomIterators.begin())
        return false;

    m_Position = *--m_RandomIterator;

    return true;
}

bool SNetServiceIterator_Circular::Next()
{
    if (++m_Position == m_ServerGroup->m_Servers.end())
        m_Position = m_ServerGroup->m_Servers.begin();
    return m_Position != m_Pivot;
}

bool SNetServiceIterator_Circular::Prev()
{
    if (m_Position == m_Pivot)
        return false;
    if (m_Position == m_ServerGroup->m_Servers.begin())
        m_Position = m_ServerGroup->m_Servers.end();
    --m_Position;
    return true;
}

SNetServiceIterator_Weighted::SNetServiceIterator_Weighted(
        SDiscoveredServers* server_group_impl, Uint4 key_crc32) :
    SNetServiceIteratorImpl(server_group_impl),
    m_KeyCRC32(key_crc32)
{
    TNetServerList::const_iterator server_list_iter(m_Position);

    if ((m_SingleServer =
            (++server_list_iter == server_group_impl->m_SuppressedBegin)))
        // Nothing to do if there's only one server.
        return;

    // Find the server with the highest rank.
    SServerRank highest_rank(x_GetServerRank(m_Position));

    do {
        SServerRank server_rank(x_GetServerRank(server_list_iter));
        if (highest_rank < server_rank)
            highest_rank = server_rank;
        // To avoid unnecessary memory allocations, do not save
        // the calculated server ranks in hope that Next()
        // will be called very rarely for this type of iterators.
    } while (++server_list_iter != server_group_impl->m_SuppressedBegin);

    m_Position = highest_rank.m_ServerListIter;
}

bool SNetServiceIterator_Weighted::Next()
{
    if (m_SingleServer)
        return false;

    if (m_ServerRanks.empty()) {
        TNetServerList::const_iterator server_list_iter(
                m_ServerGroup->m_Servers.begin());
        do
            m_ServerRanks.push_back(x_GetServerRank(server_list_iter));
        while (++server_list_iter != m_ServerGroup->m_SuppressedBegin);

        // Sort the ranks in *reverse* order.
        sort(m_ServerRanks.rbegin(), m_ServerRanks.rend());

        // Skip the server with the highest rank, which was the first
        // server returned by this iterator object.
        m_CurrentServerRank = m_ServerRanks.begin() + 1;
    } else if (++m_CurrentServerRank == m_ServerRanks.end())
        return false;

    m_Position = m_CurrentServerRank->m_ServerListIter;
    return true;
}

bool SNetServiceIterator_Weighted::Prev()
{
    if (m_SingleServer)
        return false;

    _ASSERT(!m_ServerRanks.empty());

    if (m_CurrentServerRank == m_ServerRanks.begin())
        return false;

    m_Position = (--m_CurrentServerRank)->m_ServerListIter;
    return true;
}

SNetServerPoolImpl::SNetServerPoolImpl(INetServerConnectionListener* listener,
        bool old_style_auth) :
    m_Listener(listener),
    m_EnforcedServer(0, 0),
    m_MaxTotalTime(CTimeout::eInfinite),
    m_UseOldStyleAuth(old_style_auth)
{
}

void SNetServiceImpl::Construct(SNetServerInPool* server)
{
    m_ServiceType = CNetService::eSingleServerService;
    m_DiscoveredServers = AllocServerGroup(0);
    CFastMutexGuard server_mutex_lock(m_ServerPool->m_ServerMutex);
    m_DiscoveredServers->m_Servers.push_back(TServerRate(server, 1));
    m_DiscoveredServers->m_SuppressedBegin =
        m_DiscoveredServers->m_Servers.end();
}

void SNetServiceImpl::Construct()
{
    if (!m_ServiceName.empty()) {
        string host, port;

        if (!NStr::SplitInTwo(m_ServiceName, ":", host, port))
            m_ServiceType = CNetService::eLoadBalancedService;
        else
            Construct(m_ServerPool->FindOrCreateServerImpl(SServerAddress(host,
                    (unsigned short) NStr::StringToInt(port))));
    }
}

#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
void CNetService::AllowXSiteConnections()
{
    SNetServiceXSiteAPI::AllowXSiteConnections();
}

bool CNetService::IsUsingXSiteProxy()
{
    return SNetServiceXSiteAPI::IsUsingXSiteProxy();
}

static const char kXSiteFwd[] = "XSITEFWD";

void SNetServiceXSiteAPI::AllowXSiteConnections()
{
    m_AllowXSiteConnections.store(true);

    SConnNetInfo* net_info = ConnNetInfo_Create(kXSiteFwd);

    // ATTENTION!
    //   We specifically set "fSERV_Promiscuous" flag here to avoid a
    //   fallback hit to DISPD.CGI if the service is marked as "Down" in the
    //   local LBSM table (and thus, is not immediately "visible" unless
    //   the "fSERV_Promiscuous" flag is set.
    SSERV_Info* sinfo = SERV_GetInfo(kXSiteFwd,
            fSERV_Standalone|fSERV_Promiscuous, SERV_LOCALHOST, net_info);

    ConnNetInfo_Destroy(net_info);

    if (sinfo == NULL) {
        NCBI_THROW(CNetSrvConnException, eLBNameNotFound,
            "Cannot find cross-site proxy");
    }

    m_ColoNetwork.store(SOCK_NetToHostLong(sinfo->host) >> 16);

    free(sinfo);
}

bool SNetServiceXSiteAPI::IsUsingXSiteProxy()
{
    return m_AllowXSiteConnections.load();
}

void SNetServiceXSiteAPI::InitXSite(ISynRegistry& registry, const string& section)
{
    if (registry.Get({ "netservice_api", section }, "allow_xsite_conn", false)) {
        AllowXSiteConnections();
    }
}

void SNetServiceXSiteAPI::ConnectXSite(CSocket& socket,
        SNetServerImpl::SConnectDeadline& deadline,
        const SServerAddress& original)
{
    SServerAddress actual(original);
    ticket_t ticket = 0;

    if (IsUsingXSiteProxy() && IsColoAddr(actual.host)) {
        union {
            SFWDRequestReply rq;
            char buffer[FWD_MAX_RR_SIZE + 1];
        };

        memset(buffer, 0, sizeof(buffer));
        rq.host =                     actual.host;
        rq.port = SOCK_HostToNetShort(actual.port);
        rq.flag = SOCK_HostToNetShort(1);
        _ASSERT(offsetof(SFWDRequestReply, text) +
                sizeof(kXSiteFwd) < sizeof(buffer));
        memcpy(rq.text, kXSiteFwd, sizeof(kXSiteFwd));

        size_t len = 0;

        CConn_ServiceStream svc(kXSiteFwd);
        if (svc.write((const char*) &rq.ticket/*0*/, sizeof(rq.ticket)) &&
                svc.write(buffer, offsetof(SFWDRequestReply, text) +
                        sizeof(kXSiteFwd))) {
            svc.read(buffer, sizeof(buffer) - 1);
            len = (size_t) svc.gcount();
            _ASSERT(len < sizeof(buffer));
        }

        memset(buffer + len, 0, sizeof(buffer) - len);

        if (len < offsetof(SFWDRequestReply, text) ||
            (rq.flag & 0xF0F0) || rq.port == 0) {
            const char* err;
            if (len == 0)
                err = "Connection refused";
            else if (len < offsetof(SFWDRequestReply, text))
                err = "Short response received";
            else if (!(rq.flag & 0xF0F0))
                err = rq.flag & 0x0F0F ? "Client rejected" : "Unknown error";
            else if (NStr::strncasecmp(buffer, "NCBI", 4) == 0)
                err = buffer;
            else if (rq.text[0])
                err = rq.text;
            else
                err = "Unspecified error";
            NCBI_THROW_FMT(CNetSrvConnException, eConnectionFailure,
                           "Error while acquiring an auth ticket from a "
                           "cross-site connection proxy: " << err);
        }

        if (rq.ticket != 0) {
            actual.host =                     rq.host;
            actual.port = SOCK_NetToHostShort(rq.port);
        } else {
            SOCK sock;
            actual.port = 0;
            EIO_Status io_st = CONN_GetSOCK(svc.GetCONN(), &sock);
            if (sock != NULL)
                SOCK_CreateOnTop(sock, 0, &sock);
            if (io_st != eIO_Success  ||  sock == NULL) {
                NCBI_THROW(CNetSrvConnException, eConnectionFailure,
                           "Error while connecting to proxy.");
            }
            socket.Reset(sock, eTakeOwnership, eCopyTimeoutsToSOCK);
        }
        ticket = rq.ticket;
    }

    if (actual.port) {
        SNetServerImpl::ConnectImpl(socket, deadline, actual, original);
    }

    if (ticket && socket.Write(&ticket, sizeof(ticket)) != eIO_Success) {
        NCBI_THROW(CNetSrvConnException, eConnectionFailure,
                "Error while sending proxy auth ticket.");
    }
}

atomic<unsigned> SNetServiceXSiteAPI::m_ColoNetwork{0};
atomic<bool> SNetServiceXSiteAPI::m_AllowXSiteConnections{false};

#else

void SNetServiceXSiteAPI::InitXSite(ISynRegistry&, const string&)
{
}

void SNetServiceXSiteAPI::ConnectXSite(CSocket& socket,
        SNetServerImpl::SConnectDeadline& deadline,
        const SServerAddress& original)
{
    SNetServerImpl::ConnectImpl(socket, deadline, original, original);
}

#endif

CConfig* FindSection(const char* const* sections, const CConfig::TParamTree* tree,
        string* section)
{
    _ASSERT(section);

    if (section->empty()) {
        *section = *sections++;
    }

    for (;;) {
        if (const CConfig::TParamTree* sub_tree = tree->FindSubNode(*section)) {
            return new CConfig(sub_tree);
        }

        if (!*sections) {
            break;
        }

        *section = *sections++;
    }

    return NULL;
}

void SNetServiceImpl::Init(CObject* api_impl, const string& service_name,
    CConfig* config, string section, const char* const* default_sections)
{
    _ASSERT(m_Listener);

    // Initialize the connect library and LBSM structures
    // used in DiscoverServersIfNeeded().
    {
        class CInPlaceConnIniter : protected CConnIniter
        {
        public:
            void NoOp() {};
        } conn_initer;
        conn_initer.NoOp();
    }

    CMemoryRegistry empty_registry;
    CConfig empty_config(empty_registry);
    CConfigRegistry config_registry(&empty_config);

    auto_ptr<CConfig::TParamTree> param_tree;

    if (config == NULL) {
        CMutexGuard guard(CNcbiApplication::GetInstanceMutex());

        if (CNcbiApplication* app = CNcbiApplication::Instance()) {

            if (const CNcbiRegistry* reg = &app->GetConfig()) {
                param_tree.reset(CConfig::ConvertRegToTree(*reg));

                if (CConfig *alt = FindSection(default_sections, param_tree.get(), &section)) {
                    config_registry.Reset(config = alt, eTakeOwnership);
                }
            }
        }
    } else if (CConfig *alt = FindSection(default_sections, config->GetTree(), &section)) {
        config_registry.Reset(config = alt, eTakeOwnership);
    }

    CSynRegistry syn_registry(&config_registry);
    CCachedSynRegistry registry(&syn_registry);

    m_ServiceName = service_name;
    NStr::TruncateSpacesInPlace(m_ServiceName);

    // TODO:
    // Do not check for emptiness and always read values (client, service, etc) from registry
    // after values provided in ctors get into an underlying memory registry.

    // Do not override explicitly set client name
    if (m_ClientName.empty()) m_ClientName = registry.Get(section, { "client_name", "client" }, "");

    m_Listener->OnPreInit(api_impl, registry, &section, m_ClientName);

    if (m_ServiceName.empty()) {
        m_ServiceName = registry.Get(section, { "service", "service_name" }, "");

        if (m_ServiceName.empty()) {
            string host = registry.Get(section, { "server", "host" }, "");
            string port = registry.Get(section, "port", "");

            if (!host.empty() && !port.empty()) m_ServiceName = host + ":" + port;
        }
    }

    InitXSite(registry, section);

    m_UseSmartRetries = registry.Get(section, "smart_service_retries", m_UseSmartRetries);

    int max_retries = registry.Get({ section, "netservice_api" }, "connection_max_retries", CONNECTION_MAX_RETRIES);
    if (max_retries >= 0) m_ConnectionMaxRetries = max_retries;

    double retry_delay = registry.Get({ section, "netservice_api" }, "retry_delay", RETRY_DELAY_DEFAULT);
    if (retry_delay >= 0) m_ConnectionRetryDelay = retry_delay * kMilliSecondsPerSecond;

    if (m_ClientName.empty() || m_ClientName == "noname" ||
            NStr::FindNoCase(m_ClientName, "unknown") != NPOS) {
        CMutexGuard guard(CNcbiApplication::GetInstanceMutex());
        CNcbiApplication* app = CNcbiApplication::Instance();
        if (app == NULL) {
            NCBI_THROW_FMT(CArgException, eNoValue,
                m_APIName << ": client name is not set");
        }
        m_ClientName = app->GetProgramDisplayName();
    }

    m_ServerPool->Init(registry, section, m_Listener.GetPointerOrNull());

    Construct();

    m_Listener->OnInit(api_impl, registry, section);
}

void SNetServerPoolImpl::Init(ISynRegistry& registry, const string& section, INetServerConnectionListener* listener)
{
    int max_requests = CSimpleRebalanceStrategy::DefaultMaxRequests();
    double max_seconds = CSimpleRebalanceStrategy::DefaultMaxSeconds();

    const auto kConnTimeoutDefault = 2.0;
    const auto kCommTimeoutDefault = 12.0;
    const auto kFirstServerTimeoutDefault = 0.3;

    g_CTimeoutToSTimeout(CTimeout(kConnTimeoutDefault), m_ConnTimeout);
    g_CTimeoutToSTimeout(CTimeout(kCommTimeoutDefault), m_CommTimeout);
    g_CTimeoutToSTimeout(CTimeout(kFirstServerTimeoutDefault), m_FirstServerTimeout);
   
    m_LBSMAffinity.first = registry.Get(section, "use_lbsm_affinity", "");

    // Get affinity value from the local LBSM configuration file.
    if (!m_LBSMAffinity.first.empty()) {
        m_LBSMAffinity.second = LBSMD_GetHostParameter(SERV_LOCALHOST, m_LBSMAffinity.first.c_str());
    }

    double conn_timeout = registry.Get(section, "connection_timeout", kConnTimeoutDefault);
    if (conn_timeout > 0) g_CTimeoutToSTimeout(CTimeout(conn_timeout), m_ConnTimeout);

    double comm_timeout = registry.Get(section, "communication_timeout", kCommTimeoutDefault);
    if (comm_timeout > 0) g_CTimeoutToSTimeout(CTimeout(comm_timeout), m_CommTimeout);

    double first_srv_timeout = registry.Get(section, "first_server_timeout", kFirstServerTimeoutDefault);

    if (first_srv_timeout > 0) {
        g_CTimeoutToSTimeout(CTimeout(first_srv_timeout), m_FirstServerTimeout);
    } else if (comm_timeout > 0) {
        m_FirstServerTimeout = m_CommTimeout;
    }

    double max_total_time = registry.Get(section, "max_connection_time", 0.0);
    if (max_total_time > 0) m_MaxTotalTime = CTimeout(max_total_time);

    max_requests = registry.Get(section, "rebalance_requests", max_requests);
    max_seconds = registry.Get(section, "rebalance_time", max_seconds);

    m_ThrottleParams.Init(registry, section);

    m_RebalanceStrategy = new CSimpleRebalanceStrategy(max_requests, max_seconds);

    m_Listener = listener;
}

void SNetServerPoolImpl::SThrottleParams::Init(ISynRegistry& registry, const string& section)
{
    m_ServerThrottlePeriod = registry.Get(section, "throttle_relaxation_period", 0);

    if (m_ServerThrottlePeriod <= 0) return;

    m_MaxConsecutiveIOFailures = registry.Get(section,
            { "throttle_by_consecutive_connection_failures", "throttle_by_subsequent_connection_failures" }, 0);

    m_ThrottleUntilDiscoverable = registry.Get(section, "throttle_hold_until_active_in_lb", false);

    // These values must correspond to each other
    const auto default_error_rate = "0/1";
    m_IOFailureThresholdNumerator = 0;
    m_IOFailureThresholdDenominator = 1;

    const string error_rate = registry.Get(section, "throttle_by_connection_error_rate", default_error_rate);

    if (error_rate == default_error_rate || error_rate.empty()) return;

    string numerator_str, denominator_str;

    if (!NStr::SplitInTwo(error_rate, "/", numerator_str, denominator_str)) return;

    const auto flags = NStr::fConvErr_NoThrow | NStr::fAllowLeadingSpaces | NStr::fAllowTrailingSpaces;

    int numerator = NStr::StringToInt(numerator_str, flags);
    int denominator = NStr::StringToInt(denominator_str, flags);

    if (numerator < 0) numerator = 0;

    if (denominator < 1) {
        denominator = 1;

    } else if (denominator > CONNECTION_ERROR_HISTORY_MAX) {
        numerator = (numerator * CONNECTION_ERROR_HISTORY_MAX) / denominator;
        denominator = CONNECTION_ERROR_HISTORY_MAX;
    }

    m_IOFailureThresholdNumerator = numerator;
    m_IOFailureThresholdDenominator = denominator;
}

SDiscoveredServers* SNetServiceImpl::AllocServerGroup(
    unsigned discovery_iteration)
{
    if (m_ServerGroupPool == NULL)
        return new SDiscoveredServers(discovery_iteration);
    else {
        SDiscoveredServers* server_group = m_ServerGroupPool;
        m_ServerGroupPool = server_group->m_NextGroupInPool;

        server_group->Reset(discovery_iteration);

        return server_group;
    }
}

string SNetServiceImpl::MakeAuthString()
{
    string auth;
    auth.reserve(256);

    auth += "client=\"";
    auth += NStr::PrintableString(m_ClientName);
    auth += '\"';

    if (!m_ServerPool->m_UseOldStyleAuth) {
        if (m_ServiceType == CNetService::eLoadBalancedService) {
            auth += " svc=\"";
            auth += NStr::PrintableString(m_ServiceName);
            auth += '\"';
        }

        CMutexGuard guard(CNcbiApplication::GetInstanceMutex());
        CNcbiApplication* app = CNcbiApplication::Instance();
        if (app != NULL) {
            auth += " client_path=\"";
            auth += NStr::PrintableString(app->GetProgramExecutablePath());
            auth += '\"';
        }
    }

    return auth;
}

const string& CNetService::GetServiceName() const
{
    return m_Impl->m_ServiceName;
}

CNetServerPool CNetService::GetServerPool()
{
    return m_Impl->m_ServerPool;
}

CNetService::EServiceType CNetService::GetServiceType() const
{
    return m_Impl->m_ServiceType;
}

bool CNetService::IsLoadBalanced() const
{
    return m_Impl->m_ServiceType == CNetService::eLoadBalancedService;
}

void CNetServerPool::StickToServer(const string& host, unsigned short port)
{
    m_Impl->m_EnforcedServer = SServerAddress(host, port);
}

void CNetService::PrintCmdOutput(const string& cmd,
        CNcbiOstream& output_stream, CNetService::ECmdOutputStyle output_style,
        CNetService::EIterationMode iteration_mode)
{
    bool load_balanced = IsLoadBalanced() ?
        output_style != eMultilineOutput_NoHeaders : false;

    for (CNetServiceIterator it = Iterate(iteration_mode); it; ++it) {
        if (load_balanced)
            output_stream << '[' << (*it).GetServerAddress() << ']' << endl;

        switch (output_style) {
        case eSingleLineOutput:
            output_stream << (*it).ExecWithRetry(cmd, false).response << endl;
            break;

        case eUrlEncodedOutput:
            {
                CUrlArgs url_parser((*it).ExecWithRetry(cmd, false).response);

                ITERATE(CUrlArgs::TArgs, field, url_parser.GetArgs()) {
                    output_stream << field->name <<
                            ": " << field->value << endl;
                }
            }
            break;

        default:
            {
                CNetServerMultilineCmdOutput output(
                        (*it).ExecWithRetry(cmd, true));

                if (output_style == eMultilineOutput_NetCacheStyle)
                    output->SetNetCacheCompatMode();

                string line;

                while (output.ReadLine(line))
                    output_stream << line << endl;
            }
        }

        if (load_balanced)
            output_stream << endl;
    }
}

SNetServerInPool* SNetServerPoolImpl::FindOrCreateServerImpl(
        const SServerAddress& server_address)
{
    pair<TNetServerByAddress::iterator, bool> loc(m_Servers.insert(
            TNetServerByAddress::value_type(server_address,
                    (SNetServerInPool*) NULL)));

    if (!loc.second)
        return loc.first->second;

    SNetServerInPool* server = new SNetServerInPool(server_address.host, server_address.port,
            m_Listener->AllocServerProperties().GetPointerOrNull());

    loc.first->second = server;

    return server;
}

CRef<SNetServerInPool> SNetServerPoolImpl::ReturnServer(
        SNetServerInPool* server_impl)
{
    m_RebalanceStrategy->OnResourceRequested();

    CFastMutexGuard server_mutex_lock(m_ServerMutex);

    server_impl->m_ServerPool = this;
    return CRef<SNetServerInPool>(server_impl);
}

CNetServer SNetServiceImpl::GetServer(const SServerAddress& server_address)
{
    m_ServerPool->m_RebalanceStrategy->OnResourceRequested();

    CFastMutexGuard server_mutex_lock(m_ServerPool->m_ServerMutex);

    SNetServerInPool* server = m_ServerPool->FindOrCreateServerImpl(
            m_ServerPool->m_EnforcedServer.host == 0 ?
            server_address : m_ServerPool->m_EnforcedServer);

    server->m_ServerPool = m_ServerPool;

    return new SNetServerImpl(this, server);
}

CNetServer CNetService::GetServer(const string& host,
        unsigned short port)
{
    return m_Impl->GetServer(SServerAddress(host, port));
}

CNetServer CNetService::GetServer(unsigned host, unsigned short port)
{
    return m_Impl->GetServer(SServerAddress(host, port));
}

class SRandomServiceTraversal : public IServiceTraversal
{
public:
    SRandomServiceTraversal(CNetService::TInstance service) :
        m_Service(service)
    {
    }

    virtual CNetServer BeginIteration();
    virtual CNetServer NextServer();

private:
    CNetService m_Service;
    CNetServiceIterator m_Iterator;
};

CNetServer SRandomServiceTraversal::BeginIteration()
{
    return *(m_Iterator = m_Service.Iterate(CNetService::eRandomize));
}

CNetServer SRandomServiceTraversal::NextServer()
{
    return ++m_Iterator ? *m_Iterator : CNetServer();
}

CNetServer::SExecResult SNetServiceImpl::FindServerAndExec(const string& cmd,
        bool multiline_output)
{
    switch (m_ServiceType) {
    default: // CNetService::eServiceNotDefined
        NCBI_THROW_FMT(CNetSrvConnException, eSrvListEmpty,
                m_APIName << ": command '" << cmd <<
                        "' requires a server but none specified");

    case CNetService::eLoadBalancedService:
        {
            CNetServer::SExecResult exec_result;

            SRandomServiceTraversal random_traversal(this);

            IterateUntilExecOK(cmd, multiline_output,
                    exec_result, &random_traversal,
                    SNetServiceImpl::eIgnoreServerErrors,
                    m_Listener);

            return exec_result;
        }

    case CNetService::eSingleServerService:
        {
            CNetServer server(new SNetServerImpl(this,
                    m_ServerPool->ReturnServer(
                    m_DiscoveredServers->m_Servers.front().first)));

            return server.ExecWithRetry(cmd, multiline_output);
        }
    }
}

CNetServer::SExecResult CNetService::FindServerAndExec(const string& cmd,
        bool multiline_output)
{
    return m_Impl->FindServerAndExec(cmd, multiline_output);
}

void CNetService::ExecOnAllServers(const string& cmd)
{
    for (CNetServiceIterator it = Iterate(eIncludePenalized); it; ++it)
        (*it).ExecWithRetry(cmd, false);
}

void SNetServiceImpl::DiscoverServersIfNeeded()
{
    if (m_ServiceType == CNetService::eServiceNotDefined) {
        NCBI_THROW_FMT(CNetSrvConnException, eSrvListEmpty,
            m_APIName << ": service name is not set");
    }

    if (m_ServiceType == CNetService::eLoadBalancedService) {
        // The service is load-balanced, check if rebalancing is required.
        m_ServerPool->m_RebalanceStrategy->OnResourceRequested();
        if (m_ServerPool->m_RebalanceStrategy->NeedRebalance())
            ++m_LatestDiscoveryIteration;

        if (m_DiscoveredServers == NULL ||
            m_DiscoveredServers->m_DiscoveryIteration !=
                m_LatestDiscoveryIteration) {
            // The requested server group either does not exist or
            // does not contain up-to-date server list, thus it needs
            // to be created anew.

            // Query the Load Balancer.
            SERV_ITER srv_it;

            // FIXME Retry logic can be removed as soon as LBSMD with
            // packet compression is installed universally.
            int try_count = TServConn_MaxFineLBNameRetries::GetDefault();
            for (;;) {
                if (!m_NetInfo) {
                    const auto s = m_ServiceName.c_str();
                    m_NetInfo.reset(ConnNetInfo_Create(s), ConnNetInfo_Destroy);
                }

                srv_it = SERV_OpenP(m_ServiceName.c_str(),
                        fSERV_Standalone |
                        fSERV_IncludeStandby |
                        fSERV_IncludeReserved |
                        fSERV_IncludeSuppressed,
                    SERV_LOCALHOST, 0, 0.0, m_NetInfo.get(), NULL, 0, 0 /*false*/,
                    m_ServerPool->m_LBSMAffinity.first.c_str(),
                    m_ServerPool->m_LBSMAffinity.second);

                if (srv_it != 0 || --try_count < 0)
                    break;

                ERR_POST_X(4, "Could not find LB service name '" <<
                    m_ServiceName <<
                        "', will retry after delay");
                SleepMilliSec(m_ConnectionRetryDelay);
            }

            SDiscoveredServers* server_group = m_DiscoveredServers;

            if (server_group != NULL && !server_group->m_Service)
                server_group->Reset(m_LatestDiscoveryIteration);
            else
                // Either the group does not exist or it has been
                // "issued" to the outside callers; allocate a new one.
                server_group = m_DiscoveredServers =
                    AllocServerGroup(m_LatestDiscoveryIteration);

            CFastMutexGuard server_mutex_lock(m_ServerPool->m_ServerMutex);

            const SSERV_Info* sinfo;

            TNetServerList& servers = server_group->m_Servers;
            TNetServerList::size_type number_of_regular_servers = 0;
            TNetServerList::size_type number_of_standby_servers = 0;
            double max_standby_rate = LBSMD_PENALIZED_RATE_BOUNDARY;

            // Fill the 'servers' array in accordance with the layout
            // described above the SDiscoveredServers::m_Servers declaration.
            while ((sinfo = SERV_GetNextInfoEx(srv_it, 0)) != 0)
                if (sinfo->time > 0 && sinfo->time != NCBI_TIME_INFINITE &&
                        sinfo->rate != 0.0) {
                    SNetServerInPool* server = m_ServerPool->
                            FindOrCreateServerImpl(SServerAddress(sinfo->host, sinfo->port));
                    {{
                        CFastMutexGuard guard(server->m_ThrottleLock);
                        server->m_DiscoveredAfterThrottling = true;
                    }}

                    TServerRate server_rate(server, sinfo->rate);

                    if (sinfo->rate > 0)
                        servers.insert(servers.begin() +
                            number_of_regular_servers++, server_rate);
                    else if (sinfo->rate < max_standby_rate ||
                            sinfo->rate <= LBSMD_PENALIZED_RATE_BOUNDARY)
                        servers.push_back(server_rate);
                    else {
                        servers.insert(servers.begin() +
                            number_of_regular_servers, server_rate);
                        if (sinfo->rate == max_standby_rate)
                            ++number_of_standby_servers;
                        else {
                            max_standby_rate = sinfo->rate;
                            number_of_standby_servers = 1;
                        }
                    }
                }

            server_group->m_SuppressedBegin = servers.begin() +
                (number_of_regular_servers > 0 ?
                    number_of_regular_servers : number_of_standby_servers);

            server_mutex_lock.Release();

            SERV_Close(srv_it);
        }
    }
}

void SNetServiceImpl::GetDiscoveredServers(
    CRef<SDiscoveredServers>& discovered_servers)
{
    CFastMutexGuard discovery_mutex_lock(m_DiscoveryMutex);
    DiscoverServersIfNeeded();
    discovered_servers = m_DiscoveredServers;
    discovered_servers->m_Service = this;
}

bool SNetServiceImpl::IsInService(CNetServer::TInstance server)
{
    CRef<SDiscoveredServers> servers;
    GetDiscoveredServers(servers);

    // Find the requested server among the discovered servers.
    ITERATE(TNetServerList, it, servers->m_Servers) {
        if (it->first == server->m_ServerInPool)
            return true;
    }

    return false;
}

void SNetServiceImpl::IterateUntilExecOK(const string& cmd,
    bool multiline_output,
    CNetServer::SExecResult& exec_result,
    IServiceTraversal* service_traversal,
    SNetServiceImpl::EServerErrorHandling error_handling,
    INetServerConnectionListener* conn_listener)
{
    if (conn_listener == NULL)
        conn_listener = m_Listener;

    int retry_count = m_ConnectionMaxRetries;

    const unsigned long retry_delay = m_ConnectionRetryDelay;

    const CTimeout& max_total_time = m_ServerPool->m_MaxTotalTime;
    CDeadline deadline(max_total_time);

    enum EIterationMode {
        eInitialIteration,
        eRetry
    } iteration_mode = eInitialIteration;
    CNetServer server = service_traversal->BeginIteration();

    vector<CNetServer> servers_to_retry;
    unsigned current_server = 0;

    bool skip_server;

    unsigned number_of_servers = 0;
    unsigned ns_with_submits_disabled = 0;
    unsigned servers_throttled = 0;
    bool blob_not_found = false;

    STimeout* timeout = retry_count <= 0 && !m_UseSmartRetries ?
            NULL : &m_ServerPool->m_FirstServerTimeout;

    CMessageListener_Basic err_listener;

    for (;;) {
        skip_server = false;

        try {
            server->ConnectAndExec(cmd, multiline_output, exec_result,
                    timeout, NULL, conn_listener);
            return;
        }
        catch (CNetCacheBlobTooOldException& /*ex rethrown*/) {
            throw;
        }
        catch (CNetCacheException& ex) {
            if (retry_count <= 0 && !m_UseSmartRetries)
                throw;
            if (ex.GetErrCode() == CNetCacheException::eBlobNotFound) {
                blob_not_found = true;
                skip_server = true;
            } else if (error_handling == eRethrowServerErrors)
                throw;
            else
                conn_listener->OnWarning(ex.GetMsg(), server);

            err_listener.PostMessage(CMessage_Basic(ex.GetMsg(), eDiag_Warning));
        }
        catch (CNetScheduleException& ex) {
            if (retry_count <= 0 && !m_UseSmartRetries)
                throw;
            if (ex.GetErrCode() == CNetScheduleException::eSubmitsDisabled) {
                ++ns_with_submits_disabled;
                skip_server = true;
            } else if (error_handling == eRethrowServerErrors)
                throw;
            else
                conn_listener->OnWarning(ex.GetMsg(), server);

            err_listener.PostMessage(CMessage_Basic(ex.GetMsg(), eDiag_Warning));
        }
        catch (CNetSrvConnException& ex) {
            if (retry_count <= 0 && !m_UseSmartRetries)
                throw;
            switch (ex.GetErrCode()) {
            case CNetSrvConnException::eReadTimeout:
                break;
            case CNetSrvConnException::eConnectionFailure:
                conn_listener->OnWarning(ex.GetMsg(), server);
                break;

            case CNetSrvConnException::eServerThrottle:
                ++servers_throttled;
                break;

            default:
                throw;
            }

            err_listener.PostMessage(CMessage_Basic(ex.GetMsg(), eDiag_Warning));
        }

        ++number_of_servers;

        if (iteration_mode == eInitialIteration) {
            if (!skip_server)
                servers_to_retry.push_back(server);
            server = service_traversal->NextServer();
        } else {
            if (!skip_server)
                ++current_server;
            else
                servers_to_retry.erase(servers_to_retry.begin() +
                        current_server);

            if (current_server < servers_to_retry.size())
                server = servers_to_retry[current_server];
            else
                server = NULL;
        }

        if (!deadline.IsInfinite() &&
                deadline.GetRemainingTime().GetAsMilliSeconds() <= (server ? 0 : retry_delay)) {
            NCBI_THROW_FMT(CNetSrvConnException, eReadTimeout, "Exceeded max_connection_time=" <<
                    max_total_time.GetAsMilliSeconds() << "; cmd=[" << cmd << "]");
        }

        if (!server) {
            if (number_of_servers == ns_with_submits_disabled) {
                NCBI_THROW_FMT(CNetSrvConnException, eSrvListEmpty,
                        "Cannot execute ["  << cmd <<
                        "]: all NetSchedule servers are "
                        "in REFUSESUBMITS mode.");
            }
            if (number_of_servers == servers_throttled) {
                NCBI_THROW_FMT(CNetSrvConnException, eSrvListEmpty,
                        "Cannot execute ["  << cmd <<
                        "]: all servers are throttled.");
            }
            if (retry_count <= 0 || servers_to_retry.empty()) {
                if (blob_not_found) {
                    NCBI_THROW_FMT(CNetCacheException, eBlobNotFound,
                            "Cannot execute ["  << cmd << "]: blob not found.");
                }
                NCBI_THROW_FMT(CNetSrvConnException, eSrvListEmpty,
                        "Unable to execute [" << cmd <<
                        "] on any of the discovered servers.");
            }

            for (size_t i = 0; i < err_listener.Count(); ++i) {
                LOG_POST(Warning << err_listener.GetMessage(i).GetText());
            }

            err_listener.Clear();
            LOG_POST(Warning << "Unable to send [" << cmd << "] to any "
                    "of the discovered servers; will retry after delay.");

            SleepMilliSec(retry_delay);

            number_of_servers = 0;
            ns_with_submits_disabled = 0;
            servers_throttled = 0;

            iteration_mode = eRetry;
            server = servers_to_retry[current_server = 0];
        }

        --retry_count;

        timeout = NULL;
    }
}

shared_ptr<SNetServiceImpl::CTry> SNetServiceImpl::GetTryGuard()
{
    return make_shared<CTry>(this);
}

void SNetServerPoolImpl::ResetServerConnections()
{
    CFastMutexGuard server_mutex_lock(m_ServerMutex);

    NON_CONST_ITERATE(TNetServerByAddress, it, m_Servers) {
        it->second->m_CurrentConnectionGeneration.Add(1);
    }
}

SNetServerPoolImpl::~SNetServerPoolImpl()
{
    // Clean up m_Servers
    NON_CONST_ITERATE(TNetServerByAddress, it, m_Servers) {
        delete it->second;
    }

    if (m_LBSMAffinity.second) free(const_cast<char*>(m_LBSMAffinity.second));
}

SNetServiceImpl::~SNetServiceImpl()
{
    delete m_DiscoveredServers;

    // Clean up m_ServerGroupPool
    SDiscoveredServers* server_group = m_ServerGroupPool;
    while (server_group != NULL) {
        SDiscoveredServers* next_group = server_group->m_NextGroupInPool;
        delete server_group;
        server_group = next_group;
    }
}

void CNetServerPool::SetCommunicationTimeout(const STimeout& to)
{
    m_Impl->m_CommTimeout = to;
}
const STimeout& CNetServerPool::GetCommunicationTimeout() const
{
    return m_Impl->m_CommTimeout;
}

CNetServiceIterator CNetService::Iterate(CNetService::EIterationMode mode)
{
    CRef<SDiscoveredServers> servers;
    m_Impl->GetDiscoveredServers(servers);

    if (mode != eIncludePenalized) {
        if (servers->m_Servers.begin() < servers->m_SuppressedBegin) {
            if (mode == eSortByLoad)
                return new SNetServiceIterator_OmitPenalized(servers);
            else
                return new SNetServiceIterator_RandomPivot(servers);
        }
    } else
        if (!servers->m_Servers.empty())
            return new SNetServiceIteratorImpl(servers);

    NCBI_THROW(CNetSrvConnException, eSrvListEmpty,
        "Couldn't find any available servers for the " +
        m_Impl->m_ServiceName + " service.");
}

CNetServiceIterator CNetService::Iterate(CNetServer::TInstance priority_server)
{
    CRef<SDiscoveredServers> servers;
    m_Impl->GetDiscoveredServers(servers);

    // Find the requested server among the discovered servers.
    ITERATE(TNetServerList, it, servers->m_Servers) {
        if (it->first == priority_server->m_ServerInPool)
            return new SNetServiceIterator_Circular(servers, it);
    }

    if (!servers->m_Servers.empty())
        // The requested server is not found in this service,
        // however there are servers, so return them.
        return new SNetServiceIteratorImpl(servers);

    NCBI_THROW(CNetSrvConnException, eSrvListEmpty,
        "Couldn't find any available servers for the " +
        m_Impl->m_ServiceName + " service.");
}

CNetServiceIterator CNetService::IterateByWeight(const string& key)
{
    CRef<SDiscoveredServers> servers;
    m_Impl->GetDiscoveredServers(servers);

    if (servers->m_Servers.begin() == servers->m_SuppressedBegin) {
        NCBI_THROW(CNetSrvConnException, eSrvListEmpty,
            "Couldn't find any available servers for the " +
            m_Impl->m_ServiceName + " service.");
    }

    CChecksum key_crc32(CChecksum::eCRC32);

    key_crc32.AddChars(key.data(), key.length());

    return new SNetServiceIterator_Weighted(servers, key_crc32.GetChecksum());
}

CNetServiceIterator CNetService::ExcludeServer(CNetServer::TInstance server)
{
    CRef<SDiscoveredServers> servers;
    m_Impl->GetDiscoveredServers(servers);

    // Find the requested server among the discovered servers.
    ITERATE(TNetServerList, it, servers->m_Servers) {
        if (it->first == server->m_ServerInPool) {
            // The server is found. Make an iterator and
            // skip to the next server (the iterator may become NULL).
            CNetServiceIterator circular_iter(
                    new SNetServiceIterator_Circular(servers, it));
            return ++circular_iter;
        }
    }

    // The requested server is not found in this service, so
    // return the rest of servers or NULL if there are none.
    return !servers->m_Servers.empty() ?
            new SNetServiceIteratorImpl(servers) : NULL;
}

CNetServiceIterator CNetService::FindServer(INetServerFinder* finder,
    CNetService::EIterationMode mode)
{
    string error_messages;

    CNetServiceIterator it = Iterate(mode);

    for (; it; ++it) {
        try {
            if (finder->Consider(*it))
                break;
        }
        catch (CNetServiceException& ex) {
            if (ex.GetErrCode() != CNetServiceException::eCommunicationError)
                throw;

            if (!error_messages.empty())
                error_messages += '\n';

            error_messages += (*it)->m_ServerInPool->m_Address.AsString();
            error_messages += ": ";
            error_messages += ex.what();
        }
        catch (CIO_Exception& ex) {
            if (!error_messages.empty())
                error_messages += '\n';

            error_messages += (*it)->m_ServerInPool->m_Address.AsString();
            error_messages += ": ";
            error_messages += ex.what();
        }
    }

    if (!error_messages.empty()) {
        LOG_POST(error_messages);
    }

    return it;
}

CNetService CNetService::Clone(const string& name)
{
    _ASSERT(m_Impl);
    return name == m_Impl->m_ServiceName ? m_Impl :
        new SNetServiceImpl(name, m_Impl);
}

CNetService SNetServiceMap::GetServiceByName(const string& service_name,
        SNetServiceImpl* prototype)
{
    CFastMutexGuard guard(m_ServiceMapMutex);
    return GetServiceByNameImpl(service_name, prototype);
}

CNetService SNetServiceMap::GetServiceByNameImpl(const string& service_name,
        SNetServiceImpl* prototype)
{
    pair<TNetServiceByName::iterator, bool> loc(m_ServiceByName.insert(
            TNetServiceByName::value_type(service_name, CNetService())));

    return !loc.second ? loc.first->second :
            (loc.first->second =
                    new SNetServiceImpl(service_name, prototype));
}

bool SNetServiceMap::IsAllowed(const string& service_name) const
{
    // Not restricted or found
    return !m_Restricted || m_Allowed.count(service_name);
}

bool SNetServiceMap::IsAllowed(CNetServer::TInstance server,
        SNetServiceImpl* prototype)
{
    if (!m_Restricted) return true;

    CFastMutexGuard guard(m_ServiceMapMutex);

    // Check if server belongs to some allowed service
    for (auto& service_name: m_Allowed) {
        CNetService service(GetServiceByNameImpl(service_name, prototype));

        if (service->IsInService(server)) return true;
    }

    return false;
}

void SNetServiceMap::AddToAllowed(const string& service_name)
{
    m_Allowed.insert(service_name);
}

CJsonNode g_ExecToJson(IExecToJson& exec_to_json, CNetService service,
        CNetService::EServiceType service_type,
        CNetService::EIterationMode iteration_mode)
{
    if (service_type == CNetService::eSingleServerService)
        return exec_to_json.ExecOn(service.Iterate().GetServer());

    CJsonNode result(CJsonNode::NewObjectNode());

    for (CNetServiceIterator it = service.Iterate(iteration_mode); it; ++it)
        result.SetByKey((*it).GetServerAddress(), exec_to_json.ExecOn(*it));

    return result;
}

CNetService g_DiscoverService(const string& service_name,
        const string& client_name)
{
    struct SNoOpConnectionListener : public INetServerConnectionListener
    {
        CRef<INetServerProperties> AllocServerProperties() override
        {
            return CRef<INetServerProperties>(new INetServerProperties);
        }

        void OnInit(CObject*, ISynRegistry&, const string&) override {}
        void OnConnected(CNetServerConnection&) override {}
        void OnError(const string&, CNetServer&) override {}
        void OnWarning(const string&, CNetServer&) override {}
    };

    CNetService service(new SNetServiceImpl("Discovery", client_name, new SNoOpConnectionListener));

    static const char* const sections[] = {"discovery", nullptr};

    service->Init(nullptr, service_name, nullptr, "", sections);

    return service;
}

END_NCBI_SCOPE
