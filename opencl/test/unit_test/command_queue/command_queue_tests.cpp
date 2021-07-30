/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/test_checks_shared.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/event/event.h"
#include "opencl/source/event/user_event.h"
#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/command_stream/command_stream_fixture.h"
#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/fixtures/dispatch_flags_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/fixtures/memory_management_fixture.h"
#include "opencl/test/unit_test/helpers/raii_hw_helper.h"
#include "opencl/test/unit_test/libult/ult_command_stream_receiver.h"
#include "opencl/test/unit_test/mocks/mock_allocation_properties.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_csr.h"
#include "opencl/test/unit_test/mocks/mock_event.h"
#include "opencl/test/unit_test/mocks/mock_image.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_mdi.h"
#include "opencl/test/unit_test/mocks/mock_memory_manager.h"
#include "opencl/test/unit_test/mocks/mock_os_context.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "test.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace NEO;

struct CommandQueueMemoryDevice
    : public MemoryManagementFixture,
      public ClDeviceFixture {

    void SetUp() override {
        MemoryManagementFixture::SetUp();
        ClDeviceFixture::SetUp();
    }

    void TearDown() override {
        ClDeviceFixture::TearDown();
        platformsImpl->clear();
        MemoryManagementFixture::TearDown();
    }
};

struct CommandQueueTest
    : public CommandQueueMemoryDevice,
      public ContextFixture,
      public CommandQueueFixture,
      ::testing::TestWithParam<uint64_t /*cl_command_queue_properties*/> {

    using CommandQueueFixture::SetUp;
    using ContextFixture::SetUp;

    CommandQueueTest() {
    }

    void SetUp() override {
        CommandQueueMemoryDevice::SetUp();
        properties = GetParam();

        cl_device_id device = pClDevice;
        ContextFixture::SetUp(1, &device);
        CommandQueueFixture::SetUp(pContext, pClDevice, properties);
    }

    void TearDown() override {
        CommandQueueFixture::TearDown();
        ContextFixture::TearDown();
        CommandQueueMemoryDevice::TearDown();
    }

    cl_command_queue_properties properties;
    const HardwareInfo *pHwInfo = nullptr;
};

TEST_P(CommandQueueTest, GivenNonFailingAllocationWhenCreatingCommandQueueThenCommandQueueIsCreated) {
    InjectedFunction method = [this](size_t failureIndex) {
        auto retVal = CL_INVALID_VALUE;
        auto pCmdQ = CommandQueue::create(
            pContext,
            pClDevice,
            nullptr,
            false,
            retVal);

        if (MemoryManagement::nonfailingAllocation == failureIndex) {
            EXPECT_EQ(CL_SUCCESS, retVal);
            EXPECT_NE(nullptr, pCmdQ);
        } else {
            EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal) << "for allocation " << failureIndex;
            EXPECT_EQ(nullptr, pCmdQ);
        }
        delete pCmdQ;
    };
    injectFailures(method);
}

INSTANTIATE_TEST_CASE_P(CommandQueue,
                        CommandQueueTest,
                        ::testing::ValuesIn(AllCommandQueueProperties));

TEST(CommandQueue, WhenConstructingCommandQueueThenTaskLevelAndTaskCountAreZero) {
    MockCommandQueue cmdQ(nullptr, nullptr, 0, false);
    EXPECT_EQ(0u, cmdQ.taskLevel);
    EXPECT_EQ(0u, cmdQ.taskCount);
}

TEST(CommandQueue, WhenConstructingCommandQueueThenQueueFamilyIsNotSelected) {
    MockCommandQueue cmdQ(nullptr, nullptr, 0, false);
    EXPECT_FALSE(cmdQ.isQueueFamilySelected());
}

struct GetTagTest : public ClDeviceFixture,
                    public CommandQueueFixture,
                    public CommandStreamFixture,
                    public ::testing::Test {

    using CommandQueueFixture::SetUp;

    void SetUp() override {
        ClDeviceFixture::SetUp();
        CommandQueueFixture::SetUp(nullptr, pClDevice, 0);
        CommandStreamFixture::SetUp(pCmdQ);
    }

    void TearDown() override {
        CommandStreamFixture::TearDown();
        CommandQueueFixture::TearDown();
        ClDeviceFixture::TearDown();
    }
};

TEST_F(GetTagTest, GivenSetHwTagWhenGettingHwTagThenCorrectTagIsReturned) {
    uint32_t tagValue = 0xdeadbeef;
    *pTagMemory = tagValue;
    EXPECT_EQ(tagValue, pCmdQ->getHwTag());
}

TEST_F(GetTagTest, GivenInitialValueWhenGettingHwTagThenCorrectTagIsReturned) {
    MockContext context;
    MockCommandQueue commandQueue(&context, pClDevice, 0, false);

    EXPECT_EQ(initialHardwareTag, commandQueue.getHwTag());
}

TEST(CommandQueue, GivenUpdatedCompletionStampWhenGettingCompletionStampThenUpdatedValueIsReturned) {
    MockContext context;

    MockCommandQueue cmdQ(&context, nullptr, 0, false);

    CompletionStamp cs = {
        cmdQ.taskCount + 100,
        cmdQ.taskLevel + 50,
        5};
    cmdQ.updateFromCompletionStamp(cs, nullptr);

    EXPECT_EQ(cs.taskLevel, cmdQ.taskLevel);
    EXPECT_EQ(cs.taskCount, cmdQ.taskCount);
    EXPECT_EQ(cs.flushStamp, cmdQ.flushStamp->peekStamp());
}

TEST(CommandQueue, givenTimeStampWithTaskCountNotReadyStatusWhenupdateFromCompletionStampIsBeingCalledThenQueueTaskCountIsNotUpdated) {
    MockContext context;

    MockCommandQueue cmdQ(&context, nullptr, 0, false);

    cmdQ.taskCount = 1u;

    CompletionStamp cs = {
        CompletionStamp::notReady,
        0,
        0};
    cmdQ.updateFromCompletionStamp(cs, nullptr);
    EXPECT_EQ(1u, cmdQ.taskCount);
}

TEST(CommandQueue, GivenOOQwhenUpdateFromCompletionStampWithTrueIsCalledThenTaskLevelIsUpdated) {
    MockContext context;
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};

    MockCommandQueue cmdQ(&context, nullptr, props, false);
    auto oldTL = cmdQ.taskLevel;

    CompletionStamp cs = {
        cmdQ.taskCount + 100,
        cmdQ.taskLevel + 50,
        5};
    cmdQ.updateFromCompletionStamp(cs, nullptr);

    EXPECT_NE(oldTL, cmdQ.taskLevel);
    EXPECT_EQ(oldTL + 50, cmdQ.taskLevel);
    EXPECT_EQ(cs.taskCount, cmdQ.taskCount);
    EXPECT_EQ(cs.flushStamp, cmdQ.flushStamp->peekStamp());
}

TEST(CommandQueue, givenDeviceWhenCreatingCommandQueueThenPickCsrFromDefaultEngine) {
    auto mockDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    MockCommandQueue cmdQ(nullptr, mockDevice.get(), 0, false);

    auto defaultCsr = mockDevice->getDefaultEngine().commandStreamReceiver;
    EXPECT_EQ(defaultCsr, &cmdQ.getGpgpuCommandStreamReceiver());
}

struct CommandQueueWithBlitOperationsTests : public ::testing::TestWithParam<uint32_t> {};

TEST_P(CommandQueueWithBlitOperationsTests, givenDeviceNotSupportingBlitOperationsWhenQueueIsCreatedThenDontRegisterBcsCsr) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = false;
    auto mockDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    MockCommandQueue cmdQ(nullptr, mockDevice.get(), 0, false);
    auto cmdType = GetParam();

    EXPECT_EQ(nullptr, cmdQ.getBcsCommandStreamReceiver());

    auto defaultCsr = mockDevice->getDefaultEngine().commandStreamReceiver;
    EXPECT_EQ(defaultCsr, &cmdQ.getGpgpuCommandStreamReceiver());

    auto blitAllowed = cmdQ.blitEnqueueAllowed(cmdType);
    EXPECT_EQ(defaultCsr, &cmdQ.getCommandStreamReceiver(blitAllowed));
}

HWTEST_P(CommandQueueWithBlitOperationsTests, givenDeviceWithSubDevicesSupportingBlitOperationsWhenQueueIsCreatedThenBcsIsTakenFromFirstSubDevice) {
    DebugManagerStateRestore restorer;
    VariableBackup<bool> mockDeviceFlagBackup{&MockDevice::createSingleDevice, false};
    DebugManager.flags.CreateMultipleSubDevices.set(2);
    DebugManager.flags.EnableBlitterForEnqueueOperations.set(1);
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    REQUIRE_BLITTER_OR_SKIP(&hwInfo);

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    EXPECT_EQ(2u, device->getNumAvailableDevices());
    std::unique_ptr<OsContext> bcsOsContext;

    auto subDevice = device->getDeviceById(0);
    auto &bcsEngine = subDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::Regular);

    MockCommandQueue cmdQ(nullptr, device.get(), 0, false);
    auto cmdType = GetParam();
    auto blitAllowed = cmdQ.blitEnqueueAllowed(cmdType);

    EXPECT_NE(nullptr, cmdQ.getBcsCommandStreamReceiver());
    EXPECT_EQ(bcsEngine.commandStreamReceiver, cmdQ.getBcsCommandStreamReceiver());
    EXPECT_EQ(bcsEngine.commandStreamReceiver, &cmdQ.getCommandStreamReceiver(blitAllowed));
    EXPECT_EQ(bcsEngine.osContext, &cmdQ.getCommandStreamReceiver(blitAllowed).getOsContext());
}

INSTANTIATE_TEST_CASE_P(uint32_t,
                        CommandQueueWithBlitOperationsTests,
                        ::testing::Values(CL_COMMAND_WRITE_BUFFER,
                                          CL_COMMAND_WRITE_BUFFER_RECT,
                                          CL_COMMAND_READ_BUFFER,
                                          CL_COMMAND_READ_BUFFER_RECT,
                                          CL_COMMAND_COPY_BUFFER,
                                          CL_COMMAND_COPY_BUFFER_RECT,
                                          CL_COMMAND_SVM_MEMCPY));

