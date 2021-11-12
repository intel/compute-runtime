/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/os_interface/linux/drm_mock_device_blob.h"

#include "drm/i915_drm.h"
#include "gtest/gtest.h"

void DrmMockEngine::handleQueryItem(drm_i915_query_item *queryItem) {
    switch (queryItem->query_id) {
    case DRM_I915_QUERY_ENGINE_INFO:
        if (queryEngineInfoSuccessCount == 0) {
            queryItem->length = -EINVAL;
        } else {
            queryEngineInfoSuccessCount--;
            auto numberOfEngines = 2u;
            int engineInfoSize = sizeof(drm_i915_query_engine_info) + numberOfEngines * sizeof(drm_i915_engine_info);
            if (queryItem->length == 0) {
                queryItem->length = engineInfoSize;
            } else {
                EXPECT_EQ(engineInfoSize, queryItem->length);
                auto queryEnginenInfo = reinterpret_cast<drm_i915_query_engine_info *>(queryItem->data_ptr);
                EXPECT_EQ(0u, queryEnginenInfo->num_engines);
                queryEnginenInfo->num_engines = numberOfEngines;
                queryEnginenInfo->engines[0].engine.engine_class = I915_ENGINE_CLASS_RENDER;
                queryEnginenInfo->engines[0].engine.engine_instance = 1;
                queryEnginenInfo->engines[1].engine.engine_class = I915_ENGINE_CLASS_COPY;
                queryEnginenInfo->engines[1].engine.engine_instance = 1;
            }
        }
        break;
    case DRM_I915_QUERY_HWCONFIG_TABLE: {
        if (failQueryDeviceBlob) {
            queryItem->length = -EINVAL;
        } else {
            int deviceBlobSize = sizeof(dummyDeviceBlobData);
            if (queryItem->length == 0) {
                queryItem->length = deviceBlobSize;
            } else {
                EXPECT_EQ(deviceBlobSize, queryItem->length);
                auto deviceBlobData = reinterpret_cast<uint32_t *>(queryItem->data_ptr);
                memcpy(deviceBlobData, &dummyDeviceBlobData, deviceBlobSize);
            }
        }
    } break;
    }
}
