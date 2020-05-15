/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/tools/source/sysman/power/linux/os_power_imp.h"

namespace L0 {
namespace ult {

class PublicLinuxPowerImp : public L0::LinuxPowerImp {
  public:
    using LinuxPowerImp::pSysfsAccess;
};
} // namespace ult
} // namespace L0