TEST(CommandQueue, givenCmdQueueBlockedByReadyVirtualEventWhenUnblockingThenUpdateFlushTaskFromEvent) {
    auto mockDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto context = new MockContext;
    auto cmdQ = new MockCommandQueue(context, mockDevice.get(), 0, false);
    auto userEvent = new Event(cmdQ, CL_COMMAND_NDRANGE_KERNEL, 0, 0);
    userEvent->setStatus(CL_COMPLETE);
    userEvent->flushStamp->setStamp(5);
    userEvent->incRefInternal();

    FlushStamp expectedFlushStamp = 0;
    EXPECT_EQ(expectedFlushStamp, cmdQ->flushStamp->peekStamp());
    cmdQ->virtualEvent = userEvent;

    EXPECT_FALSE(cmdQ->isQueueBlocked());
    EXPECT_EQ(userEvent->flushStamp->peekStamp(), cmdQ->flushStamp->peekStamp());
    userEvent->decRefInternal();
    cmdQ->decRefInternal();
    context->decRefInternal();
}

TEST(CommandQueue, givenCmdQueueBlockedByAbortedVirtualEventWhenUnblockingThenUpdateFlushTaskFromEvent) {
    auto context = new MockContext;
    auto mockDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto cmdQ = new MockCommandQueue(context, mockDevice.get(), 0, false);

    auto userEvent = new Event(cmdQ, CL_COMMAND_NDRANGE_KERNEL, 0, 0);
    userEvent->setStatus(-1);
    userEvent->flushStamp->setStamp(5);

    FlushStamp expectedFlushStamp = 0;

    EXPECT_EQ(expectedFlushStamp, cmdQ->flushStamp->peekStamp());
    userEvent->incRefInternal();
    cmdQ->virtualEvent = userEvent;

    EXPECT_FALSE(cmdQ->isQueueBlocked());
    EXPECT_EQ(expectedFlushStamp, cmdQ->flushStamp->peekStamp());
    userEvent->decRefInternal();
    cmdQ->decRefInternal();
    context->decRefInternal();
}

struct CommandQueueCommandStreamTest : public CommandQueueMemoryDevice,
                                       public ::testing::Test {
    void SetUp() override {
        CommandQueueMemoryDevice::SetUp();
        context.reset(new MockContext(pClDevice));
    }

    void TearDown() override {
        context.reset();
        CommandQueueMemoryDevice::TearDown();
    }
    std::unique_ptr<MockContext> context;
};

HWTEST_F(CommandQueueCommandStreamTest, givenCommandQueueThatWaitsOnAbortedUserEventWhenIsQueueBlockedIsCalledThenTaskLevelAlignsToCsr) {
    MockContext context;
    auto mockDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    MockCommandQueue cmdQ(&context, mockDevice.get(), 0, false);
    auto &commandStreamReceiver = mockDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.taskLevel = 100u;

    Event userEvent(&cmdQ, CL_COMMAND_NDRANGE_KERNEL, 0, 0);
    userEvent.setStatus(-1);
    userEvent.incRefInternal();
    cmdQ.virtualEvent = &userEvent;

    EXPECT_FALSE(cmdQ.isQueueBlocked());
    EXPECT_EQ(100u, cmdQ.taskLevel);
}

TEST_F(CommandQueueCommandStreamTest, GivenValidCommandQueueWhenGettingCommandStreamThenValidObjectIsReturned) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue commandQueue(context.get(), pClDevice, props, false);

    auto &cs = commandQueue.getCS(1024);
    EXPECT_NE(nullptr, &cs);
}

TEST_F(CommandQueueCommandStreamTest, GivenValidCommandStreamWhenGettingGraphicsAllocationThenMaxAvailableSpaceAndUnderlyingBufferSizeAreCorrect) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue commandQueue(context.get(), pClDevice, props, false);
    size_t minSizeRequested = 20;

    auto &cs = commandQueue.getCS(minSizeRequested);
    ASSERT_NE(nullptr, &cs);

    auto *allocation = cs.getGraphicsAllocation();
    ASSERT_NE(nullptr, &allocation);
    size_t expectedCsSize = alignUp(minSizeRequested + CSRequirements::minCommandQueueCommandStreamSize + CSRequirements::csOverfetchSize, MemoryConstants::pageSize64k) - CSRequirements::minCommandQueueCommandStreamSize - CSRequirements::csOverfetchSize;
    EXPECT_EQ(expectedCsSize, cs.getMaxAvailableSpace());

    size_t expectedTotalSize = alignUp(minSizeRequested + CSRequirements::minCommandQueueCommandStreamSize + CSRequirements::csOverfetchSize, MemoryConstants::pageSize64k);
    EXPECT_EQ(expectedTotalSize, allocation->getUnderlyingBufferSize());
}

TEST_F(CommandQueueCommandStreamTest, GivenRequiredSizeWhenGettingCommandStreamThenMaxAvailableSpaceIsEqualOrGreaterThanRequiredSize) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue commandQueue(context.get(), pClDevice, props, false);

    size_t requiredSize = 16384;
    const auto &commandStream = commandQueue.getCS(requiredSize);
    ASSERT_NE(nullptr, &commandStream);
    EXPECT_GE(commandStream.getMaxAvailableSpace(), requiredSize);
}

TEST_F(CommandQueueCommandStreamTest, WhenGettingCommandStreamWithNewSizeThenMaxAvailableSpaceIsEqualOrGreaterThanNewSize) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue commandQueue(context.get(), pClDevice, props, false);

    auto &commandStreamInitial = commandQueue.getCS(1024);
    size_t requiredSize = commandStreamInitial.getMaxAvailableSpace() + 42;

    const auto &commandStream = commandQueue.getCS(requiredSize);
    ASSERT_NE(nullptr, &commandStream);
    EXPECT_GE(commandStream.getMaxAvailableSpace(), requiredSize);
}

TEST_F(CommandQueueCommandStreamTest, givenCommandStreamReceiverWithReusableAllocationsWhenAskedForCommandStreamThenReturnsAllocationFromReusablePool) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(context.get(), pClDevice, props, false);

    auto memoryManager = pDevice->getMemoryManager();
    size_t requiredSize = alignUp(100 + CSRequirements::minCommandQueueCommandStreamSize + CSRequirements::csOverfetchSize, MemoryConstants::pageSize64k);
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), requiredSize, GraphicsAllocation::AllocationType::COMMAND_BUFFER, pDevice->getDeviceBitfield()});
    auto &commandStreamReceiver = cmdQ.getGpgpuCommandStreamReceiver();
    commandStreamReceiver.getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION);

    EXPECT_FALSE(commandStreamReceiver.getAllocationsForReuse().peekIsEmpty());
    EXPECT_TRUE(commandStreamReceiver.getAllocationsForReuse().peekContains(*allocation));

    const auto &indirectHeap = cmdQ.getCS(100);

    EXPECT_EQ(indirectHeap.getGraphicsAllocation(), allocation);

    EXPECT_TRUE(commandStreamReceiver.getAllocationsForReuse().peekIsEmpty());
}

TEST_F(CommandQueueCommandStreamTest, givenCommandQueueWhenItIsDestroyedThenCommandStreamIsPutOnTheReusabeList) {
    auto cmdQ = new MockCommandQueue(context.get(), pClDevice, 0, false);
    const auto &commandStream = cmdQ->getCS(100);
    auto graphicsAllocation = commandStream.getGraphicsAllocation();
    EXPECT_TRUE(pDevice->getDefaultEngine().commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());

    //now destroy command queue, heap should go to reusable list
    delete cmdQ;
    EXPECT_FALSE(pDevice->getDefaultEngine().commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
    EXPECT_TRUE(pDevice->getDefaultEngine().commandStreamReceiver->getAllocationsForReuse().peekContains(*graphicsAllocation));
}

TEST_F(CommandQueueCommandStreamTest, WhenAskedForNewCommandStreamThenOldHeapIsStoredForReuse) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(context.get(), pClDevice, props, false);

    EXPECT_TRUE(pDevice->getDefaultEngine().commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());

    const auto &indirectHeap = cmdQ.getCS(100);

    EXPECT_TRUE(pDevice->getDefaultEngine().commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());

    auto graphicsAllocation = indirectHeap.getGraphicsAllocation();

    cmdQ.getCS(indirectHeap.getAvailableSpace() + 100);

    EXPECT_FALSE(pDevice->getDefaultEngine().commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());

    EXPECT_TRUE(pDevice->getDefaultEngine().commandStreamReceiver->getAllocationsForReuse().peekContains(*graphicsAllocation));
}

TEST_F(CommandQueueCommandStreamTest, givenCommandQueueWhenGetCSIsCalledThenCommandStreamAllocationTypeShouldBeSetToCommandBuffer) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(context.get(), pClDevice, props, false);

    const auto &commandStream = cmdQ.getCS(100);
    auto commandStreamAllocation = commandStream.getGraphicsAllocation();
    ASSERT_NE(nullptr, commandStreamAllocation);

    EXPECT_EQ(GraphicsAllocation::AllocationType::COMMAND_BUFFER, commandStreamAllocation->getAllocationType());
}

HWTEST_F(CommandQueueCommandStreamTest, givenMultiDispatchInfoWithSingleKernelWithFlushAllocationsDisabledWhenEstimatingNodesCountThenItEqualsMultiDispatchInfoSize) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableCacheFlushAfterWalker.set(0);

    MockCommandQueueHw<FamilyType> cmdQ(context.get(), pClDevice, nullptr);
    pDevice->getUltCommandStreamReceiver<FamilyType>().multiOsContextCapable = true;
    MockKernelWithInternals mockKernelWithInternals(*pClDevice, context.get());

    mockKernelWithInternals.mockKernel->kernelArgRequiresCacheFlush.resize(1);
    MockGraphicsAllocation cacheRequiringAllocation;
    mockKernelWithInternals.mockKernel->kernelArgRequiresCacheFlush[0] = &cacheRequiringAllocation;

    MockMultiDispatchInfo multiDispatchInfo(pClDevice, std::vector<Kernel *>({mockKernelWithInternals.mockKernel}));

    size_t estimatedNodesCount = cmdQ.estimateTimestampPacketNodesCount(multiDispatchInfo);
    EXPECT_EQ(estimatedNodesCount, multiDispatchInfo.size());
}

HWTEST_F(CommandQueueCommandStreamTest, givenMultiDispatchInfoWithSingleKernelWithFlushAllocationsEnabledWhenEstimatingNodesCountThenItEqualsMultiDispatchInfoSizePlusOne) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableCacheFlushAfterWalker.set(1);

    MockCommandQueueHw<FamilyType> cmdQ(context.get(), pClDevice, nullptr);
    MockKernelWithInternals mockKernelWithInternals(*pClDevice, context.get());

    mockKernelWithInternals.mockKernel->kernelArgRequiresCacheFlush.resize(1);
    MockGraphicsAllocation cacheRequiringAllocation;
    mockKernelWithInternals.mockKernel->kernelArgRequiresCacheFlush[0] = &cacheRequiringAllocation;

    MockMultiDispatchInfo multiDispatchInfo(pClDevice, std::vector<Kernel *>({mockKernelWithInternals.mockKernel}));

    size_t estimatedNodesCount = cmdQ.estimateTimestampPacketNodesCount(multiDispatchInfo);
    EXPECT_EQ(estimatedNodesCount, multiDispatchInfo.size() + 1);
}

