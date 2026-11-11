#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace corekv {

constexpr uint64_t kFnvPrime64 = 1099511628211ULL;
constexpr uint64_t kFnvOffsetBasis64 = 14695981039346656037ULL;

[[nodiscard]] inline uint64_t fnv1a_hash(const uint8_t *data, size_t len) {
  uint64_t h = kFnvOffsetBasis64;
  const auto *bytes = static_cast<const unsigned char *>(data);

  for (size_t i = 0; i < len; ++i) {
    h ^= bytes[i];
    h *= kFnvPrime64;
  }
  return h;
}

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
