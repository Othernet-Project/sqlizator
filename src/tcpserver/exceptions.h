// Copyright 2015, Outernet Inc.
// Some rights reserved.
// This software is free software licensed under the terms of GPLv3. See COPYING
// file that comes with the source code, or http://www.gnu.org/licenses/gpl.txt.
#ifndef TCPSERVER_TCPSERVER_EXCEPTIONS_H_
#define TCPSERVER_TCPSERVER_EXCEPTIONS_H_
#include <stdexcept>
#include <string>

namespace tcpserver {

class socket_error: public std::runtime_error {
 public:
    explicit socket_error(const std::string& message):
                                                std::runtime_error(message) {}
};

class connection_closed: public std::runtime_error {
 public:
    explicit connection_closed(const std::string& message):
                                                std::runtime_error(message) {}
};

class epoll_error: public std::runtime_error {
 public:
    explicit epoll_error(const std::string& message):
                                                std::runtime_error(message) {}
};

class server_error: public std::runtime_error {
 public:
    explicit server_error(const std::string& message):
                                                std::runtime_error(message) {}
};

}  // namespace tcpserver
#endif  // TCPSERVER_TCPSERVER_EXCEPTIONS_H_

