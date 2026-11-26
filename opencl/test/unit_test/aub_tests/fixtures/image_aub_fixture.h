/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

#include "opencl/test/unit_test/aub_tests/command_stream/aub_command_stream_fixture.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"

#include "aub_fixture.h"

#include <tuple>
#include <vector>

namespace NEO {
struct ImageAubRegion {
    cl_mem_object_type imageType;
    size_t offsets[3];
};

inline constexpr ImageAubRegion imageAubRegion1D = {CL_MEM_OBJECT_IMAGE1D, {0u, 0u, 0u}};
inline constexpr ImageAubRegion imageAubRegion1DWithOrigin = {CL_MEM_OBJECT_IMAGE1D, {1u, 0u, 0u}};
inline constexpr ImageAubRegion imageAubRegion2D = {CL_MEM_OBJECT_IMAGE2D, {0u, 0u, 0u}};
inline constexpr ImageAubRegion imageAubRegion2DWithOrigin = {CL_MEM_OBJECT_IMAGE2D, {1u, 2u, 0u}};
inline constexpr ImageAubRegion imageAubRegion3D = {CL_MEM_OBJECT_IMAGE3D, {0u, 0u, 0u}};
inline constexpr ImageAubRegion imageAubRegion3DWithOrigin = {CL_MEM_OBJECT_IMAGE3D, {1u, 2u, 3u}};

struct ImageAubOrderRegions {
    cl_channel_order order;
    ImageAubRegion regions[2];
};

inline constexpr ImageAubOrderRegions imageAubOrderRegions[] = {
    {CL_R, {imageAubRegion1D, imageAubRegion2DWithOrigin}},
    {CL_RG, {imageAubRegion2D, imageAubRegion3DWithOrigin}},
    {CL_RGBA, {imageAubRegion3D, imageAubRegion1DWithOrigin}}};

inline constexpr cl_channel_type imageAubChannelTypes[] = {CL_UNORM_INT8, CL_SIGNED_INT16, CL_UNSIGNED_INT32, CL_HALF_FLOAT, CL_FLOAT};

using ImageAubFormatRegionParams = std::tuple<uint32_t /*cl_channel_type*/, uint32_t /*cl_channel_order*/, ImageAubRegion>;

inline std::vector<ImageAubFormatRegionParams> getImageAubFormatRegionParams() {
    std::vector<ImageAubFormatRegionParams> params;
    for (const auto channelType : imageAubChannelTypes) {
        for (const auto &orderRegions : imageAubOrderRegions) {
            for (const auto &region : orderRegions.regions) {
                params.emplace_back(channelType, orderRegions.order, region);
            }
        }
    }
    return params;
}

struct ImageAubFixture : public AUBCommandStreamFixture {
    typedef AUBCommandStreamFixture CommandStreamFixture;

    using AUBCommandStreamFixture::setUp;

    void setUp(bool enableBlitter, uint32_t imageType = 0) {
        if (enableBlitter) {

            MockExecutionEnvironment mockExecutionEnvironment{};
            auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHelper<ProductHelper>();

            if (!productHelper.isBlitterForImagesSupported() || !productHelper.blitEnqueuePreferred(false)) {
                GTEST_SKIP();
            }

            if (imageType == CL_MEM_OBJECT_IMAGE3D && !productHelper.isTile64With3DSurfaceOnBCSSupported(*defaultHwInfo)) {
                GTEST_SKIP();
            }

            auto hardwareInfo = *defaultHwInfo;
            hardwareInfo.capabilityTable.blitterOperationsSupported = true;
            CommandStreamFixture::setUp(&hardwareInfo);
        } else {
            CommandStreamFixture::setUp(nullptr);
        }
    }

    void tearDown() {
        if (pCmdQ) {
            auto blocked = pCmdQ->isQueueBlocked();
            UNRECOVERABLE_IF(blocked);
        }

        CommandStreamFixture::tearDown();
    }

    DebugManagerStateRestore restorer;
};
} // namespace NEO
