#include <string>
#include <vector>

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#ifndef WINVER
#define WINVER _WIN32_WINNT
#endif

#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>

#include "network.h"

static bool fail_windows_network_snapshot( NetworkSnapshot & snapshot,
  const std::string & message )
{
  snapshot.ok = false;
  snapshot.error_message = message;
  snapshot.interfaces.clear();
  return false;
}

static std::string wide_to_utf8( const wchar_t * value )
{
  if( value == NULL || *value == L'\0' )
  {
    return std::string();
  }

  int size = WideCharToMultiByte(
    CP_UTF8, 0, value, -1, NULL, 0, NULL, NULL );
  if( size <= 1 )
  {
    return std::string();
  }

  std::vector< char > buffer( size );
  if( WideCharToMultiByte(
        CP_UTF8, 0, value, -1, &buffer[0], size, NULL, NULL ) != size )
  {
    return std::string();
  }

  return std::string( &buffer[0] );
}

static std::string get_windows_interface_name( const MIB_IF_ROW2 & row )
{
  std::string name = wide_to_utf8( row.Alias );
  if( !name.empty() )
  {
    return name;
  }

  name = wide_to_utf8( row.Description );
  if( !name.empty() )
  {
    return name;
  }

  return std::to_string(
    static_cast< unsigned long long >( row.InterfaceIndex ) );
}

bool read_network_snapshot( NetworkSnapshot & snapshot )
{
  snapshot.ok = true;
  snapshot.error_message.clear();
  snapshot.interfaces.clear();

  PMIB_IF_TABLE2 table = NULL;
  DWORD result = GetIfTable2( &table );
  if( result != NO_ERROR )
  {
    return fail_windows_network_snapshot( snapshot,
      "GetIfTable2 failed: " + std::to_string( result ) );
  }

  for( ULONG index = 0; index < table->NumEntries; ++index )
  {
    const MIB_IF_ROW2 & row = table->Table[index];

    NetworkInterfaceSample sample;
    sample.id = format_guid_string( row.InterfaceGuid.Data1,
      row.InterfaceGuid.Data2, row.InterfaceGuid.Data3,
      row.InterfaceGuid.Data4 );
    sample.name = get_windows_interface_name( row );
    sample.is_loopback = ( row.Type == IF_TYPE_SOFTWARE_LOOPBACK );
    sample.is_active = ( row.OperStatus == IfOperStatusUp );
    sample.has_stats = true;
    sample.rx_bytes = row.InOctets;
    sample.tx_bytes = row.OutOctets;
    snapshot.interfaces.push_back( sample );
  }

  if( table != NULL )
  {
    FreeMibTable( table );
  }

  return true;
}
