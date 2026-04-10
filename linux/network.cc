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

#include <cstring>
#include <fstream>
#include <net/if.h>
#include <sstream>
#include <string>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "network.h"
#include "network_reader.h"

static bool fail_linux_network_snapshot( NetworkSnapshot & snapshot,
  std::string & error_message,
  const std::string & message )
{
  error_message = message;
  snapshot.ok = false;
  snapshot.error_message = message;
  snapshot.interfaces.clear();
  return false;
}

static bool is_blank_linux_network_line( const std::string & line )
{
  return line.find_first_not_of( " \t\r" ) == std::string::npos;
}

static bool read_linux_interface_oper_state( const std::string & interface_name,
  LinuxInterfaceState & state )
{
  state.oper_state_known = false;
  state.oper_state_up = false;

  std::ifstream input(
    std::string( "/sys/class/net/" ) + interface_name + "/operstate" );
  std::string operstate;

  if( !input || !( input >> operstate ) )
  {
    return true;
  }

  if( operstate == "unknown" )
  {
    return true;
  }

  state.oper_state_known = true;
  state.oper_state_up = operstate == "up";
  return true;
}

static bool read_linux_interface_state(
  const std::string & interface_name,
  LinuxInterfaceState & state,
  std::string & error_message )
{
  error_message.clear();
  state = LinuxInterfaceState();

  if( interface_name.size() >= IFNAMSIZ )
  {
    error_message = "interface name is too long";
    return false;
  }

  int socket_fd = socket( AF_INET, SOCK_DGRAM, 0 );
  if( socket_fd < 0 )
  {
    error_message = "failed to open interface state socket";
    return false;
  }

  struct ifreq request;
  std::memset( &request, 0, sizeof( request ) );
  std::strncpy( request.ifr_name, interface_name.c_str(),
    sizeof( request.ifr_name ) - 1 );

  bool ok = true;
  if( ioctl( socket_fd, SIOCGIFFLAGS, &request ) != 0 )
  {
    error_message = "failed to query interface flags";
    ok = false;
  }
  else
  {
    state.is_up = ( request.ifr_flags & IFF_UP ) != 0;
    state.is_running = ( request.ifr_flags & IFF_RUNNING ) != 0;
    read_linux_interface_oper_state( interface_name, state );
  }

  close( socket_fd );
  return ok;
}

bool read_linux_network_snapshot( std::istream & input,
  NetworkSnapshot & snapshot,
  std::string & error_message,
  LinuxInterfaceStateReader state_reader )
{
  error_message.clear();
  snapshot.ok = true;
  snapshot.error_message.clear();
  snapshot.interfaces.clear();

  if( state_reader == NULL )
  {
    state_reader = read_linux_interface_state;
  }

  std::string line;
  if( !std::getline( input, line ) ||
      line.find( "Inter-|" ) == std::string::npos )
  {
    return fail_linux_network_snapshot( snapshot, error_message,
      "failed to parse /proc/net/dev header" );
  }

  if( !std::getline( input, line ) ||
      line.find( "face |" ) == std::string::npos )
  {
    return fail_linux_network_snapshot( snapshot, error_message,
      "failed to parse /proc/net/dev header" );
  }

  bool found_interface = false;
  bool any_state_lookup_succeeded = false;
  bool any_state_lookup_failed = false;
  std::string state_error_message;

  while( std::getline( input, line ) )
  {
    if( is_blank_linux_network_line( line ) )
    {
      continue;
    }

    std::string::size_type colon = line.find( ':' );
    if( colon == std::string::npos )
    {
      return fail_linux_network_snapshot( snapshot, error_message,
        "failed to parse /proc/net/dev row" );
    }

    std::string name = line.substr( 0, colon );
    std::string::size_type start = name.find_first_not_of( " \t" );
    if( start == std::string::npos )
    {
      return fail_linux_network_snapshot( snapshot, error_message,
        "failed to parse /proc/net/dev row" );
    }
    name = name.substr( start );

    std::istringstream counters( line.substr( colon + 1 ) );
    NetworkInterfaceSample sample;
    LinuxInterfaceState state;
    std::string sample_error_message;
    sample.id = name;
    sample.name = name;
    sample.is_loopback = ( name == "lo" );

    if( !state_reader( name, state, sample_error_message ) )
    {
      any_state_lookup_failed = true;
      state = LinuxInterfaceState();

      if( state_error_message.empty() )
      {
        if( sample_error_message.empty() )
        {
          state_error_message = "failed to query interface state";
        }
        else
        {
          state_error_message = sample_error_message;
        }
      }
    }
    else
    {
      any_state_lookup_succeeded = true;
    }

    sample.is_active = network_interface_is_active_from_oper_state(
      state.is_up,
      state.is_running,
      state.oper_state_known,
      state.oper_state_up );
    sample.has_stats = true;
    counters >> sample.rx_bytes;

    unsigned long long ignored = 0;
    for( int i = 0; i < 7; ++i )
    {
      counters >> ignored;
    }

    counters >> sample.tx_bytes;

    if( counters.fail() )
    {
      return fail_linux_network_snapshot( snapshot, error_message,
        "failed to parse /proc/net/dev row" );
    }

    found_interface = true;
    snapshot.interfaces.push_back( sample );
  }

  if( !found_interface )
  {
    return fail_linux_network_snapshot( snapshot, error_message,
      "failed to parse /proc/net/dev row" );
  }

  if( !any_state_lookup_succeeded && any_state_lookup_failed )
  {
    return fail_linux_network_snapshot( snapshot, error_message,
      state_error_message );
  }

  return true;
}

bool read_linux_network_snapshot( std::istream & input,
  NetworkSnapshot & snapshot,
  std::string & error_message )
{
  return read_linux_network_snapshot( input, snapshot, error_message, NULL );
}

bool read_network_snapshot( NetworkSnapshot & snapshot )
{
  std::ifstream input( "/proc/net/dev" );
  if( !input )
  {
    snapshot.ok = false;
    snapshot.error_message = "unable to open /proc/net/dev";
    snapshot.interfaces.clear();
    return false;
  }

  std::string error_message;
  return read_linux_network_snapshot( input, snapshot, error_message );
}

void net_status( NetworkStatus & status, const std::string & interface,
  unsigned int net_usage_delay )
{
  NetworkOptions options;
  options.mode = NETWORK_MODE_DEFAULT;
  options.sample_delay_us = net_usage_delay;

  if( !interface.empty() )
  {
    options.interface_names.push_back( interface );
  }

  status = sample_network_status( options );
}
