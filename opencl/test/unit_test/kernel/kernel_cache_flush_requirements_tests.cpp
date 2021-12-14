/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/test.h"

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
        PlatformFixture::SetUp();
    }
    void clearPlatform() {
        PlatformFixture::TearDown();
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
    DebugManager.flags.EnableCacheFlushAfterWalker.set(1);
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
    DebugManager.flags.EnableCacheFlushAfterWalker.set(1);
    DebugManager.flags.EnableCacheFlushAfterWalkerForAllQueues.set(0);
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
    DebugManager.flags.EnableCacheFlushAfterWalker.set(1);
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
    DebugManager.flags.CreateMultipleSubDevices.set(1);
    initializePlatform();
    DebugManager.flags.EnableCacheFlushAfterWalker.set(1);
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
    DebugManager.flags.EnableCacheFlushAfterWalker.set(1);
    uint32_t numDevices = 2;
    DebugManager.flags.CreateMultipleSubDevices.set(numDevices);
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
    DebugManager.flags.EnableCacheFlushAfterWalker.set(1);
    uint32_t numDevices = 2;
    DebugManager.flags.CreateMultipleSubDevices.set(numDevices);
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
    DebugManager.flags.EnableCacheFlushAfterWalker.set(1);
    uint32_t numDevices = 2;
    DebugManager.flags.CreateMultipleSubDevices.set(numDevices);
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

HWTEST2_F(KernelWithCacheFlushTests, givenCacheFlushRequiredWhenEstimatingThenAddRequiredCommands, IsAtLeastXeHpCore) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.CreateMultipleSubDevices.set(2);

    initializePlatform();

    if (!pPlatform->getClDevice(0)->getHardwareInfo().capabilityTable.supportCacheFlushAfterWalker) {
        clearPlatform();
        GTEST_SKIP();
    }

    auto device = pPlatform->getClDevice(0);

    auto mockKernel = std::make_unique<MockKernelWithInternals>(*device);
    MockContext mockContext(device);
    mockContext.contextType = ContextType::CONTEXT_TYPE_SPECIALIZED;
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(&mockContext, device, nullptr);

    CsrDependencies csrDeps;
    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo(mockKernel->mockKernel);
    dispatchInfo.setKernel(mockKernel->mockKernel);
    dispatchInfo.setNumberOfWorkgroups({1, 1, 1});
    dispatchInfo.setTotalNumberOfWorkgroups({1, 1, 1});
    multiDispatchInfo.push(dispatchInfo);

    size_t initialSize = 0;
    size_t sizeWithCacheFlush = 0;
    size_t expectedDiff = sizeof(typename FamilyType::PIPE_CONTROL);
    if constexpr (FamilyType::isUsingL3Control) {
        expectedDiff += sizeof(typename FamilyType::L3_CONTROL) + sizeof(typename FamilyType::L3_FLUSH_ADDRESS_RANGE);
    }

    {
        EXPECT_FALSE(mockKernel->mockKernel->Kernel::requiresCacheFlushCommand(*cmdQ));

        initialSize = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, csrDeps, false, false, false, *cmdQ, multiDispatchInfo, false, false);
    }

    {
        DebugManager.flags.EnableCacheFlushAfterWalker.set(1);
        void *allocPtr = reinterpret_cast<void *>(static_cast<uintptr_t>(6 * MemoryConstants::pageSize));
        MockGraphicsAllocation globalAllocation{allocPtr, MemoryConstants::pageSize * 2};
        mockKernel->mockProgram->setGlobalSurface(&globalAllocation);

        cmdQ->requiresCacheFlushAfterWalker = true;
        auto &ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> &>(cmdQ->getGpgpuCommandStreamReceiver());
        ultCsr.multiOsContextCapable = false;
        EXPECT_TRUE(mockKernel->mockKernel->Kernel::requiresCacheFlushCommand(*cmdQ));

        sizeWithCacheFlush = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, csrDeps, false, false, false, *cmdQ, multiDispatchInfo, false, false);
    }

    EXPECT_EQ(initialSize + expectedDiff, sizeWithCacheFlush);

    mockKernel->mockProgram->setGlobalSurface(nullptr);
    clearPlatform();
}

