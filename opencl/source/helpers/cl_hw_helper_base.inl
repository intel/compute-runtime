/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/source/program/kernel_info.h"

namespace NEO {

template <typename GfxFamily>
inline bool ClHwHelperHw<GfxFamily>::requiresAuxResolves(const KernelInfo &kernelInfo) const {
    return hasStatelessAccessToBuffer(kernelInfo);
}

template <typename GfxFamily>
inline bool ClHwHelperHw<GfxFamily>::hasStatelessAccessToBuffer(const KernelInfo &kernelInfo) const {
    bool hasStatelessAccessToBuffer = false;
    for (uint32_t i = 0; i < kernelInfo.kernelArgInfo.size(); ++i) {
        if (kernelInfo.kernelArgInfo[i].isBuffer) {
            hasStatelessAccessToBuffer |= !kernelInfo.kernelArgInfo[i].pureStatefulBufferAccess;
        }
    }
    return hasStatelessAccessToBuffer;
}

} // namespace NEO
