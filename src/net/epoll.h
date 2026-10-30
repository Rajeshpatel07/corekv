#pragma once

#include <span>
#include <sys/epoll.h>

namespace corekv::net {

class Epoll {
public:
  Epoll();
  ~Epoll();

  Epoll(const Epoll &) = delete;
  Epoll &operator=(const Epoll &) = delete;

  Epoll(Epoll &&other) noexcept;
  Epoll &operator=(Epoll &&other) noexcept;

  [[nodiscard]] int fd() const noexcept;

  void add(int fd, const epoll_event &event) const;
  void modify(int fd, const epoll_event &event) const;
  void remove(int fd) const;

  [[nodiscard]] std::span<epoll_event>
  wait(std::span<epoll_event> events) const;

private:
  void close() noexcept;

  int fd_{-1};
};

} // namespace corekv::net
