#include "server/server.h"

#include "parser/parser.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <tuple>
#include <utility>

namespace corekv::server {
namespace {

constexpr std::size_t kMaxEvents = 64;

[[nodiscard]] epoll_event make_event(std::uint32_t events,
                                     void *data) noexcept {
  epoll_event event{};
  event.events = events;
  event.data.ptr = data;
  return event;
}

} // namespace

void Server::run() {
  epoll_.add(listener_.fd(), make_event(EPOLLIN, &listener_));

  std::clog << "Server listening on port " << net::kDefaultTcpPort << '\n';

  std::array<epoll_event, kMaxEvents> events{};
  while (true) {
    for (epoll_event &event : epoll_.wait(events)) {
      if (event.data.ptr == &listener_) {
        accept_clients();
        continue;
      }
      handle_client_event(event);
    }
  }
}

void Server::accept_clients() {
  while (auto client = listener_.try_accept()) {
    register_client(std::move(client));
  }
}

void Server::register_client(std::unique_ptr<net::ClientObject> client) {
  net::ClientObject *raw = client.get();
  epoll_.add(raw->socket.fd(), make_event(EPOLLIN, raw));
  std::ignore = client.release();
}

void Server::disconnect_client(net::ClientObject &client) {
  const int fd = client.socket.fd();
  std::unique_ptr<net::ClientObject> owner{&client};
  epoll_.remove(fd);
}

void Server::handle_client_event(epoll_event event) {
  auto *client = static_cast<net::ClientObject *>(event.data.ptr);

  if ((event.events & (EPOLLERR | EPOLLHUP)) != 0) {
    disconnect_client(*client);
    return;
  }

  if ((event.events & EPOLLIN) != 0) {
    switch (parser::read_from_client(*client)) {
    case parser::IoResult::ok:
      event.events = EPOLLOUT;
      epoll_.modify(client->socket.fd(), event);
      break;
    case parser::IoResult::would_block:
      break;
    case parser::IoResult::closed:
      disconnect_client(*client);
      break;
    }
    return;
  }

  if ((event.events & EPOLLOUT) != 0) {
    switch (parser::write_to_client(*client)) {
    case parser::IoResult::ok:
      event.events = EPOLLIN;
      epoll_.modify(client->socket.fd(), event);
      break;
    case parser::IoResult::would_block:
      break;
    case parser::IoResult::closed:
      disconnect_client(*client);
      break;
    }
  }
}

} // namespace corekv::server
