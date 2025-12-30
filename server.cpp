#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
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

  long body_size;
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

  std::cout << "client: " << buffer << std::endl;

  return true;
}

bool handle_write(int fd, std::string &msg) {
  int body_size = htonl(msg.size());
  char header[4];
  memcpy(header, &body_size, 4);

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
  int server = socket(AF_INET, SOCK_STREAM, 0);

  int opt = 1;
  int live = 1;

  setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  setsockopt(server, SOL_SOCKET, SO_KEEPALIVE, &live, sizeof(live));

  sockaddr_in serveraddr;
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_port = htons(8000);
  serveraddr.sin_addr.s_addr = INADDR_ANY; // 0.0.0.0

  if (bind(server, (sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
    perror("bind: ");
    exit(1);
    return 1;
  }

  listen(server, SOMAXCONN);

  std::cout << "Server listening on port 8000..." << std::endl;

  while (true) {
    sockaddr_in client;
    socklen_t client_len = sizeof(client);
    int conn = accept(server, (sockaddr *)&client, &client_len);

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client.sin_addr.s_addr, ip, sizeof(ip));
    std::cout << "client connected:: " << ip << ":" << ntohs(client.sin_port)
              << std::endl;

    std::string input;
    while (true) {
      if (!handle_read(conn, input)) {
        break;
      }
      if (input == "exit") {
        std::cout << "Connection closed...." << std::endl;
        break;
      }
      if (!handle_write(conn, input)) {
        break;
      }
    }

    close(conn);
  }
  close(server);
}
