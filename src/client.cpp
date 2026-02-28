#include <arpa/inet.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

enum {
  TAG_NIL = 0,  // nil (no value follows)
  TAG_ERR = 1,  // error: [4-byte len][error message]
  TAG_STR = 2,  // string: [4-byte len][value]
  TAG_INT = 3,  // int32: [4-byte value in big-endian]
  TAG_LONG = 4, // int64: [8-byte value in big-endian]
  TAG_DBL = 5,  // double: [8-byte value in big-endian]
  TAG_ARR = 6,  // array: [4-byte count][element1][element2]...
};

// Trim whitespace from string
std::string trim(const std::string &str) {
  size_t first = str.find_first_not_of(' ');
  if (first == std::string::npos) {
    return str;
  }
  size_t last = str.find_last_not_of(' ');
  return str.substr(first, last - first + 1);
}

// Split string by spaces
std::vector<std::string> split(const std::string &src) {
  std::vector<std::string> tokens;
  std::string temp;
  for (char c : src) {
    if (c == ' ') {
      if (!temp.empty()) {
        tokens.push_back(temp);
        temp.clear();
      }
    } else {
      temp += c;
    }
  }
  if (!temp.empty()) {
    tokens.push_back(temp);
  }
  return tokens;
}

// Read exactly n bytes from fd (handles partial reads)
static bool read_all(int fd, void *buf, size_t n) {
  char *ptr = static_cast<char *>(buf);
  while (n > 0) {
    ssize_t rv = read(fd, ptr, n);
    if (rv <= 0) {
      if (rv < 0) {
        perror("read");
      }
      return false;
    }
    ptr += rv;
    n -= static_cast<size_t>(rv);
  }
  return true;
}

// Write exactly n bytes to fd (handles partial writes)
static bool write_all(int fd, const void *buf, size_t n) {
  const char *ptr = static_cast<const char *>(buf);
  while (n > 0) {
    ssize_t wt = write(fd, ptr, n);
    if (wt <= 0) {
      perror("write");
      return false;
    }
    ptr += wt;
    n -= static_cast<size_t>(wt);
  }
  return true;
}

// Read a NIL response (no data follows tag)
void readNil() { std::cout << "(nil)" << std::endl; }

// Read an ERR response: [4-byte len][message]
void readErr(int fd, uint32_t len) {
  std::string msg;
  msg.resize(len);
  if (!read_all(fd, &msg[0], len)) {
    std::cerr << "Failed to read error message" << std::endl;
    return;
  }
  std::cerr << "ERR: " << msg << std::endl;
}

// Read a STR response: [4-byte len][value]
void readStr(int fd, uint32_t len) {
  std::string val;
  val.resize(len);
  if (!read_all(fd, &val[0], len)) {
    std::cerr << "Failed to read string value" << std::endl;
    return;
  }
  std::cout << "\"" << val << "\"" << std::endl;
}

// Read an INT response: [4-byte value in big-endian]
void readInt(int fd) {
  int32_t val;
  if (!read_all(fd, &val, sizeof(val))) {
    std::cerr << "Failed to read int value" << std::endl;
    return;
  }
  val = static_cast<int32_t>(ntohl(static_cast<uint32_t>(val)));
  std::cout << val << std::endl;
}

// Read a LONG response: [8-byte value in big-endian]
void readLong(int fd) {
  int64_t val;
  if (!read_all(fd, &val, sizeof(val))) {
    std::cerr << "Failed to read long value" << std::endl;
    return;
  }
  val = be64toh(val);
  std::cout << val << std::endl;
}

// Read a DBL response: [8-byte value in big-endian]
void readDbl(int fd) {
  uint64_t bits;
  if (!read_all(fd, &bits, sizeof(bits))) {
    std::cerr << "Failed to read double value" << std::endl;
    return;
  }
  bits = be64toh(bits);
  double val;
  memcpy(&val, &bits, sizeof(val));
  std::cout << val << std::endl;
}

// Forward declarations for recursive array parsing
void readArray(int fd);
void readArrayElement(int fd);

