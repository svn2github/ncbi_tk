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
* Author: Eugene Vasilchenko, Denis Vakatov, Anatoliy Kuznetsov
*
* File Description:
*   Definition CGI application class and its context class.
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/rwstream.hpp>
#include <corelib/ncbi_param.hpp>
#include <corelib/stream_utils.hpp>
#include <cgi/cgiapp.hpp>
#include <cgi/cgictx.hpp>
#include <cgi/cgi_exception.hpp>
#include <corelib/ncbi_system.hpp> // for SuppressSystemMessageBox

#ifdef NCBI_OS_UNIX
#  include <unistd.h>
#endif


BEGIN_NCBI_SCOPE


NCBI_PARAM_DECL(bool, CGI, Print_Http_Referer);
NCBI_PARAM_DEF_EX(bool, CGI, Print_Http_Referer, false, eParam_NoThread,
                  CGI_PRINT_HTTP_REFERER);
static NCBI_PARAM_TYPE(CGI, Print_Http_Referer) s_PrintRefererParam;


///////////////////////////////////////////////////////
// IO streams with byte counting for CGI applications
//


class CCGIStreamReader : public IReader
{
public:
    CCGIStreamReader(istream& is) : m_IStr(is) { }

    virtual ERW_Result Read(void*   buf,
                            size_t  count,
                            size_t* bytes_read = 0);
    virtual ERW_Result PendingCount(size_t* count)
    { return eRW_NotImplemented; }

protected:
    istream& m_IStr;
};


ERW_Result CCGIStreamReader::Read(void*   buf,
                                  size_t  count,
                                  size_t* bytes_read)
{
    streamsize x_read = CStreamUtils::Readsome(m_IStr, (char*)buf, count);
    ERW_Result result;
    if (x_read < 0) {
        result = eRW_Error;
    }
    else if (x_read > 0) {
        result = eRW_Success;
    }
    else {
        result = eRW_Eof;
    }
    if (bytes_read) {
        *bytes_read = x_read < 0 ? 0 : x_read;
    }
    return result;
}


class CCGIStreamWriter : public IWriter
{
public:
    CCGIStreamWriter(ostream& os) : m_OStr(os) { }

    virtual ERW_Result Write(const void* buf,
                             size_t      count,
                             size_t*     bytes_written = 0);

    virtual ERW_Result Flush(void)
    { return m_OStr.flush() ? eRW_Success : eRW_Error; }

protected:
    ostream& m_OStr;
};


ERW_Result CCGIStreamWriter::Write(const void* buf,
                                   size_t      count,
                                   size_t*     bytes_written)
{
    ERW_Result result;
    if (!m_OStr.write((char*)buf, count)) {
        result = eRW_Error;
    }
    else {
        result = eRW_Success;
    }
    if (bytes_written) {
        *bytes_written = result == eRW_Success ? count : 0;
    }
    return result;
}


///////////////////////////////////////////////////////
// CCgiApplication
//


CCgiApplication* CCgiApplication::Instance(void)
{
    return dynamic_cast<CCgiApplication*> (CParent::Instance());
}


