/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/fixtures/memory_management_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/test_macros/test.h"
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
#include "opencl/test/unit_test/fixtures/multi_tile_fixture.h"
#include "opencl/test/unit_test/helpers/raii_hw_helper.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_event.h"
#include "opencl/test/unit_test/mocks/mock_image.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_mdi.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

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

TEST(CommandQueue, givenEnableTimestampWaitWhenCheckIsTimestampWaitEnabledThenReturnProperValue) {
    DebugManagerStateRestore restorer;
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useWaitForTimestamps = true;
    auto mockDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    MockCommandQueue cmdQ(nullptr, mockDevice.get(), 0, false);

    {
        DebugManager.flags.EnableTimestampWait.set(0);
        EXPECT_FALSE(cmdQ.isWaitForTimestampsEnabled());
    }

    {
        DebugManager.flags.EnableTimestampWait.set(1);
        EXPECT_EQ(cmdQ.isWaitForTimestampsEnabled(), cmdQ.getGpgpuCommandStreamReceiver().isUpdateTagFromWaitEnabled());
    }

    {
        DebugManager.flags.EnableTimestampWait.set(2);
        EXPECT_EQ(cmdQ.isWaitForTimestampsEnabled(), cmdQ.getGpgpuCommandStreamReceiver().isDirectSubmissionEnabled());
    }

    {
        DebugManager.flags.EnableTimestampWait.set(3);
        EXPECT_EQ(cmdQ.isWaitForTimestampsEnabled(), cmdQ.getGpgpuCommandStreamReceiver().isAnyDirectSubmissionEnabled());
    }

    {
        DebugManager.flags.EnableTimestampWait.set(4);
        EXPECT_TRUE(cmdQ.isWaitForTimestampsEnabled());
    }
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

TEST(CommandQueue, givenDeviceNotSupportingBlitOperationsWhenQueueIsCreatedThenDontRegisterAnyBcsCsrs) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = false;
    auto mockDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    MockCommandQueue cmdQ(nullptr, mockDevice.get(), 0, false);

    EXPECT_EQ(0u, cmdQ.countBcsEngines());

    auto defaultCsr = mockDevice->getDefaultEngine().commandStreamReceiver;
    EXPECT_EQ(defaultCsr, &cmdQ.getGpgpuCommandStreamReceiver());
}

TEST(CommandQueue, givenDeviceWithSubDevicesSupportingBlitOperationsWhenQueueIsCreatedThenBcsIsTakenFromFirstSubDevice) {
    DebugManagerStateRestore restorer;
    VariableBackup<bool> mockDeviceFlagBackup{&MockDevice::createSingleDevice, false};
    DebugManager.flags.CreateMultipleSubDevices.set(2);
    DebugManager.flags.EnableBlitterForEnqueueOperations.set(1);
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    REQUIRE_FULL_BLITTER_OR_SKIP(&hwInfo);

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    EXPECT_EQ(2u, device->getNumGenericSubDevices());
    std::unique_ptr<OsContext> bcsOsContext;

    auto subDevice = device->getSubDevice(0);
    auto &bcsEngine = subDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::Regular);

    MockCommandQueue cmdQ(nullptr, device.get(), 0, false);

    EXPECT_NE(nullptr, cmdQ.getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS));
    EXPECT_EQ(bcsEngine.commandStreamReceiver, cmdQ.getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS));
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

HWTEST_F(CommandQueueCommandStreamTest, WhenCheckIsTextureCacheFlushNeededThenReturnProperValue) {
    MockContext context;
    auto mockDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockCommandQueue cmdQ(&context, mockDevice.get(), 0, false);
    auto &commandStreamReceiver = mockDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_FALSE(cmdQ.isTextureCacheFlushNeeded(CL_COMMAND_COPY_BUFFER_RECT));

    for (auto i = CL_COMMAND_NDRANGE_KERNEL; i < CL_COMMAND_RELEASE_GL_OBJECTS; i++) {
        if (i == CL_COMMAND_COPY_IMAGE) {
            commandStreamReceiver.directSubmissionAvailable = true;
            EXPECT_TRUE(cmdQ.isTextureCacheFlushNeeded(i));
            commandStreamReceiver.directSubmissionAvailable = false;
            EXPECT_FALSE(cmdQ.isTextureCacheFlushNeeded(i));
        } else {
            commandStreamReceiver.directSubmissionAvailable = true;
            EXPECT_FALSE(cmdQ.isTextureCacheFlushNeeded(i));
            commandStreamReceiver.directSubmissionAvailable = false;
            EXPECT_FALSE(cmdQ.isTextureCacheFlushNeeded(i));
        }
    }
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
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), requiredSize, AllocationType::COMMAND_BUFFER, pDevice->getDeviceBitfield()});
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

    EXPECT_EQ(AllocationType::COMMAND_BUFFER, commandStreamAllocation->getAllocationType());
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
    if (this->GetParam() == IndirectHeap::Type::INDIRECT_OBJECT && commandStreamReceiver.canUse4GbHeaps) {
        EXPECT_TRUE(indirectHeap.getGraphicsAllocation()->is32BitAllocation());
    } else {
        EXPECT_FALSE(indirectHeap.getGraphicsAllocation()->is32BitAllocation());
    }
}

HWTEST_P(CommandQueueIndirectHeapTest, GivenIndirectHeapWhenGettingAvailableSpaceThenCorrectSizeIsReturned) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(context.get(), pClDevice, props, false);

    auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), sizeof(uint32_t));
    if (this->GetParam() == IndirectHeap::Type::SURFACE_STATE) {
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
    if (this->GetParam() == IndirectHeap::Type::SURFACE_STATE) {
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
    auto allocationType = AllocationType::LINEAR_STREAM;
    if (this->GetParam() == IndirectHeap::Type::INDIRECT_OBJECT && commandStreamReceiver.canUse4GbHeaps) {
        allocationType = AllocationType::INTERNAL_HEAP;
    }
    allocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), allocationSize, allocationType, pDevice->getDeviceBitfield()});
    if (this->GetParam() == IndirectHeap::Type::SURFACE_STATE) {
        allocation->setSize(commandStreamReceiver.defaultSshSize * 2);
    }

    commandStreamReceiver.getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION);

    EXPECT_FALSE(commandStreamReceiver.getAllocationsForReuse().peekIsEmpty());
    EXPECT_TRUE(commandStreamReceiver.getAllocationsForReuse().peekContains(*allocation));

    const auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), 100);

    EXPECT_EQ(indirectHeap.getGraphicsAllocation(), allocation);

    // if we obtain heap from reusable pool, we need to keep the size of allocation
    // surface state heap is an exception, it is capped at (max_ssh_size_for_HW - page_size)
    if (this->GetParam() == IndirectHeap::Type::SURFACE_STATE) {
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

    bool requireInternalHeap = IndirectHeap::Type::INDIRECT_OBJECT == heapType && commandStreamReceiver.canUse4GbHeaps;
    const auto &indirectHeap = cmdQ.getIndirectHeap(heapType, 100);
    auto indirectHeapAllocation = indirectHeap.getGraphicsAllocation();
    ASSERT_NE(nullptr, indirectHeapAllocation);
    auto expectedAllocationType = AllocationType::LINEAR_STREAM;
    if (requireInternalHeap) {
        expectedAllocationType = AllocationType::INTERNAL_HEAP;
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

TEST_F(CommandQueueIndirectHeapTest, givenForceDefaultHeapSizeWhenGetHeapMemoryIsCalledThenHeapIsCreatedWithProperSize) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ForceDefaultHeapSize.set(64 * MemoryConstants::kiloByte);

    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(context.get(), pClDevice, props, false);

    IndirectHeap *indirectHeap = nullptr;
    cmdQ.allocateHeapMemory(IndirectHeap::Type::INDIRECT_OBJECT, 100, indirectHeap);
    EXPECT_NE(nullptr, indirectHeap);
    EXPECT_NE(nullptr, indirectHeap->getGraphicsAllocation());
    EXPECT_EQ(indirectHeap->getAvailableSpace(), 64 * MemoryConstants::megaByte);

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
        IndirectHeap::Type::DYNAMIC_STATE,
        IndirectHeap::Type::INDIRECT_OBJECT,
        IndirectHeap::Type::SURFACE_STATE));

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

HWTEST_F(CommandQueueTests, givenEngineUsageHintSetWithInvalidValueWhenCreatingCommandQueueThenReturnSuccess) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EngineUsageHint.set(static_cast<int32_t>(EngineUsage::EngineUsageCount));

    auto pDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    MockContext context(pDevice.get());

    cl_int retVal = CL_SUCCESS;
    cl_queue_properties propertiesCooperativeQueue[] = {CL_QUEUE_FAMILY_INTEL, 0, CL_QUEUE_INDEX_INTEL, 0, 0};

    auto pCmdQ = CommandQueue::create(
        &context,
        pDevice.get(),
        propertiesCooperativeQueue,
        false,
        retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, pCmdQ);
    EXPECT_EQ(EngineUsage::Regular, pCmdQ->getGpgpuEngine().getEngineUsage());
    delete pCmdQ;
}

struct WaitForQueueCompletionTests : public ::testing::Test {
    template <typename Family>
    struct MyCmdQueue : public CommandQueueHw<Family> {
        MyCmdQueue(Context *context, ClDevice *device) : CommandQueueHw<Family>(context, device, nullptr, false){};
        WaitStatus waitUntilComplete(uint32_t gpgpuTaskCountToWait, Range<CopyEngineState> copyEnginesToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, bool cleanTemporaryAllocationList, bool skipWait) override {
            requestedUseQuickKmdSleep = useQuickKmdSleep;
            waitUntilCompleteCounter++;

            return WaitStatus::Ready;
        }
        bool isQueueBlocked() override {
            return false;
        }
        bool requestedUseQuickKmdSleep = false;
        uint32_t waitUntilCompleteCounter = 0;
    };

