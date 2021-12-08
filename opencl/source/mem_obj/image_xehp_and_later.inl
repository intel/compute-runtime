/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/context/context.h"
#include "opencl/source/mem_obj/image.h"

namespace NEO {
template <>
void ImageHw<Family>::appendSurfaceStateParams(Family::RENDER_SURFACE_STATE *surfaceState, uint32_t rootDeviceIndex, bool useGlobalAtomics) {
    auto imageCtxType = this->context->peekContextType();

    bool enableMultiGpuPartialWrites = (imageCtxType != ContextType::CONTEXT_TYPE_SPECIALIZED) && (context->containsMultipleSubDevices(rootDeviceIndex));

    bool enableMultiGpuAtomics = enableMultiGpuPartialWrites;

    if (DebugManager.flags.EnableMultiGpuAtomicsOptimization.get()) {
        enableMultiGpuAtomics &= useGlobalAtomics;
    }

    surfaceState->setDisableSupportForMultiGpuAtomics(!enableMultiGpuAtomics);
    surfaceState->setDisableSupportForMultiGpuPartialWrites(!enableMultiGpuPartialWrites);

    if (DebugManager.flags.ForceMultiGpuAtomics.get() != -1) {
        surfaceState->setDisableSupportForMultiGpuAtomics(!!DebugManager.flags.ForceMultiGpuAtomics.get());
    }
    if (DebugManager.flags.ForceMultiGpuPartialWrites.get() != -1) {
        surfaceState->setDisableSupportForMultiGpuPartialWrites(!!DebugManager.flags.ForceMultiGpuPartialWrites.get());
    }
}
} // namespace NEO
