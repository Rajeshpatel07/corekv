#include "resp.h"
#include "net/socket.h"
#include "parser/parser.h"
#include <charconv>
#include <cstring>
#include <netinet/in.h>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace corekv::RESP {
namespace {

template <typename Integer>
[[nodiscard]] Integer parse_number(std::string_view text, const char *what) {
  Integer value{};
  const char *first = text.data();
  const char *last = first + text.size();
  const auto [end, error] = std::from_chars(first, last, value);
  if (error != std::errc{} || end != last) {
    throw ProtocolError{what};
  }
  return value;
}

} // namespace

// Handle RESP bulk string
std::string decode_bulk_string(net::ClientObject &client) {

  client.read_offset += 1;
  if (client.read_offset >= client.read_buffer.size()) {
    throw IncompleteFrame{};
  }

  size_t crlf_pos = std::string::npos;
  for (size_t i = client.read_offset; i + 1 < client.read_buffer.size(); ++i) {
    if (client.read_buffer[i] == '\r' && client.read_buffer[i + 1] == '\n') {
      crlf_pos = i;
      break;
    }
  }

  if (crlf_pos == std::string::npos) {
    throw IncompleteFrame{};
  }

  size_t len = crlf_pos - client.read_offset;
  std::string int_len(len, '\0');
  parser::read_from_buffer(client.read_buffer,
                           reinterpret_cast<uint8_t *>(int_len.data()), len,
                           client.read_offset);

  client.read_offset += 2;

  const int length = parse_number<int>(
      int_len, "RESP bulk string length is not a valid number");
  if (length < 0) {
    throw ProtocolError("RESP bulk string length cannot be negative");
  }

  const size_t data_len = static_cast<size_t>(length);
  std::string response(data_len, '\0');

  parser::read_from_buffer(client.read_buffer,
                           reinterpret_cast<uint8_t *>(response.data()),
                           data_len, client.read_offset);

  if (client.read_offset + 1 >= client.read_buffer.size() ||
      client.read_buffer[client.read_offset] != '\r' ||
      client.read_buffer[client.read_offset + 1] != '\n') {
    throw IncompleteFrame{};
  }
  client.read_offset += 2;

  return response;
}

DataBuffer decode_array(net::ClientObject &client) {
  client.read_offset += 1; // Skip '*'
  if (client.read_offset >= client.read_buffer.size()) {
    throw IncompleteFrame{};
  }

  size_t crlf_pos = std::string::npos;
  for (size_t i{client.read_offset}; i + 1 < client.read_buffer.size(); ++i) {
    if (client.read_buffer[i] == '\r' && client.read_buffer[i + 1] == '\n') {
      crlf_pos = i;
      break;
    }
  }

  if (crlf_pos == std::string::npos) {
    throw IncompleteFrame{};
  }

  size_t len_str_size = crlf_pos - client.read_offset;
  std::string int_len(len_str_size, '\0');

  parser::read_from_buffer(client.read_buffer,
                           reinterpret_cast<uint8_t *>(int_len.data()),
                           len_str_size, client.read_offset);

  client.read_offset += 2;

  const int length =
      parse_number<int>(int_len, "RESP array length is not a valid number");
  if (length < 0) {
    throw ProtocolError("RESP array length cannot be negative");
  }
  const size_t array_len = static_cast<size_t>(length);

  DataBuffer arr;
  arr.reserve(array_len);

  for (size_t i{0}; i < array_len; ++i) {
    if (client.read_offset >= client.read_buffer.size()) {
      throw IncompleteFrame{};
    }

    const uint8_t type = client.read_buffer[client.read_offset];
    if (type != '$') {
      throw ProtocolError{std::string{"expected '$', got '"} +
                          static_cast<char>(type) + "'"};
    }

    arr.emplace_back(decode_bulk_string(client));
  }

  return arr;
}

void encode_array(net::ClientObject &client,
                  const std::vector<std::optional<std::string>> &values) {

  std::string prefix{"*" + std::to_string(values.size()) + "\r\n"};
  parser::append_to_buffer(reinterpret_cast<uint8_t *>(prefix.data()),
                           client.write_buffer, prefix.size());

  for (const auto &value : values) {
    if (value.has_value()) {
      encode_bulk_string(client, value.value());
    } else {
      encode_null_bulk_string(client);
    }
  }
}

void encode_simple_string(net::ClientObject &client, const std::string &data) {
  std::string str{"+" + data + "\r\n"};
  parser::append_to_buffer(reinterpret_cast<uint8_t *>(str.data()),
                           client.write_buffer, str.size());
}

void encode_bulk_string(net::ClientObject &client, const std::string &data) {
  std::string str{"$" + std::to_string(data.size()) + "\r\n" + data + "\r\n"};
  parser::append_to_buffer(reinterpret_cast<uint8_t *>(str.data()),
                           client.write_buffer, str.size());
}

void encode_null_bulk_string(net::ClientObject &client) {
  std::string str{"$-1\r\n"};
  parser::append_to_buffer(reinterpret_cast<uint8_t *>(str.data()),
                           client.write_buffer, str.size());
}

void encode_integer(net::ClientObject &client, int64_t num) {
  std::string str{":" + std::to_string(num) + "\r\n"};
  parser::append_to_buffer(reinterpret_cast<uint8_t *>(str.data()),
                           client.write_buffer, str.size());
}

void encode_simple_error(net::ClientObject &client,
                         const std::string &message) {
  std::string str{"-" + message + "\r\n"};
  parser::append_to_buffer(reinterpret_cast<uint8_t *>(str.data()),
                           client.write_buffer, str.size());
}

void encode_bulk_error(net::ClientObject &client, const std::string &message) {
  std::string str{"!" + std::to_string(message.size()) + "\r\n" + message +
                  "\r\n"};
  parser::append_to_buffer(reinterpret_cast<uint8_t *>(str.data()),
                           client.write_buffer, str.size());
}

} // namespace corekv::RESP
