/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/gpgpu_walker.h"
#include "runtime/helpers/options.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_kernel.h"

#include "gtest/gtest.h"

using namespace OCLRT;

TEST(localWorkSizeTest, given1DimWorkGroupAndSimdEqual8WhenComputeCalledThenLocalGroupComputed) {
    //wsInfo maxWorkGroupSize, hasBariers, simdSize, slmTotalSize, coreFamily, numThreadsPerSubSlice, localMemorySize, imgUsed, yTiledSurface
    WorkSizeInfo wsInfo(256, 0u, 8, 0u, platformDevices[0]->pPlatform->eRenderCoreFamily, 32u, 0u, false, false);
    uint32_t workDim = 1;
    size_t workGroup[3] = {6144, 1, 1};
    size_t workGroupSize[3];

    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 48;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 16u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 1536;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 333;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 9u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, given1DimWorkGroupAndSimdEqual32WhenComputeCalledThenLocalGroupComputed) {
    WorkSizeInfo wsInfo(256, 0u, 32, 0u, platformDevices[0]->pPlatform->eRenderCoreFamily, 32u, 0u, false, false);
    uint32_t workDim = 1;
    size_t workGroup[3] = {6144, 1, 1};
    size_t workGroupSize[3];

    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 48;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 48u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 512;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, given2DimWorkGroupAndSimdEqual8WhenComputeCalledThenLocalGroupComputed) {
    WorkSizeInfo wsInfo(256, 0u, 8, 0u, platformDevices[0]->pPlatform->eRenderCoreFamily, 56u, 0u, false, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {384, 96, 1};
    size_t workGroupSize[3];

    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 128u);
    EXPECT_EQ(workGroupSize[1], 2u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 48;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 16u);
    ;
    EXPECT_EQ(workGroupSize[1], 16u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 12;
    workGroup[1] = 512;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 4u);
    EXPECT_EQ(workGroupSize[1], 64u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, given2DimWorkGroupAndSimdEqual32WhenComputeCalledThenLocalGroupComputed) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(false);
    WorkSizeInfo wsInfo(256, 0u, 32, 0u, platformDevices[0]->pPlatform->eRenderCoreFamily, 32u, 0u, false, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {384, 96, 1};
    size_t workGroupSize[3];

    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 128u);
    EXPECT_EQ(workGroupSize[1], 2u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 48;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 16u);
    EXPECT_EQ(workGroupSize[1], 16u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 12;
    workGroup[1] = 512;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 4u);
    EXPECT_EQ(workGroupSize[1], 64u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 1;
    workGroup[1] = 384;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 1u);
    EXPECT_EQ(workGroupSize[1], 128u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, given3DimWorkGroupAndSimdEqual8WhenComputeCalledThenLocalGroupComputed) {
    WorkSizeInfo wsInfo(256, 0u, 8, 0u, platformDevices[0]->pPlatform->eRenderCoreFamily, 56u, 0u, false, false);
    uint32_t workDim = 3;
    size_t workGroup[3] = {384, 384, 384};
    size_t workGroupSize[3];

    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 128u);
    EXPECT_EQ(workGroupSize[1], 2u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 96;
    workGroup[1] = 4;
    workGroup[2] = 4;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 32u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 2u);

    workGroup[0] = 12;
    workGroup[1] = 512;
    workGroup[2] = 48;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 4u);
    EXPECT_EQ(workGroupSize[1], 64u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 2;
    workGroup[1] = 2;
    workGroup[2] = 3;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 2u);
    EXPECT_EQ(workGroupSize[1], 2u);
    EXPECT_EQ(workGroupSize[2], 3u);
}

