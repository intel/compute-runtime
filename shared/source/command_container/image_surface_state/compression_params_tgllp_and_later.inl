/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {
template <typename Family>
void EncodeSurfaceState<Family>::setAuxParamsForMCSCCS(R_SURFACE_STATE *surfaceState) {
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

} // namespace NEO
