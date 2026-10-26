#pragma once

#include <sys/epoll.h>
namespace Corekv {

bool read_from_client(int epoll_fd, epoll_event &ev);
bool write_to_client(int epoll_fd, epoll_event &ev);

} // namespace Corekv
