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
        throw sqlite_error(sqlite3_errstr(ret), sqlite3_errmsg(db_));

    uint64_t param_count = sqlite3_bind_parameter_count(statement_);
    if (obj.type == msgpack::type::ARRAY) {
        if (obj.via.array.size != param_count)
            throw sqlite_error("Parameter binding failed.",
                               "Number of passed parameters does not match "
                               "number of required parameters.");
        for (uint64_t i = 0; i < param_count; i++) {
            int rc = bind_param(obj.via.array.ptr[i], i + 1);
            if (rc != SQLITE_OK)
                throw sqlite_error(sqlite3_errstr(rc), sqlite3_errmsg(db_));
        }
    } else {
        std::map<std::string, msgpack::object> unpacked;
        try {
            obj.convert(unpacked);
        } catch (msgpack::type_error& e) {
            throw sqlite_error("Parameter binding failed.",
                               "Binding parameters to statement failed. "
                               "Invalid map.");
        }
        for (uint64_t i = 0; i < param_count; i++) {
            std::string binding_name(sqlite3_bind_parameter_name(statement_, i + 1));
            binding_name.erase(0, 1);  // remove first `:` character
            msgpack::object value;
            try {
                value = unpacked.at(binding_name);
            } catch (std::out_of_range& e) {
                throw sqlite_error("Parameter binding failed.",
                                   "Binding parameters to statement failed. "
                                   "Missing key: " + binding_name);
            }
            int rc = bind_param(value, i + 1);
            if (rc != SQLITE_OK)
                throw sqlite_error(sqlite3_errstr(rc), sqlite3_errmsg(db_));
        }
    }
}

Statement::~Statement() {
    sqlite3_finalize(statement_);
}

void Statement::add_columns_meta_info(Packer* packer) {
    int col_count = sqlite3_column_count(statement_);
    if (col_count == 0)
        return;

    packer->pack("columns");
    packer->pack_array(col_count);
    for (int i = 0; i < col_count; ++i) {
        const char* col_name = sqlite3_column_name(statement_, i);
        const char* col_type = sqlite3_column_decltype(statement_, i);
        if (col_type != NULL)
            packer->pack(std::make_tuple(col_name, col_type));
        else
            packer->pack(std::make_tuple(col_name, msgpack::type::nil_t()));
    }
}

void Statement::fetch_into(Packer* packer) {
    int col_count = sqlite3_data_count(statement_);
    packer->pack_array(col_count);
    for (int i = 0; i < col_count; ++i) {
        int col_type = sqlite3_column_type(statement_, i);
        if (col_type == SQLITE_NULL) {
            packer->pack_nil();
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

uint64_t Statement::execute(Packer* header, Packer* data, bool collect_result) {
    add_columns_meta_info(header);
    uint64_t rowcount = 0;
    while (true) {
        int ret = sqlite3_step(statement_);
        if (ret == SQLITE_DONE) {
            header->pack("rowcount");
            if (sqlite3_stmt_readonly(statement_))
                rowcount = sqlite3_changes(db_);
            header->pack(rowcount);
            return rowcount;
        } else if (ret == SQLITE_ROW) {
            rowcount += 1;
            if (collect_result)
                fetch_into(data);
        } else {
            throw sqlite_error(sqlite3_errstr(ret), sqlite3_errmsg(db_));
        }
    }
}

}  // namespace sqlizator
