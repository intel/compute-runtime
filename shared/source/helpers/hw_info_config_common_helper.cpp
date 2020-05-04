/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info_config_common_helper.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"

namespace NEO {
namespace HwInfoConfigCommonHelper {
void enableBlitterOperationsSupport(HardwareInfo &hardwareInfo) {
    hardwareInfo.capabilityTable.blitterOperationsSupported = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily).obtainBlitterPreference(hardwareInfo);

    if (DebugManager.flags.EnableBlitterOperationsSupport.get() != -1) {
        hardwareInfo.capabilityTable.blitterOperationsSupported = !!DebugManager.flags.EnableBlitterOperationsSupport.get();
    }
}
} // namespace HwInfoConfigCommonHelper
} // namespace NEO