struct CommandQueueIndirectHeapTest : public CommandQueueMemoryDevice,
                                      public ::testing::TestWithParam<IndirectHeap::Type> {
    void SetUp() override {
        CommandQueueMemoryDevice::SetUp();
        context.reset(new MockContext(pClDevice));
    }

    void TearDown() override {
        context.reset();
        CommandQueueMemoryDevice::TearDown();
    }
    std::unique_ptr<MockContext> context;
};

TEST_P(CommandQueueIndirectHeapTest, WhenGettingIndirectHeapThenValidObjectIsReturned) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(context.get(), pClDevice, props, false);

    auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), 8192);
    EXPECT_NE(nullptr, &indirectHeap);
}

HWTEST_P(CommandQueueIndirectHeapTest, givenIndirectObjectHeapWhenItIsQueriedForInternalAllocationThenTrueIsReturned) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(context.get(), pClDevice, props, false);
    auto &commandStreamReceiver = pClDevice->getUltCommandStreamReceiver<FamilyType>();

    auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), 8192);
    if (this->GetParam() == IndirectHeap::INDIRECT_OBJECT && commandStreamReceiver.canUse4GbHeaps) {
        EXPECT_TRUE(indirectHeap.getGraphicsAllocation()->is32BitAllocation());
    } else {
        EXPECT_FALSE(indirectHeap.getGraphicsAllocation()->is32BitAllocation());
    }
}

HWTEST_P(CommandQueueIndirectHeapTest, GivenIndirectHeapWhenGettingAvailableSpaceThenCorrectSizeIsReturned) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(context.get(), pClDevice, props, false);

    auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), sizeof(uint32_t));
    if (this->GetParam() == IndirectHeap::SURFACE_STATE) {
        size_t expectedSshUse = cmdQ.getGpgpuCommandStreamReceiver().defaultSshSize - MemoryConstants::pageSize - UnitTestHelper<FamilyType>::getDefaultSshUsage();
        EXPECT_EQ(expectedSshUse, indirectHeap.getAvailableSpace());
    } else {
        EXPECT_EQ(64 * KB, indirectHeap.getAvailableSpace());
    }
}

TEST_P(CommandQueueIndirectHeapTest, GivenRequiredSizeWhenGettingIndirectHeapThenIndirectHeapHasRequiredSize) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(context.get(), pClDevice, props, false);

    size_t requiredSize = 16384;
    const auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), requiredSize);
    ASSERT_NE(nullptr, &indirectHeap);
    EXPECT_GE(indirectHeap.getMaxAvailableSpace(), requiredSize);
}

TEST_P(CommandQueueIndirectHeapTest, WhenGettingIndirectHeapWithNewSizeThenMaxAvailableSpaceIsEqualOrGreaterThanNewSize) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(context.get(), pClDevice, props, false);

    auto &indirectHeapInitial = cmdQ.getIndirectHeap(this->GetParam(), 10);
    size_t requiredSize = indirectHeapInitial.getMaxAvailableSpace() + 42;

    const auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), requiredSize);
    ASSERT_NE(nullptr, &indirectHeap);
    if (this->GetParam() == IndirectHeap::SURFACE_STATE) {
        //no matter what SSH is always capped
        EXPECT_EQ(cmdQ.getGpgpuCommandStreamReceiver().defaultSshSize - MemoryConstants::pageSize,
                  indirectHeap.getMaxAvailableSpace());
    } else {
        EXPECT_LE(requiredSize, indirectHeap.getMaxAvailableSpace());
    }
}

TEST_P(CommandQueueIndirectHeapTest, WhenGettingIndirectHeapThenSizeIsAlignedToCacheLine) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(context.get(), pClDevice, props, false);
    size_t minHeapSize = 64 * KB;

    auto &indirectHeapInitial = cmdQ.getIndirectHeap(this->GetParam(), 2 * minHeapSize + 1);

    EXPECT_TRUE(isAligned<MemoryConstants::cacheLineSize>(indirectHeapInitial.getAvailableSpace()));

    indirectHeapInitial.getSpace(indirectHeapInitial.getAvailableSpace()); // use whole space to force obtain reusable

    const auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), minHeapSize + 1);

    ASSERT_NE(nullptr, &indirectHeap);
    EXPECT_TRUE(isAligned<MemoryConstants::cacheLineSize>(indirectHeap.getAvailableSpace()));
}

HWTEST_P(CommandQueueIndirectHeapTest, givenCommandStreamReceiverWithReusableAllocationsWhenAskedForHeapAllocationThenAllocationFromReusablePoolIsReturned) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(context.get(), pClDevice, props, false);

    auto memoryManager = pDevice->getMemoryManager();

    auto allocationSize = defaultHeapSize * 2;

    GraphicsAllocation *allocation = nullptr;

    auto &commandStreamReceiver = pClDevice->getUltCommandStreamReceiver<FamilyType>();
    auto allocationType = GraphicsAllocation::AllocationType::LINEAR_STREAM;
    if (this->GetParam() == IndirectHeap::INDIRECT_OBJECT && commandStreamReceiver.canUse4GbHeaps) {
        allocationType = GraphicsAllocation::AllocationType::INTERNAL_HEAP;
    }
    allocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), allocationSize, allocationType, pDevice->getDeviceBitfield()});
    if (this->GetParam() == IndirectHeap::SURFACE_STATE) {
        allocation->setSize(commandStreamReceiver.defaultSshSize * 2);
    }

    commandStreamReceiver.getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION);

    EXPECT_FALSE(commandStreamReceiver.getAllocationsForReuse().peekIsEmpty());
    EXPECT_TRUE(commandStreamReceiver.getAllocationsForReuse().peekContains(*allocation));

    const auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), 100);

    EXPECT_EQ(indirectHeap.getGraphicsAllocation(), allocation);

    // if we obtain heap from reusable pool, we need to keep the size of allocation
    // surface state heap is an exception, it is capped at (max_ssh_size_for_HW - page_size)
    if (this->GetParam() == IndirectHeap::SURFACE_STATE) {
        EXPECT_EQ(commandStreamReceiver.defaultSshSize - MemoryConstants::pageSize, indirectHeap.getMaxAvailableSpace());
    } else {
        EXPECT_EQ(allocationSize, indirectHeap.getMaxAvailableSpace());
    }

    EXPECT_TRUE(commandStreamReceiver.getAllocationsForReuse().peekIsEmpty());
}

HWTEST_P(CommandQueueIndirectHeapTest, WhenAskedForNewHeapThenOldHeapIsStoredForReuse) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(context.get(), pClDevice, props, false);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_TRUE(commandStreamReceiver.getAllocationsForReuse().peekIsEmpty());
    *commandStreamReceiver.getTagAddress() = 1u;
    commandStreamReceiver.taskCount = 2u;

    const auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), 100);
    auto heapSize = indirectHeap.getAvailableSpace();

    auto graphicsAllocation = indirectHeap.getGraphicsAllocation();

    // Request a larger heap than the first.
    cmdQ.getIndirectHeap(this->GetParam(), heapSize + 6000);

    EXPECT_FALSE(commandStreamReceiver.getAllocationsForReuse().peekIsEmpty());

    EXPECT_TRUE(commandStreamReceiver.getAllocationsForReuse().peekContains(*graphicsAllocation));
    *commandStreamReceiver.getTagAddress() = 2u;
}

TEST_P(CommandQueueIndirectHeapTest, GivenCommandQueueWithoutHeapAllocationWhenAskedForNewHeapThenNewAllocationIsAcquiredWithoutStoring) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(context.get(), pClDevice, props, false);

    auto memoryManager = pDevice->getMemoryManager();
    auto &csr = pDevice->getUltCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>();
    EXPECT_TRUE(pDevice->getDefaultEngine().commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());

    const auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), 100);
    auto heapSize = indirectHeap.getAvailableSpace();

    auto graphicsAllocation = indirectHeap.getGraphicsAllocation();

    csr.indirectHeap[this->GetParam()]->replaceGraphicsAllocation(nullptr);
    csr.indirectHeap[this->GetParam()]->replaceBuffer(nullptr, 0);

    // Request a larger heap than the first.
    cmdQ.getIndirectHeap(this->GetParam(), heapSize + 6000);

    EXPECT_NE(graphicsAllocation, indirectHeap.getGraphicsAllocation());
    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_P(CommandQueueIndirectHeapTest, givenCommandQueueWithResourceCachingActiveWhenQueueISDestroyedThenIndirectHeapIsNotOnReuseList) {
    auto cmdQ = new MockCommandQueue(context.get(), pClDevice, 0, false);
    cmdQ->getIndirectHeap(this->GetParam(), 100);
    EXPECT_TRUE(pDevice->getDefaultEngine().commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());

    //now destroy command queue, heap should go to reusable list
    delete cmdQ;
    EXPECT_TRUE(pDevice->getDefaultEngine().commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
}

TEST_P(CommandQueueIndirectHeapTest, GivenCommandQueueWithHeapAllocatedWhenIndirectHeapIsReleasedThenHeapAllocationAndHeapBufferIsSetToNullptr) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(context.get(), pClDevice, props, false);

    EXPECT_TRUE(pDevice->getDefaultEngine().commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());

    const auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), 100);
    auto heapSize = indirectHeap.getMaxAvailableSpace();

    EXPECT_NE(0u, heapSize);

    auto graphicsAllocation = indirectHeap.getGraphicsAllocation();
    EXPECT_NE(nullptr, graphicsAllocation);

    cmdQ.releaseIndirectHeap(this->GetParam());
    auto &csr = pDevice->getUltCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>();

    EXPECT_EQ(nullptr, csr.indirectHeap[this->GetParam()]->getGraphicsAllocation());

    EXPECT_EQ(nullptr, indirectHeap.getCpuBase());
    EXPECT_EQ(0u, indirectHeap.getMaxAvailableSpace());
}

TEST_P(CommandQueueIndirectHeapTest, GivenCommandQueueWithoutHeapAllocatedWhenIndirectHeapIsReleasedThenIndirectHeapAllocationStaysNull) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(context.get(), pClDevice, props, false);

    cmdQ.releaseIndirectHeap(this->GetParam());
    auto &csr = pDevice->getUltCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>();

    EXPECT_EQ(nullptr, csr.indirectHeap[this->GetParam()]);
}

