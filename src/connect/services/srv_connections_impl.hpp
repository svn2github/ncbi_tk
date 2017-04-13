#ifndef CONNECT_SERVICES___SRV_CONNECTIONS_IMPL__HPP
#define CONNECT_SERVICES___SRV_CONNECTIONS_IMPL__HPP

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
 * Authors:  Dmitry Kazimirov
 *
 * File Description:
 *
 */

#include "netservice_params.hpp"

#include <connect/services/netservice_api.hpp>

#include <corelib/ncbimtx.hpp>

#include <util/ncbi_url.hpp>

BEGIN_NCBI_SCOPE

struct SNetServerMultilineCmdOutputImpl : public CObject
{
    SNetServerMultilineCmdOutputImpl(
        const CNetServer::SExecResult& exec_result) :
            m_Connection(exec_result.conn),
            m_FirstOutputLine(exec_result.response),
            m_FirstLineConsumed(false),
            m_NetCacheCompatMode(false),
            m_ReadCompletely(false)
    {
    }

    virtual ~SNetServerMultilineCmdOutputImpl();

    void SetNetCacheCompatMode() { m_NetCacheCompatMode = true; }

    CNetServerConnection m_Connection;

    string m_FirstOutputLine;
    bool m_FirstLineConsumed;

    bool m_NetCacheCompatMode;
    bool m_ReadCompletely;
};

class INetServerProperties : public CObject
{
};

struct INetServerConnectionListener : CObject
{
    virtual CRef<INetServerProperties> AllocServerProperties() = 0;

    // Event handlers.
    virtual void OnPreInit(CObject* api_impl, ISynRegistry& registry, string* section, string& client_name);
    virtual void OnInit(CObject* api_impl, ISynRegistry& registry, const string& section) = 0;
    virtual void OnConnected(CNetServerConnection& connection) = 0;
    virtual void OnError(const string& err_msg, CNetServer& server) = 0;
    virtual void OnWarning(const string& warn_msg, CNetServer& server) = 0;

    CRef<INetEventHandler> m_EventHandler;
};

struct SNetServerConnectionImpl : public CObject
{
    SNetServerConnectionImpl(SNetServerImpl* pool);

    // Return this connection to the pool.
    virtual void DeleteThis();

    void WriteLine(const string& line);
    void ReadCmdOutputLine(string& result,
            bool multiline_output,
            INetServerConnectionListener* conn_listener = NULL);

    void Close();
    void Abort();

    virtual ~SNetServerConnectionImpl();

    // The server this connection is connected to.
    CNetServer m_Server;
    CAtomicCounter::TValue m_Generation;
    SNetServerConnectionImpl* m_NextFree;

    CSocket m_Socket;
};

class INetServerExecHandler
{
public:
    virtual ~INetServerExecHandler() {}

    virtual void Exec(CNetServerConnection::TInstance conn_impl,
            STimeout* timeout) = 0;
};

class INetServerExecListener
{
public:
    virtual ~INetServerExecListener() {}

    virtual void OnExec(CNetServerConnection::TInstance conn_impl,
            const string& cmd) = 0;
};

// A host:port pair.
struct SServerAddress {
    SServerAddress(unsigned h, unsigned short p) : host(h), port(p), name(h) {}
    SServerAddress(const string &n, unsigned short p)
        : host(g_NetService_gethostbyname(n)), port(p), name(n, host) {}

    bool operator ==(const SServerAddress& h) const
    {
        return host == h.host && port == h.port;
    }

    bool operator <(const SServerAddress& right) const
    {
        int cmp = int(host) - int(right.host);
        return cmp < 0 || (cmp == 0 && port < right.port);
    }

    string AsString() const
    {
        string address(name.get(host));
        address += ':';
        address += NStr::UIntToString(port);

        return address;
    }

    unsigned host;
    unsigned short port;

private:
    struct SName
    {
        SName() : host(0) {}
        SName(unsigned h) : host(h) {}
        SName(const string &n, unsigned h) : name(n), host(h) {}

        string get(unsigned h)
        {
            // Name was not looked up yet or host changed
            if (name.empty() || host != h) {
                host = h;
                name = g_NetService_gethostnamebyaddr(h);
            }

            return name;
        }