int CCgiApplication::Run(void)
{
    // Value to return from this method Run()
    int result;

    // Try to run as a Fast-CGI loop
    if ( x_RunFastCGI(&result) ) {
        return result;
    }

    /// Run as a plain CGI application

    // Make sure to restore old diagnostic state after the Run()
    CDiagRestorer diag_restorer;

    // Compose diagnostics prefix
#if defined(NCBI_OS_UNIX)
    PushDiagPostPrefix(NStr::IntToString(getpid()).c_str());
#endif
    PushDiagPostPrefix(GetEnvironment().Get(m_DiagPrefixEnv).c_str());

    // Timing
    CTime start_time(CTime::eCurrent);

    // Logging for statistics
    bool is_stat_log = GetConfig().GetBool("CGI", "StatLog", false,
                                           0, CNcbiRegistry::eReturn);
    auto_ptr<CCgiStatistics> stat(is_stat_log ? CreateStat() : 0);

    // Logging
    ELogOpt logopt = GetLogOpt();
    if (logopt == eLog) {
        x_LogPost("(CGI) CCgiApplication::Run ",
                  0, start_time, &GetEnvironment(), fBegin);
    }

    try {
        _TRACE("(CGI) CCgiApplication::Run: calling ProcessRequest");

        m_Context.reset( CreateContext() );
        ConfigureDiagnostics(*m_Context);
        x_AddLBCookie();
        try {
            // Print request start message
            x_OnEvent(eStartRequest, 0);

            VerifyCgiContext(*m_Context);
            // Set HTTP_REFERER
            string ref = m_Context->GetSelfURL();
            if ( !ref.empty() ) {
                string args =
                    m_Context->GetRequest().GetProperty(eCgi_QueryString);
                if ( !args.empty() ) {
                    ref += "?" + args;
                }
                GetConfig().Set("CONN", "HTTP_REFERER", ref);
            }
            // Print HTTP_REFERER
            if ( s_PrintRefererParam.Get() ) {
                ref = m_Context->GetRequest().GetProperty(eCgi_HttpReferer);
                if ( !ref.empty() ) {
                    GetDiagContext().PrintExtra("HTTP_REFERER=" + ref);
                }
            }
            result = ProcessRequest(*m_Context);
            if (result != 0) {
                SetHTTPStatus(500);
            }
        }
        catch (CCgiException& e) {
            if ( e.GetStatusCode() <  CCgiException::e200_Ok  ||
                 e.GetStatusCode() >= CCgiException::e400_BadRequest ) {
                throw;
            }
            // If for some reason exception with status 2xx was thrown,
            // set the result to 0, update HTTP status and continue.
            m_Context->GetResponse().SetStatus(e.GetStatusCode(),
                                               e.GetStatusMessage());
            SetHTTPStatus(e.GetStatusCode());
            result = 0;
        }
        _TRACE("CCgiApplication::Run: flushing");
        m_Context->GetResponse().Flush();
        _TRACE("CCgiApplication::Run: return " << result);
        x_OnEvent(result == 0 ? eSuccess : eError, result);
        x_OnEvent(eExit, result);
    }
    catch (exception& e) {
        // Call the exception handler and set the CGI exit code
        result = OnException(e, NcbiCout);
        x_OnEvent(eException, result);

        // Logging
        {{
            string msg = "(CGI) CCgiApplication::ProcessRequest() failed: ";
            msg += e.what();

            if (logopt != eNoLog) {
                x_LogPost(msg.c_str(), 0, start_time, 0, fBegin|fEnd);
            }
            if ( is_stat_log ) {
                stat->Reset(start_time, result, &e);
                msg = stat->Compose();
                stat->Submit(msg);
            }
        }}

        // Exception reporting
        {{
            CException* ex = dynamic_cast<CException*> (&e);
            if ( ex ) {
                NCBI_REPORT_EXCEPTION("(CGI) CCgiApplication::Run", *ex);
            } else if (logopt == eNoLog) {
                ERR_POST(e.what());
            }
        }}
    }

    // Logging
    if (logopt == eLog) {
        x_LogPost("(plain CGI) CCgiApplication::Run ",
                  0, start_time, 0, fEnd);
    }

    if ( is_stat_log ) {
        stat->Reset(start_time, result);
        string msg = stat->Compose();
        stat->Submit(msg);
    }

    x_OnEvent(eEndRequest, 120);
    x_OnEvent(eExit, result);

    return result;
}

void CCgiApplication::SetupArgDescriptions(CArgDescriptions* arg_desc)
{
    arg_desc->SetArgsType(CArgDescriptions::eCgiArgs);

    CParent::SetupArgDescriptions(arg_desc);
}


CCgiContext& CCgiApplication::x_GetContext( void ) const
{
    if ( !m_Context.get() ) {
        ERR_POST("CCgiApplication::GetContext: no context set");
        throw runtime_error("no context set");
    }
    return *m_Context;
}


CNcbiResource& CCgiApplication::x_GetResource( void ) const
{
    if ( !m_Resource.get() ) {
        ERR_POST("CCgiApplication::GetResource: no resource set");
        throw runtime_error("no resource set");
    }
    return *m_Resource;
}


void CCgiApplication::Init(void)
{
    // Convert multi-line diagnostic messages into one-line ones by default.
    SetDiagPostFlag(eDPF_PreMergeLines);
    SetDiagPostFlag(eDPF_MergeLines);

    CParent::Init();

    m_Resource.reset(LoadResource());

    m_DiagPrefixEnv = GetConfig().Get("CGI", "DiagPrefixEnv");
}


