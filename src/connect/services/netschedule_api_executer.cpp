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
 * Author:  Anatoliy Kuznetsov, Maxim Didenko
 *
 * File Description:
 *   Implementation of NetSchedule API.
 *
 */

#include <ncbi_pch.hpp>
#include <connect/services/netschedule_api.hpp>
#include <connect/ncbi_conn_exception.hpp>

#include "netschedule_api_wait.hpp"

BEGIN_NCBI_SCOPE

inline
bool s_SendCmdWaitResponse(const CNetScheduleAPI& api, CNetSrvConnector& con, 
                           const string& cmd, size_t& err_count, string* resp = NULL)
{
    string r;
    try {
        r = api.SendCmdWaitResponse(con, cmd);
    } catch (CNetServiceException& ex) {
        ERR_POST( con.GetHost() << ":" << con.GetPort() 
                  << " returned error: \"" << ex.what() << "\"");
        if (ex.GetErrCode() == CNetServiceException::eCommunicationError) {
            if( ++err_count >= api.GetPoll().GetServersNumber() )
                NCBI_THROW(CNetServiceException, eCommunicationError, 
                           "Communication error");
            return false;
        } else throw;
    } catch (CIO_Exception& ex) {
        ERR_POST( con.GetHost() << ":" << con.GetPort() 
                  << " returned error: \"" << ex.what() << "\"");
        if( ++err_count >= api.GetPoll().GetServersNumber() )
            NCBI_THROW(CNetServiceException, eCommunicationError, 
                       "Communication error");
        return false;
    }
    if (resp) *resp = r;
    return true;
}

void CNetScheduleExecuter::SetRunTimeout(const string& job_key, 
                                         unsigned      time_to_run) const
{
    m_API->x_SendJobCmdWaitResponse("JRTO" , job_key, time_to_run); 
}


void CNetScheduleExecuter::JobDelayExpiration(const string& job_key, 
                                              unsigned      runtime_inc) const
{
    m_API->x_SendJobCmdWaitResponse("JDEX" , job_key, runtime_inc); 
}



bool CNetScheduleExecuter::GetJob(CNetScheduleJob& job, 
                                  unsigned short udp_port) const
{
    size_t err_count = 0;
    string cmd = "GET";
    if (udp_port != 0) 
        cmd += ' ' + NStr::IntToString(udp_port);

    for (CNetSrvConnectorPoll::iterator it = m_API->GetPoll().random_begin(); 
         it != m_API->GetPoll().end(); ++it) {
        CNetSrvConnectorHolder ch = *it;
          
        string resp;
        if (!s_SendCmdWaitResponse(*m_API, ch, cmd, err_count, &resp))
            continue;
       
        if (!resp.empty()) {
            job.mask = CNetScheduleAPI::eEmptyMask;
            string tmp;
            x_ParseGetJobResponse(&job.job_id, &job.input, &tmp, &tmp, &job.mask, resp);
            return true;
        }
    }
    return false;
}


bool CNetScheduleExecuter::GetJobWaitNotify(CNetScheduleJob& job,
                                            unsigned   wait_time,
                                            unsigned short udp_port) const
{
    size_t err_count = 0;
    string cmd = "WGET";
    cmd += ' ' + NStr::IntToString(udp_port) + ' ' + NStr::IntToString(wait_time);
    for (CNetSrvConnectorPoll::iterator it = m_API->GetPoll().random_begin(); 
         it != m_API->GetPoll().end(); ++it) {

        string resp;
        if (!s_SendCmdWaitResponse(*m_API,*it, cmd, err_count, &resp))
            continue;
        
        if (!resp.empty()) {
            job.mask = CNetScheduleAPI::eEmptyMask;
            string tmp;
            x_ParseGetJobResponse(&job.job_id, &job.input, &tmp, &tmp, &job.mask, resp);
            return true;
        }
    }
    return false;
}


bool CNetScheduleExecuter::WaitJob(CNetScheduleJob& job,
                                   unsigned   wait_time,
                                   unsigned short udp_port,
                                   EWaitMode      wait_mode) const
{
    //cerr << ">>WaitJob" << endl;
    bool job_received = 
        GetJobWaitNotify(job, wait_time, udp_port);
    if (job_received) {
        return job_received;
    }

    if (wait_mode != eWaitNotification) {
        return 0;
    }
    WaitQueueNotification(wait_time, udp_port);

    // no matter is WaitResult we re-try the request
    // using reliable comm.level and notify server that
    // we no longer on the UDP socket

    bool ret = GetJob(job, udp_port);
    return ret;

}

struct SWaitQueuePred {
    SWaitQueuePred(const string queue_name) : m_QueueName(queue_name)
    {
        m_MinLen = m_QueueName.size() + 9;
    }
    bool operator()(const string& buf) 
    {
        static const char* sign = "NCBI_JSQ_";
        if ((buf.size() < m_MinLen) || ((buf[0] ^ sign[0]) | (buf[1] ^ sign[1])) ) {
            return false;
        }
        
        const char* queue = buf.data() + 9;
        if ( m_QueueName == string(queue)) {
            return true;
        }
        return false;
    }
    string m_QueueName;
    size_t m_MinLen;
};


/* static */
bool CNetScheduleExecuter::WaitNotification(const string&  queue_name,
                                       unsigned       wait_time,
                                       unsigned short udp_port)
{
    return s_WaitNotification(wait_time, udp_port, SWaitQueuePred(queue_name));
}