    void SetUp() override {
        device = std::make_unique<MockClDevice>(MockClDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
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

template <class GfxFamily>
class CommandStreamReceiverHwMock : public CommandStreamReceiverHw<GfxFamily> {
  public:
    CommandStreamReceiverHwMock(ExecutionEnvironment &executionEnvironment,
                                uint32_t rootDeviceIndex,
                                const DeviceBitfield deviceBitfield)
        : CommandStreamReceiverHw<GfxFamily>(executionEnvironment, rootDeviceIndex, deviceBitfield) {}

    WaitStatus waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, QueueThrottle throttle) override {
        waitForTaskCountWithKmdNotifyFallbackCounter++;
        return waitForTaskCountWithKmdNotifyFallbackReturnValue;
    }

    WaitStatus waitForTaskCount(uint32_t requiredTaskCount) override {
        waitForTaskCountCalledCounter++;
        return waitForTaskCountReturnValue;
    }

    WaitStatus waitForTaskCountAndCleanTemporaryAllocationList(uint32_t requiredTaskCount) override {
        waitForTaskCountAndCleanTemporaryAllocationListCalledCounter++;
        return waitForTaskCountAndCleanTemporaryAllocationListReturnValue;
    }

    int waitForTaskCountCalledCounter{0};
    int waitForTaskCountWithKmdNotifyFallbackCounter{0};
    int waitForTaskCountAndCleanTemporaryAllocationListCalledCounter{0};

    WaitStatus waitForTaskCountReturnValue{WaitStatus::Ready};
    WaitStatus waitForTaskCountWithKmdNotifyFallbackReturnValue{WaitStatus::Ready};
    WaitStatus waitForTaskCountAndCleanTemporaryAllocationListReturnValue{WaitStatus::Ready};
};

struct WaitUntilCompletionTests : public ::testing::Test {
    template <typename Family>
    struct MyCmdQueue : public CommandQueueHw<Family> {
      public:
        using CommandQueue::gpgpuEngine;

        MyCmdQueue(Context *context, ClDevice *device) : CommandQueueHw<Family>(context, device, nullptr, false){};

        CommandStreamReceiver *getBcsCommandStreamReceiver(aub_stream::EngineType bcsEngineType) const override {
            return bcsCsrToReturn;
        }

        CommandStreamReceiver *bcsCsrToReturn{nullptr};
    };

    void SetUp() override {
        device = std::make_unique<MockClDevice>(MockClDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
        context.reset(new MockContext(device.get()));
    }

    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockContext> context;
};

HWTEST_F(WaitUntilCompletionTests, givenCleanTemporaryAllocationListEqualsFalseWhenWaitingUntilCompleteThenWaitForTaskCountIsCalledAndItsReturnValueIsPropagated) {
    std::unique_ptr<CommandStreamReceiverHwMock<FamilyType>> cmdStream(new CommandStreamReceiverHwMock<FamilyType>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield()));
    cmdStream->initializeTagAllocation();
    cmdStream->waitForTaskCountReturnValue = WaitStatus::Ready;

    std::unique_ptr<MyCmdQueue<FamilyType>> cmdQ(new MyCmdQueue<FamilyType>(context.get(), device.get()));
    CommandStreamReceiver *oldCommandStreamReceiver = cmdQ->gpgpuEngine->commandStreamReceiver;
    cmdQ->gpgpuEngine->commandStreamReceiver = cmdStream.get();

    constexpr uint32_t taskCount = 0u;
    constexpr bool cleanTemporaryAllocationList = false;
    StackVec<CopyEngineState, bcsInfoMaskSize> activeBcsStates{};

    const auto waitStatus = cmdQ->waitUntilComplete(taskCount, activeBcsStates, cmdQ->flushStamp->peekStamp(), false, cleanTemporaryAllocationList, false);
    EXPECT_EQ(WaitStatus::Ready, waitStatus);
    EXPECT_EQ(1, cmdStream->waitForTaskCountCalledCounter);

    cmdQ->gpgpuEngine->commandStreamReceiver = oldCommandStreamReceiver;
}

HWTEST_F(WaitUntilCompletionTests, givenGpuHangAndCleanTemporaryAllocationListEqualsTrueWhenWaitingUntilCompleteThenWaitForTaskCountAndCleanAllocationIsCalledAndGpuHangIsReturned) {
    std::unique_ptr<CommandStreamReceiverHwMock<FamilyType>> cmdStream(new CommandStreamReceiverHwMock<FamilyType>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield()));
    cmdStream->initializeTagAllocation();
    cmdStream->waitForTaskCountAndCleanTemporaryAllocationListReturnValue = WaitStatus::GpuHang;

    std::unique_ptr<MyCmdQueue<FamilyType>> cmdQ(new MyCmdQueue<FamilyType>(context.get(), device.get()));
    CommandStreamReceiver *oldCommandStreamReceiver = cmdQ->gpgpuEngine->commandStreamReceiver;
    cmdQ->gpgpuEngine->commandStreamReceiver = cmdStream.get();

    constexpr uint32_t taskCount = 0u;
    constexpr bool cleanTemporaryAllocationList = true;
    StackVec<CopyEngineState, bcsInfoMaskSize> activeBcsStates{};

    const auto waitStatus = cmdQ->waitUntilComplete(taskCount, activeBcsStates, cmdQ->flushStamp->peekStamp(), false, cleanTemporaryAllocationList, false);
    EXPECT_EQ(WaitStatus::GpuHang, waitStatus);
    EXPECT_EQ(1, cmdStream->waitForTaskCountAndCleanTemporaryAllocationListCalledCounter);

    cmdQ->gpgpuEngine->commandStreamReceiver = oldCommandStreamReceiver;
}

HWTEST_F(WaitUntilCompletionTests, givenEmptyBcsStatesAndSkipWaitEqualsTrueWhenWaitingUntilCompleteThenWaitForTaskCountWithKmdNotifyFallbackIsNotCalled) {
    std::unique_ptr<CommandStreamReceiverHwMock<FamilyType>> cmdStream(new CommandStreamReceiverHwMock<FamilyType>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield()));
    cmdStream->initializeTagAllocation();

    std::unique_ptr<MyCmdQueue<FamilyType>> cmdQ(new MyCmdQueue<FamilyType>(context.get(), device.get()));
    CommandStreamReceiver *oldCommandStreamReceiver = cmdQ->gpgpuEngine->commandStreamReceiver;
    cmdQ->gpgpuEngine->commandStreamReceiver = cmdStream.get();

    constexpr uint32_t taskCount = 0u;
    constexpr bool skipWait = true;
    StackVec<CopyEngineState, bcsInfoMaskSize> activeBcsStates{};

    cmdQ->waitUntilComplete(taskCount, activeBcsStates, cmdQ->flushStamp->peekStamp(), false, false, skipWait);
    EXPECT_EQ(0, cmdStream->waitForTaskCountWithKmdNotifyFallbackCounter);

    cmdQ->gpgpuEngine->commandStreamReceiver = oldCommandStreamReceiver;
}

HWTEST_F(WaitUntilCompletionTests, givenGpuHangAndSkipWaitEqualsFalseWhenWaitingUntilCompleteThenOnlyWaitForTaskCountWithKmdNotifyFallbackIsCalledAndGpuHangIsReturned) {
    std::unique_ptr<CommandStreamReceiverHwMock<FamilyType>> cmdStream(new CommandStreamReceiverHwMock<FamilyType>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield()));
    cmdStream->initializeTagAllocation();
    cmdStream->waitForTaskCountWithKmdNotifyFallbackReturnValue = WaitStatus::GpuHang;

    std::unique_ptr<MyCmdQueue<FamilyType>> cmdQ(new MyCmdQueue<FamilyType>(context.get(), device.get()));
    CommandStreamReceiver *oldCommandStreamReceiver = cmdQ->gpgpuEngine->commandStreamReceiver;
    cmdQ->gpgpuEngine->commandStreamReceiver = cmdStream.get();

    constexpr uint32_t taskCount = 0u;
    constexpr bool skipWait = false;
    StackVec<CopyEngineState, bcsInfoMaskSize> activeBcsStates{};

    const auto waitStatus = cmdQ->waitUntilComplete(taskCount, activeBcsStates, cmdQ->flushStamp->peekStamp(), false, false, skipWait);
    EXPECT_EQ(WaitStatus::GpuHang, waitStatus);

    EXPECT_EQ(0, cmdStream->waitForTaskCountCalledCounter);
    EXPECT_EQ(1, cmdStream->waitForTaskCountWithKmdNotifyFallbackCounter);
    EXPECT_EQ(0, cmdStream->waitForTaskCountAndCleanTemporaryAllocationListCalledCounter);

    cmdQ->gpgpuEngine->commandStreamReceiver = oldCommandStreamReceiver;
}

