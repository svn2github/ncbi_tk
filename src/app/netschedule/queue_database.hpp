#ifndef NETSCHEDULE_QUEUE_DATABASE__HPP
#define NETSCHEDULE_QUEUE_DATABASE__HPP


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
 * File Description:
 *   Top level queue database (Thread-Safe, synchronized).
 *
 */


#include <corelib/ncbimtx.hpp>
#include <corelib/ncbicntr.hpp>

#include <utility>

#include <connect/services/netschedule_api.hpp>
#include <connect/services/json_over_uttp.hpp>
#include "ns_util.hpp"
#include "job_status.hpp"
#include "queue_clean_thread.hpp"
#include "ns_notifications.hpp"
#include "ns_queue.hpp"
#include "queue_vc.hpp"
#include "background_host.hpp"
#include "ns_service_thread.hpp"
#include "ns_precise_time.hpp"

BEGIN_NCBI_SCOPE

// See CXX-9245
const Uint8 kBDBMemSizeInMemDefault = 2 * 1000 * 1000 * 1000;   // 2 GB
const Uint8 kBDBMemSizeInMemLowLimit = 100 * 1000 * 1000;       // 100 MB

class CNetScheduleServer;


// Holds parameters together with a queue instance
typedef map<string,
            pair<SQueueParameters, CRef<CQueue> >,
            PNocase >   TQueueInfo;


struct SNSDBEnvironmentParams
{
    string    db_path;
    string    db_log_path;
    unsigned  max_queues;          // Number of pre-allocated queues
    Uint8     cache_ram_size;      // Size of database cache
    unsigned  mutex_max;           // Number of mutexes
    unsigned  max_locks;           // Number of locks
    unsigned  max_lockers;         // Number of lockers
    unsigned  max_lockobjects;     // Number of lock objects

    //    Size of in-memory LOG (when 0 log is put to disk)
    //    In memory LOG is not durable, put it to memory
    //    only if you need better performance
    Uint8     log_mem_size;
    unsigned  max_trans;           // Maximum number of active transactions
    unsigned  checkpoint_kb;
    unsigned  checkpoint_min;
    bool      sync_transactions;
    bool      direct_db;
    bool      direct_log;
    bool      database_in_ram;

    bool Read(const IRegistry& reg, const string& sname);
};




// Top level queue database. (Thread-Safe, synchronized.)
class CQueueDataBase
{
public:
    CQueueDataBase(CNetScheduleServer *            server,
                   const SNSDBEnvironmentParams &  params,
                   bool                            reinit);
    ~CQueueDataBase();

    // Read queue information from registry and configure queues
    // accordingly.
    // returns minimum run timeout, necessary for watcher thread
    time_t  Configure(const IRegistry &  reg,
                      CJsonNode &        diff);

    // Count Pending and Running jobs in all queues
    unsigned int  CountActiveJobs(void) const;
    unsigned int  CountAllJobs(void) const;
    bool  AnyJobs(void) const;

    CRef<CQueue> OpenQueue(const string &  name);

    void CreateDynamicQueue(const CNSClientId &  client,
                            const string &  qname, const string &  qclass,
                            const string &  description = "");
    void DeleteDynamicQueue(const CNSClientId &  client,
                            const string &  qname);
    SQueueParameters QueueInfo(const string &  qname) const;
    string GetQueueNames(const string &  sep) const;

    void Close(void);
    bool QueueExists(const string &  qname) const;

    // Remove old jobs
    void Purge(void);
    void StopPurge(void);
    void RunPurgeThread(void);
    void StopPurgeThread(void);

    // Collect garbage from affinities
    void PurgeAffinities(void);
    void PurgeGroups(void);
    void StaleWNodes(void);
    void PurgeBlacklistedJobs(void);
    void PurgeClientRegistry(void);
    void PurgeJobInfoCache(void);

    // Notify all listeners
    void NotifyListeners(void);
    void RunNotifThread(void);
    void StopNotifThread(void);
    void WakeupNotifThread(void);
    CNSPreciseTime SendExactNotifications(void);

    // Print statistics
    void PrintStatistics(size_t &  aff_count);
    void PrintJobCounters(void);
    void RunServiceThread(void);
    void StopServiceThread(void);

    void CheckExecutionTimeout(bool  logging);
    void RunExecutionWatcherThread(const CNSPreciseTime &  run_delay);
    void StopExecutionWatcherThread(void);


    /// Force transaction checkpoint
    void TransactionCheckPoint(bool  clean_log = false);

    // BerkeleyDB-specific statistics
    void PrintMutexStat(CNcbiOstream& out)
    {
        m_Env->PrintMutexStat(out);
    }

    void PrintLockStat(CNcbiOstream& out)
    {
        m_Env->PrintLockStat(out);
    }

    void PrintMemStat(CNcbiOstream& out)
    {
        m_Env->PrintMemStat(out);
    }

    string PrintTransitionCounters(void);
    string PrintJobsStat(const CNSClientId &  client);
    string GetQueueClassesInfo(void) const;
    string GetQueueClassesConfig(void) const;
    string GetQueueInfo(void) const;
    string GetQueueConfig(void) const;
    string GetLinkedSectionConfig(void) const;

    map< string, string >  GetLinkedSection(const string &  section_name) const;

private:
    // No copy
    CQueueDataBase(const CQueueDataBase&);
    CQueueDataBase& operator=(const CQueueDataBase&);

protected:
    // get next job id (counter increment)
    unsigned int  GetNextId();

    // Returns first id for the batch
    unsigned int  GetNextIdBatch(unsigned int  count);

private:
    void x_Open(const SNSDBEnvironmentParams &  params, bool  reinit);
    void x_CreateAndMountQueue(const string &            qname,
                               const SQueueParameters &  params,
                               SQueueDbBlock *           queue_db_block);

