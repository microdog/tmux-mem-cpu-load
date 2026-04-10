#ifndef LINUX_NETWORK_READER_H_
#define LINUX_NETWORK_READER_H_

#include <istream>
#include <string>

#include "network.h"

struct LinuxInterfaceState
{
  LinuxInterfaceState()
    : is_up( false ),
      is_running( false ),
      oper_state_known( false ),
      oper_state_up( false )
  {
  }

  bool is_up;
  bool is_running;
  bool oper_state_known;
  bool oper_state_up;
};

typedef bool ( *LinuxInterfaceStateReader )(
  const std::string & interface_name,
  LinuxInterfaceState & state,
  std::string & error_message );

bool read_linux_network_snapshot( std::istream & input,
  NetworkSnapshot & snapshot,
  std::string & error_message );
bool read_linux_network_snapshot( std::istream & input,
  NetworkSnapshot & snapshot,
  std::string & error_message,
  LinuxInterfaceStateReader state_reader );

#endif
