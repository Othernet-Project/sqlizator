// Copyright 2015, Outernet Inc.
// Some rights reserved.
// This software is free software licensed under the terms of GPLv3. See COPYING
// file that comes with the source code, or http://www.gnu.org/licenses/gpl.txt.
#include <unistd.h>
#include <sys/socket.h>

#include <cstring>
#include <string>

#include "tcpserver/clientsocket.h"
#include "tcpserver/commontypes.h"
#include "tcpserver/exceptions.h"

namespace tcpserver {

ClientSocket::ClientSocket(int server_socket_fd): server_socket_fd_(server_socket_fd) {}

ClientSocket::~ClientSocket() {
    if (socket_fd_ != -1)
        close(socket_fd_);
}

int ClientSocket::fd() {
    return socket_fd_;
}

int ClientSocket::accept() {
    struct sockaddr in_addr;
    socklen_t in_len = sizeof(in_addr);
    int flags = SOCK_NONBLOCK | SOCK_CLOEXEC;
    socket_fd_ = ::accept4(server_socket_fd_, &in_addr, &in_len, flags);
    if ((socket_fd_ == -1) && (errno != EAGAIN) && (errno != EWOULDBLOCK)) {
        std::string msg(std::strerror(errno));
        throw socket_error(msg);
    }
    return socket_fd_;
}

ssize_t ClientSocket::recv(byte_vec* into) {
    while (true) {
        char buf[512];
        ssize_t bytes_read = ::read(socket_fd_, buf, sizeof(buf));
        if (bytes_read == -1) {
            // if EAGAIN, all data is read
            if (errno != EAGAIN) {
                std::string msg(std::strerror(errno));
                throw socket_error(msg);
            }
            break;
        } else if (bytes_read == 0) {
            // remote has closed the connection
            throw connection_closed("Remote has closed the connection.");
        }
        into->insert(into->end(), buf, buf + bytes_read);
    }
    return into->size();
}

ssize_t ClientSocket::send(const byte_vec& data) {
    ssize_t data_size = data.size();
    ssize_t bytes_sent = 0;
    ssize_t bytes_left = data_size;
    ssize_t n;
    while (bytes_sent < data_size) {
        n = ::send(socket_fd_, &data[bytes_sent], bytes_left, 0);
        if (n == -1) {
            std::string msg(std::strerror(errno));
            throw socket_error(msg);
        }
        bytes_sent += n;
        bytes_left -= n;
    }
    return bytes_sent;
}

}  // namespace tcpserver