    unsigned x_PurgeUnconditional(void);
    void     x_OptimizeStatusMatrix(const CNSPreciseTime &  current_time);
    bool     x_CheckStopPurge(void);
    SQueueParameters x_SingleQueueInfo(TQueueInfo::const_iterator  found) const;

    CBackgroundHost &    m_Host;
    CRequestExecutor &   m_Executor;
    CBDB_Env *           m_Env;
    string               m_DataPath;
    string               m_DumpPath;

    mutable CFastMutex   m_ConfigureLock;

    // Effective queue classes
    TQueueParams         m_QueueClasses;
    // Effective queues
    TQueueInfo           m_Queues;

    // Pre-allocated Berkeley DB blocks
    CQueueDbBlockArray   m_QueueDbBlockArray;

    bool                 m_StopPurge;         // Purge stop flag
    CFastMutex           m_PurgeLock;
    unsigned int         m_FreeStatusMemCnt;  // Free memory counter
    time_t               m_LastFreeMem;       // time of the last memory opt

    CRef<CJobQueueCleanerThread>            m_PurgeThread;
    CRef<CServiceThread>                    m_ServiceThread;
    CRef<CGetJobNotificationThread>         m_NotifThread;
    CRef<CJobQueueExecutionWatcherThread>   m_ExeWatchThread;

    CNetScheduleServer *                    m_Server;

private:
    // Last scan attributes
    string              m_PurgeQueue;           // The queue name
    size_t              m_PurgeStatusIndex;     // Scanned status index
    unsigned int        m_PurgeJobScanned;      // Scanned job ID within status

    // Linked sections support
    mutable CFastMutex                      m_LinkedSectionsGuard;
    // Section name -> section values
    map< string, map< string, string > >    m_LinkedSections;

    bool x_PurgeQueue(CQueue &                queue,
                      size_t                  status_to_start,
                      size_t                  status_to_end,
                      unsigned int            start_job_id,
                      unsigned int            end_job_id,
                      size_t                  max_scanned,
                      size_t                  max_mark_deleted,
                      const CNSPreciseTime &  current_time,
                      size_t &                total_scanned,
                      size_t &                total_mark_deleted);
    void  x_DeleteQueuesAndClasses(void);
    CRef<CQueue>  x_GetLastPurged(void);
    CRef<CQueue>  x_GetFirst(void);
    CRef<CQueue>  x_GetNext(const string &  current_name);

    // Crash detect support:
    // - upon start the server creates CRASH_FLAG file
    // - when gracefully finished the file is deleted
    // - at the start it is checked if the file is there. If it is then
    //   it means the server crashed
    void  x_CreateCrashFlagFile(void);
    bool  x_DoesCrashFlagFileExist(void) const;
    void  x_RemoveCrashFlagFile(void);

    // Dump problem detect support:
    // - upon dtart the server creates DUMP_ERROR_FLAG file
    // - if all the NS info was dumped successfully the file is deleted
    // - at the start it is checked if the file is there. If it is then
    //   it means the previous instance had problems dumping something
    void  x_CreateDumpErrorFlagFile(void);
    bool  x_DoesDumpErrorFlagFileExist(void) const;
    void  x_RemoveDumpErrorFlagFile(void);


    bool  x_ConfigureQueueClasses(const TQueueParams &  classes_from_ini,
                                  CJsonNode &           diff);
    bool  x_ConfigureQueues(const TQueueParams &  queues_from_ini,
                            CJsonNode &           diff);

    TQueueParams  x_ReadIniFileQueueClassDescriptions(const IRegistry &   reg);
    TQueueParams  x_ReadIniFileQueueDescriptions(const IRegistry &     reg,
                                                 const TQueueParams &  classes);
    void  x_ReadLinkedSections(const IRegistry &  reg,
                               CJsonNode &        diff);
    CJsonNode  x_DetectChangesInLinkedSection(
                                const map<string, string> &  old_values,
                                const map<string, string> &  new_values);

    void  x_ValidateConfiguration(const TQueueParams &  queues_from_ini) const;
    unsigned int
    x_CountQueuesToAdd(const TQueueParams &  queues_from_ini) const;

    CRef<CQueue>  x_GetQueueAt(unsigned int  index);

    void x_Dump(void);
    void x_DumpQueueOrClass(FILE *  f,
                            const string &  qname, const string &  qclass,
                            bool  is_queue,
                            const SQueueParameters &  params);
    void x_DumpLinkedSection(FILE *  f, const string &  sname,
                             const map<string, string> &  values);
    void x_RemoveDump(void);
    void x_RemoveBDBFiles(void);
    void x_CreateStorageVersionFile(void);

    bool x_CheckOpenPreconditions(bool  reinit);
    CBDB_Env *  x_CreateBDBEnvironment(const SNSDBEnvironmentParams &  params);

    void x_ReadDumpQueueDesrc(set<string, PNocase> &  dump_static_queues,
                              map<string, string,
                                  PNocase> &  dump_dynamic_queues,
                              TQueueParams &  dump_queue_classes);
    set<string, PNocase> x_GetConfigQueues(void);
    void x_AppendDumpLinkedSections(void);
    CNSPreciseTime CalculateRuntimePrecision(void) const;
    void x_BackupDump(void);
    void x_CreateSpaceReserveFile(void);
    bool x_RemoveSpaceReserveFile(void);
    string x_GetDumpSpaceFileName(void) const;
}; // CQueueDataBase


END_NCBI_SCOPE

#endif /* NETSCHEDULE_QUEUE_DATABASE__HPP */

