// Copyright 2015, Outernet Inc.
// Some rights reserved.
// This software is free software licensed under the terms of GPLv3. See COPYING
// file that comes with the source code, or http://www.gnu.org/licenses/gpl.txt.
#include <msgpack.hpp>

#include <iostream>
#include <chrono>
#include <cstdio>
#include <map>
#include <stdexcept>
#include <string>

#include "sqlizator/database.h"
#include "sqlizator/exceptions.h"
#include "sqlizator/response.h"
#include "sqlizator/server.h"

namespace sqlizator {

DBServer::DBServer(const std::string& port): tcpserver::Server(port) {
    endpoints_.insert(std::make_pair("connect", &DBServer::endpoint_connect));
    endpoints_.insert(std::make_pair("drop", &DBServer::endpoint_drop));
    endpoints_.insert(std::make_pair("query", &DBServer::endpoint_query));
}

void DBServer::set_status(int status,
                          const std::string& message,
                          const std::string& extended,
                          Packer* reply_header) {
    reply_header->pack(std::string("status"));
    reply_header->pack(status);
    reply_header->pack(std::string("message"));
    reply_header->pack(message);
    reply_header->pack(std::string("details"));
    reply_header->pack(extended);
}

int DBServer::get_optional_arg(const StringMap& msg,
                               const std::string& name,
                               int default_value) {
    auto it = msg.find(name);
    if (it != msg.end()) {
        try {
            return std::stoi(it->second);
        } catch (...) {
            return default_value;
        }
    }
    return default_value;
}

void DBServer::start() {
    Server::start();

    pool_.submit(std::bind(&DBServer::process_loop, this));
    pool_.submit(std::bind(&DBServer::process_loop, this));
    pool_.submit(std::bind(&DBServer::process_loop, this));
    pool_.submit(std::bind(&DBServer::process_loop, this));
}

void DBServer::endpoint_connect(int client_id,
                                const msgpack::object& request,
                                Packer* reply_header,
                                Packer* reply_data) {
    reply_header->pack_map(header_sizes::CONNECT);
    StringMap msg;
    try {
        request.convert(msg);
    } catch (msgpack::type_error& e) {
        // TODO: log error, message cannot be deserialized
        set_status(status_codes::DESERIALIZATION_ERROR,
                   "Deserialization failed.",
                   e.what(),
                   reply_header);
        return;
    }
    std::string path;
    try {
        path = msg["database"];
    } catch (std::out_of_range& e) {
        set_status(status_codes::INVALID_REQUEST,
                   "Missing database path.",
                   "",
                   reply_header);
        return;
    }
    // check if it's already connected to the database maybe
    if (!connections_.count(client_id)) {
        // no connection exists yet
        int max_retry = get_optional_arg(msg, "max_retry", DEFAULT_MAX_RETRY);
        int sleep_ms = get_optional_arg(msg, "sleep_ms", DEFAULT_SLEEP_MS);
        std::shared_ptr<Database> db(new Database(path, max_retry, sleep_ms));
        try {
            db->connect();
        } catch (sqlite_error& e) {
            set_status(status_codes::DATABASE_OPENING_ERROR,
                       e.what(),
                       e.extended(),
                       reply_header);
            return;
        }
        // apply passed in pragmas to newly opened database
        for (auto it = msg.begin(); it != msg.end(); ++it) {
            if (std::find(std::begin(PRAGMAS),
                          std::end(PRAGMAS),
                          it->first) != std::end(PRAGMAS))
                db->pragma(it->first, it->second);
        }
        connections_.insert(std::make_pair(client_id, std::move(db)));
    } else {
        // already connected to a database with that name
        auto db = connections_.at(client_id);
        // in case the database was already open, verify that the passed in path
        // matches the path of the already open database. in case it doesn't, the
        // same name was used for two different databases, which is unacceptable
        if (db->path() != path) {
            set_status(status_codes::INVALID_REQUEST,
                       "Connection requested from same socket to a different database.",
                       db->path(),
                       reply_header);
            return;
        }
    }
    set_status(status_codes::OK, response_messages::OK, "", reply_header);
}

void DBServer::endpoint_drop(int client_id,
                             const msgpack::object& request,
                             Packer* reply_header,
                             Packer* reply_data) {
    reply_header->pack_map(header_sizes::DROP);
    std::map<std::string, std::string> msg;
    try {
        request.convert(msg);
    } catch (msgpack::type_error& e) {
        // TODO: log error, message cannot be deserialized
        set_status(status_codes::DESERIALIZATION_ERROR,
                   "Deserialization failed.",
                   e.what(),
                   reply_header);
        return;
    }
    std::string path;
    try {
        path = msg["database"];
    } catch (std::out_of_range& e) {
        set_status(status_codes::INVALID_REQUEST,
                   "Missing database path.",
                   "",
                   reply_header);
        return;
    }
    // delete connections to the particular database
    auto it = connections_.begin();
    while (it != connections_.end()) {
        if (it->second->path() == path)
            it = connections_.erase(it);
        else
            it++;
    }
    // remove database file from file system
    std::remove(path.c_str());
    set_status(status_codes::OK, response_messages::OK, "", reply_header);
}

void DBServer::write_query_header_defaults(Packer* reply_header) {
    reply_header->pack(std::string("rowcount"));
    reply_header->pack(-1);
    reply_header->pack(std::string("columns"));
    reply_header->pack_nil();
}

void DBServer::endpoint_query(int client_id,
                              const msgpack::object& request,
                              Packer* reply_header,
                              Packer* reply_data) {
    reply_header->pack_map(header_sizes::QUERY);
    MsgType msg;
    try {
        request.convert(msg);
    } catch (msgpack::type_error& e) {
        // TODO: log error, message cannot be deserialized
        set_status(status_codes::DESERIALIZATION_ERROR,
                   "Deserialization failed.",
                   e.what(),
                   reply_header);
        write_query_header_defaults(reply_header);
        return;
    }
    if (!connections_.count(client_id)) {
        set_status(status_codes::INVALID_REQUEST,
                   "Not connected to database.",
                   msg.database,
                   reply_header);
        return;
    }
    auto db = connections_.at(client_id);
    try {
        db->query(client_id,
                  msg.operation,
                  msg.query,
                  msg.parameters,
                  reply_header,
                  reply_data);
    } catch (sqlite_error& e) {
        // TODO: log error, invalid query
        set_status(status_codes::INVALID_QUERY,
                   e.what(),
                   e.extended(),
                   reply_header);
        write_query_header_defaults(reply_header);
        return;
    }
    set_status(status_codes::OK, response_messages::OK, "", reply_header);
}

DBServer::endpoint_fn DBServer::identify_endpoint(const msgpack::object& request) {
    RequestData data(request.as<RequestData>());
    msgpack::object endpoint_name;
    try {
        endpoint_name = data.at("endpoint");
    } catch (std::out_of_range& e) {
        throw invalid_request("Missing endpoint name");
    }
    if (endpoint_name.type != msgpack::type::STR)
        throw invalid_request("Invalid endpoint name");

    DBServer::endpoint_fn endpoint;
    std::string name(endpoint_name.via.str.ptr, endpoint_name.via.str.size);
    try {
        endpoint = endpoints_[name];
    } catch (std::out_of_range& e) {
        throw invalid_request("Unknown endpoint specified");
    }
    return endpoint;
}

void DBServer::handle(int client_id, const byte_vec& input) {
    // std::cout << client_id << " Unpacking" << std::endl;
    // unpack incoming data
    msgpack::unpacked result;
    std::string str_input(input.begin(), input.end());
    msgpack::unpack(result, str_input.data(), input.size());
    msgpack::object request(result.get());

    // std::cout << client_id << " Added to request queue" << std::endl;
    request_queue_.push(ClientRequest{client_id, std::move(request)});
}

void DBServer::process_loop() {
    while(true) {
        ClientRequest request;
        // std::cout << "Waiting for request" << std::endl;
        request_queue_.wait_pop(request);
        // std::cout << "Got request" << std::endl;
        // prepare reply object
        msgpack::sbuffer header_buf;
        msgpack::sbuffer data_buf;
        Packer reply_header(&header_buf);
        Packer reply_data(&data_buf);
        // identify endpoint function based on request data
        endpoint_fn endpoint;
        try {
            endpoint = identify_endpoint(request.request);
        } catch (invalid_request& e) {
            set_status(status_codes::INVALID_REQUEST, e.what(), "", &reply_header);
        }
        // std::cout<< "endpoint identified" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        // get reply from endpoint function
        (this->*endpoint)(request.client_id, request.request, &reply_header, &reply_data);
        tcpserver::ClientResponse response;
        response.client_id = request.client_id;
        byte_vec &output = response.output;
        // write serialized reply data into output buffer
        output.insert(output.end(),
                      header_buf.data(),
                      header_buf.data() + header_buf.size());
        output.insert(output.end(),
                       data_buf.data(),
                       data_buf.data() + data_buf.size());
        // std::cout << request.client_id << " Adding response to queue";
        response_queue_.push(std::move(response));
    }
}

void DBServer::disconnected(int client_id) {
    connections_.erase(client_id);
}

}  // namespace sqlizator

