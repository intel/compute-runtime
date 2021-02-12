/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"

#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/source/program/kernel_info.h"

namespace NEO {

template <typename Family>
bool ClHwHelperHw<Family>::isBlitAuxTranslationRequired(const HardwareInfo &hwInfo, const MultiDispatchInfo &multiDispatchInfo) {
    return (HwHelperHw<Family>::getAuxTranslationMode() == AuxTranslationMode::Blit) &&
           hwInfo.capabilityTable.blitterOperationsSupported &&
           multiDispatchInfo.getKernelObjsForAuxTranslation() &&
           (multiDispatchInfo.getKernelObjsForAuxTranslation()->size() > 0);
}

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

template <typename GfxFamily>
inline bool ClHwHelperHw<GfxFamily>::allowRenderCompressionForContext(const HardwareInfo &hwInfo, const Context &context) const {
    return true;
}

template <typename GfxFamily>
inline bool ClHwHelperHw<GfxFamily>::getQueueFamilyName(std::string &name, EngineGroupType type) const {
    return false;
}

template <typename GfxFamily>
inline bool ClHwHelperHw<GfxFamily>::preferBlitterForLocalToLocalTransfers() const {
    return false;
}

} // namespace NEO
