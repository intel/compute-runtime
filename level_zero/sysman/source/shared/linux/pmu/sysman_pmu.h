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
    virtual int32_t getConfigFromEventFile(const std::string_view &eventFile, uint64_t &config) = 0;
    virtual int32_t getConfigAfterFormat(const std::string_view &formatDir, uint64_t &config, uint32_t engineClass, uint32_t engineInstance, uint32_t gt) = 0;
    static PmuInterface *create(LinuxSysmanImp *pLinuxSysmanImp);
};

} // namespace Sysman
} // namespace L0
