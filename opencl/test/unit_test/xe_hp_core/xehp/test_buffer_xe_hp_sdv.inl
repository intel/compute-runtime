/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
using XeHpSdvBufferTests = ::testing::Test;

XEHPTEST_F(XeHpSdvBufferTests, givenContextTypeDefaultWhenBufferIsWritableAndOnlyOneTileIsAvailableThenRemainFlagsToTrue) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(1);
    initPlatform();
    EXPECT_EQ(0u, platform()->getClDevice(0)->getNumGenericSubDevices());
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    MockContext context(platform()->getClDevice(0));
    context.contextType = ContextType::CONTEXT_TYPE_DEFAULT;

    size_t size = 0x1000;
    auto retVal = CL_SUCCESS;
    auto buffer = std::unique_ptr<Buffer>(
        Buffer::create(
            &context,
            CL_MEM_READ_WRITE,
            size,
            nullptr,
            retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    RENDER_SURFACE_STATE surfaceState = FamilyType::cmdInitRenderSurfaceState;
    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuAtomics());
    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuPartialWrites());

    surfaceState.setDisableSupportForMultiGpuAtomics(false);
    surfaceState.setDisableSupportForMultiGpuPartialWrites(false);
    buffer->setArgStateful(&surfaceState, false, false, false, false, context.getDevice(0)->getDevice(), false, false);

    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuAtomics());
    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuPartialWrites());
}

XEHPTEST_F(XeHpSdvBufferTests, givenContextTypeDefaultWhenBufferIsWritableThenFlipPartialFlagsToFalse) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(4);
    initPlatform();

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    MockContext context(platform()->getClDevice(0));
    context.contextType = ContextType::CONTEXT_TYPE_DEFAULT;

    size_t size = 0x1000;
    auto retVal = CL_SUCCESS;
    auto buffer = std::unique_ptr<Buffer>(
        Buffer::create(
            &context,
            CL_MEM_READ_WRITE,
            size,
            nullptr,
            retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    RENDER_SURFACE_STATE surfaceState = FamilyType::cmdInitRenderSurfaceState;
    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuAtomics());
    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuPartialWrites());

    buffer->setArgStateful(&surfaceState, false, false, false, false, context.getDevice(0)->getDevice(), true, true);

    EXPECT_FALSE(surfaceState.getDisableSupportForMultiGpuAtomics());
    EXPECT_FALSE(surfaceState.getDisableSupportForMultiGpuPartialWrites());
}

XEHPTEST_F(XeHpSdvBufferTests, givenContextTypeUnrestrictiveWhenBufferIsWritableThenFlipPartialFlagsToFalse) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(4);
    initPlatform();

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    MockContext context(platform()->getClDevice(0));
    context.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;

    size_t size = 0x1000;
    auto retVal = CL_SUCCESS;
    auto buffer = std::unique_ptr<Buffer>(
        Buffer::create(
            &context,
            CL_MEM_READ_WRITE,
            size,
            nullptr,
            retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    RENDER_SURFACE_STATE surfaceState = FamilyType::cmdInitRenderSurfaceState;
    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuAtomics());
    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuPartialWrites());

    buffer->setArgStateful(&surfaceState, false, false, false, false, context.getDevice(0)->getDevice(), true, true);

    EXPECT_FALSE(surfaceState.getDisableSupportForMultiGpuAtomics());
    EXPECT_FALSE(surfaceState.getDisableSupportForMultiGpuPartialWrites());
}

XEHPTEST_F(XeHpSdvBufferTests, givenContextTypeDefaultWhenBufferIsNotWritableThenRemainPartialFlagsToTrue) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    MockContext context;
    context.contextType = ContextType::CONTEXT_TYPE_DEFAULT;

    size_t size = 0x1000;
    auto retVal = CL_SUCCESS;

    auto buffer = std::unique_ptr<Buffer>(Buffer::create(
        &context,
        CL_MEM_READ_ONLY,
        size,
        nullptr,
        retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    RENDER_SURFACE_STATE surfaceState = FamilyType::cmdInitRenderSurfaceState;
    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuAtomics());
    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuPartialWrites());

    surfaceState.setDisableSupportForMultiGpuAtomics(false);
    surfaceState.setDisableSupportForMultiGpuPartialWrites(false);
    buffer->setArgStateful(&surfaceState, false, false, false, false, context.getDevice(0)->getDevice(), true, false);

    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuAtomics());
    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuPartialWrites());
}

