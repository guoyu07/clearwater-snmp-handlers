/**
* Project Clearwater - IMS in the Cloud
* Copyright (C) 2015 Metaswitch Networks Ltd
*
* This program is free software: you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation, either version 3 of the License, or (at your
* option) any later version, along with the "Special Exception" for use of
* the program along with SSL, set forth below. This program is distributed
* in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more
* details. You should have received a copy of the GNU General Public
* License along with this program. If not, see
* <http://www.gnu.org/licenses/>.
*
* The author can be reached by email at clearwater@metaswitch.com or by
* post at Metaswitch Networks Ltd, 100 Church St, Enfield EN2 6BQ, UK
*
* Special Exception
* Metaswitch Networks Ltd grants you permission to copy, modify,
* propagate, and distribute a work formed by combining OpenSSL with The
* Software, or a work derivative of such a combination, even if such
* copying, modification, propagation, or distribution would otherwise
* violate the terms of the GPL. You must comply with the GPL in all
* respects for all of the code used other than OpenSSL.
* "OpenSSL" means OpenSSL toolkit software distributed by the OpenSSL
* Project and licensed under the OpenSSL Licenses, or a work based on such
* software and licensed under the OpenSSL Licenses.
* "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
* under which the OpenSSL Project distributes the OpenSSL toolkit software,
* as those licenses appear in the file LICENSE-OPENSSL.
*/

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/agent_trap.h>
#include <signal.h>
#include <string>
#include <set>
#include <vector>
#include <iostream>
#include <semaphore.h>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>

#include "utils.h"
#include "snmp_agent.h"
#include "logger.h"
#include "log.h"
#include "alarm_table_defs.hpp"
#include "alarm_req_listener.hpp"
#include "alarm_model_table.hpp"
#include "itu_alarm_table.hpp"
#include "alarm_active_table.hpp"
#include "alarm_scheduler.hpp"

static sem_t term_sem;
// Signal handler that triggers termination.
void agent_terminate_handler(int sig)
{
  sem_post(&term_sem);
}

enum OptionTypes
{
  OPT_COMMUNITY=256+1,
  OPT_SNMP_NOTIFICATION_TYPE,
  OPT_LOCAL_IP,
  OPT_HOSTNAME,
  OPT_SNMP_IPS,
  OPT_LOG_LEVEL,
  OPT_LOG_DIR
};

const static struct option long_opt[] =
{
  { "community",                       required_argument, 0, OPT_COMMUNITY},
  { "snmp-notification-types",         required_argument, 0, OPT_SNMP_NOTIFICATION_TYPE},
  { "snmp-ips",                        required_argument, 0, OPT_SNMP_IPS},
  { "local-ip",                        required_argument, 0, OPT_LOCAL_IP},
  { "hostname",                        required_argument, 0, OPT_HOSTNAME},
  { "log-level",                       required_argument, 0, OPT_LOG_LEVEL},
  { "log-dir",                         required_argument, 0, OPT_LOG_DIR},
};

static void usage(void)
{
    puts("Options:\n"
         "\n"
         " --local-ip <ip>            Local IP address of this node\n"
         " --hostname <name>          Hostname to identify this node in enterprise alarms\n"
         " --snmp-ips <ip>,<ip>       Send SNMP notifications to the specified IPs\n"
         " --community <name>         Include the given community string on notifications\n"
         " --snmp-notification-types  Sends SNMP notifiations with the specified format\n"
         " --log-dir <directory>\n"
         "                            Log to file in specified directory\n"
         " --log-level N              Set log level to N (default: 4)\n"
        );
}

int main (int argc, char **argv)
{
  std::vector<std::string> trap_ips;
  char* community = NULL;
  std::set<NotificationType> snmp_notifications;
  std::string local_ip = "0.0.0.0";
  std::string hostname = "";
  std::string logdir = "";
  int loglevel = 4;
  int c;
  int optind;

  opterr = 0;
  while ((c = getopt_long(argc, argv, "", long_opt, &optind)) != -1)
  {
    switch (c)
      {
      case OPT_COMMUNITY:
        community = optarg;
        break;
      case OPT_SNMP_NOTIFICATION_TYPE:
        {
          std::vector<std::string> notification_types;
          Utils::split_string(std::string(optarg), ',', notification_types);
          for (std::vector<std::string>::iterator it = notification_types.begin();
               it != notification_types.end();
               ++it)
          {
            if (*it == "rfc3877")
            {
              snmp_notifications.insert(NotificationType::RFC3877);
            }
            else if (*it == "enterprise")
            {
              snmp_notifications.insert(NotificationType::ENTERPRISE);
            }
            else
            {
              std::cout << "Invalid config option" << *it << " used for snmp notification type";
            }
          }
          break;
        }
      case OPT_SNMP_IPS:
        Utils::split_string(optarg, ',', trap_ips);
        break;
      case OPT_LOCAL_IP:
        local_ip = optarg;
        break;
      case OPT_HOSTNAME:
        hostname = optarg;
        break;
      case OPT_LOG_LEVEL:
        loglevel = atoi(optarg);
        break;
      case OPT_LOG_DIR:
        logdir = optarg;
        break;
      default:
        usage();
        abort();
      }
  }
  
  Log::setLoggingLevel(loglevel);
  Log::setLogger(new Logger(logdir, "clearwater-alarms"));

  // If no config options for snmp notifications have been found we use the
  // default RFC3877.
  if (snmp_notifications.empty())
  {
    snmp_notifications.insert(NotificationType::RFC3877);
    TRC_DEBUG("No SNMP notification types found, defaulting to RFC3877");
  }

  snmp_setup("clearwater-alarms");
  sem_init(&term_sem, 0, 0);
  // Connect to the informsinks
  for (std::vector<std::string>::iterator ii = trap_ips.begin();
       ii != trap_ips.end();
       ii++)
  {
    create_trap_session(const_cast<char*>(ii->c_str()), 162, community,
                        SNMP_VERSION_2c, SNMP_MSG_INFORM);  
  }

  // Initialise the ZMQ listeners and alarm tables
  // Pull in any local alarm definitions off the node.
  AlarmTableDefs* alarm_table_defs = new AlarmTableDefs();
  std::string alarms_path = "/usr/share/clearwater/infrastructure/alarms/";

  if (!alarm_table_defs->initialize(alarms_path))
  {
    TRC_ERROR("Hit error parsing the alarm file - shutting down");
    return 1;
  }

  AlarmScheduler* alarm_scheduler = new AlarmScheduler(alarm_table_defs, snmp_notifications, hostname);
  AlarmReqListener* alarm_req_listener = new AlarmReqListener(alarm_scheduler);

  init_alarmModelTable(*alarm_table_defs);
  init_ituAlarmTable(*alarm_table_defs);
  init_alarmActiveTable(local_ip);
 
  init_snmp_handler_threads("clearwater-alarms");

  // Exit if the ReqListener wasn't able to fully start
  if (!alarm_req_listener->start(&term_sem))
  {
    TRC_ERROR("Hit error starting the listener - shutting down");
    return 1;
  }

  TRC_STATUS("Alarm agent has started");
 
  signal(SIGTERM, agent_terminate_handler);
  
  sem_wait(&term_sem);
  snmp_terminate("clearwater-alarms");

  delete alarm_req_listener; alarm_req_listener = NULL;
  delete alarm_scheduler; alarm_scheduler = NULL;
  delete alarm_table_defs; alarm_table_defs = NULL;
}
