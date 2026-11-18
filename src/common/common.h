#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace corekv {

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

} // namespace corekv
