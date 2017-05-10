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
 * Authors:  Victor Joukov
 *
 * File Description: Utility functions for NetSchedule
 *
 */

#include <ncbi_pch.hpp>

#include "ns_util.hpp"
#include "ns_queue.hpp"
#include "ns_ini_params.hpp"
#include "ns_queue_parameters.hpp"
#include "queue_database.hpp"
#include <util/bitset/bmalgo.h>
#include <util/checksum.hpp>
#include <connect/ncbi_socket.hpp>

#include <unistd.h>


BEGIN_NCBI_SCOPE


static void NS_ValidateServerSection(const IRegistry &  reg,
                                     vector<string> &   warnings,
                                     bool               throw_port_exception,
                                     bool &             decrypting_error);
static void NS_ValidateBDBSection(const IRegistry &  reg,
                                  vector<string> &   warnings);
static void NS_ValidateQueuesAndClasses(const IRegistry &  reg,
                                        list<string> &  queues,
                                        vector<string> &  warnings);
static TQueueParams NS_ValidateClasses(const IRegistry &  reg,
                                       vector<string> &  warnings);
static void NS_ValidateQueues(const IRegistry &  reg,
                              const TQueueParams &  qclasses,
                              list<string> &  queues,
                              vector<string> &  warnings);
static void NS_ValidateServiceToQueueSection(const IRegistry &  reg,
                                             const list<string> &  queues,
                                             vector<string> &  warnings);
static string NS_OutOfLimitMessage(const string &  section,
                                   const string &  entry,
                                   unsigned int  low_limit,
                                   unsigned int  high_limit);
static void
NS_ValidateRegistrySettings(const IRegistry &  reg,
                            const string &  section,
                            const string &  name,
                            unsigned int  default_max,
                            unsigned int  default_high_mark_percentage,
                            unsigned int  default_low_mark_percentage,
                            unsigned int  default_high_removal,
                            unsigned int  default_low_removal,
                            unsigned int  default_dirt_percentage,
                            vector<string> &  warnings);


// Forms the ini file value name for warnings
string NS_RegValName(const string &  section, const string &  entry)
{
    return "[" + section + "]/" + entry;
}

bool NS_ValidateDouble(const IRegistry &  reg,
                       const string &  section, const string &  entry,
                       vector<string> &  warnings)
{
    try {
        reg.GetDouble(section, entry, 0.0);
    } catch (...) {
        warnings.push_back(g_WarnPrefix + NS_RegValName(section, entry) +
                           ". Expected a floating point value.");
        return false;
    }
    return true;
}

bool NS_ValidateBool(const IRegistry &  reg,
                     const string &  section, const string &  entry,
                     vector<string> &  warnings)
{
    try {
        reg.GetBool(section, entry, false);
    } catch (...) {
        warnings.push_back(g_WarnPrefix + NS_RegValName(section, entry) +
                           ". Expected boolean value.");
        return false;
    }
    return true;
}

bool NS_ValidateInt(const IRegistry &  reg,
                    const string &  section, const string &  entry,
                    vector<string> &  warnings)
{
    try {
        reg.GetInt(section, entry, 0);
    } catch (...) {
        warnings.push_back(g_WarnPrefix + NS_RegValName(section, entry) +
                           ". Expected integer value.");
        return false;
    }
    return true;
}

bool NS_ValidateString(const IRegistry &  reg,
                       const string &  section, const string &  entry,
                       vector<string> &  warnings)
{
    try {
        reg.GetString(section, entry, "");
    } catch (...) {
        warnings.push_back(g_WarnPrefix + NS_RegValName(section, entry) +
                           ". Expected string value.");
        return false;
    }
    return true;
}


