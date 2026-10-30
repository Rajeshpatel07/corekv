#pragma once

#include "net/epoll.h"
#include <cstdint>
#include <vector>

namespace corekv::net {

struct ClientObject {
    int fd{-1};
    std::vector<uint8_t> read_buffer;
    std::vector<uint8_t> write_buffer;
};

class Socket {
public:
    Socket() = default;
    explicit Socket(int fd);
    
    Socket(const Socket &) = delete;
    Socket &operator=(const Socket &) = delete;
    
    Socket(Socket &&other) noexcept;
    Socket &operator=(Socket &&other) noexcept;
    
    ~Socket(); // Fixed: RAII Destructor added

    void close();
    [[nodiscard]] int fd() const noexcept;
    void set_socket_nonblocking();

private:
    int fd_{-1};
};

class TcpListener {
public:
    TcpListener() = default;
    
    TcpListener(const TcpListener &) = delete;
    TcpListener &operator=(const TcpListener &) = delete;
    
    TcpListener(TcpListener &&other) noexcept;
    TcpListener &operator=(TcpListener &&other) noexcept;
    
    ~TcpListener() = default;

    [[nodiscard]] int fd() const noexcept;
    void open();
    void try_accept(Epoll &epoll_instance);

private:
    Socket socket_;
};

} // namespace corekv::net
