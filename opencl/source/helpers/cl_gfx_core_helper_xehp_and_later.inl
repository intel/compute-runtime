/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"

#include "opencl/source/helpers/cl_gfx_core_helper.h"

namespace NEO {

template <typename GfxFamily>
inline cl_command_queue_capabilities_intel ClGfxCoreHelperHw<GfxFamily>::getAdditionalDisabledQueueFamilyCapabilities(EngineGroupType type) const {
    return 0;
}

template <typename GfxFamily>
cl_device_feature_capabilities_intel ClGfxCoreHelperHw<GfxFamily>::getSupportedDeviceFeatureCapabilities(const HardwareInfo &hwInfo) const {

    if (hwInfo.caps.matrixMultiplyAccumulateSupported) {
        return CL_DEVICE_FEATURE_FLAG_DPAS_INTEL | CL_DEVICE_FEATURE_FLAG_DP4A_INTEL;
    }
    return CL_DEVICE_FEATURE_FLAG_DP4A_INTEL;
}

template <typename GfxFamily>
bool ClGfxCoreHelperHw<GfxFamily>::isFormatRedescribable(cl_image_format format) const {
    return false;
}
} // namespace NEO