bool NS_ValidateDataSize(const IRegistry &  reg,
                         const string &  section, const string &  entry,
                         vector<string> &  warnings)
{
    if (!reg.HasEntry(section, entry))
        return true;

    try {
        CConfig                         conf(reg);
        const CConfig::TParamTree *     param_tree = conf.GetTree();
        const TPluginManagerParamTree * section_tree =
                                            param_tree->FindSubNode(section);

        if (!section_tree)
            return true;    // Section not found

        CConfig     c((CConfig::TParamTree*)section_tree, eNoOwnership);
        c.GetDataSize("netschedule", entry, CConfig::eErr_Throw, 0);
    } catch (...) {
        warnings.push_back(g_WarnPrefix + NS_RegValName(section, entry) +
                           ". Expected data size value.");
        return false;
    }
    return true;
}


unsigned int NS_GetDataSize(const IRegistry &  reg,
                            const string &  section, const string &  entry,
                            unsigned int  default_val)
{
    CConfig                         conf(reg);
    const CConfig::TParamTree *     param_tree = conf.GetTree();
    const TPluginManagerParamTree * section_tree =
                                        param_tree->FindSubNode(section);

    if (!section_tree)
        return default_val;

    CConfig     c((CConfig::TParamTree*)section_tree, eNoOwnership);
    return c.GetDataSize("netschedule", entry, CConfig::eErr_NoThrow,
                         default_val);
}


void NS_ValidateConfigFile(const IRegistry &  reg, vector<string> &  warnings,
                           bool  throw_port_exception,
                           bool &  decrypting_error)
{
    list<string>    queues;

    NS_ValidateServerSection(reg, warnings, throw_port_exception,
                             decrypting_error);
    NS_ValidateBDBSection(reg, warnings);
    NS_ValidateQueuesAndClasses(reg, queues, warnings);
    NS_ValidateServiceToQueueSection(reg, queues, warnings);
}


