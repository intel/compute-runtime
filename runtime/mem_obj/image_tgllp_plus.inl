/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
void ImageHw<Family>::setAuxParamsForMCSCCS(RENDER_SURFACE_STATE *surfaceState, Gmm *gmm) {
    surfaceState->setAuxiliarySurfaceMode(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_MCS_LCE);
}

template <>
void ImageHw<Family>::appendSurfaceStateDepthParams(RENDER_SURFACE_STATE *surfaceState) {
    const auto gmm = this->graphicsAllocation->getDefaultGmm();
    if (gmm) {
        const bool isDepthResource = gmm->gmmResourceInfo->getResourceFlags()->Gpu.Depth;
        surfaceState->setDepthStencilResource(isDepthResource);
    }
}
