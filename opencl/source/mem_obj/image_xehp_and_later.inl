/*
 * Copyright (C) 2016-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {

template <>
void ImageHw<Family>::setMediaSurfaceRotation(void *memory) {
    using MEDIA_SURFACE_STATE = typename Family::MEDIA_SURFACE_STATE;
    using SURFACE_FORMAT = typename MEDIA_SURFACE_STATE::SURFACE_FORMAT;

    auto surfaceState = reinterpret_cast<MEDIA_SURFACE_STATE *>(memory);

    surfaceState->setRotation(MEDIA_SURFACE_STATE::ROTATION_NO_ROTATION_OR_0_DEGREE);
    surfaceState->setXOffset(0);
    surfaceState->setYOffset(0);
}

template <>
void ImageHw<Family>::setSurfaceMemoryObjectControlStateIndexToMocsTable(void *memory, uint32_t value) {
    using MEDIA_SURFACE_STATE = typename Family::MEDIA_SURFACE_STATE;
    using SURFACE_FORMAT = typename MEDIA_SURFACE_STATE::SURFACE_FORMAT;

    auto surfaceState = reinterpret_cast<MEDIA_SURFACE_STATE *>(memory);

    surfaceState->setSurfaceMemoryObjectControlStateIndexToMocsTables(value);
}

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