// Populates the warnings list if there are problems
void NS_ValidateServerSection(const IRegistry &  reg,
                              vector<string> &   warnings,
                              bool               throw_port_exception,
                              bool &             decrypting_error)
{
    const string    section = "server";

    // port is a unique value in this section. NS must not start
    // if there is a problem with port.
    bool    ok = NS_ValidateInt(reg, section, "port", warnings);
    if (ok) {
        int     port_val = reg.GetInt(section, "port", 0);
        if (port_val < port_low_limit || port_val > port_high_limit) {
            string  msg = "Invalid " + NS_RegValName(section, "port") +
                          " value. Allowed range: " +
                          NStr::NumericToString(port_low_limit) +
                          " to " + NStr::NumericToString(port_high_limit);
            if (throw_port_exception)
                NCBI_THROW(CNetScheduleException, eInvalidParameter, msg);
            warnings.push_back(msg);
        }
    } else {
        if (throw_port_exception)
            NCBI_THROW(CNetScheduleException, eInvalidParameter,
                       "Invalid " + NS_RegValName(section, "port") +
                       " parameter.");
    }

    ok = NS_ValidateInt(reg, section, "max_connections", warnings);
    if (ok) {
        int     val = reg.GetInt(section, "max_connections",
                                 default_max_connections);
        if (val < int(max_connections_low_limit) ||
            val > int(max_connections_high_limit)) {
            string  message = NS_OutOfLimitMessage(section, "max_connections",
                                                   max_connections_low_limit,
                                                   max_connections_high_limit);
            // See the logic in SNS_Parameters::Read()::ns_server_params.cpp
            if (val < int(max_connections_low_limit))
                message += " Value " + NStr::NumericToString(
                                (int)((max_connections_low_limit +
                                       max_connections_high_limit) / 2));
            else
                message += " Value " + NStr::NumericToString(
                                            max_connections_high_limit);
            warnings.push_back(message + " will be used when restarted");
        }
    }

    ok = NS_ValidateInt(reg, section, "max_threads", warnings);
    unsigned int    max_threads_val = default_max_threads;
    if (ok) {
        int     val = reg.GetInt(section, "max_threads", default_max_threads);
        if (val < int(max_threads_low_limit) ||
            val > int(max_threads_high_limit))
            warnings.push_back(NS_OutOfLimitMessage(section, "max_threads",
                                                    max_threads_low_limit,
                                                    max_threads_high_limit));
        else
            max_threads_val = val;
    }

    ok = NS_ValidateInt(reg, section, "init_threads", warnings);
    unsigned int    init_threads_val = default_init_threads;
    if (ok) {
        int     val = reg.GetInt(section, "init_threads", default_init_threads);
        if (val < int(init_threads_low_limit) ||
            val > int(init_threads_high_limit))
            warnings.push_back(NS_OutOfLimitMessage(section, "init_threads",
                                                    init_threads_low_limit,
                                                    init_threads_high_limit));
        else
            init_threads_val = val;
    }

    if (init_threads_val > max_threads_val)
        warnings.push_back(g_ValidPrefix + "values " +
                 NS_RegValName(section, "max_threads") + " and " +
                 NS_RegValName(section, "init_threads") +
                 " break the mandatory condition "
                 "init_threads <= max_threads");


    NS_ValidateBool(reg, section, "use_hostname", warnings);

    ok = NS_ValidateInt(reg, section, "network_timeout", warnings);
    if (ok) {
        int     val = reg.GetInt(section, "network_timeout",
                                 default_network_timeout);
        if (val <= 0)
            warnings.push_back(g_ValidPrefix + "value " +
                     NS_RegValName(section, "network_timeout") +
                     " must be > 0");
    }

    NS_ValidateBool(reg, section, "log", warnings);
    NS_ValidateBool(reg, section, "log_batch_each_job", warnings);
    NS_ValidateBool(reg, section, "log_notification_thread", warnings);
    NS_ValidateBool(reg, section, "log_cleaning_thread", warnings);
    NS_ValidateBool(reg, section, "log_execution_watcher_thread", warnings);
    NS_ValidateBool(reg, section, "log_statistics_thread", warnings);


    ok = NS_ValidateInt(reg, section, "del_batch_size", warnings);
    unsigned int    del_batch_size_val = default_del_batch_size;
    if (ok) {
        int     val = reg.GetInt(section, "del_batch_size",
                                 default_del_batch_size);
        if (val <= 0)
            warnings.push_back(g_ValidPrefix + "value " +
                     NS_RegValName(section, "del_batch_size") +
                     " must be > 0");
        else
            del_batch_size_val = val;
    }

    ok = NS_ValidateInt(reg, section, "markdel_batch_size", warnings);
    unsigned int    markdel_batch_size_val = default_markdel_batch_size;
    if (ok) {
        int     val = reg.GetInt(section, "markdel_batch_size",
                                 default_markdel_batch_size);
        if (val <= 0)
            warnings.push_back(g_ValidPrefix + "value " +
                     NS_RegValName(section, "markdel_batch_size") +
                     " must be > 0");
        else
            markdel_batch_size_val = val;
    }

    ok = NS_ValidateInt(reg, section, "scan_batch_size", warnings);
    unsigned int    scan_batch_size_val = default_scan_batch_size;
    if (ok) {
        int     val = reg.GetInt(section, "scan_batch_size",
                                 default_scan_batch_size);
        if (val <= 0)
            warnings.push_back(g_ValidPrefix + "value " +
                     NS_RegValName(section, "scan_batch_size") +
                     " must be > 0");
        else
            scan_batch_size_val = val;
    }

    ok = NS_ValidateDouble(reg, section, "purge_timeout", warnings);
    if (ok) {
        double  val = reg.GetDouble(section, "purge_timeout",
                                    default_purge_timeout);
        if (val <= 0.0)
            warnings.push_back(g_ValidPrefix + "value " +
                     NS_RegValName(section, "purge_timeout") +
                     " must be > 0");
    }

    if (scan_batch_size_val < markdel_batch_size_val)
        warnings.push_back(g_ValidPrefix + "values " +
                 NS_RegValName(section, "scan_batch_size") + " and " +
                 NS_RegValName(section, "markdel_batch_size") +
                 " break the mandatory condition "
                 "markdel_batch_size <= scan_batch_size");

    if (markdel_batch_size_val < del_batch_size_val)
        warnings.push_back(g_ValidPrefix + "values " +
                 NS_RegValName(section, "markdel_batch_size") + " and " +
                 NS_RegValName(section, "del_batch_size") +
                 " break the mandatory condition "
                 "del_batch_size <= markdel_batch_size");


    ok = NS_ValidateInt(reg, section, "stat_interval", warnings);
    if (ok) {
        int     val = reg.GetInt(section, "stat_interval",
                                 default_stat_interval);
        if (val <= 0)
            warnings.push_back(g_ValidPrefix + "value " +
                     NS_RegValName(section, "stat_interval") +
                     " must be > 0");
    }

    ok = NS_ValidateInt(reg, section, "job_counters_interval", warnings);
    if (ok) {
        int     val = reg.GetInt(section, "job_counters_interval",
                                 default_job_counters_interval);
        if (val < 0)
            warnings.push_back(g_ValidPrefix + "value " +
                    NS_RegValName(section, "job_counters_interval") +
                    " must be >= 0");
    }

    ok = NS_ValidateInt(reg, section, "max_client_data", warnings);
    if (ok) {
        int     val = reg.GetInt(section, "max_client_data",
                                 default_max_client_data);
        if (val <= 0)
            warnings.push_back(g_ValidPrefix + "value " +
                     NS_RegValName(section, "max_client_data") +
                     " must be > 0");
    }


    NS_ValidateRegistrySettings(reg, section, "affinity",
                                default_max_affinities,
                                default_affinity_high_mark_percentage,
                                default_affinity_low_mark_percentage,
                                default_affinity_high_removal,
                                default_affinity_low_removal,
                                default_affinity_dirt_percentage,
                                warnings);
    NS_ValidateRegistrySettings(reg, section, "group",
                                default_max_groups,
                                default_group_high_mark_percentage,
                                default_group_low_mark_percentage,
                                default_group_high_removal,
                                default_group_low_removal,
                                default_group_dirt_percentage,
                                warnings);
    NS_ValidateRegistrySettings(reg, section, "scope",
                                default_max_scopes,
                                default_scope_high_mark_percentage,
                                default_scope_low_mark_percentage,
                                default_scope_high_removal,
                                default_scope_low_removal,
                                default_scope_dirt_percentage,
                                warnings);

    NS_ValidateString(reg, section, "admin_host", warnings);
    NS_ValidateString(reg, section,
                      "state_transition_perf_log_queues", warnings);
    NS_ValidateString(reg, section,
                      "state_transition_perf_log_classes", warnings);

    // Instead of just validating the administrator names we try to read them
    decrypting_error = false;
    try {
        reg.GetEncryptedString(section, "admin_client_name",
                               IRegistry::fPlaintextAllowed);
    } catch (const CRegistryException &  ex) {
        warnings.push_back(g_ValidPrefix +
                           NS_RegValName(section, "admin_client_name") +
                           " decrypting error detected. " +
                           string(ex.what()));
        decrypting_error = true;
    } catch (...) {
        warnings.push_back(g_ValidPrefix +
                           NS_RegValName(section, "admin_client_name") +
                           " unknown decrypting error");
        decrypting_error = true;
    }

    NS_ValidateDataSize(reg, section, "reserve_dump_space", warnings);

    ok = NS_ValidateInt(reg, section, "wst_cache_size", warnings);
    if (ok) {
        int     val = reg.GetInt(section, "wst_cache_size",
                                 default_wst_cache_size);
        if (val < 0)
            warnings.push_back(g_ValidPrefix + "value " +
                     NS_RegValName(section, "wst_cache_size") +
                     " must be >= 0");
    }
}