void CCgiApplication::Exit(void)
{
    m_Resource.reset(0);
    CParent::Exit();
}


CNcbiResource* CCgiApplication::LoadResource(void)
{
    return 0;
}


CCgiServerContext* CCgiApplication::LoadServerContext(CCgiContext& /*context*/)
{
    return 0;
}


NCBI_PARAM_DECL(bool, CGI, Count_Transfered);
NCBI_PARAM_DEF_EX(bool, CGI, Count_Transfered, false, eParam_NoThread,
                  CGI_COUNT_TRANSFERED);
typedef NCBI_PARAM_TYPE(CGI, Count_Transfered) TCGI_Count_Transfered;


CCgiContext* CCgiApplication::CreateContext
(CNcbiArguments*   args,
 CNcbiEnvironment* env,
 CNcbiIstream*     inp,
 CNcbiOstream*     out,
 int               ifd,
 int               ofd)
{
    int errbuf_size =
        GetConfig().GetInt("CGI", "RequestErrBufSize", 256, 0,
                           CNcbiRegistry::eReturn);

    if ( TCGI_Count_Transfered::GetDefault() ) {
        if ( !inp ) {
            if ( !m_InputStream.get() ) {
                m_InputStream.reset(
                    new CRStream(new CCGIStreamReader(std::cin),
                                CRWStreambuf::fOwnReader));
            }
            inp = m_InputStream.get();
        }
        if ( !out ) {
            if ( !m_OutputStream.get() ) {
                m_OutputStream.reset(
                    new CWStream(new CCGIStreamWriter(std::cout),
                                CRWStreambuf::fOwnWriter));
            }
            out = m_OutputStream.get();
            if ( m_InputStream.get() ) {
                // If both streams are created by the application, tie them.
                inp->tie(out);
            }
        }
    }
    return
        new CCgiContext(*this, args, env, inp, out, ifd, ofd,
                        (errbuf_size >= 0) ? (size_t) errbuf_size : 256,
                        m_RequestFlags);
}


void CCgiApplication::SetCafService(CCookieAffinity* caf)
{
    m_Caf.reset(caf);
}



// Flexible diagnostics support
//

class CStderrDiagFactory : public CDiagFactory
{
public:
    virtual CDiagHandler* New(const string&) {
        return new CStreamDiagHandler(&NcbiCerr);
    }
};


class CAsBodyDiagFactory : public CDiagFactory
{
public:
    CAsBodyDiagFactory(CCgiApplication* app) : m_App(app) {}
    virtual CDiagHandler* New(const string&) {
        CCgiResponse& response = m_App->GetContext().GetResponse();
        CDiagHandler* result   = new CStreamDiagHandler(&response.out());
        response.SetContentType("text/plain");
        response.WriteHeader();
        response.SetOutput(0); // suppress normal output
        return result;
    }

private:
    CCgiApplication* m_App;
};


CCgiApplication::CCgiApplication(void) 
 : m_RequestFlags(0),
   m_HostIP(0), 
   m_Iteration(0),
   m_ArgContextSync(false),
   m_HTTPStatus(200),
   m_RequestTimer(CStopWatch::eStop)
{
    // Disable system popup messages
    SuppressSystemMessageBox();

    // Turn on iteration number
    SetDiagPostFlag(eDPF_RequestId);
    SetDiagTraceFlag(eDPF_RequestId);

    SetStdioFlags(fBinaryCin | fBinaryCout);
    DisableArgDescriptions();
    RegisterDiagFactory("stderr", new CStderrDiagFactory);
    RegisterDiagFactory("asbody", new CAsBodyDiagFactory(this));
}


CCgiApplication::~CCgiApplication(void)
{
    ITERATE (TDiagFactoryMap, it, m_DiagFactories) {
        delete it->second;
    }
    if ( m_HostIP )
        free(m_HostIP);
}