XEHPTEST_F(XeHpSdvBufferTests, givenContextTypeSpecializedWhenBufferIsWritableThenRemainPartialFlagsToTrue) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    MockContext context;
    context.contextType = ContextType::CONTEXT_TYPE_SPECIALIZED;

    size_t size = 0x1000;
    auto retVal = CL_SUCCESS;

    auto buffer = std::unique_ptr<Buffer>(Buffer::create(
        &context,
        CL_MEM_READ_WRITE,
        size,
        nullptr,
        retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    RENDER_SURFACE_STATE surfaceState = FamilyType::cmdInitRenderSurfaceState;
    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuAtomics());
    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuPartialWrites());

    surfaceState.setDisableSupportForMultiGpuAtomics(false);
    surfaceState.setDisableSupportForMultiGpuPartialWrites(false);
    buffer->setArgStateful(&surfaceState, false, false, false, false, context.getDevice(0)->getDevice(), false, false);

    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuAtomics());
    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuPartialWrites());
}

XEHPTEST_F(XeHpSdvBufferTests, givenDebugFlagForMultiTileSupportWhenSurfaceStateIsSetThenValuesMatch) {
    DebugManagerStateRestore restore;
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    MockContext context;
    context.contextType = ContextType::CONTEXT_TYPE_SPECIALIZED;

    size_t size = 0x1000;
    auto retVal = CL_SUCCESS;

    auto buffer = std::unique_ptr<Buffer>(Buffer::create(
        &context,
        CL_MEM_READ_WRITE,
        size,
        nullptr,
        retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    RENDER_SURFACE_STATE surfaceState = FamilyType::cmdInitRenderSurfaceState;

    DebugManager.flags.ForceMultiGpuAtomics.set(0);
    DebugManager.flags.ForceMultiGpuPartialWrites.set(0);

    buffer->setArgStateful(&surfaceState, false, false, false, false, context.getDevice(0)->getDevice(), false, false);

    EXPECT_EQ(0u, surfaceState.getDisableSupportForMultiGpuAtomics());
    EXPECT_EQ(0u, surfaceState.getDisableSupportForMultiGpuPartialWrites());

    DebugManager.flags.ForceMultiGpuAtomics.set(1);
    DebugManager.flags.ForceMultiGpuPartialWrites.set(1);

    buffer->setArgStateful(&surfaceState, false, false, false, false, context.getDevice(0)->getDevice(), false, false);

    EXPECT_EQ(1u, surfaceState.getDisableSupportForMultiGpuAtomics());
    EXPECT_EQ(1u, surfaceState.getDisableSupportForMultiGpuPartialWrites());
}

XEHPTEST_F(XeHpSdvBufferTests, givenNullContextWhenBufferAllocationIsNullThenRemainPartialFlagsToTrue) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE surfaceState = FamilyType::cmdInitRenderSurfaceState;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    auto size = MemoryConstants::pageSize;
    auto ptr = alignedMalloc(size, MemoryConstants::pageSize);

    surfaceState.setDisableSupportForMultiGpuAtomics(false);
    surfaceState.setDisableSupportForMultiGpuPartialWrites(false);
    Buffer::setSurfaceState(device.get(), &surfaceState, false, false, size, ptr, 0, nullptr, 0, 0, false, false);

    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuAtomics());
    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuPartialWrites());

    alignedFree(ptr);
}

struct MultiGpuGlobalAtomicsBufferTest : public XeHpSdvBufferTests,
                                         public ::testing::WithParamInterface<std::tuple<unsigned int, unsigned int, bool, bool, bool>> {
};

