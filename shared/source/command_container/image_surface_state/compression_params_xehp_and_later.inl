/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/resource_info.h"

namespace NEO {

template <typename Family>
void EncodeSurfaceState<Family>::appendImageCompressionParams(R_SURFACE_STATE *surfaceState, GraphicsAllocation *allocation, GmmHelper *gmmHelper, bool imageFromBuffer,
                                                              GMM_YUV_PLANE_ENUM plane) {
    const auto ccsMode = R_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E;
    const auto mcsLceMode = R_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_MCS_LCE;
    if ((ccsMode == surfaceState->getAuxiliarySurfaceMode() || mcsLceMode == surfaceState->getAuxiliarySurfaceMode() || surfaceState->getMemoryCompressionEnable())) {
        uint32_t compressionFormat;
        auto gmmResourceInfo = allocation->getDefaultGmm()->gmmResourceInfo.get();
        if (gmmResourceInfo->getResourceFlags()->Info.MediaCompressed) {
            compressionFormat = gmmHelper->getClientContext()->getMediaSurfaceStateCompressionFormat(gmmResourceInfo->getResourceFormat());
            EncodeWA<Family>::adjustCompressionFormatForPlanarImage(compressionFormat, plane);
        } else {
            compressionFormat = gmmHelper->getClientContext()->getSurfaceStateCompressionFormat(gmmResourceInfo->getResourceFormat());
        }

        if (imageFromBuffer) {
            if (DebugManager.flags.ForceBufferCompressionFormat.get() != -1) {
                compressionFormat = DebugManager.flags.ForceBufferCompressionFormat.get();
            }
            appendParamsForImageFromBuffer(surfaceState);
        }

        surfaceState->setCompressionFormat(compressionFormat);
    }
}
} // namespace NEO
