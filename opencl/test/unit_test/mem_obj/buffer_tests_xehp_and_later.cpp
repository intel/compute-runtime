/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using namespace NEO;

using XeHPAndLaterBufferTests = ::testing::Test;

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterBufferTests, givenDebugFlagSetWhenProgramingSurfaceStateThenForceCompressionFormat) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    DebugManagerStateRestore restorer;
    uint32_t compressionFormat = 3;

    MockContext context;

    auto gmmContext = context.getDevice(0)->getGmmHelper()->getClientContext();
    uint32_t defaultCompressionFormat = gmmContext->getSurfaceStateCompressionFormat(GMM_RESOURCE_FORMAT::GMM_FORMAT_GENERIC_8BIT);

    auto retVal = CL_SUCCESS;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = new Gmm(context.getDevice(0)->getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
    gmm->setCompressionEnabled(true);

    auto buffer = std::unique_ptr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE, 1, nullptr, retVal));
    buffer->getGraphicsAllocation(0)->setGmm(gmm, 0);

    RENDER_SURFACE_STATE surfaceState = FamilyType::cmdInitRenderSurfaceState;

    {
        buffer->setArgStateful(&surfaceState, false, false, false, false, context.getDevice(0)->getDevice(), false);
        EXPECT_EQ(defaultCompressionFormat, surfaceState.getCompressionFormat());
    }

    {
        debugManager.flags.ForceBufferCompressionFormat.set(compressionFormat);
        buffer->setArgStateful(&surfaceState, false, false, false, false, context.getDevice(0)->getDevice(), false);
        EXPECT_EQ(compressionFormat, surfaceState.getCompressionFormat());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterBufferTests, givenDebugVariableForcingL1CachingWhenBufferSurfaceStateIsSetThenItIsCachedInL1) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceL1Caching.set(1u);
    MockContext context;
    const auto size = MemoryConstants::pageSize;
    const auto flags = CL_MEM_READ_WRITE;

    auto retVal = CL_SUCCESS;
    auto buffer = std::unique_ptr<Buffer>(Buffer::create(
        &context,
        flags,
        size,
        nullptr,
        retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    typename FamilyType::RENDER_SURFACE_STATE surfaceState = {};
    buffer->setArgStateful(&surfaceState, false, false, false, false, context.getDevice(0)->getDevice(), false);

    const auto expectedMocs = context.getDevice(0)->getGmmHelper()->getL1EnabledMOCS();
    const auto actualMocs = surfaceState.getMemoryObjectControlState();
    EXPECT_EQ(expectedMocs, actualMocs);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterBufferTests, givenDebugVariableForcingL1CachingDisabledWhenBufferSurfaceStateIsSetThenItIsCachedInL3) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceL1Caching.set(0u);
    MockContext context;
    const auto size = MemoryConstants::pageSize;
    const auto flags = CL_MEM_READ_WRITE;

    auto retVal = CL_SUCCESS;
    auto buffer = std::unique_ptr<Buffer>(Buffer::create(
        &context,
        flags,
        size,
        nullptr,
        retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    typename FamilyType::RENDER_SURFACE_STATE surfaceState = {};
    buffer->setArgStateful(&surfaceState, false, false, false, false, context.getDevice(0)->getDevice(), false);

    const auto expectedMocs = context.getDevice(0)->getGmmHelper()->getL3EnabledMOCS();
    const auto actualMocs = surfaceState.getMemoryObjectControlState();
    EXPECT_EQ(expectedMocs, actualMocs);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterBufferTests, givenBufferWhenArgumentIsConstAndAuxModeIsOnThenL3DisabledPolicyIsChoosen) {
    MockContext context;
    const auto size = MemoryConstants::pageSize;
    const auto flags = CL_MEM_READ_ONLY;

    auto retVal = CL_SUCCESS;
    auto buffer = std::unique_ptr<Buffer>(Buffer::create(
        &context,
        flags,
        size,
        nullptr,
        retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    typename FamilyType::RENDER_SURFACE_STATE surfaceState = {};
    buffer->setArgStateful(&surfaceState, true, true, false, false, context.getDevice(0)->getDevice(), false);

    const auto expectedMocs = context.getDevice(0)->getGmmHelper()->getUncachedMOCS();
    const auto actualMocs = surfaceState.getMemoryObjectControlState();
    EXPECT_EQ(expectedMocs, actualMocs);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterBufferTests, givenBufferSetSurfaceThatMemoryPtrAndSizeIsAlignedToCachelineThenL1CacheShouldBeOn) {
    MockContext context;

    auto size = MemoryConstants::pageSize;
    auto ptr = (void *)alignedMalloc(size * 2, MemoryConstants::pageSize);

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE surfaceState = {};

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    Buffer::setSurfaceState(device.get(), &surfaceState, false, false, size, ptr, 0, nullptr, 0, 0, false);

    auto mocs = surfaceState.getMemoryObjectControlState();
    auto gmmHelper = device->getGmmHelper();
    EXPECT_EQ(gmmHelper->getL1EnabledMOCS(), mocs);

    alignedFree(ptr);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterBufferTests, givenAlignedCacheableNonReadOnlyBufferThenChooseOclBufferPolicy) {
    MockContext context;
    const auto size = MemoryConstants::pageSize;
    const auto ptr = (void *)alignedMalloc(size * 2, MemoryConstants::pageSize);
    const auto flags = CL_MEM_USE_HOST_PTR;

    auto retVal = CL_SUCCESS;
    auto buffer = std::unique_ptr<Buffer>(Buffer::create(
        &context,
        flags,
        size,
        ptr,
        retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    typename FamilyType::RENDER_SURFACE_STATE surfaceState = {};
    buffer->setArgStateful(&surfaceState, false, false, false, false, context.getDevice(0)->getDevice(), false);

    const auto expectedMocs = context.getDevice(0)->getDevice().getGmmHelper()->getL1EnabledMOCS();
    const auto actualMocs = surfaceState.getMemoryObjectControlState();
    EXPECT_EQ(expectedMocs, actualMocs);

    alignedFree(ptr);
}
