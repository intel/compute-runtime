/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/release_helper/release_helper.h"

namespace NEO {
template <typename Family>
size_t EncodeDispatchKernel<Family>::getDefaultIOHAlignment() {
    return 1;
}

template <typename Family>
uint32_t EncodeDispatchKernel<Family>::getThreadCountPerSubslice(const HardwareInfo &hwInfo) {
    return hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.DualSubSliceCount;
}

template <typename Family>
uint32_t EncodeDispatchKernel<Family>::alignPreferredSlmSize(uint32_t slmSize) {
    return EncodeDispatchKernel<Family>::alignSlmSize(slmSize);
}

template <typename Family>
void EncodeSurfaceState<Family>::disableCompressionFlags(R_SURFACE_STATE *surfaceState) {
    surfaceState->setAuxiliarySurfaceMode(Family::RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
    surfaceState->setMemoryCompressionEnable(false);
}

template <typename Family>
void EncodeSurfaceState<Family>::setAuxParamsForMCSCCS(R_SURFACE_STATE *surfaceState, const ReleaseHelper *releaseHelper) {
    if (releaseHelper && releaseHelper->isAuxSurfaceModeOverrideRequired())
        surfaceState->setAuxiliarySurfaceMode(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
    else
        surfaceState->setAuxiliarySurfaceMode(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_MCS_LCE);
}

template <typename Family>
void EncodeSurfaceState<Family>::setClearColorParams(R_SURFACE_STATE *surfaceState, Gmm *gmm) {
    if (gmm->gmmResourceInfo->getResourceFlags()->Gpu.IndirectClearColor) {
        surfaceState->setClearValueAddressEnable(true);

        auto gmmHelper = gmm->getGmmHelper();
        uint64_t clearColorAddress = gmmHelper->decanonize(surfaceState->getSurfaceBaseAddress() +
                                                           gmm->gmmResourceInfo->getUnifiedAuxSurfaceOffset(GMM_UNIFIED_AUX_TYPE::GMM_AUX_CC));

        surfaceState->setClearColorAddress(static_cast<uint32_t>(clearColorAddress & 0xFFFFFFFFULL));
        surfaceState->setClearColorAddressHigh(static_cast<uint32_t>(clearColorAddress >> 32));
    }
}

template <typename Family>
void EncodeSurfaceState<Family>::setFlagsForMediaCompression(R_SURFACE_STATE *surfaceState, Gmm *gmm) {
    if (gmm->gmmResourceInfo->getResourceFlags()->Info.MediaCompressed) {
        surfaceState->setAuxiliarySurfaceMode(Family::RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
        surfaceState->setMemoryCompressionEnable(true);
    } else {
        surfaceState->setMemoryCompressionEnable(false);
    }
}

template <typename Family>
void EncodeSurfaceState<Family>::setImageAuxParamsForCCS(R_SURFACE_STATE *surfaceState, Gmm *gmm) {
    using AUXILIARY_SURFACE_MODE = typename Family::RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;
    // Its expected to not program pitch/qpitch/baseAddress for Aux surface in CCS scenarios
    surfaceState->setAuxiliarySurfaceMode(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
    setFlagsForMediaCompression(surfaceState, gmm);

    setClearColorParams(surfaceState, gmm);
    ImageSurfaceStateHelper<Family>::setUnifiedAuxBaseAddress(surfaceState, gmm);
}

template <typename Family>
void EncodeSurfaceState<Family>::setBufferAuxParamsForCCS(R_SURFACE_STATE *surfaceState) {
    using AUXILIARY_SURFACE_MODE = typename R_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    surfaceState->setAuxiliarySurfaceMode(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
}

template <typename Family>
bool EncodeSurfaceState<Family>::isAuxModeEnabled(R_SURFACE_STATE *surfaceState, Gmm *gmm) {
    using AUXILIARY_SURFACE_MODE = typename R_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    return (surfaceState->getAuxiliarySurfaceMode() == AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
}

template <typename Family>
void EncodeEnableRayTracing<Family>::append3dStateBtd(void *ptr3dStateBtd) {}

template <typename Family>
bool EncodeSurfaceState<Family>::shouldProgramAuxForMcs(bool isAuxCapable, bool hasMcsSurface) {
    return isAuxCapable && hasMcsSurface;
}

} // namespace NEO
