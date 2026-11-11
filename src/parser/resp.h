#pragma once

#include "common/common.h"
#include "net/socket.h"
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <variant>

namespace corekv::RESP {

class IncompleteFrame : public std::exception {
public:
  [[nodiscard]] const char *what() const noexcept override {
    return "incomplete RESP frame";
  }
};

class ProtocolError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

std::string decode_simple_string(net::ClientObject &client);
std::string decode_bulk_string(net::ClientObject &client);
int64_t decode_integer(net::ClientObject &client);
RespValue decode_array(net::ClientObject &client);

// WARN: Need performance Improvements
void encode_handler(net::ClientObject &client, const RespValue &value);
void encode_array(net::ClientObject &client, const RespArray &value);
void encode_simple_string(net::ClientObject &client, const std::string &data);
void encode_bulk_string(net::ClientObject &client, const std::string &data);
void encode_null_bulk_string(net::ClientObject &client);
void encode_integer(net::ClientObject &client, int64_t num);
void encode_simple_error(net::ClientObject &client, const std::string &message);
void encode_bulk_error(net::ClientObject &client, const std::string &message);

} // namespace corekv::RESP
