/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
void setMipTailStartLod<Family>(Family::RENDER_SURFACE_STATE *surfaceState, Gmm *gmm) {
    surfaceState->setMipTailStartLod(0);

    if (gmm != nullptr) {
        surfaceState->setMipTailStartLod(gmm->gmmResourceInfo->getMipTailStartLodSurfaceState());
    }
}