void CNetScheduleExecuter::x_ParseGetJobResponse(string*        job_key, 
                                                 string*        input,
                                                 string*        jout,
                                                 string*        jerr,
                                                 CNetScheduleAPI::TJobMask*      job_mask,
                                                 const string&  response)
{
    // Server message format:
    //    JOB_KEY "input" ["out" ["err"]] [mask]

    _ASSERT(!response.empty());
    _ASSERT(input);
    _ASSERT(jout);
    _ASSERT(jerr);
    _ASSERT(job_mask);

    input->erase();
    job_key->erase();
    jout->erase();
    jerr->erase();

    const char* str = response.c_str();
    while (*str && isspace((unsigned char)(*str)))
        ++str;
    if (*str == 0) {
    throw_err:
        NCBI_THROW(CNetScheduleException, eProtocolSyntaxError, 
                   "Internal error. Cannot parse server output.");
    }

    for(;*str && !isspace((unsigned char)(*str)); ++str) {
        job_key->push_back(*str);
    }
    if (*str == 0) {
        goto throw_err;
    }

    while (*str && isspace((unsigned char)(*str)))
        ++str;

    if (*str && *str == '"') {
        ++str;
        for( ;*str && *str; ++str) {
            if (*str == '"' && *(str-1) != '\\') { ++str; break; }
            input->push_back(*str);
        }
    } else {
        goto throw_err;
    }

    *input = NStr::ParseEscapes(*input);

    // parse "out"
    while (*str && isspace((unsigned char)(*str)))
        ++str;
    if (*str == 0)
        return;

    if (*str == '"') {
        ++str;
        for( ;*str && *str; ++str) {
            if (*str == '"' && *(str-1) != '\\') { ++str; break; }
            jout->push_back(*str);
        }
    } else {
        goto throw_err;
    }
    *jout = NStr::ParseEscapes(*jout);

    while (*str && isspace((unsigned char)(*str)))
        ++str;

    if (*str == 0) {
        return;
    }
    if (*str == '"') {
        ++str;
        for( ;*str && *str; ++str) {
            if (*str == '"' && *(str-1) != '\\') { ++str; break; }
            jerr->push_back(*str);
        }
    } else {
        goto throw_err;
    }
    *jerr = NStr::ParseEscapes(*jerr);

    // parse mask
    while (*str && isspace((unsigned char)(*str)))
        ++str;
    if (*str == 0)
        return;

    *job_mask = atoi(str);
}


void CNetScheduleExecuter::PutResult(const CNetScheduleJob& job) const
{
    if (job.output.length() > kNetScheduleMaxDataSize) {
        NCBI_THROW(CNetScheduleException, eDataTooLong, 
            "Output data too long.");
    }

    m_API->x_SendJobCmdWaitResponse("PUT" , job.job_id, job.ret_code, job.output); 

}


bool CNetScheduleExecuter::PutResultGetJob(const CNetScheduleJob& done_job,
                                           CNetScheduleJob& new_job) const
{
    if (done_job.job_id.empty()) 
        return GetJob(new_job, 0);

    if (done_job.output.length() > kNetScheduleMaxDataSize) {
        NCBI_THROW(CNetScheduleException, eDataTooLong, 
            "Output data too long.");
    }

    string resp = m_API->x_SendJobCmdWaitResponse("JXCG" , done_job.job_id, done_job.ret_code, done_job.output); 

    if (!resp.empty()) {
        new_job.mask = CNetScheduleAPI::eEmptyMask;
        string tmp;
        x_ParseGetJobResponse(&new_job.job_id, &new_job.input, &tmp, &tmp, &new_job.mask, resp);
        _ASSERT(!new_job.job_id.empty());
        return true;
    }
    return false;
}


void CNetScheduleExecuter::PutProgressMsg(const CNetScheduleJob& job) const
{
    if (job.progress_msg.length() >= kNetScheduleMaxDataSize) {
        NCBI_THROW(CNetScheduleException, eDataTooLong, 
                   "Progress message too long");
    }
    m_API->x_SendJobCmdWaitResponse("MPUT" , job.job_id, job.progress_msg); 
}


void CNetScheduleExecuter::PutFailure(const CNetScheduleJob& job) const
{
    if (job.output.length() > kNetScheduleMaxDataSize) {
        NCBI_THROW(CNetScheduleException, eDataTooLong, 
            "Output data too long.");
    }

    if (job.error_msg.length() >= kNetScheduleMaxErrSize) {
        NCBI_THROW(CNetScheduleException, eDataTooLong, 
                   "Error message too long");
    }

    m_API->x_SendJobCmdWaitResponse("FPUT" , job.job_id, job.error_msg, job.output, job.ret_code); 
}



void CNetScheduleExecuter::ReturnJob(const string& job_key) const
{
    m_API->x_SendJobCmdWaitResponse("RETURN" , job_key); 
}

void CNetScheduleExecuter::x_RegUnregClient(const string&  cmd, 
                                            unsigned short udp_port) const
{
    size_t err_count = 0;
    string tmp = cmd + ' ' + NStr::IntToString(udp_port);
    for (CNetSrvConnectorPoll::iterator it = m_API->GetPoll().begin(); 
         it != m_API->GetPoll().end(); ++it) {
        CNetSrvConnectorHolder ch = *it;

        s_SendCmdWaitResponse(*m_API, ch, tmp, err_count);
    }
}


void CNetScheduleExecuter::RegisterClient(unsigned short udp_port) const
{
    x_RegUnregClient("REGC", udp_port);
}

void CNetScheduleExecuter::UnRegisterClient(unsigned short udp_port) const
{
    x_RegUnregClient("URGC", udp_port);
}



END_NCBI_SCOPE