int CCgiApplication::OnException(exception& e, CNcbiOstream& os)
{
    // Discriminate between different types of error
    string status_str = "500 Server Error";
    SetHTTPStatus(500);
    if ( dynamic_cast<CCgiException*> (&e) ) {
        CCgiException& cgi_e = dynamic_cast<CCgiException&>(e);
        if ( cgi_e.GetStatusCode() != CCgiException::eStatusNotSet ) {
            SetHTTPStatus((unsigned int)cgi_e.GetStatusCode());
            status_str = NStr::IntToString(m_HTTPStatus) +
                " " + cgi_e.GetStatusMessage();
        }
        else {
            // Convert CgiRequestException to error 400
            if ( dynamic_cast<CCgiRequestException*> (&e) ) {
                SetHTTPStatus(400);
                status_str = "400 Malformed HTTP Request";
            }
        }
    }

    // HTTP header
    os << "Status: " << status_str << HTTP_EOL;
    os << "Content-Type: text/plain" HTTP_EOL HTTP_EOL;

    // Message
    os << "ERROR:  " << status_str << " " HTTP_EOL HTTP_EOL;
    os << e.what();

    if ( dynamic_cast<CArgException*> (&e) ) {
        string ustr;
        const CArgDescriptions* descr = GetArgDescriptions();
        if (descr) {
            os << descr->PrintUsage(ustr) << HTTP_EOL HTTP_EOL;
        }
    }


    // Check for problems in sending the response
    if ( !os.good() ) {
        ERR_POST("CCgiApplication::OnException() failed to send error page"
                 " back to the client");
        return -1;
    }
    return 0;
}


const CArgs& CCgiApplication::GetArgs(void) const
{
    // Are there no argument descriptions or no CGI context (yet?)
    if (!GetArgDescriptions()  ||  !m_Context.get())
        return CParent::GetArgs();

    // Is everything already in-sync
    if ( m_ArgContextSync )
        return *m_CgiArgs;

    // Create CGI version of args, if necessary
    if ( !m_CgiArgs.get() )
        m_CgiArgs.reset(new CArgs());

    // Copy cmd-line arg values to CGI args
    *m_CgiArgs = CParent::GetArgs();

    // Add CGI parameters to the CGI version of args
    GetArgDescriptions()->ConvertKeys(m_CgiArgs.get(),
                                      GetContext().GetRequest().GetEntries(),
                                      true /*update=yes*/);

    m_ArgContextSync = true;
    return *m_CgiArgs;
}


void CCgiApplication::x_OnEvent(EEvent event, int status)
{
    switch ( event ) {
    case eStartRequest:
        {
            // Reset HTTP status code
            SetHTTPStatus(200);

            // Set context properties
            const CCgiRequest& req = m_Context->GetRequest();
            GetDiagContext().SetProperty("server_name",
                req.GetProperty(eCgi_ServerName));
            string client = req.GetRandomProperty("CAF_PROXIED_HOST");
            if ( client.empty() ) {
                client = req.GetProperty(eCgi_RemoteAddr);
            }
            GetDiagContext().SetProperty(
                CDiagContext::kProperty_ClientIP, client);
            //GetDiagContext().SetProperty(CDiagContext::kProperty_SessionID,
            //                             req.GetSession(CCgiRequest::eDontLoad)
            //                                   .RetrieveSessionId());
            GetDiagContext().SetProperty(CDiagContext::kProperty_SessionID,
                m_Context->RetrieveTrackingId());

            // Print request start message
            if ( !CDiagContext::IsSetOldPostFormat() ) {
                GetDiagContext().PrintRequestStart(req.GetCGIEntriesStr());
            }

            // Start timer
            m_RequestTimer.Restart();
            break;
        }
    case eSuccess:
    case eError:
    case eException:
        {
            GetDiagContext().SetProperty(CDiagContext::kProperty_ReqStatus,
                NStr::IntToString(m_HTTPStatus));
            if ( m_InputStream.get() ) {
                if ( m_InputStream->eof() ) {
                    m_InputStream->clear();
                }
                GetDiagContext().SetProperty(CDiagContext::kProperty_BytesRd,
                    NStr::IntToString(m_InputStream->tellg()));
            }
            if ( m_OutputStream.get() ) {
                GetDiagContext().SetProperty(CDiagContext::kProperty_BytesWr,
                    NStr::IntToString(m_OutputStream->tellp()));
            }
            break;
        }
    case eEndRequest:
        {
            GetDiagContext().SetProperty(CDiagContext::kProperty_ReqTime,
                m_RequestTimer.AsString());
            if ( !CDiagContext::IsSetOldPostFormat() ) {
                GetDiagContext().PrintRequestStop();
            }
            GetDiagContext().SetProperty(
                CDiagContext::kProperty_ClientIP, kEmptyStr);
            GetDiagContext().SetProperty(
                CDiagContext::kProperty_SessionID, kEmptyStr);
            break;
        }
    case eExit:
    case eExecutable:
    case eWatchFile:
    case eExitOnFail:
    case eExitRequest:
    case eWaiting:
        {
            break;
        }
    }

    OnEvent(event, status);
}


