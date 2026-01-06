/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/product_helper.h"

#include "hw_cmds.h"

namespace NEO {

int ProductHelper::configureHwInfoWddm(const HardwareInfo *inHwInfo, HardwareInfo *outHwInfo, const RootDeviceEnvironment &rootDeviceEnvironment) {
    this->setCapabilityCoherencyFlag(*outHwInfo, outHwInfo->capabilityTable.ftrSupportsCoherency);
    outHwInfo->capabilityTable.ftrSupportsCoherency &= inHwInfo->featureTable.flags.ftrL3IACoherency;

    setupDefaultEngineType(*outHwInfo, rootDeviceEnvironment);
    setupPreemptionMode(*outHwInfo, rootDeviceEnvironment, true);
    setupPreemptionSurfaceSize(*outHwInfo, rootDeviceEnvironment);
    setupKmdNotifyProperties(outHwInfo->capabilityTable.kmdNotifyProperties);
    auto ret = setupProductSpecificConfig(*outHwInfo, rootDeviceEnvironment);
    setupImageSupport(*outHwInfo);
    return ret;
}

} // namespace NEO
