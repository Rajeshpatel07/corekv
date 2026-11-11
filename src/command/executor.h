#pragma once

#include "common/common.h"
#include "net/socket.h"
namespace corekv::executor {

// Routes one decoded RESP command to its handler and queues the reply in
// client.write_buffer.
void execute_command(net::ClientObject &client, RespValue &command);

} // namespace corekv::executor
