// Copyright 2015, Outernet Inc.
// Some rights reserved.
// This software is free software licensed under the terms of GPLv3. See COPYING
// file that comes with the source code, or http://www.gnu.org/licenses/gpl.txt.
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "tcpserver/exceptions.h"
#include "tcpserver/server.h"

namespace tcpserver {

const size_t SOCKET_THREAD_POOL_SIZE = 8;

Server::Server(const std::string& port): socket_(port),
                                         epoll_(),
                                         started_(false),
                                         pool_(SOCKET_THREAD_POOL_SIZE)
                                         {}

Server::~Server() {
    if(started_) {
        stop();
    }
}

void Server::start() {
    if(started_) {
        throw server_error("server already running");
    }
    started_ = true;
    pool_.submit(std::bind(&Server::request_loop, this));
    pool_.submit(std::bind(&Server::response_loop, this));
}

void Server::request_loop() {
    try {
        socket_.bind();
        socket_.listen();
    } catch (socket_error& e) {
        throw server_error(e.what());
    }
    try {
        epoll_.add(socket_.fd(), std::bind(&Server::accept_connection,
                                           this,
                                           std::placeholders::_1));
    } catch (epoll_error& e) {
        // TODO: log error
        throw server_error(e.what());
    }
    while (started_) {
        try {
            epoll_.wait();
        } catch (epoll_error& e) {
            throw server_error(e.what());
        }
    }
}

void Server::response_loop() {
    while(started_) {
        ClientResponse response;
        response_queue_.wait_pop(response);
        write_response(response.client_id, response.output);
    }
}

void Server::wait() {
    if(!started_) {
        throw server_error("server not running");
    }
    pool_.join();
}

void Server::stop() {
    if(!started_) {
        throw server_error("server not running");
    }

    started_ = false;
    pool_.join();
    // TODO: close all open connections
}

void Server::accept_connection(int fd) {
    while (true) {
        std::unique_ptr<ClientSocket> p_clientsocket(new ClientSocket(fd));
        int in_fd;
        try {
            in_fd = p_clientsocket->accept();
        } catch (socket_error& e) {
            // TODO: log error
            continue;
        }
        if (in_fd == -1)
            break;

        {
            Lock lock(clients_mutex_);
            clients_.insert(std::make_pair(in_fd, std::move(p_clientsocket)));
            try {
                epoll_.add(in_fd, std::bind(&Server::read_request,
                                            this,
                                            std::placeholders::_1));
            } catch (epoll_error& e) {
                // TODO: log error
                drop_connection(in_fd);
            }
        }
    }
}

void Server::drop_connection(int fd) {
    Lock lock(clients_mutex_);
    try {
        epoll_.remove(fd);
    } catch (epoll_error) {
        // Do nothing because the connection could have been dropped before
        // it was added to epoll
    }
    clients_.erase(fd);
    disconnected(fd);
}

void Server::read_request(int fd) {
    ClientSocket* p_clientsocket;
    {
        Lock lock(clients_mutex_);
        try {
            p_clientsocket = clients_.at(fd).get();
        } catch (std::out_of_range& e) {
            // TODO: log invalid client
            return;
        }
    }
    // socket found, read incoming data into input buffer
    byte_vec input;
    try {
        p_clientsocket->recv(&input);
    } catch (socket_error& e) {
        // TODO: log error
        // deleting the object will close the socket which will automatically
        // make epoll stop monitoring it as well
        drop_connection(fd);
        return;
    } catch (connection_closed& e) {
        // delete socket, but still process the received data after
        drop_connection(fd);
        return;
    }
    handle(fd, input);
}


void Server::write_response(int fd, const byte_vec& output) {
    ClientSocket* p_clientsocket;
    {
        Lock lock(clients_mutex_);
        try {
            p_clientsocket = clients_.at(fd).get();
        } catch (std::out_of_range& e) {
            // TODO: log invalid client
            return;
        }
    }
    try {
        p_clientsocket->send(output);
    } catch (socket_error& e) {
        drop_connection(fd);
    }
}
}  // namespace tcpserver
