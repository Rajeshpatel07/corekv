#include "resp.h"
#include "net/socket.h"
#include "parser/parser.h"
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <string>

namespace corekv::RESP {

void print_resp_value(const RespValue &val, size_t indent) {
  std::string prefix(indent, ' ');

  std::visit(
      [&prefix, indent](const auto &arg) {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, std::string>) {
          std::cout << prefix << "[String] " << arg << "\n";
        } else if constexpr (std::is_same_v<T, int64_t>) {
          std::cout << prefix << "[Integer] " << arg << "\n";
        } else if constexpr (std::is_same_v<T, std::unique_ptr<RespArray>>) {
          if (arg) {
            std::cout << prefix << "[Nested Array, size: " << arg->size()
                      << "]\n";
            // RECURSION: Traverse each element inside the nested array
            for (const auto &item : *arg) {
              print_resp_value(item,
                               indent + 2); // Indent sub-items for readability
            }
          } else {
            std::cout << prefix << "[Null/Empty Array]\n";
          }
        }
      },
      val.data);
}

// Handle RESP simple string
std::string decode_simple_string(net::ClientObject &client) {
  client.read_offset += 1;
  if (client.read_offset >= client.read_buffer.size()) {
    throw std::runtime_error("RESP simple string is missing data");
  }

  size_t crlf_pos = std::string::npos;
  for (size_t i = client.read_offset; i + 1 < client.read_buffer.size(); ++i) {
    if (client.read_buffer[i] == '\r' && client.read_buffer[i + 1] == '\n') {
      crlf_pos = i;
      break;
    }
  }

  if (crlf_pos == std::string::npos) {
    throw std::runtime_error("RESP simple string is missing CRLF");
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
  if (client.read_offset >= client.read_buffer.size()) {
    throw std::runtime_error("RESP integer is missing data");
  }

  bool is_negative{client.read_buffer[client.read_offset] == '-'};
  if (is_negative) {
    client.read_offset += 1;
    if (client.read_offset >= client.read_buffer.size()) {
      throw std::runtime_error("RESP integer is missing data");
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
    throw std::runtime_error("RESP integer is missing CRLF");
  }

  size_t len = crlf_pos - client.read_offset;
  std::string payload(len, '\0');
  parser::read_from_buffer(client.read_buffer,
                           reinterpret_cast<uint8_t *>(payload.data()), len,
                           client.read_offset);

  client.read_offset += 2;

  size_t parsed = 0;
  int response = std::stoi(payload, &parsed);
  if (parsed != payload.size()) {
    throw std::runtime_error("RESP integer is not a valid number");
  }

  if (is_negative) {
    response *= -1;
  }

  return response;
}

// Handle RESP bulk string
std::string decode_bulk_string(net::ClientObject &client) {

  client.read_offset += 1;
  if (client.read_offset >= client.read_buffer.size()) {
    throw std::runtime_error("RESP bulk string is missing data");
  }

  size_t crlf_pos = std::string::npos;
  for (size_t i = client.read_offset; i + 1 < client.read_buffer.size(); ++i) {
    if (client.read_buffer[i] == '\r' && client.read_buffer[i + 1] == '\n') {
      crlf_pos = i;
      break;
    }
  }

  if (crlf_pos == std::string::npos) {
    throw std::runtime_error("RESP bulk string length is missing CRLF");
  }

  size_t len = crlf_pos - client.read_offset;
  std::string int_len(len, '\0');
  parser::read_from_buffer(client.read_buffer,
                           reinterpret_cast<uint8_t *>(int_len.data()), len,
                           client.read_offset);

  client.read_offset += 2;

  size_t parsed = 0;
  const int length = std::stoi(int_len, &parsed);
  if (parsed != int_len.size()) {
    throw std::runtime_error("RESP bulk string length is not a valid number");
  }
  if (length < 0) {
    throw std::runtime_error("RESP bulk string length cannot be negative");
  }

  const size_t data_len = static_cast<size_t>(length);
  std::string response(data_len, '\0');

  parser::read_from_buffer(client.read_buffer,
                           reinterpret_cast<uint8_t *>(response.data()),
                           data_len, client.read_offset);

  if (client.read_offset + 1 >= client.read_buffer.size() ||
      client.read_buffer[client.read_offset] != '\r' ||
      client.read_buffer[client.read_offset + 1] != '\n') {
    throw std::runtime_error("RESP bulk string is missing CRLF");
  }
  client.read_offset += 2;

  return response;
}

int64_t decode_big_numbers(net::ClientObject &client) {
  client.read_offset += 1;
  if (client.read_offset >= client.read_buffer.size()) {
    throw std::runtime_error("RESP big number is missing data");
  }

  bool is_negative = client.read_buffer[client.read_offset] == '-';
  if (is_negative) {
    client.read_offset += 1;
    if (client.read_offset >= client.read_buffer.size()) {
      throw std::runtime_error("RESP big number is missing data");
    }
  }

  size_t crlf_pos = std::string::npos;
  for (size_t i{client.read_offset}; i + 1 < client.read_buffer.size(); ++i) {
    if (client.read_buffer[i] == '\r' && client.read_buffer[i + 1] == '\n') {
      crlf_pos = i;
      break;
    }
  }

  if (crlf_pos == std::string::npos) {
    throw std::runtime_error("RESP big number is missing CRLF");
  }

  size_t num_len = crlf_pos - client.read_offset;

  std::string num(num_len, '\0');
  parser::read_from_buffer(client.read_buffer,
                           reinterpret_cast<uint8_t *>(num.data()), num_len,
                           client.read_offset);

  client.read_offset += 2;

  size_t parsed = 0;
  int64_t response = std::stoll(num, &parsed);
  if (parsed != num.size()) {
    throw std::runtime_error("RESP big number is not a valid number");
  }
  if (is_negative) {
    response *= -1;
  }

  return response;
}

RespValue decode_array(net::ClientObject &client) {
  client.read_offset += 1; // Skip '*'
  if (client.read_offset >= client.read_buffer.size()) {
    throw std::runtime_error("RESP array is missing data");
  }

  size_t crlf_pos = std::string::npos;
  for (size_t i{client.read_offset}; i + 1 < client.read_buffer.size(); ++i) {
    if (client.read_buffer[i] == '\r' && client.read_buffer[i + 1] == '\n') {
      crlf_pos = i;
      break;
    }
  }

  if (crlf_pos == std::string::npos) {
    throw std::runtime_error("RESP array length is missing CRLF");
  }

  size_t len_str_size = crlf_pos - client.read_offset;
  std::string int_len(len_str_size, '\0');

  parser::read_from_buffer(client.read_buffer,
                           reinterpret_cast<uint8_t *>(int_len.data()),
                           len_str_size, client.read_offset);

  client.read_offset += 2;

  size_t parsed = 0;
  const int length = std::stoi(int_len, &parsed);
  if (parsed != int_len.size()) {
    throw std::runtime_error("RESP array length is not a valid number");
  }
  if (length < 0) {
    throw std::runtime_error("RESP array length cannot be negative");
  }
  const size_t array_len = static_cast<size_t>(length);

  auto arr = std::make_unique<RespArray>();
  arr->reserve(array_len);

  for (size_t i{0}; i < array_len; ++i) {
    if (client.read_offset >= client.read_buffer.size()) {
      throw std::runtime_error("RESP array is missing an element");
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
      throw std::runtime_error("RESP array contains an unknown element type");
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

void encode_integer(net::ClientObject &client, int32_t num) {
  std::string str{":" + std::to_string(num) + "\r\n"};
  parser::append_to_buffer(reinterpret_cast<uint8_t *>(str.data()),
                           client.write_buffer, str.size());
}

void encode_big_number(net::ClientObject &client, int64_t num) {
  std::string str{"(" + std::to_string(num) + "\r\n"};
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
