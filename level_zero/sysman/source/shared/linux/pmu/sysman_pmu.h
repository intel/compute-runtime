/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <unistd.h>

namespace L0 {
namespace Sysman {
class LinuxSysmanImp;
class PmuInterface {
  public:
    virtual ~PmuInterface() = default;
    virtual int64_t pmuInterfaceOpen(uint64_t config, int group, uint32_t format) = 0;
    virtual int pmuRead(int fd, uint64_t *data, ssize_t sizeOfdata) = 0;
    static PmuInterface *create(LinuxSysmanImp *pLinuxSysmanImp);
};

} // namespace Sysman
} // namespace L0
