/* vim: tabstop=2 shiftwidth=2 expandtab textwidth=80 linebreak wrap
 *
 * Copyright 2012 Matthew McCormick
 * Copyright 2015 Pawel 'l0ner' Soltys
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef NETWORK_H_
#define NETWORK_H_

#include <string>
#include <vector>

enum NETWORK_MODE
{
  NETWORK_MODE_OFF,
  NETWORK_MODE_DEFAULT,
  NETWORK_MODE_DOWNLOAD_ONLY,
  NETWORK_MODE_UPLOAD_ONLY,
  NETWORK_MODE_DYNAMIC
};

enum NETWORK_STATE
{
  NETWORK_STATE_HIDDEN,
  NETWORK_STATE_OK,
  NETWORK_STATE_ERROR
};

struct NetworkOptions
{
  NetworkOptions()
    : mode( NETWORK_MODE_DEFAULT ),
      interface_names(),
      sample_delay_us( 0 )
  {
  }

  NETWORK_MODE mode;
  std::vector< std::string > interface_names;
  unsigned int sample_delay_us;
};

struct NetworkInterfaceSample
{
  NetworkInterfaceSample()
    : id(),
      name(),
      is_loopback( false ),
      is_active( false ),
      has_stats( false ),
      rx_bytes( 0 ),
      tx_bytes( 0 )
  {
  }

  std::string id;
  std::string name;
  bool is_loopback;
  bool is_active;
  bool has_stats;
  unsigned long long rx_bytes;
  unsigned long long tx_bytes;
};

struct NetworkSnapshot
{
  NetworkSnapshot()
    : ok( false ),
      error_message(),
      interfaces()
  {
  }

  bool ok;
  std::string error_message;
  std::vector< NetworkInterfaceSample > interfaces;
};

struct NetworkStatus
{
  NetworkStatus()
    : state( NETWORK_STATE_OK ),
      rx_bytes( 0 ),
      tx_bytes( 0 ),
      rx_rate( 0.0f ),
      tx_rate( 0.0f ),
      error_message()
  {
  }

  NETWORK_STATE state;
  unsigned long long rx_bytes;
  unsigned long long tx_bytes;
  float rx_rate;
  float tx_rate;
  std::string error_message;
};

std::vector< std::string > parse_network_selectors(
  const std::vector< std::string > & arguments );
NetworkStatus make_hidden_network_status();
NetworkStatus make_error_network_status( const std::string & error_message );
std::string format_guid_string( unsigned int data1,
  unsigned short data2,
  unsigned short data3,
  const unsigned char data4[8] );
bool network_interface_is_active_from_oper_state(
  bool is_up,
  bool is_running,
  bool oper_state_known,
  bool oper_state_up );
NetworkStatus build_network_status(
  const NetworkSnapshot & before,
  const NetworkSnapshot & after,
  const NetworkOptions & options );
NetworkStatus sample_network_status( const NetworkOptions & options );
bool read_network_snapshot( NetworkSnapshot & snapshot );
void net_status( NetworkStatus & status, const std::string & interface,
  unsigned int net_usage_delay );

std::string net_string( const NetworkStatus & net_status,
  NETWORK_MODE mode = NETWORK_MODE_DEFAULT,
  bool use_colors = false,
  bool use_powerline_left = false,
  bool use_powerline_right = false,
  bool segments_to_right = false,
  short right_color = 0 );

unsigned int get_net_color_idx( const NetworkStatus & net_status );

#endif
