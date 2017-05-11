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
 * Authors:  Anatoliy Kuznetsov, Victor Joukov
 *
 * File Description: netschedule commands handler
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>
#include <corelib/resource_info.hpp>

#include "ns_handler.hpp"
#include "ns_server.hpp"
#include "ns_server_misc.hpp"
#include "ns_rollback.hpp"
#include "queue_database.hpp"
#include "ns_application.hpp"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>


USING_NCBI_SCOPE;


// A few popular protocol response constants
const string    kEndOfResponse = "\n";
const string    kOKCompleteResponse = "OK:" + kEndOfResponse;
const string    kErrNoJobFoundResponse = "ERR:eJobNotFound:" + kEndOfResponse;
const string    kOKResponsePrefix = "OK:";



// NetSchedule command parser
//

CNetScheduleHandler::SCommandMap CNetScheduleHandler::sm_BatchHeaderMap[] = {
    { "BTCH", { &CNetScheduleHandler::x_ProcessBatchStart,
                eNS_Queue | eNS_Submitter | eNS_Program },
        { { "size", eNSPT_Int, eNSPA_Required } } },
    { "ENDS", { &CNetScheduleHandler::x_ProcessBatchSequenceEnd,
                eNS_Queue | eNS_Submitter | eNS_Program } },
    { NULL }
};

CNetScheduleHandler::SCommandMap CNetScheduleHandler::sm_BatchEndMap[] = {
    { "ENDB" },
    { NULL }
};

SNSProtoArgument s_BatchArgs[] = {
    { "input", eNSPT_Str, eNSPA_Required      },
    { "aff",   eNSPT_Str, eNSPA_Optional, ""  },
    { "msk",   eNSPT_Int, eNSPA_Optional, "0" },
    { NULL }
};

