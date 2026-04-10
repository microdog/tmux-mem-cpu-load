#include <net/if.h>
#include <ifaddrs.h>
#include <net/if_dl.h>
#include <string>

#include "network.h"

bool read_network_snapshot( NetworkSnapshot & snapshot )
{
  snapshot.ok = true;
  snapshot.error_message.clear();
  snapshot.interfaces.clear();

  struct ifaddrs * interfaces = NULL;
  if( getifaddrs( &interfaces ) != 0 )
  {
    snapshot.ok = false;
    snapshot.error_message = "getifaddrs failed";
    return false;
  }

  for( struct ifaddrs * current = interfaces;
       current != NULL;
       current = current->ifa_next )
  {
    if( current->ifa_addr == NULL || current->ifa_data == NULL ||
        current->ifa_addr->sa_family != AF_LINK )
    {
      continue;
    }

    struct if_data * stats =
      reinterpret_cast< struct if_data * >( current->ifa_data );
    NetworkInterfaceSample sample;
    bool is_up = ( current->ifa_flags & IFF_UP ) != 0;
    bool is_running = ( current->ifa_flags & IFF_RUNNING ) != 0;
    bool link_state_known = stats->ifi_link_state != LINK_STATE_UNKNOWN;
    bool link_state_up = stats->ifi_link_state == LINK_STATE_UP;
    sample.id = current->ifa_name;
    sample.name = current->ifa_name;
    sample.is_loopback = ( current->ifa_flags & IFF_LOOPBACK ) != 0;
    sample.is_active = network_interface_is_active_from_oper_state( is_up,
      is_running, link_state_known, link_state_up );
    sample.has_stats = true;
    sample.rx_bytes = stats->ifi_ibytes;
    sample.tx_bytes = stats->ifi_obytes;
    snapshot.interfaces.push_back( sample );
  }

  freeifaddrs( interfaces );
  return true;
}
