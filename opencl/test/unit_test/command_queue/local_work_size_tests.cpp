/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/local_work_size.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/command_queue/cl_local_work_size.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

using namespace NEO;

TEST(localWorkSizeTest, givenDisableEUFusionWhenCreatingWorkSizeInfoThenCorrectMinWorkGroupSizeIsSet) {
    uint32_t simdSize = 8u;
    uint32_t numThreadsPerSubS = 8u;
    WorkSizeInfo wsInfo(256,                 // maxWorkGroupSize
                        1u,                  // hasBariers
                        simdSize,            // simdSize
                        0u,                  // slmTotalSize
                        defaultHwInfo.get(), // hardwareInfo
                        numThreadsPerSubS,   // numThreadsPerSubS
                        0u,                  // localMemorySize
                        false,               // imgUsed
                        false,               // yTiledSurface
                        true                 // disableEUFusion
    );

    auto &hwHelper = HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily);
    bool fusedDispatchEnabled = hwHelper.isFusedEuDispatchEnabled(*defaultHwInfo, true);
    auto WGSMultiple = fusedDispatchEnabled ? 2 : 1;

    uint32_t maxBarriersPerHSlice = (defaultHwInfo.get()->platform.eRenderCoreFamily >= IGFX_GEN9_CORE) ? 32 : 16;
    uint32_t expectedMinWGS = WGSMultiple * simdSize * numThreadsPerSubS / maxBarriersPerHSlice;
    EXPECT_EQ(expectedMinWGS, wsInfo.minWorkGroupSize);
}

TEST(localWorkSizeTest, given3DimWorkGroupAndSimdEqual8AndBarriersWhenComputeCalledThenLocalGroupComputedCorrectly) {
    WorkSizeInfo wsInfo(256,                 // maxWorkGroupSize
                        1u,                  // hasBariers
                        8,                   // simdSize
                        0u,                  // slmTotalSize
                        defaultHwInfo.get(), // hardwareInfo
                        32u,                 // numThreadsPerSubSlice
                        0u,                  // localMemorySize
                        false,               // imgUsed
                        false,               // yTiledSurface
                        false                // disableEUFusion
    );

    uint32_t workDim = 3;
    size_t workGroup[3] = {10000, 10000, 10000};
    size_t workGroupSize[3];

    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 200u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 50;
    workGroup[1] = 2000;
    workGroup[2] = 100;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 50u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, givenSmallerLocalMemSizeThanSlmTotalSizeThenExceptionIsThrown) {
    EXPECT_THROW(WorkSizeInfo wsInfo(256,                 // maxWorkGroupSize
                                     1u,                  // hasBariers
                                     8,                   // simdSize
                                     128u,                // slmTotalSize
                                     defaultHwInfo.get(), // hardwareInfo
                                     32u,                 // numThreadsPerSubSlice
                                     64u,                 // localMemorySize
                                     false,               // imgUsed
                                     false,               // yTiledSurface
                                     false                // disableEUFusion
                                     ),
                 std::exception);
}

