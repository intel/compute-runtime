/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/i915.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/os_interface/linux/drm_mock_device_blob.h"

#include "gtest/gtest.h"

void DrmMockEngine::handleQueryItem(QueryItem *queryItem) {
    if (queryItem->queryId == static_cast<uint64_t>(ioctlHelper->getDrmParamValue(DrmParam::QueryEngineInfo))) {
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
                auto queryEnginenInfo = reinterpret_cast<drm_i915_query_engine_info *>(queryItem->dataPtr);
                EXPECT_EQ(0u, queryEnginenInfo->num_engines);
                queryEnginenInfo->num_engines = numberOfEngines;
                queryEnginenInfo->engines[0].engine.engine_class = static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::EngineClassRender));
                queryEnginenInfo->engines[0].engine.engine_instance = 1;
                queryEnginenInfo->engines[1].engine.engine_class = static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::EngineClassCopy));
                queryEnginenInfo->engines[1].engine.engine_instance = 1;
            }
        }
    } else if (queryItem->queryId == static_cast<uint64_t>(ioctlHelper->getDrmParamValue(DrmParam::QueryHwconfigTable))) {
        if (failQueryDeviceBlob) {
            queryItem->length = -EINVAL;
        } else {
            int deviceBlobSize = sizeof(dummyDeviceBlobData);
            if (queryItem->length == 0) {
                queryItem->length = deviceBlobSize;
            } else {
                EXPECT_EQ(deviceBlobSize, queryItem->length);
                auto deviceBlobData = reinterpret_cast<uint32_t *>(queryItem->dataPtr);
                memcpy(deviceBlobData, &dummyDeviceBlobData, deviceBlobSize);
            }
        }
    }
}