        string name;
        unsigned host;
    };

    mutable SName name;
};

struct SNetServerInPool : public CObject
{
    SNetServerInPool(unsigned host, unsigned short port,
            INetServerProperties* server_properties);

    // Releases a reference to the parent service object,
    // and if that was the last reference, the service object
    // will be deleted, which in turn will lead to this server
    // object being deleted too (along with all other server
    // objects that the parent service object may contain).
    virtual void DeleteThis();

    virtual ~SNetServerInPool();

    // Server throttling implementation.
    enum EConnOpResult {
        eCOR_Success,
        eCOR_Failure
    };

    void AdjustThrottlingParameters(EConnOpResult op_result);
    void CheckIfThrottled();
    void ResetThrottlingParameters();

    // A smart pointer to the server pool object that contains
    // this NetServer. Valid only when this object is returned
    // to an outside caller via ReturnServer() or GetServer().
    CNetServerPool m_ServerPool;

    SServerAddress m_Address;

    // API-specific server properties.
    CRef<INetServerProperties> m_ServerProperties;

    CAtomicCounter m_CurrentConnectionGeneration;
    SNetServerConnectionImpl* m_FreeConnectionListHead;
    int m_FreeConnectionListSize;
    CFastMutex m_FreeConnectionListLock;

    int m_NumberOfConsecutiveIOFailures;
    EConnOpResult m_IOFailureRegister[CONNECTION_ERROR_HISTORY_MAX];
    int m_IOFailureRegisterIndex;
    int m_IOFailureCounter;
    bool m_Throttled;
    bool m_DiscoveredAfterThrottling;
    string m_ThrottleMessage;
    CTime m_ThrottledUntil;
    CFastMutex m_ThrottleLock;
    Uint4 m_RankBase;
};

struct SNetServerInfoImpl : public CObject
{
    typedef CUrlArgs::TArgs TAttributes;

    auto_ptr<CUrlArgs> m_URLParser;

    TAttributes m_FreeFormVersionAttributes;

    TAttributes* m_Attributes;
    TAttributes::const_iterator m_NextAttribute;

    SNetServerInfoImpl(const string& version_string);
};

struct SNetServerImpl : public CObject
{
    struct SConnectDeadline;

    SNetServerImpl(CNetService::TInstance service,
            SNetServerInPool* server_in_pool) :
        m_Service(service),
        m_ServerInPool(server_in_pool)
    {
    }

    CNetServerConnection GetConnectionFromPool();

    void TryExec(INetServerExecHandler& handler,
            STimeout* timeout = NULL,
            INetServerConnectionListener* conn_listener = NULL);

    void ConnectAndExec(const string& cmd,
            bool multiline_output,
            CNetServer::SExecResult& exec_result,
            STimeout* timeout = NULL,
            INetServerExecListener* exec_listener = NULL,
            INetServerConnectionListener* conn_listener = NULL);

    static void ConnectImpl(CSocket&, SConnectDeadline&, const SServerAddress&,
            const SServerAddress&);

    CNetService m_Service;
    CRef<SNetServerInPool> m_ServerInPool;

private:
    CNetServerConnection Connect(STimeout* timeout,
            INetServerConnectionListener* conn_listener);
};

class CTimeoutKeeper
{
public:
    CTimeoutKeeper(CSocket* sock, STimeout* timeout)
    {
        if (timeout == NULL)
            m_Socket = NULL;
        else {
            m_Socket = sock;
            m_ReadTimeout = *sock->GetTimeout(eIO_Read);
            m_WriteTimeout = *sock->GetTimeout(eIO_Write);
            sock->SetTimeout(eIO_ReadWrite, timeout);
        }
    }

    ~CTimeoutKeeper()
    {
        if (m_Socket != NULL) {
            m_Socket->SetTimeout(eIO_Read, &m_ReadTimeout);
            m_Socket->SetTimeout(eIO_Write, &m_WriteTimeout);
        }
    }

    CSocket* m_Socket;
    STimeout m_ReadTimeout;
    STimeout m_WriteTimeout;
};

END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES___SRV_CONNECTIONS_IMPL__HPP */
