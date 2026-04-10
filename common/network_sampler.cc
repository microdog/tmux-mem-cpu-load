#include <chrono>
#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "network.h"

#ifndef TMUX_MEM_CPU_LOAD_HAVE_NETWORK_READER
bool read_network_snapshot( NetworkSnapshot & snapshot )
{
  snapshot.ok = true;
  snapshot.error_message.clear();
  snapshot.interfaces.clear();
  return true;
}
#endif

static bool contains_selector( const std::vector< std::string > & selectors,
  const NetworkInterfaceSample & sample )
{
  return std::find( selectors.begin(), selectors.end(), sample.name ) !=
      selectors.end() ||
    std::find( selectors.begin(), selectors.end(), sample.id ) !=
      selectors.end();
}

std::vector< std::string > parse_network_selectors(
  const std::vector< std::string > & arguments )
{
  std::vector< std::string > selectors;
  std::set< std::string > seen;

  for( std::vector< std::string >::const_iterator argument = arguments.begin();
       argument != arguments.end();
       ++argument )
  {
    std::stringstream stream( *argument );
    std::string token;

    while( std::getline( stream, token, ',' ) )
    {
      std::string::size_type start = token.find_first_not_of( " \t" );
      std::string::size_type end = token.find_last_not_of( " \t" );

      if( start == std::string::npos )
      {
        continue;
      }

      token = token.substr( start, end - start + 1 );

      if( seen.insert( token ).second )
      {
        selectors.push_back( token );
      }
    }
  }

  return selectors;
}

NetworkStatus make_hidden_network_status()
{
  NetworkStatus status;
  status.state = NETWORK_STATE_HIDDEN;
  return status;
}

NetworkStatus make_error_network_status( const std::string & error_message )
{
  NetworkStatus status;
  status.state = NETWORK_STATE_ERROR;
  status.error_message = error_message;
  return status;
}

bool network_interface_is_active_from_oper_state(
  bool is_up,
  bool is_running,
  bool oper_state_known,
  bool oper_state_up )
{
  if( !is_up )
  {
    return false;
  }

  if( oper_state_known )
  {
    return oper_state_up;
  }

  return is_running;
}

NetworkStatus build_network_status(
  const NetworkSnapshot & before,
  const NetworkSnapshot & after,
  const NetworkOptions & options )
{
  if( options.mode == NETWORK_MODE_OFF )
  {
    return make_hidden_network_status();
  }

  if( !before.ok )
  {
    return make_error_network_status( before.error_message );
  }

  if( !after.ok )
  {
    return make_error_network_status( after.error_message );
  }

  std::vector< std::string > selectors =
    parse_network_selectors( options.interface_names );
  bool explicit_selection = !selectors.empty();
  std::map< std::string, NetworkInterfaceSample > before_by_id;

  for( std::vector< NetworkInterfaceSample >::const_iterator sample =
         before.interfaces.begin();
       sample != before.interfaces.end();
       ++sample )
  {
    before_by_id[sample->id] = *sample;
  }

  unsigned long long total_rx_delta = 0;
  unsigned long long total_tx_delta = 0;
  unsigned long long total_rx_bytes = 0;
  unsigned long long total_tx_bytes = 0;
  bool matched_interface = false;

  for( std::vector< NetworkInterfaceSample >::const_iterator sample =
         after.interfaces.begin();
       sample != after.interfaces.end();
       ++sample )
  {
    if( !sample->has_stats )
    {
      continue;
    }

    if( explicit_selection )
    {
      if( !contains_selector( selectors, *sample ) )
      {
        continue;
      }
    }
    else if( sample->is_loopback || !sample->is_active )
    {
      continue;
    }

    std::map< std::string, NetworkInterfaceSample >::const_iterator before_it =
      before_by_id.find( sample->id );
    if( before_it == before_by_id.end() || !before_it->second.has_stats )
    {
      continue;
    }

    unsigned long long rx_delta = 0;
    unsigned long long tx_delta = 0;

    if( sample->rx_bytes >= before_it->second.rx_bytes )
    {
      rx_delta = sample->rx_bytes - before_it->second.rx_bytes;
    }

    if( sample->tx_bytes >= before_it->second.tx_bytes )
    {
      tx_delta = sample->tx_bytes - before_it->second.tx_bytes;
    }

    matched_interface = true;
    total_rx_delta += rx_delta;
    total_tx_delta += tx_delta;
    total_rx_bytes += sample->rx_bytes;
    total_tx_bytes += sample->tx_bytes;
  }

  if( !matched_interface )
  {
    return make_hidden_network_status();
  }

  float time_seconds = options.sample_delay_us / 1000000.0f;
  if( time_seconds <= 0.0f )
  {
    time_seconds = 1.0f;
  }

  NetworkStatus status;
  status.state = NETWORK_STATE_OK;
  status.rx_bytes = total_rx_bytes;
  status.tx_bytes = total_tx_bytes;
  status.rx_rate = total_rx_delta / time_seconds;
  status.tx_rate = total_tx_delta / time_seconds;
  status.error_message.clear();
  return status;
}

NetworkStatus sample_network_status( const NetworkOptions & options )
{
  if( options.mode == NETWORK_MODE_OFF )
  {
    return make_hidden_network_status();
  }

  NetworkSnapshot before;
  if( !read_network_snapshot( before ) )
  {
    return make_error_network_status( before.error_message );
  }

  if( options.sample_delay_us > 0 )
  {
    std::this_thread::sleep_for(
      std::chrono::microseconds( options.sample_delay_us ) );
  }

  NetworkSnapshot after;
  if( !read_network_snapshot( after ) )
  {
    return make_error_network_status( after.error_message );
  }

  return build_network_status( before, after, options );
}
