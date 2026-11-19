#pragma once

#include "net/socket.h"
#include "parser/resp.h"
namespace corekv::executor {

// Routes one decoded RESP command to its handler and queues the reply in
// client.write_buffer.
void execute_command(net::ClientObject &client, const RESP::DataBuffer &args);

} // namespace corekv::executor
