// Copyright 2015, Outernet Inc.
// Some rights reserved.
// This software is free software licensed under the terms of GPLv3. See COPYING
// file that comes with the source code, or http://www.gnu.org/licenses/gpl.txt.
#include <sqlite3.h>
#include <msgpack.hpp>

#include <string>
#include <iostream>

#include "sqlizator/database.h"
#include "sqlizator/exceptions.h"
#include "sqlizator/response.h"
#include "sqlizator/statement.h"

namespace sqlizator {

Database::Database(const std::string& path): path_(path) {}

Database::~Database() {
    sqlite3_close(db_);
}

void Database::query(Operation operation,
                     const std::string& query,
                     const msgpack::object_handle& parameters,
                     Packer* into) {
    Statement stmt(db_, query, parameters);
    bool collect_result = (operation == Operation::EXECUTE_AND_FETCH);
    stmt.execute(into, collect_result);
}

void trace_callback(void* udp, const char* sql) {
    std::cout << "[SQL] " << sql << std::endl;
}

void Database::connect() {
    int result = sqlite3_open(path_.c_str(), &db_);
    if (result != SQLITE_OK) {
        std::string msg(sqlite3_errmsg(db_));
        throw sqlite_error(msg);
    }
    sqlite3_trace(db_, trace_callback, NULL);
}

void Database::close() {
    sqlite3_close(db_);
}

std::string Database::path() {
    return path_;
}

}  // namespace sqlizator