HWTEST_F(WaitUntilCompletionTests, givenGpuHangOnBcsCsrWhenWaitingUntilCompleteThenOnlyWaitForTaskCountWithKmdNotifyFallbackIsCalledOnBcsCsrAndGpuHangIsReturned) {
    std::unique_ptr<CommandStreamReceiverHwMock<FamilyType>> gpgpuCmdStream(new CommandStreamReceiverHwMock<FamilyType>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield()));
    gpgpuCmdStream->initializeTagAllocation();
    gpgpuCmdStream->waitForTaskCountWithKmdNotifyFallbackReturnValue = WaitStatus::Ready;

    std::unique_ptr<CommandStreamReceiverHwMock<FamilyType>> bcsCmdStream(new CommandStreamReceiverHwMock<FamilyType>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield()));
    bcsCmdStream->initializeTagAllocation();
    bcsCmdStream->waitForTaskCountWithKmdNotifyFallbackReturnValue = WaitStatus::GpuHang;

    std::unique_ptr<MyCmdQueue<FamilyType>> cmdQ(new MyCmdQueue<FamilyType>(context.get(), device.get()));
    CommandStreamReceiver *oldCommandStreamReceiver = cmdQ->gpgpuEngine->commandStreamReceiver;
    cmdQ->gpgpuEngine->commandStreamReceiver = gpgpuCmdStream.get();
    cmdQ->bcsCsrToReturn = bcsCmdStream.get();

    constexpr uint32_t taskCount = 0u;
    constexpr bool skipWait = false;
    StackVec<CopyEngineState, bcsInfoMaskSize> activeBcsStates{CopyEngineState{}};

    const auto waitStatus = cmdQ->waitUntilComplete(taskCount, activeBcsStates, cmdQ->flushStamp->peekStamp(), false, false, skipWait);
    EXPECT_EQ(WaitStatus::GpuHang, waitStatus);

    EXPECT_EQ(0, gpgpuCmdStream->waitForTaskCountCalledCounter);
    EXPECT_EQ(1, gpgpuCmdStream->waitForTaskCountWithKmdNotifyFallbackCounter);
    EXPECT_EQ(0, gpgpuCmdStream->waitForTaskCountAndCleanTemporaryAllocationListCalledCounter);

    EXPECT_EQ(0, bcsCmdStream->waitForTaskCountCalledCounter);
    EXPECT_EQ(1, bcsCmdStream->waitForTaskCountWithKmdNotifyFallbackCounter);
    EXPECT_EQ(0, bcsCmdStream->waitForTaskCountAndCleanTemporaryAllocationListCalledCounter);

    cmdQ->gpgpuEngine->commandStreamReceiver = oldCommandStreamReceiver;
}

HWTEST_F(WaitUntilCompletionTests, givenGpuHangOnBcsCsrWhenWaitingUntilCompleteThenWaitForTaskCountAndCleanTemporaryAllocationListIsCalledOnBcsCsrAndGpuHangIsReturned) {
    std::unique_ptr<CommandStreamReceiverHwMock<FamilyType>> gpgpuCmdStream(new CommandStreamReceiverHwMock<FamilyType>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield()));
    gpgpuCmdStream->initializeTagAllocation();
    gpgpuCmdStream->waitForTaskCountWithKmdNotifyFallbackReturnValue = WaitStatus::Ready;

    std::unique_ptr<CommandStreamReceiverHwMock<FamilyType>> bcsCmdStream(new CommandStreamReceiverHwMock<FamilyType>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield()));
    bcsCmdStream->initializeTagAllocation();
    bcsCmdStream->waitForTaskCountWithKmdNotifyFallbackReturnValue = WaitStatus::Ready;
    bcsCmdStream->waitForTaskCountAndCleanTemporaryAllocationListReturnValue = WaitStatus::GpuHang;

    std::unique_ptr<MyCmdQueue<FamilyType>> cmdQ(new MyCmdQueue<FamilyType>(context.get(), device.get()));
    CommandStreamReceiver *oldCommandStreamReceiver = cmdQ->gpgpuEngine->commandStreamReceiver;
    cmdQ->gpgpuEngine->commandStreamReceiver = gpgpuCmdStream.get();
    cmdQ->bcsCsrToReturn = bcsCmdStream.get();

    constexpr uint32_t taskCount = 0u;
    constexpr bool skipWait = false;
    StackVec<CopyEngineState, bcsInfoMaskSize> activeBcsStates{CopyEngineState{}};

    const auto waitStatus = cmdQ->waitUntilComplete(taskCount, activeBcsStates, cmdQ->flushStamp->peekStamp(), false, false, skipWait);
    EXPECT_EQ(WaitStatus::GpuHang, waitStatus);

    EXPECT_EQ(0, gpgpuCmdStream->waitForTaskCountCalledCounter);
    EXPECT_EQ(1, gpgpuCmdStream->waitForTaskCountWithKmdNotifyFallbackCounter);
    EXPECT_EQ(0, gpgpuCmdStream->waitForTaskCountAndCleanTemporaryAllocationListCalledCounter);

    EXPECT_EQ(0, bcsCmdStream->waitForTaskCountCalledCounter);
    EXPECT_EQ(1, bcsCmdStream->waitForTaskCountWithKmdNotifyFallbackCounter);
    EXPECT_EQ(1, bcsCmdStream->waitForTaskCountAndCleanTemporaryAllocationListCalledCounter);

    cmdQ->gpgpuEngine->commandStreamReceiver = oldCommandStreamReceiver;
}

HWTEST_F(WaitUntilCompletionTests, givenSuccessOnBcsCsrWhenWaitingUntilCompleteThenGpgpuCsrWaitStatusIsReturned) {
    std::unique_ptr<CommandStreamReceiverHwMock<FamilyType>> gpgpuCmdStream(new CommandStreamReceiverHwMock<FamilyType>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield()));
    gpgpuCmdStream->initializeTagAllocation();
    gpgpuCmdStream->waitForTaskCountWithKmdNotifyFallbackReturnValue = WaitStatus::Ready;
    gpgpuCmdStream->waitForTaskCountReturnValue = WaitStatus::Ready;

    std::unique_ptr<CommandStreamReceiverHwMock<FamilyType>> bcsCmdStream(new CommandStreamReceiverHwMock<FamilyType>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield()));
    bcsCmdStream->initializeTagAllocation();
    bcsCmdStream->waitForTaskCountWithKmdNotifyFallbackReturnValue = WaitStatus::Ready;
    bcsCmdStream->waitForTaskCountAndCleanTemporaryAllocationListReturnValue = WaitStatus::Ready;

    std::unique_ptr<MyCmdQueue<FamilyType>> cmdQ(new MyCmdQueue<FamilyType>(context.get(), device.get()));
    CommandStreamReceiver *oldCommandStreamReceiver = cmdQ->gpgpuEngine->commandStreamReceiver;
    cmdQ->gpgpuEngine->commandStreamReceiver = gpgpuCmdStream.get();
    cmdQ->bcsCsrToReturn = bcsCmdStream.get();

    constexpr uint32_t taskCount = 0u;
    constexpr bool skipWait = false;
    StackVec<CopyEngineState, bcsInfoMaskSize> activeBcsStates{CopyEngineState{}};

    const auto waitStatus = cmdQ->waitUntilComplete(taskCount, activeBcsStates, cmdQ->flushStamp->peekStamp(), false, false, skipWait);
    EXPECT_EQ(WaitStatus::Ready, waitStatus);

    EXPECT_EQ(1, gpgpuCmdStream->waitForTaskCountCalledCounter);
    EXPECT_EQ(1, gpgpuCmdStream->waitForTaskCountWithKmdNotifyFallbackCounter);
    EXPECT_EQ(0, gpgpuCmdStream->waitForTaskCountAndCleanTemporaryAllocationListCalledCounter);

    EXPECT_EQ(0, bcsCmdStream->waitForTaskCountCalledCounter);
    EXPECT_EQ(1, bcsCmdStream->waitForTaskCountWithKmdNotifyFallbackCounter);
    EXPECT_EQ(1, bcsCmdStream->waitForTaskCountAndCleanTemporaryAllocationListCalledCounter);

    cmdQ->gpgpuEngine->commandStreamReceiver = oldCommandStreamReceiver;
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

    auto &hwInfo = *NEO::defaultHwInfo.get();
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    cmdQ.getGpgpuCommandStreamReceiver().allocateDebugSurface(hwHelper.getSipKernelMaxDbgSurfaceSize(hwInfo));
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
    auto hwInfo = *NEO::defaultHwInfo.get();
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    commandStreamReceiver.allocateDebugSurface(hwHelper.getSipKernelMaxDbgSurfaceSize(hwInfo));
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

TEST(CommandQueue, givenBlitterOperationsSupportedWhenCreatingQueueThenTimestampPacketIsCreated) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableTimestampPacket.set(0);

    MockContext context{};
    HardwareInfo *hwInfo = context.getDevice(0)->getRootDeviceEnvironment().getMutableHardwareInfo();
    if (!HwInfoConfig::get(defaultHwInfo->platform.eProductFamily)->isBlitterFullySupported(*defaultHwInfo.get())) {
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
    if (queue.countBcsEngines() == 0) {
        queue.bcsEngines[0] = &context.getDevice(0)->getDefaultEngine();
    }
    hwInfo->capabilityTable.blitterOperationsSupported = false;

    MultiGraphicsAllocation multiAlloc{1};
    MockGraphicsAllocation alloc{};
    multiAlloc.addAllocation(&alloc);
    alloc.memoryPool = MemoryPool::System4KBPages;
    CsrSelectionArgs selectionArgs{CL_COMMAND_READ_BUFFER, &multiAlloc, &multiAlloc, 0u, nullptr};

    queue.isCopyOnly = false;
    EXPECT_EQ(queue.getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled(),
              queue.blitEnqueueAllowed(selectionArgs));

    queue.isCopyOnly = true;
    EXPECT_TRUE(queue.blitEnqueueAllowed(selectionArgs));
}

TEST(CommandQueue, givenSimpleClCommandWhenCallingBlitEnqueueAllowedThenReturnCorrectValue) {
    MockContext context{};

    MockCommandQueue queue(&context, context.getDevice(0), 0, false);
    if (queue.countBcsEngines() == 0) {
        queue.bcsEngines[0] = &context.getDevice(0)->getDefaultEngine();
    }

    MultiGraphicsAllocation multiAlloc{1};
    MockGraphicsAllocation alloc{};
    multiAlloc.addAllocation(&alloc);
    alloc.memoryPool = MemoryPool::System4KBPages;

    for (cl_command_type cmdType : {CL_COMMAND_READ_BUFFER, CL_COMMAND_READ_BUFFER_RECT,
                                    CL_COMMAND_WRITE_BUFFER, CL_COMMAND_WRITE_BUFFER_RECT,
                                    CL_COMMAND_COPY_BUFFER, CL_COMMAND_COPY_BUFFER_RECT,
                                    CL_COMMAND_SVM_MAP, CL_COMMAND_SVM_UNMAP,
                                    CL_COMMAND_SVM_MEMCPY}) {
        CsrSelectionArgs args{cmdType, &multiAlloc, &multiAlloc, 0u, nullptr};

        bool expectedValue = queue.getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled();
        if (cmdType == CL_COMMAND_COPY_IMAGE_TO_BUFFER) {
            expectedValue = false;
        }

        EXPECT_EQ(expectedValue, queue.blitEnqueueAllowed(args));
    }
}

