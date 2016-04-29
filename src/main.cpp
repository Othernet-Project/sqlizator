// Copyright 2015, Outernet Inc.
// Some rights reserved.
// This software is free software licensed under the terms of GPLv3. See COPYING
// file that comes with the source code, or http://www.gnu.org/licenses/gpl.txt.
#include <array>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "sqlizator/server.h"

typedef std::map<std::string, std::string> ConfMap;

static const int OPTION_COUNT = 4;
static const std::array<std::string, OPTION_COUNT> OPTIONS = {
    "port"
};
static const int DEFAULT_PORT = 8080;

void print_usage() {
    std::cerr << "Usage: sqlizator "
              << "[--port NUMBER] "
              << std::endl;
}

bool parse_args(int argc, char* argv[], ConfMap* into) {
    for (int i = 1; i < argc; i += 2) {
        bool matched = false;
        for (auto it = OPTIONS.begin(); it != OPTIONS.end(); ++it) {
            if (std::string(argv[i]) == "--" + *it) {
                if (i + 1 < argc) {
                    (*into)[*it] = std::string(argv[i + 1]);
                    matched = true;
                } else {
                    std::cerr << "--" + *it + " option requires one argument."
                              << std::endl;
                    return false;
                }
            }
        }
        if (!matched) {
            print_usage();
            return false;
        }
    }
    return true;
}

int main(int argc, char* argv[]) {
    // prepare default options
    int port = DEFAULT_PORT;
    // parse command line args
    ConfMap args;
    if (!parse_args(argc, argv, &args))
        return 1;
    // override default options with defined command line arguments
    if (args.find("port") != args.end())
        port = std::stoi(args["port"]);

    sqlizator::DBServer srv(std::to_string(port));
    srv.start();
    srv.wait();
    return 0;
}

