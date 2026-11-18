#pragma once

#include "net/socket.h"
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <variant>

namespace corekv::RESP {

struct RespValue;

using RespArray = std::vector<RespValue>;

struct RespValue {
  RespValue() = default;
  ~RespValue() = default;
  RespValue(const RespValue &) = delete;
  RespValue &operator=(const RespValue &) = delete;
  RespValue(RespValue &&) = default;
  RespValue &operator=(RespValue &&) = default;

  std::variant<std::string, int64_t, std::unique_ptr<RespArray>> data;

  bool is_string() const { return std::holds_alternative<std::string>(data); }
  bool is_int() const { return std::holds_alternative<int64_t>(data); }
  bool is_array() const {
    return std::holds_alternative<std::unique_ptr<RespArray>>(data);
  }

  const std::string &get_string() const { return std::get<std::string>(data); }
  int64_t get_int() const { return std::get<int64_t>(data); }
  RespArray &get_array() { return *std::get<std::unique_ptr<RespArray>>(data); }
  const RespArray &get_array() const {
    return *std::get<std::unique_ptr<RespArray>>(data);
  }
};

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
void encode_array(net::ClientObject &client, const RespArray &value);
void encode_simple_string(net::ClientObject &client, const std::string &data);
void encode_bulk_string(net::ClientObject &client, const std::string &data);
void encode_null_bulk_string(net::ClientObject &client);
void encode_integer(net::ClientObject &client, int64_t num);
void encode_simple_error(net::ClientObject &client, const std::string &message);
void encode_bulk_error(net::ClientObject &client, const std::string &message);

} // namespace corekv::RESP