TEST(CommandQueue, givenImageTransferClCommandWhenCallingBlitEnqueueAllowedThenReturnCorrectValue) {
    DebugManagerStateRestore restore{};
    DebugManager.flags.EnableBlitterForEnqueueOperations.set(1);
    DebugManager.flags.EnableBlitterForEnqueueImageOperations.set(1);

    MockContext context{};
    MockCommandQueue queue(&context, context.getDevice(0), 0, false);
    if (queue.countBcsEngines() == 0) {
        queue.bcsEngines[0] = &context.getDevice(0)->getDefaultEngine();
    }

    MockImageBase image{};
    auto alloc = static_cast<MockGraphicsAllocation *>(image.getGraphicsAllocation(0));
    alloc->memoryPool = MemoryPool::System4KBPages;

    size_t origin[3] = {0, 0, 0};
    size_t region[3] = {1, 1, 1};
    {
        CsrSelectionArgs args{CL_COMMAND_READ_IMAGE, &image, {}, 0u, region, origin, nullptr};
        EXPECT_TRUE(queue.blitEnqueueAllowed(args));
    }
    {
        CsrSelectionArgs args{CL_COMMAND_WRITE_IMAGE, {}, &image, 0u, region, nullptr, origin};
        EXPECT_TRUE(queue.blitEnqueueAllowed(args));
    }
    {
        CsrSelectionArgs args{CL_COMMAND_COPY_IMAGE, &image, &image, 0u, region, origin, origin};
        EXPECT_TRUE(queue.blitEnqueueAllowed(args));
    }
    {
        MockImageBase dstImage{};
        dstImage.imageDesc.num_mip_levels = 2;
        auto dstAlloc = static_cast<MockGraphicsAllocation *>(dstImage.getGraphicsAllocation(0));
        dstAlloc->memoryPool = MemoryPool::System4KBPages;

        CsrSelectionArgs args{CL_COMMAND_COPY_IMAGE, &image, &dstImage, 0u, region, origin, origin};
        EXPECT_FALSE(queue.blitEnqueueAllowed(args));
    }
}

TEST(CommandQueue, givenImageToBufferClCommandWhenCallingBlitEnqueueAllowedThenReturnCorrectValue) {
    MockContext context{};
    MockCommandQueue queue(&context, context.getDevice(0), 0, false);
    if (queue.countBcsEngines() == 0) {
        queue.bcsEngines[0] = &context.getDevice(0)->getDefaultEngine();
    }

    MultiGraphicsAllocation multiAlloc{1};
    MockGraphicsAllocation alloc{};
    multiAlloc.addAllocation(&alloc);
    alloc.memoryPool = MemoryPool::System4KBPages;

    CsrSelectionArgs args{CL_COMMAND_COPY_IMAGE_TO_BUFFER, &multiAlloc, &multiAlloc, 0u, nullptr};
    EXPECT_FALSE(queue.blitEnqueueAllowed(args));
}

template <bool blitter, bool selectBlitterWithQueueFamilies>
struct CsrSelectionCommandQueueTests : ::testing::Test {
    void SetUp() override {
        HardwareInfo hwInfo = *::defaultHwInfo;
        hwInfo.capabilityTable.blitterOperationsSupported = blitter;
        if (blitter) {
            REQUIRE_FULL_BLITTER_OR_SKIP(&hwInfo);
        }

        device = MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo);
        clDevice = std::make_unique<MockClDevice>(device);
        context = std::make_unique<MockContext>(clDevice.get());

        cl_command_queue_properties queueProperties[5] = {};
        if (selectBlitterWithQueueFamilies) {
            queueProperties[0] = CL_QUEUE_FAMILY_INTEL;
            queueProperties[1] = device->getEngineGroupIndexFromEngineGroupType(EngineGroupType::Copy);
            queueProperties[2] = CL_QUEUE_INDEX_INTEL;
            queueProperties[3] = 0;
        }

        queue = std::make_unique<MockCommandQueue>(context.get(), clDevice.get(), queueProperties, false);
    }

    MockDevice *device;
    std::unique_ptr<MockClDevice> clDevice;
    std::unique_ptr<MockContext> context;
    std::unique_ptr<MockCommandQueue> queue;
};

using CsrSelectionCommandQueueWithoutBlitterTests = CsrSelectionCommandQueueTests<false, false>;
using CsrSelectionCommandQueueWithBlitterTests = CsrSelectionCommandQueueTests<true, false>;
using CsrSelectionCommandQueueWithQueueFamiliesBlitterTests = CsrSelectionCommandQueueTests<true, true>;

TEST_F(CsrSelectionCommandQueueWithoutBlitterTests, givenBlitterNotPresentWhenSelectingBlitterThenReturnGpgpuCsr) {
    DebugManagerStateRestore restore{};
    DebugManager.flags.EnableBlitterForEnqueueOperations.set(1);

    BuiltinOpParams builtinOpParams{};
    MockGraphicsAllocation srcGraphicsAllocation{};
    MockGraphicsAllocation dstGraphicsAllocation{};
    MockBuffer srcMemObj{srcGraphicsAllocation};
    MockBuffer dstMemObj{dstGraphicsAllocation};
    builtinOpParams.srcMemObj = &srcMemObj;
    builtinOpParams.dstMemObj = &dstMemObj;

    {
        srcGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        dstGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        EXPECT_EQ(&queue->getGpgpuCommandStreamReceiver(), &queue->selectCsrForBuiltinOperation(args));
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        dstGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        EXPECT_EQ(&queue->getGpgpuCommandStreamReceiver(), &queue->selectCsrForBuiltinOperation(args));
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        dstGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        EXPECT_EQ(&queue->getGpgpuCommandStreamReceiver(), &queue->selectCsrForBuiltinOperation(args));
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        dstGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        EXPECT_EQ(&queue->getGpgpuCommandStreamReceiver(), &queue->selectCsrForBuiltinOperation(args));
    }
}

TEST_F(CsrSelectionCommandQueueWithBlitterTests, givenBlitterPresentButDisabledWithDebugFlagWhenSelectingBlitterThenReturnGpgpuCsr) {
    DebugManagerStateRestore restore{};
    DebugManager.flags.EnableBlitterForEnqueueOperations.set(0);

    BuiltinOpParams builtinOpParams{};
    MockGraphicsAllocation srcGraphicsAllocation{};
    MockGraphicsAllocation dstGraphicsAllocation{};
    MockBuffer srcMemObj{srcGraphicsAllocation};
    MockBuffer dstMemObj{dstGraphicsAllocation};
    builtinOpParams.srcMemObj = &srcMemObj;
    builtinOpParams.dstMemObj = &dstMemObj;

    {
        srcGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        dstGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        EXPECT_EQ(&queue->getGpgpuCommandStreamReceiver(), &queue->selectCsrForBuiltinOperation(args));
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        dstGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        EXPECT_EQ(&queue->getGpgpuCommandStreamReceiver(), &queue->selectCsrForBuiltinOperation(args));
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        dstGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        EXPECT_EQ(&queue->getGpgpuCommandStreamReceiver(), &queue->selectCsrForBuiltinOperation(args));
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        dstGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        EXPECT_EQ(&queue->getGpgpuCommandStreamReceiver(), &queue->selectCsrForBuiltinOperation(args));
    }
}

TEST_F(CsrSelectionCommandQueueWithBlitterTests, givenBlitterPresentAndLocalToLocalCopyBufferCommandWhenSelectingBlitterThenReturnValueBasedOnDebugFlagAndHwPreference) {
    DebugManagerStateRestore restore{};
    DebugManager.flags.EnableBlitterForEnqueueOperations.set(1);

    const bool hwPreference = ClHwHelper::get(::defaultHwInfo->platform.eRenderCoreFamily).preferBlitterForLocalToLocalTransfers();
    const auto &hwPreferenceCsr = hwPreference ? *queue->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS) : queue->getGpgpuCommandStreamReceiver();

    BuiltinOpParams builtinOpParams{};
    MockGraphicsAllocation srcGraphicsAllocation{};
    MockGraphicsAllocation dstGraphicsAllocation{};
    MockBuffer srcMemObj{srcGraphicsAllocation};
    MockBuffer dstMemObj{dstGraphicsAllocation};
    builtinOpParams.srcMemObj = &srcMemObj;
    builtinOpParams.dstMemObj = &dstMemObj;

    srcGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
    dstGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
    CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};

    DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(-1);
    EXPECT_EQ(&hwPreferenceCsr, &queue->selectCsrForBuiltinOperation(args));
    DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(0);
    EXPECT_EQ(&queue->getGpgpuCommandStreamReceiver(), &queue->selectCsrForBuiltinOperation(args));
    DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(1);
    EXPECT_EQ(queue->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS), &queue->selectCsrForBuiltinOperation(args));
}

TEST_F(CsrSelectionCommandQueueWithBlitterTests, givenBlitterPresentAndNotLocalToLocalCopyBufferCommandWhenSelectingCsrThenUseBcsRegardlessOfDebugFlag) {
    DebugManagerStateRestore restore{};
    DebugManager.flags.EnableBlitterForEnqueueOperations.set(1);

    const auto &bcsCsr = *queue->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS);

    BuiltinOpParams builtinOpParams{};
    MockGraphicsAllocation srcGraphicsAllocation{};
    MockGraphicsAllocation dstGraphicsAllocation{};
    MockBuffer srcMemObj{srcGraphicsAllocation};
    MockBuffer dstMemObj{dstGraphicsAllocation};
    builtinOpParams.srcMemObj = &srcMemObj;
    builtinOpParams.dstMemObj = &dstMemObj;

    {
        srcGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        dstGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(-1);
        EXPECT_EQ(&bcsCsr, &queue->selectCsrForBuiltinOperation(args));
        DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(0);
        EXPECT_EQ(&bcsCsr, &queue->selectCsrForBuiltinOperation(args));
        DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(1);
        EXPECT_EQ(&bcsCsr, &queue->selectCsrForBuiltinOperation(args));
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        dstGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(-1);
        EXPECT_EQ(&bcsCsr, &queue->selectCsrForBuiltinOperation(args));
        DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(0);
        EXPECT_EQ(&bcsCsr, &queue->selectCsrForBuiltinOperation(args));
        DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(1);
        EXPECT_EQ(&bcsCsr, &queue->selectCsrForBuiltinOperation(args));
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        dstGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(-1);
        EXPECT_EQ(&bcsCsr, &queue->selectCsrForBuiltinOperation(args));
        DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(0);
        EXPECT_EQ(&bcsCsr, &queue->selectCsrForBuiltinOperation(args));
        DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(1);
        EXPECT_EQ(&bcsCsr, &queue->selectCsrForBuiltinOperation(args));
    }
}

