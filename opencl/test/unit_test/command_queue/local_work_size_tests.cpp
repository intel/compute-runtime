/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/local_work_size.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/command_queue/cl_local_work_size.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

using namespace NEO;

struct LocalWorkSizeTest : public ::testing::Test {

    MockExecutionEnvironment mockExecutionEnvironment{};
    RootDeviceEnvironment &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0].get();
};

TEST_F(LocalWorkSizeTest, givenDisableEUFusionWhenCreatingWorkSizeInfoThenCorrectMinWorkGroupSizeIsSet) {

    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();

    uint32_t simdSize = 8u;
    uint32_t numThreadsPerSubS = 8u;
    WorkSizeInfo wsInfo(256,                   // maxWorkGroupSize
                        1u,                    // hasBariers
                        simdSize,              // simdSize
                        0u,                    // slmTotalSize
                        rootDeviceEnvironment, // rootDeviceEnvironment
                        numThreadsPerSubS,     // numThreadsPerSubS
                        0u,                    // localMemorySize
                        false,                 // imgUsed
                        false,                 // yTiledSurface
                        true                   // disableEUFusion
    );

    bool fusedDispatchEnabled = gfxCoreHelper.isFusedEuDispatchEnabled(*defaultHwInfo, true);
    auto wgsMultiple = fusedDispatchEnabled ? 2 : 1;

    uint32_t maxBarriersPerHSlice = 32;
    uint32_t expectedMinWGS = wgsMultiple * simdSize * numThreadsPerSubS / maxBarriersPerHSlice;
    EXPECT_EQ(expectedMinWGS, wsInfo.minWorkGroupSize);
}

TEST_F(LocalWorkSizeTest, GivenSlmLargerThanLocalThenWarningIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.PrintDebugMessages.set(true);
    ::testing::internal::CaptureStderr();

    EXPECT_THROW(WorkSizeInfo wsInfo(256,                   // maxWorkGroupSize
                                     1u,                    // hasBariers
                                     8,                     // simdSize
                                     128u,                  // slmTotalSize
                                     rootDeviceEnvironment, // rootDeviceEnvironment
                                     32u,                   // numThreadsPerSubSlice
                                     64u,                   // localMemorySize
                                     false,                 // imgUsed
                                     false,                 // yTiledSurface
                                     false                  // disableEUFusion
                                     ),
                 std::exception);

    std::string output = testing::internal::GetCapturedStderr();
    EXPECT_EQ(std::string("Size of SLM (128) larger than available (64)\n"), output);
}

TEST_F(LocalWorkSizeTest, GivenSlmSmallerThanLocalThenWarningIsNotReturned) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.PrintDebugMessages.set(true);
    ::testing::internal::CaptureStderr();

    WorkSizeInfo wsInfo(256,                   // maxWorkGroupSize
                        1u,                    // hasBariers
                        8,                     // simdSize
                        64u,                   // slmTotalSize
                        rootDeviceEnvironment, // rootDeviceEnvironment
                        32u,                   // numThreadsPerSubSlice
                        128u,                  // localMemorySize
                        false,                 // imgUsed
                        false,                 // yTiledSurface
                        false                  // disableEUFusion
    );

    std::string output = testing::internal::GetCapturedStderr();
    EXPECT_EQ(std::string(""), output);
}