// Validates the [bdb] section
void NS_ValidateBDBSection(const IRegistry &  reg,
                           vector<string> &   warnings)
{
    CConfig                         conf(reg);
    const CConfig::TParamTree*      param_tree = conf.GetTree();
    const TPluginManagerParamTree*  bdb_tree = param_tree->FindSubNode("bdb");

    if (!bdb_tree)
        return;

    CConfig bdb_conf((CConfig::TParamTree*)bdb_tree, eNoOwnership);
    bool    database_in_ram = bdb_conf.GetBool("netschedule",
                                               "database_in_ram",
                                               CConfig::eErr_NoThrow, false);
    if (!database_in_ram)
        return;

    Uint8   cache_ram_size = bdb_conf.GetDataSize("netschedule",
                                                  "mem_size",
                                                  CConfig::eErr_NoThrow, 0);
    // CXX-9245
    if (cache_ram_size == 0) {
        warnings.push_back(
            g_ValidPrefix + NS_RegValName("bdb", "mem_size") +
            " needs to be adjusted to 2GB because " +
            NS_RegValName("bdb", "database_in_ram") +
            " is true and mem_size is zero.");
    } else if (cache_ram_size < kBDBMemSizeInMemLowLimit) {
        warnings.push_back(
            g_ValidPrefix + NS_RegValName("bdb", "mem_size") +
            "needs to be adjusted to 100 MB because " +
            NS_RegValName("bdb", "database_in_ram") +
            " is true and mem_size is less than 100MB.");
    }
}

