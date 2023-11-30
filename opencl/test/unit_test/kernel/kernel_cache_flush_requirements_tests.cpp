/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

namespace NEO {

class KernelWithCacheFlushTests : public PlatformFixture, public testing::TestWithParam<std::tuple<const char *, const char *>> {
  public:
    void SetUp() override {
    }
    void TearDown() override {
    }
    void initializePlatform() {
        PlatformFixture::setUp();
    }
    void clearPlatform() {
        PlatformFixture::tearDown();
    }
};
TEST_F(KernelWithCacheFlushTests, givenDeviceWhichDoesntRequireCacheFlushWhenCheckIfKernelRequireFlushThenReturnedFalse) {
    initializePlatform();
    auto device = pPlatform->getClDevice(0);

    auto mockKernel = std::make_unique<MockKernelWithInternals>(*device);
    MockContext mockContext(device);
    MockCommandQueue queue(mockContext);
    bool flushRequired = mockKernel->mockKernel->Kernel::requiresCacheFlushCommand(queue);
    EXPECT_FALSE(flushRequired);
    clearPlatform();
}
TEST_F(KernelWithCacheFlushTests, givenQueueWhichDoesntRequireCacheFlushWhenCheckIfKernelRequireFlushThenReturnedFalse) {
    initializePlatform();
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableCacheFlushAfterWalker.set(1);
    auto device = pPlatform->getClDevice(0);

    auto mockKernel = std::make_unique<MockKernelWithInternals>(*device);
    MockContext mockContext(device);
    MockCommandQueue queue(mockContext);

    bool flushRequired = mockKernel->mockKernel->Kernel::requiresCacheFlushCommand(queue);
    EXPECT_FALSE(flushRequired);
    clearPlatform();
}
TEST_F(KernelWithCacheFlushTests, givenCacheFlushForAllQueuesDisabledWhenCheckIfKernelRequireFlushThenReturnedFalse) {
    initializePlatform();
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableCacheFlushAfterWalker.set(1);
    debugManager.flags.EnableCacheFlushAfterWalkerForAllQueues.set(0);
    auto device = pPlatform->getClDevice(0);

    auto mockKernel = std::make_unique<MockKernelWithInternals>(*device);
    MockContext mockContext(device);
    MockCommandQueue queue(mockContext);
    bool flushRequired = mockKernel->mockKernel->Kernel::requiresCacheFlushCommand(queue);

    EXPECT_FALSE(flushRequired);
    clearPlatform();
}
HWTEST_F(KernelWithCacheFlushTests, givenCacheFlushForMultiEngineEnabledWhenCheckIfKernelRequireFlushThenReturnedFalse) {
    initializePlatform();
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableCacheFlushAfterWalker.set(1);
    auto device = pPlatform->getClDevice(0);

    auto mockKernel = std::make_unique<MockKernelWithInternals>(*device);
    MockContext mockContext(device);
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(&mockContext, device, nullptr);
    cmdQ->requiresCacheFlushAfterWalker = true;
    auto &ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> &>(cmdQ->getGpgpuCommandStreamReceiver());
    ultCsr.multiOsContextCapable = true;
    bool flushRequired = mockKernel->mockKernel->Kernel::requiresCacheFlushCommand(*cmdQ.get());

    EXPECT_FALSE(flushRequired);
    clearPlatform();
}

HWTEST_F(KernelWithCacheFlushTests, givenCacheFlushForSingleDeviceProgramWhenCheckIfKernelRequireFlushThenReturnedFalse) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.CreateMultipleSubDevices.set(1);
    initializePlatform();
    debugManager.flags.EnableCacheFlushAfterWalker.set(1);
    auto device = pPlatform->getClDevice(0);

    auto mockKernel = std::make_unique<MockKernelWithInternals>(*device);
    MockContext mockContext(device);
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(&mockContext, device, nullptr);
    auto &ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> &>(cmdQ->getGpgpuCommandStreamReceiver());
    ultCsr.multiOsContextCapable = false;
    cmdQ->requiresCacheFlushAfterWalker = true;
    bool flushRequired = mockKernel->mockKernel->Kernel::requiresCacheFlushCommand(*cmdQ.get());

    EXPECT_FALSE(flushRequired);
    clearPlatform();
}

HWTEST_F(KernelWithCacheFlushTests, givenCacheFlushForDefaultTypeContextWhenCheckIfKernelRequireFlushThenReturnedFalse) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableCacheFlushAfterWalker.set(1);
    uint32_t numDevices = 2;
    debugManager.flags.CreateMultipleSubDevices.set(numDevices);
    initializePlatform();
    auto device = pPlatform->getClDevice(0);

    auto mockKernel = std::make_unique<MockKernelWithInternals>(*device);
    MockContext mockContext(device);
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(&mockContext, device, nullptr);
    cmdQ->requiresCacheFlushAfterWalker = true;
    auto &ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> &>(cmdQ->getGpgpuCommandStreamReceiver());
    ultCsr.multiOsContextCapable = false;
    bool flushRequired = mockKernel->mockKernel->Kernel::requiresCacheFlushCommand(*cmdQ.get());

    EXPECT_FALSE(flushRequired);
    clearPlatform();
}
HWTEST_F(KernelWithCacheFlushTests, givenCacheFlushWithNullGlobalSurfaceWhenCheckIfKernelRequireFlushThenReturnedFalse) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableCacheFlushAfterWalker.set(1);
    uint32_t numDevices = 2;
    debugManager.flags.CreateMultipleSubDevices.set(numDevices);
    initializePlatform();
    auto device = pPlatform->getClDevice(0);

    auto mockKernel = std::make_unique<MockKernelWithInternals>(*device);
    MockContext mockContext(device);
    mockContext.contextType = ContextType::CONTEXT_TYPE_SPECIALIZED;
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(&mockContext, device, nullptr);
    cmdQ->requiresCacheFlushAfterWalker = true;
    auto &ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> &>(cmdQ->getGpgpuCommandStreamReceiver());
    ultCsr.multiOsContextCapable = false;
    bool flushRequired = mockKernel->mockKernel->Kernel::requiresCacheFlushCommand(*cmdQ.get());

    EXPECT_FALSE(flushRequired);
    clearPlatform();
}
HWTEST_F(KernelWithCacheFlushTests, givenCacheFlushWithGlobalSurfaceWhenCheckIfKernelRequireFlushThenReturnedTrue) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableCacheFlushAfterWalker.set(1);
    uint32_t numDevices = 2;
    debugManager.flags.CreateMultipleSubDevices.set(numDevices);
    initializePlatform();
    auto device = pPlatform->getClDevice(0);

    auto mockKernel = std::make_unique<MockKernelWithInternals>(*device);
    MockContext mockContext(device);
    mockContext.contextType = ContextType::CONTEXT_TYPE_SPECIALIZED;

    void *allocPtr = reinterpret_cast<void *>(static_cast<uintptr_t>(6 * MemoryConstants::pageSize));
    MockGraphicsAllocation globalAllocation{allocPtr, MemoryConstants::pageSize * 2};
    mockKernel->mockProgram->setGlobalSurface(&globalAllocation);

    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(&mockContext, device, nullptr);
    cmdQ->requiresCacheFlushAfterWalker = true;
    auto &ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> &>(cmdQ->getGpgpuCommandStreamReceiver());
    ultCsr.multiOsContextCapable = false;
    bool flushRequired = mockKernel->mockKernel->Kernel::requiresCacheFlushCommand(*cmdQ.get());

    EXPECT_TRUE(flushRequired);
    mockKernel->mockProgram->setGlobalSurface(nullptr);
    clearPlatform();
}
} // namespace NEO
