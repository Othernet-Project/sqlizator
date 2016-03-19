// Copyright 2015, Outernet Inc.
// Some rights reserved.
// This software is free software licensed under the terms of GPLv3. See COPYING
// file that comes with the source code, or http://www.gnu.org/licenses/gpl.txt.
#ifndef SQLIZATOR_SQLIZATOR_SERVER_H_
#define SQLIZATOR_SQLIZATOR_SERVER_H_
#include <map>
#include <memory>
#include <string>

#include "sqlizator/database.h"
#include "sqlizator/response.h"
#include "tcpserver/server.h"

namespace sqlizator {

using tcpserver::byte_vec;
typedef std::map<std::string, std::shared_ptr<Database>> DBContainer;
typedef std::map<std::string, msgpack::object> RequestData;

struct MsgType {
    std::string database;
    std::string query;
    Operation operation;
    msgpack::object_handle parameters;
};

class DBServer: public tcpserver::Server {
 private:
    typedef void (DBServer::*endpoint_fn)(const msgpack::object& request,
                                          Packer* reply_header,
                                          Packer* reply_data);
    typedef std::map<std::string, endpoint_fn> EndpointMap;
    DBContainer databases_;
    EndpointMap endpoints_;

    void set_status(int status, const std::string& message, Packer* reply_header);
    void endpoint_connect(const msgpack::object& request,
                          Packer* reply_header,
                          Packer* reply_data);
    void endpoint_drop(const msgpack::object& request,
                       Packer* reply_header,
                       Packer* reply_data);
    void endpoint_query(const msgpack::object& request,
                        Packer* reply_header,
                        Packer* reply_data);
    endpoint_fn identify_endpoint(const msgpack::object& request);
    virtual void handle(const byte_vec& input, byte_vec* output);

 public:
    explicit DBServer(const std::string& port);
};

}  // namespace sqlizator


namespace msgpack {
MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS) {
namespace adaptor {

template<>
struct convert<sqlizator::MsgType> {
    msgpack::object const& operator()(msgpack::object const& o,
                                      sqlizator::MsgType& v) const {
        if (o.type != msgpack::type::MAP)
            throw msgpack::type_error();

        msgpack::object_kv* p_mo(o.via.map.ptr);
        msgpack::object_kv* const p_mo_end(p_mo + o.via.map.size);
        for (; p_mo < p_mo_end; ++p_mo) {
            if (p_mo->key.type != msgpack::type::STR)
                throw msgpack::type_error();

            std::string key(p_mo->key.via.str.ptr, p_mo->key.via.str.size);
            if (key == "database") {
                if (p_mo->val.type != msgpack::type::STR)
                    throw msgpack::type_error();
                v.database = std::string(p_mo->val.via.str.ptr, p_mo->val.via.str.size);
            } else if (key == "query") {
                if (p_mo->val.type != msgpack::type::STR)
                    throw msgpack::type_error();
                v.query = std::string(p_mo->val.via.str.ptr, p_mo->val.via.str.size);
            } else if (key == "operation") {
                if (p_mo->val.type != msgpack::type::POSITIVE_INTEGER)
                    throw msgpack::type_error();
                v.operation = static_cast<sqlizator::Operation>(p_mo->val.via.u64);
            } else if (key == "parameters") {
                if (p_mo->val.type != msgpack::type::ARRAY &&
                        p_mo->val.type != msgpack::type::MAP)
                    throw msgpack::type_error();
                v.parameters = msgpack::clone(p_mo->val);
            }
        }
        return o;
    }
};

} // namespace adaptor
} // MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS)
} // namespace msgpack

#endif  // SQLIZATOR_SQLIZATOR_SERVER_H_
