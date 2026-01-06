#include "socket.hpp"
#include <fcntl.h>

void set_socket_flags(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);

  flags |= O_NONBLOCK;

  fcntl(fd, F_SETFL, flags);
}

void add_to_buffer(std::vector<uint8_t> &dist, const uint8_t *data, int size) {
  dist.insert(dist.end(), data, data + size);
}

void rm_from_buffer(std::vector<uint8_t> &src, int size) {
  src.erase(src.begin(), src.begin() + size);
}
