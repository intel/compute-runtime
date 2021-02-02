/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/cl_hw_helper.h"

namespace NEO {

template <typename GfxFamily>
inline cl_command_queue_capabilities_intel ClHwHelperHw<GfxFamily>::getAdditionalDisabledQueueFamilyCapabilities(EngineGroupType type) const {
    if (type == EngineGroupType::Copy) {
        return CL_QUEUE_CAPABILITY_CREATE_CROSS_QUEUE_EVENTS_INTEL;
    }
    return 0;
}

template <typename GfxFamily>
cl_ulong ClHwHelperHw<GfxFamily>::getKernelPrivateMemSize(const KernelInfo &kernelInfo) const {
    return kernelInfo.patchInfo.pAllocateStatelessPrivateSurface ? kernelInfo.patchInfo.pAllocateStatelessPrivateSurface->PerThreadPrivateMemorySize : 0;
}

} // namespace NEO
