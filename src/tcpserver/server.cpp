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

Server::Server(const std::string& port): socket_(port),
                                         epoll_(),
                                         thread_(),
                                         started_(false){}

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
    thread_ = std::thread(&Server::run, this);
}

void Server::run() {
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

void Server::wait() {
    if(!started_) {
        throw server_error("server not running");
    }
    if(thread_.joinable()) {
        thread_.join();
    }
}

void Server::stop() {
    if(!started_) {
        throw server_error("server not running");
    }

    started_ = false;
    thread_.join();
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

        clients_.insert(std::make_pair(in_fd, std::move(p_clientsocket)));
        try {
            epoll_.add(in_fd, std::bind(&Server::receive_data,
                                        this,
                                        std::placeholders::_1));
        } catch (epoll_error& e) {
            // TODO: log error
            drop_connection(in_fd);
        }
    }
}

void Server::drop_connection(int fd) {
    try {
        epoll_.remove(fd);
    } catch (epoll_error) {
        // Do nothing because the connection could have been dropped before
        // it was added to epoll
    }
    clients_.erase(fd);
    disconnected(fd);
}

void Server::receive_data(int fd) {
    ClientSocket* p_clientsocket;
    try {
        p_clientsocket = clients_.at(fd).get();
    } catch (std::out_of_range& e) {
        // TODO: log invalid client
        return;
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
    // process freshly read incoming data and write response into output buffer
    byte_vec output;
    handle(fd, input, &output);
    // send response from output buffer back to client
    try {
        p_clientsocket->send(output);
    } catch (socket_error& e) {
        // TODO: log error
        drop_connection(fd);
    }
}

}  // namespace tcpserver
