// Copyright 2015, Outernet Inc.
// Some rights reserved.
// This software is free software licensed under the terms of GPLv3. See COPYING
// file that comes with the source code, or http://www.gnu.org/licenses/gpl.txt.
#include <sys/epoll.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <string>

#include "tcpserver/epoll.h"
#include "tcpserver/exceptions.h"

namespace tcpserver {

Epoll::Epoll() {
    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ == -1) {
        std::string msg(std::strerror(errno));
        throw epoll_error(msg);
    }
}

Epoll::~Epoll() {
    if (epoll_fd_ != -1)
        close(epoll_fd_);
}

void Epoll::add(int fd, epoll_fn fn) {
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event) == -1) {
        std::string msg(std::strerror(errno));
        throw epoll_error(msg);
    }
    callbacks_[fd] = fn;
}

void Epoll::remove(int fd) {
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, 0) == -1) {
        std::string msg(std::strerror(errno));
        throw epoll_error(msg);
    }
    callbacks_.erase(fd);
}

void Epoll::wait() {
    int fds_ready = epoll_wait(epoll_fd_, events_, MAX_EVENTS, -1);
    if (fds_ready == -1) {
        std::string msg(std::strerror(errno));
        throw epoll_error(msg);
    }
    for (int i = 0; i < fds_ready; i++) {
        if ((events_[i].events & EPOLLERR) ||
                (events_[i].events & EPOLLHUP) ||
                (!(events_[i].events & EPOLLIN))) {
            close(events_[i].data.fd);
            continue;
        }
        auto found = callbacks_.find(events_[i].data.fd);
        if (found != callbacks_.end()) {
            epoll_fn fn = found->second;
            fn(events_[i].data.fd);
        }
    }
}

}  // namespace tcpserver
