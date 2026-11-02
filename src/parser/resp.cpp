#include "resp.h"
#include "parser/parser.h"
#include <cstring>
#include <netinet/in.h>
#include <string>

namespace corekv::RESP {

// Handle RESP simple string
std::string decode_simple_string(net::ClientObject &client) {
  client.read_offset += 1;
  size_t crlf_pos = std::string::npos;
  for (size_t i = client.read_offset; i < client.read_buffer.size() - 1; ++i) {
    if (client.read_buffer[i] == '\r' && client.read_buffer[i + 1] == '\n') {
      crlf_pos = i;
      break;
    }
  }

  if (crlf_pos == std::string::npos) {
    return "";
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
int decode_integer(net::ClientObject &client) {

  client.read_offset += 1;
  bool is_negative{client.read_buffer[client.read_offset] == '-'};
  if (is_negative) {
    client.read_offset += 1;
  }

  size_t crlf_pos = std::string::npos;

  for (size_t i = client.read_offset; i < client.read_buffer.size() - 1; ++i) {
    if (client.read_buffer[i] == '\r' && client.read_buffer[i + 1] == '\n') {
      crlf_pos = i;
      break;
    }
  }

  if (crlf_pos == std::string::npos) {
    return 0;
  }

  size_t len = crlf_pos - client.read_offset;
  std::string payload(len, '\0');
  parser::read_from_buffer(client.read_buffer,
                           reinterpret_cast<uint8_t *>(payload.data()), len,
                           client.read_offset);

  client.read_offset += 2;

  int response = std::stoi(payload);

  if (is_negative) {
    response *= -1;
  }

  return response;
}

// Handle RESP bulk string
std::string decode_bulk_string(net::ClientObject &client) {

  client.read_offset += 1;
  size_t crlf_pos = std::string::npos;
  for (size_t i = client.read_offset; i < client.read_buffer.size() - 1; ++i) {
    if (client.read_buffer[i] == '\r' && client.read_buffer[i + 1] == '\n') {
      crlf_pos = i;
      break;
    }
  }

  if (crlf_pos == std::string::npos) {
    return "";
  }

  size_t len = crlf_pos - client.read_offset;
  std::string int_len(len, '\0');
  parser::read_from_buffer(client.read_buffer,
                           reinterpret_cast<uint8_t *>(int_len.data()), len,
                           client.read_offset);

  client.read_offset += 2;

  size_t data_len = static_cast<size_t>(std::stoi(int_len));

  std::string response(data_len, '\0');

  parser::read_from_buffer(client.read_buffer,
                           reinterpret_cast<uint8_t *>(response.data()),
                           data_len, client.read_offset);
  client.read_offset += 2;

  return response;
}

int64_t decode_big_numbers(net::ClientObject &client) {
  client.read_offset += 1;

  bool is_negative = client.read_buffer[client.read_offset] == '-';
  if (is_negative) {
    client.read_offset += 1;
  }

  size_t crlf_pos = std::string::npos;
  for (size_t i{client.read_offset}; i < client.read_buffer.size() - 1; ++i) {
    if (client.read_buffer[i] == '\r' && client.read_buffer[i + 1] == '\n') {
      crlf_pos = i;
      break;
    }
  }

  size_t num_len = crlf_pos - client.read_offset;

  std::string num(num_len, '\0');
  parser::read_from_buffer(client.read_buffer,
                           reinterpret_cast<uint8_t *>(num.data()), num_len,
                           client.read_offset);

  client.read_offset += 2;

  int64_t response = stoi(num);
  if (is_negative) {
    response *= -1;
  }

  return response;
}

RespValue decode_array(net::ClientObject &client) {
  client.read_offset += 1; // Skip '*'

  size_t crlf_pos = std::string::npos;
  for (size_t i{client.read_offset}; i < client.read_buffer.size() - 1; ++i) {
    if (client.read_buffer[i] == '\r' && client.read_buffer[i + 1] == '\n') {
      crlf_pos = i;
      break;
    }
  }

  if (crlf_pos == std::string::npos) {
    return RespValue{};
  }

  size_t len_str_size = crlf_pos - client.read_offset;
  std::string int_len(len_str_size, '\0');

  parser::read_from_buffer(client.read_buffer,
                           reinterpret_cast<uint8_t *>(int_len.data()),
                           len_str_size, client.read_offset);

  client.read_offset += 2;

  size_t array_len = static_cast<size_t>(std::stoi(int_len));

  auto arr = std::make_unique<RespArray>();
  arr->reserve(array_len);

  for (size_t i{0}; i < array_len; ++i) {
    if (client.read_offset >= client.read_buffer.size()) {
      break;
    }

    uint8_t type = client.read_buffer[client.read_offset];
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
    case '(':
      element.data = decode_big_numbers(client);
      break;
    case '*': {
      RespValue nested_arr = decode_array(client);
      element.data =
          std::move(std::get<std::unique_ptr<RespArray>>(nested_arr.data));
      break;
    }
    default:
      break;
    }

    arr->push_back(std::move(element));
  }

  RespValue result;
  result.data = std::move(arr);
  return result;
}

} // namespace corekv::RESP
