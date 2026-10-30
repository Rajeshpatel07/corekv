#include "socket.h"
#include "net/epoll.h"
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>

namespace corekv::net {

Socket::Socket(int fd) : fd_(fd) {}

Socket::Socket(Socket &&other) noexcept : fd_(other.fd_) { other.fd_ = -1; }

Socket &Socket::operator=(Socket &&other) noexcept {
  if (this != &other) {
    close();
    fd_ = other.fd_;
    other.fd_ = -1;
  }
  return *this;
}

Socket::~Socket() { close(); }

int Socket::fd() const noexcept { return fd_; }

void Socket::set_socket_nonblocking() {
  int flags = fcntl(fd(), F_GETFL, 0);
  if (flags < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to get fd flags");
  }

  if (fcntl(fd(), F_SETFL, flags | O_NONBLOCK) < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to set fd to non_block");
  }
}

void Socket::close() {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

TcpListener::TcpListener(TcpListener &&other) noexcept
    : socket_(std::move(other.socket_)) {}

TcpListener &TcpListener::operator=(TcpListener &&other) noexcept {
  if (this != &other) {
    socket_ = std::move(other.socket_);
  }
  return *this;
}

int TcpListener::fd() const noexcept { return socket_.fd(); }

void TcpListener::open() {
  int raw_fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if (raw_fd < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to create socket");
  }
  socket_ = Socket(raw_fd);

  int opt_reuseaddr = 1;
  if (setsockopt(socket_.fd(), SOL_SOCKET, SO_REUSEADDR, &opt_reuseaddr,
                 sizeof(opt_reuseaddr)) < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to set SO_REUSEADDR");
  }

  int opt_keepalive = 1;
  if (setsockopt(socket_.fd(), SOL_SOCKET, SO_KEEPALIVE, &opt_keepalive,
                 sizeof(opt_keepalive)) < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to set SO_KEEPALIVE");
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(8000);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(socket_.fd(), reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) <
      0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to bind socket");
  }

  if (listen(socket_.fd(), SOMAXCONN) < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to listen on socket");
  }
}

void TcpListener::try_accept(Epoll &epoll_instance) {
  while (true) {
    sockaddr_in client{};
    socklen_t len = sizeof(client);

    int client_fd = accept4(socket_.fd(), reinterpret_cast<sockaddr *>(&client),
                            &len, SOCK_NONBLOCK);

    if (client_fd < 0) {
      if (errno == EINTR)
        continue;
      break;
    }

    auto *client_obj = new ClientObject{client_fd, {}, {}};

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.ptr = client_obj;

    epoll_instance.add(ev);
  }
}

void TcpListener::close(){
  socket_.close();
}

} // namespace corekv::net
