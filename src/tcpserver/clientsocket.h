// Copyright 2015, Outernet Inc.
// Some rights reserved.
// This software is free software licensed under the terms of GPLv3. See COPYING
// file that comes with the source code, or http://www.gnu.org/licenses/gpl.txt.
#ifndef TCPSERVER_TCPSERVER_CLIENTSOCKET_H_
#define TCPSERVER_TCPSERVER_CLIENTSOCKET_H_
#include <string>

#include "tcpserver/commontypes.h"

namespace tcpserver {

class ClientSocket {
 private:
    int server_socket_fd_;
    int socket_fd_;

 public:
    explicit ClientSocket(int server_socket_fd);
    ~ClientSocket();
    int fd();
    int accept();
    ssize_t recv(byte_vec* into);
    ssize_t send(const byte_vec& data);
};

}  // namespace tcpserver
#endif  // TCPSERVER_TCPSERVER_CLIENTSOCKET_H_

