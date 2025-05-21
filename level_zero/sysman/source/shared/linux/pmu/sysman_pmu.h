/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <string>
#include <unistd.h>

namespace L0 {
namespace Sysman {
class LinuxSysmanImp;
class PmuInterface {
  public:
    virtual ~PmuInterface() = default;
    virtual int64_t pmuInterfaceOpen(uint64_t config, int group, uint32_t format) = 0;
    virtual int32_t pmuRead(int fd, uint64_t *data, ssize_t sizeOfdata) = 0;
    virtual int32_t getPmuConfigs(const std::string_view &sysmanDeviceDir, uint64_t engineClass, uint64_t engineInstance, uint64_t gtId, uint64_t &activeTicksConfig, uint64_t &totalTicksConfig) = 0;
    virtual int32_t getPmuConfigsForVf(const std::string_view &sysmanDeviceDir, uint64_t fnNumber, uint64_t &activeTicksConfig, uint64_t &totalTicksConfig) = 0;
    static PmuInterface *create(LinuxSysmanImp *pLinuxSysmanImp);
};

} // namespace Sysman
} // namespace L0
