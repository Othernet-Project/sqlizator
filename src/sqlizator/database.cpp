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
#include <iostream>
namespace sqlizator {

Database::Database(const std::string& path,
                   int max_retry,
                   int sleep_ms): path_(path),
                                  bh_data_(max_retry, sleep_ms),
                                  write_cursor_id_(-1) {}

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

void Database::query(int cursor_id,
                     Operation operation,
                     const std::string& query,
                     const msgpack::object_handle& parameters,
                     Packer* header,
                     Packer* data) {
    Statement stmt(db_, query, parameters);
    bool collect_result = (operation == Operation::EXECUTE_AND_FETCH);
    std::cout << cursor_id << " " << write_cursor_id_ << " " << stmt.sql << std::endl;
    if(write_cursor_id_ == -1) {
        if(stmt.is_readonly()) {
            stmt.execute(header, data, collect_result);
        } else if(stmt.is_begin()) {
            std::unique_lock<std::mutex> lock(write_mutex_);
            write_var_.wait(lock, [this] { return write_cursor_id_ == -1;  });
            write_cursor_id_ = cursor_id;
            stmt.execute(header, data, collect_result);
            lock.unlock();
            write_var_.notify_one();
        }  else {
            std::unique_lock<std::mutex> lock(write_mutex_);
            write_var_.wait(lock, [this, cursor_id] { return write_cursor_id_ == -1; });
            stmt.execute(header, data, collect_result);
            lock.unlock();
            write_var_.notify_one();
        }
    } else if (write_cursor_id_ == cursor_id) {
        if (stmt.is_commit() || stmt.is_rollback()) {
            std::unique_lock<std::mutex> lock(write_mutex_);
            write_var_.wait(lock, [this, cursor_id] { return write_cursor_id_ == cursor_id;  });
            stmt.execute(header, data, collect_result);
            write_cursor_id_ = -1;
            lock.unlock();
            write_var_.notify_one();
        } else {
            std::unique_lock<std::mutex> lock(write_mutex_);
            write_var_.wait(lock, [this, cursor_id] { return write_cursor_id_ == cursor_id;  });
            stmt.execute(header, data, collect_result);
            lock.unlock();
        }
    }
}

void trace_callback(void* udp, const char* sql) {
    std::cout << "[SQL] " << sql << std::endl;
}

void Database::connect() {
    int ret = sqlite3_open(path_.c_str(), &db_);
    if (ret != SQLITE_OK)
        throw sqlite_error(sqlite3_errstr(ret), sqlite3_errmsg(db_));

    sqlite3_busy_handler(db_, busy_handler, &bh_data_);
    sqlite3_trace(db_, trace_callback, NULL);
}

void Database::close() {
    sqlite3_close(db_);
}

std::string Database::path() {
    return path_;
}

}  // namespace sqlizator
