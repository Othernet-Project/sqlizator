// Copyright 2015, Outernet Inc.
// Some rights reserved.
// This software is free software licensed under the terms of GPLv3. See COPYING
// file that comes with the source code, or http://www.gnu.org/licenses/gpl.txt.
#ifndef TCPSERVER_TCPSERVER_EPOLL_H_
#define TCPSERVER_TCPSERVER_EPOLL_H_
#include <sys/epoll.h>

#include <functional>
#include <map>
#include <string>

namespace tcpserver {

typedef std::function<void(int)> epoll_fn;

static const int MAX_EVENTS = 64;

class Epoll {
 private:
    int epoll_fd_;
    struct epoll_event events_[MAX_EVENTS];
    std::map<int, epoll_fn> callbacks_;

 public:
    Epoll();
    ~Epoll();
    void add(int fd, epoll_fn fn);
    void remove(int fd);
    void wait();
};

}  // namespace tcpserver
#endif  // TCPSERVER_TCPSERVER_EPOLL_H_
