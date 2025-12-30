#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

bool handle_read(int fd, std::string &buffer) {
  char header[4];

  int header_size = 4;
  char *ptr = header;
  while (header_size > 0) {
    int rv = read(fd, ptr, header_size);
    if (rv <= 0) {
      perror("read header:");
      return false;
    }

    ptr += rv;
    header_size -= rv;
  }

  uint32_t body_size;
  memcpy(&body_size, header, 4);
  body_size = ntohl(body_size);

  buffer.resize(body_size);
  ptr = &buffer[0];
  while (body_size > 0) {
    int rv = read(fd, ptr, body_size);
    if (rv <= 0) {
      perror("read header:");
      return false;
    }
    body_size -= rv;
    ptr += rv;
  }

  std::cout << "server: " << buffer << std::endl;

  return true;
}

bool handle_write(int fd, std::string &msg) {
  int body_size = htonl(msg.size());
  char header[4];
  memcpy(header, &body_size, 4);

  // Send header
  size_t header_size = 4;
  const char *ptr = header;
  while (header_size > 0) {
    int wt = write(fd, ptr, header_size);
    if (wt <= 0) {
      perror("write header: ");
      return false;
    }
    ptr += wt;
    header_size -= wt;
  }

  body_size = msg.size();
  ptr = msg.data();
  while (body_size > 0) {
    int wt = write(fd, ptr, body_size);
    if (wt <= 0) {
      perror("write: ");
      return false;
    }
    ptr += wt;
    body_size -= wt;
  }

  return true;
}

int main() {
  int fd = socket(AF_INET, SOCK_STREAM, 0);

  sockaddr_in address;
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_port = ntohs(8000);
  inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

  if (connect(fd, (sockaddr *)&address, sizeof(address)) < 0) {
    perror("connect: ");
    exit(1);
  }

  std::string input;
  while (true) {
    std::cout << "enter message: ";
    std::getline(std::cin, input);

    if (!handle_write(fd, input)) {
      perror("writing::");
      continue;
    }

    if (input == "exit") {
      std::cout << "Connection closed...." << std::endl;
      break;
    }
    if (!handle_read(fd, input)) {
      perror("Reading:: ");
      continue;
    }
  }

  close(fd);
}
