#include <cstdlib>
#include <iostream>
#include <sstream>

#include "network.h"
#include "network_reader.h"

static void require( bool condition, const char * message )
{
  if( !condition )
  {
    std::cerr << "linux_network_reader_test failure: " << message << "\n";
    std::exit( 1 );
  }
}

static bool read_unknown_operstate_but_running_state(
  const std::string & interface_name,
  LinuxInterfaceState & state,
  std::string & error_message )
{
  error_message.clear();
  state.is_up = true;
  state.is_running = true;
  state.oper_state_known = false;
  state.oper_state_up = false;

  if( interface_name == "lo" )
  {
    state.is_running = true;
  }

  return true;
}

static bool fail_linux_interface_state_reader(
  const std::string & interface_name,
  LinuxInterfaceState & state,
  std::string & error_message )
{
  (void)interface_name;
  state = LinuxInterfaceState();
  error_message = "failed to query interface flags";
  return false;
}

static bool fail_linux_bridge_state_reader(
  const std::string & interface_name,
  LinuxInterfaceState & state,
  std::string & error_message )
{
  if( interface_name == "bridge0" )
  {
    state = LinuxInterfaceState();
    error_message = "failed to query interface flags";
    return false;
  }

  return read_unknown_operstate_but_running_state( interface_name, state,
    error_message );
}

static void test_read_linux_network_snapshot_parses_proc_net_dev()
{
  std::istringstream input(
    "Inter-|   Receive                                                |  Transmit\n"
    " face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed\n"
    "  eth0: 100 1 0 0 0 0 0 0 200 2 0 0 0 0 0 0\n"
    "    lo: 50 1 0 0 0 0 0 0 50 1 0 0 0 0 0 0\n"
  );

  NetworkSnapshot snapshot;
  std::string error_message;
  bool ok = read_linux_network_snapshot( input, snapshot, error_message,
    read_unknown_operstate_but_running_state );

  require( ok, "happy-path parse should succeed" );
  require( snapshot.ok, "happy-path snapshot should be marked ok" );
  require( snapshot.interfaces.size() == 2,
    "happy-path parse should keep both interfaces" );
  require( snapshot.interfaces[0].name == "eth0",
    "first parsed interface should be eth0" );
  require( snapshot.interfaces[0].rx_bytes == 100,
    "eth0 rx bytes should parse" );
  require( snapshot.interfaces[0].tx_bytes == 200,
    "eth0 tx bytes should parse" );
  require( !snapshot.interfaces[0].is_loopback,
    "eth0 should not be loopback" );
  require( snapshot.interfaces[1].is_loopback,
    "lo should be loopback" );
}

static void test_read_linux_network_snapshot_uses_running_fallback_when_operstate_is_unknown()
{
  std::istringstream input(
    "Inter-|   Receive                                                |  Transmit\n"
    " face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed\n"
    "  eth0: 100 1 0 0 0 0 0 0 200 2 0 0 0 0 0 0\n"
  );

  NetworkSnapshot snapshot;
  std::string error_message;
  bool ok = read_linux_network_snapshot( input, snapshot, error_message,
    read_unknown_operstate_but_running_state );

  require( ok, "unknown operstate with running flags should still succeed" );
  require( snapshot.ok,
    "unknown operstate with running flags should keep snapshot ok" );
  require( snapshot.interfaces.size() == 1,
    "fallback parse should keep the interface" );
  require( snapshot.interfaces[0].is_active,
    "running flags should keep interface active when operstate is unavailable" );
}

static void test_read_linux_network_snapshot_rejects_missing_headers()
{
  std::istringstream input(
    "  eth0: 100 1 0 0 0 0 0 0 200 2 0 0 0 0 0 0\n"
  );

  NetworkSnapshot snapshot;
  std::string error_message;
  bool ok = read_linux_network_snapshot( input, snapshot, error_message );

  require( !ok, "missing headers should fail parsing" );
  require( !snapshot.ok, "missing headers should mark snapshot not ok" );
  require( !error_message.empty(),
    "missing headers should report an error message" );
}

static void test_read_linux_network_snapshot_rejects_malformed_body()
{
  std::istringstream input(
    "Inter-|   Receive                                                |  Transmit\n"
    " face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed\n"
    "  eth0 100 1 0 0 0 0 0 0 200 2 0 0 0 0 0 0\n"
  );

  NetworkSnapshot snapshot;
  std::string error_message;
  bool ok = read_linux_network_snapshot( input, snapshot, error_message );

  require( !ok, "malformed body should fail parsing" );
  require( !snapshot.ok, "malformed body should mark snapshot not ok" );
  require( !error_message.empty(),
    "malformed body should report an error message" );
}

static void test_read_linux_network_snapshot_fails_when_interface_state_lookup_fails()
{
  std::istringstream input(
    "Inter-|   Receive                                                |  Transmit\n"
    " face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed\n"
    "  eth0: 100 1 0 0 0 0 0 0 200 2 0 0 0 0 0 0\n"
  );

  NetworkSnapshot snapshot;
  std::string error_message;
  bool ok = read_linux_network_snapshot( input, snapshot, error_message,
    fail_linux_interface_state_reader );

  require( !ok, "state-reader failures should fail parsing" );
  require( !snapshot.ok, "state-reader failures should mark snapshot not ok" );
  require( error_message == "failed to query interface flags",
    "state-reader failures should preserve the lookup error" );
}

static void test_read_linux_network_snapshot_keeps_other_interfaces_when_one_state_lookup_fails()
{
  std::istringstream input(
    "Inter-|   Receive                                                |  Transmit\n"
    " face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed\n"
    "  eth0: 100 1 0 0 0 0 0 0 200 2 0 0 0 0 0 0\n"
    " bridge0: 50 1 0 0 0 0 0 0 50 1 0 0 0 0 0 0\n"
  );

  NetworkSnapshot snapshot;
  std::string error_message;
  bool ok = read_linux_network_snapshot( input, snapshot, error_message,
    fail_linux_bridge_state_reader );

  require( ok, "one interface state failure should not fail the whole snapshot" );
  require( snapshot.ok,
    "one interface state failure should still leave snapshot ok" );
  require( error_message.empty(),
    "one interface state failure should not promote a collector error" );
  require( snapshot.interfaces.size() == 2,
    "other interfaces should still be preserved" );
  require( snapshot.interfaces[0].is_active,
    "healthy interfaces should keep their active state" );
  require( snapshot.interfaces[1].name == "bridge0",
    "the failed interface should still be present for explicit selection" );
  require( !snapshot.interfaces[1].is_active,
    "failed interface state lookups should degrade to inactive for default aggregation" );
}

int main()
{
  test_read_linux_network_snapshot_parses_proc_net_dev();
  test_read_linux_network_snapshot_uses_running_fallback_when_operstate_is_unknown();
  test_read_linux_network_snapshot_rejects_missing_headers();
  test_read_linux_network_snapshot_rejects_malformed_body();
  test_read_linux_network_snapshot_fails_when_interface_state_lookup_fails();
  test_read_linux_network_snapshot_keeps_other_interfaces_when_one_state_lookup_fails();
  std::cout << "linux_network_reader_test: ok\n";
  return 0;
}
