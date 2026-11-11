#include "resp.h"
#include "net/socket.h"
#include "parser/parser.h"
#include <charconv>
#include <cstring>
#include <netinet/in.h>
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

// Handle RESP simple string
std::string decode_simple_string(net::ClientObject &client) {
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

  size_t data_len = crlf_pos - (client.read_offset);

  std::string response(data_len, '\0');

  parser::read_from_buffer(client.read_buffer,
                           reinterpret_cast<uint8_t *>(response.data()),
                           data_len, client.read_offset);

  client.read_offset += 2;

  return response;
}

// Handle RESP integer
int64_t decode_integer(net::ClientObject &client) {

  client.read_offset += 1;
  if (client.read_offset >= client.read_buffer.size()) {
    throw IncompleteFrame{};
  }

  bool is_negative{client.read_buffer[client.read_offset] == '-'};
  if (is_negative) {
    client.read_offset += 1;
    if (client.read_offset >= client.read_buffer.size()) {
      throw IncompleteFrame{};
    }
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
  std::string payload(len, '\0');
  parser::read_from_buffer(client.read_buffer,
                           reinterpret_cast<uint8_t *>(payload.data()), len,
                           client.read_offset);

  client.read_offset += 2;

  int64_t response =
      parse_number<int64_t>(payload, "RESP integer is not a valid number");

  if (is_negative) {
    response *= -1;
  }

  return response;
}

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

RespValue decode_array(net::ClientObject &client) {
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

  auto arr = std::make_unique<RespArray>();
  arr->reserve(array_len);

  for (size_t i{0}; i < array_len; ++i) {
    if (client.read_offset >= client.read_buffer.size()) {
      throw IncompleteFrame{};
    }

    const uint8_t type = client.read_buffer[client.read_offset];
    RespValue element;

    switch (type) {
    case '+':
      element.data = decode_simple_string(client);
      break;
    case '$':
      element.data = decode_bulk_string(client);
      break;
    case ':':
      element.data = decode_integer(client);
      break;
    case '*': {
      RespValue nested_arr = decode_array(client);
      element.data =
          std::move(std::get<std::unique_ptr<RespArray>>(nested_arr.data));
      break;
    }
    default:
      throw ProtocolError("RESP array contains an unknown element type");
    }

    arr->push_back(std::move(element));
  }

  RespValue result;
  result.data = std::move(arr);
  return result;
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

void encode_integer(net::ClientObject &client, int32_t num) {
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
