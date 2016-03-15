// Copyright 2015, Outernet Inc.
// Some rights reserved.
// This software is free software licensed under the terms of GPLv3. See COPYING
// file that comes with the source code, or http://www.gnu.org/licenses/gpl.txt.
#ifndef SQLIZATOR_SQLIZATOR_STATEMENT_H_
#define SQLIZATOR_SQLIZATOR_STATEMENT_H_
#include <stdint.h>

#include <sqlite3.h>
#include <msgpack.hpp>

#include <string>

#include "sqlizator/response.h"

namespace sqlizator {

class Statement {
 private:
    sqlite3* db_;
    sqlite3_stmt* statement_;

    int bind_param(const msgpack::object& v, int pos);
    void add_meta_info(Packer* packer);
    void fetch_into(Packer* packer);
 public:
    explicit Statement(sqlite3* db,
                       const std::string& query,
                       const msgpack::object_handle& parameters);
    ~Statement();
    uint64_t execute(Packer* packer, bool collect_result);
};

}  // namespace sqlizator
#endif  // SQLIZATOR_SQLIZATOR_STATEMENT_H_
