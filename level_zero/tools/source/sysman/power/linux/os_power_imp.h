/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/tools/source/sysman/power/os_power.h"

#include "sysman/linux/os_sysman_imp.h"

namespace L0 {

class LinuxPowerImp : public OsPower, public NEO::NonCopyableClass {
  public:
    ze_result_t getEnergyCounter(uint64_t &energy) override;
    LinuxPowerImp(OsSysman *pOsSysman);
    LinuxPowerImp() = default;
    ~LinuxPowerImp() override = default;

  protected:
    SysfsAccess *pSysfsAccess = nullptr;
};
} // namespace L0
