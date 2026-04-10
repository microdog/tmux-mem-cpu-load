#include <cstdlib>
#include <iostream>
#include <vector>

#include "network.h"

void reset_network_reader_stub();
int network_reader_stub_call_count();
void network_reader_stub_set_snapshots(
  const std::vector< NetworkSnapshot > & snapshots );
void network_reader_stub_set_failure( int call_number,
  const std::string & error_message );

static void require( bool condition, const char * message )
{
  if( !condition )
  {
    std::cerr << "network_logic_test failure: " << message << "\n";
    std::exit( 1 );
  }
}

static NetworkInterfaceSample make_sample( const std::string & name,
  bool is_loopback,
  bool is_active,
  unsigned long long rx_bytes,
  unsigned long long tx_bytes )
{
  NetworkInterfaceSample sample;
  sample.id = name;
  sample.name = name;
  sample.is_loopback = is_loopback;
  sample.is_active = is_active;
  sample.has_stats = true;
  sample.rx_bytes = rx_bytes;
  sample.tx_bytes = tx_bytes;
  return sample;
}

static NetworkInterfaceSample make_sample_with_id( const std::string & id,
  const std::string & name,
  bool is_loopback,
  bool is_active,
  unsigned long long rx_bytes,
  unsigned long long tx_bytes )
{
  NetworkInterfaceSample sample = make_sample( name, is_loopback, is_active,
    rx_bytes, tx_bytes );
  sample.id = id;
  return sample;
}

static NetworkSnapshot make_snapshot(
  const std::vector< NetworkInterfaceSample > & interfaces )
{
  NetworkSnapshot snapshot;
  snapshot.ok = true;
  snapshot.error_message.clear();
  snapshot.interfaces = interfaces;
  return snapshot;
}

static void test_parse_network_selectors_splits_commas_and_deduplicates()
{
  std::vector< std::string > arguments;
  arguments.push_back( "en0,utun2" );
  arguments.push_back( "utun2" );
  arguments.push_back( "en0" );

  std::vector< std::string > selectors = parse_network_selectors( arguments );

  require( selectors.size() == 2, "selectors size should be 2" );
  require( selectors[0] == "en0", "first selector should be en0" );
  require( selectors[1] == "utun2", "second selector should be utun2" );
}

static void test_make_hidden_network_status_returns_hidden_defaults()
{
  NetworkStatus status = make_hidden_network_status();

  require( status.state == NETWORK_STATE_HIDDEN,
    "hidden status should have hidden state" );
  require( status.rx_bytes == 0, "hidden status should zero rx_bytes" );
  require( status.tx_bytes == 0, "hidden status should zero tx_bytes" );
  require( status.rx_rate == 0.0f, "hidden status should zero rx_rate" );
  require( status.tx_rate == 0.0f, "hidden status should zero tx_rate" );
  require( status.error_message.empty(),
    "hidden status should have empty error_message" );
}

static void test_format_guid_string_formats_canonical_guid_text()
{
  const unsigned char data4[8] =
    { 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78 };

  std::string guid = format_guid_string( 0x12345678, 0x9abc, 0xdef0, data4 );

  require( guid == "12345678-9abc-def0-9abc-def012345678",
    "guid string should use canonical lowercase text" );
}

static void test_build_network_status_aggregates_active_non_loopback_interfaces()
{
  NetworkOptions options;
  options.mode = NETWORK_MODE_DEFAULT;
  options.sample_delay_us = 1000000;

  NetworkSnapshot before = make_snapshot( std::vector< NetworkInterfaceSample >{
    make_sample( "en0", false, true, 1000, 500 ),
    make_sample( "utun2", false, true, 4000, 2000 ),
    make_sample( "lo0", true, true, 9000, 9000 ),
    make_sample( "bridge0", false, false, 500, 500 )
  } );
  NetworkSnapshot after = make_snapshot( std::vector< NetworkInterfaceSample >{
    make_sample( "en0", false, true, 3000, 1500 ),
    make_sample( "utun2", false, true, 7000, 2600 ),
    make_sample( "lo0", true, true, 9500, 9500 ),
    make_sample( "bridge0", false, false, 900, 900 )
  } );

  NetworkStatus status = build_network_status( before, after, options );

  require( status.state == NETWORK_STATE_OK,
    "aggregate status should be ok" );
  require( status.rx_bytes == 10000,
    "aggregate status should sum rx bytes" );
  require( status.tx_bytes == 4100,
    "aggregate status should sum tx bytes" );
  require( status.rx_rate == 5000.0f,
    "aggregate status should compute rx rate" );
  require( status.tx_rate == 1600.0f,
    "aggregate status should compute tx rate" );
}

