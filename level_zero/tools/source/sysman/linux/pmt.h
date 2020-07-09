/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/tools/source/sysman/linux/fs_access.h"

#include <map>

namespace L0 {

class PlatformMonitoringTech : NEO::NonCopyableOrMovableClass {
  public:
    PlatformMonitoringTech() = default;
    virtual ~PlatformMonitoringTech();
    MOCKABLE_VIRTUAL void init(const std::string &deviceName, FsAccess *pFsAccess);
    template <typename ReadType>
    ze_result_t readValue(const std::string key, ReadType &value);
    bool isPmtSupported() { return pmtSupported; }

  protected:
    bool pmtSupported = false;
    char *mappedMemory = nullptr;

  private:
    static const std::string baseTelemDevice;
    static const std::string baseTelemSysFS;
    uint64_t size = 0;
    uint64_t baseOffset = 0;
};

const std::map<std::string, uint64_t> keyOffsetMap = {
    {"PACKAGE_ENERGY", 0x400},
    {"COMPUTE_TEMPERATURES", 0x68},
    {"SOC_TEMPERATURES", 0x60},
    {"CORE_TEMPERATURES", 0x6c}};

template <typename ReadType>
ze_result_t PlatformMonitoringTech::readValue(const std::string key, ReadType &value) {
    if (!pmtSupported) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    auto offset = keyOffsetMap.find(key);
    if (offset == keyOffsetMap.end()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    value = *reinterpret_cast<ReadType *>(mappedMemory + offset->second);
    return ZE_RESULT_SUCCESS;
}

} // namespace L0
