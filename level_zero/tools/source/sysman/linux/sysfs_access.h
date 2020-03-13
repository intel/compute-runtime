/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

#include <fstream>
#include <iostream>
#include <list>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

namespace L0 {

class SysfsAccess {
  public:
    static SysfsAccess *create(const int fd);
    static SysfsAccess *create(const std::string path);

    ze_result_t canRead(const std::string file);
    ze_result_t canWrite(const std::string file);

    ze_result_t read(const std::string file, std::string &val) { return this->readAsString(file, val); }
    ze_result_t read(const std::string file, std::vector<std::string> &val) { return this->readAsLines(file, val); }
    ze_result_t read(const std::string file, int &val);
    ze_result_t read(const std::string file, double &val);

    ze_result_t write(const std::string file, const std::string val) { return this->writeAsString(file, val); }
    ze_result_t write(const std::string file, const int val);
    ze_result_t write(const std::string file, const double val);
    ze_result_t readSymLink(const std::string path, std::string &buf);
    ze_bool_t fileExists(const std::string file);

  private:
    ze_result_t readAsString(const std::string file, std::string &val);
    ze_result_t readAsLines(const std::string file, std::vector<std::string> &val);
    ze_result_t writeAsString(const std::string file, const std::string val);
    std::string fullPath(const std::string file);

    SysfsAccess(const int maj, const int min);
    SysfsAccess(const std::string path);

    std::string dirname;
    static const std::string charDevPath;
};

} // namespace L0
