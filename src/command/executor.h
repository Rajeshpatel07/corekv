#pragma once

#include "net/socket.h"
#include "parser/resp.h"

namespace corekv::executor {
enum CMDS { GET, SET, DEL };

// Routes one decoded RESP command to its handler and queues the reply in
// client.write_buffer.
void execution_handler(net::ClientObject &client,
                       const RESP::RespValue &command);

} // namespace corekv::executor
