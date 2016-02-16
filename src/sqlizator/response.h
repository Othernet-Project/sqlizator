// Copyright 2015, Outernet Inc.
// Some rights reserved.
// This software is free software licensed under the terms of GPLv3. See COPYING
// file that comes with the source code, or http://www.gnu.org/licenses/gpl.txt.
#ifndef SQLIZATOR_SQLIZATOR_ERROR_CODES_H_
#define SQLIZATOR_SQLIZATOR_ERROR_CODES_H_
#include <msgpack.hpp>

#include <stdexcept>
#include <string>

namespace sqlizator {

typedef msgpack::packer<msgpack::sbuffer> Packer;

namespace status_codes {

static const int OK = 0;
static const int UNKNOWN_ERROR = 1;
static const int DESERIALIZATION_ERROR = 2;
static const int BAD_MESSAGE = 3;
static const int DATABASE_NOT_FOUND = 4;
static const int INVALID_QUERY = 5;

}  // namespace status_codes

}  // namespace sqlizator
#endif  // SQLIZATOR_SQLIZATOR_ERROR_CODES_H_

