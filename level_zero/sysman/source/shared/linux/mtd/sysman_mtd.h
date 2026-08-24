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
#include <memory>
#include <mtd/mtd-user.h>

namespace L0 {
namespace Sysman {

class MemoryTechnologyDeviceInterface : NEO::NonCopyableAndNonMovableClass {
  public:
    static std::unique_ptr<MemoryTechnologyDeviceInterface> create();
    virtual ze_result_t erase(const std::string &filePath, uint32_t offset, size_t size);
    virtual ze_result_t write(const std::string &filePath, uint32_t offset, const uint8_t *data, size_t size);

    virtual ~MemoryTechnologyDeviceInterface() = default;
};

} // namespace Sysman
} // namespace L0
