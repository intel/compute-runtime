/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using namespace NEO;

struct BufferTestsTgllp : ::testing::Test {
    void SetUp() override {
        context = std::make_unique<MockContext>();
        device = context->getDevice(0);
    }

    std::unique_ptr<MockContext> context{};
    ClDevice *device{};

    cl_int retVal = CL_SUCCESS;
};

GEN12LPTEST_F(BufferTestsTgllp, givenBufferNotReadonlyWhenProgrammingSurfaceStateThenUseL3) {
    auto buffer = std::unique_ptr<Buffer>(Buffer::create(
        context.get(),
        CL_MEM_READ_WRITE,
        MemoryConstants::pageSize,
        nullptr,
        retVal));
    ASSERT_EQ(CL_SUCCESS, retVal);

    typename FamilyType::RENDER_SURFACE_STATE surfaceState = {};
    buffer->setArgStateful(&surfaceState, false, false, false, false, device->getDevice(), false);

    const auto expectedMocs = device->getGmmHelper()->getL3EnabledMOCS();
    const auto actualMocs = surfaceState.getMemoryObjectControlState();
    EXPECT_EQ(expectedMocs, actualMocs);
}

GEN12LPTEST_F(BufferTestsTgllp, givenBufferReadonlyWhenProgrammingSurfaceStateThenUseL3) {
    auto buffer = std::unique_ptr<Buffer>(Buffer::create(
        context.get(),
        CL_MEM_READ_WRITE,
        MemoryConstants::pageSize,
        nullptr,
        retVal));
    ASSERT_EQ(CL_SUCCESS, retVal);

    typename FamilyType::RENDER_SURFACE_STATE surfaceState = {};
    buffer->setArgStateful(&surfaceState, false, false, false, true, context->getDevice(0)->getDevice(), false);

    const auto expectedMocs = device->getGmmHelper()->getL3EnabledMOCS();
    const auto actualMocs = surfaceState.getMemoryObjectControlState();
    EXPECT_EQ(expectedMocs, actualMocs);
}

GEN12LPTEST_F(BufferTestsTgllp, givenConstantSurfaceWhenProgrammingSurfaceStateThenUseL3) {
    auto buffer = std::unique_ptr<Buffer>(Buffer::create(
        context.get(),
        CL_MEM_READ_WRITE,
        MemoryConstants::pageSize,
        nullptr,
        retVal));
    ASSERT_EQ(CL_SUCCESS, retVal);
    buffer->getGraphicsAllocation(0)->setAllocationType(AllocationType::constantSurface);

    typename FamilyType::RENDER_SURFACE_STATE surfaceState = {};
    buffer->setArgStateful(&surfaceState, false, false, false, false, context->getDevice(0)->getDevice(), false);

    const auto expectedMocs = device->getGmmHelper()->getL3EnabledMOCS();
    const auto actualMocs = surfaceState.getMemoryObjectControlState();
    EXPECT_EQ(expectedMocs, actualMocs);
}

GEN12LPTEST_F(BufferTestsTgllp, givenL1ForceEnabledWhenProgrammingSurfaceStateThenUseL3) {
    DebugManagerStateRestore restore{};
    debugManager.flags.ForceL1Caching.set(1);

    auto buffer = std::unique_ptr<Buffer>(Buffer::create(
        context.get(),
        CL_MEM_READ_WRITE,
        MemoryConstants::pageSize,
        nullptr,
        retVal));
    ASSERT_EQ(CL_SUCCESS, retVal);

    typename FamilyType::RENDER_SURFACE_STATE surfaceState = {};
    buffer->setArgStateful(&surfaceState, false, false, false, false, device->getDevice(), false);

    const auto expectedMocs = device->getGmmHelper()->getL3EnabledMOCS();
    const auto actualMocs = surfaceState.getMemoryObjectControlState();
    EXPECT_EQ(expectedMocs, actualMocs);
}

GEN12LPTEST_F(BufferTestsTgllp, givenBufferReadonlyAndL1ForceEnabledWhenProgrammingSurfaceStateThenUseL1) {
    DebugManagerStateRestore restore{};
    debugManager.flags.ForceL1Caching.set(1);

    auto buffer = std::unique_ptr<Buffer>(Buffer::create(
        context.get(),
        CL_MEM_READ_ONLY,
        MemoryConstants::pageSize,
        nullptr,
        retVal));
    ASSERT_EQ(CL_SUCCESS, retVal);

    typename FamilyType::RENDER_SURFACE_STATE surfaceState = {};
    buffer->setArgStateful(&surfaceState, false, false, false, false, device->getDevice(), false);

    const auto expectedMocs = device->getGmmHelper()->getL1EnabledMOCS();
    const auto actualMocs = surfaceState.getMemoryObjectControlState();
    EXPECT_EQ(expectedMocs, actualMocs);
}

GEN12LPTEST_F(BufferTestsTgllp, givenBufferReadonlyL1ForceDisabledWhenProgrammingSurfaceStateThenUseL3) {
    DebugManagerStateRestore restore{};
    debugManager.flags.ForceL1Caching.set(0);

    auto buffer = std::unique_ptr<Buffer>(Buffer::create(
        context.get(),
        CL_MEM_READ_WRITE,
        MemoryConstants::pageSize,
        nullptr,
        retVal));
    ASSERT_EQ(CL_SUCCESS, retVal);

    typename FamilyType::RENDER_SURFACE_STATE surfaceState = {};
    buffer->setArgStateful(&surfaceState, false, false, false, true, device->getDevice(), false);

    const auto expectedMocs = device->getGmmHelper()->getL3EnabledMOCS();
    const auto actualMocs = surfaceState.getMemoryObjectControlState();
    EXPECT_EQ(expectedMocs, actualMocs);
}
using Gen12lpCreateBufferTest = ::testing::Test;
GEN12LPTEST_F(Gen12lpCreateBufferTest, WhenCreatingBufferWithCopyHostPtrThenDontUseBlitOperation) {
    uint32_t hostPtr = 0;
    auto rootDeviceIndex = 1u;

    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    MockExecutionEnvironment mockExecutionEnvironment{&hwInfo, false, 2};
    auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHelper<ProductHelper>();
    EXPECT_FALSE(productHelper.isBlitterFullySupported(hwInfo));
    std::unique_ptr<MockClDevice> newDevice = std::make_unique<MockClDevice>(MockClDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, rootDeviceIndex));
    std::unique_ptr<BcsMockContext> newBcsMockContext = std::make_unique<BcsMockContext>(newDevice.get());

    auto bcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(newBcsMockContext->bcsCsr.get());

    static_cast<MockMemoryManager *>(newDevice->getExecutionEnvironment()->memoryManager.get())->enable64kbpages[rootDeviceIndex] = true;
    static_cast<MockMemoryManager *>(newDevice->getExecutionEnvironment()->memoryManager.get())->localMemorySupported[rootDeviceIndex] = true;

    EXPECT_EQ(0u, bcsCsr->blitBufferCalled);
    cl_int retVal = 0;
    auto bufferForBlt = clUniquePtr(Buffer::create(newBcsMockContext.get(), CL_MEM_COPY_HOST_PTR, sizeof(hostPtr), &hostPtr, retVal));
    EXPECT_EQ(0u, bcsCsr->blitBufferCalled);
}
