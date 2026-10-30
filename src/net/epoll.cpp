#include "net/epoll.h"

#include <cerrno>
#include <cstddef>
#include <stdexcept>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace corekv::net {

Epoll::Epoll() {
  fd_ = ::epoll_create1(EPOLL_CLOEXEC);
  if (fd_ < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to create epoll instance");
  }
}

Epoll::Epoll(Epoll &&other) noexcept : fd_(std::exchange(other.fd_, -1)) {}

Epoll &Epoll::operator=(Epoll &&other) noexcept {
  if (this != &other) {
    close();
    fd_ = std::exchange(other.fd_, -1);
  }
  return *this;
}

Epoll::~Epoll() { close(); }

void Epoll::close() noexcept {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

int Epoll::fd() const noexcept { return fd_; }

void Epoll::add(int fd, const epoll_event &event) const {
  // epoll_ctl predates const-correctness and takes a non-const pointer even
  // though it does not modify the event.
  epoll_event copy = event;
  if (::epoll_ctl(fd_, EPOLL_CTL_ADD, fd, &copy) < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to add descriptor to epoll");
  }
}

void Epoll::modify(int fd, const epoll_event &event) const {
  epoll_event copy = event;
  if (::epoll_ctl(fd_, EPOLL_CTL_MOD, fd, &copy) < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to modify descriptor in epoll");
  }
}

void Epoll::remove(int fd) const {
  if (::epoll_ctl(fd_, EPOLL_CTL_DEL, fd, nullptr) < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to remove descriptor from epoll");
  }
}

std::span<epoll_event> Epoll::wait(std::span<epoll_event> events) const {
  if (events.empty()) {
    throw std::invalid_argument{"Epoll::wait requires a non-empty buffer"};
  }

  while (true) {
    const int ready =
        ::epoll_wait(fd_, events.data(), static_cast<int>(events.size()), -1);

    if (ready < 0) {
      if (errno == EINTR) {
        continue;
      }
      throw std::system_error(errno, std::generic_category(),
                              "epoll_wait failed");
    }

    //set the std::span size equals to ready not the std::array's size
    return events.first(static_cast<std::size_t>(ready));
  }
}

} // namespace corekv::net
