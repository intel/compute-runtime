/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/surface_format_info.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/sharings/unified/unified_image.h"

namespace NEO {

void *UnifiedImage::swapGmm(GraphicsAllocation *graphicsAllocation, Context *context, ImageInfo *imgInfo) {
    if (graphicsAllocation->getDefaultGmm()->gmmResourceInfo->getResourceType() == RESOURCE_BUFFER) {
        imgInfo->linearStorage = true;
        auto gmmHelper = context->getDevice(0)->getRootDeviceEnvironment().getGmmHelper();
        auto gmm = std::make_unique<Gmm>(gmmHelper, *imgInfo, StorageInfo{}, false);
        gmm->updateImgInfoAndDesc(*imgInfo, 0, NEO::ImagePlane::noPlane);
        delete graphicsAllocation->getDefaultGmm();
        graphicsAllocation->setDefaultGmm(gmm.release());
    }

    return 0;
}

} // namespace NEO
