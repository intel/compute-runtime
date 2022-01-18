/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hp_core/hw_cmds_base.h"

#include "opencl/source/context/context.h"
#include "opencl/source/mem_obj/image.inl"

namespace NEO {

using Family = XeHpFamily;
static auto gfxCore = IGFX_XE_HP_CORE;

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
#include "opencl/source/mem_obj/image_tgllp_and_later.inl"

// factory initializer
#include "opencl/source/mem_obj/image_factory_init.inl"
