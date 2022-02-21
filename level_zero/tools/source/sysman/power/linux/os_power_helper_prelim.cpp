/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/power/linux/os_power_imp.h"
#include "level_zero/tools/source/sysman/sysman_const.h"

#include "sysman/linux/os_sysman_imp.h"

namespace L0 {

// The top-level hwmon(hwmon1 in example) contains all the power related information and device level
// energy counters. The other hwmon directories contain per tile energy counters.
// ex:- device/hwmon/hwmon1/energy1_input                name = "i915"   (Top level hwmon)
//      device/hwmon/hwmon2/energy1_input                name = "i915_gt0"  (Tile 0)
//      device/hwmon/hwmon3/energy1_input                name = "i915_gt1"  (Tile 1)

bool LinuxPowerImp::isEnergyHwmonDir(std::string name) {
    if (isSubdevice == true) {
        if (name == i915 + "_gt" + std::to_string(subdeviceId)) {
            return true;
        }
    } else if (name == i915) {
        return true;
    }
    return false;
}

} // namespace L0
