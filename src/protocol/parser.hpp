#pragma once

#include "../net/socket.hpp"
#include <vector>
#include <string>

namespace corekv {

void handleRead(Connection *conn);

void handleWrite(Connection *conn);

bool parseMessage(Connection *conn);

} // namespace corekv