TEST_F(LocalWorkSizeTest, whenSettingHasBarriersWithNoFusedDispatchThenMinWorkGroupSizeIsSetCorrectly) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.CFEFusedEUDispatch.set(0);

    WorkSizeInfo wsInfo0(256,                   // maxWorkGroupSize
                         0u,                    // hasBariers
                         8,                     // simdSize
                         0u,                    // slmTotalSize
                         rootDeviceEnvironment, // rootDeviceEnvironment
                         32u,                   // numThreadsPerSubSlice
                         128u,                  // localMemorySize
                         false,                 // imgUsed
                         false,                 // yTiledSurface
                         false                  // disableEUFusion
    );
    EXPECT_EQ(0u, wsInfo0.minWorkGroupSize);

    WorkSizeInfo wsInfo1(256,                   // maxWorkGroupSize
                         1u,                    // hasBariers
                         8,                     // simdSize
                         0u,                    // slmTotalSize
                         rootDeviceEnvironment, // rootDeviceEnvironment
                         32u,                   // numThreadsPerSubSlice
                         128u,                  // localMemorySize
                         false,                 // imgUsed
                         false,                 // yTiledSurface
                         false                  // disableEUFusion
    );
    EXPECT_NE(0u, wsInfo1.minWorkGroupSize);
}

TEST_F(LocalWorkSizeTest, given3DimWorkGroupAndSimdEqual8AndBarriersWhenComputeCalledThenLocalGroupComputedCorrectly) {
    WorkSizeInfo wsInfo(256,                   // maxWorkGroupSize
                        1u,                    // hasBariers
                        8,                     // simdSize
                        0u,                    // slmTotalSize
                        rootDeviceEnvironment, // rootDeviceEnvironment
                        32u,                   // numThreadsPerSubSlice
                        0u,                    // localMemorySize
                        false,                 // imgUsed
                        false,                 // yTiledSurface
                        false                  // disableEUFusion
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

TEST_F(LocalWorkSizeTest, givenSmallerLocalMemSizeThanSlmTotalSizeThenExceptionIsThrown) {
    EXPECT_THROW(WorkSizeInfo wsInfo(256,                   // maxWorkGroupSize
                                     1u,                    // hasBariers
                                     8,                     // simdSize
                                     128u,                  // slmTotalSize
                                     rootDeviceEnvironment, // rootDeviceEnvironment
                                     32u,                   // numThreadsPerSubSlice
                                     64u,                   // localMemorySize
                                     false,                 // imgUsed
                                     false,                 // yTiledSurface
                                     false                  // disableEUFusion
                                     ),
                 std::exception);
}

TEST_F(LocalWorkSizeTest, given2DimWorkGroupAndSimdEqual8AndNoBarriersWhenComputeCalledThenLocalGroupComputedCorrectly) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableComputeWorkSizeSquared.set(true);

    // wsInfo maxWorkGroupSize, hasBariers, simdSize, slmTotalSize, rootDeviceEnvironment, numThreadsPerSubSlice, localMemorySize, imgUsed, yTiledSurface, disableEUFusion
    WorkSizeInfo wsInfo(256, 0u, 8, 0u, rootDeviceEnvironment, 32u, 0u, false, false, false);
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

TEST_F(LocalWorkSizeTest, given1DimWorkGroupAndSimdEqual8WhenComputeCalledThenLocalGroupComputed) {
    // wsInfo maxWorkGroupSize, hasBariers, simdSize, slmTotalSize, hardwareInfo, numThreadsPerSubSlice, localMemorySize, imgUsed, yTiledSurface, disableEUFusion
    WorkSizeInfo wsInfo(256, 0u, 8, 0u, rootDeviceEnvironment, 32u, 0u, false, false, false);
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

TEST_F(LocalWorkSizeTest, given1DimWorkGroupAndSimdEqual32WhenComputeCalledThenLocalGroupComputed) {
    WorkSizeInfo wsInfo(256, 0u, 32, 0u, rootDeviceEnvironment, 32u, 0u, false, false, false);
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

TEST_F(LocalWorkSizeTest, given2DimWorkGroupAndSimdEqual8WhenComputeCalledThenLocalGroupComputed) {
    WorkSizeInfo wsInfo(256, 0u, 8, 0u, rootDeviceEnvironment, 56u, 0u, false, false, false);
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

TEST_F(LocalWorkSizeTest, given2DimWorkGroupAndSimdEqual32WhenComputeCalledThenLocalGroupComputed) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableComputeWorkSizeSquared.set(false);
    WorkSizeInfo wsInfo(256, 0u, 32, 0u, rootDeviceEnvironment, 32u, 0u, false, false, false);

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

TEST_F(LocalWorkSizeTest, givenSimdEqual32AndPreferredWgCountPerSubslice2WhenComputeCalledThenLocalGroupSizeIsLimited) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableComputeWorkSizeSquared.set(false);
    WorkSizeInfo wsInfo(1024, 0u, 32, 0u, rootDeviceEnvironment, 32u, 0u, false, false, false);
    wsInfo.setPreferredWgCountPerSubslice(2);

    constexpr uint32_t maxLws = 32 * 32 / 2; // simd size * num threadsPerSubslice / preferredWgCountPerSubslice

    uint32_t workDim = 2;
    size_t workGroup[3] = {384, 96, 1};
    size_t workGroupSize[3];

    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 128u);
    EXPECT_EQ(workGroupSize[1], 2u);
    EXPECT_EQ(workGroupSize[2], 1u);
    EXPECT_TRUE(workGroupSize[0] * workGroupSize[1] < maxLws);

    workGroup[0] = 1024 * 256;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
    EXPECT_TRUE(workGroupSize[0] * workGroupSize[1] <= maxLws);

    workGroup[0] = 48;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 16u);
    EXPECT_EQ(workGroupSize[1], 32u);
    EXPECT_EQ(workGroupSize[2], 1u);
    EXPECT_TRUE(workGroupSize[0] * workGroupSize[1] <= maxLws);

    workGroup[0] = 512;
    workGroup[1] = 1;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 512u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
    EXPECT_TRUE(workGroupSize[0] * workGroupSize[1] <= maxLws);

    workGroup[0] = 12;
    workGroup[1] = 512;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 4u);
    EXPECT_EQ(workGroupSize[1], 64u);
    EXPECT_EQ(workGroupSize[2], 1u);
    EXPECT_TRUE(workGroupSize[0] * workGroupSize[1] <= maxLws);

    workGroup[0] = 1;
    workGroup[1] = 384;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 1u);
    EXPECT_EQ(workGroupSize[1], 128u);
    EXPECT_EQ(workGroupSize[2], 1u);
    EXPECT_TRUE(workGroupSize[0] * workGroupSize[1] <= maxLws);

    workGroup[0] = 128;
    workGroup[1] = 4;
    wsInfo.imgUsed = true;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 128u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);
    EXPECT_TRUE(workGroupSize[0] * workGroupSize[1] <= maxLws);

    workGroup[0] = 64;
    workGroup[1] = 8;
    wsInfo.imgUsed = false;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 64u);
    EXPECT_EQ(workGroupSize[1], 8u);
    EXPECT_EQ(workGroupSize[2], 1u);
    EXPECT_TRUE(workGroupSize[0] * workGroupSize[1] <= maxLws);

    workGroup[0] = 1024;
    workGroup[1] = 9;
    wsInfo.imgUsed = true;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 512u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
    EXPECT_TRUE(workGroupSize[0] * workGroupSize[1] <= maxLws);
}

