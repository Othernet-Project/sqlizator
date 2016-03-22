// Copyright 2015, Outernet Inc.
// Some rights reserved.
// This software is free software licensed under the terms of GPLv3. See COPYING
// file that comes with the source code, or http://www.gnu.org/licenses/gpl.txt.
#ifndef TCPSERVER_TCPSERVER_SERVER_H_
#define TCPSERVER_TCPSERVER_SERVER_H_
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "tcpserver/clientsocket.h"
#include "tcpserver/commontypes.h"
#include "tcpserver/epoll.h"
#include "tcpserver/serversocket.h"

namespace tcpserver {

typedef std::map<int, std::unique_ptr<ClientSocket>> SocketMap;

class Server {
 private:
    ServerSocket socket_;
    Epoll epoll_;
    SocketMap clients_;

    void accept_connection(int fd);
    void drop_connection(int fd);
    void receive_data(int fd);
    virtual void handle(int client_id,
                        const byte_vec& input,
                        byte_vec* output) = 0;
    virtual void disconnected(int client_id) = 0;

 public:
    explicit Server(const std::string& port);
    virtual ~Server();
    void start();
};

}  // namespace tcpserver
#endif  // TCPSERVER_TCPSERVER_SERVER_H_
