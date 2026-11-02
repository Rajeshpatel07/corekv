#pragma once

#include "net/socket.h"
#include <memory>
#include <string>
#include <variant>
namespace corekv::RESP {

struct RespValue;

using RespArray = std::vector<RespValue>;

struct RespValue {
  std::variant<std::string, int64_t, std::unique_ptr<RespArray>> data;

  bool is_string() const { return std::holds_alternative<std::string>(data); }
  bool is_int() const { return std::holds_alternative<int64_t>(data); }
  bool is_array() const {
    return std::holds_alternative<std::unique_ptr<RespArray>>(data);
  }

  const std::string &get_string() const { return std::get<std::string>(data); }
  int64_t get_int() const { return std::get<int64_t>(data); }
  const RespArray &get_array() const {
    return *std::get<std::unique_ptr<RespArray>>(data);
  }
};

void print_resp_value(const RespValue &val, size_t indent = 0);
std::string decode_simple_string(net::ClientObject &client);
std::string decode_bulk_string(net::ClientObject &client);
int decode_integer(net::ClientObject &client);
int64_t decode_big_numbers(net::ClientObject &client);
RespValue decode_array(net::ClientObject &client);

// WARN: Need performance Improvements
void encode_simple_string(net::ClientObject &client, std::string &data);
void encode_bulk_string(net::ClientObject &client, std::string &data);
void encode_integer(net::ClientObject &client, int32_t num);
void encode_big_number(net::ClientObject &client, int64_t num);

} // namespace corekv::RESP
