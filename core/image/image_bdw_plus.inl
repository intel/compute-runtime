/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
void setClearColorParams<Family>(typename Family::RENDER_SURFACE_STATE *surfaceState, const Gmm *gmm) {
}

template <>
void setFlagsForMediaCompression<Family>(typename Family::RENDER_SURFACE_STATE *surfaceState, Gmm *gmm) {
    using AUXILIARY_SURFACE_MODE = typename Family::RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;
    if (gmm->gmmResourceInfo->getResourceFlags()->Info.MediaCompressed) {
        surfaceState->setAuxiliarySurfaceMode(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
    }
}
