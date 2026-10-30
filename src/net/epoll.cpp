#include "epoll.h"
#include "socket.h"
#include <cerrno>
#include <sys/epoll.h>
#include <system_error>
#include <unistd.h>

namespace corekv::net {

Epoll::Epoll(Epoll &&other) noexcept : fd_(other.fd_) { other.fd_ = -1; }

Epoll &Epoll::operator=(Epoll &&other) noexcept {
  if (this != &other) {
    close();
    fd_ = other.fd_;
    other.fd_ = -1;
  }
  return *this;
}

Epoll::~Epoll() { close(); }

void Epoll::close() {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

int Epoll::fd() const noexcept { return fd_; }

void Epoll::create() {
  fd_ = epoll_create1(0);
  if (fd_ < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to create epoll instance");
  }
}

void Epoll::add(epoll_event &ev) {
  auto *client = static_cast<ClientObject *>(ev.data.ptr);
  if (epoll_ctl(fd_, EPOLL_CTL_ADD, client->fd, &ev) < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to add socket to epoll");
  }
}

void Epoll::modify(epoll_event &ev) {
  auto *client = static_cast<ClientObject *>(ev.data.ptr);
  if (epoll_ctl(fd_, EPOLL_CTL_MOD, client->fd, &ev) < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to modify socket in epoll");
  }
}

void Epoll::remove(epoll_event &ev) {
  auto *client = static_cast<ClientObject *>(ev.data.ptr);
  if (epoll_ctl(fd_, EPOLL_CTL_DEL, client->fd, &ev) < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to remove socket from epoll");
  }
  delete client;
}

int Epoll::wait(std::vector<epoll_event> &events) {
  while (true) {
    int ndfs =
        epoll_wait(fd_, events.data(), static_cast<int>(events.size()), -1);

    if (ndfs < 0) {
      if (errno == EINTR)
        continue;
      throw std::system_error(errno, std::generic_category(),
                              "epoll_wait failed");
    }

    return ndfs;
  }
}

} // namespace corekv::net
