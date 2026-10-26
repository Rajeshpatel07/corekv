
#include "parser.h"
#include <cerrno>
#include <iostream>
#include <sys/epoll.h>
#include <system_error>
#include <unistd.h>
namespace Corekv {

bool read_from_client(int epoll_fd, epoll_event &ev) {
  char buff[1024];
  ssize_t rv = read(ev.data.fd, buff, sizeof(buff) - 1);

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

  ev.events = EPOLLOUT;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, ev.data.fd, &ev) < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to modify epoll event to EPOLLOUT");
  }

  return true;
}

bool write_to_client(int epoll_fd, epoll_event &ev) {
  std::string msg{"Hello from server!"};

  ssize_t wt = write(ev.data.fd, msg.data(), msg.size());
  if (wt < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return true; 
    }
    throw std::system_error(errno, std::generic_category(),
                            "Failed to write to socket");
  }

  return false;
}

} // namespace Corekv
