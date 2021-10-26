/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/cl_device_helpers.h"
#include "opencl/source/helpers/cl_hw_helper.h"

namespace NEO {

template <typename GfxFamily>
inline cl_command_queue_capabilities_intel ClHwHelperHw<GfxFamily>::getAdditionalDisabledQueueFamilyCapabilities(EngineGroupType type) const {
    return 0;
}

template <typename GfxFamily>
cl_ulong ClHwHelperHw<GfxFamily>::getKernelPrivateMemSize(const KernelInfo &kernelInfo) const {
    const auto &kernelAttributes = kernelInfo.kernelDescriptor.kernelAttributes;
    return (kernelAttributes.perThreadScratchSize[1] > 0) ? kernelAttributes.perThreadScratchSize[1] : kernelAttributes.perHwThreadPrivateMemorySize;
}

template <typename GfxFamily>
cl_device_feature_capabilities_intel ClHwHelperHw<GfxFamily>::getSupportedDeviceFeatureCapabilities() const {
    return ClDeviceHelper::getExtraCapabilities();
}

template <typename GfxFamily>
bool ClHwHelperHw<GfxFamily>::isFormatRedescribable(cl_image_format format) const {
    return false;
}
} // namespace NEO
