#ifndef CONNECT_SERVICES__NETSERVICE_PARAMS_HPP
#define CONNECT_SERVICES__NETSERVICE_PARAMS_HPP

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
 * File Description:
 *   Declarations of the configuration parameters for connect/services.
 *
 * Authors:
 *   Dmitry Kazimirov
 *
 */

#include <connect/ncbi_types.h>

#include <corelib/ncbi_param.hpp>
#include <corelib/ncbi_config.hpp>

// Delay between two successive connection attempts in seconds.
#define RETRY_DELAY_DEFAULT 1.0

// Connection timeout, which is used as the eIO_Open timeout
// in CSocket::Connect.
#define CONNECTION_TIMEOUT_DEFAULT 2.0

// Communication timeout, upon reaching which the connection
// is closed on the client side.
#define COMMUNICATION_TIMEOUT_DEFAULT 12.0

// How many seconds the API should wait before attempting to
// connect to a misbehaving server again.
#define THROTTLE_RELAXATION_PERIOD_DEFAULT 0

// The parameters below describe different conditions that
// trigger server throttling.
#define THROTTLE_BY_SUBSEQUENT_CONNECTION_FAILURES_DEFAULT 0

// Connection failure rate, which is when reached, triggers
// server throttling.
#define THROTTLE_BY_ERROR_RATE_DEFAULT_NUMERATOR 0

// THROTTLE_BY_ERROR_RATE_DEFAULT_DENOMINATOR cannot be greater
// than CONNECTION_ERROR_HISTORY_MAX.
#define THROTTLE_BY_ERROR_RATE_DEFAULT_DENOMINATOR 0

// Whether to check with LBSMD before re-enabling the server.
#define THROTTLE_HOLD_UNTIL_ACTIVE_IN_LB_DEFAULT false

// The size of an internal array, which is used for calculation
// of the connection failure rate.
#define CONNECTION_ERROR_HISTORY_MAX 128

// Maximum cumulative query time in seconds.
// The parameter is ignored if it's zero.
#define MAX_CONNECTION_TIME_DEFAULT 0.0

#define FIRST_SERVER_TIMEOUT_DEFAULT 0.3

#define COMMIT_JOB_INTERVAL_DEFAULT 2

BEGIN_NCBI_SCOPE

NCBI_PARAM_DECL(string, netservice_api, communication_timeout);
typedef NCBI_PARAM_TYPE(netservice_api, communication_timeout)
    TServConn_CommTimeout;

NCBI_PARAM_DECL(unsigned int, netservice_api, connection_max_retries);
typedef NCBI_PARAM_TYPE(netservice_api, connection_max_retries)
    TServConn_ConnMaxRetries;

NCBI_PARAM_DECL(string, netservice_api, retry_delay);
typedef NCBI_PARAM_TYPE(netservice_api, retry_delay)
    TServConn_RetryDelay;

NCBI_PARAM_DECL(bool, netservice_api, use_linger2);
typedef NCBI_PARAM_TYPE(netservice_api, use_linger2)
    TServConn_UserLinger2;

NCBI_PARAM_DECL(int, netservice_api, max_find_lbname_retries);
typedef NCBI_PARAM_TYPE(netservice_api, max_find_lbname_retries)
    TServConn_MaxFineLBNameRetries;

NCBI_PARAM_DECL(string, netcache_api, fallback_server);
typedef NCBI_PARAM_TYPE(netcache_api, fallback_server)
    TCGI_NetCacheFallbackServer;

NCBI_PARAM_DECL(int, netservice_api, max_connection_pool_size);
typedef NCBI_PARAM_TYPE(netservice_api, max_connection_pool_size)
    TServConn_MaxConnPoolSize;

NCBI_PARAM_DECL(bool, netservice_api, connection_data_logging);
typedef NCBI_PARAM_TYPE(netservice_api, connection_data_logging)
    TServConn_ConnDataLogging;

NCBI_PARAM_DECL(bool, netservice_api, allow_xsite_conn);
typedef NCBI_PARAM_TYPE(netservice_api, allow_xsite_conn)
    TServConn_AllowXsiteConn;

// Worker node-specific parameters

// Determine how long the worker node should wait for the
// configured NetSchedule servers to appear before quitting.
// When none of the servers are accessible, the worker node
// will keep trying to connect until it succeeds, or the specified
// timeout elapses.
NCBI_PARAM_DECL(unsigned, server, max_wait_for_servers);
typedef NCBI_PARAM_TYPE(server, max_wait_for_servers)
    TWorkerNode_MaxWaitForServers;

NCBI_PARAM_DECL(bool, server, stop_on_job_errors);
typedef NCBI_PARAM_TYPE(server, stop_on_job_errors)
    TWorkerNode_StopOnJobErrors;

NCBI_PARAM_DECL(bool, server, allow_implicit_job_return);
typedef NCBI_PARAM_TYPE(server, allow_implicit_job_return)
    TWorkerNode_AllowImplicitJobReturn;

#define SECONDS_DOUBLE_TO_MS_UL(seconds) ((unsigned long) (seconds * 1000.0))

NCBI_XCONNECT_EXPORT unsigned long s_SecondsToMilliseconds(
    const string& seconds, unsigned long default_value);
NCBI_XCONNECT_EXPORT STimeout s_GetDefaultCommTimeout();
NCBI_XCONNECT_EXPORT void s_SetDefaultCommTimeout(const STimeout& tm);
NCBI_XCONNECT_EXPORT unsigned long s_GetRetryDelay();


class NCBI_XNCBI_EXPORT CConfigRegistry : public IRegistry
{
public:
    CConfigRegistry(CConfig* config);

private:
    bool x_Empty(TFlags flags) const override;
    const string& x_Get(const string& section, const string& name, TFlags flags) const override;
    bool x_HasEntry(const string& section, const string& name, TFlags flags) const override;
    const string& x_GetComment(const string& section, const string& name, TFlags flags) const override;
    void x_Enumerate(const string& section, list<string>& entries, TFlags flags) const override;

    CConfig* m_Config;
};

class NCBI_XNCBI_EXPORT CSynonymsRegistry
{
public:
    CSynonymsRegistry(IRegistry& registry) : m_Registry(registry) {}

    template <typename TType>
    TType Get(string section, string name, TType default_value)
    {
        return m_Registry.GetValue(section, name, default_value, IRegistry::eThrow);
    }

    template <typename TType>
    TType Get(string section, vector<string> names, TType default_value)
    {
        return Get(vector<string>({section}), names, default_value);
    }

    template <typename TType>
    TType Get(vector<string> sections, string name, TType default_value)
    {
        return Get(sections, vector<string>({name}), default_value);
    }

    template <typename TType>
    TType Get(vector<string> sections, vector<string> names, TType default_value)
    {
        _ASSERT(!sections.empty());
        _ASSERT(!names.empty());

        const TType empty_value{};

        auto section = sections.begin();
        auto name = names.begin();

        for (;;) {
            auto value = Get(*section, *name, empty_value);

            if (value != empty_value) return value;
            if (++name != names.end()) continue;
            if (++section == sections.end()) return default_value;

            name = names.begin();
        }
    }

private:
    IRegistry& m_Registry;
};

END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES__NETSERVICE_PARAMS_HPP */
