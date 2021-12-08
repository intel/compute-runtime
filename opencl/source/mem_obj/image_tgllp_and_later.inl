/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/resource_info.h"

#include "opencl/source/mem_obj/image.h"

namespace NEO {
template <>
void ImageHw<Family>::appendSurfaceStateDepthParams(RENDER_SURFACE_STATE *surfaceState, Gmm *gmm) {
    if (gmm) {
        const bool isDepthResource = gmm->gmmResourceInfo->getResourceFlags()->Gpu.Depth;
        surfaceState->setDepthStencilResource(isDepthResource);
    }
}
} // namespace NEO