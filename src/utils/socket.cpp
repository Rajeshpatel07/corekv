#include "socket.hpp"
#include <fcntl.h>

void set_socket_flags(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);

  flags |= O_NONBLOCK;

  fcntl(fd, F_SETFL, flags);
}