TEST_P(CommandQueueIndirectHeapTest, GivenCommandQueueWithHeapWhenGraphicAllocationIsNullThenNothingOnReuseList) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(context.get(), pClDevice, props, false);

    auto &ih = cmdQ.getIndirectHeap(this->GetParam(), 0u);
    auto allocation = ih.getGraphicsAllocation();
    EXPECT_NE(nullptr, allocation);
    auto &csr = pDevice->getUltCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>();

    csr.indirectHeap[this->GetParam()]->replaceGraphicsAllocation(nullptr);
    csr.indirectHeap[this->GetParam()]->replaceBuffer(nullptr, 0);

    cmdQ.releaseIndirectHeap(this->GetParam());

    auto memoryManager = pDevice->getMemoryManager();
    EXPECT_TRUE(pDevice->getDefaultEngine().commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_P(CommandQueueIndirectHeapTest, givenCommandQueueWhenGetIndirectHeapIsCalledThenIndirectHeapAllocationTypeShouldBeSetToInternalHeapForIohAndLinearStreamForOthers) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(context.get(), pClDevice, props, false);
    auto &commandStreamReceiver = pClDevice->getUltCommandStreamReceiver<FamilyType>();

    auto heapType = this->GetParam();

    bool requireInternalHeap = IndirectHeap::INDIRECT_OBJECT == heapType && commandStreamReceiver.canUse4GbHeaps;
    const auto &indirectHeap = cmdQ.getIndirectHeap(heapType, 100);
    auto indirectHeapAllocation = indirectHeap.getGraphicsAllocation();
    ASSERT_NE(nullptr, indirectHeapAllocation);
    auto expectedAllocationType = GraphicsAllocation::AllocationType::LINEAR_STREAM;
    if (requireInternalHeap) {
        expectedAllocationType = GraphicsAllocation::AllocationType::INTERNAL_HEAP;
    }
    EXPECT_EQ(expectedAllocationType, indirectHeapAllocation->getAllocationType());
}

TEST_P(CommandQueueIndirectHeapTest, givenCommandQueueWhenGetHeapMemoryIsCalledThenHeapIsCreated) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(context.get(), pClDevice, props, false);

    IndirectHeap *indirectHeap = nullptr;
    cmdQ.allocateHeapMemory(this->GetParam(), 100, indirectHeap);
    EXPECT_NE(nullptr, indirectHeap);
    EXPECT_NE(nullptr, indirectHeap->getGraphicsAllocation());

    pDevice->getMemoryManager()->freeGraphicsMemory(indirectHeap->getGraphicsAllocation());
    delete indirectHeap;
}

TEST_P(CommandQueueIndirectHeapTest, givenCommandQueueWhenGetHeapMemoryIsCalledWithAlreadyAllocatedHeapThenGraphicsAllocationIsCreated) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(context.get(), pClDevice, props, false);

    IndirectHeap heap(nullptr, size_t{100});

    IndirectHeap *indirectHeap = &heap;
    cmdQ.allocateHeapMemory(this->GetParam(), 100, indirectHeap);
    EXPECT_EQ(&heap, indirectHeap);
    EXPECT_NE(nullptr, indirectHeap->getGraphicsAllocation());

    pDevice->getMemoryManager()->freeGraphicsMemory(indirectHeap->getGraphicsAllocation());
}

INSTANTIATE_TEST_CASE_P(
    Device,
    CommandQueueIndirectHeapTest,
    testing::Values(
        IndirectHeap::DYNAMIC_STATE,
        IndirectHeap::INDIRECT_OBJECT,
        IndirectHeap::SURFACE_STATE));

using CommandQueueTests = ::testing::Test;
HWTEST_F(CommandQueueTests, givenMultipleCommandQueuesWhenMarkerIsEmittedThenGraphicsAllocationIsReused) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    MockContext context(device.get());
    std::unique_ptr<CommandQueue> commandQ(new MockCommandQueue(&context, device.get(), 0, false));
    *device->getDefaultEngine().commandStreamReceiver->getTagAddress() = 0;
    commandQ->enqueueMarkerWithWaitList(0, nullptr, nullptr);
    commandQ->enqueueMarkerWithWaitList(0, nullptr, nullptr);

    auto commandStreamGraphicsAllocation = commandQ->getCS(0).getGraphicsAllocation();
    commandQ.reset(new MockCommandQueue(&context, device.get(), 0, false));
    commandQ->enqueueMarkerWithWaitList(0, nullptr, nullptr);
    commandQ->enqueueMarkerWithWaitList(0, nullptr, nullptr);
    auto commandStreamGraphicsAllocation2 = commandQ->getCS(0).getGraphicsAllocation();
    EXPECT_EQ(commandStreamGraphicsAllocation, commandStreamGraphicsAllocation2);
}

struct WaitForQueueCompletionTests : public ::testing::Test {
    template <typename Family>
    struct MyCmdQueue : public CommandQueueHw<Family> {
        MyCmdQueue(Context *context, ClDevice *device) : CommandQueueHw<Family>(context, device, nullptr, false){};
        void waitUntilComplete(uint32_t gpgpuTaskCountToWait, uint32_t bcsTaskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep) override {
            requestedUseQuickKmdSleep = useQuickKmdSleep;
            waitUntilCompleteCounter++;
        }
        bool isQueueBlocked() override {
            return false;
        }
        bool requestedUseQuickKmdSleep = false;
        uint32_t waitUntilCompleteCounter = 0;
    };

    void SetUp() override {
        device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
        context.reset(new MockContext(device.get()));
    }

    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockContext> context;
};

HWTEST_F(WaitForQueueCompletionTests, givenBlockingCallAndUnblockedQueueWhenEnqueuedThenCallWaitWithoutQuickKmdSleepRequest) {
    std::unique_ptr<MyCmdQueue<FamilyType>> cmdQ(new MyCmdQueue<FamilyType>(context.get(), device.get()));
    uint32_t tmpPtr = 0;
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create(context.get()));
    cmdQ->enqueueReadBuffer(buffer.get(), CL_TRUE, 0, 1, &tmpPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(1u, cmdQ->waitUntilCompleteCounter);
    EXPECT_FALSE(cmdQ->requestedUseQuickKmdSleep);
}

HWTEST_F(WaitForQueueCompletionTests, givenBlockingCallAndBlockedQueueWhenEnqueuedThenCallWaitWithoutQuickKmdSleepRequest) {
    std::unique_ptr<MyCmdQueue<FamilyType>> cmdQ(new MyCmdQueue<FamilyType>(context.get(), device.get()));
    std::unique_ptr<Event> blockingEvent(new Event(cmdQ.get(), CL_COMMAND_NDRANGE_KERNEL, 0, 0));
    cl_event clBlockingEvent = blockingEvent.get();
    uint32_t tmpPtr = 0;
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create(context.get()));
    cmdQ->enqueueReadBuffer(buffer.get(), CL_TRUE, 0, 1, &tmpPtr, nullptr, 1, &clBlockingEvent, nullptr);
    EXPECT_EQ(1u, cmdQ->waitUntilCompleteCounter);
    EXPECT_FALSE(cmdQ->requestedUseQuickKmdSleep);
}

HWTEST_F(WaitForQueueCompletionTests, whenFinishIsCalledThenCallWaitWithoutQuickKmdSleepRequest) {
    std::unique_ptr<MyCmdQueue<FamilyType>> cmdQ(new MyCmdQueue<FamilyType>(context.get(), device.get()));
    cmdQ->finish();
    EXPECT_EQ(1u, cmdQ->waitUntilCompleteCounter);
    EXPECT_FALSE(cmdQ->requestedUseQuickKmdSleep);
}

TEST(CommandQueue, givenEnqueueAcquireSharedObjectsWhenNoObjectsThenReturnSuccess) {
    MockContext context;
    MockCommandQueue cmdQ(&context, nullptr, 0, false);

    cl_uint numObjects = 0;
    cl_mem *memObjects = nullptr;

    cl_int result = cmdQ.enqueueAcquireSharedObjects(numObjects, memObjects, 0, nullptr, nullptr, 0);
    EXPECT_EQ(result, CL_SUCCESS);
}

class MockSharingHandler : public SharingHandler {
  public:
    void synchronizeObject(UpdateData &updateData) override {
        updateData.synchronizationStatus = ACQUIRE_SUCCESFUL;
    }
};

TEST(CommandQueue, givenEnqueuesForSharedObjectsWithImageWhenUsingSharingHandlerThenReturnSuccess) {
    MockContext context;
    MockCommandQueue cmdQ(&context, context.getDevice(0), 0, false);
    MockSharingHandler *mockSharingHandler = new MockSharingHandler;

    auto image = std::unique_ptr<Image>(ImageHelper<Image2dDefaults>::create(&context));
    image->setSharingHandler(mockSharingHandler);

    cl_mem memObject = image.get();
    cl_uint numObjects = 1;
    cl_mem *memObjects = &memObject;

    cl_int result = cmdQ.enqueueAcquireSharedObjects(numObjects, memObjects, 0, nullptr, nullptr, 0);
    EXPECT_EQ(result, CL_SUCCESS);

    result = cmdQ.enqueueReleaseSharedObjects(numObjects, memObjects, 0, nullptr, nullptr, 0);
    EXPECT_EQ(result, CL_SUCCESS);
}

TEST(CommandQueue, givenEnqueuesForSharedObjectsWithImageWhenUsingSharingHandlerWithEventThenReturnSuccess) {
    auto mockDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext context;
    MockCommandQueue cmdQ(&context, mockDevice.get(), 0, false);
    MockSharingHandler *mockSharingHandler = new MockSharingHandler;

    auto image = std::unique_ptr<Image>(ImageHelper<Image2dDefaults>::create(&context));
    image->setSharingHandler(mockSharingHandler);

    cl_mem memObject = image.get();
    cl_uint numObjects = 1;
    cl_mem *memObjects = &memObject;

    Event *eventAcquire = new Event(&cmdQ, CL_COMMAND_NDRANGE_KERNEL, 1, 5);
    cl_event clEventAquire = eventAcquire;
    cl_int result = cmdQ.enqueueAcquireSharedObjects(numObjects, memObjects, 0, nullptr, &clEventAquire, 0);
    EXPECT_EQ(result, CL_SUCCESS);
    ASSERT_NE(clEventAquire, nullptr);
    eventAcquire->release();

    Event *eventRelease = new Event(&cmdQ, CL_COMMAND_NDRANGE_KERNEL, 1, 5);
    cl_event clEventRelease = eventRelease;
    result = cmdQ.enqueueReleaseSharedObjects(numObjects, memObjects, 0, nullptr, &clEventRelease, 0);
    EXPECT_EQ(result, CL_SUCCESS);
    ASSERT_NE(clEventRelease, nullptr);
    eventRelease->release();
}