TEST_F(CsrSelectionCommandQueueWithBlitterTests, givenInvalidTransferDirectionWhenSelectingCsrThenThrowError) {
    DebugManagerStateRestore restore{};
    DebugManager.flags.EnableBlitterForEnqueueOperations.set(1);

    BuiltinOpParams builtinOpParams{};
    MockGraphicsAllocation srcGraphicsAllocation{};
    MockGraphicsAllocation dstGraphicsAllocation{};
    MockBuffer srcMemObj{srcGraphicsAllocation};
    MockBuffer dstMemObj{dstGraphicsAllocation};
    builtinOpParams.srcMemObj = &srcMemObj;
    builtinOpParams.dstMemObj = &dstMemObj;

    CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
    args.direction = static_cast<TransferDirection>(0xFF);
    EXPECT_ANY_THROW(queue->selectCsrForBuiltinOperation(args));
}

TEST_F(CsrSelectionCommandQueueWithBlitterTests, givenBlitterAndAssignBCSAtEnqueueSetToFalseWhenSelectCsrThenDefaultBcsReturned) {
    DebugManagerStateRestore restore{};
    DebugManager.flags.EnableBlitterForEnqueueOperations.set(1);
    DebugManager.flags.AssignBCSAtEnqueue.set(0);

    BuiltinOpParams builtinOpParams{};
    MockGraphicsAllocation srcGraphicsAllocation{};
    MockGraphicsAllocation dstGraphicsAllocation{};
    MockBuffer srcMemObj{srcGraphicsAllocation};
    MockBuffer dstMemObj{dstGraphicsAllocation};
    builtinOpParams.srcMemObj = &srcMemObj;
    builtinOpParams.dstMemObj = &dstMemObj;

    CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
    args.direction = TransferDirection::LocalToHost;

    auto &csr = queue->selectCsrForBuiltinOperation(args);

    EXPECT_EQ(&csr, queue->getBcsCommandStreamReceiver(queue->bcsEngineTypes[0]));
}

TEST_F(CsrSelectionCommandQueueWithQueueFamiliesBlitterTests, givenBlitterSelectedWithQueueFamiliesWhenSelectingBlitterThenSelectBlitter) {
    DebugManagerStateRestore restore{};

    BuiltinOpParams builtinOpParams{};
    MockGraphicsAllocation srcGraphicsAllocation{};
    MockGraphicsAllocation dstGraphicsAllocation{};
    MockBuffer srcMemObj{srcGraphicsAllocation};
    MockBuffer dstMemObj{dstGraphicsAllocation};
    builtinOpParams.srcMemObj = &srcMemObj;
    builtinOpParams.dstMemObj = &dstMemObj;

    {
        srcGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        dstGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS), &queue->selectCsrForBuiltinOperation(args));
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        dstGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS), &queue->selectCsrForBuiltinOperation(args));
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        dstGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS), &queue->selectCsrForBuiltinOperation(args));
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        dstGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS), &queue->selectCsrForBuiltinOperation(args));
    }
}

TEST_F(CsrSelectionCommandQueueWithQueueFamiliesBlitterTests, givenBlitterSelectedWithQueueFamiliesButDisabledWithDebugFlagWhenSelectingBlitterThenIgnoreDebugFlagAndSelectBlitter) {
    DebugManagerStateRestore restore{};
    DebugManager.flags.EnableBlitterForEnqueueOperations.set(0);

    BuiltinOpParams builtinOpParams{};
    MockGraphicsAllocation srcGraphicsAllocation{};
    MockGraphicsAllocation dstGraphicsAllocation{};
    MockBuffer srcMemObj{srcGraphicsAllocation};
    MockBuffer dstMemObj{dstGraphicsAllocation};
    builtinOpParams.srcMemObj = &srcMemObj;
    builtinOpParams.dstMemObj = &dstMemObj;

    {
        srcGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        dstGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS), &queue->selectCsrForBuiltinOperation(args));
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        dstGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS), &queue->selectCsrForBuiltinOperation(args));
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        dstGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS), &queue->selectCsrForBuiltinOperation(args));
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        dstGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS), &queue->selectCsrForBuiltinOperation(args));
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

TEST(CommandQueue, given64KBTileWith3DImageTypeWhenCallingBlitEnqueueImageAllowedThenCorrectResultIsReturned) {
    DebugManagerStateRestore restorer;
    MockContext context{};
    MockCommandQueue queue(&context, context.getDevice(0), 0, false);
    const auto &hwInfo = *defaultHwInfo;
    const auto &hwInfoConfig = HwInfoConfig::get(hwInfo.platform.eProductFamily);

    size_t correctRegion[3] = {10u, 10u, 0};
    size_t correctOrigin[3] = {1u, 1u, 0};
    std::array<std::unique_ptr<Image>, 5> images = {
        std::unique_ptr<Image>(ImageHelper<Image1dDefaults>::create(&context)),
        std::unique_ptr<Image>(ImageHelper<Image1dArrayDefaults>::create(&context)),
        std::unique_ptr<Image>(ImageHelper<Image2dDefaults>::create(&context)),
        std::unique_ptr<Image>(ImageHelper<Image2dArrayDefaults>::create(&context)),
        std::unique_ptr<Image>(ImageHelper<Image3dDefaults>::create(&context))};

    for (auto blitterEnabled : {0, 1}) {
        DebugManager.flags.EnableBlitterForEnqueueImageOperations.set(blitterEnabled);
        for (auto isTile64 : {0, 1}) {
            for (const auto &image : images) {
                auto imageType = image->getImageDesc().image_type;
                auto gfxAllocation = image->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex());
                auto mockGmmResourceInfo = reinterpret_cast<MockGmmResourceInfo *>(gfxAllocation->getDefaultGmm()->gmmResourceInfo.get());
                mockGmmResourceInfo->getResourceFlags()->Info.Tile64 = isTile64;

                if (isTile64 && (imageType == CL_MEM_OBJECT_IMAGE3D)) {
                    auto supportExpected = hwInfoConfig->isTile64With3DSurfaceOnBCSSupported(hwInfo) && blitterEnabled;
                    EXPECT_EQ(supportExpected, queue.blitEnqueueImageAllowed(correctOrigin, correctRegion, *image));
                } else {
                    EXPECT_EQ(blitterEnabled, queue.blitEnqueueImageAllowed(correctOrigin, correctRegion, *image));
                }
            }
        }
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

struct CommandQueueWithTimestampPacketTests : ::testing::Test {
    void SetUp() override {
        DebugManager.flags.EnableTimestampPacket.set(1);
    }

    DebugManagerStateRestore restore{};
};

TEST_F(CommandQueueWithTimestampPacketTests, givenInOrderQueueWhenSetupBarrierTimestampForBcsEnginesCalledThenEnsureBarrierNodeIsPresent) {
    MockContext context{};
    MockCommandQueue queue{context};
    TimestampPacketDependencies dependencies{};
    for (auto &containers : queue.bcsTimestampPacketContainers) {
        EXPECT_TRUE(containers.lastBarrierToWaitFor.peekNodes().empty());
    }

    // No pending barrier, skip
    queue.setupBarrierTimestampForBcsEngines(aub_stream::EngineType::ENGINE_RCS, dependencies);
    EXPECT_EQ(0u, dependencies.barrierNodes.peekNodes().size());

    // Add barrier node
    queue.getGpgpuCommandStreamReceiver().requestStallingCommandsOnNextFlush();
    queue.setupBarrierTimestampForBcsEngines(aub_stream::EngineType::ENGINE_RCS, dependencies);
    EXPECT_EQ(1u, dependencies.barrierNodes.peekNodes().size());
    auto node1 = dependencies.barrierNodes.peekNodes()[0];

    // Do not add new node, if it exists
    queue.setupBarrierTimestampForBcsEngines(aub_stream::EngineType::ENGINE_RCS, dependencies);
    EXPECT_EQ(1u, dependencies.barrierNodes.peekNodes().size());
    auto node2 = dependencies.barrierNodes.peekNodes()[0];
    EXPECT_EQ(node2, node1);

    for (auto &containers : queue.bcsTimestampPacketContainers) {
        EXPECT_TRUE(containers.lastBarrierToWaitFor.peekNodes().empty());
    }
}

TEST_F(CommandQueueWithTimestampPacketTests, givenOutOfOrderQueueWhenSetupBarrierTimestampForBcsEnginesCalledOnBcsEngineThenEnsureBarrierNodeIsPresentAndSaveItForOtherBcses) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    MockContext context{};
    MockCommandQueue queue{&context, context.getDevice(0), props, false};
    TimestampPacketDependencies dependencies{};
    queue.getGpgpuCommandStreamReceiver().requestStallingCommandsOnNextFlush();
    for (auto &containers : queue.bcsTimestampPacketContainers) {
        EXPECT_TRUE(containers.lastBarrierToWaitFor.peekNodes().empty());
    }

    queue.setupBarrierTimestampForBcsEngines(aub_stream::EngineType::ENGINE_BCS, dependencies);
    EXPECT_EQ(1u, dependencies.barrierNodes.peekNodes().size());
    auto barrierNode = dependencies.barrierNodes.peekNodes()[0];

    for (auto currentBcsIndex = 0u; currentBcsIndex < queue.bcsTimestampPacketContainers.size(); currentBcsIndex++) {
        auto &containers = queue.bcsTimestampPacketContainers[currentBcsIndex];
        if (currentBcsIndex == 0) {
            EXPECT_EQ(0u, containers.lastBarrierToWaitFor.peekNodes().size());
        } else {
            EXPECT_EQ(1u, containers.lastBarrierToWaitFor.peekNodes().size());
            EXPECT_EQ(barrierNode, containers.lastBarrierToWaitFor.peekNodes()[0]);
        }
    }
    EXPECT_EQ(queue.bcsTimestampPacketContainers.size(), barrierNode->refCountFetchSub(0));
}