static void test_build_network_status_hides_when_explicit_interfaces_are_absent()
{
  NetworkOptions options;
  options.mode = NETWORK_MODE_DEFAULT;
  options.sample_delay_us = 1000000;
  options.interface_names = std::vector< std::string >( 1, "en9" );

  NetworkSnapshot before = make_snapshot( std::vector< NetworkInterfaceSample >{
    make_sample( "en0", false, true, 1000, 500 )
  } );
  NetworkSnapshot after = make_snapshot( std::vector< NetworkInterfaceSample >{
    make_sample( "en0", false, true, 1200, 700 )
  } );

  NetworkStatus status = build_network_status( before, after, options );

  require( status.state == NETWORK_STATE_HIDDEN,
    "explicitly missing interfaces should hide status" );
}

static void test_build_network_status_matches_explicit_interface_ids()
{
  NetworkOptions options;
  options.mode = NETWORK_MODE_DEFAULT;
  options.sample_delay_us = 1000000;
  options.interface_names = std::vector< std::string >( 1, "42" );

  NetworkSnapshot before = make_snapshot( std::vector< NetworkInterfaceSample >{
    make_sample_with_id( "42", "Ethernet", false, true, 1000, 500 )
  } );
  NetworkSnapshot after = make_snapshot( std::vector< NetworkInterfaceSample >{
    make_sample_with_id( "42", "Ethernet", false, true, 3000, 1500 )
  } );

  NetworkStatus status = build_network_status( before, after, options );

  require( status.state == NETWORK_STATE_OK,
    "explicit interface ids should match selected interfaces" );
  require( status.rx_rate == 2000.0f,
    "explicit interface id match should use selected counters" );
  require( status.tx_rate == 1000.0f,
    "explicit interface id match should use selected counters" );
}

static void test_build_network_status_hides_when_default_selection_has_no_eligible_interfaces()
{
  NetworkOptions options;
  options.mode = NETWORK_MODE_DEFAULT;
  options.sample_delay_us = 1000000;

  NetworkSnapshot before = make_snapshot( std::vector< NetworkInterfaceSample >{
    make_sample( "lo0", true, true, 1000, 1000 ),
    make_sample( "bridge0", false, false, 2000, 2000 )
  } );
  NetworkSnapshot after = make_snapshot( std::vector< NetworkInterfaceSample >{
    make_sample( "lo0", true, true, 1500, 1500 ),
    make_sample( "bridge0", false, false, 2600, 2600 )
  } );

  NetworkStatus status = build_network_status( before, after, options );

  require( status.state == NETWORK_STATE_HIDDEN,
    "default selection should hide when no eligible interface remains" );
}

static void test_net_string_renders_error_marker()
{
  NetworkStatus status;
  status.state = NETWORK_STATE_ERROR;
  status.rx_bytes = 0;
  status.tx_bytes = 0;
  status.rx_rate = 0.0f;
  status.tx_rate = 0.0f;
  status.error_message = "collector failure";

  std::string rendered = net_string( status, NETWORK_MODE_DEFAULT,
    false, false, false );

  require( rendered.find( "!net" ) != std::string::npos,
    "error status should render !net marker" );
}

static void test_net_string_keeps_legacy_default_status_visible()
{
  NetworkStatus status;

  std::string rendered = net_string( status, NETWORK_MODE_DEFAULT,
    false, false, false );

  require( !rendered.empty(),
    "legacy default status should still render while old sampler is active" );
}

static void test_sample_network_status_skips_reader_when_mode_is_off()
{
  reset_network_reader_stub();

  NetworkOptions options;
  options.mode = NETWORK_MODE_OFF;
  options.sample_delay_us = 1;

  NetworkStatus status = sample_network_status( options );

  require( status.state == NETWORK_STATE_HIDDEN,
    "off mode should return hidden status" );
  require( network_reader_stub_call_count() == 0,
    "off mode should not read snapshots" );
}

static void test_sample_network_status_renders_error_when_reader_fails()
{
  reset_network_reader_stub();
  network_reader_stub_set_failure( 2, "collector failure" );

  NetworkOptions options;
  options.mode = NETWORK_MODE_DEFAULT;
  options.sample_delay_us = 0;

  NetworkStatus status = sample_network_status( options );
  std::string rendered = net_string( status, NETWORK_MODE_DEFAULT,
    false, false, false );

  require( status.state == NETWORK_STATE_ERROR,
    "reader failures should return error status" );
  require( network_reader_stub_call_count() == 2,
    "sampler should attempt both reads before surfacing second-read errors" );
  require( rendered.find( "!net" ) != std::string::npos,
    "reader failures should render the unavailable marker" );
}

