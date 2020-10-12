/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "test.h"

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
    buffer->setArgStateful(&surfaceState, false, false, false, false, device->getDevice());

    const auto expectedMocs = device->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);
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
    buffer->setArgStateful(&surfaceState, false, false, false, true, context->getDevice(0)->getDevice());

    const auto expectedMocs = device->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);
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
    buffer->getGraphicsAllocation(0)->setAllocationType(GraphicsAllocation::AllocationType::CONSTANT_SURFACE);

    typename FamilyType::RENDER_SURFACE_STATE surfaceState = {};
    buffer->setArgStateful(&surfaceState, false, false, false, false, context->getDevice(0)->getDevice());

    const auto expectedMocs = device->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);
    const auto actualMocs = surfaceState.getMemoryObjectControlState();
    EXPECT_EQ(expectedMocs, actualMocs);
}

GEN12LPTEST_F(BufferTestsTgllp, givenL1ForceEnabledWhenProgrammingSurfaceStateThenUseL3) {
    DebugManagerStateRestore restore{};
    DebugManager.flags.ForceL1Caching.set(1);

    auto buffer = std::unique_ptr<Buffer>(Buffer::create(
        context.get(),
        CL_MEM_READ_WRITE,
        MemoryConstants::pageSize,
        nullptr,
        retVal));
    ASSERT_EQ(CL_SUCCESS, retVal);

    typename FamilyType::RENDER_SURFACE_STATE surfaceState = {};
    buffer->setArgStateful(&surfaceState, false, false, false, false, device->getDevice());

    const auto expectedMocs = device->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);
    const auto actualMocs = surfaceState.getMemoryObjectControlState();
    EXPECT_EQ(expectedMocs, actualMocs);
}

GEN12LPTEST_F(BufferTestsTgllp, givenBufferReadonlyAndL1ForceEnabledWhenProgrammingSurfaceStateThenUseL1) {
    DebugManagerStateRestore restore{};
    DebugManager.flags.ForceL1Caching.set(1);

    auto buffer = std::unique_ptr<Buffer>(Buffer::create(
        context.get(),
        CL_MEM_READ_ONLY,
        MemoryConstants::pageSize,
        nullptr,
        retVal));
    ASSERT_EQ(CL_SUCCESS, retVal);

    typename FamilyType::RENDER_SURFACE_STATE surfaceState = {};
    buffer->setArgStateful(&surfaceState, false, false, false, false, device->getDevice());

    const auto expectedMocs = device->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST);
    const auto actualMocs = surfaceState.getMemoryObjectControlState();
    EXPECT_EQ(expectedMocs, actualMocs);
}

GEN12LPTEST_F(BufferTestsTgllp, givenBufferReadonlyL1ForceDisabledWhenProgrammingSurfaceStateThenUseL3) {
    DebugManagerStateRestore restore{};
    DebugManager.flags.ForceL1Caching.set(0);

    auto buffer = std::unique_ptr<Buffer>(Buffer::create(
        context.get(),
        CL_MEM_READ_WRITE,
        MemoryConstants::pageSize,
        nullptr,
        retVal));
    ASSERT_EQ(CL_SUCCESS, retVal);

    typename FamilyType::RENDER_SURFACE_STATE surfaceState = {};
    buffer->setArgStateful(&surfaceState, false, false, false, true, device->getDevice());

    const auto expectedMocs = device->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);
    const auto actualMocs = surfaceState.getMemoryObjectControlState();
    EXPECT_EQ(expectedMocs, actualMocs);
}