void CCgiApplication::OnEvent(EEvent /*event*/,
                              int    /*exit_code*/)
{
    return;
}


void CCgiApplication::RegisterDiagFactory(const string& key,
                                          CDiagFactory* fact)
{
    m_DiagFactories[key] = fact;
}


CDiagFactory* CCgiApplication::FindDiagFactory(const string& key)
{
    TDiagFactoryMap::const_iterator it = m_DiagFactories.find(key);
    if (it == m_DiagFactories.end())
        return 0;
    return it->second;
}


void CCgiApplication::ConfigureDiagnostics(CCgiContext& context)
{
    // Disable for production servers?
    ConfigureDiagDestination(context);
    ConfigureDiagThreshold(context);
    ConfigureDiagFormat(context);
}


void CCgiApplication::ConfigureDiagDestination(CCgiContext& context)
{
    const CCgiRequest& request = context.GetRequest();

    bool   is_set;
    string dest   = request.GetEntry("diag-destination", &is_set);
    if ( !is_set )
        return;

    SIZE_TYPE colon = dest.find(':');
    CDiagFactory* factory = FindDiagFactory(dest.substr(0, colon));
    if ( factory ) {
        SetDiagHandler(factory->New(dest.substr(colon + 1)));
    }
}


void CCgiApplication::ConfigureDiagThreshold(CCgiContext& context)
{
    const CCgiRequest& request = context.GetRequest();

    bool   is_set;
    string threshold = request.GetEntry("diag-threshold", &is_set);
    if ( !is_set )
        return;

    if (threshold == "fatal") {
        SetDiagPostLevel(eDiag_Fatal);
    } else if (threshold == "critical") {
        SetDiagPostLevel(eDiag_Critical);
    } else if (threshold == "error") {
        SetDiagPostLevel(eDiag_Error);
    } else if (threshold == "warning") {
        SetDiagPostLevel(eDiag_Warning);
    } else if (threshold == "info") {
        SetDiagPostLevel(eDiag_Info);
    } else if (threshold == "trace") {
        SetDiagPostLevel(eDiag_Info);
        SetDiagTrace(eDT_Enable);
    }
}


void CCgiApplication::ConfigureDiagFormat(CCgiContext& context)
{
    const CCgiRequest& request = context.GetRequest();

    typedef map<string, TDiagPostFlags> TFlagMap;
    static TFlagMap s_FlagMap;

    TDiagPostFlags defaults = (eDPF_Prefix | eDPF_Severity
                               | eDPF_ErrCode | eDPF_ErrSubCode);

    if ( !CDiagContext::IsSetOldPostFormat() ) {
        defaults |= (eDPF_UID | eDPF_PID | eDPF_RequestId |
            eDPF_SerialNo | eDPF_ErrorID);
    }

    TDiagPostFlags new_flags = 0;

    bool   is_set;
    string format = request.GetEntry("diag-format", &is_set);
    if ( !is_set )
        return;

    if (s_FlagMap.empty()) {
        s_FlagMap["file"]        = eDPF_File;
        s_FlagMap["path"]        = eDPF_LongFilename;
        s_FlagMap["line"]        = eDPF_Line;
        s_FlagMap["prefix"]      = eDPF_Prefix;
        s_FlagMap["severity"]    = eDPF_Severity;
        s_FlagMap["code"]        = eDPF_ErrCode;
        s_FlagMap["subcode"]     = eDPF_ErrSubCode;
        s_FlagMap["time"]        = eDPF_DateTime;
        s_FlagMap["omitinfosev"] = eDPF_OmitInfoSev;
        s_FlagMap["all"]         = eDPF_All;
        s_FlagMap["trace"]       = eDPF_Trace;
        s_FlagMap["log"]         = eDPF_Log;
        s_FlagMap["errorid"]     = eDPF_ErrorID;
        s_FlagMap["location"]    = eDPF_Location;
        s_FlagMap["pid"]         = eDPF_PID;
        s_FlagMap["tid"]         = eDPF_TID;
        s_FlagMap["serial"]      = eDPF_SerialNo;
        s_FlagMap["serial_thr"]  = eDPF_SerialNo_Thread;
        s_FlagMap["iteration"]   = eDPF_RequestId;
        s_FlagMap["uid"]         = eDPF_UID;
    }
    list<string> flags;
    NStr::Split(format, " ", flags);
    ITERATE(list<string>, flag, flags) {
        TFlagMap::const_iterator it;
        if ((it = s_FlagMap.find(*flag)) != s_FlagMap.end()) {
            new_flags |= it->second;
        } else if ((*flag)[0] == '!'
                   &&  ((it = s_FlagMap.find(flag->substr(1)))
                        != s_FlagMap.end())) {
            new_flags &= ~(it->second);
        } else if (*flag == "default") {
            new_flags |= defaults;
        }
    }
    SetDiagPostAllFlags(new_flags);
}