TEST_F(LocalWorkSizeTest, givenSimdEqual32AndPreferredWgCountPerSubslice4WhenComputeCalledThenLocalGroupSizeIsLimited) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableComputeWorkSizeSquared.set(false);
    WorkSizeInfo wsInfo(1024, 0u, 32, 0u, rootDeviceEnvironment, 32u, 0u, false, false, false);
    wsInfo.setPreferredWgCountPerSubslice(4);

    constexpr uint32_t maxLws = 32 * 32 / 4; // simd size * num threadsPerSubslice / preferredWgCountPerSubslice

    uint32_t workDim = 2;
    size_t workGroup[3] = {384, 96, 1};
    size_t workGroupSize[3];

    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 128u);
    EXPECT_EQ(workGroupSize[1], 2u);
    EXPECT_EQ(workGroupSize[2], 1u);
    EXPECT_TRUE(workGroupSize[0] * workGroupSize[1] <= maxLws);

    workGroup[0] = 1024 * 256;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
    EXPECT_TRUE(workGroupSize[0] * workGroupSize[1] <= maxLws);

    workGroup[0] = 48;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 16u);
    EXPECT_EQ(workGroupSize[1], 16u);
    EXPECT_EQ(workGroupSize[2], 1u);
    EXPECT_TRUE(workGroupSize[0] * workGroupSize[1] <= maxLws);

    workGroup[0] = 512;
    workGroup[1] = 1;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
    EXPECT_TRUE(workGroupSize[0] * workGroupSize[1] <= maxLws);

    workGroup[0] = 12;
    workGroup[1] = 512;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 4u);
    EXPECT_EQ(workGroupSize[1], 64u);
    EXPECT_EQ(workGroupSize[2], 1u);
    EXPECT_TRUE(workGroupSize[0] * workGroupSize[1] <= maxLws);

    workGroup[0] = 1;
    workGroup[1] = 384;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 1u);
    EXPECT_EQ(workGroupSize[1], 128u);
    EXPECT_EQ(workGroupSize[2], 1u);
    EXPECT_TRUE(workGroupSize[0] * workGroupSize[1] <= maxLws);

    workGroup[0] = 128;
    workGroup[1] = 4;
    wsInfo.imgUsed = true;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 64u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);
    EXPECT_TRUE(workGroupSize[0] * workGroupSize[1] <= maxLws);

    workGroup[0] = 64;
    workGroup[1] = 8;
    wsInfo.imgUsed = false;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 64u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);
    EXPECT_TRUE(workGroupSize[0] * workGroupSize[1] <= maxLws);

    workGroup[0] = 1024;
    workGroup[1] = 9;
    wsInfo.imgUsed = true;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
    EXPECT_TRUE(workGroupSize[0] * workGroupSize[1] <= maxLws);

    workGroup[0] = 2048;
    workGroup[1] = 1;
    wsInfo.imgUsed = false;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
    EXPECT_TRUE(workGroupSize[0] * workGroupSize[1] <= maxLws);
}

