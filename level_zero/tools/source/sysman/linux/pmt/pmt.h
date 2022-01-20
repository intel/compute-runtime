/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/sysman/linux/fs_access.h"

#include <fcntl.h>
#include <map>
#include <sys/stat.h>
#include <sys/types.h>

namespace L0 {

class PlatformMonitoringTech : NEO::NonCopyableOrMovableClass {
  public:
    PlatformMonitoringTech() = delete;
    PlatformMonitoringTech(FsAccess *pFsAccess, ze_bool_t onSubdevice, uint32_t subdeviceId);
    virtual ~PlatformMonitoringTech();

    virtual ze_result_t readValue(const std::string key, uint32_t &value);
    virtual ze_result_t readValue(const std::string key, uint64_t &value);
    static ze_result_t enumerateRootTelemIndex(FsAccess *pFsAccess, std::string &rootPciPathOfGpuDevice);
    static void create(const std::vector<ze_device_handle_t> &deviceHandles,
                       FsAccess *pFsAccess, std::string &rootPciPathOfGpuDevice,
                       std::map<uint32_t, L0::PlatformMonitoringTech *> &mapOfSubDeviceIdToPmtObject);

  protected:
    static uint32_t rootDeviceTelemNodeIndex;
    std::string telemetryDeviceEntry{};
    std::map<std::string, uint64_t> keyOffsetMap;
    ze_result_t getKeyOffsetMap(std::string guid, std::map<std::string, uint64_t> &keyOffsetMap);
    ze_result_t init(FsAccess *pFsAccess, const std::string &rootPciPathOfGpuDevice);
    static void doInitPmtObject(FsAccess *pFsAccess, uint32_t subdeviceId, PlatformMonitoringTech *pPmt, const std::string &rootPciPathOfGpuDevice,
                                std::map<uint32_t, L0::PlatformMonitoringTech *> &mapOfSubDeviceIdToPmtObject);
    decltype(&NEO::SysCalls::open) openFunction = NEO::SysCalls::open;
    decltype(&NEO::SysCalls::close) closeFunction = NEO::SysCalls::close;
    decltype(&NEO::SysCalls::pread) preadFunction = NEO::SysCalls::pread;

  private:
    static const std::string baseTelemSysFS;
    static const std::string telem;
    uint64_t baseOffset = 0;
    uint32_t subdeviceId = 0;
    ze_bool_t isSubdevice = 0;
};

} // namespace L0
