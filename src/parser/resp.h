#pragma once

#include "net/socket.h"
#include <string>
namespace corekv::RESP {

std::string decode_simple_string(net::ClientObject &client);
std::string decode_bulk_string(net::ClientObject &client);
int decode_integer(net::ClientObject &client);

// NOTE: implement all the function below
// std::string decode_simple_error(net::ClientObject &client);
// std::vector<std::string> decode_array(net::ClientObject &client);

} // namespace corekv::RESP