CCgiApplication::ELogOpt CCgiApplication::GetLogOpt() const
{
    string log = GetConfig().Get("CGI", "Log");

    CCgiApplication::ELogOpt logopt = eNoLog;
    if ((NStr::CompareNocase(log, "On") == 0) ||
        (NStr::CompareNocase(log, "true") == 0)) {
        logopt = eLog;
    } else if (NStr::CompareNocase(log, "OnError") == 0) {
        logopt = eLogOnError;
    }
#ifdef _DEBUG
    else if (NStr::CompareNocase(log, "OnDebug") == 0) {
        logopt = eLog;
    }
#endif

    return logopt;
}


void CCgiApplication::x_LogPost(const char*             msg_header,
                                unsigned int            iteration,
                                const CTime&            start_time,
                                const CNcbiEnvironment* env,
                                TLogPostFlags           flags)
    const
{
    CNcbiOstrstream msg;
    const CNcbiRegistry& reg = GetConfig();

    if ( msg_header ) {
        msg << msg_header << NcbiEndl;
    }

    if ( flags & fBegin ) {
        bool is_print_iter = reg.GetBool("FastCGI", "PrintIterNo",
                                         false, 0, CNcbiRegistry::eErrPost);
        if ( is_print_iter ) {
            msg << " Iteration = " << iteration << NcbiEndl;
        }
    }

    bool is_timing =
        reg.GetBool("CGI", "TimeStamp", false, 0, CNcbiRegistry::eErrPost);
    if ( is_timing ) {
        msg << " start time = "  << start_time.AsString();

        if ( flags & fEnd ) {
            CTime end_time(CTime::eCurrent);
            CTimeSpan elapsed = end_time.DiffTimeSpan(start_time);

            msg << "    end time = " << end_time.AsString()
                << "    elapsed = "  << elapsed.AsString();
        }
        msg << NcbiEndl;
    }

    if ((flags & fBegin)  &&  env) {
        string print_env = reg.Get("CGI", "PrintEnv");
        if ( !print_env.empty() ) {
            if (NStr::CompareNocase(print_env, "all") == 0) {
                // TODO
                ERR_POST("CCgiApp::  [CGI].PrintEnv=all not implemented");
            } else {  // list of variables
                list<string> vars;
                NStr::Split(print_env, ",; ", vars);
                ITERATE (list<string>, i, vars) {
                    msg << *i << "=" << env->Get(*i) << NcbiEndl;
                }
            }
        }
    }

    ERR_POST( (string) CNcbiOstrstreamToString(msg) );
}


CCgiStatistics* CCgiApplication::CreateStat()
{
    return new CCgiStatistics(*this);
}

ICgiSessionStorage* 
CCgiApplication::GetSessionStorage(CCgiSessionParameters&) const 
{
    return 0;
}

