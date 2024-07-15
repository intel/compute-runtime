/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper.h"

#include "shared/source/built_ins/sip_kernel_type.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"

namespace NEO {

ProductHelperCreateFunctionType productHelperFactory[IGFX_MAX_PRODUCT] = {};

void ProductHelper::setupPreemptionSurfaceSize(HardwareInfo &hwInfo, const RootDeviceEnvironment &rootDeviceEnvironment) {
    if (debugManager.flags.OverridePreemptionSurfaceSizeInMb.get() >= 0) {
        hwInfo.gtSystemInfo.CsrSizeInMb = static_cast<uint32_t>(debugManager.flags.OverridePreemptionSurfaceSizeInMb.get());
    }

    if (debugManager.flags.ForceSipClass.get() == static_cast<uint32_t>(SipClassType::builtins)) {
        return;
    }
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();

    hwInfo.capabilityTable.requiredPreemptionSurfaceSize = hwInfo.gtSystemInfo.CsrSizeInMb * MemoryConstants::megaByte;
    gfxCoreHelper.adjustPreemptionSurfaceSize(hwInfo.capabilityTable.requiredPreemptionSurfaceSize, rootDeviceEnvironment);
}

} // namespace NEO
