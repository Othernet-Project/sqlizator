// Copyright 2015, Outernet Inc.
// Some rights reserved.
// This software is free software licensed under the terms of GPLv3. See COPYING
// file that comes with the source code, or http://www.gnu.org/licenses/gpl.txt.
#ifndef SQLIZATOR_SQLIZATOR_DATABASE_H_
#define SQLIZATOR_SQLIZATOR_DATABASE_H_
#include <sqlite3.h>
#include <msgpack.hpp>

#include <string>
#include <vector>

#include "sqlizator/response.h"

namespace sqlizator {

enum Operation {
    EXECUTE = 1,
    EXECUTE_AND_FETCH = 2
};

static const int DEFAULT_MAX_RETRY = 100;
static const int DEFAULT_SLEEP_MS = 100;

struct BHData {
	int max_retry;
	int sleep_ms;
    explicit BHData(int max_retry, int sleep_ms): max_retry(max_retry),
                                                  sleep_ms(sleep_ms) {}
};

const std::vector<std::string> PRAGMAS{"journal_mode", "foreign_keys"};

class Database {
 private:
    sqlite3* db_;
    std::string path_;
    int max_retry_;
    int sleep_ms_;
 public:
    explicit Database(const std::string& path,
                      int max_retry = DEFAULT_MAX_RETRY,
                      int sleep_ms = DEFAULT_SLEEP_MS);
    ~Database();
    void connect();
    void close();
    void pragma(const std::string& key, const std::string& value);
    void query(Operation operation,
               const std::string& query,
               const msgpack::object_handle& parameters,
               Packer* header,
               Packer* data);
    std::string path();
};

}  // namespace sqlizator
#endif  // SQLIZATOR_SQLIZATOR_DATABASE_H_
