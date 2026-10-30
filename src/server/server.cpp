#include "server.h"
#include "net/socket.h"
#include "parser/parser.h"
#include <sys/epoll.h>

namespace corekv::server {

Server::Server() {
  listener_.open();
  start_eventloop();
}

Server::Server(Server &&other) noexcept
    : epoll_(std::move(other.epoll_)), listener_(std::move(other.listener_)) {}

Server &Server::operator=(Server &&other) noexcept {
  if (this != &other) {
    epoll_.close();
    listener_.close();
    epoll_ = std::move(other.epoll_);
    listener_ = std::move(other.listener_);
  }
  return *this;
}

void Server::start_eventloop() {
  epoll_.create();

  std::cout << "server started at port 8000\n";
  epoll_event ev{};
  ev.events = EPOLLIN;
  auto *obj{new net::ClientObject};
  obj->fd = listener_.fd();

  ev.data.ptr = obj;

  epoll_.add(ev);

  std::vector<epoll_event> events(64);

  while (true) {
    int ndfs = epoll_.wait(events);

    for (int i = 0; i < ndfs; i++) {
      auto *obj = static_cast<net::ClientObject *>(events[i].data.ptr);
      uint32_t event = events[i].events;

      if (event == EPOLLERR || event == EPOLLHUP) {
        epoll_.remove(events[i]);
        continue;
      }

      if (obj->fd == listener_.fd()) {
        listener_.try_accept(epoll_);
        continue;
      }

      handle_client_events(events[i]);
    }
  }
}

void Server::handle_client_events(epoll_event &ev) {

  auto *obj = static_cast<net::ClientObject *>(ev.data.ptr);
  uint32_t event = ev.events;
  if (event == EPOLLIN) {
    parser::read_from_client(epoll_.fd(), ev);
    ev.events = EPOLLOUT;
    epoll_.modify(ev);
  }

  if (event == EPOLLOUT) {
    parser::write_to_client(epoll_.fd(), ev);
    ev.events=EPOLLHUP;
    epoll_.modify(ev);
  }
}

} // namespace corekv::server
