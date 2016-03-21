// Copyright 2015, Outernet Inc.
// Some rights reserved.
// This software is free software licensed under the terms of GPLv3. See COPYING
// file that comes with the source code, or http://www.gnu.org/licenses/gpl.txt.
#include <msgpack.hpp>

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
                          Packer* reply_header) {
    reply_header->pack(std::string("status"));
    reply_header->pack(status);
    reply_header->pack(std::string("message"));
    reply_header->pack(message);
}

void DBServer::endpoint_connect(const msgpack::object& request,
                                Packer* reply_header,
                                Packer* reply_data) {
    reply_header->pack_map(header_sizes::CONNECT);
    std::map<std::string, std::string> msg;
    try {
        request.convert(msg);
    } catch (msgpack::type_error& e) {
        // TODO: log error, message cannot be deserialized
        set_status(status_codes::DESERIALIZATION_ERROR, e.what(), reply_header);
        return;
    }
    std::string name;
    std::string path;
    try {
        name = msg["database"];
        path = msg["path"];
    } catch (std::out_of_range& e) {
        set_status(status_codes::INVALID_REQUEST,
                   "Missing database name or path.",
                   reply_header);
        return;
    }
    // check if it's already connected to the database maybe
    if (!databases_.count(name)) {
        // no connection exists yet
        databases_.insert(std::make_pair(name,
                                         std::shared_ptr<Database>(new Database(path))));
        Database& db = *databases_.at(name);
        try {
            db.connect();
        } catch (sqlite_error& e) {
            set_status(status_codes::DATABASE_OPENING_ERROR, e.what(), reply_header);
            return;
        }
        // apply passed in pragmas to newly opened database
        for (auto it = msg.begin(); it != msg.end(); ++it) {
            if (std::find(std::begin(PRAGMAS),
                          std::end(PRAGMAS),
                          it->first) != std::end(PRAGMAS))
                db.pragma("PRAGMA " + it->first + "=" + it->second + ";");
        }
    } else {
        // already connected to a database with that name
        Database& db = *databases_.at(name);
        // in case the database was already open, verify that the passed in path
        // matches the path of the already open database. in case it doesn't, the
        // same name was used for two different databases, which is unacceptable
        if (db.path() != path) {
            set_status(status_codes::INVALID_REQUEST,
                       "Database name already in use.",
                       reply_header);
            return;
        }
    }
    set_status(status_codes::OK, response_messages::OK, reply_header);
}

void DBServer::endpoint_drop(const msgpack::object& request,
                             Packer* reply_header,
                             Packer* reply_data) {
    reply_header->pack_map(header_sizes::DROP);
    std::map<std::string, std::string> msg;
    try {
        request.convert(msg);
    } catch (msgpack::type_error& e) {
        // TODO: log error, message cannot be deserialized
        set_status(status_codes::DESERIALIZATION_ERROR, e.what(), reply_header);
        return;
    }
    std::string name;
    std::string path;
    try {
        name = msg["database"];
        path = msg["path"];
    } catch (std::out_of_range& e) {
        set_status(status_codes::INVALID_REQUEST,
                   "Missing database name or path.",
                   reply_header);
        return;
    }
    if (!databases_.count(name)) {
        set_status(status_codes::INVALID_REQUEST,
                   "Database name not found.",
                   reply_header);
        return;
    }
    Database& db = *databases_.at(name);
    if (db.path() != path) {
        set_status(status_codes::INVALID_REQUEST,
                   "Database paths do not match.",
                   reply_header);
        return;
    }
    db.close();
    std::remove(path.c_str());
    set_status(status_codes::OK, response_messages::OK, reply_header);
}

void DBServer::write_query_header_defaults(Packer* reply_header) {
    reply_header->pack(std::string("rowcount"));
    reply_header->pack(-1);
    reply_header->pack(std::string("columns"));
    reply_header->pack_nil();
}

void DBServer::endpoint_query(const msgpack::object& request,
                              Packer* reply_header,
                              Packer* reply_data) {
    reply_header->pack_map(header_sizes::QUERY);
    MsgType msg;
    try {
        request.convert(msg);
    } catch (msgpack::type_error& e) {
        // TODO: log error, message cannot be deserialized
        set_status(status_codes::DESERIALIZATION_ERROR, e.what(), reply_header);
        write_query_header_defaults(reply_header);
        return;
    }
    if (!databases_.count(msg.database)) {
        set_status(status_codes::DATABASE_NOT_FOUND,
                   "Database not found.",
                   reply_header);
        return;
    }
    Database& db = *databases_.at(msg.database);
    try {
        db.query(msg.operation,
                 msg.query,
                 msg.parameters,
                 reply_header,
                 reply_data);
    } catch (sqlite_error& e) {
        // TODO: log error, invalid query
        set_status(status_codes::INVALID_QUERY, e.what(), reply_header);
        write_query_header_defaults(reply_header);
        return;
    }
    set_status(status_codes::OK, response_messages::OK, reply_header);
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

void DBServer::handle(const byte_vec& input, byte_vec* output) {
    // unpack incoming data
    msgpack::unpacked result;
    std::string str_input(input.begin(), input.end());
    msgpack::unpack(result, str_input.data(), input.size());
    msgpack::object request(result.get());
    // prepare reply object
    msgpack::sbuffer header_buf;
    msgpack::sbuffer data_buf;
    Packer reply_header(&header_buf);
    Packer reply_data(&data_buf);
    // identify endpoint function based on request data
    endpoint_fn endpoint;
    try {
        endpoint = identify_endpoint(request);
    } catch (invalid_request& e) {
        set_status(status_codes::INVALID_REQUEST, e.what(), &reply_header);
    }
    // get reply from endpoint function
    (this->*endpoint)(request, &reply_header, &reply_data);
    // write serialized reply data into output buffer
    output->insert(output->end(),
                   header_buf.data(),
                   header_buf.data() + header_buf.size());
    output->insert(output->end(),
                   data_buf.data(),
                   data_buf.data() + data_buf.size());
}

}  // namespace sqlizator

