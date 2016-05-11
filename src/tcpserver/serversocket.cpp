// Copyright 2015, Outernet Inc.
// Some rights reserved.
// This software is free software licensed under the terms of GPLv3. See COPYING
// file that comes with the source code, or http://www.gnu.org/licenses/gpl.txt.
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <string>

#include "tcpserver/exceptions.h"
#include "tcpserver/serversocket.h"

namespace tcpserver {

ServerSocket::ServerSocket(const std::string& port): port_(port),
                                                     socket_fd_(-1) {}

ServerSocket::~ServerSocket() {
    if (socket_fd_ != -1)
        close(socket_fd_);
}

int ServerSocket::bind() {
    struct addrinfo hints;
    struct addrinfo* addr_infos;

    std::memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int ret_val = getaddrinfo(NULL, port_.c_str(), &hints, &addr_infos);
    if (ret_val != 0) {
        freeaddrinfo(addr_infos);
        std::string msg(gai_strerror(ret_val));
        throw socket_error(msg);
    }

    for (auto aip = addr_infos; aip != NULL; aip = aip->ai_next) {
        int flags = aip->ai_socktype | SOCK_CLOEXEC | O_NONBLOCK;
        socket_fd_ = socket(aip->ai_family, flags, aip->ai_protocol);
        if (socket_fd_ == -1)
            continue;

        int so_reuseaddr = 1;
        setsockopt(socket_fd_,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   &so_reuseaddr,
                   sizeof(so_reuseaddr));
        if (::bind(socket_fd_, aip->ai_addr, aip->ai_addrlen) == 0)
            break; // bind succeeded
        // in case bind failed, close the created socket file descriptor
        close(socket_fd_);
        socket_fd_ = -1;
    }

    freeaddrinfo(addr_infos);
    if (socket_fd_ == -1)
        throw socket_error("Failed to bind.");

    return socket_fd_;
}

void ServerSocket::listen() {
    if (::listen(socket_fd_, SOMAXCONN) == -1) {
        std::string msg(std::strerror(errno));
        throw socket_error(msg);
    }
}

int ServerSocket::fd() {
    return socket_fd_;
}

}  // namespace tcpserver