HWTEST_F(KernelWithCacheFlushTests, givenCacheFlushWithAllocationsRequireCacheFlushFlagOnWhenCheckIfKernelRequireFlushThenReturnedTrue) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableCacheFlushAfterWalker.set(1);
    uint32_t numDevices = 2;
    DebugManager.flags.CreateMultipleSubDevices.set(numDevices);
    initializePlatform();
    auto device = pPlatform->getClDevice(0);

    auto mockKernel = std::make_unique<MockKernelWithInternals>(*device);
    MockContext mockContext(device);
    mockContext.contextType = ContextType::CONTEXT_TYPE_SPECIALIZED;
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(&mockContext, device, nullptr);
    cmdQ->requiresCacheFlushAfterWalker = true;
    auto &ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> &>(cmdQ->getGpgpuCommandStreamReceiver());
    ultCsr.multiOsContextCapable = false;
    mockKernel->mockKernel->svmAllocationsRequireCacheFlush = true;
    bool flushRequired = mockKernel->mockKernel->Kernel::requiresCacheFlushCommand(*cmdQ.get());

    EXPECT_TRUE(flushRequired);
    clearPlatform();
}
HWTEST_F(KernelWithCacheFlushTests, givenCacheFlushWithAllocationsWhichRequireCacheFlushWhenCheckIfKernelRequireFlushThenReturnedTrue) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableCacheFlushAfterWalker.set(1);
    uint32_t numDevices = 2;
    DebugManager.flags.CreateMultipleSubDevices.set(numDevices);
    initializePlatform();
    auto device = pPlatform->getClDevice(0);

    auto mockKernel = std::make_unique<MockKernelWithInternals>(*device);
    MockContext mockContext(device);
    mockContext.contextType = ContextType::CONTEXT_TYPE_SPECIALIZED;
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(&mockContext, device, nullptr);
    cmdQ->requiresCacheFlushAfterWalker = true;
    auto &ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> &>(cmdQ->getGpgpuCommandStreamReceiver());
    ultCsr.multiOsContextCapable = false;
    mockKernel->mockKernel->svmAllocationsRequireCacheFlush = false;
    mockKernel->mockKernel->kernelArgRequiresCacheFlush.resize(2);
    MockGraphicsAllocation cacheRequiringAllocation;
    mockKernel->mockKernel->kernelArgRequiresCacheFlush[1] = &cacheRequiringAllocation;
    bool flushRequired = mockKernel->mockKernel->Kernel::requiresCacheFlushCommand(*cmdQ.get());

    EXPECT_TRUE(flushRequired);
    clearPlatform();
}

HWTEST_F(KernelWithCacheFlushTests,
         givenEnableCacheFlushAfterWalkerForAllQueuesFlagSetWhenCheckIfKernelRequierFlushThenTrueIsAlwaysReturned) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableCacheFlushAfterWalker.set(1);
    DebugManager.flags.EnableCacheFlushAfterWalkerForAllQueues.set(1);
    MockGraphicsAllocation cacheRequiringAllocation;

    for (auto isMultiEngine : ::testing::Bool()) {
        for (auto isMultiDevice : ::testing::Bool()) {
            for (auto isDefaultContext : ::testing::Bool()) {
                for (auto svmAllocationRequiresCacheFlush : ::testing::Bool()) {
                    for (auto kernelArgRequiresCacheFlush : ::testing::Bool()) {
                        auto deviceCount = (isMultiDevice ? 2 : 0);
                        auto contextType =
                            (isDefaultContext ? ContextType::CONTEXT_TYPE_DEFAULT : ContextType::CONTEXT_TYPE_SPECIALIZED);
                        GraphicsAllocation *kernelArg = (kernelArgRequiresCacheFlush ? &cacheRequiringAllocation : nullptr);

                        DebugManager.flags.CreateMultipleSubDevices.set(deviceCount);
                        initializePlatform();

                        auto device = pPlatform->getClDevice(0);
                        MockContext mockContext(device);
                        mockContext.contextType = contextType;
                        auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(&mockContext, device, nullptr);
                        cmdQ->requiresCacheFlushAfterWalker = true;
                        auto &ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> &>(cmdQ->getGpgpuCommandStreamReceiver());
                        ultCsr.multiOsContextCapable = isMultiEngine;

                        auto mockKernel = std::make_unique<MockKernelWithInternals>(*device);
                        mockKernel->mockKernel->svmAllocationsRequireCacheFlush = svmAllocationRequiresCacheFlush;
                        mockKernel->mockKernel->kernelArgRequiresCacheFlush.resize(1);
                        mockKernel->mockKernel->kernelArgRequiresCacheFlush[0] = kernelArg;

                        auto flushRequired = mockKernel->mockKernel->Kernel::requiresCacheFlushCommand(*cmdQ.get());
                        EXPECT_TRUE(flushRequired);
                        clearPlatform();
                    }
                }
            }
        }
    }
}
} // namespace NEO
