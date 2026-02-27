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

std::string trim(const std::string &str) {
  size_t first = str.find_first_not_of(' ');
  if (std::string::npos == first) {
    return str;
  }
  size_t last = str.find_last_not_of(' ');
  return str.substr(first, (last - first + 1));
}

std::vector<std::string> split(const std::string &src) {
  std::vector<std::string> tokens;
  std::string temp = "";
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

bool handle_read(int fd, std::string &buffer) {
  char header[4];
  char *ptr = header;
  int head_size = 4;

  while (head_size > 0) {
    int rv = read(fd, ptr, head_size);

    if (rv < 0) {
      perror("read header");
      return false;
    }
    if (rv == 0) {
      std::cerr << "Server closed connection (EOF)." << std::endl;
      return false;
    }

    ptr += rv;
    head_size -= rv;
  }

  uint32_t net_len;
  memcpy(&net_len, header, 4);
  uint32_t payload_size = ntohl(net_len);

  // if (payload_size > 10 * 1024 * 1024) {
  //   std::cerr << "Packet too large: " << payload_size << " bytes." <<
  //   std::endl; return false;
  // }

  buffer.resize(payload_size);
  ptr = &buffer[0];
  while (payload_size > 0) {
    int rv = read(fd, ptr, payload_size);

    if (rv < 0) {
      perror("read body");
      return false;
    }
    if (rv == 0) {
      std::cerr << "Server closed connection during body read." << std::endl;
      return false;
    }

    ptr += rv;
    payload_size -= rv;
  }

  return true;
}

bool handle_write(int fd, std::string &msg) {

  std::vector<std::string> cmd = split(msg);

  uint32_t payload_size = 0;
  for (const std::string &s : cmd) {
    payload_size += 4;
    payload_size += s.size();
  }

  std::string payload;
  payload.reserve(4 + payload_size);

  uint32_t net_payload_size = htonl(payload_size);
  payload.append((char *)&net_payload_size, 4);

  for (const std::string &s : cmd) {
    uint32_t word_len = htonl(s.size());
    payload.append((char *)&word_len, 4);

    payload.append(s);
  }

  char *ptr = payload.data();
  size_t size = payload.size();

  while (size > 0) {
    int wt = write(fd, ptr, size);
    if (wt <= 0) {
      perror("write: ");
      return false;
    }
    ptr += wt;
    size -= wt;
  }

  return true;
}

int main() {
  int fd = socket(AF_INET, SOCK_STREAM, 0);

  sockaddr_in address;
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_port = htons(8000);
  inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

  if (connect(fd, (sockaddr *)&address, sizeof(address)) < 0) {
    perror("connect: ");
    exit(1);
  }

  std::cout << "NOTE: get, set, del, exit and help commands are available..\n";
  while (true) {
    std::cout << "> ";
    std::string input;
    std::cin.clear();
    std::getline(std::cin, input);
    input = trim(input);

    if (input.empty()) {
      continue;
    }
    if (input == "help") {
      std::cout << "get [key] 		get the value of key from DB\n";
      std::cout << "del [key]		remove the key-value from DB\n";
      std::cout << "set [key] [value]	store the key and value in DB\n";
      std::cout << "exit			exit\n";
      std::cout << "help			show help\n";
      continue;
    }
    if (input == "exit") {
      break;
    }

    if (!handle_write(fd, input)) {
      std::cerr << "Error while writting..." << std::endl;
      break;
    }

    std::string buffer;
    if (!handle_read(fd, buffer)) {
      std::cerr << "Error while reading..." << std::endl;
      break;
    }
    std::cout << buffer << std::endl;
  }

  close(fd);
  return 0;
}