TEST_F(CommandQueueWithTimestampPacketTests, givenOutOfOrderQueueWhenSetupBarrierTimestampForBcsEnginesAndOverwritePreviousOneThenEnsureBarrierNodeHasDataAssigned) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    MockContext context{};
    MockCommandQueue queue{&context, context.getDevice(0), props, false};
    TimestampPacketDependencies dependencies{};
    queue.getGpgpuCommandStreamReceiver().requestStallingCommandsOnNextFlush();
    for (auto &containers : queue.bcsTimestampPacketContainers) {
        EXPECT_TRUE(containers.lastBarrierToWaitFor.peekNodes().empty());
    }

    queue.setupBarrierTimestampForBcsEngines(aub_stream::EngineType::ENGINE_BCS, dependencies);
    EXPECT_EQ(1u, dependencies.barrierNodes.peekNodes().size());
    auto barrierNode = dependencies.barrierNodes.peekNodes()[0];
    EXPECT_EQ(1u, barrierNode->getContextEndValue(0u));
    dependencies.moveNodesToNewContainer(*queue.getDeferredTimestampPackets());
    queue.getGpgpuCommandStreamReceiver().requestStallingCommandsOnNextFlush();
    barrierNode->incRefCount();
    barrierNode->incRefCount();
    barrierNode->incRefCount();
    barrierNode->incRefCount();

    queue.setupBarrierTimestampForBcsEngines(aub_stream::EngineType::ENGINE_BCS, dependencies);
    EXPECT_EQ(1u, barrierNode->getContextEndValue(0u));
    EXPECT_EQ(1u, dependencies.barrierNodes.peekNodes().size());
    barrierNode->refCountFetchSub(4u);
    barrierNode = dependencies.barrierNodes.peekNodes()[0];
    EXPECT_EQ(1u, barrierNode->getContextEndValue(0u));
    dependencies.moveNodesToNewContainer(*queue.getDeferredTimestampPackets());
    queue.getGpgpuCommandStreamReceiver().requestStallingCommandsOnNextFlush();

    queue.setupBarrierTimestampForBcsEngines(aub_stream::EngineType::ENGINE_BCS, dependencies);
    EXPECT_NE(1u, barrierNode->getContextEndValue(0u));
    EXPECT_EQ(1u, dependencies.barrierNodes.peekNodes().size());
    barrierNode = dependencies.barrierNodes.peekNodes()[0];
    EXPECT_EQ(1u, barrierNode->getContextEndValue(0u));
}

TEST_F(CommandQueueWithTimestampPacketTests, givenOutOfOrderQueueWhenSetupBarrierTimestampForBcsEnginesCalledOnNonBcsEngineThenEnsureBarrierNodeIsPresentAndSaveItForBcses) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    MockContext context{};
    MockCommandQueue queue{&context, context.getDevice(0), props, false};
    TimestampPacketDependencies dependencies{};
    queue.getGpgpuCommandStreamReceiver().requestStallingCommandsOnNextFlush();
    for (auto &containers : queue.bcsTimestampPacketContainers) {
        EXPECT_TRUE(containers.lastBarrierToWaitFor.peekNodes().empty());
    }

    for (auto engineType : {aub_stream::EngineType::ENGINE_RCS,
                            aub_stream::EngineType::ENGINE_CCS}) {
        queue.setupBarrierTimestampForBcsEngines(engineType, dependencies);
        EXPECT_EQ(1u, dependencies.barrierNodes.peekNodes().size());
        auto barrierNode = dependencies.barrierNodes.peekNodes()[0];

        for (auto &containers : queue.bcsTimestampPacketContainers) {
            EXPECT_EQ(1u, containers.lastBarrierToWaitFor.peekNodes().size());
            EXPECT_EQ(barrierNode, containers.lastBarrierToWaitFor.peekNodes()[0]);
        }
        EXPECT_EQ(1u + queue.bcsTimestampPacketContainers.size(), barrierNode->refCountFetchSub(0));
    }
}

TEST_F(CommandQueueWithTimestampPacketTests, givenSavedBarrierWhenProcessBarrierTimestampForBcsEngineCalledThenMoveSaveBarrierPacketToBarrierNodes) {
    MockContext context{};
    MockCommandQueue queue{context};
    TimestampPacketDependencies dependencies{};

    // No saved barriers
    queue.processBarrierTimestampForBcsEngine(aub_stream::EngineType::ENGINE_BCS, dependencies);
    EXPECT_TRUE(dependencies.barrierNodes.peekNodes().empty());

    // Save barrier
    TagNodeBase *node = queue.getGpgpuCommandStreamReceiver().getTimestampPacketAllocator()->getTag();
    queue.bcsTimestampPacketContainers[0].lastBarrierToWaitFor.add(node);
    queue.processBarrierTimestampForBcsEngine(aub_stream::EngineType::ENGINE_BCS, dependencies);
    EXPECT_EQ(1u, dependencies.barrierNodes.peekNodes().size());
    EXPECT_EQ(node, dependencies.barrierNodes.peekNodes()[0]);
    EXPECT_TRUE(queue.bcsTimestampPacketContainers[0].lastBarrierToWaitFor.peekNodes().empty());
}

TEST_F(CommandQueueWithTimestampPacketTests, givenOutOfOrderQueueWhenBarrierTimestampAreSetupOnComputeEngineAndProcessedOnBcsThenPacketIsInBarrierNodes) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    MockContext context{};
    MockCommandQueue queue{&context, context.getDevice(0), props, false};
    queue.getGpgpuCommandStreamReceiver().requestStallingCommandsOnNextFlush();

    for (auto engineType : {aub_stream::EngineType::ENGINE_RCS,
                            aub_stream::EngineType::ENGINE_CCS}) {
        TimestampPacketDependencies dependencies{};
        queue.setupBarrierTimestampForBcsEngines(engineType, dependencies);

        TimestampPacketDependencies blitDependencies{};
        queue.processBarrierTimestampForBcsEngine(aub_stream::EngineType::ENGINE_BCS, blitDependencies);
        EXPECT_EQ(1u, blitDependencies.barrierNodes.peekNodes().size());
    }
}

TEST_F(CommandQueueWithTimestampPacketTests, givenOutOfOrderQueueWhenBarrierTimestampAreSetupOnBcsEngineAndProcessedOnBcsThenPacketIsInBarrierNodes) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    MockContext context{};
    MockCommandQueue queue{&context, context.getDevice(0), props, false};
    queue.getGpgpuCommandStreamReceiver().requestStallingCommandsOnNextFlush();

    TimestampPacketDependencies dependencies{};
    queue.setupBarrierTimestampForBcsEngines(aub_stream::EngineType::ENGINE_BCS, dependencies);
    queue.processBarrierTimestampForBcsEngine(aub_stream::EngineType::ENGINE_BCS, dependencies);
    EXPECT_EQ(1u, dependencies.barrierNodes.peekNodes().size());
}

TEST_F(CommandQueueWithTimestampPacketTests, givenInOrderQueueWhenSettingLastBcsPacketThenDoNotSaveThePacket) {
    MockContext context{};
    MockCommandQueue queue{context};

    queue.setLastBcsPacket(aub_stream::EngineType::ENGINE_BCS);
    EXPECT_TRUE(queue.bcsTimestampPacketContainers[0].lastSignalledPacket.peekNodes().empty());
}

TEST_F(CommandQueueWithTimestampPacketTests, givenOutOfOrderQueueWhenSettingLastBcsPacketThenSaveOnlyOneLastPacket) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    MockContext context{};
    MockCommandQueue queue{&context, context.getDevice(0), props, false};

    queue.timestampPacketContainer->add(queue.getGpgpuCommandStreamReceiver().getTimestampPacketAllocator()->getTag());
    queue.setLastBcsPacket(aub_stream::EngineType::ENGINE_BCS);
    EXPECT_EQ(queue.timestampPacketContainer->peekNodes(), queue.bcsTimestampPacketContainers[0].lastSignalledPacket.peekNodes());
    EXPECT_EQ(1u, queue.timestampPacketContainer->peekNodes().size());

    queue.timestampPacketContainer->moveNodesToNewContainer(*queue.getDeferredTimestampPackets());

    queue.timestampPacketContainer->add(queue.getGpgpuCommandStreamReceiver().getTimestampPacketAllocator()->getTag());
    queue.setLastBcsPacket(aub_stream::EngineType::ENGINE_BCS);
    EXPECT_EQ(queue.timestampPacketContainer->peekNodes(), queue.bcsTimestampPacketContainers[0].lastSignalledPacket.peekNodes());
    EXPECT_EQ(1u, queue.timestampPacketContainer->peekNodes().size());
}

TEST_F(CommandQueueWithTimestampPacketTests, givenLastSignalledPacketWhenFillingCsrDependenciesThenMovePacketToCsrDependencies) {
    MockContext context{};
    MockCommandQueue queue{context};
    queue.bcsTimestampPacketContainers[0].lastSignalledPacket.add(queue.getGpgpuCommandStreamReceiver().getTimestampPacketAllocator()->getTag());

    CsrDependencies csrDeps;
    queue.fillCsrDependenciesWithLastBcsPackets(csrDeps);
    EXPECT_EQ(1u, queue.bcsTimestampPacketContainers[0].lastSignalledPacket.peekNodes().size());
    EXPECT_EQ(&queue.bcsTimestampPacketContainers[0].lastSignalledPacket, csrDeps.timestampPacketContainer[0]);
}