// Populates the warnings list if there are problems in the config file
// the queues parameter is filled with what queues will be accepted
void NS_ValidateQueuesAndClasses(const IRegistry &  reg,
                                 list<string> &     queues,
                                 vector<string> &   warnings)
{
    TQueueParams    qclasses;

    qclasses = NS_ValidateClasses(reg, warnings);
    NS_ValidateQueues(reg, qclasses, queues, warnings);
}


TQueueParams NS_ValidateClasses(const IRegistry &   reg,
                                vector<string> &    warnings)
{
    TQueueParams        queue_classes;
    list<string>        sections;

    reg.EnumerateSections(&sections);

    ITERATE(list<string>, it, sections) {
        string              queue_class;
        string              prefix;
        const string &      section_name = *it;

        NStr::SplitInTwo(section_name, "_", prefix, queue_class);
        if (NStr::CompareNocase(prefix, "qclass") != 0)
           continue;
        if (queue_class.empty()) {
            warnings.push_back(g_ValidPrefix + "section " + section_name +
                               " does not have a queue class name");
            continue;
        }
        if (queue_class.size() > kMaxQueueNameSize - 1) {
            warnings.push_back(g_ValidPrefix + "section " + section_name +
                               " introduces a queue class which name length"
                               " exceeds the max allowed lenght of " +
                               NStr::NumericToString(kMaxQueueNameSize - 1) +
                               " bytes");
            continue;
        }

        SQueueParameters    params;
        if (params.ReadQueueClass(reg, section_name, warnings)) {
            // false => problems with linked sections; see CXX-2617
            // The same sections cannot appear twice
            queue_classes[queue_class] = params;
        }
    }

    return queue_classes;
}


// Returns true if the config file is well formed
void NS_ValidateQueues(const IRegistry &     reg,
                       const TQueueParams &  qclasses,
                       list<string> &        queues,
                       vector<string> &      warnings)
{
    list<string>    sections;

    reg.EnumerateSections(&sections);
    for (list<string>::const_iterator  k = sections.begin();
         k != sections.end(); ++k) {
        string      queue_name;
        string      prefix;
        string      section_name = *k;

        NStr::SplitInTwo(section_name, "_", prefix, queue_name);
        if (NStr::CompareNocase(prefix, "queue") != 0)
           continue;

        if (queue_name.empty()) {
            warnings.push_back(g_ValidPrefix + "section " + section_name +
                               " does not have a queue name");
            continue;
        }
        if (queue_name.size() > kMaxQueueNameSize - 1) {
            warnings.push_back(g_ValidPrefix + "section " + section_name +
                               " introduces a queue which name length"
                               " exceeds the max allowed lenght of " +
                               NStr::NumericToString(kMaxQueueNameSize - 1) +
                               " bytes");
            continue;
        }

        SQueueParameters    params;
        if (params.ReadQueue(reg, section_name, qclasses, warnings)) {
            // false => problems with linked sections; see CXX-2617
            queues.push_back(queue_name);
        }
    }
}