TEST(CommandQueue, givenEnqueueAcquireSharedObjectsWhenIncorrectArgumentsThenReturnProperError) {
    MockContext context;
    MockCommandQueue cmdQ(&context, nullptr, 0, false);

    cl_uint numObjects = 1;
    cl_mem *memObjects = nullptr;

    cl_int result = cmdQ.enqueueAcquireSharedObjects(numObjects, memObjects, 0, nullptr, nullptr, 0);
    EXPECT_EQ(result, CL_INVALID_VALUE);

    numObjects = 0;
    memObjects = (cl_mem *)1;

    result = cmdQ.enqueueAcquireSharedObjects(numObjects, memObjects, 0, nullptr, nullptr, 0);
    EXPECT_EQ(result, CL_INVALID_VALUE);

    numObjects = 0;
    memObjects = (cl_mem *)1;

    result = cmdQ.enqueueAcquireSharedObjects(numObjects, memObjects, 0, nullptr, nullptr, 0);
    EXPECT_EQ(result, CL_INVALID_VALUE);

    cl_mem memObject = nullptr;

    numObjects = 1;
    memObjects = &memObject;

    result = cmdQ.enqueueAcquireSharedObjects(numObjects, memObjects, 0, nullptr, nullptr, 0);
    EXPECT_EQ(result, CL_INVALID_MEM_OBJECT);

    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create(&context));
    memObject = buffer.get();

    numObjects = 1;
    memObjects = &memObject;

    result = cmdQ.enqueueAcquireSharedObjects(numObjects, memObjects, 0, nullptr, nullptr, 0);
    EXPECT_EQ(result, CL_INVALID_MEM_OBJECT);
}

TEST(CommandQueue, givenEnqueueReleaseSharedObjectsWhenNoObjectsThenReturnSuccess) {
    MockContext context;
    MockCommandQueue cmdQ(&context, nullptr, 0, false);

    cl_uint numObjects = 0;
    cl_mem *memObjects = nullptr;

    cl_int result = cmdQ.enqueueReleaseSharedObjects(numObjects, memObjects, 0, nullptr, nullptr, 0);
    EXPECT_EQ(result, CL_SUCCESS);
}

TEST(CommandQueue, givenEnqueueReleaseSharedObjectsWhenIncorrectArgumentsThenReturnProperError) {
    MockContext context;
    MockCommandQueue cmdQ(&context, nullptr, 0, false);

    cl_uint numObjects = 1;
    cl_mem *memObjects = nullptr;

    cl_int result = cmdQ.enqueueReleaseSharedObjects(numObjects, memObjects, 0, nullptr, nullptr, 0);
    EXPECT_EQ(result, CL_INVALID_VALUE);

    numObjects = 0;
    memObjects = (cl_mem *)1;

    result = cmdQ.enqueueReleaseSharedObjects(numObjects, memObjects, 0, nullptr, nullptr, 0);
    EXPECT_EQ(result, CL_INVALID_VALUE);

    numObjects = 0;
    memObjects = (cl_mem *)1;

    result = cmdQ.enqueueReleaseSharedObjects(numObjects, memObjects, 0, nullptr, nullptr, 0);
    EXPECT_EQ(result, CL_INVALID_VALUE);

    cl_mem memObject = nullptr;

    numObjects = 1;
    memObjects = &memObject;

    result = cmdQ.enqueueReleaseSharedObjects(numObjects, memObjects, 0, nullptr, nullptr, 0);
    EXPECT_EQ(result, CL_INVALID_MEM_OBJECT);

    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create(&context));
    memObject = buffer.get();

    numObjects = 1;
    memObjects = &memObject;

    result = cmdQ.enqueueReleaseSharedObjects(numObjects, memObjects, 0, nullptr, nullptr, 0);
    EXPECT_EQ(result, CL_INVALID_MEM_OBJECT);
}

TEST(CommandQueue, givenEnqueueAcquireSharedObjectsCallWhenAcquireFailsThenCorrectErrorIsReturned) {
    const auto rootDeviceIndex = 1u;
    class MockSharingHandler : public SharingHandler {
        int validateUpdateData(UpdateData &data) override {
            EXPECT_EQ(1u, data.rootDeviceIndex);
            return CL_INVALID_MEM_OBJECT;
        }
    };

    UltClDeviceFactory deviceFactory{2, 0};
    MockContext context(deviceFactory.rootDevices[rootDeviceIndex]);
    MockCommandQueue cmdQ(&context, context.getDevice(0), 0, false);
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create(&context));

    MockSharingHandler *handler = new MockSharingHandler;
    buffer->setSharingHandler(handler);
    cl_mem memObject = buffer.get();
    auto retVal = cmdQ.enqueueAcquireSharedObjects(1, &memObject, 0, nullptr, nullptr, 0);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
    buffer->setSharingHandler(nullptr);
}

HWTEST_F(CommandQueueCommandStreamTest, givenDebugKernelWhenSetupDebugSurfaceIsCalledThenSurfaceStateIsCorrectlySet) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    MockProgram program(toClDeviceVector(*pClDevice));
    program.enableKernelDebug();
    std::unique_ptr<MockDebugKernel> kernel(MockKernel::create<MockDebugKernel>(*pDevice, &program));
    MockCommandQueue cmdQ(context.get(), pClDevice, 0, false);

    const auto &systemThreadSurfaceAddress = kernel->getAllocatedKernelInfo()->kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful;
    kernel->setSshLocal(nullptr, sizeof(RENDER_SURFACE_STATE) + systemThreadSurfaceAddress);
    auto &commandStreamReceiver = cmdQ.getGpgpuCommandStreamReceiver();

    cmdQ.getGpgpuCommandStreamReceiver().allocateDebugSurface(SipKernel::maxDbgSurfaceSize);
    cmdQ.setupDebugSurface(kernel.get());

    auto debugSurface = commandStreamReceiver.getDebugSurfaceAllocation();
    ASSERT_NE(nullptr, debugSurface);
    RENDER_SURFACE_STATE *surfaceState = (RENDER_SURFACE_STATE *)kernel->getSurfaceStateHeap();
    EXPECT_EQ(debugSurface->getGpuAddress(), surfaceState->getSurfaceBaseAddress());
}

HWTEST_F(CommandQueueCommandStreamTest, givenCsrWithDebugSurfaceAllocatedWhenSetupDebugSurfaceIsCalledThenDebugSurfaceIsReused) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    MockProgram program(toClDeviceVector(*pClDevice));
    program.enableKernelDebug();
    std::unique_ptr<MockDebugKernel> kernel(MockKernel::create<MockDebugKernel>(*pDevice, &program));
    MockCommandQueue cmdQ(context.get(), pClDevice, 0, false);

    const auto &systemThreadSurfaceAddress = kernel->getAllocatedKernelInfo()->kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful;
    kernel->setSshLocal(nullptr, sizeof(RENDER_SURFACE_STATE) + systemThreadSurfaceAddress);
    auto &commandStreamReceiver = cmdQ.getGpgpuCommandStreamReceiver();
    commandStreamReceiver.allocateDebugSurface(SipKernel::maxDbgSurfaceSize);
    auto debugSurface = commandStreamReceiver.getDebugSurfaceAllocation();
    ASSERT_NE(nullptr, debugSurface);

    cmdQ.setupDebugSurface(kernel.get());

    EXPECT_EQ(debugSurface, commandStreamReceiver.getDebugSurfaceAllocation());
    RENDER_SURFACE_STATE *surfaceState = (RENDER_SURFACE_STATE *)kernel->getSurfaceStateHeap();
    EXPECT_EQ(debugSurface->getGpuAddress(), surfaceState->getSurfaceBaseAddress());
}

struct MockTimestampPacketContainer : TimestampPacketContainer {
    MockTimestampPacketContainer(Context &context) : context(context) {
    }
    ~MockTimestampPacketContainer() override {
        EXPECT_EQ(1, context.getRefInternalCount());
    }
    Context &context;
};

TEST(CommandQueueDestructorTest, whenCommandQueueIsDestroyedThenDestroysTimestampPacketContainerBeforeReleasingContext) {
    auto context = new MockContext;
    EXPECT_EQ(1, context->getRefInternalCount());
    MockCommandQueue queue(context, context->getDevice(0), nullptr, false);
    queue.timestampPacketContainer.reset(new MockTimestampPacketContainer(*context));
    EXPECT_EQ(2, context->getRefInternalCount());
    context->release();
    EXPECT_EQ(1, context->getRefInternalCount());
}

TEST(CommandQueuePropertiesTests, whenGetEngineIsCalledThenQueueEngineIsReturned) {
    MockCommandQueue queue;
    EngineControl engineControl;
    queue.gpgpuEngine = &engineControl;
    EXPECT_EQ(queue.gpgpuEngine, &queue.getGpgpuEngine());
}

TEST(CommandQueue, GivenCommandQueueWhenEnqueueResourceBarrierCalledThenSuccessReturned) {
    MockContext context;
    MockCommandQueue cmdQ(&context, nullptr, 0, false);

    cl_int result = cmdQ.enqueueResourceBarrier(
        nullptr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, result);
}

TEST(CommandQueue, GivenCommandQueueWhenCheckingIfIsCacheFlushCommandCalledThenFalseReturned) {
    MockContext context;
    MockCommandQueue cmdQ(&context, nullptr, 0, false);

    bool isCommandCacheFlush = cmdQ.isCacheFlushCommand(0u);
    EXPECT_FALSE(isCommandCacheFlush);
}

TEST(CommandQueue, GivenCommandQueueWhenEnqueueInitDispatchGlobalsCalledThenSuccessReturned) {
    MockContext context;
    MockCommandQueue cmdQ(&context, nullptr, 0, false);

    cl_int result = cmdQ.enqueueInitDispatchGlobals(
        nullptr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, result);
}

TEST(CommandQueue, givenBlitterOperationsSupportedWhenCreatingQueueThenTimestampPacketIsCreated) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableTimestampPacket.set(0);

    MockContext context{};
    HardwareInfo *hwInfo = context.getDevice(0)->getRootDeviceEnvironment().getMutableHardwareInfo();
    if (!HwHelper::get(hwInfo->platform.eDisplayCoreFamily).obtainBlitterPreference(*hwInfo)) {
        GTEST_SKIP();
    }

    hwInfo->capabilityTable.blitterOperationsSupported = true;
    MockCommandQueue cmdQ(&context, context.getDevice(0), 0, false);
    EXPECT_NE(nullptr, cmdQ.timestampPacketContainer);
}

