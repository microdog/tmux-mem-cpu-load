#include "network.h"

#include <string>
#include <vector>

static int g_read_network_snapshot_call_count = 0;
static int g_read_network_snapshot_failure_call = 0;
static std::string g_read_network_snapshot_failure_message;
static std::vector< NetworkSnapshot > g_read_network_snapshot_results;
static std::size_t g_read_network_snapshot_result_index = 0;

void reset_network_reader_stub()
{
  g_read_network_snapshot_call_count = 0;
  g_read_network_snapshot_failure_call = 0;
  g_read_network_snapshot_failure_message.clear();
  g_read_network_snapshot_results.clear();
  g_read_network_snapshot_result_index = 0;
}

int network_reader_stub_call_count()
{
  return g_read_network_snapshot_call_count;
}

void network_reader_stub_set_failure( int call_number,
  const std::string & error_message )
{
  g_read_network_snapshot_failure_call = call_number;
  g_read_network_snapshot_failure_message = error_message;
}

void network_reader_stub_set_snapshots(
  const std::vector< NetworkSnapshot > & snapshots )
{
  g_read_network_snapshot_results = snapshots;
  g_read_network_snapshot_result_index = 0;
}

bool read_network_snapshot( NetworkSnapshot & snapshot )
{
  ++g_read_network_snapshot_call_count;
  if( g_read_network_snapshot_call_count == g_read_network_snapshot_failure_call )
  {
    snapshot.ok = false;
    snapshot.error_message = g_read_network_snapshot_failure_message;
    snapshot.interfaces.clear();
    return false;
  }

  if( g_read_network_snapshot_result_index <
      g_read_network_snapshot_results.size() )
  {
    snapshot = g_read_network_snapshot_results[
      g_read_network_snapshot_result_index ];
    ++g_read_network_snapshot_result_index;
    return snapshot.ok;
  }

  snapshot.ok = true;
  snapshot.error_message.clear();
  snapshot.interfaces.clear();
  return true;
}
