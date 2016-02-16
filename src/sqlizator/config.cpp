// Copyright 2015, Outernet Inc.
// Some rights reserved.
// This software is free software licensed under the terms of GPLv3. See COPYING
// file that comes with the source code, or http://www.gnu.org/licenses/gpl.txt.
#include <yaml-cpp/yaml.h>

#include <string>
#include <utility>

#include "sqlizator/config.h"

namespace sqlizator {

ConfigReader::ConfigReader(const std::string& conf_path) {
    YAML::Node config = YAML::LoadFile(conf_path);
    for (auto it = config.begin(); it != config.end(); ++it) {
        std::string section(it->first.as<std::string>());
        YAML::Node data = it->second;
        if (section == "databases") {
            for (auto jt = data.begin(); jt != data.end(); ++jt) {
                YAML::Node raw_dbconf = *jt;
                std::string name(raw_dbconf["name"].as<std::string>());
                DBConfig cfg;
                cfg.path = raw_dbconf["path"].as<std::string>();
                db_configs_.insert(std::make_pair(name, cfg));
            }
        }
    }
}

const DBConfig& ConfigReader::database(const std::string& key) {
    return db_configs_.at(key);
}

const DBConfigMapIterators ConfigReader::databases() {
    return std::make_pair(db_configs_.begin(), db_configs_.end());
}

}  // namespace sqlizator
