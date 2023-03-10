/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/compiler_product_helper.h"

#include "opencl/source/helpers/cl_gfx_core_helper.h"

namespace NEO {

template <typename GfxFamily>
inline cl_command_queue_capabilities_intel ClGfxCoreHelperHw<GfxFamily>::getAdditionalDisabledQueueFamilyCapabilities(EngineGroupType type) const {
    return 0;
}

template <typename GfxFamily>
cl_ulong ClGfxCoreHelperHw<GfxFamily>::getKernelPrivateMemSize(const KernelInfo &kernelInfo) const {
    const auto &kernelAttributes = kernelInfo.kernelDescriptor.kernelAttributes;
    return (kernelAttributes.perThreadScratchSize[1] > 0) ? kernelAttributes.perThreadScratchSize[1] : kernelAttributes.perHwThreadPrivateMemorySize;
}

template <typename GfxFamily>
cl_device_feature_capabilities_intel ClGfxCoreHelperHw<GfxFamily>::getSupportedDeviceFeatureCapabilities(const RootDeviceEnvironment &rootDeviceEnvironment) const {
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    auto &compilerProductHelper = rootDeviceEnvironment.getHelper<CompilerProductHelper>();
    if (compilerProductHelper.isMatrixMultiplyAccumulateSupported(hwInfo)) {
        return CL_DEVICE_FEATURE_FLAG_DPAS_INTEL | CL_DEVICE_FEATURE_FLAG_DP4A_INTEL;
    }
    return CL_DEVICE_FEATURE_FLAG_DP4A_INTEL;
}

template <typename GfxFamily>
bool ClGfxCoreHelperHw<GfxFamily>::isFormatRedescribable(cl_image_format format) const {
    return false;
}
} // namespace NEO
