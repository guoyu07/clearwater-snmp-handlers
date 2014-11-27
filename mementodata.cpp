/**
* Project Clearwater - IMS in the Cloud
* Copyright (C) 2013 Metaswitch Networks Ltd
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

#include "globals.hpp"
#include "nodedata.hpp"
#include "custom_handler.hpp"
#include "zmq_message_handler.hpp"

OID memento_cassandra_read_latency_oid = OID("1.2.826.0.1.1578918.9.8.2.3");
OID memento_record_size_oid = OID("1.2.826.0.1.1578918.9.8.2.4");
OID memento_record_length_oid = OID("1.2.826.0.1.1578918.9.8.2.5");

OID memento_auth_challenges_oid = OID("1.2.826.0.1.1578918.9.8.3.2.2");
OID memento_auth_attempts_oid = OID("1.2.826.0.1.1578918.9.8.3.2.3");
OID memento_auth_successes_oid = OID("1.2.826.0.1.1578918.9.8.3.2.4");
OID memento_auth_failures_oid = OID("1.2.826.0.1.1578918.9.8.3.2.5");
OID memento_auth_stales_oid = OID("1.2.826.0.1.1578918.9.8.3.2.6");

AccumulatedWithCountStatHandler memento_cassandra_read_latency_handler(memento_cassandra_read_latency_oid, &tree);
AccumulatedWithCountStatHandler memento_record_size_handler(memento_record_size_oid, &tree);
AccumulatedWithCountStatHandler memento_record_length_handler(memento_record_length_oid, &tree);

BareStatHandler memento_auth_challenges_handler(memento_auth_challenges_oid, &tree);
BareStatHandler memento_auth_attempts_handler(memento_auth_attempts_oid, &tree);
BareStatHandler memento_auth_successes_handler(memento_auth_successes_oid, &tree);
BareStatHandler memento_auth_failures_handler(memento_auth_failures_oid, &tree);
BareStatHandler memento_auth_stales_handler(memento_auth_stales_oid, &tree);

NodeData::NodeData()
{
  name = "memento_handler";
  port = "6671";
  root_oid = OID("1.2.826.0.1.1578918.9.8.2");
  stats = {"auth_challenges",
           "auth_attempts",
           "auth_successes",
           "auth_failures",
           "auth_stales",
           "cassandra_read_latency",
           "record_size",
           "record_length"};
  stat_to_handler = {{"auth_challenges", &memento_auth_challenges_handler},
                     {"auth_attempts", &memento_auth_attempts_handler},
                     {"auth_successes", &memento_auth_successes_handler},
                     {"auth_failures", &memento_auth_failures_handler},
                     {"auth_stales", &memento_auth_stales_handler},
                     {"cassandra_read_latency", &memento_cassandra_read_latency_handler},
                     {"record_size", &memento_record_size_handler},
                     {"record_length", &memento_record_length_handler}
                    };
};

NodeData node_data;

extern "C"
{
  // SNMPd looks for an init_<module_name> function in this library
  void init_memento_handler()
  {
    initialize_handler();
  }
}