TEST_F(CommandQueueWithTimestampPacketTests, givenLastSignalledPacketWhenClearingPacketsThenClearThePacket) {
    MockContext context{};
    MockCommandQueue queue{context};
    queue.bcsTimestampPacketContainers[0].lastSignalledPacket.add(queue.getGpgpuCommandStreamReceiver().getTimestampPacketAllocator()->getTag());

    queue.clearLastBcsPackets();
    EXPECT_EQ(0u, queue.bcsTimestampPacketContainers[0].lastBarrierToWaitFor.peekNodes().size());
}

TEST_F(CommandQueueWithTimestampPacketTests, givenQueueWhenSettingAndQueryingLastBcsPacketThenReturnCorrectResults) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    MockContext context{};
    MockCommandQueue queue{&context, context.getDevice(0), props, false};
    queue.timestampPacketContainer->add(queue.getGpgpuCommandStreamReceiver().getTimestampPacketAllocator()->getTag());

    queue.setLastBcsPacket(aub_stream::EngineType::ENGINE_BCS);

    CsrDependencies csrDeps;
    queue.fillCsrDependenciesWithLastBcsPackets(csrDeps);
    EXPECT_FALSE(csrDeps.timestampPacketContainer.empty());

    queue.clearLastBcsPackets();
    for (auto &containers : queue.bcsTimestampPacketContainers) {
        EXPECT_TRUE(containers.lastSignalledPacket.peekNodes().empty());
    }
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
        const EngineInstancesContainer getGpgpuEngineInstances(const HardwareInfo &hwInfo) const override {
            EngineInstancesContainer result{};
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

        EngineGroupType getEngineGroupType(aub_stream::EngineType engineType, EngineUsage engineUsage, const HardwareInfo &hwInfo) const override {
            switch (engineType) {
            case aub_stream::ENGINE_RCS:
                return EngineGroupType::RenderCompute;
            case aub_stream::ENGINE_CCS:
            case aub_stream::ENGINE_CCS1:
            case aub_stream::ENGINE_CCS2:
            case aub_stream::ENGINE_CCS3:
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
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    MockDevice *device = MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0);
    MockClDevice clDevice{device};
    MockContext context{&clDevice};
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
    EXPECT_EQ(engineBcs.commandStreamReceiver, queueBcs.getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS));
    EXPECT_TRUE(queueBcs.isCopyOnly);
    EXPECT_TRUE(queueBcs.isQueueFamilySelected());
    EXPECT_EQ(properties[1], queueBcs.getQueueFamilyIndex());
    EXPECT_EQ(properties[3], queueBcs.getQueueIndexWithinFamily());
    EXPECT_NE(nullptr, queueBcs.getTimestampPacketContainer());
}

HWTEST_F(CommandQueueOnSpecificEngineTests, givenRootDeviceAndMultipleFamiliesWhenCreatingQueueOnSpecificEngineThenUseDefaultEngine) {
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo->capabilityTable.blitterOperationsSupported = true;
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

    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo->capabilityTable.blitterOperationsSupported = true;
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
    EXPECT_EQ(engineBcs.commandStreamReceiver, queueBcs.getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS));
    EXPECT_TRUE(queueBcs.isCopyOnly);
    EXPECT_NE(nullptr, queueBcs.getTimestampPacketContainer());
    EXPECT_TRUE(queueBcs.isQueueFamilySelected());
    EXPECT_EQ(properties[1], queueBcs.getQueueFamilyIndex());
    EXPECT_EQ(properties[3], queueBcs.getQueueIndexWithinFamily());
}

HWTEST_F(CommandQueueOnSpecificEngineTests, givenBcsFamilySelectedWhenCreatingQueueOnSpecificEngineThenInitializeBcsProperly) {
    auto raiiHwHelper = overrideHwHelper<FamilyType, MockHwHelper<FamilyType, 0, 0, 1>>();

    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo->capabilityTable.blitterOperationsSupported = true;
    MockContext context{};
    cl_command_queue_properties properties[5] = {};

    fillProperties(properties, 0, 0);
    EngineControl &engineBcs = context.getDevice(0)->getEngine(aub_stream::ENGINE_BCS, EngineUsage::Regular);
    MockCommandQueue queueBcs(&context, context.getDevice(0), properties, false);
    EXPECT_EQ(engineBcs.commandStreamReceiver, queueBcs.getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS));
    EXPECT_TRUE(queueBcs.isCopyOnly);
    EXPECT_NE(nullptr, queueBcs.getTimestampPacketContainer());
    EXPECT_TRUE(queueBcs.isQueueFamilySelected());
    EXPECT_EQ(properties[1], queueBcs.getQueueFamilyIndex());
    EXPECT_EQ(properties[3], queueBcs.getQueueIndexWithinFamily());
}

HWTEST_F(CommandQueueOnSpecificEngineTests, givenNotInitializedRcsOsContextWhenCreatingQueueThenInitializeOsContext) {
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo->capabilityTable.blitterOperationsSupported = true;
    DebugManagerStateRestore restore{};
    DebugManager.flags.NodeOrdinal.set(static_cast<int32_t>(aub_stream::EngineType::ENGINE_RCS));
    DebugManager.flags.DeferOsContextInitialization.set(1);

    auto raiiHwHelper = overrideHwHelper<FamilyType, MockHwHelper<FamilyType, 1, 1, 1>>();
    MockContext context{};
    cl_command_queue_properties properties[5] = {};

    OsContext &osContext = *context.getDevice(0)->getEngine(aub_stream::ENGINE_CCS, EngineUsage::Regular).osContext;
    EXPECT_FALSE(osContext.isInitialized());

    const auto ccsFamilyIndex = static_cast<cl_uint>(context.getDevice(0)->getDevice().getEngineGroupIndexFromEngineGroupType(EngineGroupType::Compute));
    fillProperties(properties, ccsFamilyIndex, 0);
    MockCommandQueueHw<FamilyType> queue(&context, context.getDevice(0), properties);
    ASSERT_EQ(&osContext, queue.gpgpuEngine->osContext);
    EXPECT_TRUE(osContext.isInitialized());
}

HWTEST_F(CommandQueueOnSpecificEngineTests, givenNotInitializedCcsOsContextWhenCreatingQueueThenInitializeOsContext) {
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo->capabilityTable.blitterOperationsSupported = true;
    DebugManagerStateRestore restore{};
    DebugManager.flags.NodeOrdinal.set(static_cast<int32_t>(aub_stream::EngineType::ENGINE_CCS));
    DebugManager.flags.DeferOsContextInitialization.set(1);

    auto raiiHwHelper = overrideHwHelper<FamilyType, MockHwHelper<FamilyType, 1, 1, 1>>();
    MockContext context{};
    cl_command_queue_properties properties[5] = {};

    OsContext &osContext = *context.getDevice(0)->getEngine(aub_stream::ENGINE_RCS, EngineUsage::Regular).osContext;
    EXPECT_FALSE(osContext.isInitialized());

    const auto rcsFamilyIndex = static_cast<cl_uint>(context.getDevice(0)->getDevice().getEngineGroupIndexFromEngineGroupType(EngineGroupType::RenderCompute));
    fillProperties(properties, rcsFamilyIndex, 0);
    MockCommandQueueHw<FamilyType> queue(&context, context.getDevice(0), properties);
    ASSERT_EQ(&osContext, queue.gpgpuEngine->osContext);
    EXPECT_TRUE(osContext.isInitialized());
}

TEST_F(MultiTileFixture, givenSubDeviceWhenQueueIsCreatedThenItContainsProperDevice) {
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo->capabilityTable.blitterOperationsSupported = true;
    auto tile0 = platform()->getClDevice(0)->getSubDevice(0);

    const cl_device_id deviceId = tile0;
    auto returnStatus = CL_SUCCESS;
    auto context = clCreateContext(nullptr, 1, &deviceId, nullptr, nullptr, &returnStatus);
    EXPECT_EQ(CL_SUCCESS, returnStatus);
    EXPECT_NE(nullptr, context);

    auto commandQueue = clCreateCommandQueueWithProperties(context, tile0, nullptr, &returnStatus);
    EXPECT_EQ(CL_SUCCESS, returnStatus);
    EXPECT_NE(nullptr, commandQueue);

    auto neoQueue = castToObject<CommandQueue>(commandQueue);
    EXPECT_EQ(&tile0->getDevice(), &neoQueue->getDevice());

    clReleaseCommandQueue(commandQueue);
    clReleaseContext(context);
}

TEST_F(MultiTileFixture, givenTile1WhenQueueIsCreatedThenItContainsTile1Device) {
    auto tile1 = platform()->getClDevice(0)->getSubDevice(1);

    const cl_device_id deviceId = tile1;
    auto returnStatus = CL_SUCCESS;
    auto context = clCreateContext(nullptr, 1, &deviceId, nullptr, nullptr, &returnStatus);
    EXPECT_EQ(CL_SUCCESS, returnStatus);
    EXPECT_NE(nullptr, context);

    auto commandQueue = clCreateCommandQueueWithProperties(context, tile1, nullptr, &returnStatus);
    EXPECT_EQ(CL_SUCCESS, returnStatus);
    EXPECT_NE(nullptr, commandQueue);

    auto neoQueue = castToObject<CommandQueue>(commandQueue);
    EXPECT_EQ(&tile1->getDevice(), &neoQueue->getDevice());

    clReleaseCommandQueue(commandQueue);
    clReleaseContext(context);
}

struct CopyOnlyQueueTests : ::testing::Test {
    void SetUp() override {
        typeUsageRcs.first = EngineHelpers::remapEngineTypeToHwSpecific(typeUsageRcs.first, *defaultHwInfo);

        auto device = MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get());
        auto copyEngineGroup = std::find_if(device->regularEngineGroups.begin(), device->regularEngineGroups.end(), [](const auto &engineGroup) {
            return engineGroup.engineGroupType == EngineGroupType::Copy;
        });
        if (copyEngineGroup == device->regularEngineGroups.end()) {
            GTEST_SKIP();
        }
        device->regularEngineGroups.clear();
        device->allEngines.clear();

