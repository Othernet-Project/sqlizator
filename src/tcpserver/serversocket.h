// Copyright 2015, Outernet Inc.
// Some rights reserved.
// This software is free software licensed under the terms of GPLv3. See COPYING
// file that comes with the source code, or http://www.gnu.org/licenses/gpl.txt.
#ifndef TCPSERVER_TCPSERVER_SERVERSOCKET_H_
#define TCPSERVER_TCPSERVER_SERVERSOCKET_H_
#include <string>

namespace tcpserver {

class ServerSocket {
 private:
    std::string port_;
    int socket_fd_;

 public:
    explicit ServerSocket(const std::string& port);
    ~ServerSocket();
    int bind();
    void listen();
    int fd();
};

}  // namespace tcpserver
#endif  // TCPSERVER_TCPSERVER_SERVERSOCKET_H_
