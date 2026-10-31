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

  std::string payload(data_len, '\0');

  parser::read_from_buffer(client.read_buffer,
                           reinterpret_cast<uint8_t *>(payload.data()),
                           data_len, client.read_offset);

  client.read_offset += 2;

  return payload;
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

  std::string payload(data_len, '\0');

  parser::read_from_buffer(client.read_buffer,
                           reinterpret_cast<uint8_t *>(payload.data()),
                           data_len, client.read_offset);
  client.read_offset += 2;

  return payload;
}

} // namespace corekv::RESP
