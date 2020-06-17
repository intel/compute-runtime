/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "sysman/memory/os_memory.h"

namespace L0 {

class SysfsAccess;

class LinuxMemoryImp : public OsMemory, public NEO::NonCopyableClass {
  public:
    ze_result_t getMemorySize(uint64_t &maxSize, uint64_t &allocSize) override;
    ze_result_t getMemHealth(zet_mem_health_t &memHealth) override;
    LinuxMemoryImp(OsSysman *pOsSysman);
    LinuxMemoryImp() = default;
    ~LinuxMemoryImp() override = default;
};
} // namespace L0
