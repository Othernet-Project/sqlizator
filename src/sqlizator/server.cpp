// Copyright 2015, Outernet Inc.
// Some rights reserved.
// This software is free software licensed under the terms of GPLv3. See COPYING
// file that comes with the source code, or http://www.gnu.org/licenses/gpl.txt.
#include <msgpack.hpp>

#include <stdexcept>
#include <string>

#include "sqlizator/database.h"
#include "sqlizator/exceptions.h"
#include "sqlizator/response.h"
#include "sqlizator/server.h"

namespace sqlizator {

DBServer::DBServer(const std::string& conf_path,
                   const std::string& port): tcpserver::Server(port),
                                             config_(conf_path) {
    init_databases();
}

void DBServer::init_databases() {
    DBConfigMapIterators its(config_.databases());
    for (auto it = its.first; it != its.second; ++it) {
        std::string name(it->first);
        const DBConfig& db_conf = it->second;
        databases_.insert(std::make_pair(name, Database(db_conf.path)));
        Database& db = databases_.at(name);
        db.connect();
    }
}

void DBServer::deserialize(const std::string& src, MsgType* dest) {
    msgpack::unpacked result;
    msgpack::unpack(result, src.data(), src.size());
    msgpack::object obj(result.get());
    try {
        obj.convert(*dest);
    } catch (msgpack::type_error& e) {
        std::string msg("Deserialization failed: " + std::string(e.what()));
        throw deserialization_error(msg);
    }
}

void DBServer::add_header(int status,
                          const std::string& message,
                          Packer* result) {
    result->pack_map(2);
    result->pack(std::string("status"));
    result->pack(status);
    result->pack(std::string("message"));
    result->pack(message);
}

void DBServer::process(const byte_vec& input, Packer* result) {
    MsgType msg;
    std::string str(input.begin(), input.end());
    try {
        deserialize(str, &msg);
    } catch (deserialization_error& e) {
        // TODO: log error, message cannot be deserialized
        add_header(status_codes::DESERIALIZATION_ERROR, e.what(), result);
        return;
    }
    Database* db;
    try {
        db = &(databases_.at(msg.database));
    } catch (std::out_of_range& e) {
        // TODO: log error, invalid database name
        add_header(status_codes::DATABASE_NOT_FOUND, e.what(), result);
        return;
    }
    try {
        db->query(msg.operation, msg.query, msg.parameters, result);
    } catch (sqlite_error& e) {
        // TODO: log error, invalid query
        add_header(status_codes::INVALID_QUERY, e.what(), result);
        return;
    }
}

void DBServer::handle(const byte_vec& input, byte_vec* output) {
    msgpack::sbuffer buffer;
    Packer result(&buffer);
    process(input, &result);
    output->insert(output->end(), buffer.data(), buffer.data() + buffer.size());
}

}  // namespace sqlizator

