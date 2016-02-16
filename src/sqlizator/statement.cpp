// Copyright 2015, Outernet Inc.
// Some rights reserved.
// This software is free software licensed under the terms of GPLv3. See COPYING
// file that comes with the source code, or http://www.gnu.org/licenses/gpl.txt.
#include <stdint.h>

#include <sqlite3.h>
#include <msgpack.hpp>

#include <map>
#include <string>
#include <vector>

#include "sqlizator/exceptions.h"
#include "sqlizator/response.h"
#include "sqlizator/statement.h"

namespace sqlizator {

int Statement::bind_param(const msgpack::object& v, int pos) {
    switch (v.type) {
        case msgpack::type::NIL:
            return sqlite3_bind_null(statement_, pos);
        case msgpack::type::BOOLEAN:
            return sqlite3_bind_int(statement_, pos, v.via.boolean ? 1 : 0);
        case msgpack::type::POSITIVE_INTEGER:
            return sqlite3_bind_int64(statement_, pos, v.via.u64);
        case msgpack::type::NEGATIVE_INTEGER:
            return sqlite3_bind_int64(statement_, pos, v.via.i64);
        case msgpack::type::FLOAT:
            return sqlite3_bind_double(statement_, pos, v.via.f64);
        case msgpack::type::STR: {
            std::string value(v.via.str.ptr, v.via.str.size);
            return sqlite3_bind_text(statement_,
                                     pos,
                                     value.data(),
                                     value.size(),
                                     SQLITE_TRANSIENT);
        }
        case msgpack::type::BIN:
            return sqlite3_bind_blob(statement_,
                                     pos,
                                     v.via.bin.ptr,
                                     v.via.bin.size,
                                     SQLITE_TRANSIENT);
        case msgpack::type::EXT:
            return sqlite3_bind_blob(statement_,
                                     pos,
                                     v.via.ext.data(),
                                     v.via.ext.size,
                                     SQLITE_TRANSIENT);
        case msgpack::type::MAP:
        case msgpack::type::ARRAY:
            return SQLITE_ERROR;
    }
    return SQLITE_ERROR;
}

Statement::Statement(sqlite3* db,
                     const std::string& query,
                     const msgpack::object_handle& parameters):
                                                            db_(db),
                                                            statement_(NULL) {
    msgpack::object obj(parameters.get());
    int ret = sqlite3_prepare_v2(db_,
                                 query.data(),
                                 static_cast<int>(query.size()),
                                 &statement_,
                                 NULL);
    if (ret != SQLITE_OK)
        throw sqlite_error(sqlite3_errstr(ret));

    uint64_t param_count = sqlite3_bind_parameter_count(statement_);
    if (obj.type == msgpack::type::ARRAY) {
        if (obj.via.array.size != param_count)
            throw sqlite_error("Number of passed parameters does not match "
                               "number of required parameters.");
        for (uint64_t i = 0; i < param_count; i++) {
            int rc = bind_param(obj.via.array.ptr[i], i + 1);
            if (rc != SQLITE_OK)
                throw sqlite_error(sqlite3_errstr(rc));
        }
    } else {
        std::map<std::string, msgpack::object> unpacked;
        for (uint64_t i = 0; i < obj.via.map.size; i++) {
            msgpack::object_kv p_mo(obj.via.map.ptr[i]);
            std::string key(p_mo.key.via.str.ptr, p_mo.key.via.str.size);
            unpacked.insert(std::make_pair(key, p_mo.val));
        }
        for (uint64_t i = 0; i < param_count; i++) {
            std::string binding_name(sqlite3_bind_parameter_name(statement_, i + 1));
            binding_name.erase(0, 1);  // remove first `:` character
            msgpack::object obj;
            try {
                obj = unpacked.at(binding_name);
            } catch (std::out_of_range& e) {
                throw sqlite_error("Binding parameters to statement failed. "
                                   "Missing key: " + binding_name);
            }
            int rc = bind_param(obj, i + 1);
            if (rc != SQLITE_OK)
                throw sqlite_error(sqlite3_errstr(rc));
        }
    }
}

Statement::~Statement() {
    sqlite3_finalize(statement_);
}

void Statement::fetch_into(Packer* packer) {
    int col_count = sqlite3_data_count(statement_);
    packer->pack_map(col_count);
    for (int i = 0; i < col_count; ++i) {
        int col_type = sqlite3_column_type(statement_, i);
        packer->pack(sqlite3_column_name(statement_, i));
        if (col_type == SQLITE_NULL) {
            packer->pack(NULL);
        } else if (col_type == SQLITE_INTEGER) {
            packer->pack(sqlite3_column_int64(statement_, i));
        } else if (col_type == SQLITE_FLOAT) {
            packer->pack(sqlite3_column_double(statement_, i));
        } else if (col_type == SQLITE_TEXT) {
            ssize_t size = sqlite3_column_bytes(statement_, i);
            const unsigned char* text = sqlite3_column_text(statement_, i);
            packer->pack(std::string(reinterpret_cast<const char*>(text), size));
        } else {
            ssize_t size = sqlite3_column_bytes(statement_, i);
            const void* blob = sqlite3_column_blob(statement_, i);
            const char* data = static_cast<const char*>(blob);
            packer->pack(std::vector<unsigned char>(data, data + size));
        }
    }
}

uint64_t Statement::execute(Packer* packer, bool collect_result) {
    uint64_t row_count = 0;
    while (true) {
        int ret = sqlite3_step(statement_);
        if (ret == SQLITE_DONE) {
            return row_count;
        } else if (ret == SQLITE_ROW) {
            row_count += 1;
            if (collect_result)
                fetch_into(packer);
        } else {
            throw sqlite_error(sqlite3_errstr(ret));
        }
    }
}

}  // namespace sqlizator
