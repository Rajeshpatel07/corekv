#include "net/socket.h"

#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace corekv::net {

Socket::Socket(int fd) noexcept : fd_(fd) {}

Socket::Socket(Socket &&other) noexcept : fd_(std::exchange(other.fd_, -1)) {}

Socket &Socket::operator=(Socket &&other) noexcept {
  if (this != &other) {
    close();
    fd_ = std::exchange(other.fd_, -1);
  }
  return *this;
}

Socket::~Socket() { close(); }

int Socket::fd() const noexcept { return fd_; }

void Socket::close() noexcept {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

void Socket::set_nonblocking() const {
  const int flags = ::fcntl(fd_, F_GETFL, 0);
  if (flags < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to get fd flags");
  }

  if (::fcntl(fd_, F_SETFL, flags | O_NONBLOCK) < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to make fd non-blocking");
  }
}

TcpListener::TcpListener() { open(); }

int TcpListener::fd() const noexcept { return socket_.fd(); }

void TcpListener::open() {
  // SOCK_CLOEXEC/SOCK_NONBLOCK avoid a separate fcntl() race after creation.
  const int raw_fd =
      ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (raw_fd < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to create socket");
  }
  socket_ = Socket{raw_fd};

  int reuse_address = 1;
  if (::setsockopt(socket_.fd(), SOL_SOCKET, SO_REUSEADDR, &reuse_address,
                   sizeof(reuse_address)) < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to set SO_REUSEADDR");
  }

  int keep_alive = 1;
  if (::setsockopt(socket_.fd(), SOL_SOCKET, SO_KEEPALIVE, &keep_alive,
                   sizeof(keep_alive)) < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to set SO_KEEPALIVE");
  }

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(kDefaultTcpPort);
  address.sin_addr.s_addr = INADDR_ANY;

  if (::bind(socket_.fd(), reinterpret_cast<const sockaddr *>(&address),
             static_cast<socklen_t>(sizeof(address))) < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to bind socket");
  }

  if (::listen(socket_.fd(), SOMAXCONN) < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to listen on socket");
  }
}

std::unique_ptr<ClientObject> TcpListener::try_accept() {
  while (true) {
    sockaddr_in client_address{};
    socklen_t length = static_cast<socklen_t>(sizeof(client_address));

    const int client_fd =
        ::accept4(socket_.fd(), reinterpret_cast<sockaddr *>(&client_address),
                  &length, SOCK_NONBLOCK | SOCK_CLOEXEC);

    if (client_fd >= 0) {
      // If make_unique throws, the temporary Socket closes client_fd.
      return std::make_unique<ClientObject>(Socket{client_fd});
    }

    if (errno == EINTR) {
      continue;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return nullptr;
    }

    // Any other accept error is a real failure, not a reason to stop quietly.
    throw std::system_error(errno, std::generic_category(),
                            "Failed to accept connection");
  }
}

} // namespace corekv::net