TEST(localWorkSizeTest, given3DimWorkGroupAndSimdEqual32WhenComputeCalledThenLocalGroupComputed) {
    OCLRT::WorkSizeInfo wsInfo(256, 0u, 32, 0u, platformDevices[0]->pPlatform->eRenderCoreFamily, 32u, 0u, false, false);
    uint32_t workDim = 3;
    size_t workGroup[3] = {384, 384, 384};
    size_t workGroupSize[3];
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 128u);
    EXPECT_EQ(workGroupSize[1], 2u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 96;
    workGroup[1] = 6;
    workGroup[2] = 4;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 32u);
    EXPECT_EQ(workGroupSize[1], 2u);
    EXPECT_EQ(workGroupSize[2], 4u);

    workGroup[0] = 12;
    workGroup[1] = 512;
    workGroup[2] = 48;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 4u);
    EXPECT_EQ(workGroupSize[1], 64u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 6;
    workGroup[1] = 4;
    workGroup[2] = 64;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 2u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 32u);

    workGroup[0] = 113;
    workGroup[1] = 113;
    workGroup[2] = 113;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 113u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, given2DimWorkGroupAndSquaredAlgorithmWhenComputeCalledThenLocalGroupComputed) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(true);

    WorkSizeInfo wsInfo(256, 0u, 32, 0u, platformDevices[0]->pPlatform->eRenderCoreFamily, 32u, 0u, false, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {384, 96, 1};
    size_t workGroupSize[3];

    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 16u);
    EXPECT_EQ(workGroupSize[1], 16u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, given1DimWorkGroupAndSquaredAlgorithmOnWhenComputeCalledThenSquaredAlgorithmIsNotExecuted) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(true);

    WorkSizeInfo wsInfo(256, 0u, 32, 0u, platformDevices[0]->pPlatform->eRenderCoreFamily, 32u, 0u, false, false);
    uint32_t workDim = 1;
    size_t workGroup[3] = {1024, 1, 1};
    size_t workGroupSize[3];

    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, given2DdispatchWithImagesAndSquaredAlgorithmOnWhenLwsIsComputedThenSquaredAlgorithmIsNotExecuted) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(true);

    WorkSizeInfo wsInfo(256, 0u, 32, 0u, platformDevices[0]->pPlatform->eRenderCoreFamily, 32u, 0u, true, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {256, 96, 1};
    size_t workGroupSize[3];

    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 64u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, givenKernelWithTileYImagesAndBarrierWhenWorkgroupSizeIsComputedThenItMimicsTilingPattern) {
    WorkSizeInfo wsInfo(256, true, 32, 0u, platformDevices[0]->pPlatform->eRenderCoreFamily, 32u, 0u, true, true);
    uint32_t workDim = 2;
    size_t workGroup[3] = {1, 1, 1};
    size_t workGroupSize[3];

    workGroup[0] = 2048;
    workGroup[1] = 2048;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 32u);
    EXPECT_EQ(workGroupSize[1], 8u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 1920;
    workGroup[1] = 1080;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 32u);
    EXPECT_EQ(workGroupSize[1], 8u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, givenKernelWithTileYImagesAndNoBarriersWhenWorkgroupSizeIsComputedThenItMimicsTilingPattern) {
    WorkSizeInfo wsInfo(256, false, 32, 0u, platformDevices[0]->pPlatform->eRenderCoreFamily, 32u, 0u, true, true);
    uint32_t workDim = 2;
    size_t workGroup[3] = {1, 1, 1};
    size_t workGroupSize[3];

    workGroup[0] = 2048;
    workGroup[1] = 2048;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 64u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 1920;
    workGroup[1] = 1080;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 64u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, givenSimd16KernelWithTileYImagesAndNoBarriersWhenWorkgroupSizeIsComputedThenItMimicsTilingPattern) {
    WorkSizeInfo wsInfo(256, false, 16, 0u, platformDevices[0]->pPlatform->eRenderCoreFamily, 32u, 0u, true, true);
    uint32_t workDim = 2;
    size_t workGroup[3] = {1, 1, 1};
    size_t workGroupSize[3];

    workGroup[0] = 2048;
    workGroup[1] = 2048;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 32u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 1920;
    workGroup[1] = 1080;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 32u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, givenKernelWithTwoDimensionalGlobalSizesWhenLwsIsComputedThenItHasMaxWorkgroupSize) {
    WorkSizeInfo wsInfo(256, 0u, 32, 0u, platformDevices[0]->pPlatform->eRenderCoreFamily, 32u, 0u, false, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {1, 1, 1};
    size_t workGroupSize[3];

    workGroup[0] = 1024;
    workGroup[1] = 1024;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, givenKernelWithBarriersAndTiledImagesWithYdimensionHigherThenXDimensionWhenLwsIsComputedThenItMimicsTiling) {
    WorkSizeInfo wsInfo(256, 0u, 32, 0u, platformDevices[0]->pPlatform->eRenderCoreFamily, 32u, 0u, true, true);
    uint32_t workDim = 2;
    size_t workGroup[3] = {1, 1, 1};
    size_t workGroupSize[3];

    workGroup[0] = 256;
    workGroup[1] = 1024;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 64u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 512;
    workGroup[1] = 2048;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 64u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 480;
    workGroup[1] = 1080;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 32u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 196;
    workGroup[1] = 30;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 49u);
    EXPECT_EQ(workGroupSize[1], 5u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, givenHighOneDimensionalGwsWhenLwsIsComputedThenMaxWorkgoupSizeIsUsed) {
    WorkSizeInfo wsInfo(256, 0u, 32, 0u, platformDevices[0]->pPlatform->eRenderCoreFamily, 32u, 0u, false, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {1, 1, 1};
    size_t workGroupSize[3];

    workGroup[0] = 65536;
    workGroup[1] = 1;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 524288;
    workGroup[1] = 1;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, givenVeriousGwsSizesWithImagesWhenLwsIsComputedThenProperSizesAreReturned) {
    WorkSizeInfo wsInfo(256, 0u, 32, 0u, platformDevices[0]->pPlatform->eRenderCoreFamily, 32u, 0u, true, true);
    uint32_t workDim = 2;
    size_t workGroup[3] = {1, 1, 1};
    size_t workGroupSize[3];

    workGroup[0] = 256;
    workGroup[1] = 1024;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 64u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 512;
    workGroup[1] = 2048;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 64u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 208;
    workGroup[1] = 2;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 16u);
    EXPECT_EQ(workGroupSize[1], 2u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 6;
    workGroup[1] = 128;
    wsInfo.simdSize = 8;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 2u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 3;
    workGroup[1] = 128;
    wsInfo.simdSize = 8;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 1u);
    EXPECT_EQ(workGroupSize[1], 128u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, givenHigh1DGwsAndSimdSize16WhenLwsIsComputedThenMaxWorkgroupSizeIsChoosen) {
    WorkSizeInfo wsInfo(256u, 0u, 16, 0u, platformDevices[0]->pPlatform->eRenderCoreFamily, 56u, 0, false, false);

    size_t workGroup[3] = {1, 1, 1};
    size_t workGroupSize[3];

    workGroup[0] = 1048576;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, 1);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, givenHigh1DGwsAndSimdSize8WhenLwsIsComputedThenMaxWorkgroupSizeIsChoosen) {
    WorkSizeInfo wsInfo(256u, 0u, 8, 0u, platformDevices[0]->pPlatform->eRenderCoreFamily, 32u, 0, false, false);

    size_t workGroup[3] = {1, 1, 1};
    size_t workGroupSize[3];

    workGroup[0] = 1048576;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, 1);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, givenKernelUtilizingImagesAndSlmWhenLwsIsBeingComputedThenItMimicsGlobalWorkgroupSizes) {
    WorkSizeInfo wsInfo(256u, 1u, 32, 4096u, platformDevices[0]->pPlatform->eRenderCoreFamily, 56u, 65536u, true, true);
    uint32_t workDim = 2;
    size_t workGroup[3] = {1, 1, 1};
    size_t workGroupSize[3];

    workGroup[0] = 2048;
    workGroup[1] = 2048;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 16u);
    EXPECT_EQ(workGroupSize[1], 16u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 1920;
    workGroup[1] = 1080;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 32u);
    EXPECT_EQ(workGroupSize[1], 8u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, useStrictRatio) {
    WorkSizeInfo wsInfo(256u, 0u, 32u, 0u, platformDevices[0]->pPlatform->eRenderCoreFamily, 0u, 0u, true, true);
    uint32_t workDim = 2;
    size_t workGroup[3] = {194, 234, 1};
    size_t workGroupSize[3];

    workGroup[0] = 194;
    workGroup[1] = 234;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 2u);
    EXPECT_EQ(workGroupSize[1], 117u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 100;
    workGroup[1] = 100;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 20u);
    EXPECT_EQ(workGroupSize[1], 5u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 54;
    workGroup[1] = 154;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 27u);
    EXPECT_EQ(workGroupSize[1], 7u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, useBarriers) {
    WorkSizeInfo wsInfo(256u, 1u, 32u, 0u, platformDevices[0]->pPlatform->eRenderCoreFamily, 56u, 0u, true, true);
    uint32_t workDim = 2;
    size_t workGroup[3] = {194, 234, 1};
    size_t workGroupSize[3];

    workGroup[0] = 194;
    workGroup[1] = 234;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 97u);
    EXPECT_EQ(workGroupSize[1], 2u);
    EXPECT_EQ(workGroupSize[2], 1u);

    wsInfo.yTiledSurfaces = false;
    wsInfo.imgUsed = false;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 2u);
    EXPECT_EQ(workGroupSize[1], 78u);
    EXPECT_EQ(workGroupSize[2], 1u);

    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 2u);
    EXPECT_EQ(workGroupSize[1], 78u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, given2DimWorkWhenComputeSquaredCalledThenLocalGroupComputed) {
    WorkSizeInfo wsInfo(256, 0u, 16, 0u, platformDevices[0]->pPlatform->eRenderCoreFamily, 6u, 0u, false, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {2048, 272, 1};
    size_t workGroupSize[3];

    OCLRT::computeWorkgroupSizeSquared(wsInfo.maxWorkGroupSize, workGroupSize, workGroup, wsInfo.simdSize, workDim);
    EXPECT_EQ(workGroupSize[0], 16u);
    EXPECT_EQ(workGroupSize[1], 16u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 1024;
    workGroup[1] = 1024;
    OCLRT::computeWorkgroupSizeSquared(wsInfo.maxWorkGroupSize, workGroupSize, workGroup, wsInfo.simdSize, workDim);
    EXPECT_EQ(workGroupSize[0], 16u);
    EXPECT_EQ(workGroupSize[1], 16u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 512;
    workGroup[1] = 104;
    OCLRT::computeWorkgroupSizeSquared(wsInfo.maxWorkGroupSize, workGroupSize, workGroup, wsInfo.simdSize, workDim);
    EXPECT_EQ(workGroupSize[0], 32u);
    EXPECT_EQ(workGroupSize[1], 8u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 104;
    workGroup[1] = 512;
    OCLRT::computeWorkgroupSizeSquared(wsInfo.maxWorkGroupSize, workGroupSize, workGroup, wsInfo.simdSize, workDim);
    EXPECT_EQ(workGroupSize[0], 8u);
    EXPECT_EQ(workGroupSize[1], 32u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 184;
    workGroup[1] = 368;
    OCLRT::computeWorkgroupSizeSquared(wsInfo.maxWorkGroupSize, workGroupSize, workGroup, wsInfo.simdSize, workDim);
    EXPECT_EQ(workGroupSize[0], 8u);
    EXPECT_EQ(workGroupSize[1], 16u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 113;
    workGroup[1] = 2;
    OCLRT::computeWorkgroupSizeSquared(wsInfo.maxWorkGroupSize, workGroupSize, workGroup, wsInfo.simdSize, workDim);
    EXPECT_EQ(workGroupSize[0], 113u);
    EXPECT_EQ(workGroupSize[1], 2u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, givenDeviceSupportingLws1024AndKernelCompiledInSimd8WhenGwsIs1024ThenLwsIsComputedAsMaxOptimalMultipliedBySimd) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(false);
    WorkSizeInfo wsInfo(1024, 0u, 8, 0u, platformDevices[0]->pPlatform->eRenderCoreFamily, 56u, 0u, false, false);

    uint32_t workDim = 2;
    size_t workGroup[3] = {32, 32, 1};
    size_t workGroupSize[3];

    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 32u);
    EXPECT_EQ(workGroupSize[1], 8u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, givenDeviceWith36ThreadsPerSubsliceWhenSimd16KernelIsBeingSubmittedThenWorkgroupContainsOf8HwThreads) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(false);
    WorkSizeInfo wsInfo(256, 0u, 16, 0u, platformDevices[0]->pPlatform->eRenderCoreFamily, 36u, 0u, false, false);

    uint32_t workDim = 2;
    size_t workGroup[3] = {1024, 1024, 1};
    size_t workGroupSize[3];

    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 128u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, givenDeviceWith56ThreadsPerSubsliceWhenSimd16KernelIsBeingSubmittedThenWorkgroupContainsOf16HwThreads) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(false);
    WorkSizeInfo wsInfo(256, 0u, 16, 0u, platformDevices[0]->pPlatform->eRenderCoreFamily, 56u, 0u, false, false);

    uint32_t workDim = 2;
    size_t workGroup[3] = {1024, 1024, 1};
    size_t workGroupSize[3];

    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, givenDispatchInfoWhenWorkSizeInfoIsCreatedThenItHasCorrectNumberOfThreads) {
    MockDevice device(*platformDevices[0]);
    MockKernelWithInternals kernel(device);
    DispatchInfo dispatchInfo;
    dispatchInfo.setKernel(kernel.mockKernel);

    auto threadsPerEu = platformDevices[0]->pSysInfo->ThreadCount / platformDevices[0]->pSysInfo->EUCount;
    auto euPerSubSlice = platformDevices[0]->pSysInfo->ThreadCount / platformDevices[0]->pSysInfo->MaxEuPerSubSlice;

    auto deviceInfo = device.getMutableDeviceInfo();
    deviceInfo->maxNumEUsPerSubSlice = euPerSubSlice;
    deviceInfo->numThreadsPerEU = threadsPerEu;

    WorkSizeInfo workSizeInfo(dispatchInfo);
    EXPECT_EQ(workSizeInfo.numThreadsPerSubSlice, threadsPerEu * euPerSubSlice);
}
TEST(localWorkSizeTest, givenMaxWorkgroupSizeEqualToSimdSizeWhenLwsIsCalculatedThenItIsDownsizedToMaxWorkgroupSize) {
    WorkSizeInfo wsInfo(32, 0u, 32, 0u, platformDevices[0]->pPlatform->eRenderCoreFamily, 32u, 0u, false, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {32, 32, 1};
    size_t workGroupSize[3];

    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 32u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}
