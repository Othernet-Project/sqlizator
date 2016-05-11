// Copyright 2015, Outernet Inc.
// Some rights reserved.
// This software is free software licensed under the terms of GPLv3. See COPYING
// file that comes with the source code, or http://www.gnu.org/licenses/gpl.txt.
#ifndef TCPSERVER_TCPSERVER_SERVER_H_
#define TCPSERVER_TCPSERVER_SERVER_H_
#include <stdint.h>

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "tcpserver/clientsocket.h"
#include "tcpserver/commontypes.h"
#include "tcpserver/epoll.h"
#include "tcpserver/serversocket.h"
#include "utils/threadpool.h"
#include "utils/threadsafe_queue.h"

namespace tcpserver {

typedef std::map<int, std::unique_ptr<ClientSocket>> SocketMap;

struct ClientResponse {
    int client_id;
    byte_vec output;
};

class Server {
 private:
    typedef std::recursive_mutex Mutex;
    typedef std::lock_guard<Mutex> Lock;

    ServerSocket socket_;
    Epoll epoll_;
    SocketMap clients_;
    Mutex clients_mutex_;

    volatile bool started_;

    void accept_connection(int fd);
    void drop_connection(int fd);
    void read_request(int fd);
    void write_response(int fd, const byte_vec& response);

    void request_loop();
    void response_loop();

protected:
    utils::ThreadPool pool_;
    utils::ThreadsafeQueue<ClientResponse> response_queue_;

    virtual void handle(int client_id, const byte_vec& input) = 0;
    virtual void disconnected(int client_id) = 0;

 public:
    explicit Server(const std::string& port);
    virtual ~Server();
    virtual void start();
    virtual void wait();
    virtual void stop();
};

}  // namespace tcpserver
#endif  // TCPSERVER_TCPSERVER_SERVER_H_
