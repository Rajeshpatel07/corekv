#include "utils/parser.hpp"
#include "utils/socket.hpp"
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <poll.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

class Conn {
public:
  int fd = -1;
  bool want_read = false;
  bool want_write = false;
  bool want_close = false;
  std::vector<uint8_t> incomming;
  std::vector<uint8_t> outgoing;
};

std::unordered_map<std::string, std::string> store;

void execute_cmd(std::vector<std::string> &cmd, Conn *conn) {
  std::string res = "";
  if (cmd.size() == 2 && cmd[0] == "get") {
    res = store.count(cmd[1]) ? store[cmd[1]] : "nil";
  } else if (cmd.size() == 3 && cmd[0] == "set") {
    store[cmd[1]] = cmd[2];
    res = "ok";
  } else if (cmd.size() == 2 && cmd[0] == "del") {
    store.erase(cmd[1]);
    res = "ok";
  } else {
    res = "invalid command";
  }
  generate_response(conn->outgoing, res);
  return;
}

Conn *handle_accept(int fd) {
  sockaddr_in client;
  socklen_t len = sizeof(client);
  int ac = accept(fd, (sockaddr *)&client, &len);

  if (ac < 0) {

    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      perror("accept:");
    }
    return nullptr;
    perror("accept: ");
  }

  char ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &client.sin_addr, ip, sizeof(ip));
  std::cout << "new client: " << ip << ":" << ntohs(client.sin_port)
            << std::endl;

  set_socket_flags(ac);
  Conn *newConn = new Conn();
  newConn->fd = ac;
  newConn->want_read = true;
  return newConn;
}

bool parse_message(Conn *conn) {
  if (conn->incomming.size() < 4) {
    return false;
  }

  uint32_t header;
  read_u32(conn->incomming, header, 4);

  int k_max_size = 32 << 22;

  if (header > k_max_size) {
    conn->want_close = true;
    return false;
  }

  if (conn->incomming.size() < 4 + header) {
    return false;
  }

  std::vector<std::string> cmd;
  rm_from_buffer(conn->incomming, 4);
  get_cmds(conn->incomming, cmd, header);
  execute_cmd(cmd, conn);

  return true;
}

void handle_read(Conn *conn) {

  char buffer[1024 * 60];

  int rv = read(conn->fd, buffer, sizeof(buffer));

  if (rv <= 0) {
    if (rv < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      return;
    }

    conn->want_close = true;
    if (rv < 0) {
      perror("read: "); // Only print real errors
    }
    return;
  }

  add_to_buffer(conn->incomming, (const uint8_t *)buffer, rv);

  while (parse_message(conn)) {
  }
  if (conn->outgoing.size() > 0) {
    conn->want_read = false;
    conn->want_write = true;
  }
}

void handle_write(Conn *conn) {

  int wt = write(conn->fd, conn->outgoing.data(), conn->outgoing.size());
  if (wt <= 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return;
    }
    conn->want_close = true;
    return;
  }

  if (wt > 0) {
    rm_from_buffer(conn->outgoing, wt);
    // handle_write(conn);
  }

  if (conn->outgoing.empty()) {
    conn->want_write = false;
    conn->want_read = true;
  }
}

int main() {
  int server = socket(AF_INET, SOCK_STREAM, 0);

  int opt = 1;
  int alive = 1;
  setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  setsockopt(server, SOL_SOCKET, SO_KEEPALIVE, &alive, sizeof(alive));

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
  set_socket_flags(server);
  std::cout << "Server listening on port 8000..." << std::endl;

  std::vector<Conn *> fd2conn;

  std::vector<pollfd> poll_args;
  while (true) {
    poll_args.clear();
    pollfd pfd = {server, POLLIN, 0};
    poll_args.push_back(pfd);

    for (Conn *con : fd2conn) {
      if (!con)
        continue;
      pollfd p = {con->fd, POLLERR, 0};
      if (con->want_read) {
        p.events |= POLLIN;
      }
      if (con->want_write) {
        p.events |= POLLOUT;
      }
      poll_args.push_back(p);
    }

    int poll_res = poll(poll_args.data(), (nfds_t)poll_args.size(), -1);
    if (poll_res < 0 && errno == EINTR) {
      continue;
    }
    if (poll_res < 0) {
      exit(1);
    }

    if (poll_args[0].revents & POLLIN) {
      if (Conn *conn = handle_accept(server)) {
        if (fd2conn.size() <= (size_t)conn->fd) {
          fd2conn.resize(conn->fd + 1);
        }
        fd2conn[conn->fd] = conn;
      }
    }

    for (int i = 1; i < poll_args.size(); i++) {
      int ready = poll_args[i].revents;
      Conn *conn = fd2conn[poll_args[i].fd];

      if (ready & POLLIN) {
        handle_read(conn);
      }
      if (ready & POLLOUT) {
        handle_write(conn);
      }

      if (ready & POLLERR || conn->want_close) {
        close(conn->fd);
        fd2conn[conn->fd] = NULL;
        delete conn;
      }
    }
  }
  close(server);
  return 0;
}
