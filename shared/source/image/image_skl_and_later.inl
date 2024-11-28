/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
void setMipTailStartLOD<Family>(Family::RENDER_SURFACE_STATE *surfaceState, Gmm *gmm) {
    surfaceState->setMipTailStartLOD(0);

    if (gmm != nullptr) {
        surfaceState->setMipTailStartLOD(gmm->gmmResourceInfo->getMipTailStartLODSurfaceState());
    }
}
