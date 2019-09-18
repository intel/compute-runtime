/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
void ImageHw<Family>::setClearColorParams(Family::RENDER_SURFACE_STATE *surfaceState, const Gmm *gmm) {
    if (gmm->gmmResourceInfo->getResourceFlags()->Gpu.IndirectClearColor) {
        surfaceState->setClearValueAddressEnable(true);

        uint64_t clearColorAddress = GmmHelper::decanonize(surfaceState->getSurfaceBaseAddress() +
                                                           gmm->gmmResourceInfo->getUnifiedAuxSurfaceOffset(GMM_UNIFIED_AUX_TYPE::GMM_AUX_CC));
        surfaceState->setClearColorAddress(static_cast<uint32_t>(clearColorAddress & 0xFFFFFFFFULL));
        surfaceState->setClearColorAddressHigh(static_cast<uint32_t>(clearColorAddress >> 32));
    }
}

template <>
void ImageHw<Family>::setAuxParamsForMCSCCS(RENDER_SURFACE_STATE *surfaceState, Gmm *gmm) {
    surfaceState->setAuxiliarySurfaceMode(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_MCS_LCE);
}

template <>
void ImageHw<Family>::setFlagsForMediaCompression(RENDER_SURFACE_STATE *surfaceState, Gmm *gmm) {
    if (gmm->gmmResourceInfo->getResourceFlags()->Info.MediaCompressed) {
        surfaceState->setAuxiliarySurfaceMode(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
        surfaceState->setMemoryCompressionEnable(true);
    } else {
        surfaceState->setMemoryCompressionEnable(false);
    }
}