TEST(CommandQueue, givenCopyOnlyQueueWhenCallingBlitEnqueueAllowedThenReturnTrue) {
    MockContext context{};
    HardwareInfo *hwInfo = context.getDevice(0)->getRootDeviceEnvironment().getMutableHardwareInfo();
    MockCommandQueue queue(&context, context.getDevice(0), 0, false);
    if (!queue.bcsEngine) {
        queue.bcsEngine = &context.getDevice(0)->getDefaultEngine();
    }
    hwInfo->capabilityTable.blitterOperationsSupported = false;

    queue.isCopyOnly = false;
    EXPECT_EQ(queue.getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled(),
              queue.blitEnqueueAllowed(CL_COMMAND_READ_BUFFER));

    queue.isCopyOnly = true;
    EXPECT_TRUE(queue.blitEnqueueAllowed(CL_COMMAND_READ_BUFFER));
}

TEST(CommandQueue, givenClCommandWhenCallingBlitEnqueueAllowedThenReturnCorrectValue) {
    MockContext context{};

    MockCommandQueue queue(&context, context.getDevice(0), 0, false);
    if (!queue.bcsEngine) {
        queue.bcsEngine = &context.getDevice(0)->getDefaultEngine();
    }

    bool supported = queue.getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled();

    EXPECT_EQ(supported, queue.blitEnqueueAllowed(CL_COMMAND_READ_BUFFER));
    EXPECT_EQ(supported, queue.blitEnqueueAllowed(CL_COMMAND_WRITE_BUFFER));
    EXPECT_EQ(supported, queue.blitEnqueueAllowed(CL_COMMAND_COPY_BUFFER));
    EXPECT_EQ(supported, queue.blitEnqueueAllowed(CL_COMMAND_READ_BUFFER_RECT));
    EXPECT_EQ(supported, queue.blitEnqueueAllowed(CL_COMMAND_WRITE_BUFFER_RECT));
    EXPECT_EQ(supported, queue.blitEnqueueAllowed(CL_COMMAND_COPY_BUFFER_RECT));
    EXPECT_EQ(supported, queue.blitEnqueueAllowed(CL_COMMAND_SVM_MEMCPY));
    EXPECT_EQ(supported, queue.blitEnqueueAllowed(CL_COMMAND_READ_IMAGE));
    EXPECT_EQ(supported, queue.blitEnqueueAllowed(CL_COMMAND_WRITE_IMAGE));
    EXPECT_EQ(supported, queue.blitEnqueueAllowed(CL_COMMAND_COPY_IMAGE));
    EXPECT_FALSE(queue.blitEnqueueAllowed(CL_COMMAND_COPY_IMAGE_TO_BUFFER));
}

TEST(CommandQueue, givenRegularClCommandWhenCallingBlitEnqueuePreferredThenReturnCorrectValue) {
    MockContext context{};
    MockCommandQueue queue{context};
    BuiltinOpParams builtinOpParams{};

    EXPECT_TRUE(queue.blitEnqueuePreferred(CL_COMMAND_READ_BUFFER, builtinOpParams));
    EXPECT_TRUE(queue.blitEnqueuePreferred(CL_COMMAND_WRITE_BUFFER, builtinOpParams));
    EXPECT_TRUE(queue.blitEnqueuePreferred(CL_COMMAND_READ_BUFFER_RECT, builtinOpParams));
    EXPECT_TRUE(queue.blitEnqueuePreferred(CL_COMMAND_WRITE_BUFFER_RECT, builtinOpParams));
    EXPECT_TRUE(queue.blitEnqueuePreferred(CL_COMMAND_COPY_BUFFER_RECT, builtinOpParams));
    EXPECT_TRUE(queue.blitEnqueuePreferred(CL_COMMAND_READ_IMAGE, builtinOpParams));
    EXPECT_TRUE(queue.blitEnqueuePreferred(CL_COMMAND_WRITE_IMAGE, builtinOpParams));
    EXPECT_TRUE(queue.blitEnqueuePreferred(CL_COMMAND_COPY_IMAGE, builtinOpParams));
}

TEST(CommandQueue, givenLocalToLocalCopyBufferCommandWhenCallingBlitEnqueuePreferredThenReturnValueBasedOnDebugFlagAndHwPreference) {
    const bool preferBlitterHw = ClHwHelper::get(::defaultHwInfo->platform.eRenderCoreFamily).preferBlitterForLocalToLocalTransfers();
    DebugManagerStateRestore restore{};
    MockContext context{};
    MockCommandQueue queue{context};
    BuiltinOpParams builtinOpParams{};
    MockGraphicsAllocation srcGraphicsAllocation{};
    MockGraphicsAllocation dstGraphicsAllocation{};
    MockBuffer srcMemObj{srcGraphicsAllocation};
    MockBuffer dstMemObj{dstGraphicsAllocation};
    builtinOpParams.srcMemObj = &srcMemObj;
    builtinOpParams.dstMemObj = &dstMemObj;

    srcGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
    dstGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
    DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(-1);
    EXPECT_EQ(preferBlitterHw, queue.blitEnqueuePreferred(CL_COMMAND_COPY_BUFFER, builtinOpParams));
    DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(0);
    EXPECT_FALSE(queue.blitEnqueuePreferred(CL_COMMAND_COPY_BUFFER, builtinOpParams));
    DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(1);
    EXPECT_TRUE(queue.blitEnqueuePreferred(CL_COMMAND_COPY_BUFFER, builtinOpParams));
}

TEST(CommandQueue, givenNotLocalToLocalCopyBufferCommandWhenCallingBlitEnqueuePreferredThenReturnTrueRegardlessOfDebugFlag) {
    DebugManagerStateRestore restore{};
    MockContext context{};
    MockCommandQueue queue{context};
    BuiltinOpParams builtinOpParams{};
    MockGraphicsAllocation srcGraphicsAllocation{};
    MockGraphicsAllocation dstGraphicsAllocation{};
    MockBuffer srcMemObj{srcGraphicsAllocation};
    MockBuffer dstMemObj{dstGraphicsAllocation};
    builtinOpParams.srcMemObj = &srcMemObj;
    builtinOpParams.dstMemObj = &dstMemObj;

    srcGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
    dstGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
    DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(-1);
    EXPECT_TRUE(queue.blitEnqueuePreferred(CL_COMMAND_COPY_BUFFER, builtinOpParams));
    DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(0);
    EXPECT_TRUE(queue.blitEnqueuePreferred(CL_COMMAND_COPY_BUFFER, builtinOpParams));
    DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(1);
    EXPECT_TRUE(queue.blitEnqueuePreferred(CL_COMMAND_COPY_BUFFER, builtinOpParams));

    srcGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
    dstGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
    DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(-1);
    EXPECT_TRUE(queue.blitEnqueuePreferred(CL_COMMAND_COPY_BUFFER, builtinOpParams));
    DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(0);
    EXPECT_TRUE(queue.blitEnqueuePreferred(CL_COMMAND_COPY_BUFFER, builtinOpParams));
    DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(1);
    EXPECT_TRUE(queue.blitEnqueuePreferred(CL_COMMAND_COPY_BUFFER, builtinOpParams));
}

TEST(CommandQueue, givenLocalToLocalSvmCopyCommandWhenCallingBlitEnqueuePreferredThenReturnValueBasedOnDebugFlagAndHwPreference) {
    const bool preferBlitterHw = ClHwHelper::get(::defaultHwInfo->platform.eRenderCoreFamily).preferBlitterForLocalToLocalTransfers();
    DebugManagerStateRestore restore{};
    MockContext context{};
    MockCommandQueue queue{context};
    BuiltinOpParams builtinOpParams{};
    MockGraphicsAllocation srcSvmAlloc{};
    MockGraphicsAllocation dstSvmAlloc{};
    builtinOpParams.srcSvmAlloc = &srcSvmAlloc;
    builtinOpParams.dstSvmAlloc = &dstSvmAlloc;

    srcSvmAlloc.memoryPool = MemoryPool::LocalMemory;
    dstSvmAlloc.memoryPool = MemoryPool::LocalMemory;
    DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(-1);
    EXPECT_EQ(preferBlitterHw, queue.blitEnqueuePreferred(CL_COMMAND_SVM_MEMCPY, builtinOpParams));
    DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(0);
    EXPECT_FALSE(queue.blitEnqueuePreferred(CL_COMMAND_SVM_MEMCPY, builtinOpParams));
    DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(1);
    EXPECT_TRUE(queue.blitEnqueuePreferred(CL_COMMAND_SVM_MEMCPY, builtinOpParams));
}

TEST(CommandQueue, givenNotLocalToLocalSvmCopyCommandWhenCallingBlitEnqueuePreferredThenReturnTrueRegardlessOfDebugFlag) {
    DebugManagerStateRestore restore{};
    MockContext context{};
    MockCommandQueue queue{context};
    BuiltinOpParams builtinOpParams{};
    MockGraphicsAllocation srcSvmAlloc{};
    MockGraphicsAllocation dstSvmAlloc{};
    builtinOpParams.srcSvmAlloc = &srcSvmAlloc;
    builtinOpParams.dstSvmAlloc = &dstSvmAlloc;

    srcSvmAlloc.memoryPool = MemoryPool::System4KBPages;
    dstSvmAlloc.memoryPool = MemoryPool::LocalMemory;
    DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(-1);
    EXPECT_TRUE(queue.blitEnqueuePreferred(CL_COMMAND_SVM_MEMCPY, builtinOpParams));
    DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(0);
    EXPECT_TRUE(queue.blitEnqueuePreferred(CL_COMMAND_SVM_MEMCPY, builtinOpParams));
    DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(1);
    EXPECT_TRUE(queue.blitEnqueuePreferred(CL_COMMAND_SVM_MEMCPY, builtinOpParams));

    srcSvmAlloc.memoryPool = MemoryPool::LocalMemory;
    dstSvmAlloc.memoryPool = MemoryPool::System4KBPages;
    DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(-1);
    EXPECT_TRUE(queue.blitEnqueuePreferred(CL_COMMAND_SVM_MEMCPY, builtinOpParams));
    DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(0);
    EXPECT_TRUE(queue.blitEnqueuePreferred(CL_COMMAND_SVM_MEMCPY, builtinOpParams));
    DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(1);
    EXPECT_TRUE(queue.blitEnqueuePreferred(CL_COMMAND_SVM_MEMCPY, builtinOpParams));
}

