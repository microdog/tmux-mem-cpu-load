#include <cstdio>
#include <cstring>
#include <net/if.h>
#include <net/if_media.h>
#include <ifaddrs.h>
#include <net/if_dl.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "network.h"

static void read_interface_media_state( int socket_fd,
  const char * interface_name,
  bool & media_state_known,
  bool & media_state_up )
{
  media_state_known = false;
  media_state_up = false;

  if( socket_fd < 0 )
  {
    return;
  }

  struct ifmediareq request;
  std::memset( &request, 0, sizeof( request ) );
  std::snprintf( request.ifm_name, sizeof( request.ifm_name ), "%s",
    interface_name );

  if( ioctl( socket_fd, SIOCGIFMEDIA, &request ) != 0 )
  {
    return;
  }

  if( ( request.ifm_status & IFM_AVALID ) == 0 )
  {
    return;
  }

  media_state_known = true;
  media_state_up = ( request.ifm_status & IFM_ACTIVE ) != 0;
}

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

  int media_socket = socket( AF_INET, SOCK_DGRAM, 0 );

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
    bool media_state_known = false;
    bool media_state_up = false;
    bool is_up = ( current->ifa_flags & IFF_UP ) != 0;
    bool is_running = ( current->ifa_flags & IFF_RUNNING ) != 0;

    read_interface_media_state( media_socket, current->ifa_name,
      media_state_known, media_state_up );
    sample.id = current->ifa_name;
    sample.name = current->ifa_name;
    sample.is_loopback = ( current->ifa_flags & IFF_LOOPBACK ) != 0;
    sample.is_active = network_interface_is_active_from_oper_state( is_up,
      is_running, media_state_known, media_state_up );
    sample.has_stats = true;
    sample.rx_bytes = stats->ifi_ibytes;
    sample.tx_bytes = stats->ifi_obytes;
    snapshot.interfaces.push_back( sample );
  }

  if( media_socket >= 0 )
  {
    close( media_socket );
  }

  freeifaddrs( interfaces );
  return true;
}