void CCgiApplication::x_AddLBCookie()
{
    const CNcbiRegistry& reg = GetConfig();

    string cookie_name = GetConfig().Get("CGI-LB", "Name");
    if ( cookie_name.empty() )
        return;

    int life_span = reg.GetInt("CGI-LB", "LifeSpan", 0, 0,
                               CNcbiRegistry::eReturn);

    string domain = reg.GetString("CGI-LB", "Domain", ".ncbi.nlm.nih.gov");

    if ( domain.empty() ) {
        ERR_POST("CGI-LB: 'Domain' not specified.");
    } else {
        if (domain[0] != '.') {     // domain must start with dot
            domain.insert(0, ".");
        }
    }

    string path = reg.Get("CGI-LB", "Path");

    bool secure = reg.GetBool("CGI-LB", "Secure", false,
                              0, CNcbiRegistry::eErrPost);

    string host;

    // Getting host configuration can take some time
    // for fast CGIs we try to avoid overhead and call it only once
    // m_HostIP variable keeps the cached value

    if ( m_HostIP ) {     // repeated call
        host = m_HostIP;
    }
    else {               // first time call
        host = reg.Get("CGI-LB", "Host");
        if ( host.empty() ) {
            if ( m_Caf.get() ) {
                char  host_ip[64] = {0,};
                m_Caf->GetHostIP(host_ip, sizeof(host_ip));
                m_HostIP = m_Caf->Encode(host_ip, 0);
                host = m_HostIP;
            }
            else {
                ERR_POST("CGI-LB: 'Host' not specified.");
            }
        }
    }


    CCgiCookie cookie(cookie_name, host, domain, path);
    if (life_span > 0) {
        CTime exp_time(CTime::eCurrent, CTime::eGmt);
        exp_time.AddSecond(life_span);
        cookie.SetExpTime(exp_time);
    }
    cookie.SetSecure(secure);

    GetContext().GetResponse().Cookies().Add(cookie);
}


void CCgiApplication::VerifyCgiContext(CCgiContext& context)
{
    string x_moz = context.GetRequest().GetRandomProperty("X_MOZ");
    if ( NStr::EqualNocase(x_moz, "prefetch") ) {
        NCBI_CGI_THROW_WITH_STATUS(CCgiRequestException, eData,
            "Prefetch is not allowed for CGIs",
            CCgiException::e403_Forbidden);
    }
}


void CCgiApplication::AppStart(void)
{
    // Print application start message
    if ( !CDiagContext::IsSetOldPostFormat() ) {
        GetDiagContext().PrintStart(kEmptyStr);
    }
}


void CCgiApplication::AppStop(int exit_code)
{
    GetDiagContext().SetProperty(CDiagContext::kProperty_ExitCode,
        NStr::IntToString(exit_code));
}


const char* kToolkitRcPath = "/etc/toolkitrc";
const char* kWebDirToPort = "Web_dir_to_port";

string CCgiApplication::GetDefaultLogPath(void) const
{
    string log_path = "/log/";

    string exe_path = GetProgramExecutablePath();
    CNcbiIfstream is(kToolkitRcPath, ios::binary);
    CNcbiRegistry reg(is);
    list<string> entries;
    reg.EnumerateEntries(kWebDirToPort, &entries);
    size_t min_pos = exe_path.length();
    string web_dir;
    // Find the first dir name corresponding to one of the entries
    ITERATE(list<string>, it, entries) {
        if (!it->empty()  &&  (*it)[0] != '/') {
            // not an absolute path
            string mask = "/" + *it;
            if (mask[mask.length() - 1] != '/') {
                mask += "/";
            }
            size_t pos = exe_path.find(mask);
            if (pos < min_pos) {
                min_pos = pos;
                web_dir = *it;
            }
        }
        else {
            // absolute path
            if (exe_path.substr(0, it->length()) == *it) {
                web_dir = *it;
                break;
            }
        }
    }
    if ( !web_dir.empty() ) {
        return log_path + reg.GetString(kWebDirToPort, web_dir, kEmptyStr);
    }
    // Could not find a valid web-dir entry, use port or 'srv'
    const char* port = ::getenv("SERVER_PORT");
    return port ? log_path + string(port) : log_path + "srv";
}


///////////////////////////////////////////////////////
// CCgiStatistics
//


CCgiStatistics::CCgiStatistics(CCgiApplication& cgi_app)
    : m_CgiApp(cgi_app), m_LogDelim(";")
{
}


CCgiStatistics::~CCgiStatistics()
{
}


void CCgiStatistics::Reset(const CTime& start_time,
                           int          result,
                           const std::exception*  ex)
{
    m_StartTime = start_time;
    m_Result    = result;
    m_ErrMsg    = ex ? ex->what() : kEmptyStr;
}


