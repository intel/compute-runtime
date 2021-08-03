/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
void ImageHw<Family>::appendSurfaceStateDepthParams(RENDER_SURFACE_STATE *surfaceState, Gmm *gmm) {
    if (gmm) {
        const bool isDepthResource = gmm->gmmResourceInfo->getResourceFlags()->Gpu.Depth;
        surfaceState->setDepthStencilResource(isDepthResource);
    }
}
