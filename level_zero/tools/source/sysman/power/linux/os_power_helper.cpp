/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/power/linux/os_power_imp.h"

namespace L0 {

bool LinuxPowerImp::isEnergyHwmonDir(std::string name) {
    if (isSubdevice == false && (name == i915)) {
        return true;
    }
    return false;
}

} // namespace L0
