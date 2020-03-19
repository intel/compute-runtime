/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/ze_api.h"
#include "level_zero/zet_api.h"

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

class FsAccess {
  public:
    static FsAccess *create();
    ~FsAccess() = default;

    ze_result_t canRead(const std::string file);
    ze_result_t canWrite(const std::string file);

    ze_result_t read(const std::string file, std::string &val);
    ze_result_t read(const std::string file, std::vector<std::string> &val);

    ze_result_t write(const std::string file, const std::string val);

    ze_result_t readSymLink(const std::string path, std::string &buf);
    ze_result_t getRealPath(const std::string path, std::string &buf);
    ze_result_t listDirectory(const std::string path, std::vector<std::string> &list);
    std::string getBaseName(const std::string path);
    std::string getDirName(const std::string path);
    ze_bool_t fileExists(const std::string file);

  protected:
    FsAccess();
};

class ProcfsAccess : private FsAccess {
  public:
    static ProcfsAccess *create();
    ~ProcfsAccess() = default;

    ze_result_t listProcesses(std::vector<::pid_t> &list);
    ::pid_t myProcessId();
    ze_result_t getFileDescriptors(const ::pid_t pid, std::vector<int> &list);
    ze_result_t getFileName(const ::pid_t pid, const int fd, std::string &val);

  private:
    ProcfsAccess() = default;

    std::string fullPath(const ::pid_t pid);
    std::string fdDirPath(const ::pid_t pid);
    std::string fullFdPath(const ::pid_t pid, const int fd);
    static const std::string procDir;
    static const std::string fdDir;
};

class SysfsAccess : private FsAccess {
  public:
    static SysfsAccess *create(const std::string file);
    ~SysfsAccess() = default;

    ze_result_t canRead(const std::string file);
    ze_result_t canWrite(const std::string file);

    ze_result_t read(const std::string file, std::string &val);
    ze_result_t read(const std::string file, int &val);
    ze_result_t read(const std::string file, double &val);
    ze_result_t read(const std::string file, std::vector<std::string> &val);

    ze_result_t write(const std::string file, const std::string val);
    ze_result_t write(const std::string file, const int val);
    ze_result_t write(const std::string file, const double val);

    ze_result_t readSymLink(const std::string path, std::string &buf);
    ze_result_t getRealPath(const std::string path, std::string &buf);
    ze_result_t bindDevice(const std::string device);
    ze_result_t unbindDevice(const std::string device);
    ze_bool_t fileExists(const std::string file);
    ze_bool_t isMyDeviceFile(const std::string dev);

  private:
    SysfsAccess() = delete;
    SysfsAccess(const std::string file);

    std::string fullPath(const std::string file);

    std::vector<std::string> deviceNames;
    std::string dirname;
    static const std::string drmPath;
    static const std::string devicesPath;
    static const std::string primaryDevName;
    static const std::string drmDriverDevNodeDir;
    static const std::string intelGpuBindEntry;
    static const std::string intelGpuUnbindEntry;
};

} // namespace L0
