
#include "parser.h"
#include "net/socket.h"
#include <cerrno>
#include <iostream>
#include <sys/epoll.h>
#include <system_error>
#include <unistd.h>

namespace corekv::parser {

bool read_from_client(int epoll_fd, epoll_event &ev) {
  auto *obj = static_cast<net::ClientObject *>(ev.data.ptr);
  char buff[1024];
  ssize_t rv = read(obj->fd, buff, sizeof(buff) - 1);

  if (rv < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return true;
    }
    throw std::system_error(errno, std::generic_category(),
                            "Failed to read from socket");
  }

  if (rv == 0) {
    std::cout << "Client disconnected.\n";
    return false;
  }

  buff[rv] = '\0';
  std::cout << "message from client: " << buff << "\n";

  return true;
}

bool write_to_client(int epoll_fd, epoll_event &ev) {

  auto *obj = static_cast<net::ClientObject *>(ev.data.ptr);
  std::string msg{"Hello from server!"};

  ssize_t wt = write(obj->fd, msg.data(), msg.size());
  if (wt < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return true;
    }
    throw std::system_error(errno, std::generic_category(),
                            "Failed to write to socket");
  }

  return false;
}

} // namespace corekv::parser