TEST(localWorkSizeTest, given2DimWorkGroupAndSimdEqual8AndNoBarriersWhenComputeCalledThenLocalGroupComputedCorrectly) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(true);

    //wsInfo maxWorkGroupSize, hasBariers, simdSize, slmTotalSize, hardwareInfo, numThreadsPerSubSlice, localMemorySize, imgUsed, yTiledSurface, disableEUFusion
    WorkSizeInfo wsInfo(256, 0u, 8, 0u, defaultHwInfo.get(), 32u, 0u, false, false, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {10003, 10003, 1};
    size_t workGroupSize[3];

    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 7u);
    EXPECT_EQ(workGroupSize[1], 7u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 21;
    workGroup[1] = 3000;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 21u);
    EXPECT_EQ(workGroupSize[1], 8u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, given1DimWorkGroupAndSimdEqual8WhenComputeCalledThenLocalGroupComputed) {
    //wsInfo maxWorkGroupSize, hasBariers, simdSize, slmTotalSize, hardwareInfo, numThreadsPerSubSlice, localMemorySize, imgUsed, yTiledSurface, disableEUFusion
    WorkSizeInfo wsInfo(256, 0u, 8, 0u, defaultHwInfo.get(), 32u, 0u, false, false, false);
    uint32_t workDim = 1;
    size_t workGroup[3] = {6144, 1, 1};
    size_t workGroupSize[3];

    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 48;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 16u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 1536;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 333;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 9u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, given1DimWorkGroupAndSimdEqual32WhenComputeCalledThenLocalGroupComputed) {
    WorkSizeInfo wsInfo(256, 0u, 32, 0u, defaultHwInfo.get(), 32u, 0u, false, false, false);
    uint32_t workDim = 1;
    size_t workGroup[3] = {6144, 1, 1};
    size_t workGroupSize[3];

    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 48;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 48u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 512;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, given2DimWorkGroupAndSimdEqual8WhenComputeCalledThenLocalGroupComputed) {
    WorkSizeInfo wsInfo(256, 0u, 8, 0u, defaultHwInfo.get(), 56u, 0u, false, false, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {384, 96, 1};
    size_t workGroupSize[3];

    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 128u);
    EXPECT_EQ(workGroupSize[1], 2u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 48;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 16u);
    EXPECT_EQ(workGroupSize[1], 16u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 12;
    workGroup[1] = 512;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 4u);
    EXPECT_EQ(workGroupSize[1], 64u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, given2DimWorkGroupAndSimdEqual32WhenComputeCalledThenLocalGroupComputed) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(false);
    WorkSizeInfo wsInfo(256, 0u, 32, 0u, defaultHwInfo.get(), 32u, 0u, false, false, false);

    uint32_t workDim = 2;
    size_t workGroup[3] = {384, 96, 1};
    size_t workGroupSize[3];

    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 128u);
    EXPECT_EQ(workGroupSize[1], 2u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 48;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 16u);
    EXPECT_EQ(workGroupSize[1], 16u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 12;
    workGroup[1] = 512;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 4u);
    EXPECT_EQ(workGroupSize[1], 64u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 1;
    workGroup[1] = 384;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 1u);
    EXPECT_EQ(workGroupSize[1], 128u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, given3DimWorkGroupAndSimdEqual8WhenComputeCalledThenLocalGroupComputed) {
    WorkSizeInfo wsInfo(256, 0u, 8, 0u, defaultHwInfo.get(), 56u, 0u, false, false, false);
    uint32_t workDim = 3;
    size_t workGroup[3] = {384, 384, 384};
    size_t workGroupSize[3];

    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 128u);
    EXPECT_EQ(workGroupSize[1], 2u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 96;
    workGroup[1] = 4;
    workGroup[2] = 4;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 32u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 2u);

    workGroup[0] = 12;
    workGroup[1] = 512;
    workGroup[2] = 48;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 4u);
    EXPECT_EQ(workGroupSize[1], 64u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 2;
    workGroup[1] = 2;
    workGroup[2] = 3;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 2u);
    EXPECT_EQ(workGroupSize[1], 2u);
    EXPECT_EQ(workGroupSize[2], 3u);
}

