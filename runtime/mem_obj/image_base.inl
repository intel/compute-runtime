/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace OCLRT {

template <typename GfxFamily>
void ImageHw<GfxFamily>::setClearColorParams(RENDER_SURFACE_STATE *surfaceState, const Gmm *gmm) {
}

template <typename GfxFamily>
void ImageHw<GfxFamily>::setAuxParamsForMCSCCS(RENDER_SURFACE_STATE *surfaceState, Gmm *gmm) {
}
} // namespace OCLRT