TEST(CommandQueue, givenCopySizeAndOffsetWhenCallingBlitEnqueueImageAllowedThenReturnCorrectValue) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableBlitterForEnqueueImageOperations.set(1);
    MockContext context{};
    MockCommandQueue queue(&context, context.getDevice(0), 0, false);
    MockImageBase image;
    image.imageDesc.num_mip_levels = 1;

    auto maxBlitWidth = static_cast<size_t>(BlitterConstants::maxBlitWidth);
    auto maxBlitHeight = static_cast<size_t>(BlitterConstants::maxBlitHeight);

    std::tuple<size_t, size_t, size_t, size_t, bool> testParams[]{
        {1, 1, 0, 0, true},
        {maxBlitWidth, maxBlitHeight, 0, 0, true},
        {maxBlitWidth + 1, maxBlitHeight, 0, 0, false},
        {maxBlitWidth, maxBlitHeight + 1, 0, 0, false},
        {maxBlitWidth, maxBlitHeight, 1, 0, false},
        {maxBlitWidth, maxBlitHeight, 0, 1, false},
        {maxBlitWidth - 1, maxBlitHeight - 1, 1, 1, true}};

    for (auto &[regionX, regionY, originX, originY, expectedResult] : testParams) {
        size_t region[3] = {regionX, regionY, 0};
        size_t origin[3] = {originX, originY, 0};
        EXPECT_EQ(expectedResult, queue.blitEnqueueImageAllowed(origin, region, image));
    }
}

TEST(CommandQueue, givenMipMappedImageWhenCallingBlitEnqueueImageAllowedThenCorrectResultIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableBlitterForEnqueueImageOperations.set(1);
    MockContext context{};
    MockCommandQueue queue(&context, context.getDevice(0), 0, false);

    size_t correctRegion[3] = {10u, 10u, 0};
    size_t correctOrigin[3] = {1u, 1u, 0};
    MockImageBase image;

    image.imageDesc.num_mip_levels = 1;
    EXPECT_TRUE(queue.blitEnqueueImageAllowed(correctOrigin, correctRegion, image));

    image.imageDesc.num_mip_levels = 2;
    EXPECT_FALSE(queue.blitEnqueueImageAllowed(correctOrigin, correctRegion, image));
}

TEST(CommandQueue, givenImageWithDifferentImageTypesWhenCallingBlitEnqueueImageAllowedThenCorrectResultIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableBlitterForEnqueueImageOperations.set(1);
    MockContext context{};
    MockCommandQueue queue(&context, context.getDevice(0), 0, false);

    size_t correctRegion[3] = {10u, 10u, 0};
    size_t correctOrigin[3] = {1u, 1u, 0};
    MockImageBase image;

    int imageTypes[] = {CL_MEM_OBJECT_IMAGE1D, CL_MEM_OBJECT_IMAGE1D_ARRAY, CL_MEM_OBJECT_IMAGE2D, CL_MEM_OBJECT_IMAGE2D_ARRAY, CL_MEM_OBJECT_IMAGE3D};

    for (auto imageType : imageTypes) {
        image.imageDesc.image_type = imageType;
        EXPECT_TRUE(queue.blitEnqueueImageAllowed(correctOrigin, correctRegion, image));
    }
}

TEST(CommandQueue, givenSupportForOperationWhenValidatingSupportThenReturnSuccess) {
    MockCommandQueue queue{};

    queue.queueCapabilities = CL_QUEUE_CAPABILITY_MAP_BUFFER_INTEL;
    EXPECT_FALSE(queue.validateCapabilityForOperation(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL, 0, nullptr, nullptr));

    queue.queueCapabilities |= CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL;
    EXPECT_TRUE(queue.validateCapabilityForOperation(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL, 0, nullptr, nullptr));
}

TEST(CommandQueue, givenSupportForWaitListAndWaitListPassedWhenValidatingSupportThenReturnSuccess) {
    MockContext context{};
    MockCommandQueue queue{context};

    MockEvent<Event> events[] = {
        {&queue, CL_COMMAND_READ_BUFFER, 0, 0},
        {&queue, CL_COMMAND_READ_BUFFER, 0, 0},
        {&queue, CL_COMMAND_READ_BUFFER, 0, 0},
    };
    MockEvent<UserEvent> userEvent{&context};
    const cl_event waitList[] = {events, events + 1, events + 2, &userEvent};
    const cl_uint waitListSize = static_cast<cl_uint>(arrayCount(waitList));

    queue.queueCapabilities = CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL;
    EXPECT_TRUE(queue.validateCapabilityForOperation(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL, 0, nullptr, nullptr));
    EXPECT_FALSE(queue.validateCapabilityForOperation(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL, waitListSize, waitList, nullptr));

    queue.queueCapabilities = CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL |
                              CL_QUEUE_CAPABILITY_SINGLE_QUEUE_EVENT_WAIT_LIST_INTEL |
                              CL_QUEUE_CAPABILITY_CREATE_SINGLE_QUEUE_EVENTS_INTEL;
    EXPECT_TRUE(queue.validateCapabilityForOperation(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL, 0, nullptr, nullptr));
    EXPECT_TRUE(queue.validateCapabilityForOperation(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL, waitListSize, waitList, nullptr));
}

TEST(CommandQueue, givenCrossQueueDependencyAndBothQueuesSupportItWhenValidatingSupportThenReturnTrue) {
    MockContext context{};
    MockCommandQueue queue{context};
    MockCommandQueue otherQueue{context};

    MockEvent<Event> events[] = {
        {&otherQueue, CL_COMMAND_READ_BUFFER, 0, 0},
        {&otherQueue, CL_COMMAND_READ_BUFFER, 0, 0},
        {&otherQueue, CL_COMMAND_READ_BUFFER, 0, 0},
    };
    const cl_event waitList[] = {events, events + 1, events + 2};
    const cl_uint waitListSize = static_cast<cl_uint>(arrayCount(waitList));

    queue.queueCapabilities = CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL;
    otherQueue.queueCapabilities = CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL;
    EXPECT_FALSE(queue.validateCapabilityForOperation(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL, waitListSize, waitList, nullptr));

    queue.queueCapabilities = CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL | CL_QUEUE_CAPABILITY_CROSS_QUEUE_EVENT_WAIT_LIST_INTEL;
    otherQueue.queueCapabilities = CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL;
    EXPECT_FALSE(queue.validateCapabilityForOperation(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL, waitListSize, waitList, nullptr));

    queue.queueCapabilities = CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL;
    otherQueue.queueCapabilities = CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL | CL_QUEUE_CAPABILITY_CREATE_CROSS_QUEUE_EVENTS_INTEL;
    EXPECT_FALSE(queue.validateCapabilityForOperation(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL, waitListSize, waitList, nullptr));

    queue.queueCapabilities = CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL | CL_QUEUE_CAPABILITY_CROSS_QUEUE_EVENT_WAIT_LIST_INTEL;
    otherQueue.queueCapabilities = CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL | CL_QUEUE_CAPABILITY_CREATE_CROSS_QUEUE_EVENTS_INTEL;
    EXPECT_TRUE(queue.validateCapabilityForOperation(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL, waitListSize, waitList, nullptr));
}

TEST(CommandQueue, givenUserEventInWaitListWhenValidatingSupportThenReturnTrue) {
    MockContext context{};
    MockCommandQueue queue{context};

    MockEvent<Event> events[] = {
        {&queue, CL_COMMAND_READ_BUFFER, 0, 0},
        {&queue, CL_COMMAND_READ_BUFFER, 0, 0},
        {&queue, CL_COMMAND_READ_BUFFER, 0, 0},
    };
    MockEvent<UserEvent> userEvent{&context};
    const cl_event waitList[] = {events, events + 1, events + 2, &userEvent};
    const cl_uint waitListSize = static_cast<cl_uint>(arrayCount(waitList));

    queue.queueCapabilities = CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL |
                              CL_QUEUE_CAPABILITY_CREATE_SINGLE_QUEUE_EVENTS_INTEL |
                              CL_QUEUE_CAPABILITY_SINGLE_QUEUE_EVENT_WAIT_LIST_INTEL;
    EXPECT_TRUE(queue.validateCapabilityForOperation(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL, waitListSize, waitList, nullptr));
}

TEST(CommandQueue, givenSupportForOutEventAndOutEventIsPassedWhenValidatingSupportThenReturnSuccess) {
    MockCommandQueue queue{};
    cl_event outEvent{};

    queue.queueCapabilities = CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL;
    EXPECT_TRUE(queue.validateCapabilityForOperation(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL, 0, nullptr, nullptr));
    EXPECT_FALSE(queue.validateCapabilityForOperation(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL, 0, nullptr, &outEvent));

    queue.queueCapabilities = CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL | CL_QUEUE_CAPABILITY_CREATE_SINGLE_QUEUE_EVENTS_INTEL;
    EXPECT_TRUE(queue.validateCapabilityForOperation(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL, 0, nullptr, nullptr));
    EXPECT_TRUE(queue.validateCapabilityForOperation(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL, 0, nullptr, &outEvent));

    queue.queueCapabilities = CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL | CL_QUEUE_CAPABILITY_CREATE_CROSS_QUEUE_EVENTS_INTEL;
    EXPECT_TRUE(queue.validateCapabilityForOperation(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL, 0, nullptr, nullptr));
    EXPECT_TRUE(queue.validateCapabilityForOperation(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL, 0, nullptr, &outEvent));

    queue.queueCapabilities = CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL |
                              CL_QUEUE_CAPABILITY_CREATE_SINGLE_QUEUE_EVENTS_INTEL |
                              CL_QUEUE_CAPABILITY_CREATE_CROSS_QUEUE_EVENTS_INTEL;
    EXPECT_TRUE(queue.validateCapabilityForOperation(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL, 0, nullptr, nullptr));
    EXPECT_TRUE(queue.validateCapabilityForOperation(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL, 0, nullptr, &outEvent));
}

