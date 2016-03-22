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

Database::Database(const std::string& path,
                   int max_retry,
                   int sleep_ms): path_(path),
                                  max_retry_(max_retry),
                                  sleep_ms_(sleep_ms) {}

Database::~Database() {
    sqlite3_close(db_);
}


int pragma_callback(void *, int, char **, char **) {
    return 0;
}

int busy_handler(void *data, int retry) {
	BHData* bh_data = reinterpret_cast<BHData*>(data);
	if (retry < bh_data->max_retry) {
		sqlite3_sleep(bh_data->sleep_ms);
		return 1;
	}
    // exceeded max retry attempts, let it go, let it go
	return 0;
}

void Database::pragma(const std::string& key, const std::string& value) {
    std::string query("PRAGMA " + key + "=" + value + ";");
    int ret = sqlite3_exec(db_, query.data(), pragma_callback, 0, NULL);
    if (ret != SQLITE_OK) {
        throw sqlite_error(sqlite3_errstr(ret), sqlite3_errmsg(db_));
    }
}

void Database::query(Operation operation,
                     const std::string& query,
                     const msgpack::object_handle& parameters,
                     Packer* header,
                     Packer* data) {
    Statement stmt(db_, query, parameters);
    bool collect_result = (operation == Operation::EXECUTE_AND_FETCH);
    stmt.execute(header, data, collect_result);
}

void trace_callback(void* udp, const char* sql) {
    std::cout << "[SQL] " << sql << std::endl;
}

void Database::connect() {
    int ret = sqlite3_open(path_.c_str(), &db_);
    if (ret != SQLITE_OK) {
        throw sqlite_error(sqlite3_errstr(ret), sqlite3_errmsg(db_));
    }
    BHData bh_data(max_retry_, sleep_ms_);
    sqlite3_busy_handler(db_, busy_handler, &bh_data);
    sqlite3_trace(db_, trace_callback, NULL);
}

void Database::close() {
    sqlite3_close(db_);
}

std::string Database::path() {
    return path_;
}

}  // namespace sqlizator
