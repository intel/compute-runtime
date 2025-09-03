/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include <level_zero/zes_api.h>

#include <map>
#include <memory>
#include <mutex>
#include <vector>

namespace L0 {
namespace Sysman {

class FdCacheInterface : public NEO::NonCopyableAndNonMovableClass {
  public:
    FdCacheInterface() = default;
    ~FdCacheInterface();

    static const int maxSize = 10;
    int getFd(std::string file);

  protected:
    // Map of File name to pair of file descriptor and reference count to file.
    std::map<std::string, std::pair<int, uint32_t>> fdMap = {};

  private:
    void eraseLeastUsedEntryFromCache();
};

class FsAccessInterface {
  public:
    static std::unique_ptr<FsAccessInterface> create();
    virtual ~FsAccessInterface();

    virtual ze_result_t canRead(const std::string file);
    virtual ze_result_t canWrite(const std::string file);
    virtual ze_result_t getFileMode(const std::string file, ::mode_t &mode);

    virtual ze_result_t read(const std::string file, uint64_t &val);
    virtual ze_result_t read(const std::string file, std::string &val);
    virtual ze_result_t read(const std::string file, std::vector<std::string> &val);
    virtual ze_result_t read(const std::string file, double &val);
    virtual ze_result_t read(const std::string file, uint32_t &val);
    virtual ze_result_t read(const std::string file, int32_t &val);

    virtual ze_result_t write(const std::string file, const std::string val);

    virtual ze_result_t readSymLink(const std::string path, std::string &buf);
    virtual ze_result_t getRealPath(const std::string path, std::string &buf);
    virtual ze_result_t listDirectory(const std::string path, std::vector<std::string> &list);
    virtual bool isRootUser();
    std::string getBaseName(const std::string path);
    std::string getDirName(const std::string path);
    virtual bool fileExists(const std::string file);
    virtual bool directoryExists(const std::string path);

  protected:
    FsAccessInterface();
    MOCKABLE_VIRTUAL std::unique_lock<std::mutex> obtainMutex();

  private:
    template <typename T>
    ze_result_t readValue(const std::string file, T &val);
    std::unique_ptr<FdCacheInterface> pFdCacheInterface = nullptr;
    std::mutex fsMutex{};
};

class ProcFsAccessInterface : private FsAccessInterface {
  public:
    static std::unique_ptr<ProcFsAccessInterface> create();
    ~ProcFsAccessInterface() override;

    MOCKABLE_VIRTUAL ze_result_t listProcesses(std::vector<::pid_t> &list);
    MOCKABLE_VIRTUAL ::pid_t myProcessId();
    MOCKABLE_VIRTUAL ze_result_t getFileDescriptors(const ::pid_t pid, std::vector<int> &list);
    MOCKABLE_VIRTUAL ze_result_t getFileName(const ::pid_t pid, const int fd, std::string &val);
    MOCKABLE_VIRTUAL bool isAlive(const ::pid_t pid);
    MOCKABLE_VIRTUAL void kill(const ::pid_t pid);

  protected:
    ProcFsAccessInterface();

  private:
    std::string fullPath(const ::pid_t pid);
    std::string fdDirPath(const ::pid_t pid);
    std::string fullFdPath(const ::pid_t pid, const int fd);
    static const std::string procDir;
    static const std::string fdDir;
};

class SysFsAccessInterface : protected FsAccessInterface {
  public:
    static std::unique_ptr<SysFsAccessInterface> create(const std::string file);
    ~SysFsAccessInterface() override;

    ze_result_t canRead(const std::string file) override;
    ze_result_t canWrite(const std::string file) override;
    ze_result_t getFileMode(const std::string file, ::mode_t &mode) override;

    ze_result_t read(const std::string file, std::string &val) override;
    ze_result_t read(const std::string file, int32_t &val) override;
    ze_result_t read(const std::string file, uint32_t &val) override;
    ze_result_t read(const std::string file, uint64_t &val) override;
    ze_result_t read(const std::string file, double &val) override;
    ze_result_t read(const std::string file, std::vector<std::string> &val) override;

    ze_result_t write(const std::string file, const std::string val) override;
    MOCKABLE_VIRTUAL ze_result_t write(const std::string file, const int val);
    MOCKABLE_VIRTUAL ze_result_t write(const std::string file, const uint64_t val);
    MOCKABLE_VIRTUAL ze_result_t write(const std::string file, const double val);
    ze_result_t write(const std::string file, std::vector<std::string> val);

    MOCKABLE_VIRTUAL ze_result_t scanDirEntries(const std::string path, std::vector<std::string> &list);
    ze_result_t readSymLink(const std::string path, std::string &buf) override;
    ze_result_t getRealPath(const std::string path, std::string &buf) override;
    MOCKABLE_VIRTUAL ze_result_t bindDevice(const std::string &gpuBindEntry, const std::string &device);
    MOCKABLE_VIRTUAL ze_result_t unbindDevice(const std::string &gpuUnbindEntry, const std::string &device);
    bool fileExists(const std::string file) override;
    MOCKABLE_VIRTUAL bool isMyDeviceFile(const std::string dev);
    bool directoryExists(const std::string path) override;
    bool isRootUser() override;

  protected:
    SysFsAccessInterface();
    SysFsAccessInterface(const std::string file);
    std::vector<std::string> deviceNames;

  private:
    std::string fullPath(const std::string file);
    std::string dirname;
    static const std::string drmPath;
    static const std::string devicesPath;
    static const std::string primaryDevName;
    static const std::string drmDriverDevNodeDir;
};

} // namespace Sysman
} // namespace L0