static void test_sample_network_status_aggregates_successful_reader_snapshots()
{
  reset_network_reader_stub();

  network_reader_stub_set_snapshots( std::vector< NetworkSnapshot >{
    make_snapshot( std::vector< NetworkInterfaceSample >{
      make_sample( "en0", false, true, 1000, 500 ),
      make_sample( "utun2", false, true, 4000, 2000 ),
      make_sample( "lo0", true, true, 9000, 9000 ),
      make_sample( "bridge0", false, false, 500, 500 )
    } ),
    make_snapshot( std::vector< NetworkInterfaceSample >{
      make_sample( "en0", false, true, 3000, 1500 ),
      make_sample( "utun2", false, true, 7000, 2600 ),
      make_sample( "lo0", true, true, 9500, 9500 ),
      make_sample( "bridge0", false, false, 900, 900 )
    } )
  } );

  NetworkOptions options;
  options.mode = NETWORK_MODE_DEFAULT;
  options.sample_delay_us = 0;

  NetworkStatus status = sample_network_status( options );

  require( status.state == NETWORK_STATE_OK,
    "successful reader snapshots should aggregate into ok status" );
  require( status.rx_bytes == 10000,
    "successful reader snapshots should sum rx bytes" );
  require( status.tx_bytes == 4100,
    "successful reader snapshots should sum tx bytes" );
  require( status.rx_rate == 5000.0f,
    "successful reader snapshots should compute rx rate" );
  require( status.tx_rate == 1600.0f,
    "successful reader snapshots should compute tx rate" );
  require( network_reader_stub_call_count() == 2,
    "successful sampling should read exactly two snapshots" );
}

static void test_sample_network_status_hides_when_reader_has_no_eligible_interfaces()
{
  reset_network_reader_stub();

  network_reader_stub_set_snapshots( std::vector< NetworkSnapshot >{
    make_snapshot( std::vector< NetworkInterfaceSample >{
      make_sample( "lo0", true, true, 1000, 1000 ),
      make_sample( "bridge0", false, false, 2000, 2000 )
    } ),
    make_snapshot( std::vector< NetworkInterfaceSample >{
      make_sample( "lo0", true, true, 1500, 1500 ),
      make_sample( "bridge0", false, false, 2600, 2600 )
    } )
  } );

  NetworkOptions options;
  options.mode = NETWORK_MODE_DEFAULT;
  options.sample_delay_us = 0;

  NetworkStatus status = sample_network_status( options );

  require( status.state == NETWORK_STATE_HIDDEN,
    "successful reader snapshots should hide when no interface is eligible" );
  require( network_reader_stub_call_count() == 2,
    "hidden successful sampling should still read both snapshots" );
}

static void test_network_interface_is_active_from_oper_state_requires_up_state()
{
  require( !network_interface_is_active_from_oper_state( false, false,
      true, true ),
    "oper-up alone should not mark an interface active" );
  require( !network_interface_is_active_from_oper_state( true, false,
      true, false ),
    "admin-up alone should not mark an interface active" );
  require( network_interface_is_active_from_oper_state( true, false,
      true, true ),
    "interfaces should be active only when admin-up and operationally up" );
}

static void test_network_interface_is_active_from_oper_state_uses_running_fallback()
{
  require( !network_interface_is_active_from_oper_state( false, false,
      false, false ),
    "admin-down interfaces should never be active" );
  require( !network_interface_is_active_from_oper_state( true, false,
      false, false ),
    "oper-unknown interfaces without running state should be inactive" );
  require( network_interface_is_active_from_oper_state( true, true,
      false, false ),
    "oper-unknown interfaces should fall back to running state" );
  require( !network_interface_is_active_from_oper_state( true, true,
      true, false ),
    "known down state should override running state" );
  require( network_interface_is_active_from_oper_state( true, false,
      true, true ),
    "known up state should mark an interface active" );
}

int main()
{
  test_parse_network_selectors_splits_commas_and_deduplicates();
  test_make_hidden_network_status_returns_hidden_defaults();
  test_format_guid_string_formats_canonical_guid_text();
  test_build_network_status_aggregates_active_non_loopback_interfaces();
  test_build_network_status_hides_when_explicit_interfaces_are_absent();
  test_build_network_status_matches_explicit_interface_ids();
  test_build_network_status_hides_when_default_selection_has_no_eligible_interfaces();
  test_net_string_renders_error_marker();
  test_net_string_keeps_legacy_default_status_visible();
  test_sample_network_status_skips_reader_when_mode_is_off();
  test_sample_network_status_renders_error_when_reader_fails();
  test_sample_network_status_aggregates_successful_reader_snapshots();
  test_sample_network_status_hides_when_reader_has_no_eligible_interfaces();
  test_network_interface_is_active_from_oper_state_requires_up_state();
  test_network_interface_is_active_from_oper_state_uses_running_fallback();
  std::cout << "network_logic_test: ok\n";
  return 0;
}