// Returns true if the config file is well formed
void NS_ValidateServiceToQueueSection(const IRegistry &     reg,
                                      const list<string> &  queues,
                                      vector<string> &      warnings)
{
    const string            section = "service_to_queue";
    list<string>            entries;
    reg.EnumerateEntries(section, &entries);

    for (list<string>::const_iterator  k = entries.begin();
         k != entries.end(); ++k) {
        string      service_name = *k;
        string      qname = reg.Get(section, service_name);
        if (qname.empty()) {
            warnings.push_back(g_ValidPrefix +
                               NS_RegValName(section, service_name) +
                               " does not specify a queue name");
            continue;
        }

        // Check that the queue name has been provided
        if (find(queues.begin(), queues.end(), qname) == queues.end())
            warnings.push_back(g_ValidPrefix +
                               NS_RegValName(section, service_name) +
                               " does not point to an existing static queue");
    }
}


// Forms an out of limits message
string NS_OutOfLimitMessage(const string &  section,
                            const string &  entry,
                            unsigned int  low_limit,
                            unsigned int  high_limit)
{
    return g_ValidPrefix + NS_RegValName(section, entry) +
           " is out of limits (" + NStr::NumericToString(low_limit) + "..." +
           NStr::NumericToString(high_limit) + ")";

}


static const string     s_ErrorGettingChecksum = "error detected";


string NS_GetConfigFileChecksum(const string &  file_name,
                                vector<string> &  warnings)
{
    // Note: at the time of writing the ComputeFileCRC32() function does not
    //       generate exceptions at least in some error cases e.g. if there
    //       is no file. Therefore some manual checks are introduced here:
    //       - the file exists
    //       - there are read permissions for it
    // Technically speaking it is not 100% guarantee because the file could be
    // replaced between the checks and the sum culculation but it is better
    // than nothing.

    if (access(file_name.c_str(), F_OK) != 0) {
        warnings.push_back("Error computing config file checksum, "
                           "the file does not exist: " + file_name);
        return s_ErrorGettingChecksum;
    }

    if (access(file_name.c_str(), R_OK) != 0) {
        warnings.push_back("Error computing config file checksum, "
                           "there are no read permissions: " + file_name);
        return s_ErrorGettingChecksum;
    }

    try {
        string      checksum_as_string;
        CChecksum   checksum(CChecksum::eMD5);

        checksum.AddFile(file_name);
        checksum.GetMD5Digest(checksum_as_string);
        return checksum_as_string;
    } catch (const exception &  ex) {
        warnings.push_back("Error computing config file checksum. " +
                           string(ex.what()));
        return s_ErrorGettingChecksum;
    } catch (...) {
        warnings.push_back("Unknown error of computing config file checksum");
        return s_ErrorGettingChecksum;
    }
    return s_ErrorGettingChecksum;
}


