#pragma once

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

    void add(epoll_event &ev);
    void modify(epoll_event &ev);
    void remove(epoll_event &ev);
    void close(); 

private:
    int fd_{-1};
};

} // namespace corekv::net
