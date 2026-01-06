/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/os_interface/linux/drm_mock_device_blob.h"

#include "gtest/gtest.h"

DrmMockEngine::DrmMockEngine(RootDeviceEnvironment &rootDeviceEnvironment) : DrmMock(rootDeviceEnvironment) {
    rootDeviceEnvironment.setHwInfoAndInitHelpers(defaultHwInfo.get());
}

void DrmMockEngine::handleQueryItem(QueryItem *queryItem) {
    if (queryItem->queryId == static_cast<uint64_t>(ioctlHelper->getDrmParamValue(DrmParam::queryEngineInfo))) {
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
                queryEnginenInfo->engines[0].engine.engine_class = static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::engineClassRender));
                queryEnginenInfo->engines[0].engine.engine_instance = 1;
                queryEnginenInfo->engines[1].engine.engine_class = static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::engineClassCopy));
                queryEnginenInfo->engines[1].engine.engine_instance = 1;
            }
        }
    } else if (queryItem->queryId == static_cast<uint64_t>(ioctlHelper->getDrmParamValue(DrmParam::queryHwconfigTable))) {
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
                adjustL3BankGroupsInfo(queryItem);
            }
        }
    }
}

void DrmMockEngine::adjustL3BankGroupsInfo(QueryItem *queryItem) {
    auto blobData = reinterpret_cast<uint32_t *>(queryItem->dataPtr);
    auto blobSize = static_cast<size_t>(queryItem->length) / sizeof(uint32_t);
    for (size_t i = 0; i < blobSize; i++) {
        if (blobData[i] == NEO::DeviceBlobConstants::numL3BanksPerGroup && dontQueryL3BanksPerGroup) {
            blobData[i + 2] = 0;
        } else if (blobData[i] == NEO::DeviceBlobConstants::numL3BankGroups && dontQueryL3BankGroups) {
            blobData[i + 2] = 0;
        }
    }
}