XEHPTEST_P(MultiGpuGlobalAtomicsBufferTest, givenSetArgStatefulCalledThenDisableSupportForMultiGpuAtomicsIsSetCorrectly) {
    unsigned int numAvailableDevices, bufferFlags;
    bool useGlobalAtomics, areMultipleSubDevicesInContext, enableMultiGpuAtomicsOptimization;
    std::tie(numAvailableDevices, bufferFlags, useGlobalAtomics, areMultipleSubDevicesInContext, enableMultiGpuAtomicsOptimization) = GetParam();

    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(numAvailableDevices);
    DebugManager.flags.EnableMultiGpuAtomicsOptimization.set(enableMultiGpuAtomicsOptimization);
    initPlatform();

    if (numAvailableDevices == 1) {
        EXPECT_EQ(0u, platform()->getClDevice(0)->getNumGenericSubDevices());
    } else {
        EXPECT_EQ(numAvailableDevices, platform()->getClDevice(0)->getNumGenericSubDevices());
    }
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    MockContext context(platform()->getClDevice(0));
    context.contextType = ContextType::CONTEXT_TYPE_DEFAULT;

    size_t size = 0x1000;
    auto retVal = CL_SUCCESS;
    auto buffer = std::unique_ptr<Buffer>(
        Buffer::create(
            &context,
            bufferFlags,
            size,
            nullptr,
            retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    RENDER_SURFACE_STATE surfaceState = FamilyType::cmdInitRenderSurfaceState;
    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuAtomics());

    surfaceState.setDisableSupportForMultiGpuAtomics(false);
    buffer->setArgStateful(&surfaceState, false, false, false, false, context.getDevice(0)->getDevice(), useGlobalAtomics, areMultipleSubDevicesInContext);

    DeviceBitfield deviceBitfield{static_cast<uint32_t>(maxNBitValue(numAvailableDevices))};
    bool implicitScaling = ImplicitScalingHelper::isImplicitScalingEnabled(deviceBitfield, true);
    bool enabled = implicitScaling;

    if (enableMultiGpuAtomicsOptimization) {
        enabled = useGlobalAtomics && (enabled || areMultipleSubDevicesInContext);
    }

    EXPECT_EQ(!enabled, surfaceState.getDisableSupportForMultiGpuAtomics());
}

XEHPTEST_P(MultiGpuGlobalAtomicsBufferTest, givenSetSurfaceStateCalledThenDisableSupportForMultiGpuAtomicsIsSetCorrectly) {
    unsigned int numAvailableDevices, bufferFlags;
    bool useGlobalAtomics, areMultipleSubDevicesInContext, enableMultiGpuAtomicsOptimization;
    std::tie(numAvailableDevices, bufferFlags, useGlobalAtomics, areMultipleSubDevicesInContext, enableMultiGpuAtomicsOptimization) = GetParam();

    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(numAvailableDevices);
    DebugManager.flags.EnableMultiGpuAtomicsOptimization.set(enableMultiGpuAtomicsOptimization);
    initPlatform();
    if (numAvailableDevices == 1) {
        EXPECT_EQ(0u, platform()->getClDevice(0)->getNumGenericSubDevices());
    } else {
        EXPECT_EQ(numAvailableDevices, platform()->getClDevice(0)->getNumGenericSubDevices());
    }

    auto size = MemoryConstants::pageSize;
    auto ptr = alignedMalloc(size, MemoryConstants::pageSize);
    MockGraphicsAllocation gfxAllocation(ptr, size);
    gfxAllocation.setMemObjectsAllocationWithWritableFlags(bufferFlags == CL_MEM_READ_WRITE);

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE surfaceState = FamilyType::cmdInitRenderSurfaceState;
    EXPECT_TRUE(surfaceState.getDisableSupportForMultiGpuAtomics());

    surfaceState.setDisableSupportForMultiGpuAtomics(false);
    Buffer::setSurfaceState(&platform()->getClDevice(0)->getDevice(), &surfaceState, false, false, 0, nullptr, 0, &gfxAllocation, bufferFlags, 0, useGlobalAtomics, areMultipleSubDevicesInContext);

    DeviceBitfield deviceBitfield{static_cast<uint32_t>(maxNBitValue(numAvailableDevices))};
    bool implicitScaling = ImplicitScalingHelper::isImplicitScalingEnabled(deviceBitfield, true);
    bool enabled = implicitScaling;

    if (enableMultiGpuAtomicsOptimization) {
        enabled = useGlobalAtomics && (enabled || areMultipleSubDevicesInContext);
    }

    EXPECT_EQ(!enabled, surfaceState.getDisableSupportForMultiGpuAtomics());

    alignedFree(ptr);
}

static unsigned int numAvailableDevices[] = {1, 2};
static unsigned int bufferFlags[] = {CL_MEM_READ_ONLY, CL_MEM_READ_WRITE};

INSTANTIATE_TEST_CASE_P(MultiGpuGlobalAtomicsBufferTest,
                        MultiGpuGlobalAtomicsBufferTest,
                        ::testing::Combine(
                            ::testing::ValuesIn(numAvailableDevices),
                            ::testing::ValuesIn(bufferFlags),
                            ::testing::Bool(),
                            ::testing::Bool(),
                            ::testing::Bool()));