// Recursively read array elements
void readArrayElement(int fd) {
  uint8_t tag;
  if (!read_all(fd, &tag, 1)) {
    return;
  }

  switch (tag) {
  case TAG_NIL:
    std::cout << "(nil)";
    break;
  case TAG_ERR: {
    uint32_t len;
    read_all(fd, &len, 4);
    len = ntohl(len);
    readErr(fd, len);
    break;
  }
  case TAG_STR: {
    uint32_t len;
    read_all(fd, &len, 4);
    len = ntohl(len);
    readStr(fd, len);
    break;
  }
  case TAG_INT:
    readInt(fd);
    std::cout << "(int)";
    break;
  case TAG_LONG:
    readLong(fd);
    std::cout << "(long)";
    break;
  case TAG_DBL:
    readDbl(fd);
    std::cout << "(dbl)";
    break;
  case TAG_ARR:
    std::cout << "[";
    readArray(fd);
    std::cout << "]";
    break;
  default:
    std::cerr << "Unknown tag: " << static_cast<int>(tag) << std::endl;
  }
}

// Read an ARR response: [4-byte count][element1][element2]...
void readArray(int fd) {
  uint32_t count;
  if (!read_all(fd, &count, 4)) {
    std::cerr << "Failed to read array count" << std::endl;
    return;
  }
  count = ntohl(count);

  for (uint32_t i = 0; i < count; ++i) {
    readArrayElement(fd);
  }
}

// Main response handler: reads [LEN][TAG][VALUE] format
bool handle_read(int fd) {
  // Step 1: Read 4-byte length (bytes from TAG to end of VALUE)
  uint32_t total_len;
  if (!read_all(fd, &total_len, 4)) {
    return false;
  }
  total_len = ntohl(total_len);

  // Step 2: Read 1-byte tag
  uint8_t tag;
  if (!read_all(fd, &tag, 1)) {
    return false;
  }

  // Step 3: Parse value based on tag
  switch (tag) {
  case TAG_NIL:
    readNil();
    break;
  case TAG_ERR:
    readErr(fd, total_len - 1);
    break;
  case TAG_STR:
    readStr(fd, total_len - 1);
    break;
  case TAG_INT:
    if (total_len != 5) {
      std::cerr << "Invalid INT length: " << total_len << std::endl;
      return false;
    }
    readInt(fd);
    break;
  case TAG_LONG:
    if (total_len != 9) {
      std::cerr << "Invalid LONG length: " << total_len << std::endl;
      return false;
    }
    readLong(fd);
    break;
  case TAG_DBL:
    if (total_len != 9) {
      std::cerr << "Invalid DBL length: " << total_len << std::endl;
      return false;
    }
    readDbl(fd);
    break;
  case TAG_ARR:
    readArray(fd);
    break;
  default:
    std::cerr << "Unknown tag: " << static_cast<int>(tag) << std::endl;
    return false;
  }

  return true;
}

// Send request in LV format: [LEN][LEN][VAL][LEN][VAL]...
bool handle_write(int fd, const std::string &msg) {
  std::vector<std::string> cmd = split(msg);

  // Calculate total payload size
  uint32_t payload_size = 0;
  for (const std::string &s : cmd) {
    payload_size +=
        4 + static_cast<uint32_t>(s.size()); // 4-byte length + value
  }

  // Build message: [LEN][LEN][VAL][LEN][VAL]...
  std::string payload;
  payload.reserve(4 + payload_size);

  // Add total length prefix
  uint32_t net_payload_size = htonl(payload_size);
  payload.append(reinterpret_cast<const char *>(&net_payload_size), 4);

  // Add each command part
  for (const std::string &s : cmd) {
    uint32_t word_len = htonl(static_cast<uint32_t>(s.size()));
    payload.append(reinterpret_cast<const char *>(&word_len), 4);
    payload.append(s);
  }

  return write_all(fd, payload.data(), payload.size());
}

int main() {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    perror("socket");
    return 1;
  }

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(8000);
  inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

  if (connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) <
      0) {
    perror("connect");
    close(fd);
    return 1;
  }

  std::cout << "Commands: get, set, del, keys, exit, help" << std::endl;

  while (true) {
    std::cout << "> ";
    std::string input;
    std::getline(std::cin, input);
    input = trim(input);

    if (input.empty()) {
      continue;
    }

    if (input == "help") {
      std::cout << "get [key]          get value by key\n";
      std::cout << "set [key] [value]  set key-value pair\n";
      std::cout << "del [key]          delete key\n";
      std::cout << "keys               list all keys\n";
      std::cout << "exit               quit\n";
      std::cout << "help               show this help\n";
      continue;
    }

    if (input == "exit") {
      break;
    }

    if (!handle_write(fd, input)) {
      std::cerr << "Error while writing" << std::endl;
      break;
    }

    if (!handle_read(fd)) {
      std::cerr << "Error while reading" << std::endl;
      break;
    }
  }

  close(fd);
  return 0;
}
