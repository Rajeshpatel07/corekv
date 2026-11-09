#pragma once

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

std::string decode_simple_string(net::ClientObject &client);
std::string decode_bulk_string(net::ClientObject &client);
int decode_integer(net::ClientObject &client);
int64_t decode_big_numbers(net::ClientObject &client);
RespValue decode_array(net::ClientObject &client);

// WARN: Need performance Improvements
void encode_simple_string(net::ClientObject &client, const std::string &data);
void encode_bulk_string(net::ClientObject &client, const std::string &data);
void encode_null_bulk_string(net::ClientObject &client);
void encode_integer(net::ClientObject &client, int32_t num);
void encode_big_number(net::ClientObject &client, int64_t num);
void encode_simple_error(net::ClientObject &client, const std::string &message);
void encode_bulk_error(net::ClientObject &client, const std::string &message);

} // namespace corekv::RESP
