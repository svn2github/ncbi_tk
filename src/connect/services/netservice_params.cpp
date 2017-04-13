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
 *   Definitions of the configuration parameters for connect/services.
 *
 * Authors:
 *   Dmitry Kazimirov
 *
 */

#include <ncbi_pch.hpp>

#include "netservice_params.hpp"

BEGIN_NCBI_SCOPE


NCBI_PARAM_DEF(bool, netservice_api, use_linger2, false);
NCBI_PARAM_DEF(unsigned int, netservice_api, connection_max_retries, 4);
NCBI_PARAM_DEF(string, netservice_api, retry_delay,
    NCBI_AS_STRING(RETRY_DELAY_DEFAULT));
NCBI_PARAM_DEF(string, netservice_api, communication_timeout,
    NCBI_AS_STRING(COMMUNICATION_TIMEOUT_DEFAULT));
NCBI_PARAM_DEF(int, netservice_api, max_find_lbname_retries, 3);
NCBI_PARAM_DEF(string, netcache_api, fallback_server, "");
NCBI_PARAM_DEF(int, netservice_api, max_connection_pool_size, 0); // unlimited
NCBI_PARAM_DEF(bool, netservice_api, connection_data_logging, false);
NCBI_PARAM_DEF(unsigned, server, max_wait_for_servers, 24 * 60 * 60);
NCBI_PARAM_DEF(bool, server, stop_on_job_errors, true);
NCBI_PARAM_DEF(bool, server, allow_implicit_job_return, false);


static bool s_DefaultCommTimeout_Initialized = false;
static STimeout s_DefaultCommTimeout;

unsigned long s_SecondsToMilliseconds(
    const string& seconds, unsigned long default_value)
{
    const signed char* ch = (const signed char*) seconds.c_str();

    unsigned long result = 0;
    register int digit = *ch - '0';

    if (digit >= 0 && digit <= 9) {
        do
            result = result * 10 + digit;
        while ((digit = *++ch - '0') >= 0 && digit <= 9);
        if (*ch == '\0')
            return result * 1000;
    }

    if (*ch != '.')
        return default_value;

    static unsigned const powers_of_ten[4] = {1, 10, 100, 1000};
    int exponent = 3;

    while ((digit = *++ch - '0') >= 0 && digit <= 9) {
        if (--exponent < 0)
            return default_value;
        result = result * 10 + digit;
    }

    return *ch == '\0' ? result * powers_of_ten[exponent] : default_value;
}

STimeout s_GetDefaultCommTimeout()
{
    if (s_DefaultCommTimeout_Initialized)
        return s_DefaultCommTimeout;
    NcbiMsToTimeout(&s_DefaultCommTimeout,
        s_SecondsToMilliseconds(TServConn_CommTimeout::GetDefault(),
            SECONDS_DOUBLE_TO_MS_UL(COMMUNICATION_TIMEOUT_DEFAULT)));
    s_DefaultCommTimeout_Initialized = true;
    return s_DefaultCommTimeout;
}

unsigned long s_GetRetryDelay()
{
    static unsigned long retry_delay;
    static bool retry_delay_is_set = false;

    if (!retry_delay_is_set) {
        retry_delay =
            s_SecondsToMilliseconds(TServConn_RetryDelay::GetDefault(),
                SECONDS_DOUBLE_TO_MS_UL(RETRY_DELAY_DEFAULT));

        retry_delay_is_set = true;
    }

    return retry_delay;
}

template <class T>
shared_ptr<T> s_MakeShared(T* ptr, EOwnership ownership)
{
    if (ownership == eTakeOwnership) return shared_ptr<T>(ptr);
    return { shared_ptr<T>(), ptr };
}

CConfigRegistry::CConfigRegistry(CConfig* config, EOwnership ownership) :
    m_Config(s_MakeShared(config, ownership))
{
    _ASSERT(config);
}

bool CConfigRegistry::x_Empty(TFlags flags) const
{
    NCBI_ALWAYS_TROUBLE("Not implemented");
    return false; // Not reached
}

const string& CConfigRegistry::x_Get(const string& section, const string& name, TFlags) const
{
    return m_Config->GetString(section, name, CConfig::eErr_NoThrow);
}

bool CConfigRegistry::x_HasEntry(const string& section, const string& name, TFlags flags) const
{
    try {
        m_Config->GetString(section, name, CConfig::eErr_Throw);
    }
    catch (CConfigException& ex) {
        if (ex.GetErrCode() == CConfigException::eParameterMissing) return false;
        throw;
    }

    return true;
}

const string& CConfigRegistry::x_GetComment(const string& section, const string& name, TFlags flags) const
{
    NCBI_ALWAYS_TROUBLE("Not implemented");
    return kEmptyStr; // Not reached
}

void CConfigRegistry::x_Enumerate(const string& section, list<string>& entries, TFlags flags) const
{
    NCBI_ALWAYS_TROUBLE("Not implemented");
}

CSynRegistryImpl::CSynRegistryImpl(IRegistry* registry, EOwnership ownership) :
    m_Registry(s_MakeShared(registry, ownership))
{
}

template <typename TType, class TFunc>
TType s_Iterate(initializer_list<string> list, TType default_value, TFunc func)
{
    _ASSERT(list.size());

    const TType empty_value{};

    for (const auto& element : list) {
        auto value = func(element, empty_value);

        if (value != empty_value) return value;
    }

    return default_value;
}

template <typename TType>
TType ISynRegistry::GetImpl(const string& section, initializer_list<string> names, TType default_value)
{
    auto f = [&](const string& name, TType value)
    {
        return GetValue(section, name.c_str(), value);
    };

    return s_Iterate(names, default_value, f);
}

template <typename TType>
TType ISynRegistry::GetImpl(initializer_list<string> sections, const char* name, TType default_value)
{
    auto f = [&](const string& section, TType value)
    {
        return GetValue(section, name, value);
    };

    return s_Iterate(sections, default_value, f);
}

bool ISynRegistry::Has(const string& section, initializer_list<string> names)
{
    auto f = [&](const string& name, bool)
    {
        return Has(section, name.c_str());
    };

    return s_Iterate(names, false, f);
}

bool ISynRegistry::Has(initializer_list<string> sections, const char* name)
{
    auto f = [&](const string& section, bool)
    {
        return Has(section, name);
    };

    return s_Iterate(sections, false, f);
}

template string ISynRegistry::GetImpl(const string& section, initializer_list<string> names, string default_value);
template bool   ISynRegistry::GetImpl(const string& section, initializer_list<string> names, bool default_value);
template int    ISynRegistry::GetImpl(const string& section, initializer_list<string> names, int default_value);
template double ISynRegistry::GetImpl(const string& section, initializer_list<string> names, double default_value);
template string ISynRegistry::GetImpl(initializer_list<string> sections, const char* name, string default_value);
template bool   ISynRegistry::GetImpl(initializer_list<string> sections, const char* name, bool default_value);
template int    ISynRegistry::GetImpl(initializer_list<string> sections, const char* name, int default_value);
template double ISynRegistry::GetImpl(initializer_list<string> sections, const char* name, double default_value);

END_NCBI_SCOPE