CNetScheduleHandler::SCommandMap CNetScheduleHandler::sm_CommandMap[] = {

    { "SHUTDOWN",      { &CNetScheduleHandler::x_ProcessShutdown,
                         eNS_Admin },
        { { "drain",             eNSPT_Int, eNSPA_Optional, "0" },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "GETCONF",       { &CNetScheduleHandler::x_ProcessGetConf,
                         eNS_Admin },
        { { "effective",         eNSPT_Int, eNSPA_Optional, "0" },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "REFUSESUBMITS", { &CNetScheduleHandler::x_ProcessRefuseSubmits,
                         eNS_Admin },
        { { "mode",              eNSPT_Int, eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "QPAUSE",        { &CNetScheduleHandler::x_ProcessPause,
                         eNS_Queue },
        { { "pullback",          eNSPT_Int, eNSPA_Optional, "0" },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "QRESUME",       { &CNetScheduleHandler::x_ProcessResume,
                         eNS_Queue },
        { { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "RECO",          { &CNetScheduleHandler::x_ProcessReloadConfig,
                         eNS_NoChecks },
        { { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "VERSION",       { &CNetScheduleHandler::x_ProcessVersion,
                         eNS_NoChecks },
        { { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "HEALTH",        { &CNetScheduleHandler::x_ProcessHealth,
                         eNS_NoChecks },
        { { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "ACKALERT",      { &CNetScheduleHandler::x_ProcessAckAlert,
                         eNS_NoChecks },
        { { "alert",             eNSPT_Id,  eNSPA_Required      },
          { "user",              eNSPT_Str, eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "QUIT",          { &CNetScheduleHandler::x_ProcessQuitSession,
                         eNS_NoChecks },
        { { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "ACNT",          { &CNetScheduleHandler::x_ProcessActiveCount,
                         eNS_NoChecks },
        { { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "QLST",          { &CNetScheduleHandler::x_ProcessQList,
                         eNS_NoChecks },
        { { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "QINF",          { &CNetScheduleHandler::x_ProcessQueueInfo,
                         eNS_NoChecks },
        { { "qname",             eNSPT_Id,  eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "QINF2",         { &CNetScheduleHandler::x_ProcessQueueInfo,
                         eNS_NoChecks },
        { { "qname",             eNSPT_Id,  eNSPA_Optional, ""  },
          { "service",           eNSPT_Id,  eNSPA_Optional, ""  },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "SETQUEUE",      { &CNetScheduleHandler::x_ProcessSetQueue,
                         eNS_NoChecks },
        { { "qname",             eNSPT_Id,  eNSPA_Optional      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "SETSCOPE",      { &CNetScheduleHandler::x_ProcessSetScope,
                         eNS_Queue },
        { { "scope",             eNSPT_Id,  eNSPA_Optional      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "DROPQ",         { &CNetScheduleHandler::x_ProcessDropQueue,
                         eNS_Queue | eNS_Submitter | eNS_Program },
        { { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    /* QCRE checks 'program' and 'submit hosts' from the class */
    { "QCRE",          { &CNetScheduleHandler::x_ProcessCreateDynamicQueue,
                         eNS_NoChecks },
        { { "qname",             eNSPT_Id,  eNSPA_Required      },
          { "qclass",            eNSPT_Id,  eNSPA_Required      },
          { "description",       eNSPT_Str, eNSPA_Optional      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    /* QDEL checks 'program' and 'submit hosts' from the queue it deletes */
    { "QDEL",          { &CNetScheduleHandler::x_ProcessDeleteDynamicQueue,
                         eNS_NoChecks },
        { { "qname",             eNSPT_Id, eNSPA_Required       },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "STATUS",        { &CNetScheduleHandler::x_ProcessStatus,
                         eNS_Queue },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "STATUS2",       { &CNetScheduleHandler::x_ProcessStatus,
                         eNS_Queue },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    /* The STAT command makes sense with and without a queue */
    { "STAT",          { &CNetScheduleHandler::x_ProcessStatistics,
                         eNS_NoChecks },
        { { "option",            eNSPT_Id,  eNSPA_Optional      },
          { "comment",           eNSPT_Id,  eNSPA_Optional      },
          { "aff",               eNSPT_Str, eNSPA_Optional      },
          { "group",             eNSPT_Str, eNSPA_Optional      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "MPUT",          { &CNetScheduleHandler::x_ProcessPutMessage,
                         eNS_Queue | eNS_Worker | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "progress_msg",      eNSPT_Str, eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "MGET",          { &CNetScheduleHandler::x_ProcessGetMessage,
                         eNS_Queue | eNS_Submitter | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "DUMP",          { &CNetScheduleHandler::x_ProcessDump,
                         eNS_Queue },
        { { "job_key",           eNSPT_Id,  eNSPA_Optional      },
          { "status",            eNSPT_Str, eNSPA_Optional, ""  },
          { "start_after",       eNSPT_Id,  eNSPA_Optional      },
          { "count",             eNSPT_Int, eNSPA_Optional, "0" },
          { "group",             eNSPT_Str, eNSPA_Optional, ""  },
          { "aff",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "GETP",          { &CNetScheduleHandler::x_ProcessGetParam,
                         eNS_Queue },
        { { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "GETP2",         { &CNetScheduleHandler::x_ProcessGetParam,
                         eNS_Queue },
        { { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "GETC",          { &CNetScheduleHandler::x_ProcessGetConfiguration,
                         eNS_Queue },
        { { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "CLRN",          { &CNetScheduleHandler::x_ProcessClearWorkerNode,
                         eNS_Queue | eNS_Program },
        { { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "CANCELQ",       { &CNetScheduleHandler::x_ProcessCancelQueue,
                         eNS_Queue | eNS_Submitter | eNS_Program },
        { { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "SST",           { &CNetScheduleHandler::x_ProcessFastStatusS,
                         eNS_Queue | eNS_Submitter | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "SST2",          { &CNetScheduleHandler::x_ProcessFastStatusS,
                         eNS_Queue | eNS_Submitter | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "SUBMIT",        { &CNetScheduleHandler::x_ProcessSubmit,
                         eNS_Queue | eNS_Submitter | eNS_Program },
        { { "input",             eNSPT_Str, eNSPA_Required      },
          { "port",              eNSPT_Int, eNSPA_Optional      },
          { "timeout",           eNSPT_Int, eNSPA_Optional      },
          { "aff",               eNSPT_Str, eNSPA_Optional, ""  },
          { "msk",               eNSPT_Int, eNSPA_Optional, "0" },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "group",             eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "CANCEL",        { &CNetScheduleHandler::x_ProcessCancel,
                         eNS_Queue | eNS_Submitter | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Optional      },
          { "group",             eNSPT_Str, eNSPA_Optional, ""  },
          { "aff",               eNSPT_Str, eNSPA_Optional, ""  },
          { "status",            eNSPT_Str, eNSPA_Optional, ""  },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "LISTEN",        { &CNetScheduleHandler::x_ProcessListenJob,
                         eNS_Queue | eNS_Submitter | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "port",              eNSPT_Int, eNSPA_Required      },
          { "timeout",           eNSPT_Int, eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "BSUB",          { &CNetScheduleHandler::x_ProcessSubmitBatch,
                         eNS_Queue | eNS_Submitter | eNS_Program },
        { { "port",              eNSPT_Int, eNSPA_Optional      },
          { "timeout",           eNSPT_Int, eNSPA_Optional      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "group",             eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "READ",          { &CNetScheduleHandler::x_ProcessReading,
                         eNS_Queue | eNS_Reader | eNS_Program },
        { { "aff",               eNSPT_Str, eNSPA_Optional, ""  },
          { "port",              eNSPT_Int, eNSPA_Optional      },
          { "timeout",           eNSPT_Int, eNSPA_Optional,     },
          { "group",             eNSPT_Str, eNSPA_Optional, ""  },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  },
          { "affinity_may_change",
                                 eNSPT_Int, eNSPA_Optional, "0" },
          { "group_may_change",  eNSPT_Int, eNSPA_Optional, "0" } } },
    { "READ2",         { &CNetScheduleHandler::x_ProcessReading,
                         eNS_Queue | eNS_Reader | eNS_Program },
        { { "reader_aff",        eNSPT_Int, eNSPA_Required, "0" },
          { "any_aff",           eNSPT_Int, eNSPA_Required, "0" },
          { "exclusive_new_aff", eNSPT_Int, eNSPA_Optional, "0" },
          { "aff",               eNSPT_Str, eNSPA_Optional, ""  },
          { "port",              eNSPT_Int, eNSPA_Optional      },
          { "timeout",           eNSPT_Int, eNSPA_Optional,     },
          { "group",             eNSPT_Str, eNSPA_Optional, ""  },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  },
          { "affinity_may_change",
                                 eNSPT_Int, eNSPA_Optional, "0" },
          { "group_may_change",  eNSPT_Int, eNSPA_Optional, "0" },
          { "prioritized_aff",   eNSPT_Int, eNSPA_Optional, "0" } } },
    { "CFRM",          { &CNetScheduleHandler::x_ProcessConfirm,
                         eNS_Queue | eNS_Reader | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "auth_token",        eNSPT_Id,  eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "FRED",          { &CNetScheduleHandler::x_ProcessReadFailed,
                         eNS_Queue | eNS_Reader | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "auth_token",        eNSPT_Id,  eNSPA_Required      },
          { "err_msg",           eNSPT_Str, eNSPA_Optional      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  },
          { "no_retries",        eNSPT_Int, eNSPA_Optional, "0" } } },
    { "RDRB",          { &CNetScheduleHandler::x_ProcessReadRollback,
                         eNS_Queue | eNS_Reader | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "auth_token",        eNSPT_Str, eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  },
          { "blacklist",         eNSPT_Int, eNSPA_Optional, "1" } } },
    { "REREAD",        { &CNetScheduleHandler::x_ProcessReread,
                         eNS_Queue | eNS_Reader | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "WST",           { &CNetScheduleHandler::x_ProcessFastStatusW,
                         eNS_Queue },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "WST2",          { &CNetScheduleHandler::x_ProcessFastStatusW,
                         eNS_Queue },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "CHAFF",         { &CNetScheduleHandler::x_ProcessChangeAffinity,
                         eNS_Queue | eNS_Worker | eNS_Program },
        { { "add",               eNSPT_Str, eNSPA_Optional, ""  },
          { "del",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "CHRAFF",        { &CNetScheduleHandler::x_ProcessChangeAffinity,
                         eNS_Queue | eNS_Reader | eNS_Program },
        { { "add",               eNSPT_Str, eNSPA_Optional, ""  },
          { "del",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "SETAFF",        { &CNetScheduleHandler::x_ProcessSetAffinity,
                         eNS_Queue | eNS_Worker | eNS_Program },
        { { "aff",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "SETRAFF",       { &CNetScheduleHandler::x_ProcessSetAffinity,
                         eNS_Queue | eNS_Reader | eNS_Program },
        { { "aff",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "GET",           { &CNetScheduleHandler::x_ProcessGetJob,
                         eNS_Queue | eNS_Worker | eNS_Program },
        { { "port",              eNSPT_Id,  eNSPA_Optional      },
          { "aff",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "GET2",          { &CNetScheduleHandler::x_ProcessGetJob,
                         eNS_Queue | eNS_Worker | eNS_Program },
        { { "wnode_aff",         eNSPT_Int, eNSPA_Required, "0" },
          { "any_aff",           eNSPT_Int, eNSPA_Required, "0" },
          { "exclusive_new_aff", eNSPT_Int, eNSPA_Optional, "0" },
          { "aff",               eNSPT_Str, eNSPA_Optional, ""  },
          { "port",              eNSPT_Int, eNSPA_Optional      },
          { "timeout",           eNSPT_Int, eNSPA_Optional      },
          { "group",             eNSPT_Str, eNSPA_Optional, ""  },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  },
          { "prioritized_aff",   eNSPT_Int, eNSPA_Optional, "0" } } },
    { "PUT",           { &CNetScheduleHandler::x_ProcessPut,
                         eNS_Queue | eNS_Worker | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "job_return_code",   eNSPT_Id,  eNSPA_Required      },
          { "output",            eNSPT_Str, eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "PUT2",          { &CNetScheduleHandler::x_ProcessPut,
                         eNS_Queue | eNS_Worker | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "auth_token",        eNSPT_Id,  eNSPA_Required      },
          { "job_return_code",   eNSPT_Id,  eNSPA_Required      },
          { "output",            eNSPT_Str, eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "RETURN",        { &CNetScheduleHandler::x_ProcessReturn,
                         eNS_Queue | eNS_Worker | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "RETURN2",       { &CNetScheduleHandler::x_ProcessReturn,
                         eNS_Queue | eNS_Worker | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "auth_token",        eNSPT_Id,  eNSPA_Required      },
          { "blacklist",         eNSPT_Int, eNSPA_Optional, "1" },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "RESCHEDULE",    { &CNetScheduleHandler::x_ProcessReschedule,
                         eNS_Queue | eNS_Worker | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "auth_token",        eNSPT_Id,  eNSPA_Required      },
          { "aff",               eNSPT_Str, eNSPA_Optional, ""  },
          { "group",             eNSPT_Str, eNSPA_Optional, ""  },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "REDO",          { &CNetScheduleHandler::x_ProcessRedo,
                         eNS_Queue | eNS_Worker | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "WGET",          { &CNetScheduleHandler::x_ProcessGetJob,
                         eNS_Queue | eNS_Worker | eNS_Program },
        { { "port",              eNSPT_Int, eNSPA_Required      },
          { "timeout",           eNSPT_Int, eNSPA_Required      },
          { "aff",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "CWGET",         { &CNetScheduleHandler::x_ProcessCancelWaitGet,
                         eNS_Queue | eNS_Worker | eNS_Program },
        { { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "CWREAD",        { &CNetScheduleHandler::x_ProcessCancelWaitRead,
                         eNS_Queue | eNS_Reader | eNS_Program },
        { { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "FPUT",          { &CNetScheduleHandler::x_ProcessPutFailure,
                         eNS_Queue | eNS_Worker | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "err_msg",           eNSPT_Str, eNSPA_Required      },
          { "output",            eNSPT_Str, eNSPA_Required      },
          { "job_return_code",   eNSPT_Int, eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "FPUT2",         { &CNetScheduleHandler::x_ProcessPutFailure,
                         eNS_Queue | eNS_Worker | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "auth_token",        eNSPT_Id,  eNSPA_Required      },
          { "err_msg",           eNSPT_Str, eNSPA_Required      },
          { "output",            eNSPT_Str, eNSPA_Required      },
          { "job_return_code",   eNSPT_Int, eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  },
          { "no_retries",        eNSPT_Int, eNSPA_Optional, "0" } } },
    { "JXCG",          { &CNetScheduleHandler::x_ProcessJobExchange,
                         eNS_Queue | eNS_Worker | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Optchain      },
          { "job_return_code",   eNSPT_Int, eNSPA_Optchain      },
          { "output",            eNSPT_Str, eNSPA_Optional      },
          { "aff",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "JDEX",          { &CNetScheduleHandler::x_ProcessJobDelayExpiration,
                         eNS_Queue | eNS_Worker | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "timeout",           eNSPT_Int, eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },
    { "JDREX",         { &CNetScheduleHandler::x_ProcessJobDelayReadExpiration,
                         eNS_Queue | eNS_Submitter | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "timeout",           eNSPT_Int, eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""  } } },

    { "SETCLIENTDATA", { &CNetScheduleHandler::x_ProcessSetClientData,
                         eNS_Queue },
        { { "data",              eNSPT_Str, eNSPA_Required       },
          { "version",           eNSPT_Int, eNSPA_Optional, "-1" },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""   },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""   },
          { "ncbi_phid",         eNSPT_Str, eNSPA_Optional, ""   } } },

    // Obsolete commands
    { "REGC",          { &CNetScheduleHandler::x_CmdObsolete,
                         eNS_NoChecks } },
    { "URGC",          { &CNetScheduleHandler::x_CmdObsolete,
                         eNS_NoChecks } },
    { "INIT",          { &CNetScheduleHandler::x_CmdObsolete,
                         eNS_NoChecks } },
    { "AFLS",          { &CNetScheduleHandler::x_CmdObsolete,
                         eNS_NoChecks } },
    { "JRTO",          { &CNetScheduleHandler::x_CmdNotImplemented,
                         eNS_NoChecks } },
    { NULL },
};


static SNSProtoArgument s_AuthArgs[] = {
    { "client", eNSPT_Str, eNSPA_Optional, "Unknown client" },
    { "params", eNSPT_Str, eNSPA_Ellipsis },
    { NULL }
};



CRequestContextResetter::CRequestContextResetter()
{}

CRequestContextResetter::~CRequestContextResetter()
{
    CDiagContext::SetRequestContext(NULL);
}



static size_t s_BufReadHelper(void* data, const void* ptr, size_t size)
{
    ((string*) data)->append((const char *) ptr, size);
    return size;
}


static void s_ReadBufToString(BUF buf, string& str)
{
    size_t      size = BUF_Size(buf);

    str.erase();
    str.reserve(size);

    BUF_PeekAtCB(buf, 0, s_BufReadHelper, &str, size);
    BUF_Read(buf, NULL, size);
}


CNetScheduleHandler::CNetScheduleHandler(CNetScheduleServer* server)
    : m_MsgBufferSize(kInitialMessageBufferSize),
      m_MsgBuffer(new char[kInitialMessageBufferSize]),
      m_Server(server),
      m_ProcessMessage(NULL),
      m_BatchSize(0),
      m_BatchPos(0),
      m_BatchSubmPort(0),
      m_WithinBatchSubmit(false),
      m_SingleCmdParser(sm_CommandMap),
      m_BatchHeaderParser(sm_BatchHeaderMap),
      m_BatchEndParser(sm_BatchEndMap),
      m_ClientIdentificationPrinted(false),
      m_RollbackAction(NULL)
{}


CNetScheduleHandler::~CNetScheduleHandler()
{
    x_ClearRollbackAction();
    delete [] m_MsgBuffer;
}


void CNetScheduleHandler::OnOpen(void)
{
    CSocket &       socket = GetSocket();
    STimeout        to = {m_Server->GetInactivityTimeout(), 0};

    socket.DisableOSSendDelay();
    socket.SetTimeout(eIO_ReadWrite, &to);
    x_SetQuickAcknowledge();

    m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgAuth;

    // Log the fact of opened connection if needed
    if (m_Server->IsLog()) {
        CRequestContextResetter     context_resetter;
        x_CreateConnContext();
    }
}


void CNetScheduleHandler::OnWrite()
{}


void CNetScheduleHandler::OnClose(IServer_ConnectionHandler::EClosePeer peer)
{
    if (m_WithinBatchSubmit) {
        m_WithinBatchSubmit = false;
        m_Server->DecrementCurrentSubmitsCounter();
    }

    // It's possible that this method will be called before OnOpen - when
    // connection is just created and server is shutting down. In this case
    // OnOpen will never be called.
    //
    // m_ConnContext != NULL also tells that the logging is required
    if (m_ConnContext.IsNull())
        return;

    switch (peer)
    {
    case IServer_ConnectionHandler::eOurClose:
        // All the places where the server closes the connection make sure
        // that the conn and cmd request statuses are set approprietly
        break;
    case IServer_ConnectionHandler::eClientClose:
        // All the commands are synchronous so there is no need to set the
        // request status here.
        break;
    }


    // If a command has not finished its logging by some reasons - do it
    // here as the last chance.
    if (m_CmdContext.NotNull()) {
        CDiagContext::SetRequestContext(m_CmdContext);
        GetDiagContext().PrintRequestStop();
        m_CmdContext.Reset();
        CDiagContext::SetRequestContext(NULL);
    }

    CSocket &           socket = GetSocket();
    TNCBI_BigCount      read_count  = socket.GetCount(eIO_Read);
    TNCBI_BigCount      write_count = socket.GetCount(eIO_Write);

    m_ConnContext->SetBytesRd(read_count);
    m_ConnContext->SetBytesWr(write_count);

    // Call the socket shutdown only if it was a client close and
    // there was some data exchange over the connection.
    // LBSMD - when it checks a connection point - opens and aborts the
    // connection without any data transferring and in this scenario the
    // socket.Shutdown() call returns non success.
    if ((peer == IServer_ConnectionHandler::eClientClose) &&
        (read_count > 0 || write_count > 0)) {

        // The socket.Shutdown() call will fail if there is some not delivered
        // data in the socket or if the connection was not closed properly on
        // the client side.
        EIO_Status  status = socket.Shutdown(EIO_Event::eIO_ReadWrite);
        if (status != EIO_Status::eIO_Success) {
            m_ConnContext->SetRequestStatus(eStatus_SocketIOError);
            ERR_POST("Unseccessfull client socket shutdown. "
                     "The socket may have data not delivered to the client. "
                     "Error code: " << status << ": " << IO_StatusStr(status));
        }
    }

    CDiagContext::SetRequestContext(m_ConnContext);
    GetDiagContext().PrintRequestStop();
    m_ConnContext.Reset();
    CDiagContext::SetRequestContext(NULL);
}


void CNetScheduleHandler::OnTimeout()
{
    CRequestContextResetter     context_resetter;
    x_SetRequestContext();

    if (m_ConnContext.NotNull())
        m_ConnContext->SetRequestStatus(eStatus_Inactive);
}


void CNetScheduleHandler::OnOverflow(EOverflowReason reason)
{
    CRequestContextResetter     context_resetter;
    x_SetRequestContext();

    switch (reason) {
        case eOR_ConnectionPoolFull:
            ERR_POST("eCommunicationError:Connection pool full");
            break;
        case eOR_UnpollableSocket:
            ERR_POST("eCommunicationError:Unpollable connection");
            break;
        case eOR_RequestQueueFull:
            ERR_POST("eCommunicationError:Request queue full");
            break;
        default:
            ERR_POST("eCommunicationError:Unknown overflow error");
            break;
    }
}


void CNetScheduleHandler::OnMessage(BUF buffer)
{
    CRequestContextResetter     context_resetter;
    x_SetRequestContext();

    if (m_Server->ShutdownRequested()) {
        ERR_POST("NetSchedule is shutting down. Client input rejected.");
        x_SetCmdRequestStatus(eStatus_ShuttingDown);
        if (x_WriteMessage("ERR:eShuttingDown:NetSchedule server "
                           "is shutting down. Session aborted." +
                           kEndOfResponse) == eIO_Success)
            m_Server->CloseConnection(&GetSocket());
        return;
    }

    bool          error = true;
    string        error_client_message;
    unsigned int  error_code;

    try {
        // Single line user input processor
        (this->*m_ProcessMessage)(buffer);
        error = false;
    }
    catch (const CNetScheduleException &  ex) {
        if (ex.GetErrCode() == CNetScheduleException::eAuthenticationError) {
            ERR_POST(ex);
            if (m_CmdContext.NotNull())
                m_CmdContext->SetRequestStatus(ex.ErrCodeToHTTPStatusCode());
            else if (m_ConnContext.NotNull())
                m_ConnContext->SetRequestStatus(ex.ErrCodeToHTTPStatusCode());

            if (x_WriteMessage("ERR:" + string(ex.GetErrCodeString()) +
                               ":" + ex.GetMsg() +
                               kEndOfResponse) == eIO_Success)
                m_Server->CloseConnection(&GetSocket());
            return;
        }
        ERR_POST(ex);
        error_client_message = "ERR:" + string(ex.GetErrCodeString()) +
                               ":" + ex.GetMsg();
        error_code = ex.ErrCodeToHTTPStatusCode();
    }
    catch (const CNSProtoParserException &  ex) {
        ERR_POST(ex);
        error_client_message = "ERR:" + string(ex.GetErrCodeString()) +
                               ":" + ex.GetMsg();
        error_code = eStatus_BadRequest;
    }
    catch (const CNetServiceException &  ex) {
        ERR_POST(ex);
        error_client_message = "ERR:" + string(ex.GetErrCodeString()) +
                               ":" + ex.GetMsg();
        if (ex.GetErrCode() == CNetServiceException::eCommunicationError)
            error_code = eStatus_SocketIOError;
        else
            error_code = eStatus_ServerError;
    }
    catch (const CBDB_ErrnoException &  ex) {
        ERR_POST(ex);
        if (ex.IsRecovery()) {
            error_client_message = "ERR:eInternalError:" +
                    NStr::PrintableString("Fatal Berkeley DB error "
                                          "(DB_RUNRECOVERY). Emergency "
                                          "shutdown initiated. " +
                                          string(ex.what()));
            m_Server->SetShutdownFlag();
        }
        else
            error_client_message = "ERR:eInternalError:" +
                    NStr::PrintableString("Internal database error - " +
                                          string(ex.what()));
        error_code = eStatus_ServerError;
    }
    catch (const CBDB_Exception &  ex) {
        ERR_POST(ex);
        error_client_message = "ERR:" +
                               NStr::PrintableString("eInternalError:Internal "
                               "database (BDB) error - " + string(ex.what()));
        error_code = eStatus_ServerError;
    }
    catch (const exception &  ex) {
        ERR_POST("STL exception: " << ex.what());
        error_client_message = "ERR:" +
                               NStr::PrintableString("eInternalError:Internal "
                               "error - " + string(ex.what()));
        error_code = eStatus_ServerError;
    }
    catch (...) {
        ERR_POST("ERR:Unknown server exception.");
        error_client_message = "ERR:eInternalError:Unknown server exception.";
        error_code = eStatus_ServerError;
    }

    if (error) {
        x_SetCmdRequestStatus(error_code);
        x_WriteMessage(error_client_message + kEndOfResponse);
        x_PrintCmdRequestStop();
    }
}


void CNetScheduleHandler::OnError(const string &  err_message)
{
    ERR_POST(Warning << err_message);
}


void CNetScheduleHandler::x_SetQuickAcknowledge(void)
{
    int     fd = 0;
    int     val = 1;

    GetSocket().GetOSHandle(&fd, sizeof(fd));
    setsockopt(fd, IPPROTO_TCP, TCP_QUICKACK, &val, sizeof(val));
}



EIO_Status
CNetScheduleHandler::x_PrepareWriteBuffer(const string &  msg,
                                          size_t          msg_size,
                                          size_t          required_size)
{
    if (required_size > m_MsgBufferSize) {
        delete [] m_MsgBuffer;
        while (required_size > m_MsgBufferSize)
            m_MsgBufferSize += kMessageBufferIncrement;
        m_MsgBuffer = new char[m_MsgBufferSize];
    }

    memcpy(m_MsgBuffer, msg.data(), msg_size);
    m_MsgBuffer[required_size-1] = '\n';

    #if defined(_DEBUG) && !defined(NDEBUG)
    if (m_ConnContext.NotNull()) {
        SErrorEmulatorParameter     err_emul = m_Server->GetDebugWriteDelay();
        if (err_emul.IsActive()) {
            if (err_emul.as_double > 0.0) {
                CNSPreciseTime      delay(err_emul.as_double);

                // Non signal safe but good enough for error simulation
                nanosleep(&delay, NULL);
            }
        }

        err_emul = m_Server->GetDebugConnDropBeforeWrite();
        if (err_emul.IsActive()) {
            if (err_emul.as_bool) {
                m_Server->CloseConnection(&GetSocket());
                return eIO_Closed;
            }
        }

        err_emul = m_Server->GetDebugReplyWithGarbage();
        if (err_emul.IsActive()) {
            if (err_emul.as_bool) {
                string      value = m_Server->GetDebugGarbage();

                msg_size = value.size();
                while (msg_size >= 1 && msg[msg_size-1] == '\n')
                    --msg_size;
                required_size = msg_size + 1;

                if (required_size > m_MsgBufferSize) {
                    delete [] m_MsgBuffer;
                    while (required_size > m_MsgBufferSize)
                        m_MsgBufferSize += kMessageBufferIncrement;
                    m_MsgBuffer = new char[m_MsgBufferSize];
                }

                memcpy(m_MsgBuffer, value.c_str(), msg_size);
                m_MsgBuffer[required_size-1] = '\n';
            }
        }
    }
    #endif

    return eIO_Success;
}


EIO_Status CNetScheduleHandler::x_WriteMessage(const string &  msg)
{
    size_t  msg_size = msg.size();
    bool    has_eom = false;

    while (msg_size >= 1 && msg[msg_size-1] == '\n') {
        --msg_size;
        has_eom = true;
    }

    size_t          required_size = msg_size + 1;
    const char *    msg_buf = NULL;

    if (has_eom)
        msg_buf = msg.data();
    else {
        EIO_Status  status = x_PrepareWriteBuffer(msg, msg_size, required_size);
        if (status != eIO_Success)
            return status;
        msg_buf = m_MsgBuffer;
    }

    // Write to the socket as a single transaction
    size_t              written;
    CNSPreciseTime      write_start = CNSPreciseTime::Current();
    EIO_Status result = GetSocket().Write(msg_buf, required_size, &written);
    if (result == eIO_Success) {
        #if defined(_DEBUG) && !defined(NDEBUG)
        if (m_ConnContext.NotNull()) {
            SErrorEmulatorParameter     err_emul =
                                        m_Server->GetDebugConnDropAfterWrite();
            if (err_emul.IsActive()) {
                if (err_emul.as_bool) {
                    m_Server->CloseConnection(&GetSocket());
                    return eIO_Closed;
                }
            }
        }
        #endif
    } else {
        CNSPreciseTime  write_timing = CNSPreciseTime::Current() - write_start;
        x_HandleSocketErrorOnResponse(msg_buf, result, written, write_timing);
    }
    return result;
}


void  CNetScheduleHandler::x_HandleSocketErrorOnResponse(
                                        const string &          msg,
                                        EIO_Status              write_result,
                                        size_t                  written_bytes,
                                        const CNSPreciseTime &  timing)
{
    // There was an error of writing into a socket, so rollback the action if
    // necessary and close the connection
    string     report =
        "Error writing message to the client. "
        "Peer: " +  GetSocket().GetPeerAddress() + ". "
        "Socket write error status: " + IO_StatusStr(write_result) + ". "
        "Written bytes: " + NStr::NumericToString(written_bytes) + ". "
        "Socket write timing: " + NStr::NumericToString(double(timing)) + ". "
        "Message begins with: ";
    if (msg.size() > 32)
        report += msg.substr(0, 32) + " (TRUNCATED)";
    else
        report += msg;
    ERR_POST(report);

    if (m_ConnContext.NotNull()) {
        if (m_ConnContext->GetRequestStatus() == eStatus_OK)
            m_ConnContext->SetRequestStatus(eStatus_SocketIOError);
        if (m_CmdContext.NotNull()) {
            if (m_CmdContext->GetRequestStatus() == eStatus_OK)
                m_CmdContext->SetRequestStatus(eStatus_SocketIOError);
        }
    }

    try {
        if (!m_QueueName.empty()) {
            CRef<CQueue>    ref = GetQueue();
            ref->RegisterSocketWriteError(m_ClientId);
            x_ExecuteRollbackAction(ref.GetPointer());
        }
    } catch (...) {}

    m_Server->CloseConnection(&GetSocket());
}


unsigned int CNetScheduleHandler::x_GetPeerAddress(void)
{
    unsigned int        peer_addr;

    GetSocket().GetPeerAddress(&peer_addr, 0, eNH_NetworkByteOrder);

    // always use localhost(127.0*) address for clients coming from
    // the same net address (sometimes it can be 127.* or full address)
    if (peer_addr == m_Server->GetHostNetAddr())
        return CSocketAPI::GetLoopbackAddress();
    return peer_addr;
}


void CNetScheduleHandler::x_ProcessMsgAuth(BUF buffer)
{
    // This should only memorize the received string.
    // The x_ProcessMsgQueue(...)  will parse it.
    // This is done to avoid copying parsed parameters and to have exactly one
    // diagnostics extra with both auth parameters and queue name.
    s_ReadBufToString(buffer, m_RawAuthString);

    // Check if it was systems probe...
    if (strncmp(m_RawAuthString.c_str(), "GET / HTTP/1.", 13) == 0) {
        // That was systems probing ports

        x_SetConnRequestStatus(eStatus_HTTPProbe);
        m_Server->CloseConnection(&GetSocket());
        return;
    }

    m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgQueue;
    x_SetQuickAcknowledge();
}


void CNetScheduleHandler::x_ProcessMsgQueue(BUF buffer)
{
    s_ReadBufToString(buffer, m_QueueName);

    // Parse saved raw authorization string and produce log output
    TNSProtoParams      params;

    try {
        m_SingleCmdParser.ParseArguments(m_RawAuthString, s_AuthArgs, &params);
    }
    catch (CNSProtoParserException &  ex) {
        string msg = "Error authenticating client: '";
        if (m_RawAuthString.size() > 128)
            msg += string(m_RawAuthString.c_str(), 128) + " (TRUNCATED)";
        else
            msg += m_RawAuthString;
        msg += "': ";

        // This will form request context with the client IP etc.
        CRequestContextResetter     context_resetter;
        if (m_ConnContext.IsNull())
            x_CreateConnContext();

        // ex.what() is here to avoid unnecessery records in the log
        // if it is simple 'ex' -> 2 records are produced
        ERR_POST(msg << ex.what());

        x_SetConnRequestStatus(eStatus_BadRequest);
        m_Server->CloseConnection(&GetSocket());
        return;
    }

    // Memorize what we know about the client
    m_ClientId.Update(this->x_GetPeerAddress(), params);


    // Test if it is an administrative user and memorize it
    if (m_Server->AdminHostValid(m_ClientId.GetAddress()) &&
        m_Server->IsAdminClientName(m_ClientId.GetClientName()))
        m_ClientId.SetPassedChecks(eNS_Admin);

    // Produce the log output if required
    if (x_NeedCmdLogging()) {
        CDiagContext_Extra diag_extra = GetDiagContext().Extra();
        diag_extra.Print("queue", m_QueueName);
        ITERATE(TNSProtoParams, it, params) {
            diag_extra.Print(it->first, it->second);
        }
    }

    // Empty queue name is a synonim for hardcoded 'noname'.
    // To have exactly one string comparison, make the name empty if 'noname'
    if (NStr::CompareNocase(m_QueueName, "noname") == 0)
        m_QueueName = "";

    if (!m_QueueName.empty()) {
        CRef<CQueue>    queue;
        try {
            queue = m_Server->OpenQueue(m_QueueName);
            m_QueueRef.Reset(queue);
        }
        catch (const CNetScheduleException &  ex) {
            if (ex.GetErrCode() == CNetScheduleException::eUnknownQueue) {
                // This will form request context with the client IP etc.
                CRequestContextResetter     context_resetter;
                if (m_ConnContext.IsNull())
                    x_CreateConnContext();

                ERR_POST(ex);
                x_SetConnRequestStatus(ex.ErrCodeToHTTPStatusCode());
                if (x_WriteMessage("ERR:" + string(ex.GetErrCodeString()) +
                                   ":" + ex.GetMsg() +
                                   kEndOfResponse) == eIO_Success)
                    m_Server->CloseConnection(&GetSocket());
                return;
            }
            throw;
        }

        x_UpdateClientPassedChecks(queue.GetPointer());
    }
    else
        m_QueueRef.Reset(NULL);

    m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
    x_SetQuickAcknowledge();
}


// Workhorse method
void CNetScheduleHandler::x_ProcessMsgRequest(BUF buffer)
{
    if (x_NeedCmdLogging()) {
        m_CmdContext.Reset(new CRequestContext());
        m_CmdContext->SetRequestStatus(eStatus_OK);
        m_CmdContext->SetRequestID();
        CDiagContext::SetRequestContext(m_CmdContext);
    }

    SParsedCmd      cmd;
    string          msg;
    try {
        s_ReadBufToString(buffer, msg);
        cmd = m_SingleCmdParser.ParseCommand(msg);

        // It throws an exception if the input is not valid
        m_CommandArguments.AssignValues(
                cmd.params,
                cmd.command->cmd,
                x_NeedToGeneratePHIDAndSID(cmd.command->extra.processor),
                GetSocket(),
                m_Server->GetCompoundIDPool());

        x_PrintCmdRequestStart(cmd);
    }
    catch (const CNSProtoParserException &  ex) {
        // This is the exception from m_SingleCmdParser.ParseCommand(msg)

        // Parsing is done before PrintRequestStart(...) so a diag context is
        // not created here - create it just to provide an error output
        x_OnCmdParserError(true, ex.GetMsg(), "");
        return;
    }
    catch (const CNetScheduleException &  ex) {
        // This is an exception from AssignValues(...)

        // That is, the print request has not been printed yet
        x_PrintCmdRequestStart(cmd);
        throw;
    }

    const SCommandExtra &   extra = cmd.command->extra;

    if (extra.processor == &CNetScheduleHandler::x_ProcessQuitSession) {
        x_ProcessQuitSession(0);
        return;
    }


    // If the command requires queue, hold a hard reference to this
    // queue from a weak one (m_Queue) in queue_ref, and take C pointer
    // into queue_ptr. Otherwise queue_ptr is NULL, which is OK for
    // commands which does not require a queue.
    CRef<CQueue>        queue_ref;
    CQueue *            queue_ptr = NULL;

    bool                restore_client = false;
    TNSCommandChecks    orig_client_passed_checks = 0;
    unsigned int        orig_client_id = 0;

    if (extra.checks & eNS_Queue) {
        if (x_CanBeWithoutQueue(extra.processor) &&
            !m_CommandArguments.queue_from_job_key.empty()) {
            // This command must use queue name from the job key
            queue_ref.Reset(
                    m_Server->OpenQueue(
                        m_CommandArguments.queue_from_job_key));
            queue_ptr = queue_ref.GetPointer();

            if (NStr::CompareNocase(m_QueueName,
                                 m_CommandArguments.queue_from_job_key) != 0) {
                orig_client_passed_checks = m_ClientId.GetPassedChecks();
                orig_client_id = m_ClientId.GetID();
                restore_client = true;

                x_UpdateClientPassedChecks(queue_ptr);
            }
        } else {
            // Old fasion way - the queue comes from handshake
            if (m_QueueName.empty())
                NCBI_THROW(CNetScheduleException, eUnknownQueue,
                           "Job queue is required");
            queue_ref.Reset(GetQueue());
            queue_ptr = queue_ref.GetPointer();
        }
    }
    else if (extra.processor == &CNetScheduleHandler::x_ProcessStatistics ||
             extra.processor == &CNetScheduleHandler::x_ProcessRefuseSubmits) {
        if (!m_QueueName.empty()) {
            // The STAT and REFUSESUBMITS commands
            // could be with or without a queue
            queue_ref.Reset(GetQueue());
            queue_ptr = queue_ref.GetPointer();
        }
    }

    m_ClientId.CheckAccess(extra.checks, m_Server, cmd.command->cmd);

    m_ClientIdentificationPrinted = false;
    if (queue_ptr) {
        bool        client_was_found = false;
        bool        session_was_reset = false;
        string      old_session;
        bool        had_wn_pref_affs = false;
        bool        had_reader_pref_affs = false;

        // The cient has a queue, so memorize the client
        queue_ptr->TouchClientsRegistry(m_ClientId, client_was_found,
                                        session_was_reset, old_session,
                                        had_wn_pref_affs, had_reader_pref_affs);
        if (client_was_found && session_was_reset) {
            if (x_NeedCmdLogging()) {
                string  wn_val = "true";
                if (!had_wn_pref_affs)
                    wn_val = "had none";
                string  reader_val = "true";
                if (!had_reader_pref_affs)
                    reader_val = "had none";

                GetDiagContext().Extra()
                    .Print("client_node", m_ClientId.GetNode())
                    .Print("client_session", m_ClientId.GetSession())
                    .Print("client_old_session", old_session)
                    .Print("wn_preferred_affinities_reset", wn_val)
                    .Print("reader_preferred_affinities_reset", reader_val);

                m_ClientIdentificationPrinted = true;
            }
        }
    }

    // Execute the command
    (this->*extra.processor)(queue_ptr);

    if (restore_client) {
        m_ClientId.SetPassedChecks(orig_client_passed_checks);
        m_ClientId.SetID(orig_client_id);
    }
}


void CNetScheduleHandler::x_UpdateClientPassedChecks(CQueue * q)
{
    // Admin flag is not reset because it comes from a handshake stage and
    // cannot be changed later.
    m_ClientId.ResetPassedCheck();

    // First, deal with a queue
    if (q == NULL)
        return;

    m_ClientId.SetPassedChecks(eNS_Queue);
    if (!m_ClientId.IsAdmin()) {
        // Admin can do everything, so there is no need to check
        // the hosts and programs for non-admins only
        if (q->IsWorkerAllowed(m_ClientId.GetAddress()))
            m_ClientId.SetPassedChecks(eNS_Worker);
        if (q->IsSubmitAllowed(m_ClientId.GetAddress()))
            m_ClientId.SetPassedChecks(eNS_Submitter);
        if (q->IsReaderAllowed(m_ClientId.GetAddress()))
            m_ClientId.SetPassedChecks(eNS_Reader);
        if (q->IsProgramAllowed(m_ClientId.GetProgramName()))
            m_ClientId.SetPassedChecks(eNS_Program);
    }

    // Also, update the client scope
    q->SetClientScope(m_ClientId);
}


// Message processors for x_ProcessSubmitBatch
void CNetScheduleHandler::x_ProcessMsgBatchHeader(BUF buffer)
{
    // Expecting BTCH size | ENDS
    try {
        string          msg;
        s_ReadBufToString(buffer, msg);

        SParsedCmd      cmd      = m_BatchHeaderParser.ParseCommand(msg);
        const string &  size_str = cmd.params["size"];

        if (!size_str.empty())
            m_BatchSize = NStr::StringToInt(size_str);
        else
            m_BatchSize = 0;
        (this->*cmd.command->extra.processor)(0);
    }
    catch (const CNSProtoParserException &  ex) {
        m_WithinBatchSubmit = false;
        m_Server->DecrementCurrentSubmitsCounter();
        x_OnCmdParserError(false, ex.GetMsg(), ", BTCH or ENDS expected");
        return;
    }
    catch (const CNetScheduleException &  ex) {
        m_WithinBatchSubmit = false;
        m_Server->DecrementCurrentSubmitsCounter();
        ERR_POST("Server error: " << ex);
        x_SetCmdRequestStatus(ex.ErrCodeToHTTPStatusCode());
        x_WriteMessage("ERR:" + string(ex.GetErrCodeString()) +
                       ":" + ex.GetMsg() + kEndOfResponse);
        x_PrintCmdRequestStop();

        m_BatchJobs.clear();
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
    }
    catch (const CException &  ex) {
        m_WithinBatchSubmit = false;
        m_Server->DecrementCurrentSubmitsCounter();
        ERR_POST("Error processing command: " << ex);
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_WriteMessage("ERR:eProtocolSyntaxError:"
                       "Error processing BTCH or ENDS." + kEndOfResponse);
        x_PrintCmdRequestStop();

        m_BatchJobs.clear();
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
    }
    catch (...) {
        m_WithinBatchSubmit = false;
        m_Server->DecrementCurrentSubmitsCounter();
        ERR_POST("Unknown error while expecting BTCH or ENDS");
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_WriteMessage("ERR:eInternalError:Unknown error "
                       "while expecting BTCH or ENDS." + kEndOfResponse);
        x_PrintCmdRequestStop();

        m_BatchJobs.clear();
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
    }
}


void CNetScheduleHandler::x_ProcessMsgBatchJob(BUF buffer)
{
    // Expecting:
    // "input" [aff="affinity_token"] [msk=1]
    string          msg;
    s_ReadBufToString(buffer, msg);

    CJob &          job = m_BatchJobs[m_BatchPos].first;
    TNSProtoParams  params;
    try {
        m_BatchEndParser.ParseArguments(msg, s_BatchArgs, &params);
        m_CommandArguments.AssignValues(params, "", false, GetSocket(),
                                        m_Server->GetCompoundIDPool());
    }
    catch (const CNSProtoParserException &  ex) {
        m_WithinBatchSubmit = false;
        m_Server->DecrementCurrentSubmitsCounter();
        x_OnCmdParserError(false, ex.GetMsg(), "");
        return;
    }
    catch (const CNetScheduleException &  ex) {
        m_WithinBatchSubmit = false;
        m_Server->DecrementCurrentSubmitsCounter();
        ERR_POST(ex.GetMsg());
        x_SetCmdRequestStatus(ex.ErrCodeToHTTPStatusCode());
        x_WriteMessage("ERR:" + string(ex.GetErrCodeString()) +
                       ":" + ex.GetMsg() + kEndOfResponse);
        x_PrintCmdRequestStop();

        m_BatchJobs.clear();
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
        return;
    }
    catch (const CException &  ex) {
        m_WithinBatchSubmit = false;
        m_Server->DecrementCurrentSubmitsCounter();
        ERR_POST("Error processing command: " << ex);
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_WriteMessage("ERR:eProtocolSyntaxError:"
                       "Invalid batch submission, syntax error" +
                       kEndOfResponse);
        x_PrintCmdRequestStop();

        m_BatchJobs.clear();
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
        return;
    }
    catch (...) {
        m_WithinBatchSubmit = false;
        m_Server->DecrementCurrentSubmitsCounter();
        ERR_POST("Arguments parsing unknown exception. "
                 "Batch submit is rejected.");
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_WriteMessage("ERR:eProtocolSyntaxError:"
                       "Arguments parsing unknown exception" +
                       kEndOfResponse);
        x_PrintCmdRequestStop();

        m_BatchJobs.clear();
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
        return;
    }

    job.SetInput(m_CommandArguments.input);

    // See CXX-8843: no affinity is replaced with "-" affinity automatically
    if (m_CommandArguments.affinity_token.empty())
        m_CommandArguments.affinity_token = k_NoAffinityToken;

    m_BatchJobs[m_BatchPos].second = m_CommandArguments.affinity_token;

    job.SetMask(m_CommandArguments.job_mask);
    job.SetSubmNotifPort(m_BatchSubmPort);
    job.SetSubmNotifTimeout(m_BatchSubmTimeout);
    job.SetClientIP(m_BatchClientIP);
    job.SetClientSID(m_BatchClientSID);
    job.SetNCBIPHID(m_BatchNCBIPHID);

    if (++m_BatchPos >= m_BatchSize)
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgBatchSubmit;
}


string CNetScheduleHandler::x_GetConnRef(void) const
{
    // See CXX-8253
    return GetDiagContext().GetStringUID() + "_" +
           NStr::NumericToString(m_ConnContext->GetRequestID());
}


void CNetScheduleHandler::x_ProcessMsgBatchSubmit(BUF buffer)
{
    // Expecting ENDB
    try {
        string      msg;
        s_ReadBufToString(buffer, msg);
        m_BatchEndParser.ParseCommand(msg);
    }
    catch (const CNSProtoParserException &  ex) {
        m_WithinBatchSubmit = false;
        m_Server->DecrementCurrentSubmitsCounter();
        x_OnCmdParserError(false, ex.GetMsg(), ", ENDB expected");
        return;
    }
    catch (const CException &  ex) {
        m_WithinBatchSubmit = false;
        m_Server->DecrementCurrentSubmitsCounter();
        BUF_Read(buffer, 0, BUF_Size(buffer));
        ERR_POST("Error processing command: " << ex);
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_WriteMessage("ERR:eProtocolSyntaxError:"
                       "Batch submit error - unexpected end of batch" +
                       kEndOfResponse);
        x_PrintCmdRequestStop();

        m_BatchJobs.clear();
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
        return;
    }
    catch (...) {
        m_WithinBatchSubmit = false;
        m_Server->DecrementCurrentSubmitsCounter();
        ERR_POST("Unknown error while expecting ENDB.");
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_WriteMessage("ERR:eInternalError:"
                       "Unknown error while expecting ENDB." +
                       kEndOfResponse);
        x_PrintCmdRequestStop();

        m_BatchJobs.clear();
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
        return;
    }

    double      comm_elapsed = m_BatchStopWatch.Elapsed();

    // BTCH logging is in a separate context
    CRef<CRequestContext>  current_context(& CDiagContext::GetRequestContext());
    try {
        if (x_NeedCmdLogging()) {
            CRequestContext *   ctx(new CRequestContext());
            ctx->SetRequestID();
            GetDiagContext().SetRequestContext(ctx);
            GetDiagContext().PrintRequestStart()
                            .Print("_type", "cmd")
                            .Print("conn", x_GetConnRef())
                            .Print("_queue", m_QueueName)
                            .Print("bsub", m_CmdContext->GetRequestID())
                            .Print("cmd", "BTCH")
                            .Print("size", m_BatchJobs.size());
            ctx->SetRequestStatus(CNetScheduleHandler::eStatus_OK);
        }

        // we have our batch now
        CStopWatch  sw(CStopWatch::eStart);
        x_ClearRollbackAction();
        unsigned    job_id = GetQueue()->SubmitBatch(m_ClientId,
                                                     m_BatchJobs,
                                                     m_BatchGroup,
                                                     x_NeedCmdLogging(),
                                                     m_RollbackAction);
        double      db_elapsed = sw.Elapsed();

        if (x_NeedCmdLogging())
            GetDiagContext().Extra()
                .Print("start_job", job_id)
                .Print("commit_time",
                       NStr::DoubleToString(comm_elapsed, 4,
                                            NStr::fDoubleFixed))
                .Print("transaction_time",
                       NStr::DoubleToString(db_elapsed, 4,
                                            NStr::fDoubleFixed));

        x_WriteMessage("OK:" + NStr::NumericToString(job_id) + " " +
                       m_Server->GetHost().c_str() + " " +
                       NStr::NumericToString(unsigned(m_Server->GetPort())) +
                       kEndOfResponse);
        x_ClearRollbackAction();
    }
    catch (const CNetScheduleException &  ex) {
        m_WithinBatchSubmit = false;
        m_Server->DecrementCurrentSubmitsCounter();
        if (x_NeedCmdLogging()) {
            CDiagContext::GetRequestContext().
                            SetRequestStatus(ex.ErrCodeToHTTPStatusCode());
            GetDiagContext().PrintRequestStop();
            GetDiagContext().SetRequestContext(current_context);
        }
        m_BatchJobs.clear();
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
        throw;
    }
    catch (...) {
        m_WithinBatchSubmit = false;
        m_Server->DecrementCurrentSubmitsCounter();
        if (x_NeedCmdLogging()) {
            CDiagContext::GetRequestContext().
                            SetRequestStatus(eStatus_ServerError);
            GetDiagContext().PrintRequestStop();
            GetDiagContext().SetRequestContext(current_context);
        }
        m_BatchJobs.clear();
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
        throw;
    }

    if (x_NeedCmdLogging()) {
        GetDiagContext().PrintRequestStop();
        GetDiagContext().SetRequestContext(current_context);
    }

    m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgBatchHeader;
}


//////////////////////////////////////////////////////////////////////////
// Process* methods for processing commands

void CNetScheduleHandler::x_ProcessFastStatusS(CQueue* q)
{
    bool            cmdv2(m_CommandArguments.cmd == "SST2");
    CNSPreciseTime  lifetime;
    CJob            job;
    TJobStatus      status = q->GetStatusAndLifetimeAndTouch(
                                                m_CommandArguments.job_id,
                                                job, &lifetime);


    if (status == CNetScheduleAPI::eJobNotFound) {
        ERR_POST(Warning << m_CommandArguments.cmd
                         << " for unknown job: "
                         << m_CommandArguments.job_key);
        x_SetCmdRequestStatus(eStatus_NotFound);

        if (cmdv2)
            x_WriteMessage(kErrNoJobFoundResponse);
        else
            x_WriteMessage(kOKResponsePrefix +
                           NStr::NumericToString((int) status) +
                           kEndOfResponse);
    } else {
        if (cmdv2) {
            string                  pause_status_msg;
            CQueue::TPauseStatus    pause_status = q->GetPauseStatus();

            if (pause_status == CQueue::ePauseWithPullback)
                pause_status_msg = "&pause=pullback";
            else if (pause_status == CQueue::ePauseWithoutPullback)
                pause_status_msg = "&pause=nopullback";

            x_WriteMessage("OK:job_status=" +
                           CNetScheduleAPI::StatusToString(status) +
                           "&job_exptime=" +
                           NStr::NumericToString(lifetime.Sec()) +
                           pause_status_msg + kEndOfResponse);
        }
        else
            x_WriteMessage(kOKResponsePrefix +
                           NStr::NumericToString((int) status) +
                           kEndOfResponse);
        x_LogCommandWithJob(job);
    }
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessFastStatusW(CQueue* q)
{
    bool            cmdv2(m_CommandArguments.cmd == "WST2");
    CNSPreciseTime  lifetime;
    string          client_ip;
    string          client_sid;
    string          client_phid;
    TJobStatus      status = q->GetStatusAndLifetime(m_CommandArguments.job_id,
                                                     client_ip, client_sid,
                                                     client_phid, &lifetime);


    if (status == CNetScheduleAPI::eJobNotFound) {
        ERR_POST(Warning << m_CommandArguments.cmd
                         << " for unknown job: "
                         << m_CommandArguments.job_key);
        x_SetCmdRequestStatus(eStatus_NotFound);

        if (cmdv2)
            x_WriteMessage(kErrNoJobFoundResponse);
        else
            x_WriteMessage(kOKResponsePrefix +
                           NStr::NumericToString((int) status) +
                           kEndOfResponse);
    } else {
        if (cmdv2) {
            string                  pause_status_msg;
            CQueue::TPauseStatus    pause_status = q->GetPauseStatus();

            if (pause_status == CQueue::ePauseWithPullback)
                pause_status_msg = "&pause=pullback";
            else if (pause_status == CQueue::ePauseWithoutPullback)
                pause_status_msg = "&pause=nopullback";

            x_WriteMessage("OK:job_status=" +
                           CNetScheduleAPI::StatusToString(status) +
                           "&job_exptime=" +
                           NStr::NumericToString(lifetime.Sec()) +
                           pause_status_msg + kEndOfResponse);
        }
        else
            x_WriteMessage(kOKResponsePrefix +
                           NStr::NumericToString((int) status) +
                           kEndOfResponse);
        x_LogCommandWithJob(client_ip, client_sid, client_phid);
    }
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessChangeAffinity(CQueue* q)
{
    // This functionality requires client name and the session
    x_CheckNonAnonymousClient("use " + m_CommandArguments.cmd + " command");

    if (m_CommandArguments.aff_to_add.empty() &&
        m_CommandArguments.aff_to_del.empty()) {
        ERR_POST(Warning << m_CommandArguments.cmd
                         << " with neither add list nor del list");
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_WriteMessage("ERR:eInvalidParameter:" + kEndOfResponse);
        x_PrintCmdRequestStop();
        return;
    }

    list<string>    aff_to_add_list;
    list<string>    aff_to_del_list;

    NStr::Split(m_CommandArguments.aff_to_add,
                "\t,", aff_to_add_list, NStr::fSplit_NoMergeDelims);
    NStr::Split(m_CommandArguments.aff_to_del,
                "\t,", aff_to_del_list, NStr::fSplit_NoMergeDelims);

    // CXX-8843: remove '-' affinity if so
    aff_to_add_list.remove(k_NoAffinityToken);
    aff_to_del_list.remove(k_NoAffinityToken);

    // Check that the same affinity has not been mentioned in both add and del
    // lists
    for (list<string>::const_iterator k = aff_to_add_list.begin();
         k != aff_to_add_list.end(); ++k) {
        if (find(aff_to_del_list.begin(), aff_to_del_list.end(), *k) !=
                aff_to_del_list.end()) {
            x_SetCmdRequestStatus(eStatus_BadRequest);
            x_WriteMessage("ERR:eInvalidParameter:Affinity " + *k +
                           " is in both add and del lists" +
                           kEndOfResponse);
            x_PrintCmdRequestStop();
            return;
        }
    }


    // Here: prerequisites have been checked
    if (x_NeedCmdLogging() && m_ClientIdentificationPrinted == false)
        GetDiagContext().Extra()
                .Print("client_node", m_ClientId.GetNode())
                .Print("client_session", m_ClientId.GetSession());

    ECommandGroup   cmd_group = eGet;
    if (m_CommandArguments.cmd == "CHRAFF")
        cmd_group = eRead;

    list<string>  msgs = q->ChangeAffinity(m_ClientId, aff_to_add_list,
                                                       aff_to_del_list,
                                                       cmd_group);
    if (msgs.empty())
        x_WriteMessage(kOKCompleteResponse);
    else {
        string  msg;
        for (list<string>::const_iterator k = msgs.begin();
             k != msgs.end(); ++k)
            msg += "WARNING:" + *k +";";
        x_WriteMessage(kOKResponsePrefix + msg + kEndOfResponse);
    }
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessSetAffinity(CQueue* q)
{
    // This functionality requires client name and the session
    x_CheckNonAnonymousClient("use " + m_CommandArguments.cmd + " command");

    if (x_NeedCmdLogging() && m_ClientIdentificationPrinted == false)
        GetDiagContext().Extra()
                .Print("client_node", m_ClientId.GetNode())
                .Print("client_session", m_ClientId.GetSession());

    list<string>    aff_to_set;
    NStr::Split(m_CommandArguments.affinity_token,
                "\t,", aff_to_set, NStr::fSplit_NoMergeDelims);

    // CXX-8843: remove '-' affinity if so
    aff_to_set.remove(k_NoAffinityToken);

    ECommandGroup   cmd_group = eGet;
    if (m_CommandArguments.cmd == "SETRAFF")
        cmd_group = eRead;

    q->SetAffinity(m_ClientId, aff_to_set, cmd_group);

    x_WriteMessage(kOKCompleteResponse);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessSubmit(CQueue* q)
{
    if (q->GetRefuseSubmits() || m_Server->GetRefuseSubmits()) {
        x_SetCmdRequestStatus(eStatus_SubmitRefused);
        x_WriteMessage("ERR:eSubmitsDisabled:" + kEndOfResponse);
        x_PrintCmdRequestStop();
        return;
    }

    if (m_Server->IncrementCurrentSubmitsCounter() <
        kSubmitCounterInitialValue) {
        // This is a drained shutdown mode
        m_Server->DecrementCurrentSubmitsCounter();
        x_SetCmdRequestStatus(eStatus_SubmitRefused);
        x_WriteMessage("ERR:eSubmitsDisabled:" + kEndOfResponse);
        x_PrintCmdRequestStop();
        return;
    }

    // CXX-8843: if there is no affinity then replace it with the '-' affinity
    if (m_CommandArguments.affinity_token.empty())
        m_CommandArguments.affinity_token = k_NoAffinityToken;
    // CXX-8843: if there is no group then replace it with the '-' group
    if (m_CommandArguments.group.empty())
        m_CommandArguments.group = k_NoGroupToken;

    CJob        job(m_CommandArguments);
    try {
        x_ClearRollbackAction();
        unsigned int  job_id = q->Submit(m_ClientId, job,
                                         m_CommandArguments.affinity_token,
                                         m_CommandArguments.group,
                                         x_NeedCmdLogging(),
                                         m_RollbackAction);

        x_WriteMessage(kOKResponsePrefix + q->MakeJobKey(job_id) +
                       kEndOfResponse);
        x_ClearRollbackAction();
        m_Server->DecrementCurrentSubmitsCounter();
    } catch (...) {
        m_Server->DecrementCurrentSubmitsCounter();
        throw;
    }

    // There is no need to log the job key, it is logged at lower level
    // together with all the submitted job parameters
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessSubmitBatch(CQueue* q)
{
    if (q->GetRefuseSubmits() || m_Server->GetRefuseSubmits()) {
        x_SetCmdRequestStatus(eStatus_SubmitRefused);
        x_WriteMessage("ERR:eSubmitsDisabled:" + kEndOfResponse);
        x_PrintCmdRequestStop();
        return;
    }

    if (m_Server->IncrementCurrentSubmitsCounter() <
                                        kSubmitCounterInitialValue) {
        // This is a drained shutdown mode
        m_Server->DecrementCurrentSubmitsCounter();
        x_SetCmdRequestStatus(eStatus_SubmitRefused);
        x_WriteMessage("ERR:eSubmitsDisabled:" + kEndOfResponse);
        x_PrintCmdRequestStop();
        return;
    }

    try {
        // Memorize the fact that batch submit started
        m_WithinBatchSubmit = true;

        // CXX-8843: if no group is provided it is overwritten with '-' group
        // automatically
        if (m_CommandArguments.group.empty())
            m_CommandArguments.group = k_NoGroupToken;

        m_BatchSubmPort    = m_CommandArguments.port;
        m_BatchSubmTimeout = CNSPreciseTime(m_CommandArguments.timeout, 0);
        m_BatchClientIP    = m_CommandArguments.ip;
        m_BatchClientSID   = m_CommandArguments.sid;
        m_BatchNCBIPHID    = m_CommandArguments.ncbi_phid;
        m_BatchGroup       = m_CommandArguments.group;

        x_WriteMessage("OK:Batch submit ready" + kEndOfResponse);
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgBatchHeader;
    }
    catch (...) {
        // WriteMessage can generate an exception
        m_WithinBatchSubmit = false;
        m_Server->DecrementCurrentSubmitsCounter();
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
        throw;
    }
}


void CNetScheduleHandler::x_ProcessBatchStart(CQueue*)
{
    m_BatchPos = 0;
    m_BatchStopWatch.Restart();
    m_BatchJobs.resize(m_BatchSize);
    if (m_BatchSize)
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgBatchJob;
    else
        // Unfortunately, because batches can be generated by
        // client program, we better honor zero size ones.
        // Skip right to waiting for 'ENDB'.
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgBatchSubmit;
}


void CNetScheduleHandler::x_ProcessBatchSequenceEnd(CQueue*)
{
    m_WithinBatchSubmit = false;
    m_Server->DecrementCurrentSubmitsCounter();
    m_BatchJobs.clear();
    m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
    x_WriteMessage(kOKCompleteResponse);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessCancel(CQueue* q)
{
    // Job key or a group or an affinity or a status must be given
    if (m_CommandArguments.job_id == 0 &&
        m_CommandArguments.group.empty() &&
        m_CommandArguments.affinity_token.empty() &&
        m_CommandArguments.job_statuses_string.empty()) {
        if (x_NeedCmdLogging())
            ERR_POST(Warning <<
                     "Neither job key nor a group nor an "
                     "affinity nor a status list is provided "
                     "for the CANCEL command");
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_WriteMessage("ERR:eInvalidParameter:"
                       "Job key or any combination of a group and an affinity "
                       "and job statuses must be given" + kEndOfResponse);
        x_PrintCmdRequestStop();
        return;
    }

    if (m_CommandArguments.job_id != 0 &&
        (!m_CommandArguments.group.empty() ||
         !m_CommandArguments.affinity_token.empty() ||
         !m_CommandArguments.job_statuses_string.empty())) {
        if (x_NeedCmdLogging())
            ERR_POST(Warning <<
                     "CANCEL can accept either a job "
                     "key or any combination of a group "
                     "and an affinity and job statuses");
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_WriteMessage("ERR:eInvalidParameter:CANCEL can accept either a job "
                       "key or any combination of a group and an affinity and "
                       "job statuses" + kEndOfResponse);
        x_PrintCmdRequestStop();
        return;
    }


    // Here: arguments are checked. It is a certain job or any combination of
    //       a group and an affinity and job statuses
    if (!m_CommandArguments.group.empty() ||
        !m_CommandArguments.affinity_token.empty() ||
        !m_CommandArguments.job_statuses_string.empty()) {
        // CANCEL for a group and/or affinity and or job statuses

        vector<string>      warnings;
        vector<TJobStatus>  statuses;
        if (!m_CommandArguments.job_statuses_string.empty()) {

            bool                            reported = false;
            vector<TJobStatus>::iterator    k =
                                        m_CommandArguments.job_statuses.begin();
            while (k != m_CommandArguments.job_statuses.end() ) {
                if (*k == CNetScheduleAPI::eJobNotFound) {
                    if (!reported) {
                        warnings.push_back("eInvalidJobStatus:unknown job "
                                           "status in the status list");
                        if (x_NeedCmdLogging())
                            ERR_POST(Warning <<
                                     "Unknown job status in the status list. "
                                     "Ignore and continue.");
                        reported = true;
                    }
                    k = m_CommandArguments.job_statuses.erase(k);
                } else
                    ++k;
            }

            statuses = x_RemoveDuplicateStatuses(
                                    m_CommandArguments.job_statuses, warnings);

            // Here: no duplicates, no unresolved states. Check if there is
            //       the 'Canceled' state
            k = statuses.begin();
            while (k != statuses.end()) {
                if (*k == CNetScheduleAPI::eCanceled) {
                    warnings.push_back("eIgnoringCanceledStatus:attempt to "
                                       "cancel jobs in the 'Canceled' status");
                    if (x_NeedCmdLogging())
                        ERR_POST(Warning <<
                                 "Attempt to cancel jobs in the 'Canceled' "
                                 "status. Ignore and continue.");
                    statuses.erase(k);
                    break;
                }
                ++k;
            }
        }

        unsigned int    count = q->CancelSelectedJobs(
                                            m_ClientId,
                                            m_CommandArguments.group,
                                            m_CommandArguments.affinity_token,
                                            statuses,
                                            x_NeedCmdLogging(), warnings);
        if (warnings.empty())
            x_WriteMessage("OK:" + NStr::NumericToString(count) +
                           kEndOfResponse);
        else {
            string  msg;
            for (vector<string>::const_iterator  k = warnings.begin();
                 k != warnings.end(); ++k) {
                msg += "WARNING:" + *k + ";";
            }
            x_WriteMessage("OK:" + msg + NStr::NumericToString(count) +
                           kEndOfResponse);
        }

        x_PrintCmdRequestStop();
        return;
    }

    // Here: CANCEL for a job
    CJob        job;
    switch (q->Cancel(m_ClientId,
                      m_CommandArguments.job_id,
                      m_CommandArguments.job_key, job)) {
        case CNetScheduleAPI::eJobNotFound:
            if (x_NeedCmdLogging())
                ERR_POST(Warning <<
                         "CANCEL for unknown job: " <<
                         m_CommandArguments.job_key);
            x_SetCmdRequestStatus(eStatus_NotFound);
            x_WriteMessage("OK:WARNING:eJobNotFound:Job not found;0" +
                           kEndOfResponse);
            break;
        case CNetScheduleAPI::eCanceled:
            x_WriteMessage("OK:WARNING:eJobAlreadyCanceled:"
                           "Already canceled;0" + kEndOfResponse);
            x_LogCommandWithJob(job);
            break;
        default:
            x_WriteMessage("OK:1" + kEndOfResponse);
            x_LogCommandWithJob(job);
    }
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessStatus(CQueue* q)
{
    CJob            job;
    bool            cmdv2 = (m_CommandArguments.cmd == "STATUS2");
    CNSPreciseTime  lifetime;

    if (q->ReadAndTouchJob(m_CommandArguments.job_id,
                           job, &lifetime) == CNetScheduleAPI::eJobNotFound) {
        // Here: there is no such a job
        ERR_POST(Warning << m_CommandArguments.cmd
                         << " for unknown job: "
                         << m_CommandArguments.job_key);
        x_SetCmdRequestStatus(eStatus_NotFound);
        if (cmdv2)
            x_WriteMessage(kErrNoJobFoundResponse);
        else
            x_WriteMessage(kOKResponsePrefix +
                           NStr::NumericToString(
                                        (int)CNetScheduleAPI::eJobNotFound) +
                           kEndOfResponse);
        x_PrintCmdRequestStop();
        return;
    }

    // Here: the job was found
    if (cmdv2) {
        string                  pause_status_msg;
        CQueue::TPauseStatus    pause_status = q->GetPauseStatus();

        if (pause_status == CQueue::ePauseWithPullback)
            pause_status_msg = "&pause=pullback";
        else if (pause_status == CQueue::ePauseWithoutPullback)
            pause_status_msg = "&pause=nopullback";

        x_WriteMessage("OK:"
                       "job_status=" +
                       CNetScheduleAPI::StatusToString(job.GetStatus()) +
                       "&client_ip=" + NStr::URLEncode(job.GetClientIP()) +
                       "&client_sid=" + NStr::URLEncode(job.GetClientSID()) +
                       "&ncbi_phid=" + NStr::URLEncode(job.GetNCBIPHID()) +
                       "&job_exptime=" + NStr::NumericToString(lifetime.Sec()) +
                       "&ret_code=" + NStr::NumericToString(job.GetRetCode()) +
                       "&output=" + NStr::URLEncode(job.GetOutput()) +
                       "&err_msg=" + NStr::URLEncode(job.GetErrorMsg()) +
                       "&input=" + NStr::URLEncode(job.GetInput()) +
                       pause_status_msg + kEndOfResponse
                      );
    }
    else
        x_WriteMessage("OK:" + NStr::NumericToString((int) job.GetStatus()) +
                       " " + NStr::NumericToString(job.GetRetCode()) +
                       " \""   + NStr::PrintableString(job.GetOutput()) +
                       "\" \"" + NStr::PrintableString(job.GetErrorMsg()) +
                       "\" \"" + NStr::PrintableString(job.GetInput()) +
                       "\"" + kEndOfResponse);

    x_LogCommandWithJob(job);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessGetJob(CQueue* q)
{
    // GET & WGET are first versions of the command
    bool    cmdv2(m_CommandArguments.cmd == "GET2");

    if (cmdv2) {
        x_CheckNonAnonymousClient("use GET2 command");
        x_CheckPortAndTimeout();
        x_CheckGetParameters();
    }
    else {
        // The affinity options are only for the second version of the command
        m_CommandArguments.wnode_affinity = false;
        m_CommandArguments.exclusive_new_aff = false;

        // The old clients must have any_affinity set to true
        // depending on the explicit affinity - to conform the old behavior
        m_CommandArguments.any_affinity =
                                    m_CommandArguments.affinity_token.empty();
    }

    // Check if the queue is paused
    CQueue::TPauseStatus    pause_status = q->GetPauseStatus();
    if (pause_status != CQueue::eNoPause) {

        if (m_CommandArguments.timeout != 0)
            q->RegisterQueueResumeNotification(m_ClientId.GetAddress(),
                                               m_CommandArguments.port,
                                               cmdv2);

        string      pause_status_str;

        if (pause_status == CQueue::ePauseWithPullback)
            pause_status_str = "pullback";
        else
            pause_status_str = "nopullback";

        if (cmdv2)
            x_WriteMessage("OK:pause=" + pause_status_str + kEndOfResponse);
        else
            x_WriteMessage(kOKCompleteResponse);

        if (x_NeedCmdLogging())
            GetDiagContext().Extra().Print("job_key", "None")
                                    .Print("reason",
                                           "pause: " + pause_status_str);

        x_PrintCmdRequestStop();
        return;
    }


    list<string>    aff_list;
    NStr::Split(m_CommandArguments.affinity_token,
                "\t,", aff_list, NStr::fSplit_NoMergeDelims);
    list<string>    group_list;
    NStr::Split(m_CommandArguments.group,
                "\t,", group_list, NStr::fSplit_NoMergeDelims);

    CJob            job;
    string          added_pref_aff;
    x_ClearRollbackAction();
    if (q->GetJobOrWait(m_ClientId,
                        m_CommandArguments.port,
                        m_CommandArguments.timeout,
                        CNSPreciseTime::Current(), &aff_list,
                        m_CommandArguments.wnode_affinity,
                        m_CommandArguments.any_affinity,
                        m_CommandArguments.exclusive_new_aff,
                        m_CommandArguments.prioritized_aff,
                        cmdv2,
                        &group_list,
                        &job,
                        m_RollbackAction,
                        added_pref_aff) == false) {
        // Preferred affinities were reset for the client, so no job
        // and bad request
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_WriteMessage("ERR:ePrefAffExpired:" + kEndOfResponse);
    } else {
        if (job.GetId())
            x_LogCommandWithJob(job);

        if (!added_pref_aff.empty()) {
            if (x_NeedCmdLogging()) {
                if (m_ClientIdentificationPrinted)
                    GetDiagContext().Extra()
                        .Print("added_preferred_affinity", added_pref_aff);
                else
                    GetDiagContext().Extra()
                        .Print("client_node", m_ClientId.GetNode())
                        .Print("client_session", m_ClientId.GetSession())
                        .Print("added_preferred_affinity", added_pref_aff);
            }
        }
        x_PrintGetJobResponse(q, job, cmdv2);
        x_ClearRollbackAction();
    }

    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessCancelWaitGet(CQueue* q)
{
    x_CheckNonAnonymousClient("cancel waiting after WGET");

    q->CancelWaitGet(m_ClientId);
    x_WriteMessage(kOKCompleteResponse);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessCancelWaitRead(CQueue* q)
{
    x_CheckNonAnonymousClient("cancel waiting after READ");

    q->CancelWaitRead(m_ClientId);
    x_WriteMessage(kOKCompleteResponse);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessPut(CQueue* q)
{
    bool    cmdv2(m_CommandArguments.cmd == "PUT2");

    if (cmdv2) {
        x_CheckNonAnonymousClient("use PUT2 command");
        x_CheckAuthorizationToken();
    }

    CJob        job;
    TJobStatus  old_status = q->PutResult(m_ClientId, CNSPreciseTime::Current(),
                                          m_CommandArguments.job_id,
                                          m_CommandArguments.job_key,
                                          job,
                                          m_CommandArguments.auth_token,
                                          m_CommandArguments.job_return_code,
                                          m_CommandArguments.output);
    if (old_status == CNetScheduleAPI::ePending ||
        old_status == CNetScheduleAPI::eRunning) {
        x_WriteMessage(kOKCompleteResponse);
        x_LogCommandWithJob(job);
        x_PrintCmdRequestStop();
        return;
    }
    if (old_status == CNetScheduleAPI::eFailed) {
        // Still accept the job results, but print a warning: CXX-3632
        ERR_POST(Warning << "Accepting results for a job in the FAILED state.");
        x_WriteMessage(kOKCompleteResponse);
        x_LogCommandWithJob(job);
        x_PrintCmdRequestStop();
        return;
    }
    if (old_status == CNetScheduleAPI::eDone) {
        ERR_POST(Warning << "Cannot accept job "
                         << m_CommandArguments.job_key
                         << " results. The job has already been done.");
        x_WriteMessage("OK:WARNING:eJobAlreadyDone:Already done;" +
                       kEndOfResponse);
        x_LogCommandWithJob(job);
        x_PrintCmdRequestStop();
        return;
    }
    if (old_status == CNetScheduleAPI::eJobNotFound) {
        ERR_POST(Warning << "Cannot accept job "
                         << m_CommandArguments.job_key
                         << " results. The job is unknown");
        x_SetCmdRequestStatus(eStatus_NotFound);
        x_WriteMessage(kErrNoJobFoundResponse);
        x_PrintCmdRequestStop();
        return;
    }

    // Here: invalid job status, nothing will be done
    ERR_POST(Warning << "Cannot accept job "
                     << m_CommandArguments.job_key
                     << " results; job is in "
                     << CNetScheduleAPI::StatusToString(old_status)
                     << " state");
    x_SetCmdRequestStatus(eStatus_InvalidJobStatus);
    x_WriteMessage("ERR:eInvalidJobStatus:"
                   "Cannot accept job results; job is in " +
                   CNetScheduleAPI::StatusToString(old_status) + " state" +
                   kEndOfResponse);
    x_LogCommandWithJob(job);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessJobExchange(CQueue* q)
{
    // The JXCG command is used only by old clients. All new client should use
    // PUT2 + GET2 sequence.
    // The old clients must have any_affinity set to true
    // depending on the explicit affinity - to conform the old behavior
    m_CommandArguments.any_affinity = m_CommandArguments.affinity_token.empty();


    CNSPreciseTime  curr = CNSPreciseTime::Current();

    // PUT part
    CJob            job;
    TJobStatus      old_status = q->PutResult(m_ClientId, curr,
                                          m_CommandArguments.job_id,
                                          m_CommandArguments.job_key,
                                          job,
                                          m_CommandArguments.auth_token,
                                          m_CommandArguments.job_return_code,
                                          m_CommandArguments.output);

    if (old_status == CNetScheduleAPI::eJobNotFound) {
        ERR_POST(Warning << "Cannot accept job "
                         << m_CommandArguments.job_key
                         << " results. The job is unknown");
    } else if (old_status == CNetScheduleAPI::eFailed) {
        // Still accept the job results, but print a warning: CXX-3632
        ERR_POST(Warning << "Accepting results for a job in the FAILED state.");
        x_LogCommandWithJob(job);
    } else if (old_status != CNetScheduleAPI::ePending &&
               old_status != CNetScheduleAPI::eRunning) {
        x_LogCommandWithJob(job);
        ERR_POST(Warning << "Cannot accept job "
                         << m_CommandArguments.job_key
                         << " results. The job has already been done.");
    } else {
        x_LogCommandWithJob(job);
    }


    // Get part
    CQueue::TPauseStatus    pause_status = q->GetPauseStatus();
    if (pause_status != CQueue::eNoPause) {

        if (m_CommandArguments.timeout != 0)
            q->RegisterQueueResumeNotification(m_ClientId.GetAddress(),
                                               m_CommandArguments.port,
                                               false);

        x_WriteMessage(kOKCompleteResponse);
        if (x_NeedCmdLogging()) {
            string      pause_status_str;

            if (pause_status == CQueue::ePauseWithPullback)
                pause_status_str = "pullback";
            else
                pause_status_str = "nopullback";

            GetDiagContext().Extra().Print("job_key", "None")
                                    .Print("reason",
                                           "pause: " + pause_status_str);
        }
        x_PrintCmdRequestStop();
        return;
    }

    list<string>        aff_list;
    NStr::Split(m_CommandArguments.affinity_token,
                "\t,", aff_list, NStr::fSplit_NoMergeDelims);

    string              added_pref_aff;
    x_ClearRollbackAction();
    if (q->GetJobOrWait(m_ClientId,
                        m_CommandArguments.port,
                        m_CommandArguments.timeout,
                        curr, &aff_list,
                        m_CommandArguments.wnode_affinity,
                        m_CommandArguments.any_affinity,
                        false,
                        false,
                        false,
                        NULL,
                        &job,
                        m_RollbackAction,
                        added_pref_aff) == false) {
        // Preferred affinities were reset for the client, or the client had
        // been garbage collected so no job and bad request
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_WriteMessage("ERR:ePrefAffExpired:" + kEndOfResponse);
    } else {
        if (added_pref_aff.empty() == false) {
            if (x_NeedCmdLogging()) {
                if (m_ClientIdentificationPrinted)
                    GetDiagContext().Extra()
                        .Print("added_preferred_affinity", added_pref_aff);
                else
                    GetDiagContext().Extra()
                        .Print("client_node", m_ClientId.GetNode())
                        .Print("client_session", m_ClientId.GetSession())
                        .Print("added_preferred_affinity", added_pref_aff);
            }
        }
        x_PrintGetJobResponse(q, job, false);
        x_ClearRollbackAction();
    }

    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessPutMessage(CQueue* q)
{
    CJob        job;

    if (q->PutProgressMessage(m_CommandArguments.job_id, job,
                              m_CommandArguments.progress_msg)) {
        x_WriteMessage(kOKCompleteResponse);
        x_LogCommandWithJob(job);
    } else {
        ERR_POST(Warning << "MPUT for unknown job "
                         << m_CommandArguments.job_key);
        x_SetCmdRequestStatus(eStatus_NotFound);
        x_WriteMessage(kErrNoJobFoundResponse);
    }
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessGetMessage(CQueue* q)
{
    CJob            job;
    CNSPreciseTime  lifetime;

    if (q->ReadAndTouchJob(m_CommandArguments.job_id,
                           job, &lifetime) != CNetScheduleAPI::eJobNotFound) {
        x_WriteMessage("OK:" + NStr::PrintableString(job.GetProgressMsg()) +
                       kEndOfResponse);
        x_LogCommandWithJob(job);
    } else {
        ERR_POST(Warning << m_CommandArguments.cmd
                         << "MGET for unknown job "
                         << m_CommandArguments.job_key);
        x_SetCmdRequestStatus(eStatus_NotFound);
        x_WriteMessage(kErrNoJobFoundResponse);
    }
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessPutFailure(CQueue* q)
{
    bool    cmdv2(m_CommandArguments.cmd == "FPUT2");

    if (cmdv2) {
        x_CheckNonAnonymousClient("use FPUT2 command");
        x_CheckAuthorizationToken();
    }

    CJob        job;
    string      warning;
    TJobStatus  old_status = q->FailJob(
                                m_ClientId,
                                m_CommandArguments.job_id,
                                m_CommandArguments.job_key,
                                job,
                                m_CommandArguments.auth_token,
                                m_CommandArguments.err_msg,
                                m_CommandArguments.output,
                                m_CommandArguments.job_return_code,
                                m_CommandArguments.no_retries,
                                warning);

    if (old_status == CNetScheduleAPI::eJobNotFound) {
        ERR_POST(Warning << "FPUT for unknown job "
                         << m_CommandArguments.job_key);
        x_SetCmdRequestStatus(eStatus_NotFound);
        x_WriteMessage(kErrNoJobFoundResponse);
        x_PrintCmdRequestStop();
        return;
    }

    if (old_status == CNetScheduleAPI::eFailed) {
        ERR_POST(Warning << "FPUT for already failed job "
                         << m_CommandArguments.job_key);
        x_WriteMessage("OK:WARNING:eJobAlreadyFailed:Already failed;" +
                       kEndOfResponse);
        x_LogCommandWithJob(job);
        x_PrintCmdRequestStop();
        return;
    }

    if (old_status != CNetScheduleAPI::eRunning) {
        ERR_POST(Warning << "Cannot fail job "
                         << m_CommandArguments.job_key
                         << "; job is in "
                         << CNetScheduleAPI::StatusToString(old_status)
                         << " state");
        x_SetCmdRequestStatus(eStatus_InvalidJobStatus);
        x_WriteMessage("ERR:eInvalidJobStatus:Cannot fail job; job is in " +
                       CNetScheduleAPI::StatusToString(old_status) + " state" +
                       kEndOfResponse);
        x_LogCommandWithJob(job);
        x_PrintCmdRequestStop();
        return;
    }

    // Here: all is fine
    if (warning.empty())
        x_WriteMessage(kOKCompleteResponse);
    else
        x_WriteMessage("OK:WARNING:" + warning + ";" + kEndOfResponse);
    x_LogCommandWithJob(job);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessDropQueue(CQueue* q)
{
    // The DROPQ implementation has been changed in NS 4.23.2
    // Earlier it was removing jobs from the queue
    // Starting from NS 4.23.2 it is cancelling all the jobs, i.e
    // DROPQ became an equivalent of the CANCELQ command except of the
    // output. DROPQ provides OK: while CANCELQ provides OK:N where N is
    // the number of the canceled jobs.
    q->CancelAllJobs(m_ClientId, x_NeedCmdLogging());
    x_WriteMessage(kOKCompleteResponse);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessReturn(CQueue* q)
{
    bool                        cmdv2(m_CommandArguments.cmd == "RETURN2");
    CQueue::TJobReturnOption    return_option = CQueue::eWithBlacklist;

    if (cmdv2) {
        x_CheckNonAnonymousClient("use RETURN2 command");
        x_CheckAuthorizationToken();

        if (m_CommandArguments.blacklist)
            return_option = CQueue::eWithBlacklist;
        else
            return_option = CQueue::eWithoutBlacklist;
    }

    CJob            job;
    string          warning;
    TJobStatus      old_status = q->ReturnJob(m_ClientId,
                                              m_CommandArguments.job_id,
                                              m_CommandArguments.job_key,
                                              job,
                                              m_CommandArguments.auth_token,
                                              warning, return_option);

    if (old_status == CNetScheduleAPI::eRunning) {
        if (warning.empty())
            x_WriteMessage(kOKCompleteResponse);
        else
            x_WriteMessage("OK:WARNING:" + warning + ";" + kEndOfResponse);
        x_PrintCmdRequestStop();
        x_LogCommandWithJob(job);
        return;
    }

    if (old_status == CNetScheduleAPI::eJobNotFound) {
        ERR_POST(Warning << "RETURN for unknown job "
                         << m_CommandArguments.job_key);
        x_SetCmdRequestStatus(eStatus_NotFound);
        x_WriteMessage(kErrNoJobFoundResponse);
        x_PrintCmdRequestStop();
        return;
    }

    ERR_POST(Warning << "Cannot return job "
                     << m_CommandArguments.job_key
                     << "; job is in "
                     << CNetScheduleAPI::StatusToString(old_status)
                     << " state");
    x_SetCmdRequestStatus(eStatus_InvalidJobStatus);
    x_WriteMessage("ERR:eInvalidJobStatus:Cannot return job; job is in " +
                   CNetScheduleAPI::StatusToString(old_status) + " state" +
                   kEndOfResponse);

    x_LogCommandWithJob(job);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessReschedule(CQueue* q)
{
    x_CheckNonAnonymousClient("use RESCHEDULE command");
    x_CheckAuthorizationToken();

    CJob            job;
    bool            auth_token_ok = true;

    // CXX-8843: replace no affinity with "-" affinity
    if (m_CommandArguments.affinity_token.empty())
        m_CommandArguments.affinity_token = k_NoAffinityToken;

    // CXX-8843: replace no group with "-" group
    if (m_CommandArguments.group.empty())
        m_CommandArguments.group = k_NoGroupToken;

    TJobStatus      old_status = q->RescheduleJob(
                                        m_ClientId,
                                        m_CommandArguments.job_id,
                                        m_CommandArguments.job_key,
                                        m_CommandArguments.auth_token,
                                        m_CommandArguments.affinity_token,
                                        m_CommandArguments.group,
                                        auth_token_ok,
                                        job);

    if (!auth_token_ok) {
        ERR_POST(Warning << "Invalid authorization token");
        x_SetCmdRequestStatus(eStatus_BadAuth);
        x_WriteMessage("ERR:eInvalidAuthToken:" + kEndOfResponse);
        x_PrintCmdRequestStop();
        return;
    }

    if (old_status == CNetScheduleAPI::eRunning) {
        x_WriteMessage(kOKCompleteResponse);
        x_PrintCmdRequestStop();
        x_LogCommandWithJob(job);
        return;
    }

    if (old_status == CNetScheduleAPI::eJobNotFound) {
        ERR_POST(Warning << "RESCHEDULE for unknown job "
                         << m_CommandArguments.job_key);
        x_SetCmdRequestStatus(eStatus_NotFound);
        x_WriteMessage(kErrNoJobFoundResponse);
        x_PrintCmdRequestStop();
        return;
    }

    ERR_POST(Warning << "Cannot reschedule job "
                     << m_CommandArguments.job_key
                     << "; job is in "
                     << CNetScheduleAPI::StatusToString(old_status)
                     << " state");
    x_SetCmdRequestStatus(eStatus_InvalidJobStatus);
    x_WriteMessage("ERR:eInvalidJobStatus:Cannot reschedule job; job is in " +
                   CNetScheduleAPI::StatusToString(old_status) + " state" +
                   kEndOfResponse);

    x_LogCommandWithJob(job);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessRedo(CQueue* q)
{
    x_CheckNonAnonymousClient("use REDO command");

    CJob            job;
    TJobStatus      old_status = q->RedoJob(m_ClientId,
                                            m_CommandArguments.job_id,
                                            m_CommandArguments.job_key,
                                            job);

    if (old_status == CNetScheduleAPI::eJobNotFound) {
        ERR_POST(Warning << "REDO for unknown job "
                         << m_CommandArguments.job_key);
        x_SetCmdRequestStatus(eStatus_NotFound);
        x_WriteMessage(kErrNoJobFoundResponse);
        x_PrintCmdRequestStop();
        return;
    }

    if (old_status == CNetScheduleAPI::ePending ||
        old_status == CNetScheduleAPI::eRunning ||
        old_status == CNetScheduleAPI::eReading) {
        ERR_POST(Warning << "Cannot redo job "
                     << m_CommandArguments.job_key
                     << "; job is in "
                     << CNetScheduleAPI::StatusToString(old_status)
                     << " state");
        x_SetCmdRequestStatus(eStatus_InvalidJobStatus);
        x_WriteMessage("ERR:eInvalidJobStatus:Cannot redo job; job is in " +
                       CNetScheduleAPI::StatusToString(old_status) + " state" +
                       kEndOfResponse);
    } else {
        x_WriteMessage(kOKCompleteResponse);
    }

    x_LogCommandWithJob(job);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessJobDelayExpiration(CQueue* q)
{
    if (m_CommandArguments.timeout <= 0) {
        ERR_POST(Warning << "Invalid timeout "
                         << m_CommandArguments.timeout
                         << " in JDEX for job "
                         << m_CommandArguments.job_key);
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_WriteMessage("ERR:eInvalidParameter:" + kEndOfResponse);
        x_PrintCmdRequestStop();
        return;
    }

    CJob            job;
    CNSPreciseTime  timeout(m_CommandArguments.timeout, 0);
    TJobStatus      status = q->JobDelayExpiration(m_CommandArguments.job_id,
                                                   job, timeout);

    if (status == CNetScheduleAPI::eJobNotFound) {
        ERR_POST(Warning << "JDEX for unknown job "
                         << m_CommandArguments.job_key);
        x_SetCmdRequestStatus(eStatus_NotFound);
        x_WriteMessage(kErrNoJobFoundResponse);
        x_PrintCmdRequestStop();
        return;
    }
    if (status != CNetScheduleAPI::eRunning) {
        ERR_POST(Warning << "Cannot change expiration for job "
                         << m_CommandArguments.job_key
                         << " in status "
                         << CNetScheduleAPI::StatusToString(status));
        x_SetCmdRequestStatus(eStatus_InvalidJobStatus);
        x_WriteMessage("ERR:eInvalidJobStatus:" +
                       CNetScheduleAPI::StatusToString(status) +
                       kEndOfResponse);
        x_LogCommandWithJob(job);
        x_PrintCmdRequestStop();
        return;
    }

    // Here: the new timeout has been applied
    x_WriteMessage(kOKCompleteResponse);
    x_LogCommandWithJob(job);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessJobDelayReadExpiration(CQueue* q)
{
    if (m_CommandArguments.timeout <= 0) {
        ERR_POST(Warning << "Invalid timeout "
                         << m_CommandArguments.timeout
                         << " in JDREX for job "
                         << m_CommandArguments.job_key);
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_WriteMessage("ERR:eInvalidParameter:" + kEndOfResponse);
        x_PrintCmdRequestStop();
        return;
    }

    CJob            job;
    CNSPreciseTime  timeout(m_CommandArguments.timeout, 0);
    TJobStatus      status = q->JobDelayReadExpiration(
                                            m_CommandArguments.job_id,
                                            job, timeout);

    if (status == CNetScheduleAPI::eJobNotFound) {
        ERR_POST(Warning << "JDREX for unknown job "
                         << m_CommandArguments.job_key);
        x_SetCmdRequestStatus(eStatus_NotFound);
        x_WriteMessage(kErrNoJobFoundResponse);
        x_PrintCmdRequestStop();
        return;
    }
    if (status != CNetScheduleAPI::eReading) {
        ERR_POST(Warning << "Cannot change read expiration for job "
                         << m_CommandArguments.job_key
                         << " in status "
                         << CNetScheduleAPI::StatusToString(status));
        x_SetCmdRequestStatus(eStatus_InvalidJobStatus);
        x_WriteMessage("ERR:eInvalidJobStatus:" +
                       CNetScheduleAPI::StatusToString(status) +
                       kEndOfResponse);
        x_LogCommandWithJob(job);
        x_PrintCmdRequestStop();
        return;
    }

    // Here: the new timeout has been applied
    x_WriteMessage(kOKCompleteResponse);
    x_LogCommandWithJob(job);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessListenJob(CQueue* q)
{
    size_t          last_event_index = 0;
    CNSPreciseTime  timeout(m_CommandArguments.timeout, 0);
    CJob            job;
    TJobStatus      status = q->SetJobListener(
                                    m_CommandArguments.job_id, job,
                                    m_ClientId.GetAddress(),
                                    m_CommandArguments.port,
                                    timeout,
                                    &last_event_index);

    if (status == CNetScheduleAPI::eJobNotFound) {
        ERR_POST(Warning << "LISTEN for unknown job "
                         << m_CommandArguments.job_key);
        x_SetCmdRequestStatus(eStatus_NotFound);
        x_WriteMessage(kErrNoJobFoundResponse);
    } else {
        x_WriteMessage("OK:job_status=" +
                       CNetScheduleAPI::StatusToString(status) +
                       "&last_event_index=" +
                       NStr::NumericToString(last_event_index) +
                       kEndOfResponse);
        x_LogCommandWithJob(job);
    }

    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessStatistics(CQueue* q)
{
    CSocket &           socket = GetSocket();
    const string &      what   = m_CommandArguments.option;
    CNSPreciseTime      curr   = CNSPreciseTime::Current();

    if (!what.empty() && what != "QCLASSES" && what != "QUEUES" &&
        what != "JOBS" && what != "ALL" && what != "CLIENTS" &&
        what != "NOTIFICATIONS" && what != "AFFINITIES" &&
        what != "GROUPS" && what != "WNODE" && what != "SERVICES" &&
        what != "ALERTS" && what != "SCOPES") {
        NCBI_THROW(CNetScheduleException, eInvalidParameter,
                   "Unsupported '" + what +
                   "' parameter for the STAT command.");
    }

    if (q == NULL && (what == "CLIENTS" || what == "NOTIFICATIONS" ||
                      what == "AFFINITIES" || what == "GROUPS" ||
                      what == "WNODE" || what == "SCOPES")) {
        NCBI_THROW(CNetScheduleException, eInvalidParameter,
                   "STAT " + what + " requires a queue");
    }

    if (q != NULL)
        q->MarkClientAsAdmin(m_ClientId);

    if (what == "QCLASSES") {
        string      info = m_Server->GetQueueClassesInfo();

        if (!info.empty())
            info += kEndOfResponse;

        x_WriteMessage(info + "OK:END" + kEndOfResponse);
        x_PrintCmdRequestStop();
        return;
    }
    if (what == "QUEUES") {
        string      info = m_Server->GetQueueInfo();

        if (!info.empty())
            info += kEndOfResponse;

        x_WriteMessage(info + "OK:END" + kEndOfResponse);
        x_PrintCmdRequestStop();
        return;
    }
    if (what == "SERVICES") {
        string                  output;
        map<string, string>     services;
        m_Server->GetServices(services);
        for (map<string, string>::const_iterator  k = services.begin();
                k != services.end(); ++k) {
            if (!output.empty())
                output += "&";
            output += k->first + "=" + k->second;
        }
        x_WriteMessage("OK:" + output + kEndOfResponse);
        x_PrintCmdRequestStop();
        return;
    }
    if (what == "ALERTS") {
        string  output = m_Server->SerializeAlerts();
        if (!output.empty())
            output += kEndOfResponse;
        x_WriteMessage(output + "OK:END" + kEndOfResponse);
        x_PrintCmdRequestStop();
        return;
    }

    if (q == NULL) {
        string      info;
        if (what == "JOBS")
            info = m_Server->PrintJobsStat(m_ClientId) + kEndOfResponse;
        else {
            // Transition counters for all the queues
            info = "OK:Started: " + m_Server->GetStartTime().AsString() +
                   kEndOfResponse;

            info += "OK:SubmitsDisabledEffective: ";
            if (m_Server->GetRefuseSubmits())   info += "1";
            else                                info += "0";
            info += kEndOfResponse;

            info += "OK:DrainedShutdown: ";
            if (m_Server->IsDrainShutdown())    info += "1";
            else                                info += "0";
            info += kEndOfResponse;

            info += m_Server->PrintTransitionCounters() + kEndOfResponse;
        }

        x_MakeSureSingleEOR(info);
        x_WriteMessage(info + "OK:END" + kEndOfResponse);
        x_PrintCmdRequestStop();
        return;
    }


    socket.DisableOSSendDelay(false);
    if (!what.empty() && what != "ALL") {
        x_StatisticsNew(q, what, curr);
        return;
    }


    string      info = "OK:Started: " +
                       m_Server->GetStartTime().AsString() + kEndOfResponse;

    info += "OK:SubmitsDisabledEffective: ";
    if (m_Server->GetRefuseSubmits() || q->GetRefuseSubmits())  info += "1";
    else                                                        info += "0";
    info += kEndOfResponse;

    info += "OK:SubmitsDisabledPrivate: ";
    if (q->GetRefuseSubmits())      info += "1";
    else                            info += "0";
    info += kEndOfResponse;

    for (size_t  k = 0; k < g_ValidJobStatusesSize; ++k) {
        TJobStatus      st = g_ValidJobStatuses[k];
        unsigned        count = q->CountStatus(st);

        info += "OK:" + CNetScheduleAPI::StatusToString(st) + ": " +
                NStr::NumericToString(count) + kEndOfResponse;

        if (what == "ALL") {
            TNSBitVector::statistics bv_stat;
            q->StatusStatistics(st, &bv_stat);
            info += "OK:"
                    "  bit_blk=" +
                    NStr::NumericToString(bv_stat.bit_blocks) +
                    "; gap_blk=" +
                    NStr::NumericToString(bv_stat.gap_blocks) +
                    "; mem_used=" +
                    NStr::NumericToString(bv_stat.memory_used) +
                    kEndOfResponse;
        }
    } // for


    if (what == "ALL") {
        info += "OK:[Berkeley DB Mutexes]:" + kEndOfResponse;
        {{
            CNcbiOstrstream ostr;

            m_Server->PrintMutexStat(ostr);
            info += "OK:" + (string)CNcbiOstrstreamToString(ostr) +
                    kEndOfResponse;
        }}

        info += "OK:[Berkeley DB Locks]:" + kEndOfResponse;
        {{
            CNcbiOstrstream ostr;

            m_Server->PrintLockStat(ostr);
            info += "OK:" + (string)CNcbiOstrstreamToString(ostr) +
                    kEndOfResponse;
        }}

        info += "OK:[Berkeley DB Memory Usage]:" + kEndOfResponse;
        {{
            CNcbiOstrstream ostr;

            m_Server->PrintMemStat(ostr);
            info += "OK:" + (string)CNcbiOstrstreamToString(ostr) +
                    kEndOfResponse;
        }}
    }

    x_MakeSureSingleEOR(info);
    x_WriteMessage(info +
                   "OK:[Transitions counters]:" + kEndOfResponse +
                   q->PrintTransitionCounters() + kEndOfResponse +
                   "OK:END" + kEndOfResponse);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessReloadConfig(CQueue* q)
{
    // Check permissions explicitly due to additional case (the encrypted admin
    // names could be not available)
    if (!m_Server->IsAdminClientName(m_ClientId.GetClientName()) &&
        !m_Server->AnybodyCanReconfigure()) {
            m_Server->RegisterAlert(eAccess, "admin privileges required "
                                             "to execute RECO");
            NCBI_THROW(CNetScheduleException, eAccessDenied,
                       "Access denied: admin privileges required");
    }

    // Unconditionally reload the decryptor in case if the key files are
    // changed
    CNcbiEncrypt::Reload();

    CNcbiApplication *      app = CNcbiApplication::Instance();
    bool                    reloaded = app->ReloadConfig(
                                            CMetaRegistry::fReloadIfChanged);

    if (reloaded || m_Server->AnybodyCanReconfigure()) {
        const CNcbiRegistry &   reg = app->GetConfig();
        vector<string>          config_warnings;
        bool                    admin_decrypt_error(false); // ignored here
        NS_ValidateConfigFile(reg, config_warnings, false, admin_decrypt_error);

        if (!config_warnings.empty()) {
            string      msg;
            string      alert_msg;
            for (vector<string>::const_iterator k = config_warnings.begin();
                 k != config_warnings.end(); ++k) {
                ERR_POST(*k);
                if (!msg.empty()) {
                    msg += "; ";
                    alert_msg += "\n";
                }
                msg += *k;
                alert_msg += *k;
            }
            m_Server->RegisterAlert(eReconfigure, alert_msg);
            x_SetCmdRequestStatus(eStatus_BadRequest);

            msg = "ERR:eInvalidParameter:Configuration file is not "
                  "well formed. " + msg;
            if (msg.size() > 1024) {
                msg.resize(1024);
                msg += " TRUNCATED";
            }

            x_WriteMessage(msg + kEndOfResponse);
            x_PrintCmdRequestStop();
            return;
        }

        // Update the config file checksum in memory
        vector<string>  config_checksum_warnings;
        string          config_checksum = NS_GetConfigFileChecksum(
                                app->GetConfigPath(), config_checksum_warnings);
        if (config_checksum_warnings.empty()) {
            m_Server->SetRAMConfigFileChecksum(config_checksum);
            m_Server->SetDiskConfigFileChecksum(config_checksum);
        } else {
            for (vector<string>::const_iterator
                    k = config_checksum_warnings.begin();
                    k != config_checksum_warnings.end(); ++k)
                ERR_POST(*k);
        }

        // Logging from the [server] section
        SNS_Parameters          params;
        params.Read(reg);

        CJsonNode   what_changed = m_Server->SetNSParameters(params, true);
        CJsonNode   services_changed = m_Server->ReadServicesConfig(reg);

        CJsonNode   diff = CJsonNode::NewObjectNode();
        m_Server->Configure(reg, diff);
        m_Server->SetAnybodyCanReconfigure(false);

        if (what_changed.GetSize() == 0 &&
            diff.GetSize() == 0 &&
            services_changed.GetSize() == 0) {
            m_Server->AcknowledgeAlert(eReconfigure, "NSAcknowledge");
            m_Server->AcknowledgeAlert(eConfigOutOfSync, "NSAcknowledge");
            if (x_NeedCmdLogging())
                 GetDiagContext().Extra().Print("accepted_changes", "none");
            x_WriteMessage("OK:WARNING:eNoParametersChanged:No changes in "
                           "changeable parameters were identified in the new "
                           "cofiguration file;" + kEndOfResponse);
            x_PrintCmdRequestStop();
            return;
        }

        // Merge the changes
        for (CJsonIterator k = what_changed.Iterate(); k; ++k)
            diff.SetByKey(k.GetKey(), k.GetNode());
        for (CJsonIterator k = services_changed.Iterate(); k; ++k)
            diff.SetByKey(k.GetKey(), k.GetNode());

        string      diff_as_string = diff.Repr();
        if (x_NeedCmdLogging())
            GetDiagContext().Extra().Print("config_changes", diff_as_string);

        m_Server->AcknowledgeAlert(eReconfigure, "NSAcknowledge");
        m_Server->AcknowledgeAlert(eConfigOutOfSync, "NSAcknowledge");
        x_WriteMessage("OK:" + diff_as_string + kEndOfResponse);
    }
    else
        x_WriteMessage("OK:WARNING:eConfigFileNotChanged:Configuration "
                       "file has not been changed, RECO ignored;" +
                       kEndOfResponse);

    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessActiveCount(CQueue* q)
{
    string      active_jobs = NStr::NumericToString(
                                            m_Server->CountActiveJobs());

    x_WriteMessage("OK:" + active_jobs + kEndOfResponse);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessDump(CQueue* q)
{
    if (m_CommandArguments.job_id != 0 &&
        (!m_CommandArguments.group.empty() ||
         !m_CommandArguments.affinity_token.empty() ||
         !m_CommandArguments.job_statuses_string.empty())) {
        if (x_NeedCmdLogging())
            ERR_POST(Warning << "DUMP can accept either a job key or no "
                                "parameters or any combination of a group and "
                                "an affinity and job statuses");
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_WriteMessage("ERR:eInvalidParameter:DUMP can accept either a job "
                       "key or no parameters or any combination of a group and "
                       "an affinity and job statuses" + kEndOfResponse);
        x_PrintCmdRequestStop();
        return;
    }

    if (m_CommandArguments.job_id == 0) {
        // The whole queue dump, may be restricted by a list of statuses
        if (!m_CommandArguments.job_statuses_string.empty()) {
            bool                            reported = false;
            vector<TJobStatus>::iterator    k =
                                        m_CommandArguments.job_statuses.begin();
            while (k != m_CommandArguments.job_statuses.end() ) {
                if (*k == CNetScheduleAPI::eJobNotFound) {
                    if (!reported) {
                        if (x_NeedCmdLogging())
                            ERR_POST(Warning <<
                                     "Unknown job status in the status list. "
                                     "Ignore and continue.");
                        reported = true;
                    }
                    k = m_CommandArguments.job_statuses.erase(k);
                } else
                    ++k;
            }
        }

        vector<string>      warnings;   // Not used here: multiline output
                                        // commands do not have a place for
                                        // warnings
        vector<TJobStatus>  statuses = x_RemoveDuplicateStatuses(
                                    m_CommandArguments.job_statuses, warnings);

        // Check for a special case: statuses were given however all of them
        // are unknown
        if (!m_CommandArguments.job_statuses_string.empty() &&
            statuses.size() == 0)
            x_WriteMessage("OK:END" + kEndOfResponse);
        else
            x_WriteMessage(
                q->PrintAllJobDbStat(m_ClientId,
                                     m_CommandArguments.group,
                                     m_CommandArguments.affinity_token,
                                     statuses,
                                     m_CommandArguments.start_after_job_id,
                                     m_CommandArguments.count,
                                     x_NeedCmdLogging()) +
                "OK:END" + kEndOfResponse);
        x_PrintCmdRequestStop();
        return;
    }


    // Certain job dump
    string      job_info = q->PrintJobDbStat(m_ClientId,
                                             m_CommandArguments.job_id);
    if (job_info.empty()) {
        // Nothing was printed because there is no such a job
        x_SetCmdRequestStatus(eStatus_NotFound);
        x_WriteMessage(kErrNoJobFoundResponse);
    } else
        x_WriteMessage(job_info + "OK:END" + kEndOfResponse);

    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessShutdown(CQueue*)
{
    if (m_CommandArguments.drain) {
        if (m_Server->ShutdownRequested()) {
            x_SetCmdRequestStatus(eStatus_BadRequest);
            x_WriteMessage("ERR:eShuttingDown:The server is in "
                           "shutting down state" + kEndOfResponse);
            x_PrintCmdRequestStop();
            return;
        }
        if (m_Server->IsDrainShutdown()) {
            x_WriteMessage("OK:WARNING:eAlreadyDrainShutdown:The server is "
                           "already in drain shutdown state;" +
                           kEndOfResponse);
            x_PrintCmdRequestStop();
        } else {
            x_WriteMessage(kOKCompleteResponse);
            x_PrintCmdRequestStop();
            m_Server->SetRefuseSubmits(true);
            m_Server->SetDrainShutdown();
        }
        return;
    }

    // Unconditional immediate shutdown.
    x_WriteMessage(kOKCompleteResponse);
    x_PrintCmdRequestStop();
    m_Server->SetRefuseSubmits(true);
    m_Server->SetShutdownFlag();
}


void CNetScheduleHandler::x_ProcessGetConf(CQueue*)
{
    string      configuration;

    if (m_CommandArguments.effective) {
        // The effective config (some parameters could be altered
        // at run-time) has been requested
        configuration = x_GetServerSection() +
                        x_GetLogSection() +
                        x_GetDiagSection() +
                        x_GetBdbSection() +
                        m_Server->GetQueueClassesConfig() +
                        m_Server->GetQueueConfig() +
                        m_Server->GetLinkedSectionConfig() +
                        m_Server->GetServiceToQueueSectionConfig() +
                        kEndOfResponse;
    } else {
        // The original config file (the one used at the startup)
        // has been requested
        CNcbiOstrstream             conf;
        CNcbiOstrstreamToString     converter(conf);

        CNcbiApplication::Instance()->GetConfig().Write(conf);
        configuration = string(converter);
    }
    x_WriteMessage(configuration + "OK:END" + kEndOfResponse);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessVersion(CQueue*)
{
    // Further NS versions should exclude ns_node, ns_session and pid from the
    // VERSION command in favor of the HEALTH command
    static string  reply =
                    "OK:server_version=" NETSCHEDULED_VERSION
                    "&storage_version=" NETSCHEDULED_STORAGE_VERSION
                    "&protocol_version=" NETSCHEDULED_PROTOCOL_VERSION
                    "&build_date=" + NStr::URLEncode(NETSCHEDULED_BUILD_DATE) +
                    "&ns_node=" + m_Server->GetNodeID() +
                    "&ns_session=" + m_Server->GetSessionID() +
                    "&pid=" + NStr::NumericToString(CDiagContext::GetPID()) +
                    kEndOfResponse;
    x_WriteMessage(reply);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessHealth(CQueue*)
{
    double      user_time;
    double      system_time;
    bool        process_time_result = GetCurrentProcessTimes(&user_time,
                                                             &system_time);
    Uint8       physical_memory = GetPhysicalMemorySize();
    size_t      mem_used_total;
    size_t      mem_used_resident;
    size_t      mem_used_shared;
    bool        mem_used_result = GetMemoryUsage(&mem_used_total,
                                                 &mem_used_resident,
                                                 &mem_used_shared);
    int         proc_fd_soft_limit;
    int         proc_fd_hard_limit;
    int         proc_fd_used = GetProcessFDCount(&proc_fd_soft_limit,
                                                 &proc_fd_hard_limit);
    int         proc_thread_count = GetProcessThreadCount();

    #if defined(_DEBUG) && !defined(NDEBUG)
    if (m_CmdContext.NotNull()) {
        SErrorEmulatorParameter  err_emul = m_Server->GetDebugFDCount();

        if (err_emul.IsActive())
            if (err_emul.as_int >= 0)
                proc_fd_used = err_emul.as_int;

        err_emul = m_Server->GetDebugMemCount();
        if (err_emul.IsActive())
            if (err_emul.as_int >= 0)
                mem_used_total = err_emul.as_int;
    }
    #endif

    string      reply =
                    "OK:pid=" +
                    NStr::NumericToString(CDiagContext::GetPID()) +
                    "&ns_node=" +
                    m_Server->GetNodeID() +
                    "&ns_session=" +
                    m_Server->GetSessionID() +
                    "&started=" +
                    NStr::URLEncode(m_Server->GetStartTime().AsString()) +
                    "&cpu_count=" +
                    NStr::NumericToString(GetCpuCount());
    if (process_time_result)
        reply += "&user_time=" + NStr::NumericToString(user_time) +
                 "&system_time=" + NStr::NumericToString(system_time);
    else
        reply += "&user_time=n/a&system_time=n/a";

    if (physical_memory > 0)
        reply += "&physical_memory=" + NStr::NumericToString(physical_memory);
    else
        reply += "&physical_memory=n/a";

    if (mem_used_result)
        reply += "&mem_used_total=" +
                 NStr::NumericToString(mem_used_total) +
                 "&mem_used_resident=" +
                 NStr::NumericToString(mem_used_resident) +
                 "&mem_used_shared=" +
                 NStr::NumericToString(mem_used_shared);
    else
        reply += "&mem_used_total=n/a&mem_used_resident=n/a"
                 "&mem_used_shared=n/a";

    if (proc_fd_soft_limit >= 0)
        reply += "&proc_fd_soft_limit=" +
                 NStr::NumericToString(proc_fd_soft_limit);
    else
        reply += "&proc_fd_soft_limit=n/a";

    if (proc_fd_hard_limit >= 0)
        reply += "&proc_fd_hard_limit=" +
                 NStr::NumericToString(proc_fd_hard_limit);
    else
        reply += "&proc_fd_hard_limit=n/a";

    if (proc_fd_used >= 0)
        reply += "&proc_fd_used=" + NStr::NumericToString(proc_fd_used);
    else
        reply += "&proc_fd_used=n/a";

    if (proc_thread_count >= 1)
        reply += "&proc_thread_count=" +
                 NStr::NumericToString(proc_thread_count);
    else
        reply += "&proc_thread_count=n/a";

    string  alerts = m_Server->GetAlerts();
    if (!alerts.empty())
        reply += "&" + alerts;

    x_WriteMessage(reply + kEndOfResponse);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessAckAlert(CQueue*)
{
    enum EAlertAckResult    result =
                m_Server->AcknowledgeAlert(m_CommandArguments.alert,
                                           m_CommandArguments.user);
    switch (result) {
        case eNotFound:
            x_WriteMessage("OK:WARNING:eAlertNotFound:Alert has not been "
                           "found;" + kEndOfResponse);
            break;
        case eAlreadyAcknowledged:
            x_WriteMessage("OK:WARNING:eAlertAlreadyAcknowledged:Alert has "
                           "already been acknowledged;" + kEndOfResponse);
            break;
        default:
            x_WriteMessage(kOKCompleteResponse);
    }
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessQList(CQueue*)
{
    x_WriteMessage("OK:" + m_Server->GetQueueNames(";") + kEndOfResponse);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessQuitSession(CQueue*)
{
    m_Server->CloseConnection(&GetSocket());
}


void CNetScheduleHandler::x_ProcessCreateDynamicQueue(CQueue*)
{
    // program and submitter restrictions must be checked for the queue class
    m_Server->CreateDynamicQueue(
                        m_ClientId,
                        m_CommandArguments.qname,
                        m_CommandArguments.qclass,
                        m_CommandArguments.description);
    x_WriteMessage(kOKCompleteResponse);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessDeleteDynamicQueue(CQueue*)
{
    // program and submitter restrictions must be checked for the queue to
    // be deleted
    m_Server->DeleteDynamicQueue(m_ClientId, m_CommandArguments.qname);
    x_WriteMessage(kOKCompleteResponse);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessQueueInfo(CQueue*)
{
    bool                cmdv2(m_CommandArguments.cmd == "QINF2");

    if (cmdv2) {
        x_CheckQInf2Parameters();

        string      qname = m_CommandArguments.qname;
        if (!m_CommandArguments.service.empty()) {
            // Service has been provided, need to resolve it to a queue
            qname = m_Server->ResolveService(m_CommandArguments.service);
            if (qname.empty()) {
                x_SetCmdRequestStatus(eStatus_NotFound);
                x_WriteMessage("ERR:eUnknownService:Cannot resolve service " +
                               m_CommandArguments.service + " to a queue" +
                               kEndOfResponse);
                x_PrintCmdRequestStop();
                return;
            }
        }

        SQueueParameters    params = m_Server->QueueInfo(qname);
        CRef<CQueue>        queue_ref;
        CQueue *            queue_ptr;
        size_t              jobs_per_state[g_ValidJobStatusesSize];
        string              jobs_part;
        string              linked_sections_part;
        size_t              total = 0;
        map< string,
             map<string, string> >      linked_sections;
        vector<string>      warnings;

        queue_ref.Reset(m_Server->OpenQueue(qname));
        queue_ptr = queue_ref.GetPointer();
        queue_ptr->GetJobsPerState(m_ClientId, "", "",
                                   jobs_per_state, warnings);
        queue_ptr->GetLinkedSections(linked_sections);

        for (size_t  index(0); index < g_ValidJobStatusesSize; ++index) {
            jobs_part += "&" +
                CNetScheduleAPI::StatusToString(g_ValidJobStatuses[index]) +
                "=" + NStr::NumericToString(jobs_per_state[index]);
            total += jobs_per_state[index];
        }
        jobs_part += "&Total=" + NStr::NumericToString(total);

        for (map< string, map<string, string> >::const_iterator
                k = linked_sections.begin(); k != linked_sections.end(); ++k) {
            string  prefix((k->first).c_str() + strlen("linked_section_"));
            for (map<string, string>::const_iterator j = k->second.begin();
                    j != k->second.end(); ++j) {
                linked_sections_part += "&" + prefix + "." +
                                        NStr::URLEncode(j->first) + "=" +
                                        NStr::URLEncode(j->second);
            }
        }
        string      qname_part;
        if (!m_CommandArguments.service.empty())
            qname_part = "queue_name=" + qname + "&";

        // Include queue classes and use URL encoding
        x_WriteMessage("OK:" + qname_part +
                       params.GetPrintableParameters(true, true) +
                       jobs_part + linked_sections_part + kEndOfResponse);
    } else {
        SQueueParameters    params = m_Server->QueueInfo(
                                                m_CommandArguments.qname);
        x_WriteMessage("OK:" + NStr::NumericToString(params.kind) + "\t" +
                       params.qclass + "\t\"" +
                       NStr::PrintableString(params.description) + "\"" +
                       kEndOfResponse);
    }

    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessSetQueue(CQueue*)
{
    if (m_CommandArguments.qname.empty() ||
        NStr::CompareNocase(m_CommandArguments.qname, "noname") == 0) {
        // Disconnecting from all the queues
        m_QueueRef.Reset(NULL);
        m_QueueName.clear();

        m_ClientId.ResetPassedCheck();

        x_WriteMessage(kOKCompleteResponse);
        x_PrintCmdRequestStop();
        return;
    }

    // Here: connecting to another queue

    CRef<CQueue>    queue_ref;
    CQueue *        queue_ptr = NULL;

    // First, deal with the given queue - try to resolve it
    try {
        queue_ref.Reset(m_Server->OpenQueue(m_CommandArguments.qname));
        queue_ptr = queue_ref.GetPointer();
    }
    catch (...) {
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_WriteMessage("ERR:eUnknownQueue:" + kEndOfResponse);
        x_PrintCmdRequestStop();
        return;
    }

    // Second, update the client with its capabilities for the new queue
    x_UpdateClientPassedChecks(queue_ptr);

    // Note:
    // The  m_ClientId.CheckAccess(...) call will take place when a command
    // for the changed queue is executed.


    {
        // The client has appeared for the queue - touch the client registry
        bool        client_was_found = false;
        bool        session_was_reset = false;
        string      old_session;
        bool        had_wn_pref_affs = false;
        bool        had_reader_pref_affs = false;

        queue_ptr->TouchClientsRegistry(m_ClientId, client_was_found,
                                        session_was_reset, old_session,
                                        had_wn_pref_affs,
                                        had_reader_pref_affs);
        if (client_was_found && session_was_reset) {
            if (x_NeedCmdLogging()) {
                string  wn_val = "true";
                if (!had_wn_pref_affs)
                    wn_val = "had none";
                string  reader_val = "true";
                if (!had_reader_pref_affs)
                    reader_val = "had none";

                GetDiagContext().Extra()
                    .Print("client_node", m_ClientId.GetNode())
                    .Print("client_session", m_ClientId.GetSession())
                    .Print("client_old_session", old_session)
                    .Print("wn_preferred_affinities_reset", wn_val)
                    .Print("reader_preferred_affinities_reset", reader_val);
            }
        }
    }

    // Final step - update the current queue reference

    m_QueueRef.Reset(queue_ref);
    m_QueueName = m_CommandArguments.qname;

    x_WriteMessage(kOKCompleteResponse);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessSetScope(CQueue* q)
{
    size_t        used_slots = q->GetScopeSlotsUsed();
    unsigned int  max_slots = m_Server->GetScopeRegistrySettings().max_records;

    if (used_slots >= max_slots) {
        ERR_POST("All scope slots are in use");
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_WriteMessage("ERR:eInternalError:All scope slots are in use" +
                       kEndOfResponse);
        x_PrintCmdRequestStop();
        return;
    }

    // Here: at the moment there are available scope slots, so let it go.
    m_ClientId.SetScope(m_CommandArguments.scope);
    q->SetClientScope(m_ClientId);
    x_WriteMessage(kOKCompleteResponse);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessGetParam(CQueue* q)
{
    unsigned int                max_input_size;
    unsigned int                max_output_size;
    map< string,
         map<string, string> >  linked_sections;

    q->GetMaxIOSizesAndLinkedSections(max_input_size, max_output_size,
                                      linked_sections);

    if (m_CommandArguments.cmd == "GETP2") {
        string  result("OK:max_input_size=" +
                       NStr::NumericToString(max_input_size) + "&" +
                       "max_output_size=" +
                       NStr::NumericToString(max_output_size));

        for (map< string, map<string, string> >::const_iterator
                k = linked_sections.begin(); k != linked_sections.end(); ++k) {
            string  prefix((k->first).c_str() + strlen("linked_section_"));
            for (map<string, string>::const_iterator j = k->second.begin();
                    j != k->second.end(); ++j) {
                result += "&" + prefix + "::" +
                          NStr::URLEncode(j->first) + "=" +
                          NStr::URLEncode(j->second);
            }
        }
        x_WriteMessage(result + kEndOfResponse);
    } else {
        x_WriteMessage("OK:max_input_size=" +
                       NStr::NumericToString(max_input_size) + ";"
                       "max_output_size=" +
                       NStr::NumericToString(max_output_size) + ";" +
                       NETSCHEDULED_FEATURES + kEndOfResponse);
    }
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessGetConfiguration(CQueue* q)
{
    CQueue::TParameterList      parameters = q->GetParameters();
    string                      configuration;

    ITERATE(CQueue::TParameterList, it, parameters) {
        configuration += "OK:" + it->first + '=' + it->second + kEndOfResponse;
    }
    x_WriteMessage(configuration + "OK:END" + kEndOfResponse);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessReading(CQueue* q)
{
    bool    cmdv2(m_CommandArguments.cmd == "READ2");

    x_CheckNonAnonymousClient("use " + m_CommandArguments.cmd + " command");
    x_CheckPortAndTimeout();

    if (cmdv2)
        x_CheckReadParameters();
    else {
        // Affinity flags are for the second version of the command
        m_CommandArguments.reader_affinity = false;
        m_CommandArguments.exclusive_new_aff = false;

        // This flag mimics the GET command behavior
        m_CommandArguments.any_affinity =
                                m_CommandArguments.affinity_token.empty();
    }

    CJob            job;
    bool            no_more_jobs = true;
    string          added_pref_aff;

    list<string>    aff_list;
    NStr::Split(m_CommandArguments.affinity_token,
                "\t,", aff_list, NStr::fSplit_NoMergeDelims);
    list<string>    group_list;
    NStr::Split(m_CommandArguments.group,
                "\t,", group_list, NStr::fSplit_NoMergeDelims);

    x_ClearRollbackAction();
    if (q->GetJobForReadingOrWait(m_ClientId,
                                  m_CommandArguments.port,
                                  m_CommandArguments.timeout,
                                  &aff_list,
                                  m_CommandArguments.reader_affinity,
                                  m_CommandArguments.any_affinity,
                                  m_CommandArguments.exclusive_new_aff,
                                  m_CommandArguments.prioritized_aff,
                                  &group_list,
                                  m_CommandArguments.affinity_may_change,
                                  m_CommandArguments.group_may_change,
                                  &job,
                                  &no_more_jobs, m_RollbackAction,
                                  added_pref_aff) == false) {
        // Preferred affinities were reset for the client, so no job
        // and bad request
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_WriteMessage("ERR:ePrefAffExpired:" + kEndOfResponse);
    } else {
        unsigned int    job_id = job.GetId();
        string          job_key;

        if (job_id) {
            job_key = q->MakeJobKey(job_id);
            x_WriteMessage("OK:job_key=" + job_key +
                           "&client_ip=" + NStr::URLEncode(job.GetClientIP()) +
                           "&client_sid=" + NStr::URLEncode(job.GetClientSID()) +
                           "&ncbi_phid=" + NStr::URLEncode(job.GetNCBIPHID()) +
                           "&auth_token=" + job.GetAuthToken() +
                           "&status=" +
                           CNetScheduleAPI::StatusToString(
                                                job.GetStatusBeforeReading()) +
                           "&affinity=" +
                           NStr::URLEncode(q->GetAffinityTokenByID(
                                                        job.GetAffinityId())) +
                           kEndOfResponse);
            x_ClearRollbackAction();
            x_LogCommandWithJob(job);

            if (!added_pref_aff.empty()) {
                if (x_NeedCmdLogging()) {
                    if (m_ClientIdentificationPrinted)
                        GetDiagContext().Extra()
                            .Print("added_preferred_affinity", added_pref_aff);
                    else
                        GetDiagContext().Extra()
                            .Print("client_node", m_ClientId.GetNode())
                            .Print("client_session", m_ClientId.GetSession())
                            .Print("added_preferred_affinity", added_pref_aff);
                }
            }
        }
        else
            x_WriteMessage("OK:no_more_jobs=" +
                           NStr::BoolToString(no_more_jobs) +
                           kEndOfResponse);

        if (x_NeedCmdLogging()) {
            if (job_id)
                GetDiagContext().Extra().Print("job_key", job_key);
            else
                GetDiagContext().Extra().Print("job_key", "None")
                                        .Print("no_more_jobs", no_more_jobs);
        }
    }

    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessConfirm(CQueue* q)
{
    x_CheckNonAnonymousClient("use CFRM command");
    x_CheckAuthorizationToken();

    CJob            job;
    TJobStatus      old_status = q->ConfirmReadingJob(
                                            m_ClientId,
                                            m_CommandArguments.job_id,
                                            m_CommandArguments.job_key,
                                            job,
                                            m_CommandArguments.auth_token);
    x_FinalizeReadCommand("CFRM", old_status, job);
}


void CNetScheduleHandler::x_ProcessReadFailed(CQueue* q)
{
    x_CheckNonAnonymousClient("use FRED command");
    x_CheckAuthorizationToken();

    CJob            job;
    TJobStatus      old_status = q->FailReadingJob(
                                            m_ClientId,
                                            m_CommandArguments.job_id,
                                            m_CommandArguments.job_key,
                                            job,
                                            m_CommandArguments.auth_token,
                                            m_CommandArguments.err_msg,
                                            m_CommandArguments.no_retries);
    x_FinalizeReadCommand("FRED", old_status, job);
}


void CNetScheduleHandler::x_ProcessReadRollback(CQueue* q)
{
    x_CheckNonAnonymousClient("use RDRB command");
    x_CheckAuthorizationToken();

    CJob            job;
    TJobStatus      old_status = q->ReturnReadingJob(
                                            m_ClientId,
                                            m_CommandArguments.job_id,
                                            m_CommandArguments.job_key,
                                            job,
                                            m_CommandArguments.auth_token,
                                            false,
                                            m_CommandArguments.blacklist,
                                            CNetScheduleAPI::eJobNotFound);
    x_FinalizeReadCommand("RDRB", old_status, job);
}


void CNetScheduleHandler::x_ProcessReread(CQueue* q)
{
    x_CheckNonAnonymousClient("use REREAD command");

    CJob            job;
    bool            no_op = false;
    TJobStatus      old_status = q->RereadJob(m_ClientId,
                                              m_CommandArguments.job_id,
                                              m_CommandArguments.job_key,
                                              job,
                                              no_op);

    if (old_status == CNetScheduleAPI::eJobNotFound) {
        ERR_POST(Warning << "REREAD for unknown job "
                         << m_CommandArguments.job_key);
        x_SetCmdRequestStatus(eStatus_NotFound);
        x_WriteMessage(kErrNoJobFoundResponse);
        x_PrintCmdRequestStop();
        return;
    }

    if (old_status == CNetScheduleAPI::ePending ||
        old_status == CNetScheduleAPI::eRunning ||
        old_status == CNetScheduleAPI::eReading) {
        ERR_POST(Warning << "Cannot reread job "
                     << m_CommandArguments.job_key
                     << "; job is in "
                     << CNetScheduleAPI::StatusToString(old_status)
                     << " state");
        x_SetCmdRequestStatus(eStatus_InvalidJobStatus);
        x_WriteMessage("ERR:eInvalidJobStatus:Cannot reread job; job is in " +
                       CNetScheduleAPI::StatusToString(old_status) + " state" +
                       kEndOfResponse);
    } else if (no_op) {
        ERR_POST(Warning << "Cannot reread job "
                    << m_CommandArguments.job_key
                    << "; job has not been read yet");
        x_WriteMessage("OK:WARNING:eJobNotRead:The job has not been read yet;" +
                       kEndOfResponse);
    } else {
        x_WriteMessage(kOKCompleteResponse);
    }

    x_LogCommandWithJob(job);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_FinalizeReadCommand(const string &  command,
                                                TJobStatus      old_status,
                                                const CJob &    job)
{
    if (old_status == CNetScheduleAPI::eJobNotFound) {
        ERR_POST(Warning << command << " for unknown job "
                         << m_CommandArguments.job_key);
        x_SetCmdRequestStatus(eStatus_NotFound);
        x_WriteMessage(kErrNoJobFoundResponse);
        x_PrintCmdRequestStop();
        return;
    }

    if (old_status != CNetScheduleAPI::eReading) {
        string      operation = "unknown";

        if (command == "CFRM")      operation = "confirm";
        else if (command == "FRED") operation = "fail";
        else if (command == "RDRB") operation = "rollback";

        ERR_POST(Warning << "Cannot " << operation
                         << " read job; job is in "
                         << CNetScheduleAPI::StatusToString(old_status)
                         << " state");
        x_SetCmdRequestStatus(eStatus_InvalidJobStatus);
        x_WriteMessage("ERR:eInvalidJobStatus:Cannot " +
                       operation + " job; job is in " +
                       CNetScheduleAPI::StatusToString(old_status) + " state" +
                       kEndOfResponse);
    } else
        x_WriteMessage(kOKCompleteResponse);

    x_LogCommandWithJob(job);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessSetClientData(CQueue* q)
{
    unsigned int    limit = m_Server->GetMaxClientData();
    unsigned int    data_size = m_CommandArguments.client_data.size();

    if (data_size > limit) {
        ERR_POST(Warning << "Client data is too long. It must be <= "
                         << limit
                         << " bytes. Received "
                         << data_size
                         << " bytes.");
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_WriteMessage("ERR:eInvalidParameter:Client data is too long. "
                       "It must be <= " + NStr::NumericToString(limit) +
                       " bytes. Received " + NStr::NumericToString(data_size) +
                       " bytes." + kEndOfResponse);
    } else {
        int     current_data_version = q->SetClientData(m_ClientId,
                                        m_CommandArguments.client_data,
                                        m_CommandArguments.client_data_version);
        x_WriteMessage("OK:version=" +
                       NStr::NumericToString(current_data_version) +
                       kEndOfResponse);
    }
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessClearWorkerNode(CQueue* q)
{
    bool        client_found = false;
    bool        had_wn_pref_affs = false;
    bool        had_reader_pref_affs = false;
    string      old_session;

    q->ClearWorkerNode(m_ClientId, client_found,
                       old_session, had_wn_pref_affs, had_reader_pref_affs);

    if (client_found) {
        if (x_NeedCmdLogging()) {
            string      wn_val = "true";
            if (!had_wn_pref_affs)
                wn_val = "had none";
            string      reader_val = "true";
            if (!had_reader_pref_affs)
                reader_val = "had none";

            GetDiagContext().Extra()
                .Print("client_node", m_ClientId.GetNode())
                .Print("client_session", m_ClientId.GetSession())
                .Print("client_old_session", old_session)
                .Print("wn_preferred_affinities_reset", wn_val)
                .Print("reader_preferred_affinities_reset", reader_val);
        }
    }

    x_WriteMessage(kOKCompleteResponse);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessCancelQueue(CQueue* q)
{
    unsigned int  count = q->CancelAllJobs(m_ClientId, x_NeedCmdLogging());
    x_WriteMessage("OK:" + NStr::NumericToString(count) + kEndOfResponse);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessRefuseSubmits(CQueue* q)
{
    if (m_CommandArguments.mode == false &&
            (m_Server->IsDrainShutdown() || m_Server->ShutdownRequested())) {
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_WriteMessage("ERR:eShuttingDown:"
                       "Server is in drained shutting down state" +
                       kEndOfResponse);
        x_PrintCmdRequestStop();
        return;
    }

    if (q == NULL) {
        // This is a whole server scope request
        m_Server->SetRefuseSubmits(m_CommandArguments.mode);
        x_WriteMessage(kOKCompleteResponse);
        x_PrintCmdRequestStop();
        return;
    }

    // This is a queue scope request.
    q->MarkClientAsAdmin(m_ClientId);
    q->SetRefuseSubmits(m_CommandArguments.mode);

    if (m_CommandArguments.mode == false &&
            m_Server->GetRefuseSubmits() == true)
        x_WriteMessage("OK:WARNING:eSubmitsDisabledForServer:Submits are "
                       "disabled on the server level;" + kEndOfResponse);
    else
        x_WriteMessage(kOKCompleteResponse);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessPause(CQueue* q)
{
    CQueue::TPauseStatus    current = q->GetPauseStatus();
    CQueue::TPauseStatus    new_value;

    if (m_CommandArguments.pullback)
        new_value = CQueue::ePauseWithPullback;
    else
        new_value = CQueue::ePauseWithoutPullback;

    q->SetPauseStatus(m_ClientId, new_value);
    if (current == CQueue::eNoPause)
        x_WriteMessage(kOKCompleteResponse);
    else {
        string  reply = "OK:WARNING:eQueueAlreadyPaused:The queue has "
                        "already been paused (previous pullback value is ";
        if (current == CQueue::ePauseWithPullback)  reply += "true";
        else                                        reply += "false";
        reply += ", new pullback value is ";
        if (m_CommandArguments.pullback)            reply += "true";
        else                                        reply += "false";
        x_WriteMessage(reply + ");" + kEndOfResponse);
    }
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessResume(CQueue* q)
{
    CQueue::TPauseStatus    current = q->GetPauseStatus();

    q->SetPauseStatus(m_ClientId, CQueue::eNoPause);
    if (current == CQueue::eNoPause)
        x_WriteMessage("OK:WARNING:eQueueNotPaused:"
                       "The queue is not paused;" + kEndOfResponse);
    else
        x_WriteMessage(kOKCompleteResponse);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_CmdNotImplemented(CQueue *)
{
    x_SetCmdRequestStatus(eStatus_NotImplemented);
    x_WriteMessage("ERR:eObsoleteCommand:" + kEndOfResponse);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_CmdObsolete(CQueue*)
{
    x_SetCmdRequestStatus(eStatus_NotImplemented);
    x_WriteMessage("OK:WARNING:eCommandObsolete:"
                   "Command is obsolete and will be ignored;" +
                   kEndOfResponse);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_CheckNonAnonymousClient(const string &  message)
{
    if (!m_ClientId.IsComplete())
        NCBI_THROW(CNetScheduleException, eInvalidClient,
                   "Anonymous client (no client_node and client_session"
                   " at handshake) cannot " + message);
}


void CNetScheduleHandler::x_CheckPortAndTimeout(void)
{
    if ((m_CommandArguments.port != 0 &&
         m_CommandArguments.timeout == 0) ||
        (m_CommandArguments.port == 0 &&
         m_CommandArguments.timeout != 0))
        NCBI_THROW(CNetScheduleException, eInvalidParameter,
                   "Either both or neither of the port and "
                   "timeout parameters must be 0");
}


void CNetScheduleHandler::x_CheckAuthorizationToken(void)
{
    if (m_CommandArguments.auth_token.empty())
        NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                   "Invalid authorization token. It cannot be empty.");
}


void CNetScheduleHandler::x_CheckGetParameters(void)
{
    // Checks that the given GETx/JXCG parameters make sense
    if (m_CommandArguments.wnode_affinity == false &&
        m_CommandArguments.any_affinity == false &&
        m_CommandArguments.affinity_token.empty() &&
        m_CommandArguments.exclusive_new_aff == false) {
        ERR_POST(Warning << "The job request without explicit affinities, "
                            "without preferred affinities and "
                            "with any_aff flag set to false "
                            "will never match any job.");
    }
    if (m_CommandArguments.exclusive_new_aff == true &&
        m_CommandArguments.any_affinity == true)
        NCBI_THROW(CNetScheduleException, eInvalidParameter,
                   "It is forbidden to have both any_affinity and "
                   "exclusive_new_aff GET2 flags set to 1.");
    if (m_CommandArguments.prioritized_aff == true &&
        m_CommandArguments.wnode_affinity == true)
        NCBI_THROW(CNetScheduleException, eInvalidParameter,
                   "It is forbidden to have both prioritized_aff and "
                   "wnode_aff GET2 flags set to 1.");
    if (m_CommandArguments.prioritized_aff == true &&
        m_CommandArguments.exclusive_new_aff == true)
        NCBI_THROW(CNetScheduleException, eInvalidParameter,
                   "It is forbidden to have both prioritized_aff and "
                   "exclusive_new_aff GET2 flags set to 1.");
    if (m_CommandArguments.prioritized_aff == true &&
        m_CommandArguments.affinity_token.empty())
        NCBI_THROW(CNetScheduleException, eInvalidParameter,
                   "If the prioritized_aff GET2 flag set to 1 then "
                   "a non empty list of explicit affinities must be provided.");
}


void CNetScheduleHandler::x_CheckReadParameters(void)
{
    // Checks that the given READ parameters make sense
    if (m_CommandArguments.reader_affinity == false &&
        m_CommandArguments.any_affinity == false &&
        m_CommandArguments.affinity_token.empty() &&
        m_CommandArguments.exclusive_new_aff == false) {
        ERR_POST(Warning << "The job read request without explicit affinities, "
                            "without preferred affinities and "
                            "with any_aff flag set to false "
                            "will never match any job.");
    }
    if (m_CommandArguments.exclusive_new_aff == true &&
        m_CommandArguments.any_affinity == true)
        NCBI_THROW(CNetScheduleException, eInvalidParameter,
                   "It is forbidden to have both any_aff and "
                   "exclusive_new_aff READ2 flags set to 1.");
    if (m_CommandArguments.prioritized_aff == true &&
        m_CommandArguments.reader_affinity == true)
        NCBI_THROW(CNetScheduleException, eInvalidParameter,
                   "It is forbidden to have both prioritized_aff and "
                   "reader_aff READ2 flags set to 1.");
    if (m_CommandArguments.prioritized_aff == true &&
        m_CommandArguments.exclusive_new_aff == true)
        NCBI_THROW(CNetScheduleException, eInvalidParameter,
                   "It is forbidden to have both prioritized_aff and "
                   "exclusive_new_aff READ2 flags set to 1.");
    if (m_CommandArguments.prioritized_aff == true &&
        m_CommandArguments.affinity_token.empty())
        NCBI_THROW(CNetScheduleException, eInvalidParameter,
                   "If the prioritized_aff READ2 flag set to 1 then "
                   "a non empty list of explicit affinities must be provided.");
}


void CNetScheduleHandler::x_CheckQInf2Parameters(void)
{
    // One of the arguments must be provided: qname or service
    if (m_CommandArguments.qname.empty() &&
        m_CommandArguments.service.empty())
        NCBI_THROW(CNetScheduleException, eInvalidParameter,
                   "QINF2 command expects a queue name or a service name. "
                   "Nothing has been provided.");

    if (!m_CommandArguments.qname.empty() &&
        !m_CommandArguments.service.empty())
        NCBI_THROW(CNetScheduleException, eInvalidParameter,
                   "QINF2 command expects only one value: queue name or "
                   "a service name. Both have been provided.");
}


void CNetScheduleHandler::x_PrintCmdRequestStart(const SParsedCmd &  cmd)
{
    if (m_CmdContext.NotNull()) {
        // Need to log SID/IP/PHID only if the command does not have
        // a job key in the parameters
        bool    is_worker_node_command = x_WorkerNodeCommand();

        CDiagContext::SetRequestContext(m_CmdContext);

        // A client IP and a session ID must be set as early as possible
        if (!is_worker_node_command) {
            ITERATE(TNSProtoParams, it, cmd.params) {
                if (it->first == "ip")
                    CDiagContext::GetRequestContext().SetClientIP(it->second);
                else if (it->first == "sid")
                    CDiagContext::GetRequestContext().SetSessionID(it->second);
            }
        }

        CDiagContext_Extra    ctxt_extra =
                GetDiagContext().PrintRequestStart()
                            .Print("_type", "cmd")
                            .Print("_queue", m_QueueName)
                            .Print("cmd", cmd.command->cmd)
                            .Print("peer", GetSocket().GetPeerAddress(eSAF_IP))
                            .Print("conn", x_GetConnRef());

        ITERATE(TNSProtoParams, it, cmd.params) {
            // IP is set before the print request start
            if (it->first == "ip")
                continue;
            // SID is set before the print request start
            if (it->first == "sid")
                continue;
            // Skip ncbi_phid because it is printed by request start anyway
            if (it->first == "ncbi_phid")
                continue;

            ctxt_extra.Print(it->first, it->second);
        }
        ctxt_extra.Flush();

        // Workaround:
        // When extra of the GetDiagContext().PrintRequestStart() is destroyed
        // or flushed it also resets the status to 0 so I need to set it here
        // to 200 though it was previously set to 200 when the request context
        // is created.
        m_CmdContext->SetRequestStatus(eStatus_OK);
    }
}


void CNetScheduleHandler::x_PrintCmdRequestStart(CTempString  msg)
{
    if (m_CmdContext.NotNull()) {
        CDiagContext::SetRequestContext(m_CmdContext);
        GetDiagContext().PrintRequestStart()
                .Print("_type", "cmd")
                .Print("_queue", m_QueueName)
                .Print("info", msg)
                .Print("peer",  GetSocket().GetPeerAddress(eSAF_IP))
                .Print("conn", x_GetConnRef())
                .Flush();

        // Workaround:
        // When extra of the GetDiagContext().PrintRequestStart() is destroyed
        // or flushed it also resets the status to 0 so I need to set it here
        // to 200 though it was previously set to 200 when the request context
        // is created.
        m_CmdContext->SetRequestStatus(eStatus_OK);
    }
}


void CNetScheduleHandler::x_PrintCmdRequestStop(void)
{
    if (m_CmdContext.NotNull()) {
        CDiagContext::SetRequestContext(m_CmdContext);
        GetDiagContext().PrintRequestStop();
        m_CmdContext.Reset();
    }
}

// The function forms a responce for various 'get job' commands and prints
// extra to the log if required
void
CNetScheduleHandler::x_PrintGetJobResponse(const CQueue *  q,
                                           const CJob &    job,
                                           bool            cmdv2)
{
    if (!job.GetId()) {
        // No suitable job found
        if (x_NeedCmdLogging())
            GetDiagContext().Extra().Print("job_key", "None");
        x_WriteMessage(kOKCompleteResponse);
        return;
    }

    string      job_key = q->MakeJobKey(job.GetId());
    if (x_NeedCmdLogging()) {
        // The only piece required for logging is the job key
        GetDiagContext().Extra().Print("job_key", job_key);
    }

    if (cmdv2) {
        string      submitter_notif_info;
        if (job.GetSubmNotifPort() != 0) {
            string  host = CSocketAPI::ntoa(job.GetSubmAddr());
            if (host == "127.0.0.1") {
                unsigned int    my_addr = CSocketAPI::GetLocalHostAddress();
                host = CSocketAPI::ntoa(my_addr);
                if (host == "127.0.0.1") {
                    ERR_POST(Warning <<
                             "Could not detect the self host address "
                             "to provide it to a worker node");
                }
            }
            submitter_notif_info =
                "&sumbitter_notif_host=" + NStr::URLEncode(host) +
                "&sumbitter_notif_port=" +
                    NStr::NumericToString(job.GetSubmNotifPort());
        }
        x_WriteMessage(
                       "OK:job_key=" + job_key +
                       "&input=" + NStr::URLEncode(job.GetInput()) +
                       "&affinity=" +
                       NStr::URLEncode(q->GetAffinityTokenByID(
                                                        job.GetAffinityId())) +
                       "&client_ip=" + NStr::URLEncode(job.GetClientIP()) +
                       "&client_sid=" + NStr::URLEncode(job.GetClientSID()) +
                       "&ncbi_phid=" + NStr::URLEncode(job.GetNCBIPHID()) +
                       "&mask=" + NStr::NumericToString(job.GetMask()) +
                       "&auth_token=" + job.GetAuthToken() +
                       submitter_notif_info +
                       kEndOfResponse);
    } else {
        x_WriteMessage(
                       "OK:" + job_key +
                       " \"" + NStr::PrintableString(job.GetInput()) + "\""
                       " \"" + NStr::PrintableString(
                                            q->GetAffinityTokenByID(
                                                job.GetAffinityId())) + "\""
                       " \"" + NStr::PrintableString(job.GetClientIP()) + " " +
                               NStr::PrintableString(job.GetClientSID()) + "\""
                       " " + NStr::NumericToString(job.GetMask()) +
                       kEndOfResponse);
    }
}


bool CNetScheduleHandler::x_CanBeWithoutQueue(FProcessor  processor) const
{
    return // STATUS/STATUS2
           processor == &CNetScheduleHandler::x_ProcessStatus ||
           // DUMP
           processor == &CNetScheduleHandler::x_ProcessDump ||
           // MGET
           processor == &CNetScheduleHandler::x_ProcessGetMessage ||
           // SST/SST2
           processor == &CNetScheduleHandler::x_ProcessFastStatusS ||
           // LISTEN
           processor == &CNetScheduleHandler::x_ProcessListenJob ||
           // CANCEL
           processor == &CNetScheduleHandler::x_ProcessCancel ||
           // MPUT
           processor == &CNetScheduleHandler::x_ProcessPutMessage ||
           // WST/WST2
           processor == &CNetScheduleHandler::x_ProcessFastStatusW ||
           // PUT/PUT2
           processor == &CNetScheduleHandler::x_ProcessPut ||
           // RETURN/RETURN2
           processor == &CNetScheduleHandler::x_ProcessReturn ||
           // RESCHEDULE
           processor == &CNetScheduleHandler::x_ProcessReschedule ||
           // FPUT/FPUT2
           processor == &CNetScheduleHandler::x_ProcessPutFailure ||
           // JXCG
           processor == &CNetScheduleHandler::x_ProcessJobExchange ||
           // JDEX
           processor == &CNetScheduleHandler::x_ProcessJobDelayExpiration ||
           // JDREX
           processor == &CNetScheduleHandler::x_ProcessJobDelayReadExpiration ||
           // CFRM
           processor == &CNetScheduleHandler::x_ProcessConfirm ||
           // FRED
           processor == &CNetScheduleHandler::x_ProcessReadFailed ||
           // RDRB
           processor == &CNetScheduleHandler::x_ProcessReadRollback;
}


bool
CNetScheduleHandler::x_NeedToGeneratePHIDAndSID(FProcessor  processor) const
{
    // The only commands which need the phid and sid generated are
    // SUBMIT and BSUB
    return processor == &CNetScheduleHandler::x_ProcessSubmit ||    // SUBMIT
           processor == &CNetScheduleHandler::x_ProcessSubmitBatch; // BSUB
}


bool
CNetScheduleHandler::x_WorkerNodeCommand(void) const
{
    // If a command has a reference to a job some way, it may come
    // from a worker node or from somewhere else.
    // Denis suggested the following way of distinguishing the source:
    // if ncbi_phid and/or sid are both empty strings then it is a
    // worker node command.
    // Depending on this decision logging will be done in a different way.
    return m_CommandArguments.ncbi_phid.empty() &&
           m_CommandArguments.sid.empty();
}


void
CNetScheduleHandler::x_LogCommandWithJob(const CJob &  job) const
{
    // If a command refers to a job some way then it should be logged
    // in a different way depending on the command source.
    if (x_NeedCmdLogging()) {
        if (x_WorkerNodeCommand()) {
            CDiagContext::GetRequestContext().SetClientIP(job.GetClientIP());
            CDiagContext::GetRequestContext().SetSessionID(job.GetClientSID());
            GetDiagContext().Extra().Print("ncbi_phid", job.GetNCBIPHID());
        } else
            GetDiagContext().Extra().Print("job_phid", job.GetNCBIPHID());
    }
}


void
CNetScheduleHandler::x_LogCommandWithJob(const string &  client_ip,
                                         const string &  client_sid,
                                         const string &  phid) const
{
    // If a command refers to a job some way then it should be logged
    // in a different way depending on the command source.
    if (x_NeedCmdLogging()) {
        if (x_WorkerNodeCommand()) {
            CDiagContext::GetRequestContext().SetClientIP(client_ip);
            CDiagContext::GetRequestContext().SetSessionID(client_sid);
            GetDiagContext().Extra().Print("ncbi_phid", phid);
        } else
            GetDiagContext().Extra().Print("job_phid", phid);
    }
}


void CNetScheduleHandler::x_ClearRollbackAction(void)
{
    if (m_RollbackAction != NULL) {
        delete m_RollbackAction;
        m_RollbackAction = NULL;
    }
}


void CNetScheduleHandler::x_ExecuteRollbackAction(CQueue * q)
{
    if (m_RollbackAction == NULL)
        return;
    m_RollbackAction->Rollback(q);
    x_ClearRollbackAction();
}


void CNetScheduleHandler::x_CreateConnContext(void)
{
    CSocket &       socket = GetSocket();

    m_ConnContext.Reset(new CRequestContext());
    m_ConnContext->SetRequestID();
    m_ConnContext->SetClientIP(socket.GetPeerAddress(eSAF_IP));

    // Set the connection request as the current one and print request start
    CDiagContext::SetRequestContext(m_ConnContext);
    GetDiagContext().PrintRequestStart()
                    .Print("_type", "conn")
                    .Print("conn", x_GetConnRef());
    m_ConnContext->SetRequestStatus(eStatus_OK);
}


static const unsigned int   kMaxParserErrMsgLength = 128;

// Writes into socket, logs the message and closes the connection
void
CNetScheduleHandler::x_OnCmdParserError(bool            need_request_start,
                                        const string &  msg,
                                        const string &  suffix)
{
    // Truncating is done to prevent output of an arbitrary long garbage

    if (need_request_start) {
        CDiagContext::GetRequestContext().SetClientIP(
                                        GetSocket().GetPeerAddress(eSAF_IP));
        x_PrintCmdRequestStart("Invalid command");
    }

    if (msg.size() < kMaxParserErrMsgLength * 2)
        ERR_POST("Error parsing command: " << msg + suffix);
    else
        ERR_POST("Error parsing command: " <<
                 msg.substr(0, kMaxParserErrMsgLength * 2) <<
                 " (TRUNCATED)" + suffix);
    x_SetCmdRequestStatus(eStatus_BadRequest);

    x_SetConnRequestStatus(eStatus_BadRequest);
    string client_error = "ERR:eProtocolSyntaxError:";
    if (msg.size() < kMaxParserErrMsgLength)
        client_error += msg + suffix;
    else
        client_error += msg.substr(0, kMaxParserErrMsgLength) +
                        " (TRUNCATED)" + suffix;

    if (x_WriteMessage(client_error + kEndOfResponse) == eIO_Success)
        m_Server->CloseConnection(&GetSocket());
}


void
CNetScheduleHandler::x_StatisticsNew(CQueue *                q,
                                     const string &          what,
                                     const CNSPreciseTime &  curr)
{
    bool            verbose = (m_CommandArguments.comment == "VERBOSE");
    string          info;
    vector<string>  warnings;

    if (what == "CLIENTS") {
        info = q->PrintClientsList(verbose);
    }
    else if (what == "NOTIFICATIONS") {
        info = q->PrintNotificationsList(verbose);
    }
    else if (what == "AFFINITIES") {
        info = q->PrintAffinitiesList(m_ClientId, verbose);
    }
    else if (what == "GROUPS") {
        info = q->PrintGroupsList(m_ClientId, verbose);
    }
    else if (what == "SCOPES") {
        info = q->PrintScopesList(verbose);
    }
    else if (what == "JOBS") {
        info = q->PrintJobsStat(m_ClientId,
                                m_CommandArguments.group,
                                m_CommandArguments.affinity_token,
                                warnings);
    }
    else if (what == "WNODE") {
        warnings.push_back("eCommandObsolete:Command is obsolete, "
                           "use STAT CLIENTS instead");
    }

    string  msg;
    for (vector<string>::const_iterator  k = warnings.begin();
         k != warnings.end(); ++k) {
        msg += "WARNING:" + *k + ";";
    }

    x_MakeSureSingleEOR(info);
    x_WriteMessage(info + "OK:" + msg + "END" + kEndOfResponse);
    x_PrintCmdRequestStop();
}


string CNetScheduleHandler::x_GetServerSection(void) const
{
    SServer_Parameters      server_params;
    m_Server->GetParameters(&server_params);

    return "[server]\n"
           "max_connections=\"" +
                NStr::NumericToString(server_params.max_connections) + "\"\n"
           "max_threads=\"" +
                NStr::NumericToString(server_params.max_threads) + "\"\n"
           "init_threads=\"" +
                NStr::NumericToString(server_params.init_threads) + "\"\n"
           "port=\"" +
                NStr::NumericToString(m_Server->GetPort()) + "\"\n"
           "use_hostname=\"" +
                NStr::BoolToString(m_Server->GetUseHostname()) + "\"\n"
           "network_timeout=\"" +
                NStr::NumericToString(m_Server->GetInactivityTimeout()) + "\"\n"
           "log=\"" +
                NStr::BoolToString(m_Server->IsLog()) + "\"\n"
           "log_batch_each_job=\"" +
                NStr::BoolToString(m_Server->IsLogBatchEachJob()) + "\"\n"
           "log_notification_thread=\"" +
                NStr::BoolToString(m_Server->IsLogNotificationThread()) + "\"\n"
           "log_cleaning_thread=\"" +
                NStr::BoolToString(m_Server->IsLogCleaningThread()) + "\"\n"
           "log_execution_watcher_thread=\"" +
                NStr::BoolToString(
                        m_Server->IsLogExecutionWatcherThread()) + "\"\n"
           "log_statistics_thread=\"" +
                NStr::BoolToString(m_Server->IsLogStatisticsThread()) + "\"\n"
           "del_batch_size=\"" +
                NStr::NumericToString(m_Server->GetDeleteBatchSize()) + "\"\n"
           "markdel_batch_size=\"" +
                NStr::NumericToString(m_Server->GetMarkdelBatchSize()) + "\"\n"
           "scan_batch_size=\"" +
                NStr::NumericToString(m_Server->GetScanBatchSize()) + "\"\n"
           "purge_timeout=\"" +
                NStr::NumericToString(m_Server->GetPurgeTimeout()) + "\"\n"
           "stat_interval=\"" +
                NStr::NumericToString(m_Server->GetStatInterval()) + "\"\n"
           "job_counters_interval=\"" +
                NStr::NumericToString(m_Server->GetJobCountersInterval()) +
                "\"\n"
           "max_client_data=\"" +
                NStr::NumericToString(m_Server->GetMaxClientData()) + "\"\n"
           "admin_host=\"" +
                m_Server->GetAdminHosts().GetAsFromConfig() + "\"\n"
           "admin_client_name=\"" +
                m_Server->GetAdminClientNames() + "\"\n" +
           "state_transition_perf_log_queues=\"" +
                m_Server->GetStateTransitionPerfLogQueues() + "\"\n" +
           "state_transition_perf_log_classes=\"" +
                m_Server->GetStateTransitionPerfLogClasses() + "\"\n" +
           m_Server->GetAffRegistrySettings().Serialize("affinity", "", "\n") +
           m_Server->GetGroupRegistrySettings().Serialize("group", "", "\n") +
           m_Server->GetScopeRegistrySettings().Serialize("scope", "", "\n") +
           "reserve_dump_space=\"" +
                NStr::NumericToString(
                        m_Server->GetReserveDumpSpace()) + "\"\n";
}


string
CNetScheduleHandler::x_GetStoredSectionValues(
                                    const string &  section_name,
                                    const map<string, string> & values) const
{
    if (values.empty())
        return "";

    string  ret = "[" + section_name + "]\n";
    for (map<string, string>::const_iterator k = values.begin();
         k != values.end(); ++k)
        ret += k->first + "=\"" + k->second + "\"\n";
    return ret;
}


string CNetScheduleHandler::x_GetLogSection(void) const
{
    CNetScheduleDApp *      app = dynamic_cast<CNetScheduleDApp*>
                                            (CNcbiApplication::Instance());
    return x_GetStoredSectionValues("log", app->GetOrigLogSection());
}


string CNetScheduleHandler::x_GetDiagSection(void) const
{
    CNetScheduleDApp *      app = dynamic_cast<CNetScheduleDApp*>
                                            (CNcbiApplication::Instance());
    return x_GetStoredSectionValues("diag", app->GetOrigDiagSection());
}


string CNetScheduleHandler::x_GetBdbSection(void) const
{
    CNetScheduleDApp *      app = dynamic_cast<CNetScheduleDApp*>
                                            (CNcbiApplication::Instance());
    return x_GetStoredSectionValues("bdb", app->GetOrigBDBSection());
}


vector<TJobStatus>
CNetScheduleHandler::x_RemoveDuplicateStatuses(const vector<TJobStatus> &  src,
                                               vector<string> &  warnings) const
{
    vector<TJobStatus>      no_duplicates;

    for (vector<TJobStatus>::const_iterator  k = src.begin();
         k != src.end(); ++k) {
        if (find(no_duplicates.begin(), no_duplicates.end(), *k) !=
                                                    no_duplicates.end()) {
            warnings.push_back("eStatusDuplicates:job status " +
                               CNetScheduleAPI::StatusToString(*k) +
                               " provided more than once");
            if (x_NeedCmdLogging())
                ERR_POST(Warning <<
                         "Job status " + CNetScheduleAPI::StatusToString(*k) +
                         " provided more than once");
        } else {
            no_duplicates.push_back(*k);
        }
    }
    return no_duplicates;
}


bool CNetScheduleHandler::x_NeedCmdLogging(void) const
{
    // All the commands in a connection should be logged. The connection
    // context is created based on an ini file logging setting. So in order to
    // know if a command logging is required the existance of the connection
    // context should be checked.
    return m_ConnContext.NotNull();
}

void CNetScheduleHandler::x_SetRequestContext(void)
{
    // Sets the appropriate request context if one was created
    if (m_ConnContext.NotNull()) {
        if (m_CmdContext.NotNull())
            CDiagContext::SetRequestContext(m_CmdContext);
        else
            CDiagContext::SetRequestContext(m_ConnContext);
    }
}

// Utility function to make sure there is exactly one \n at the end of the
// message if the message is not empty
void CNetScheduleHandler::x_MakeSureSingleEOR(string &  message)
{
    while (NStr::EndsWith(message, '\n')) {
        message.resize(message.size() - 1);
    }
    if (!message.empty())
        message += kEndOfResponse;
}

