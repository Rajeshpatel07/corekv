#include <arpa/inet.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
enum class Tag : uint8_t {
  NIL = 0,  // nil (no value follows)
  ERR = 1,  // error: [4-byte len][error message]
  STR = 2,  // string: [4-byte len][value]
  INT = 3,  // int32: [4-byte value in big-endian]
  LONG = 4, // int64: [8-byte value in big-endian]
  DBL = 5,  // double: [8-byte value in big-endian]
  ARR = 6,  // array: [4-byte count][element1][element2]...
};

namespace StringUtils {
std::string trim(const std::string &str) {
  size_t first = str.find_first_not_of(' ');
  if (first == std::string::npos)
    return "";
  size_t last = str.find_last_not_of(' ');
  return str.substr(first, last - first + 1);
}

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
  if (!temp.empty())
    tokens.push_back(temp);
  return tokens;
}
} // namespace StringUtils

namespace IO {
bool read_exact(int fd, void *buf, size_t n) {
  char *ptr = static_cast<char *>(buf);
  while (n > 0) {
    ssize_t rv = read(fd, ptr, n);
    if (rv <= 0) {
      if (rv < 0)
        perror("read");
      return false;
    }
    ptr += rv;
    n -= static_cast<size_t>(rv);
  }
  return true;
}

bool write_exact(int fd, const void *buf, size_t n) {
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

// --- Read Primitives ---
bool read_u8(int fd, uint8_t &val) { return read_exact(fd, &val, 1); }
bool read_u32(int fd, uint32_t &val) {
  if (!read_exact(fd, &val, 4))
    return false;
  val = ntohl(val);
  return true;
}
bool read_u64(int fd, uint64_t &val) {
  if (!read_exact(fd, &val, 8))
    return false;
  val = be64toh(val);
  return true;
}

void append_u32(std::string &buffer, uint32_t val) {
  uint32_t net_val = htonl(val);
  buffer.append(reinterpret_cast<const char *>(&net_val), 4);
}
} // namespace IO

class KvClient {
private:
  int fd_;

public:
  KvClient() : fd_(-1) {}

  ~KvClient() {
    if (fd_ >= 0)
      close(fd_);
  }

  bool connect_to_server(const std::string &ip, int port) {
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) {
      perror("socket");
      return false;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &address.sin_addr);

    if (connect(fd_, reinterpret_cast<sockaddr *>(&address), sizeof(address)) <
        0) {
      perror("connect");
      return false;
    }
    return true;
  }

  // Formats and sends: [Total_Len][Len][Word][Len][Word]...
  bool send_command(const std::string &input) {
    std::vector<std::string> cmd = StringUtils::split(input);
    if (cmd.empty())
      return true;

    uint32_t payload_size = 0;
    for (const auto &s : cmd) {
      payload_size += 4 + s.size();
    }

    std::string payload;
    payload.reserve(4 + payload_size);

    IO::append_u32(payload, payload_size);
    for (const auto &s : cmd) {
      IO::append_u32(payload, s.size());
      payload.append(s);
    }

    return IO::write_exact(fd_, payload.data(), payload.size());
  }

  // Reads top-level response: [Total_Len][Tag][Value]
  bool receive_response() {
    uint32_t total_len;
    if (!IO::read_u32(fd_, total_len))
      return false;

    parse_element(total_len);
    std::cout << "\n";
    return true;
  }

private:
  // Recursively handles reading based on the Tag Type
  void parse_element(uint32_t &total_len) {
    if (total_len < 1) {
      return;
    }

    uint8_t raw_tag;
    if (!IO::read_u8(fd_, raw_tag))
      return;
    total_len -= 1;

    Tag tag = static_cast<Tag>(raw_tag);
    switch (tag) {
    case Tag::NIL: {
      std::cout << "(nil)";
      break;
    }
    case Tag::ERR: {
      uint32_t len;
      if (IO::read_u32(fd_, len)) {
        std::string msg(len, '\0');
        IO::read_exact(fd_, &msg[0], len);
        total_len -= 4 + len;
        std::cerr << "(error) " << msg;
      }
      break;
    }
    case Tag::STR: {
      uint32_t len;
      if (IO::read_u32(fd_, len)) {
        std::string val(len, '\0');
        IO::read_exact(fd_, &val[0], len);
        total_len -= 4 + len;
        std::cout << "\"" << val << "\"";
      }
      break;
    }
    case Tag::INT: {
      uint32_t val;
      if (IO::read_u32(fd_, val)) {
        std::cout << static_cast<int32_t>(val);
        total_len -= 4;
      }
      break;
    }
    case Tag::LONG: {
      uint64_t val;
      if (IO::read_u64(fd_, val)) {
        total_len -= 8;
        std::cout << static_cast<int64_t>(val);
      }
      break;
    }
    case Tag::DBL: {
      uint64_t bits;
      if (IO::read_u64(fd_, bits)) {
        double val;
        std::memcpy(&val, &bits, sizeof(val));
        total_len -= 8;
        std::cout << val;
      }
      break;
    }
    case Tag::ARR: {
      uint32_t count;
      if (IO::read_u32(fd_, count)) {
        std::cout << "[";
        for (uint32_t i = 0; i < count; ++i) {
          parse_element(total_len);
          if (i < count - 1)
            std::cout << ", ";
        }
        std::cout << "]";
      }
      break;
    }
    default: {
      std::cerr << "[Unknown Tag: " << static_cast<int>(raw_tag) << "]";
      break;
    }
    }

    return;
  }
};

void print_help() {
  std::cout << "Available Commands:\n"
            << "  get [key]          get value by key\n"
            << "  set [key] [value]  set key-value pair\n"
            << "  del [key]          delete key\n"
            << "  keys               list all keys\n"
            << "  help               show this help\n"
            << "  exit               quit\n";
}

int main() {
  KvClient client;

  if (!client.connect_to_server("127.0.0.1", 8000)) {
    std::cerr << "Failed to connect to server." << std::endl;
    return 1;
  }

  std::cout << "Connected to KV Store! Type 'help' for commands.\n";

  while (true) {
    std::cout << "> ";
    std::string input;

    if (!std::getline(std::cin, input))
      break;

    input = StringUtils::trim(input);
    if (input.empty())
      continue;

    if (input == "help") {
      print_help();
      continue;
    }
    if (input == "exit") {
      break;
    }

    if (!client.send_command(input)) {
      std::cerr << "Connection lost while writing." << std::endl;
      break;
    }
    if (!client.receive_response()) {
      std::cerr << "Connection lost while reading." << std::endl;
    }
  }
  return 0;
}
