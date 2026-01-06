/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/mem_obj/buffer.h"
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
    buffer->setArgStateful(&surfaceState, false, false, false, false, device->getDevice(), false, 1u);

    const auto expectedMocs = device->getGmmHelper()->getL3EnabledMOCS();
    const auto actualMocs = surfaceState.getMemoryObjectControlState();
    EXPECT_EQ(expectedMocs, actualMocs);
}

GEN12LPTEST_F(BufferTestsTgllp, givenBufferReadonlyWhenProgrammingSurfaceStateThenUseL1) {
    auto buffer = std::unique_ptr<Buffer>(Buffer::create(
        context.get(),
        CL_MEM_READ_WRITE,
        MemoryConstants::pageSize,
        nullptr,
        retVal));
    ASSERT_EQ(CL_SUCCESS, retVal);

    typename FamilyType::RENDER_SURFACE_STATE surfaceState = {};
    buffer->setArgStateful(&surfaceState, false, false, false, true, context->getDevice(0)->getDevice(), false, 1u);

    const auto expectedMocs = device->getGmmHelper()->getL1EnabledMOCS();
    const auto actualMocs = surfaceState.getMemoryObjectControlState();
    EXPECT_EQ(expectedMocs, actualMocs);
}

GEN12LPTEST_F(BufferTestsTgllp, givenConstantSurfaceWhenProgrammingSurfaceStateThenUseL1) {
    auto buffer = std::unique_ptr<Buffer>(Buffer::create(
        context.get(),
        CL_MEM_READ_WRITE,
        MemoryConstants::pageSize,
        nullptr,
        retVal));
    ASSERT_EQ(CL_SUCCESS, retVal);
    buffer->getGraphicsAllocation(0)->setAllocationType(AllocationType::constantSurface);

    typename FamilyType::RENDER_SURFACE_STATE surfaceState = {};
    buffer->setArgStateful(&surfaceState, false, false, false, false, context->getDevice(0)->getDevice(), false, 1u);

    const auto expectedMocs = device->getGmmHelper()->getL1EnabledMOCS();
    const auto actualMocs = surfaceState.getMemoryObjectControlState();
    EXPECT_EQ(expectedMocs, actualMocs);
}

GEN12LPTEST_F(BufferTestsTgllp, givenL1ForceEnabledWhenProgrammingSurfaceStateThenUseL1) {
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
    buffer->setArgStateful(&surfaceState, false, false, false, false, device->getDevice(), false, 1u);

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
    buffer->setArgStateful(&surfaceState, false, false, false, true, device->getDevice(), false, 1u);

    const auto expectedMocs = device->getGmmHelper()->getL3EnabledMOCS();
    const auto actualMocs = surfaceState.getMemoryObjectControlState();
    EXPECT_EQ(expectedMocs, actualMocs);
}
