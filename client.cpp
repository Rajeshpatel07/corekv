#include <algorithm>
#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

void trim(std::string &str) {

  for (int i = str.size() - 1; i >= 0; i--) {
    if (str[i] == ' ') {
      str.pop_back();
    } else {
      break;
    }
  }
  std::reverse(str.begin(), str.end());
  for (int i = str.size() - 1; i >= 0; i--) {
    if (str[i] == ' ') {
      str.pop_back();
    } else {
      break;
    }
  }
  std::reverse(str.begin(), str.end());
}

void split(std::string &src, std::vector<std::string> &dist, char delimiter) {

  std::string tmp = "";
  for (size_t i = 0; i < src.size(); i++) {
    if (src[i] == ' ') {
      dist.push_back(tmp);
      tmp = "";
    } else {
      tmp += src[i];
    }
  }
  dist.push_back(tmp);
}

bool handle_read(int fd, std::string &buffer) {

  char header[4];
  char *ptr = header;

  int head_size = 4;
  while (head_size > 0) {
    int rv = read(fd, ptr, head_size);
    if (rv <= 0) {
      perror("read: ");
      return false;
    }
    ptr += rv;
    head_size -= rv;
  }

  uint32_t payload_size;
  memcpy(&payload_size, header, 4);
  payload_size = ntohl(payload_size);

  buffer.resize(payload_size);

  ptr = buffer.data();

  while (payload_size > 0) {
    int rv = read(fd, ptr, payload_size);
    if (rv <= 0) {
      perror("read: ");
      return false;
    }
    ptr += rv;
    payload_size -= rv;
  }

  return true;
}

bool handle_write(int fd, std::string &msg) {
  trim(msg);
  std::vector<std::string> cmd;
  split(msg, cmd, ' ');

  std::string payload = "";

  for (std::string s : cmd) {
    uint32_t len = htonl(s.size());
    char head[4];
    memcpy(&head, &len, 4);
    payload.insert(payload.end(), head, head + 4);
    payload += s;
  }

  uint32_t full_len = htonl(payload.size());
  char head[4];
  memcpy(&head, &full_len, 4);

  std::string final_payload;
  final_payload.insert(final_payload.end(), head, head + 4);
  final_payload.insert(final_payload.end(), payload.begin(), payload.end());

  size_t size = final_payload.size();
  char *ptr = final_payload.data();

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

  std::cout << "NOTE: only get, set and del commands are availabel..\n";
  while (true) {
    std::cout << "> ";
    std::string input;
    std::getline(std::cin, input);

    if (input == "exit") {
      break;
    }

    if (!handle_write(fd, input)) {
      std::cerr << "Error while writting..." << std::endl;
    }

    std::string buffer;
    if (!handle_read(fd, buffer)) {
      std::cerr << "Error while reading..." << std::endl;
    }
    std::cout << buffer << std::endl;
  }

  close(fd);
}
