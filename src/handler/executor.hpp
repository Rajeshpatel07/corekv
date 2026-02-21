#pragma once

#include "../net/socket.hpp"
#include <string>
#include <vector>

namespace corekv {

void executeCommand(std::vector<std::string> &cmds, Connection *conn);

} // namespace corekv