string CCgiStatistics::Compose(void)
{
    const CNcbiRegistry& reg = m_CgiApp.GetConfig();
    CTime end_time(CTime::eCurrent);

    // Check if it is assigned NOT to log the requests took less than
    // cut off time threshold
    TSeconds time_cutoff = reg.GetInt("CGI", "TimeStatCutOff", 0, 0,
                                      CNcbiRegistry::eReturn);
    if (time_cutoff > 0) {
        TSeconds diff = end_time.DiffSecond(m_StartTime);
        if (diff < time_cutoff) {
            return kEmptyStr;  // do nothing if it is a light weight request
        }
    }

    string msg, tmp_str;

    tmp_str = Compose_ProgramName();
    if ( !tmp_str.empty() ) {
        msg.append(tmp_str);
        msg.append(m_LogDelim);
    }

    tmp_str = Compose_Result();
    if ( !tmp_str.empty() ) {
        msg.append(tmp_str);
        msg.append(m_LogDelim);
    }

    bool is_timing =
        reg.GetBool("CGI", "TimeStamp", false, 0, CNcbiRegistry::eErrPost);
    if ( is_timing ) {
        tmp_str = Compose_Timing(end_time);
        if ( !tmp_str.empty() ) {
            msg.append(tmp_str);
            msg.append(m_LogDelim);
        }
    }

    tmp_str = Compose_Entries();
    if ( !tmp_str.empty() ) {
        msg.append(tmp_str);
    }

    tmp_str = Compose_ErrMessage();
    if ( !tmp_str.empty() ) {
        msg.append(tmp_str);
        msg.append(m_LogDelim);
    }

    return msg;
}


void CCgiStatistics::Submit(const string& message)
{
    LOG_POST(message);
}


string CCgiStatistics::Compose_ProgramName(void)
{
    return m_CgiApp.GetArguments().GetProgramName();
}


string CCgiStatistics::Compose_Timing(const CTime& end_time)
{
    CTimeSpan elapsed = end_time.DiffTimeSpan(m_StartTime);
    return m_StartTime.AsString() + m_LogDelim + elapsed.AsString();
}


string CCgiStatistics::Compose_Entries(void)
{
    const CCgiContext* ctx = m_CgiApp.m_Context.get();
    if ( !ctx )
        return kEmptyStr;

    const CCgiRequest& cgi_req = ctx->GetRequest();

    // LogArgs - list of CGI arguments to log.
    // Can come as list of arguments (LogArgs = param1;param2;param3),
    // or be supplemented with aliases (LogArgs = param1=1;param2=2;param3).
    // When alias is provided we use it for logging purposes (this feature
    // can be used to save logging space or reduce the net traffic).
    const CNcbiRegistry& reg = m_CgiApp.GetConfig();
    string log_args = reg.Get("CGI", "LogArgs");
    if ( log_args.empty() )
        return kEmptyStr;

    list<string> vars;
    NStr::Split(log_args, ",; \t", vars);

    string msg;
    ITERATE (list<string>, i, vars) {
        bool is_entry_found;
        const string& arg = *i;

        size_t pos = arg.find_last_of('=');
        if (pos == 0) {
            return "<misconf>" + m_LogDelim;
        } else if (pos != string::npos) {   // alias assigned
            string key = arg.substr(0, pos);
            const CCgiEntry& entry = cgi_req.GetEntry(key, &is_entry_found);
            if ( is_entry_found ) {
                string alias = arg.substr(pos+1, arg.length());
                msg.append(alias);
                msg.append("='");
                msg.append(entry.GetValue());
                msg.append("'");
                msg.append(m_LogDelim);
            }
        } else {
            const CCgiEntry& entry = cgi_req.GetEntry(arg, &is_entry_found);
            if ( is_entry_found ) {
                msg.append(arg);
                msg.append("='");
                msg.append(entry.GetValue());
                msg.append("'");
                msg.append(m_LogDelim);
            }
        }
    }

    return msg;
}


string CCgiStatistics::Compose_Result(void)
{
    return NStr::IntToString(m_Result);
}


string CCgiStatistics::Compose_ErrMessage(void)
{
    return m_ErrMsg;
}

/////////////////////////////////////////////////////////////////////////////
//  Tracking Environment

NCBI_PARAM_DEF(bool, CGI, DisableTrackingCookie, false);
NCBI_PARAM_DEF(string, CGI, TrackingCookieName, "ncbi_sid");
NCBI_PARAM_DEF(string, CGI, TrackingCookieDomain, ".nih.gov");
NCBI_PARAM_DEF(string, CGI, TrackingCookiePath, "/");


END_NCBI_SCOPE