        device->createEngine(0, typeUsageRcs);
        device->createEngine(1, typeUsageBcs);
        bcsEngine = &device->getAllEngines().back();

        clDevice = std::make_unique<MockClDevice>(device);

        context = std::make_unique<MockContext>(clDevice.get());

        properties[1] = device->getEngineGroupIndexFromEngineGroupType(EngineGroupType::Copy);
    }

    EngineTypeUsage typeUsageBcs = EngineTypeUsage{aub_stream::EngineType::ENGINE_BCS, EngineUsage::Regular};
    EngineTypeUsage typeUsageRcs = EngineTypeUsage{aub_stream::EngineType::ENGINE_RCS, EngineUsage::Regular};

    std::unique_ptr<MockClDevice> clDevice{};
    std::unique_ptr<MockContext> context{};
    std::unique_ptr<MockCommandQueue> queue{};
    const EngineControl *bcsEngine = nullptr;

    cl_queue_properties properties[5] = {CL_QUEUE_FAMILY_INTEL, 0, CL_QUEUE_INDEX_INTEL, 0, 0};
};

TEST_F(CopyOnlyQueueTests, givenBcsSelectedWhenCreatingCommandQueueThenItIsCopyOnly) {
    MockCommandQueue queue{context.get(), clDevice.get(), properties, false};
    EXPECT_EQ(bcsEngine->commandStreamReceiver, queue.getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS));
    EXPECT_EQ(1u, queue.countBcsEngines());
    EXPECT_NE(nullptr, queue.timestampPacketContainer);
    EXPECT_TRUE(queue.isCopyOnly);
}

HWTEST_F(CopyOnlyQueueTests, givenBcsSelectedWhenEnqueuingCopyThenBcsIsUsed) {
    auto srcBuffer = std::unique_ptr<Buffer>{BufferHelper<>::create(context.get())};
    auto dstBuffer = std::unique_ptr<Buffer>{BufferHelper<>::create(context.get())};
    MockCommandQueueHw<FamilyType> queue{context.get(), clDevice.get(), properties};
    auto commandStream = &bcsEngine->commandStreamReceiver->getCS(1024);

    auto usedCommandStream = commandStream->getUsed();
    cl_int retVal = queue.enqueueCopyBuffer(
        srcBuffer.get(),
        dstBuffer.get(),
        0,
        0,
        1,
        0,
        nullptr,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(usedCommandStream, commandStream->getUsed());
}

HWTEST_F(CopyOnlyQueueTests, givenBlitterEnabledWhenCreatingBcsCommandQueueThenReturnSuccess) {
    DebugManagerStateRestore restore{};
    DebugManager.flags.EnableBlitterOperationsSupport.set(1);

    cl_int retVal{};
    auto commandQueue = clCreateCommandQueueWithProperties(context.get(), clDevice.get(), properties, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, commandQueue);
    EXPECT_EQ(CL_SUCCESS, clReleaseCommandQueue(commandQueue));
}

using MultiEngineQueueHwTests = ::testing::Test;

HWCMDTEST_F(IGFX_XE_HP_CORE, MultiEngineQueueHwTests, givenQueueFamilyPropertyWhenQueueIsCreatedThenSelectValidEngine) {
    initPlatform();
    HardwareInfo localHwInfo = *defaultHwInfo;

    localHwInfo.featureTable.flags.ftrCCSNode = true;

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&localHwInfo));
    MockContext context(device.get());
    context.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;

    bool ccsFound = false;
    for (auto &engine : device->allEngines) {
        if (engine.osContext->getEngineType() == aub_stream::EngineType::ENGINE_CCS) {
            ccsFound = true;
            break;
        }
    }

    struct CommandQueueTestValues {
        CommandQueueTestValues() = delete;
        CommandQueueTestValues(cl_queue_properties engineFamily, cl_queue_properties engineIndex, aub_stream::EngineType expectedEngine)
            : expectedEngine(expectedEngine) {
            properties[1] = engineFamily;
            properties[3] = engineIndex;
        };

        cl_command_queue clCommandQueue = nullptr;
        CommandQueue *commandQueueObj = nullptr;
        cl_queue_properties properties[5] = {CL_QUEUE_FAMILY_INTEL, 0, CL_QUEUE_INDEX_INTEL, 0, 0};
        aub_stream::EngineType expectedEngine;
    };
    auto addTestValueIfAvailable = [&](std::vector<CommandQueueTestValues> &vec, EngineGroupType engineGroup, cl_queue_properties queueIndex, aub_stream::EngineType engineType, bool csEnabled) {
        if (csEnabled) {
            const auto familyIndex = device->getDevice().getEngineGroupIndexFromEngineGroupType(engineGroup);
            vec.push_back(CommandQueueTestValues(static_cast<cl_queue_properties>(familyIndex), queueIndex, engineType));
        }
    };
    auto retVal = CL_SUCCESS;
    const auto &ccsInstances = localHwInfo.gtSystemInfo.CCSInfo.Instances.Bits;
    std::vector<CommandQueueTestValues> commandQueueTestValues;
    addTestValueIfAvailable(commandQueueTestValues, EngineGroupType::RenderCompute, 0, EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_RCS, device->getHardwareInfo()), true);
    addTestValueIfAvailable(commandQueueTestValues, EngineGroupType::Compute, 0, aub_stream::ENGINE_CCS, ccsFound);
    addTestValueIfAvailable(commandQueueTestValues, EngineGroupType::Compute, 1, aub_stream::ENGINE_CCS1, ccsInstances.CCS1Enabled);
    addTestValueIfAvailable(commandQueueTestValues, EngineGroupType::Compute, 2, aub_stream::ENGINE_CCS2, ccsInstances.CCS2Enabled);
    addTestValueIfAvailable(commandQueueTestValues, EngineGroupType::Compute, 3, aub_stream::ENGINE_CCS3, ccsInstances.CCS3Enabled);

    for (auto &commandQueueTestValue : commandQueueTestValues) {
        if (commandQueueTestValue.properties[1] >= device->getHardwareInfo().gtSystemInfo.CCSInfo.NumberOfCCSEnabled) {
            continue;
        }
        commandQueueTestValue.clCommandQueue = clCreateCommandQueueWithProperties(&context, device.get(),
                                                                                  &commandQueueTestValue.properties[0], &retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);
        commandQueueTestValue.commandQueueObj = castToObject<CommandQueue>(commandQueueTestValue.clCommandQueue);

        auto &cmdQueueEngine = commandQueueTestValue.commandQueueObj->getGpgpuCommandStreamReceiver().getOsContext().getEngineType();
        EXPECT_EQ(commandQueueTestValue.expectedEngine, cmdQueueEngine);

        clReleaseCommandQueue(commandQueueTestValue.commandQueueObj);
    }
}

TEST_F(MultiTileFixture, givenDefaultContextWithRootDeviceWhenQueueIsCreatedThenQueueIsMultiEngine) {
    auto rootDevice = platform()->getClDevice(0);
    MockContext context(rootDevice);
    context.contextType = ContextType::CONTEXT_TYPE_DEFAULT;

    auto rootCsr = rootDevice->getDefaultEngine().commandStreamReceiver;

    MockCommandQueue queue(&context, rootDevice, nullptr, false);
    ASSERT_NE(nullptr, queue.gpgpuEngine);
    EXPECT_EQ(rootCsr->isMultiOsContextCapable(), queue.getGpgpuCommandStreamReceiver().isMultiOsContextCapable());
    EXPECT_EQ(rootCsr, queue.gpgpuEngine->commandStreamReceiver);
}

TEST_F(MultiTileFixture, givenDefaultContextWithSubdeviceWhenQueueIsCreatedThenQueueIsNotMultiEngine) {
    auto subdevice = platform()->getClDevice(0)->getSubDevice(0);
    MockContext context(subdevice);
    context.contextType = ContextType::CONTEXT_TYPE_DEFAULT;

    MockCommandQueue queue(&context, subdevice, nullptr, false);
    ASSERT_NE(nullptr, queue.gpgpuEngine);
    EXPECT_FALSE(queue.getGpgpuCommandStreamReceiver().isMultiOsContextCapable());
}

TEST_F(MultiTileFixture, givenUnrestrictiveContextWithRootDeviceWhenQueueIsCreatedThenQueueIsMultiEngine) {
    auto rootDevice = platform()->getClDevice(0);
    MockContext context(rootDevice);
    context.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;

    auto rootCsr = rootDevice->getDefaultEngine().commandStreamReceiver;

    MockCommandQueue queue(&context, rootDevice, nullptr, false);
    ASSERT_NE(nullptr, queue.gpgpuEngine);
    EXPECT_EQ(rootCsr->isMultiOsContextCapable(), queue.getGpgpuCommandStreamReceiver().isMultiOsContextCapable());
    EXPECT_EQ(rootCsr, queue.gpgpuEngine->commandStreamReceiver);
}

TEST_F(MultiTileFixture, givenNotDefaultContextWithRootDeviceAndTileIdMaskWhenQueueIsCreatedThenQueueIsMultiEngine) {
    auto rootClDevice = platform()->getClDevice(0);
    auto rootDevice = static_cast<RootDevice *>(&rootClDevice->getDevice());
    MockContext context(rootClDevice);
    context.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;

    auto rootCsr = rootDevice->getDefaultEngine().commandStreamReceiver;

    MockCommandQueue queue(&context, rootClDevice, nullptr, false);
    ASSERT_NE(nullptr, queue.gpgpuEngine);
    EXPECT_EQ(rootCsr->isMultiOsContextCapable(), queue.getGpgpuCommandStreamReceiver().isMultiOsContextCapable());
    EXPECT_EQ(rootCsr, queue.gpgpuEngine->commandStreamReceiver);
}