TEST_F(LocalWorkSizeTest, givenSimdEqual32AndPreferredWgCountPerSubslice4WhenBarriersOrSlmUsedThenLocalGroupSizeIsNotLimited) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableComputeWorkSizeSquared.set(false);
    WorkSizeInfo wsInfo(1024, 0u, 32, 0u, rootDeviceEnvironment, 32u, 0u, false, false, false);
    wsInfo.setPreferredWgCountPerSubslice(4);

    for (uint32_t i = 0; i < 2; i++) {
        if (i == 0) {
            wsInfo.hasBarriers = true;
        } else if (i == 1) {
            wsInfo.hasBarriers = false;
            wsInfo.slmTotalSize = 256;
        }

        uint32_t workDim = 2;
        size_t workGroup[3] = {384, 96, 1};
        size_t workGroupSize[3];

        NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
        if (wsInfo.slmTotalSize == 0) {
            EXPECT_EQ(workGroupSize[0], 384u);
            EXPECT_EQ(workGroupSize[1], 2u);
            EXPECT_EQ(workGroupSize[2], 1u);
        } else {
            // use ratio in algorithm
            EXPECT_EQ(workGroupSize[0], 64u);
            EXPECT_EQ(workGroupSize[1], 16u);
            EXPECT_EQ(workGroupSize[2], 1u);
        }

        workGroup[0] = 1024 * 256;
        NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
        EXPECT_EQ(workGroupSize[0], 512u);
        EXPECT_EQ(workGroupSize[1], 2u);
        EXPECT_EQ(workGroupSize[2], 1u);

        workGroup[0] = 48;
        NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
        EXPECT_EQ(workGroupSize[0], 48u);
        EXPECT_EQ(workGroupSize[1], 16u);
        EXPECT_EQ(workGroupSize[2], 1u);

        workGroup[0] = 128;
        workGroup[1] = 4;
        wsInfo.imgUsed = true;
        NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
        EXPECT_EQ(workGroupSize[0], 128u);
        EXPECT_EQ(workGroupSize[1], 4u);
        EXPECT_EQ(workGroupSize[2], 1u);

        workGroup[0] = 1024;
        workGroup[1] = 9;
        wsInfo.imgUsed = false;
        NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
        if (wsInfo.slmTotalSize == 0) {
            EXPECT_EQ(workGroupSize[0], 512u);
            EXPECT_EQ(workGroupSize[1], 1u);
            EXPECT_EQ(workGroupSize[2], 1u);
        } else {
            EXPECT_EQ(workGroupSize[0], 256u);
            EXPECT_EQ(workGroupSize[1], 3u);
            EXPECT_EQ(workGroupSize[2], 1u);
        }

        workGroup[0] = 2048;
        workGroup[1] = 2;
        wsInfo.imgUsed = false;
        NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
        EXPECT_EQ(workGroupSize[0], 512u);
        EXPECT_EQ(workGroupSize[1], 2u);
        EXPECT_EQ(workGroupSize[2], 1u);
    }
}

