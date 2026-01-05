/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include "level_zero/ze_api.h"

#include <fcntl.h>
#include <map>
#include <memory>
#include <mtd/mtd-user.h>

namespace L0 {
namespace Sysman {

class MemoryTechnologyDeviceInterface : NEO::NonCopyableAndNonMovableClass {
  public:
    static std::unique_ptr<MemoryTechnologyDeviceInterface> create();
    virtual ze_result_t erase(const std::string &filePath, uint32_t offset, size_t size);
    virtual ze_result_t write(const std::string &filePath, uint32_t offset, const uint8_t *data, size_t size);
    virtual ze_result_t getDeviceInfo(const std::string &filePath, std::map<std::string, std::map<uint32_t, uint32_t>> &mtdRegionDeviceInfoMap);

    virtual ~MemoryTechnologyDeviceInterface() = default;
};

namespace MtdSysman {
struct SpiDescRegionBar {
    uint32_t base : 15; // Bits [26:12] of the region base address
    uint32_t reserved0 : 1;
    uint32_t limit : 15; // Bits [26:12] of region limit address (inclusive)
    uint32_t reserved1 : 1;
};
} // namespace MtdSysman

} // namespace Sysman
} // namespace L0