TEST(localWorkSizeTest, given3DimWorkGroupAndSimdEqual32WhenComputeCalledThenLocalGroupComputed) {
    NEO::WorkSizeInfo wsInfo(256, 0u, 32, 0u, defaultHwInfo.get(), 32u, 0u, false, false, false);
    uint32_t workDim = 3;
    size_t workGroup[3] = {384, 384, 384};
    size_t workGroupSize[3];
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 128u);
    EXPECT_EQ(workGroupSize[1], 2u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 96;
    workGroup[1] = 6;
    workGroup[2] = 4;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 32u);
    EXPECT_EQ(workGroupSize[1], 2u);
    EXPECT_EQ(workGroupSize[2], 4u);

    workGroup[0] = 12;
    workGroup[1] = 512;
    workGroup[2] = 48;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 4u);
    EXPECT_EQ(workGroupSize[1], 64u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 6;
    workGroup[1] = 4;
    workGroup[2] = 64;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 2u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 32u);

    workGroup[0] = 113;
    workGroup[1] = 113;
    workGroup[2] = 113;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 113u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, given2DimWorkGroupAndSquaredAlgorithmWhenComputeCalledThenLocalGroupComputed) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(true);

    WorkSizeInfo wsInfo(256, 0u, 32, 0u, defaultHwInfo.get(), 32u, 0u, false, false, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {384, 96, 1};
    size_t workGroupSize[3];

    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 16u);
    EXPECT_EQ(workGroupSize[1], 16u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, given1DimWorkGroupAndSquaredAlgorithmOnWhenComputeCalledThenSquaredAlgorithmIsNotExecuted) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(true);

    WorkSizeInfo wsInfo(256, 0u, 32, 0u, defaultHwInfo.get(), 32u, 0u, false, false, false);
    uint32_t workDim = 1;
    size_t workGroup[3] = {1024, 1, 1};
    size_t workGroupSize[3];

    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, given2DdispatchWithImagesAndSquaredAlgorithmOnWhenLwsIsComputedThenSquaredAlgorithmIsNotExecuted) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(true);

    WorkSizeInfo wsInfo(256, 0u, 32, 0u, defaultHwInfo.get(), 32u, 0u, true, false, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {256, 96, 1};
    size_t workGroupSize[3];

    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 64u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, givenKernelWithTileYImagesAndBarrierWhenWorkgroupSizeIsComputedThenItMimicsTilingPattern) {
    WorkSizeInfo wsInfo(256, true, 32, 0u, defaultHwInfo.get(), 32u, 0u, true, true, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {1, 1, 1};
    size_t workGroupSize[3];

    workGroup[0] = 2048;
    workGroup[1] = 2048;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 32u);
    EXPECT_EQ(workGroupSize[1], 8u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 1920;
    workGroup[1] = 1080;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 32u);
    EXPECT_EQ(workGroupSize[1], 8u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, givenKernelWithTileYImagesAndNoBarriersWhenWorkgroupSizeIsComputedThenItMimicsTilingPattern) {
    WorkSizeInfo wsInfo(256, false, 32, 0u, defaultHwInfo.get(), 32u, 0u, true, true, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {1, 1, 1};
    size_t workGroupSize[3];

    workGroup[0] = 2048;
    workGroup[1] = 2048;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 64u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 1920;
    workGroup[1] = 1080;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 64u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, givenSimd16KernelWithTileYImagesAndNoBarriersWhenWorkgroupSizeIsComputedThenItMimicsTilingPattern) {
    WorkSizeInfo wsInfo(256, false, 16, 0u, defaultHwInfo.get(), 32u, 0u, true, true, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {1, 1, 1};
    size_t workGroupSize[3];

    workGroup[0] = 2048;
    workGroup[1] = 2048;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 32u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 1920;
    workGroup[1] = 1080;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 32u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, givenKernelWithTwoDimensionalGlobalSizesWhenLwsIsComputedThenItHasMaxWorkgroupSize) {
    WorkSizeInfo wsInfo(256, 0u, 32, 0u, defaultHwInfo.get(), 32u, 0u, false, false, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {1, 1, 1};
    size_t workGroupSize[3];

    workGroup[0] = 1024;
    workGroup[1] = 1024;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, givenKernelWithBarriersAndTiledImagesWithYdimensionHigherThenXDimensionWhenLwsIsComputedThenItMimicsTiling) {
    WorkSizeInfo wsInfo(256, 0u, 32, 0u, defaultHwInfo.get(), 32u, 0u, true, true, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {1, 1, 1};
    size_t workGroupSize[3];

    workGroup[0] = 256;
    workGroup[1] = 1024;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 64u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 512;
    workGroup[1] = 2048;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 64u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 480;
    workGroup[1] = 1080;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 32u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 196;
    workGroup[1] = 30;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 49u);
    EXPECT_EQ(workGroupSize[1], 5u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, givenHighOneDimensionalGwsWhenLwsIsComputedThenMaxWorkgoupSizeIsUsed) {
    WorkSizeInfo wsInfo(256, 0u, 32, 0u, defaultHwInfo.get(), 32u, 0u, false, false, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {1, 1, 1};
    size_t workGroupSize[3];

    workGroup[0] = 65536;
    workGroup[1] = 1;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 524288;
    workGroup[1] = 1;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, givenVeriousGwsSizesWithImagesWhenLwsIsComputedThenProperSizesAreReturned) {
    WorkSizeInfo wsInfo(256, 0u, 32, 0u, defaultHwInfo.get(), 32u, 0u, true, true, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {1, 1, 1};
    size_t workGroupSize[3];

    workGroup[0] = 256;
    workGroup[1] = 1024;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 64u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 512;
    workGroup[1] = 2048;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 64u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 208;
    workGroup[1] = 2;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 16u);
    EXPECT_EQ(workGroupSize[1], 2u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 6;
    workGroup[1] = 128;
    wsInfo.simdSize = 8;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 2u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 3;
    workGroup[1] = 128;
    wsInfo.simdSize = 8;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 1u);
    EXPECT_EQ(workGroupSize[1], 128u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, givenHigh1DGwsAndSimdSize16WhenLwsIsComputedThenMaxWorkgroupSizeIsChoosen) {
    WorkSizeInfo wsInfo(256u, 0u, 16, 0u, defaultHwInfo.get(), 56u, 0, false, false, false);

    size_t workGroup[3] = {1, 1, 1};
    size_t workGroupSize[3];

    workGroup[0] = 1048576;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, 1);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, givenHigh1DGwsAndSimdSize8WhenLwsIsComputedThenMaxWorkgroupSizeIsChoosen) {
    WorkSizeInfo wsInfo(256u, 0u, 8, 0u, defaultHwInfo.get(), 32u, 0, false, false, false);

    size_t workGroup[3] = {1, 1, 1};
    size_t workGroupSize[3];

    workGroup[0] = 1048576;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, 1);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, givenKernelUtilizingImagesAndSlmWhenLwsIsBeingComputedThenItMimicsGlobalWorkgroupSizes) {
    WorkSizeInfo wsInfo(256u, 1u, 32, 4096u, defaultHwInfo.get(), 56u, 65536u, true, true, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {1, 1, 1};
    size_t workGroupSize[3];

    workGroup[0] = 2048;
    workGroup[1] = 2048;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 16u);
    EXPECT_EQ(workGroupSize[1], 16u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 1920;
    workGroup[1] = 1080;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 32u);
    EXPECT_EQ(workGroupSize[1], 8u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, GivenUseStrictRatioWhenLwsIsBeingComputedThenWgsIsCalculatedCorrectly) {
    WorkSizeInfo wsInfo(256u, 0u, 32u, 0u, defaultHwInfo.get(), 0u, 0u, true, true, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {194, 234, 1};
    size_t workGroupSize[3];

    workGroup[0] = 194;
    workGroup[1] = 234;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 2u);
    EXPECT_EQ(workGroupSize[1], 117u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 100;
    workGroup[1] = 100;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 20u);
    EXPECT_EQ(workGroupSize[1], 5u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 54;
    workGroup[1] = 154;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 27u);
    EXPECT_EQ(workGroupSize[1], 7u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, GivenUseBarriersWhenLwsIsBeingComputedThenWgsIsCalculatedCorrectly) {
    WorkSizeInfo wsInfo(256u, 1u, 32u, 0u, defaultHwInfo.get(), 56u, 0u, true, true, false);

    uint32_t workDim = 2;
    size_t workGroup[3] = {194, 234, 1};
    size_t workGroupSize[3];

    workGroup[0] = 194;
    workGroup[1] = 234;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 97u);
    EXPECT_EQ(workGroupSize[1], 2u);
    EXPECT_EQ(workGroupSize[2], 1u);
    wsInfo.useRatio = false;
    wsInfo.useStrictRatio = false;

    wsInfo.yTiledSurfaces = false;
    wsInfo.imgUsed = false;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 2u);
    EXPECT_EQ(workGroupSize[1], 78u);
    EXPECT_EQ(workGroupSize[2], 1u);
    wsInfo.useRatio = false;
    wsInfo.useStrictRatio = false;

    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 2u);
    EXPECT_EQ(workGroupSize[1], 78u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, given2DimWorkWhenComputeSquaredCalledThenLocalGroupComputed) {
    WorkSizeInfo wsInfo(256, 0u, 16, 0u, defaultHwInfo.get(), 6u, 0u, false, false, false);

    uint32_t workDim = 2;
    size_t workGroup[3] = {2048, 272, 1};
    size_t workGroupSize[3];

    NEO::computeWorkgroupSizeSquared(wsInfo.maxWorkGroupSize, workGroupSize, workGroup, wsInfo.simdSize, workDim);
    EXPECT_EQ(workGroupSize[0], 16u);
    EXPECT_EQ(workGroupSize[1], 16u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 1024;
    workGroup[1] = 1024;
    NEO::computeWorkgroupSizeSquared(wsInfo.maxWorkGroupSize, workGroupSize, workGroup, wsInfo.simdSize, workDim);
    EXPECT_EQ(workGroupSize[0], 16u);
    EXPECT_EQ(workGroupSize[1], 16u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 512;
    workGroup[1] = 104;
    NEO::computeWorkgroupSizeSquared(wsInfo.maxWorkGroupSize, workGroupSize, workGroup, wsInfo.simdSize, workDim);
    EXPECT_EQ(workGroupSize[0], 32u);
    EXPECT_EQ(workGroupSize[1], 8u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 104;
    workGroup[1] = 512;
    NEO::computeWorkgroupSizeSquared(wsInfo.maxWorkGroupSize, workGroupSize, workGroup, wsInfo.simdSize, workDim);
    EXPECT_EQ(workGroupSize[0], 8u);
    EXPECT_EQ(workGroupSize[1], 32u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 184;
    workGroup[1] = 368;
    NEO::computeWorkgroupSizeSquared(wsInfo.maxWorkGroupSize, workGroupSize, workGroup, wsInfo.simdSize, workDim);
    EXPECT_EQ(workGroupSize[0], 8u);
    EXPECT_EQ(workGroupSize[1], 16u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 113;
    workGroup[1] = 2;
    NEO::computeWorkgroupSizeSquared(wsInfo.maxWorkGroupSize, workGroupSize, workGroup, wsInfo.simdSize, workDim);
    EXPECT_EQ(workGroupSize[0], 113u);
    EXPECT_EQ(workGroupSize[1], 2u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, givenDeviceSupportingLws1024AndKernelCompiledInSimd8WhenGwsIs1024ThenLwsIsComputedAsMaxOptimalMultipliedBySimd) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(false);
    WorkSizeInfo wsInfo(1024, 0u, 8, 0u, defaultHwInfo.get(), 56u, 0u, false, false, false);

    uint32_t workDim = 2;
    size_t workGroup[3] = {32, 32, 1};
    size_t workGroupSize[3];

    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 32u);
    EXPECT_EQ(workGroupSize[1], 8u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, givenDeviceWith36ThreadsPerSubsliceWhenSimd16KernelIsBeingSubmittedThenWorkgroupContainsOf8HwThreads) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(false);
    WorkSizeInfo wsInfo(256, 0u, 16, 0u, defaultHwInfo.get(), 36u, 0u, false, false, false);

    uint32_t workDim = 2;
    size_t workGroup[3] = {1024, 1024, 1};
    size_t workGroupSize[3];

    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 128u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, givenDeviceWith56ThreadsPerSubsliceWhenSimd16KernelIsBeingSubmittedThenWorkgroupContainsOf16HwThreads) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(false);
    WorkSizeInfo wsInfo(256, 0u, 16, 0u, defaultHwInfo.get(), 56u, 0u, false, false, false);

    uint32_t workDim = 2;
    size_t workGroup[3] = {1024, 1024, 1};
    size_t workGroupSize[3];

    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, givenDispatchInfoWhenWorkSizeInfoIsCreatedThenItHasCorrectNumberOfThreads) {
    MockClDevice device{new MockDevice};
    MockKernelWithInternals kernel(device);
    DispatchInfo dispatchInfo;
    dispatchInfo.setClDevice(&device);
    dispatchInfo.setKernel(kernel.mockKernel);

    auto threadsPerEu = defaultHwInfo->gtSystemInfo.ThreadCount / defaultHwInfo->gtSystemInfo.EUCount;
    auto euPerSubSlice = defaultHwInfo->gtSystemInfo.ThreadCount / defaultHwInfo->gtSystemInfo.MaxEuPerSubSlice;

    auto &deviceInfo = device.sharedDeviceInfo;
    deviceInfo.maxNumEUsPerSubSlice = euPerSubSlice;
    deviceInfo.numThreadsPerEU = threadsPerEu;

    WorkSizeInfo workSizeInfo = createWorkSizeInfoFromDispatchInfo(dispatchInfo);
    EXPECT_EQ(workSizeInfo.numThreadsPerSubSlice, threadsPerEu * euPerSubSlice);
}

using LocalWorkSizeTest = ::testing::Test;

HWTEST2_F(LocalWorkSizeTest, givenDispatchInfoWhenWorkSizeInfoIsCreatedThenWorkgroupSizeIsCorrect, IsAtMostGen11) {
    MockClDevice device{new MockDevice};
    MockKernelWithInternals kernel(device);
    kernel.kernelInfo.kernelDescriptor.kernelAttributes.barrierCount = 1;
    DispatchInfo dispatchInfo;
    dispatchInfo.setClDevice(&device);
    dispatchInfo.setKernel(kernel.mockKernel);

    const uint32_t maxBarriersPerHSlice = (defaultHwInfo->platform.eRenderCoreFamily >= IGFX_GEN9_CORE) ? 32 : 16;
    const uint32_t nonFusedMinWorkGroupSize = static_cast<uint32_t>(device.getSharedDeviceInfo().maxNumEUsPerSubSlice) *
                                              device.getSharedDeviceInfo().numThreadsPerEU *
                                              static_cast<uint32_t>(kernel.mockKernel->getKernelInfo().getMaxSimdSize()) /
                                              maxBarriersPerHSlice;
    WorkSizeInfo workSizeInfo = createWorkSizeInfoFromDispatchInfo(dispatchInfo);

    EXPECT_EQ(nonFusedMinWorkGroupSize, workSizeInfo.minWorkGroupSize);
}

using IsCoreWithFusedEu = IsWithinGfxCore<IGFX_GEN12LP_CORE, IGFX_XE_HP_CORE>;

HWTEST2_F(LocalWorkSizeTest, givenDispatchInfoWhenWorkSizeInfoIsCreatedThenTestEuFusionFtr, IsCoreWithFusedEu) {
    MockClDevice device{new MockDevice};
    MockKernelWithInternals kernel(device);
    kernel.kernelInfo.kernelDescriptor.kernelAttributes.barrierCount = 1;
    DispatchInfo dispatchInfo;
    dispatchInfo.setClDevice(&device);
    dispatchInfo.setKernel(kernel.mockKernel);

    const uint32_t maxBarriersPerHSlice = (defaultHwInfo->platform.eRenderCoreFamily >= IGFX_GEN9_CORE) ? 32 : 16;
    const uint32_t nonFusedMinWorkGroupSize = static_cast<uint32_t>(device.getSharedDeviceInfo().maxNumEUsPerSubSlice) *
                                              device.getSharedDeviceInfo().numThreadsPerEU *
                                              static_cast<uint32_t>(kernel.mockKernel->getKernelInfo().getMaxSimdSize()) /
                                              maxBarriersPerHSlice;
    const uint32_t fusedMinWorkGroupSize = 2 * nonFusedMinWorkGroupSize;
    WorkSizeInfo workSizeInfo = createWorkSizeInfoFromDispatchInfo(dispatchInfo);

    EXPECT_NE(nonFusedMinWorkGroupSize, workSizeInfo.minWorkGroupSize);
    EXPECT_EQ(fusedMinWorkGroupSize, workSizeInfo.minWorkGroupSize);
}

HWTEST2_F(LocalWorkSizeTest, givenDispatchInfoWhenWorkSizeInfoIsCreatedThenTestEuFusionFtrForcedByDebugManager, IsAtLeastGen12lp) {
    DebugManagerStateRestore dbgRestore;
    MockClDevice device{new MockDevice};
    MockKernelWithInternals kernel(device);
    kernel.kernelInfo.kernelDescriptor.kernelAttributes.barrierCount = 1;
    DispatchInfo dispatchInfo;
    dispatchInfo.setClDevice(&device);
    dispatchInfo.setKernel(kernel.mockKernel);

    const uint32_t nonFusedMinWorkGroupSize = static_cast<uint32_t>(device.getSharedDeviceInfo().maxNumEUsPerSubSlice) *
                                              device.getSharedDeviceInfo().numThreadsPerEU *
                                              static_cast<uint32_t>(kernel.mockKernel->getKernelInfo().getMaxSimdSize()) /
                                              32;
    const uint32_t fusedMinWorkGroupSize = 2 * nonFusedMinWorkGroupSize;
    EXPECT_NE(0u, nonFusedMinWorkGroupSize);

    {
        const bool fusedEuDispatchDisabled = true;
        DebugManager.flags.CFEFusedEUDispatch.set(fusedEuDispatchDisabled);
        WorkSizeInfo workSizeInfo = createWorkSizeInfoFromDispatchInfo(dispatchInfo);
        EXPECT_EQ(nonFusedMinWorkGroupSize, workSizeInfo.minWorkGroupSize);
    }

    {
        const bool fusedEuDispatchDisabled = false;
        DebugManager.flags.CFEFusedEUDispatch.set(fusedEuDispatchDisabled);
        WorkSizeInfo workSizeInfo = createWorkSizeInfoFromDispatchInfo(dispatchInfo);
        EXPECT_EQ(fusedMinWorkGroupSize, workSizeInfo.minWorkGroupSize);
    }
}

HWTEST2_F(LocalWorkSizeTest, givenWorkSizeInfoIsCreatedWithHwInfoThenTestEuFusionFtrForcedByDebugManager, IsAtLeastGen12lp) {
    DebugManagerStateRestore dbgRestore;

    const uint32_t nonFusedMinWorkGroupSize = 36 * 16 / 32;
    const uint32_t fusedMinWorkGroupSize = 2 * nonFusedMinWorkGroupSize;
    EXPECT_NE(0u, nonFusedMinWorkGroupSize);

    {
        const bool fusedEuDispatchDisabled = true;
        DebugManager.flags.CFEFusedEUDispatch.set(fusedEuDispatchDisabled);
        WorkSizeInfo workSizeInfo(512, 1u, 16, 0u, defaultHwInfo.get(), 36u, 0u, false, false, false);
        EXPECT_EQ(nonFusedMinWorkGroupSize, workSizeInfo.minWorkGroupSize);
    }

    {
        const bool fusedEuDispatchDisabled = false;
        DebugManager.flags.CFEFusedEUDispatch.set(fusedEuDispatchDisabled);
        WorkSizeInfo workSizeInfo(512, 1u, 16, 0u, defaultHwInfo.get(), 36u, 0u, false, false, false);
        EXPECT_EQ(fusedMinWorkGroupSize, workSizeInfo.minWorkGroupSize);
    }
}

TEST(localWorkSizeTest, givenDispatchInfoWhenWorkSizeInfoIsCreatedThenHasBarriersIsCorrectlySet) {
    MockClDevice device{new MockDevice};
    MockKernelWithInternals kernel(device);
    DispatchInfo dispatchInfo;
    dispatchInfo.setClDevice(&device);
    dispatchInfo.setKernel(kernel.mockKernel);

    kernel.kernelInfo.kernelDescriptor.kernelAttributes.barrierCount = 0;
    EXPECT_FALSE(createWorkSizeInfoFromDispatchInfo(dispatchInfo).hasBarriers);

    kernel.kernelInfo.kernelDescriptor.kernelAttributes.barrierCount = 1;
    EXPECT_TRUE(createWorkSizeInfoFromDispatchInfo(dispatchInfo).hasBarriers);
}

TEST(localWorkSizeTest, givenMaxWorkgroupSizeEqualToSimdSizeWhenLwsIsCalculatedThenItIsDownsizedToMaxWorkgroupSize) {
    WorkSizeInfo wsInfo(32, 0u, 32, 0u, defaultHwInfo.get(), 32u, 0u, false, false, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {32, 32, 1};
    size_t workGroupSize[3];

    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 32u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}