using KernelExecutionTypesTests = DispatchFlagsTests;
HWTEST_F(KernelExecutionTypesTests, givenConcurrentKernelWhileDoingNonBlockedEnqueueThenCorrectKernelTypeIsSetInCSR) {
    using CsrType = MockCsrHw2<FamilyType>;
    SetUpImpl<CsrType>();
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    MockKernelWithInternals mockKernelWithInternals(*device.get());
    auto pKernel = mockKernelWithInternals.mockKernel;

    pKernel->setKernelExecutionType(CL_KERNEL_EXEC_INFO_CONCURRENT_TYPE_INTEL);
    size_t gws[3] = {63, 0, 0};

    mockCmdQ->enqueueKernel(pKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    auto &mockCsr = device->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(mockCsr.lastKernelExecutionType, KernelExecutionType::Concurrent);
}

HWTEST_F(KernelExecutionTypesTests, givenKernelWithDifferentExecutionTypeWhileDoingNonBlockedEnqueueThenKernelTypeInCSRIsChanging) {
    using CsrType = MockCsrHw2<FamilyType>;
    SetUpImpl<CsrType>();
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    MockKernelWithInternals mockKernelWithInternals(*device.get());
    auto pKernel = mockKernelWithInternals.mockKernel;
    size_t gws[3] = {63, 0, 0};
    auto &mockCsr = device->getUltCommandStreamReceiver<FamilyType>();

    pKernel->setKernelExecutionType(CL_KERNEL_EXEC_INFO_CONCURRENT_TYPE_INTEL);
    mockCmdQ->enqueueKernel(pKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(mockCsr.lastKernelExecutionType, KernelExecutionType::Concurrent);

    mockCmdQ->enqueueMarkerWithWaitList(0, nullptr, nullptr);
    EXPECT_EQ(mockCsr.lastKernelExecutionType, KernelExecutionType::Concurrent);

    pKernel->setKernelExecutionType(CL_KERNEL_EXEC_INFO_DEFAULT_TYPE_INTEL);
    mockCmdQ->enqueueKernel(pKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_EQ(mockCsr.lastKernelExecutionType, KernelExecutionType::Default);
}

HWTEST_F(KernelExecutionTypesTests, givenConcurrentKernelWhileDoingBlockedEnqueueThenCorrectKernelTypeIsSetInCSR) {
    using CsrType = MockCsrHw2<FamilyType>;
    SetUpImpl<CsrType>();
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    MockKernelWithInternals mockKernelWithInternals(*device.get());
    auto pKernel = mockKernelWithInternals.mockKernel;

    pKernel->setKernelExecutionType(CL_KERNEL_EXEC_INFO_CONCURRENT_TYPE_INTEL);
    UserEvent userEvent;
    cl_event waitlist[] = {&userEvent};
    size_t gws[3] = {63, 0, 0};

    mockCmdQ->enqueueKernel(pKernel, 1, nullptr, gws, nullptr, 1, waitlist, nullptr);
    userEvent.setStatus(CL_COMPLETE);

    auto &mockCsr = device->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(mockCsr.lastKernelExecutionType, KernelExecutionType::Concurrent);
    mockCmdQ->isQueueBlocked();
}

struct CommandQueueOnSpecificEngineTests : ::testing::Test {
    static void fillProperties(cl_queue_properties *properties, cl_uint queueFamily, cl_uint queueIndex) {
        properties[0] = CL_QUEUE_FAMILY_INTEL;
        properties[1] = queueFamily;
        properties[2] = CL_QUEUE_INDEX_INTEL;
        properties[3] = queueIndex;
        properties[4] = 0;
    }

    template <typename GfxFamily, int rcsCount, int ccsCount, int bcsCount>
    class MockHwHelper : public HwHelperHw<GfxFamily> {
      public:
        const HwHelper::EngineInstancesContainer getGpgpuEngineInstances(const HardwareInfo &hwInfo) const override {
            HwHelper::EngineInstancesContainer result{};
            for (int i = 0; i < rcsCount; i++) {
                result.push_back({aub_stream::ENGINE_RCS, EngineUsage::Regular});
            }
            for (int i = 0; i < ccsCount; i++) {
                result.push_back({aub_stream::ENGINE_CCS, EngineUsage::Regular});
            }
            for (int i = 0; i < bcsCount; i++) {
                result.push_back({aub_stream::ENGINE_BCS, EngineUsage::Regular});
            }

            return result;
        }

        EngineGroupType getEngineGroupType(aub_stream::EngineType engineType, const HardwareInfo &hwInfo) const override {
            switch (engineType) {
            case aub_stream::ENGINE_RCS:
                return EngineGroupType::RenderCompute;
            case aub_stream::ENGINE_CCS:
                return EngineGroupType::Compute;
            case aub_stream::ENGINE_BCS:
                return EngineGroupType::Copy;
            default:
                UNRECOVERABLE_IF(true);
            }
        }
    };

    template <typename GfxFamily, typename HwHelperType>
    auto overrideHwHelper() {
        return RAIIHwHelperFactory<HwHelperType>{::defaultHwInfo->platform.eRenderCoreFamily};
    }
};

HWTEST_F(CommandQueueOnSpecificEngineTests, givenMultipleFamiliesWhenCreatingQueueOnSpecificEngineThenUseCorrectEngine) {
    auto raiiHwHelper = overrideHwHelper<FamilyType, MockHwHelper<FamilyType, 0, 1, 1>>();
    MockContext context{};
    cl_command_queue_properties properties[5] = {};

    fillProperties(properties, 0, 0);
    EngineControl &engineCcs = context.getDevice(0)->getEngine(aub_stream::ENGINE_CCS, EngineUsage::Regular);
    MockCommandQueue queueRcs(&context, context.getDevice(0), properties, false);
    EXPECT_EQ(&engineCcs, &queueRcs.getGpgpuEngine());
    EXPECT_FALSE(queueRcs.isCopyOnly);
    EXPECT_TRUE(queueRcs.isQueueFamilySelected());
    EXPECT_EQ(properties[1], queueRcs.getQueueFamilyIndex());
    EXPECT_EQ(properties[3], queueRcs.getQueueIndexWithinFamily());

    fillProperties(properties, 1, 0);
    EngineControl &engineBcs = context.getDevice(0)->getEngine(aub_stream::ENGINE_BCS, EngineUsage::Regular);
    MockCommandQueue queueBcs(&context, context.getDevice(0), properties, false);
    EXPECT_EQ(engineBcs.commandStreamReceiver, queueBcs.getBcsCommandStreamReceiver());
    EXPECT_TRUE(queueBcs.isCopyOnly);
    EXPECT_TRUE(queueBcs.isQueueFamilySelected());
    EXPECT_EQ(properties[1], queueBcs.getQueueFamilyIndex());
    EXPECT_EQ(properties[3], queueBcs.getQueueIndexWithinFamily());
    EXPECT_NE(nullptr, queueBcs.getTimestampPacketContainer());
}

HWTEST_F(CommandQueueOnSpecificEngineTests, givenRootDeviceAndMultipleFamiliesWhenCreatingQueueOnSpecificEngineThenUseDefaultEngine) {
    auto raiiHwHelper = overrideHwHelper<FamilyType, MockHwHelper<FamilyType, 0, 1, 1>>();
    UltClDeviceFactory deviceFactory{1, 2};
    MockContext context{deviceFactory.rootDevices[0]};
    cl_command_queue_properties properties[5] = {};

    fillProperties(properties, 0, 0);
    EngineControl &defaultEngine = context.getDevice(0)->getDefaultEngine();
    MockCommandQueue defaultQueue(&context, context.getDevice(0), properties, false);
    EXPECT_EQ(&defaultEngine, &defaultQueue.getGpgpuEngine());
    EXPECT_FALSE(defaultQueue.isCopyOnly);
    EXPECT_TRUE(defaultQueue.isQueueFamilySelected());
    EXPECT_EQ(properties[1], defaultQueue.getQueueFamilyIndex());
    EXPECT_EQ(properties[3], defaultQueue.getQueueIndexWithinFamily());
}

HWTEST_F(CommandQueueOnSpecificEngineTests, givenSubDeviceAndMultipleFamiliesWhenCreatingQueueOnSpecificEngineThenUseDefaultEngine) {
    auto raiiHwHelper = overrideHwHelper<FamilyType, MockHwHelper<FamilyType, 0, 1, 1>>();
    UltClDeviceFactory deviceFactory{1, 2};
    MockContext context{deviceFactory.subDevices[0]};
    cl_command_queue_properties properties[5] = {};

    fillProperties(properties, 0, 0);
    EngineControl &engineCcs = context.getDevice(0)->getEngine(aub_stream::ENGINE_CCS, EngineUsage::Regular);
    MockCommandQueue queueRcs(&context, context.getDevice(0), properties, false);
    EXPECT_EQ(&engineCcs, &queueRcs.getGpgpuEngine());
    EXPECT_FALSE(queueRcs.isCopyOnly);
    EXPECT_TRUE(queueRcs.isQueueFamilySelected());
    EXPECT_EQ(properties[1], queueRcs.getQueueFamilyIndex());
    EXPECT_EQ(properties[3], queueRcs.getQueueIndexWithinFamily());

    fillProperties(properties, 1, 0);
    EngineControl &engineBcs = context.getDevice(0)->getEngine(aub_stream::ENGINE_BCS, EngineUsage::Regular);
    MockCommandQueue queueBcs(&context, context.getDevice(0), properties, false);
    EXPECT_EQ(engineBcs.commandStreamReceiver, queueBcs.getBcsCommandStreamReceiver());
    EXPECT_TRUE(queueBcs.isCopyOnly);
    EXPECT_NE(nullptr, queueBcs.getTimestampPacketContainer());
    EXPECT_TRUE(queueBcs.isQueueFamilySelected());
    EXPECT_EQ(properties[1], queueBcs.getQueueFamilyIndex());
    EXPECT_EQ(properties[3], queueBcs.getQueueIndexWithinFamily());
}

HWTEST_F(CommandQueueOnSpecificEngineTests, givenBcsFamilySelectedWhenCreatingQueueOnSpecificEngineThenInitializeBcsProperly) {
    auto raiiHwHelper = overrideHwHelper<FamilyType, MockHwHelper<FamilyType, 0, 0, 1>>();
    MockContext context{};
    cl_command_queue_properties properties[5] = {};

    fillProperties(properties, 0, 0);
    EngineControl &engineBcs = context.getDevice(0)->getEngine(aub_stream::ENGINE_BCS, EngineUsage::Regular);
    MockCommandQueue queueBcs(&context, context.getDevice(0), properties, false);
    EXPECT_EQ(engineBcs.commandStreamReceiver, queueBcs.getBcsCommandStreamReceiver());
    EXPECT_TRUE(queueBcs.isCopyOnly);
    EXPECT_NE(nullptr, queueBcs.getTimestampPacketContainer());
    EXPECT_TRUE(queueBcs.isQueueFamilySelected());
    EXPECT_EQ(properties[1], queueBcs.getQueueFamilyIndex());
    EXPECT_EQ(properties[3], queueBcs.getQueueIndexWithinFamily());
}

HWTEST_F(CommandQueueOnSpecificEngineTests, givenNotInitializedCcsOsContextWhenCreatingQueueThenInitializeOsContext) {
    DebugManagerStateRestore restore{};
    DebugManager.flags.NodeOrdinal.set(static_cast<int32_t>(aub_stream::EngineType::ENGINE_RCS));
    DebugManager.flags.DeferOsContextInitialization.set(1);

    auto raiiHwHelper = overrideHwHelper<FamilyType, MockHwHelper<FamilyType, 1, 1, 1>>();
    MockContext context{};
    cl_command_queue_properties properties[5] = {};

    OsContext &osContext = *context.getDevice(0)->getEngine(aub_stream::ENGINE_CCS, EngineUsage::Regular).osContext;
    EXPECT_FALSE(osContext.isInitialized());

    fillProperties(properties, 1, 0);
    MockCommandQueueHw<FamilyType> queue(&context, context.getDevice(0), properties);
    ASSERT_EQ(&osContext, queue.gpgpuEngine->osContext);
    EXPECT_TRUE(osContext.isInitialized());
}
