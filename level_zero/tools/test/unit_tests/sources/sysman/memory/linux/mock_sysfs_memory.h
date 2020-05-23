/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/tools/source/sysman/memory/linux/os_memory_imp.h"

#include "sysman/linux/os_sysman_imp.h"

namespace L0 {
namespace ult {

class PublicLinuxMemoryImp : public L0::LinuxMemoryImp {
  public:
    using LinuxMemoryImp::pSysfsAccess;
};
} // namespace ult
} // namespace L0