TEST_F(LocalWorkSizeTest, given3DimWorkGroupAndSimdEqual8WhenComputeCalledThenLocalGroupComputed) {
    WorkSizeInfo wsInfo(256, 0u, 8, 0u, rootDeviceEnvironment, 56u, 0u, false, false, false);
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

TEST_F(LocalWorkSizeTest, given3DimWorkGroupAndSimdEqual32WhenComputeCalledThenLocalGroupComputed) {
    NEO::WorkSizeInfo wsInfo(256, 0u, 32, 0u, rootDeviceEnvironment, 32u, 0u, false, false, false);
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

TEST_F(LocalWorkSizeTest, given2DimWorkGroupAndSquaredAlgorithmWhenComputeCalledThenLocalGroupComputed) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableComputeWorkSizeSquared.set(true);

    WorkSizeInfo wsInfo(256, 0u, 32, 0u, rootDeviceEnvironment, 32u, 0u, false, false, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {384, 96, 1};
    size_t workGroupSize[3];

    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 16u);
    EXPECT_EQ(workGroupSize[1], 16u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST_F(LocalWorkSizeTest, given1DimWorkGroupAndSquaredAlgorithmOnWhenComputeCalledThenSquaredAlgorithmIsNotExecuted) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableComputeWorkSizeSquared.set(true);

    WorkSizeInfo wsInfo(256, 0u, 32, 0u, rootDeviceEnvironment, 32u, 0u, false, false, false);
    uint32_t workDim = 1;
    size_t workGroup[3] = {1024, 1, 1};
    size_t workGroupSize[3];

    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST_F(LocalWorkSizeTest, given2DdispatchWithImagesAndSquaredAlgorithmOnWhenLwsIsComputedThenSquaredAlgorithmIsNotExecuted) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableComputeWorkSizeSquared.set(true);

    WorkSizeInfo wsInfo(256, 0u, 32, 0u, rootDeviceEnvironment, 32u, 0u, true, false, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {256, 96, 1};
    size_t workGroupSize[3];

    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 64u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST_F(LocalWorkSizeTest, givenKernelWithTileYImagesAndBarrierWhenWorkgroupSizeIsComputedThenItMimicsTilingPattern) {
    WorkSizeInfo wsInfo(256, true, 32, 0u, rootDeviceEnvironment, 32u, 0u, true, true, false);
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

TEST_F(LocalWorkSizeTest, givenKernelWithTileYImagesAndNoBarriersWhenWorkgroupSizeIsComputedThenItMimicsTilingPattern) {
    WorkSizeInfo wsInfo(256, false, 32, 0u, rootDeviceEnvironment, 32u, 0u, true, true, false);
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

TEST_F(LocalWorkSizeTest, givenSimd16KernelWithTileYImagesAndNoBarriersWhenWorkgroupSizeIsComputedThenItMimicsTilingPattern) {
    WorkSizeInfo wsInfo(256, false, 16, 0u, rootDeviceEnvironment, 32u, 0u, true, true, false);
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

TEST_F(LocalWorkSizeTest, givenKernelWithTwoDimensionalGlobalSizesWhenLwsIsComputedThenItHasMaxWorkgroupSize) {
    WorkSizeInfo wsInfo(256, 0u, 32, 0u, rootDeviceEnvironment, 32u, 0u, false, false, false);
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

TEST_F(LocalWorkSizeTest, givenKernelWithBarriersAndTiledImagesWithYdimensionHigherThenXDimensionWhenLwsIsComputedThenItMimicsTiling) {
    WorkSizeInfo wsInfo(256, 0u, 32, 0u, rootDeviceEnvironment, 32u, 0u, true, true, false);
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

TEST_F(LocalWorkSizeTest, givenHighOneDimensionalGwsWhenLwsIsComputedThenMaxWorkgoupSizeIsUsed) {
    WorkSizeInfo wsInfo(256, 0u, 32, 0u, rootDeviceEnvironment, 32u, 0u, false, false, false);
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

TEST_F(LocalWorkSizeTest, givenVeriousGwsSizesWithImagesWhenLwsIsComputedThenProperSizesAreReturned) {
    WorkSizeInfo wsInfo(256, 0u, 32, 0u, rootDeviceEnvironment, 32u, 0u, true, true, false);
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

TEST_F(LocalWorkSizeTest, givenHigh1DGwsAndSimdSize16WhenLwsIsComputedThenMaxWorkgroupSizeIsChoosen) {
    WorkSizeInfo wsInfo(256u, 0u, 16, 0u, rootDeviceEnvironment, 56u, 0, false, false, false);

    size_t workGroup[3] = {1, 1, 1};
    size_t workGroupSize[3];

    workGroup[0] = 1048576;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, 1);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST_F(LocalWorkSizeTest, givenHigh1DGwsAndSimdSize8WhenLwsIsComputedThenMaxWorkgroupSizeIsChoosen) {
    WorkSizeInfo wsInfo(256u, 0u, 8, 0u, rootDeviceEnvironment, 32u, 0, false, false, false);

    size_t workGroup[3] = {1, 1, 1};
    size_t workGroupSize[3];

    workGroup[0] = 1048576;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, 1);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST_F(LocalWorkSizeTest, givenKernelUtilizingImagesAndSlmWhenLwsIsBeingComputedThenItMimicsGlobalWorkgroupSizes) {
    WorkSizeInfo wsInfo(256u, 1u, 32, 4096u, rootDeviceEnvironment, 56u, 65536u, true, true, false);
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

TEST_F(LocalWorkSizeTest, GivenUseStrictRatioWhenLwsIsBeingComputedThenWgsIsCalculatedCorrectly) {
    WorkSizeInfo wsInfo(256u, 0u, 32u, 0u, rootDeviceEnvironment, 0u, 0u, true, true, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {194, 234, 1};
    size_t workGroupSize[3];

    workGroup[0] = 194;
    workGroup[1] = 234;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 97u);
    EXPECT_EQ(workGroupSize[1], 2u);
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

TEST_F(LocalWorkSizeTest, GivenUseBarriersWhenLwsIsBeingComputedThenWgsIsCalculatedCorrectly) {
    WorkSizeInfo wsInfo(256u, 1u, 32u, 0u, rootDeviceEnvironment, 56u, 0u, true, true, false);

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
    EXPECT_EQ(workGroupSize[0], 194u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);

    wsInfo.useRatio = false;
    wsInfo.useStrictRatio = false;
    workDim = 3;
    workGroup[2] = 4;
    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 194u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST_F(LocalWorkSizeTest, given2DimWorkWhenComputeSquaredCalledThenLocalGroupComputed) {
    WorkSizeInfo wsInfo(256, 0u, 16, 0u, rootDeviceEnvironment, 6u, 0u, false, false, false);

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

TEST_F(LocalWorkSizeTest, givenDeviceSupportingLws1024AndKernelCompiledInSimd8WhenGwsIs1024ThenLwsIsComputedAsMaxOptimalMultipliedBySimd) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableComputeWorkSizeSquared.set(false);
    WorkSizeInfo wsInfo(1024, 0u, 8, 0u, rootDeviceEnvironment, 56u, 0u, false, false, false);

    uint32_t workDim = 2;
    size_t workGroup[3] = {32, 32, 1};
    size_t workGroupSize[3];

    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 32u);
    EXPECT_EQ(workGroupSize[1], 8u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST_F(LocalWorkSizeTest, givenDeviceWith36ThreadsPerSubsliceWhenSimd16KernelIsBeingSubmittedThenWorkgroupContainsOf8HwThreads) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableComputeWorkSizeSquared.set(false);
    WorkSizeInfo wsInfo(256, 0u, 16, 0u, rootDeviceEnvironment, 36u, 0u, false, false, false);

    uint32_t workDim = 2;
    size_t workGroup[3] = {1024, 1024, 1};
    size_t workGroupSize[3];

    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 128u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST_F(LocalWorkSizeTest, givenDeviceWith56ThreadsPerSubsliceWhenSimd16KernelIsBeingSubmittedThenWorkgroupContainsOf16HwThreads) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableComputeWorkSizeSquared.set(false);
    WorkSizeInfo wsInfo(256, 0u, 16, 0u, rootDeviceEnvironment, 56u, 0u, false, false, false);

    uint32_t workDim = 2;
    size_t workGroup[3] = {1024, 1024, 1};
    size_t workGroupSize[3];

    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST_F(LocalWorkSizeTest, givenDispatchInfoWhenWorkSizeInfoIsCreatedThenItHasCorrectNumberOfThreads) {
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

using IsCoreWithFusedEu = IsWithinGfxCore<IGFX_GEN12LP_CORE, IGFX_XE_HP_CORE>;

HWTEST2_F(LocalWorkSizeTest, givenDispatchInfoWhenWorkSizeInfoIsCreatedThenTestEuFusionFtr, IsCoreWithFusedEu) {
    MockClDevice device{new MockDevice};
    MockKernelWithInternals kernel(device);
    kernel.kernelInfo.kernelDescriptor.kernelAttributes.barrierCount = 1;
    DispatchInfo dispatchInfo;
    dispatchInfo.setClDevice(&device);
    dispatchInfo.setKernel(kernel.mockKernel);

    const uint32_t maxBarriersPerHSlice = 32;
    const uint32_t nonFusedMinWorkGroupSize = static_cast<uint32_t>(device.getSharedDeviceInfo().maxNumEUsPerSubSlice) *
                                              device.getSharedDeviceInfo().numThreadsPerEU *
                                              static_cast<uint32_t>(kernel.mockKernel->getKernelInfo().getMaxSimdSize()) /
                                              maxBarriersPerHSlice;
    const uint32_t fusedMinWorkGroupSize = 2 * nonFusedMinWorkGroupSize;
    WorkSizeInfo workSizeInfo = createWorkSizeInfoFromDispatchInfo(dispatchInfo);

    EXPECT_NE(nonFusedMinWorkGroupSize, workSizeInfo.minWorkGroupSize);
    EXPECT_EQ(fusedMinWorkGroupSize, workSizeInfo.minWorkGroupSize);
}

HWTEST2_F(LocalWorkSizeTest, givenDispatchInfoWhenWorkSizeInfoIsCreatedThenTestEuFusionFtrForcedByDebugManager, MatchAny) {
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
        debugManager.flags.CFEFusedEUDispatch.set(fusedEuDispatchDisabled);
        WorkSizeInfo workSizeInfo = createWorkSizeInfoFromDispatchInfo(dispatchInfo);
        EXPECT_EQ(nonFusedMinWorkGroupSize, workSizeInfo.minWorkGroupSize);
    }

    {
        const bool fusedEuDispatchDisabled = false;
        debugManager.flags.CFEFusedEUDispatch.set(fusedEuDispatchDisabled);
        WorkSizeInfo workSizeInfo = createWorkSizeInfoFromDispatchInfo(dispatchInfo);
        EXPECT_EQ(fusedMinWorkGroupSize, workSizeInfo.minWorkGroupSize);
    }
}

HWTEST2_F(LocalWorkSizeTest, givenWorkSizeInfoIsCreatedWithHwInfoThenTestEuFusionFtrForcedByDebugManager, MatchAny) {
    DebugManagerStateRestore dbgRestore;

    const uint32_t nonFusedMinWorkGroupSize = 36 * 16 / 32;
    const uint32_t fusedMinWorkGroupSize = 2 * nonFusedMinWorkGroupSize;
    EXPECT_NE(0u, nonFusedMinWorkGroupSize);

    {
        const bool fusedEuDispatchDisabled = true;
        debugManager.flags.CFEFusedEUDispatch.set(fusedEuDispatchDisabled);
        WorkSizeInfo workSizeInfo(512, 1u, 16, 0u, rootDeviceEnvironment, 36u, 0u, false, false, false);
        EXPECT_EQ(nonFusedMinWorkGroupSize, workSizeInfo.minWorkGroupSize);
    }

    {
        const bool fusedEuDispatchDisabled = false;
        debugManager.flags.CFEFusedEUDispatch.set(fusedEuDispatchDisabled);
        WorkSizeInfo workSizeInfo(512, 1u, 16, 0u, rootDeviceEnvironment, 36u, 0u, false, false, false);
        EXPECT_EQ(fusedMinWorkGroupSize, workSizeInfo.minWorkGroupSize);
    }
}

TEST_F(LocalWorkSizeTest, givenDispatchInfoWhenWorkSizeInfoIsCreatedThenHasBarriersIsCorrectlySet) {
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

TEST_F(LocalWorkSizeTest, givenMaxWorkgroupSizeEqualToSimdSizeWhenLwsIsCalculatedThenItIsDownsizedToMaxWorkgroupSize) {
    WorkSizeInfo wsInfo(32, 0u, 32, 0u, rootDeviceEnvironment, 32u, 0u, false, false, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {32, 32, 1};
    size_t workGroupSize[3];

    NEO::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 32u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST_F(LocalWorkSizeTest, givenGwsWithSmallXAndBigYWhenLwsIsCalculatedThenDescendingOrderIsNotEnforced) {
    WorkSizeInfo wsInfo(256u, true, 32u, 0u, rootDeviceEnvironment, 0u, 0u, false, false, false);

    uint32_t workDim = 3;
    size_t workGroup[3] = {2, 1024, 1};
    size_t workGroupSize[3];

    NEO::choosePrefferedWorkgroupSize(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 2u);
    EXPECT_EQ(workGroupSize[1], 128u);
    EXPECT_EQ(workGroupSize[2], 1u);

    // Enforce strict ratio requirement
    wsInfo.yTiledSurfaces = true;
    NEO::choosePrefferedWorkgroupSize(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 2u);
    EXPECT_EQ(workGroupSize[1], 128u);
    EXPECT_EQ(workGroupSize[2], 1u);

    // Enforce ratio requirement
    wsInfo.yTiledSurfaces = false;
    wsInfo.slmTotalSize = 128U;
    NEO::choosePrefferedWorkgroupSize(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 2u);
    EXPECT_EQ(workGroupSize[1], 128u);
    EXPECT_EQ(workGroupSize[2], 1u);
}
