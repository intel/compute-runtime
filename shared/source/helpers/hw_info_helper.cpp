/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info_helper.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_info.h"

namespace NEO {

bool HwInfoHelper::checkIfOcl21FeaturesEnabledOrEnforced(const HardwareInfo &hwInfo) {

    bool enabledOrEnforced = hwInfo.capabilityTable.supportsOcl21Features;
    if (const auto enforcedOclVersion = debugManager.flags.ForceOCLVersion.get(); enforcedOclVersion != 0) {
        enabledOrEnforced = (enforcedOclVersion == 21);
    }
    if (const auto enforcedOcl21Support = debugManager.flags.ForceOCL21FeaturesSupport.get(); enforcedOcl21Support != -1) {
        enabledOrEnforced = enforcedOcl21Support;
    }
    return enabledOrEnforced;
}
} // namespace NEO