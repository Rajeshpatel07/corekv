#pragma once

#include <iostream>
#include <sys/epoll.h>
#include <vector>

namespace corekv::net {

class Epoll {
public:
  Epoll() = default;
  ~Epoll();

  Epoll(const Epoll &) = delete;
  Epoll &operator=(const Epoll &) = delete;

  Epoll(Epoll &&other) noexcept;
  Epoll &operator=(Epoll &&other) noexcept;

  [[nodiscard]] int fd() const noexcept;
  void create();
  void add(epoll_event &ev);
  void modify(epoll_event &ev);
  void remove(epoll_event &ev);
  int wait(std::vector<epoll_event> &events);
  void close();

private:
  int fd_{-1};
};

} // namespace corekv::net
