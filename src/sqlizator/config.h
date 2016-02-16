// Copyright 2015, Outernet Inc.
// Some rights reserved.
// This software is free software licensed under the terms of GPLv3. See COPYING
// file that comes with the source code, or http://www.gnu.org/licenses/gpl.txt.
#ifndef SQLIZATOR_SQLIZATOR_CONFIG_H_
#define SQLIZATOR_SQLIZATOR_CONFIG_H_
#include <map>
#include <string>
#include <utility>

namespace sqlizator {

struct DBConfig {
    std::string path;
};
typedef std::map<std::string, DBConfig> DBConfigMap;
typedef std::pair<DBConfigMap::const_iterator,
                  DBConfigMap::const_iterator> DBConfigMapIterators;

class ConfigReader {
 private:
    DBConfigMap db_configs_;

 public:
    explicit ConfigReader(const std::string& conf_path);
    const DBConfig& database(const std::string& key);
    const DBConfigMapIterators databases();
};

}  // namespace sqlizator
#endif  // SQLIZATOR_SQLIZATOR_CONFIG_H_