void NS_ValidateRegistrySettings(const IRegistry &  reg,
                                 const string &  section,
                                 const string &  name,
                                 unsigned int  default_max,
                                 unsigned int  default_high_mark_percentage,
                                 unsigned int  default_low_mark_percentage,
                                 unsigned int  default_high_removal,
                                 unsigned int  default_low_removal,
                                 unsigned int  default_dirt_percentage,
                                 vector<string> &  warnings)
{
    bool        ok = true;
    string      plural;

    if (name == "affinity")
        plural = "affinities";
    else
        plural = name + "s";


    ok = NS_ValidateInt(reg, section, "max_" + plural, warnings);
    if (ok) {
        int     val = reg.GetInt(section, "max_" + plural, default_max);
        if (val <= 0)
            warnings.push_back(g_ValidPrefix + "value " +
                     NS_RegValName(section, "max_" + plural) +
                     " must be > 0");
    }

    ok = NS_ValidateInt(reg, section, name + "_high_mark_percentage",
                        warnings);
    unsigned int    high_mark_percentage_val =
                                default_high_mark_percentage;
    if (ok) {
        int     val = reg.GetInt(section, name + "_high_mark_percentage",
                                 default_high_mark_percentage);
        if (val <= 0 || val >= 100)
            warnings.push_back(g_ValidPrefix + "value " +
                     NS_RegValName(section, name + "_high_mark_percentage") +
                     " must be between 0 and 100");
        else
            high_mark_percentage_val = val;
    }

    ok = NS_ValidateInt(reg, section, name + "_low_mark_percentage",
                        warnings);
    unsigned int    low_mark_percentage_val =
                                default_low_mark_percentage;
    if (ok) {
        int     val = reg.GetInt(section, name + "_low_mark_percentage",
                                 default_low_mark_percentage);
        if (val <= 0 || val >= 100)
            warnings.push_back(g_ValidPrefix + "value " +
                     NS_RegValName(section, name + "_low_mark_percentage") +
                     " must be between 0 and 100");
        else
            low_mark_percentage_val = val;
    }

    ok = NS_ValidateInt(reg, section, name + "_high_removal", warnings);
    unsigned int    high_removal_val = default_high_removal;
    if (ok) {
        int     val = reg.GetInt(section, name + "_high_removal",
                                 default_high_removal);
        if (val <= 0)
            warnings.push_back(g_ValidPrefix + "value " +
                     NS_RegValName(section, name + "_high_removal") +
                     " must be > 0");
        else
            high_removal_val = val;
    }

    ok = NS_ValidateInt(reg, section, name + "_low_removal", warnings);
    unsigned int    low_removal_val = default_low_removal;
    if (ok) {
        int     val = reg.GetInt(section, name + "_low_removal",
                                 default_low_removal);
        if (val <= 0)
            warnings.push_back(g_ValidPrefix + "value " +
                     NS_RegValName(section, name + "_low_removal") +
                     " must be > 0");
        else
            low_removal_val = val;
    }

    ok = NS_ValidateInt(reg, section, name + "_dirt_percentage", warnings);
    unsigned int    dirt_percentage_val = default_dirt_percentage;
    if (ok) {
        int     val = reg.GetInt(section, name + "_dirt_percentage",
                                 default_dirt_percentage);
        if (val <= 0 || val >= 100)
            warnings.push_back(g_ValidPrefix + "value " +
                     NS_RegValName(section, name + "_dirt_percentage") +
                     " must be between 0 and 100");
        else
            dirt_percentage_val = val;
    }

    if (low_mark_percentage_val >= high_mark_percentage_val)
        warnings.push_back(g_ValidPrefix + "values " +
                 NS_RegValName(section, name + "_low_mark_percentage") +
                 " and " +
                 NS_RegValName(section, name + "_high_mark_percentage") +
                 " break the mandatory condition " +
                 name + "_low_mark_percentage < " +
                 name + "_high_mark_percentage");

    if (dirt_percentage_val >= low_mark_percentage_val)
        warnings.push_back(g_ValidPrefix + "values " +
                 NS_RegValName(section, name + "_low_mark_percentage") +
                 " and " +
                 NS_RegValName(section, name + "_dirt_percentage") +
                 " break the mandatory condition " +
                 name + "_dirt_percentage < " +
                 name + "_low_mark_percentage");

    if (high_removal_val < low_removal_val)
        warnings.push_back(g_ValidPrefix + "values " +
                 NS_RegValName(section, name + "_high_removal") +
                 " and " +
                 NS_RegValName(section, name + "_low_removal") +
                 " break the mandatory condition " +
                 name + "_low_removal <= " +
                 name + "_high_removal");
}


END_NCBI_SCOPE

