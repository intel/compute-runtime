/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/sysman/linux/fs_access.h"

#include <map>

namespace L0 {

class PlatformMonitoringTech : NEO::NonCopyableOrMovableClass {
  public:
    PlatformMonitoringTech() = delete;
    PlatformMonitoringTech(FsAccess *pFsAccess, ze_bool_t onSubdevice, uint32_t subdeviceId);
    virtual ~PlatformMonitoringTech();

    template <typename ReadType>
    ze_result_t readValue(const std::string key, ReadType &value);
    static ze_result_t enumerateRootTelemIndex(FsAccess *pFsAccess, std::string &rootPciPathOfGpuDevice);
    static void create(const std::vector<ze_device_handle_t> &deviceHandles,
                       FsAccess *pFsAccess, std::string &rootPciPathOfGpuDevice,
                       std::map<uint32_t, L0::PlatformMonitoringTech *> &mapOfSubDeviceIdToPmtObject);

  protected:
    char *mappedMemory = nullptr;
    static uint32_t rootDeviceTelemNodeIndex;
    std::map<std::string, uint64_t> keyOffsetMap;

  private:
    void init(FsAccess *pFsAccess);
    static const std::string baseTelemSysFS;
    static const std::string telem;
    uint64_t size = 0;
    uint64_t baseOffset = 0;
    uint32_t subdeviceId = 0;
    ze_bool_t isSubdevice = 0;
    ze_result_t retVal = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
};

template <typename ReadType>
ze_result_t PlatformMonitoringTech::readValue(const std::string key, ReadType &value) {
    if (mappedMemory == nullptr) {
        return retVal;
    }

    auto offset = keyOffsetMap.find(key);
    if (offset == keyOffsetMap.end()) {
        return retVal;
    }
    value = *reinterpret_cast<ReadType *>(mappedMemory + offset->second);
    return ZE_RESULT_SUCCESS;
}

} // namespace L0
