/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/bcs_ccs_dependency_pair_container.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/migration_sync_data.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/fixtures/memory_management_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/raii_gfx_core_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_release_helper.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/event/event.h"
#include "opencl/source/event/user_event.h"
#include "opencl/source/helpers/cl_gfx_core_helper.h"
#include "opencl/source/sharings/sharing.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/command_stream/command_stream_fixture.h"
#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/fixtures/dispatch_flags_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/fixtures/multi_tile_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_event.h"
#include "opencl/test/unit_test/mocks/mock_image.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/mocks/mock_sharing_handler.h"

#include "gtest/gtest.h"

namespace NEO {
enum QueueThrottle : uint32_t;
} // namespace NEO

using namespace NEO;

struct CommandQueueMemoryDevice
    : public MemoryManagementFixture,
      public ClDeviceFixture {

    void setUp() {
        MemoryManagementFixture::setUp();
        ClDeviceFixture::setUp();
    }

    void tearDown() {
        ClDeviceFixture::tearDown();
        platformsImpl->clear();
        MemoryManagementFixture::tearDown();
    }
};

struct CommandQueueTest
    : public CommandQueueMemoryDevice,
      public ContextFixture,
      public CommandQueueFixture,
      ::testing::TestWithParam<uint64_t /*cl_command_queue_properties*/> {

    using CommandQueueFixture::setUp;
    using ContextFixture::setUp;

    CommandQueueTest() {
    }

    void SetUp() override {
        CommandQueueMemoryDevice::setUp();
        properties = GetParam();

        cl_device_id device = pClDevice;
        ContextFixture::setUp(1, &device);
        CommandQueueFixture::setUp(pContext, pClDevice, properties);
    }

    void TearDown() override {
        CommandQueueFixture::tearDown();
        ContextFixture::tearDown();
        CommandQueueMemoryDevice::tearDown();
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

INSTANTIATE_TEST_SUITE_P(CommandQueue,
                         CommandQueueTest,
                         ::testing::ValuesIn(allCommandQueueProperties));

TEST(CommandQueue, WhenGettingErrorCodeFromTaskCountThenProperValueIsReturned) {
    EXPECT_EQ(CL_SUCCESS, CommandQueue::getErrorCodeFromTaskCount(0));
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, CommandQueue::getErrorCodeFromTaskCount(CompletionStamp::outOfHostMemory));
    EXPECT_EQ(CL_OUT_OF_RESOURCES, CommandQueue::getErrorCodeFromTaskCount(CompletionStamp::outOfDeviceMemory));
    EXPECT_EQ(CL_OUT_OF_RESOURCES, CommandQueue::getErrorCodeFromTaskCount(CompletionStamp::gpuHang));
    EXPECT_EQ(CL_OUT_OF_RESOURCES, CommandQueue::getErrorCodeFromTaskCount(CompletionStamp::failed));
}

TEST(CommandQueue, givenCommandQueueWhenDestructedThenWaitForAllEngines) {
    uint32_t waitCalled = 0;

    class MyMockCommandQueue : public MockCommandQueue {
      public:
        MyMockCommandQueue(uint32_t *waitCalled, Context *context, ClDevice *device)
            : MockCommandQueue(context, device, nullptr, false), waitCalled(waitCalled) {
        }

        WaitStatus waitForAllEngines(bool blockedQueue, PrintfHandler *printfHandler, bool cleanTemporaryAllocationsList, bool waitForTaskCountRequired) override {
            (*waitCalled)++;
            return WaitStatus::ready;
        }

        uint32_t *waitCalled = nullptr;
    };

    auto mockDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext context(mockDevice.get());

    auto cmdQ = new MyMockCommandQueue(&waitCalled, &context, mockDevice.get());
    EXPECT_EQ(0u, waitCalled);

    cl_int retVal = CL_SUCCESS;
    releaseQueue(cmdQ, retVal);

    EXPECT_EQ(1u, waitCalled); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(CommandQueue, GivenCommandQueueWhenIsBcsIsCalledThenIsCopyOnlyIsReturned) {
    MockCommandQueue cmdQ(nullptr, nullptr, 0, false);
    EXPECT_EQ(cmdQ.isBcs(), cmdQ.isCopyOnly);
}

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
        debugManager.flags.EnableTimestampWaitForQueues.set(-1);
        const auto &productHelper = mockDevice->getProductHelper();
        const auto &compilerProductHelper = mockDevice->getCompilerProductHelper();
        bool heaplessEnabled = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo);

        auto enabled = productHelper.isTimestampWaitSupportedForQueues(heaplessEnabled);

        if (productHelper.isL3FlushAfterPostSyncSupported(heaplessEnabled)) {
            enabled &= true;
        } else {
            enabled &= !productHelper.isDcFlushAllowed();
        }

        EXPECT_EQ(enabled, cmdQ.isWaitForTimestampsEnabled());
    }

    {
        debugManager.flags.EnableTimestampWaitForQueues.set(0);
        EXPECT_FALSE(cmdQ.isWaitForTimestampsEnabled());
    }

    {
        debugManager.flags.EnableTimestampWaitForQueues.set(1);
        EXPECT_EQ(cmdQ.isWaitForTimestampsEnabled(), cmdQ.getGpgpuCommandStreamReceiver().isUpdateTagFromWaitEnabled());
    }

    {
        debugManager.flags.EnableTimestampWaitForQueues.set(2);
        EXPECT_EQ(cmdQ.isWaitForTimestampsEnabled(), cmdQ.getGpgpuCommandStreamReceiver().isDirectSubmissionEnabled());
    }

    {
        debugManager.flags.EnableTimestampWaitForQueues.set(3);
        EXPECT_EQ(cmdQ.isWaitForTimestampsEnabled(), cmdQ.getGpgpuCommandStreamReceiver().isAnyDirectSubmissionEnabled());
    }

    {
        debugManager.flags.EnableTimestampWaitForQueues.set(4);
        EXPECT_TRUE(cmdQ.isWaitForTimestampsEnabled());
    }
}

struct GetTagTest : public ClDeviceFixture,
                    public CommandQueueFixture,
                    public CommandStreamFixture,
                    public ::testing::Test {

    using CommandQueueFixture::setUp;

    void SetUp() override {
        ClDeviceFixture::setUp();
        CommandQueueFixture::setUp(nullptr, pClDevice, 0);
        CommandStreamFixture::setUp(pCmdQ);
    }

    void TearDown() override {
        CommandStreamFixture::tearDown();
        CommandQueueFixture::tearDown();
        ClDeviceFixture::tearDown();
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

TEST(CommandQueue, givenDirectSubmissionLightWhenCreateCmdQThenDisallowBlitter) {
    auto mockDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    mockDevice->device.anyDirectSubmissionEnabledReturnValue = true;
    MockCommandQueue cmdQ(nullptr, mockDevice.get(), 0, false);

    EXPECT_FALSE(cmdQ.bcsAllowed);
}

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
    debugManager.flags.CreateMultipleSubDevices.set(2);
    debugManager.flags.EnableBlitterForEnqueueOperations.set(1);
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    REQUIRE_FULL_BLITTER_OR_SKIP(device->getRootDeviceEnvironment());
    EXPECT_EQ(2u, device->getNumGenericSubDevices());
    std::unique_ptr<OsContext> bcsOsContext;

    auto subDevice = device->getSubDevice(0);
    auto &bcsEngine = subDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular);

    MockCommandQueue cmdQ(nullptr, device.get(), 0, false);

    EXPECT_NE(nullptr, cmdQ.getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS));
    EXPECT_EQ(bcsEngine.commandStreamReceiver, cmdQ.getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS));
}

TEST(CommandQueue, whenCommandQueueWithInternalUsageIsCreatedThenInternalBcsEngineIsUsed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableBlitterForEnqueueOperations.set(1);
    debugManager.flags.DeferCmdQBcsInitialization.set(0);
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    REQUIRE_FULL_BLITTER_OR_SKIP(device->getRootDeviceEnvironment());

    auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto internalUsage = true;
    auto expectedEngineType = EngineHelpers::linkCopyEnginesSupported(device->getRootDeviceEnvironment(), device->getDeviceBitfield())
                                  ? aub_stream::EngineType::ENGINE_BCS2
                                  : aub_stream::EngineType::ENGINE_BCS;

    for (auto preferInternalBcsEngine : {0, 1}) {
        debugManager.flags.PreferInternalBcsEngine.set(preferInternalBcsEngine);
        auto engineUsage = gfxCoreHelper.preferInternalBcsEngine() ? EngineUsage::internal : EngineUsage::regular;
        MockCommandQueue cmdQ(nullptr, device.get(), 0, internalUsage);
        auto &bcsEngine = device->getEngine(expectedEngineType, engineUsage);

        EXPECT_NE(nullptr, cmdQ.getBcsCommandStreamReceiver(expectedEngineType));
        EXPECT_EQ(bcsEngine.commandStreamReceiver, cmdQ.getBcsCommandStreamReceiver(expectedEngineType));
    }
}

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
        CommandQueueMemoryDevice::setUp();
        context.reset(new MockContext(pClDevice));
    }

    void TearDown() override {
        context.reset();
        CommandQueueMemoryDevice::tearDown();
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

    std::set<cl_command_type> typesToFlush = {CL_COMMAND_COPY_IMAGE, CL_COMMAND_WRITE_IMAGE, CL_COMMAND_FILL_IMAGE, CL_COMMAND_COPY_BUFFER_TO_IMAGE,
                                              CL_COMMAND_READ_IMAGE, CL_COMMAND_COPY_IMAGE_TO_BUFFER};
    for (auto operation = CL_COMMAND_NDRANGE_KERNEL; operation < CL_COMMAND_SVM_MIGRATE_MEM; operation++) {
        if (typesToFlush.find(operation) != typesToFlush.end()) {
            commandStreamReceiver.directSubmissionAvailable = true;

            if (operation == CL_COMMAND_READ_IMAGE || operation == CL_COMMAND_COPY_IMAGE_TO_BUFFER) {
                auto isCacheFlushPriorImageReadRequired = mockDevice->getGfxCoreHelper().isCacheFlushPriorImageReadRequired();
                EXPECT_EQ(isCacheFlushPriorImageReadRequired, cmdQ.isTextureCacheFlushNeeded(operation));
            } else {
                EXPECT_TRUE(cmdQ.isTextureCacheFlushNeeded(operation));
            }

            commandStreamReceiver.directSubmissionAvailable = false;
            EXPECT_FALSE(cmdQ.isTextureCacheFlushNeeded(operation));
        } else {
            commandStreamReceiver.directSubmissionAvailable = true;
            EXPECT_FALSE(cmdQ.isTextureCacheFlushNeeded(operation));
            commandStreamReceiver.directSubmissionAvailable = false;
            EXPECT_FALSE(cmdQ.isTextureCacheFlushNeeded(operation));
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
    DebugManagerStateRestore restorer;
    debugManager.flags.SetAmountOfReusableAllocationsPerCmdQueue.set(0);
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(context.get(), pClDevice, props, false);

    auto memoryManager = pDevice->getMemoryManager();
    size_t requiredSize = alignUp(100 + CSRequirements::minCommandQueueCommandStreamSize + CSRequirements::csOverfetchSize, MemoryConstants::pageSize64k);
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), requiredSize, AllocationType::commandBuffer, pDevice->getDeviceBitfield()});
    auto &commandStreamReceiver = cmdQ.getGpgpuCommandStreamReceiver();
    commandStreamReceiver.getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION);

    EXPECT_FALSE(commandStreamReceiver.getAllocationsForReuse().peekIsEmpty());
    EXPECT_TRUE(commandStreamReceiver.getAllocationsForReuse().peekContains(*allocation));

    const auto &indirectHeap = cmdQ.getCS(100);

    EXPECT_EQ(indirectHeap.getGraphicsAllocation(), allocation);

    EXPECT_TRUE(commandStreamReceiver.getAllocationsForReuse().peekIsEmpty());
}

TEST_F(CommandQueueCommandStreamTest, givenCommandQueueWhenItIsDestroyedThenCommandStreamIsPutOnTheReusableList) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SetAmountOfReusableAllocationsPerCmdQueue.set(0);
    auto cmdQ = new MockCommandQueue(context.get(), pClDevice, 0, false);
    const auto &commandStream = cmdQ->getCS(100);
    auto graphicsAllocation = commandStream.getGraphicsAllocation();
    EXPECT_TRUE(pDevice->getDefaultEngine().commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());

    // now destroy command queue, heap should go to reusable list
    delete cmdQ;
    EXPECT_FALSE(pDevice->getDefaultEngine().commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
    EXPECT_TRUE(pDevice->getDefaultEngine().commandStreamReceiver->getAllocationsForReuse().peekContains(*graphicsAllocation));
}

TEST_F(CommandQueueCommandStreamTest, WhenAskedForNewCommandStreamThenOldHeapIsStoredForReuse) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SetAmountOfReusableAllocationsPerCmdQueue.set(0);
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

    EXPECT_EQ(AllocationType::commandBuffer, commandStreamAllocation->getAllocationType());
}

struct CommandQueueIndirectHeapTest : public CommandQueueMemoryDevice,
                                      public ::testing::TestWithParam<IndirectHeap::Type> {
    void SetUp() override {
        CommandQueueMemoryDevice::setUp();
        context.reset(new MockContext(pClDevice));
    }

    void TearDown() override {
        context.reset();
        CommandQueueMemoryDevice::tearDown();
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
    if (this->GetParam() == IndirectHeap::Type::indirectObject && commandStreamReceiver.canUse4GbHeaps()) {
        EXPECT_TRUE(indirectHeap.getGraphicsAllocation()->is32BitAllocation());
    } else {
        EXPECT_FALSE(indirectHeap.getGraphicsAllocation()->is32BitAllocation());
    }
}

HWTEST_P(CommandQueueIndirectHeapTest, GivenIndirectHeapWhenGettingAvailableSpaceThenCorrectSizeIsReturned) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(context.get(), pClDevice, props, false);

    auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), sizeof(uint32_t));
    if (this->GetParam() == IndirectHeap::Type::surfaceState) {
        size_t expectedSshUse = cmdQ.getGpgpuCommandStreamReceiver().defaultSshSize - MemoryConstants::pageSize - UnitTestHelper<FamilyType>::getDefaultSshUsage();
        EXPECT_EQ(expectedSshUse, indirectHeap.getAvailableSpace());
    } else {
        EXPECT_EQ(64 * MemoryConstants::kiloByte, indirectHeap.getAvailableSpace());
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
    if (this->GetParam() == IndirectHeap::Type::surfaceState) {
        // no matter what SSH is always capped
        EXPECT_EQ(cmdQ.getGpgpuCommandStreamReceiver().defaultSshSize - MemoryConstants::pageSize,
                  indirectHeap.getMaxAvailableSpace());
    } else {
        EXPECT_LE(requiredSize, indirectHeap.getMaxAvailableSpace());
    }
}

TEST_P(CommandQueueIndirectHeapTest, WhenGettingIndirectHeapThenSizeIsAlignedToCacheLine) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(context.get(), pClDevice, props, false);
    size_t minHeapSize = 64 * MemoryConstants::kiloByte;

    auto &indirectHeapInitial = cmdQ.getIndirectHeap(this->GetParam(), 2 * minHeapSize + 1);

    EXPECT_TRUE(isAligned<MemoryConstants::cacheLineSize>(indirectHeapInitial.getAvailableSpace()));

    indirectHeapInitial.getSpace(indirectHeapInitial.getAvailableSpace()); // use whole space to force obtain reusable

    const auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), minHeapSize + 1);

    ASSERT_NE(nullptr, &indirectHeap);
    EXPECT_TRUE(isAligned<MemoryConstants::cacheLineSize>(indirectHeap.getAvailableSpace()));
}

HWTEST_P(CommandQueueIndirectHeapTest, givenCommandStreamReceiverWithReusableAllocationsWhenAskedForHeapAllocationThenAllocationFromReusablePoolIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SetAmountOfReusableAllocationsPerCmdQueue.set(0);
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(context.get(), pClDevice, props, false);

    auto memoryManager = pDevice->getMemoryManager();

    auto allocationSize = NEO::HeapSize::defaultHeapSize * 2;

    GraphicsAllocation *allocation = nullptr;

    auto &commandStreamReceiver = pClDevice->getUltCommandStreamReceiver<FamilyType>();
    auto allocationType = AllocationType::linearStream;
    if (this->GetParam() == IndirectHeap::Type::indirectObject && commandStreamReceiver.canUse4GbHeaps()) {
        allocationType = AllocationType::internalHeap;
    }
    allocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), allocationSize, allocationType, pDevice->getDeviceBitfield()});
    if (this->GetParam() == IndirectHeap::Type::surfaceState) {
        allocation->setSize(commandStreamReceiver.defaultSshSize * 2);
    }

    commandStreamReceiver.getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION);

    EXPECT_FALSE(commandStreamReceiver.getAllocationsForReuse().peekIsEmpty());
    EXPECT_TRUE(commandStreamReceiver.getAllocationsForReuse().peekContains(*allocation));

    const auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), 100);

    EXPECT_EQ(indirectHeap.getGraphicsAllocation(), allocation);

    // if we obtain heap from reusable pool, we need to keep the size of allocation
    // surface state heap is an exception, it is capped at (max_ssh_size_for_HW - page_size)
    if (this->GetParam() == IndirectHeap::Type::surfaceState) {
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

HWTEST_P(CommandQueueIndirectHeapTest, GivenCommandQueueWithoutHeapAllocationWhenAskedForNewHeapThenNewAllocationIsAcquiredWithoutStoring) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(context.get(), pClDevice, props, false);

    auto memoryManager = pDevice->getMemoryManager();
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
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

TEST_P(CommandQueueIndirectHeapTest, givenCommandQueueWithResourceCachingActiveWhenQueueIsDestroyedThenIndirectHeapIsNotOnReuseList) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SetAmountOfReusableAllocationsPerCmdQueue.set(0);
    auto cmdQ = new MockCommandQueue(context.get(), pClDevice, 0, false);
    cmdQ->getIndirectHeap(this->GetParam(), 100);
    EXPECT_TRUE(pDevice->getDefaultEngine().commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());

    // now destroy command queue, heap should NOT go to reusable list
    delete cmdQ;
    EXPECT_TRUE(pDevice->getDefaultEngine().commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
}

HWTEST_P(CommandQueueIndirectHeapTest, GivenCommandQueueWithHeapAllocatedWhenIndirectHeapIsReleasedThenHeapAllocationAndHeapBufferIsSetToNullptr) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(context.get(), pClDevice, props, false);

    EXPECT_TRUE(pDevice->getDefaultEngine().commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());

    const auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), 100);
    auto heapSize = indirectHeap.getMaxAvailableSpace();

    EXPECT_NE(0u, heapSize);

    auto graphicsAllocation = indirectHeap.getGraphicsAllocation();
    EXPECT_NE(nullptr, graphicsAllocation);

    cmdQ.releaseIndirectHeap(this->GetParam());
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_EQ(nullptr, csr.indirectHeap[this->GetParam()]->getGraphicsAllocation());

    EXPECT_EQ(nullptr, indirectHeap.getCpuBase());
    EXPECT_EQ(0u, indirectHeap.getMaxAvailableSpace());
}

HWTEST_P(CommandQueueIndirectHeapTest, GivenCommandQueueWithoutHeapAllocatedWhenIndirectHeapIsReleasedThenIndirectHeapAllocationStaysNull) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(context.get(), pClDevice, props, false);

    cmdQ.releaseIndirectHeap(this->GetParam());
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_EQ(nullptr, csr.indirectHeap[this->GetParam()]);
}

HWTEST_P(CommandQueueIndirectHeapTest, GivenCommandQueueWithHeapWhenGraphicAllocationIsNullThenNothingOnReuseList) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SetAmountOfReusableAllocationsPerCmdQueue.set(0);
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(context.get(), pClDevice, props, false);

    auto &ih = cmdQ.getIndirectHeap(this->GetParam(), 0u);
    auto allocation = ih.getGraphicsAllocation();
    EXPECT_NE(nullptr, allocation);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

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

    bool requireInternalHeap = IndirectHeap::Type::indirectObject == heapType && commandStreamReceiver.canUse4GbHeaps();
    const auto &indirectHeap = cmdQ.getIndirectHeap(heapType, 100);
    auto indirectHeapAllocation = indirectHeap.getGraphicsAllocation();
    ASSERT_NE(nullptr, indirectHeapAllocation);
    auto expectedAllocationType = AllocationType::linearStream;
    if (requireInternalHeap) {
        expectedAllocationType = AllocationType::internalHeap;
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
    debugManager.flags.ForceDefaultHeapSize.set(64 * MemoryConstants::kiloByte);

    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(context.get(), pClDevice, props, false);

    IndirectHeap *indirectHeap = nullptr;
    cmdQ.allocateHeapMemory(IndirectHeap::Type::indirectObject, 100, indirectHeap);
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

INSTANTIATE_TEST_SUITE_P(
    Device,
    CommandQueueIndirectHeapTest,
    testing::Values(
        IndirectHeap::Type::dynamicState,
        IndirectHeap::Type::indirectObject,
        IndirectHeap::Type::surfaceState));

using CommandQueueTests = ::testing::Test;
HWTEST_F(CommandQueueTests, givenMultipleCommandQueuesWhenMarkerIsEmittedThenGraphicsAllocationIsReused) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    MockContext context(device.get());
    device->device.disableSecondaryEngines = true;

    std::unique_ptr<CommandQueue> commandQ(new MockCommandQueue(&context, device.get(), 0, false));
    *device->getDefaultEngine().commandStreamReceiver->getTagAddress() = commandQ->getHeaplessStateInitEnabled() ? 1 : 0;
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
    debugManager.flags.EngineUsageHint.set(static_cast<int32_t>(EngineUsage::engineUsageCount));

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
    EXPECT_EQ(EngineUsage::regular, pCmdQ->getGpgpuEngine().getEngineUsage());
    delete pCmdQ;
}

HWTEST_F(CommandQueueTests, givenNodeOrdinalSetWithRenderEngineWhenCreatingCommandQueueWithPropertiesWhereComputeEngineSetThenProperEngineUsed) {
    DebugManagerStateRestore restore;
    MockExecutionEnvironment executionEnvironment{};
    auto forcedEngine = EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_RCS, *executionEnvironment.rootDeviceEnvironments[0]);
    debugManager.flags.NodeOrdinal.set(static_cast<int32_t>(forcedEngine));

    auto pDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    MockContext context(pDevice.get());

    cl_uint expectedEngineIndex = 0u;

    cl_int retVal = CL_SUCCESS;
    auto userPropertiesEngineGroupType = static_cast<cl_uint>(EngineGroupType::compute);
    cl_uint userPropertiesEngineIndex = 2u;

    cl_queue_properties propertiesCooperativeQueue[] = {CL_QUEUE_FAMILY_INTEL, userPropertiesEngineGroupType, CL_QUEUE_INDEX_INTEL, userPropertiesEngineIndex, 0};

    const auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    EXPECT_NE(gfxCoreHelper.getEngineGroupType(static_cast<aub_stream::EngineType>(forcedEngine), EngineUsage::regular, *defaultHwInfo),
              static_cast<EngineGroupType>(userPropertiesEngineGroupType));
    EXPECT_NE(expectedEngineIndex, userPropertiesEngineIndex);

    auto pCmdQ = CommandQueue::create(
        &context,
        pDevice.get(),
        propertiesCooperativeQueue,
        false,
        retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, pCmdQ);
    EXPECT_EQ(forcedEngine, pCmdQ->getGpgpuEngine().getEngineType());
    delete pCmdQ;
}

HWTEST_F(CommandQueueTests, givenNodeOrdinalSetWithCcsEngineWhenCreatingCommandQueueWithPropertiesAndRegularCcsEngineNotExistThenEngineNotForced) {
    DebugManagerStateRestore restore;
    MockExecutionEnvironment executionEnvironment{};
    auto defaultEngine = EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_RCS, *executionEnvironment.rootDeviceEnvironments[0]);
    debugManager.flags.NodeOrdinal.set(static_cast<int32_t>(defaultEngine));

    auto pDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    MockContext context(pDevice.get());

    cl_int retVal = CL_SUCCESS;

    cl_queue_properties propertiesCooperativeQueue[] = {CL_QUEUE_FAMILY_INTEL, 0, CL_QUEUE_INDEX_INTEL, 0, 0};

    struct FakeGfxCoreHelper : GfxCoreHelperHw<FamilyType> {
        EngineGroupType getEngineGroupType(aub_stream::EngineType engineType, EngineUsage engineUsage, const HardwareInfo &hwInfo) const override {
            return EngineGroupType::renderCompute;
        }
    };
    RAIIGfxCoreHelperFactory<FakeGfxCoreHelper> overrideGfxCoreHelper{*pDevice->executionEnvironment->rootDeviceEnvironments[0]};

    auto forcedEngine = EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_CCS, pDevice->getRootDeviceEnvironment());
    debugManager.flags.NodeOrdinal.set(static_cast<int32_t>(forcedEngine));

    auto pCmdQ = CommandQueue::create(
        &context,
        pDevice.get(),
        propertiesCooperativeQueue,
        false,
        retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, pCmdQ);
    EXPECT_NE(forcedEngine, pCmdQ->getGpgpuEngine().getEngineType());
    EXPECT_EQ(defaultEngine, pCmdQ->getGpgpuEngine().getEngineType());
    delete pCmdQ;
}

HWTEST_F(CommandQueueTests, givenPreallocationsPerQueueWhenInitializeGpgpuCalledThenCSRRequestPreallocationIsCalled) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    MockContext context(device.get());
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(&context, device.get(), nullptr);
    auto &commandStreamReceiver = device->getUltCommandStreamReceiver<FamilyType>();
    DebugManagerStateRestore restorer;
    debugManager.flags.SetAmountOfReusableAllocationsPerCmdQueue.set(1);

    EXPECT_EQ(0u, commandStreamReceiver.requestedPreallocationsAmount);
    EXPECT_TRUE(commandStreamReceiver.getAllocationsForReuse().peekIsEmpty());
    EXPECT_EQ(0u, commandStreamReceiver.getResidencyAllocations().size());

    mockCmdQ->initializeGpgpu();

    EXPECT_EQ(1u, commandStreamReceiver.requestedPreallocationsAmount);
    EXPECT_FALSE(commandStreamReceiver.getAllocationsForReuse().peekIsEmpty());
    EXPECT_EQ(1u, commandStreamReceiver.getResidencyAllocations().size());

    mockCmdQ.reset();
    EXPECT_EQ(0u, commandStreamReceiver.requestedPreallocationsAmount);
    EXPECT_FALSE(commandStreamReceiver.getAllocationsForReuse().peekIsEmpty());
    EXPECT_EQ(1u, commandStreamReceiver.getResidencyAllocations().size());
}

struct WaitForQueueCompletionTests : public ::testing::Test {
    template <typename Family>
    struct MyCmdQueue : public CommandQueueHw<Family> {
        MyCmdQueue(Context *context, ClDevice *device) : CommandQueueHw<Family>(context, device, nullptr, false){};
        WaitStatus waitUntilComplete(TaskCountType gpgpuTaskCountToWait, std::span<CopyEngineState> copyEnginesToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, bool cleanTemporaryAllocationList, bool skipWait) override {
            requestedUseQuickKmdSleep = useQuickKmdSleep;
            waitUntilCompleteCounter++;

            return WaitStatus::ready;
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
    cmdQ->finish(false);
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

    WaitStatus waitForTaskCountWithKmdNotifyFallback(TaskCountType taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, QueueThrottle throttle) override {
        waitForTaskCountWithKmdNotifyFallbackCounter++;
        return waitForTaskCountWithKmdNotifyFallbackReturnValue;
    }

    WaitStatus waitForTaskCount(TaskCountType requiredTaskCount) override {
        waitForTaskCountCalledCounter++;
        return waitForTaskCountReturnValue;
    }

    WaitStatus waitForTaskCountAndCleanTemporaryAllocationList(TaskCountType requiredTaskCount) override {
        waitForTaskCountAndCleanTemporaryAllocationListCalledCounter++;
        return waitForTaskCountAndCleanTemporaryAllocationListReturnValue;
    }

    int waitForTaskCountCalledCounter{0};
    int waitForTaskCountWithKmdNotifyFallbackCounter{0};
    int waitForTaskCountAndCleanTemporaryAllocationListCalledCounter{0};

    WaitStatus waitForTaskCountReturnValue{WaitStatus::ready};
    WaitStatus waitForTaskCountWithKmdNotifyFallbackReturnValue{WaitStatus::ready};
    WaitStatus waitForTaskCountAndCleanTemporaryAllocationListReturnValue{WaitStatus::ready};
};

struct WaitUntilCompletionTests : public ::testing::Test {
    template <typename Family>
    struct MyCmdQueue : public CommandQueueHw<Family> {
      public:
        using CommandQueue::gpgpuEngine;

        MyCmdQueue(Context *context, ClDevice *device) : CommandQueueHw<Family>(context, device, nullptr, false){};

        CommandStreamReceiver *getBcsCommandStreamReceiver(aub_stream::EngineType bcsEngineType) override {
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
    cmdStream->waitForTaskCountReturnValue = WaitStatus::ready;

    std::unique_ptr<MyCmdQueue<FamilyType>> cmdQ(new MyCmdQueue<FamilyType>(context.get(), device.get()));
    CommandStreamReceiver *oldCommandStreamReceiver = &cmdQ->getGpgpuCommandStreamReceiver();
    cmdQ->gpgpuEngine->commandStreamReceiver = cmdStream.get();

    constexpr TaskCountType taskCount = 0u;
    constexpr bool cleanTemporaryAllocationList = false;
    StackVec<CopyEngineState, bcsInfoMaskSize> activeBcsStates{};

    const auto waitStatus = cmdQ->waitUntilComplete(taskCount, activeBcsStates, cmdQ->flushStamp->peekStamp(), false, cleanTemporaryAllocationList, false);
    EXPECT_EQ(WaitStatus::ready, waitStatus);
    EXPECT_EQ(1, cmdStream->waitForTaskCountCalledCounter);

    cmdQ->gpgpuEngine->commandStreamReceiver = oldCommandStreamReceiver;
}

HWTEST_F(WaitUntilCompletionTests, givenGpuHangAndCleanTemporaryAllocationListEqualsTrueWhenWaitingUntilCompleteThenWaitForTaskCountAndCleanAllocationIsCalledAndGpuHangIsReturned) {
    std::unique_ptr<CommandStreamReceiverHwMock<FamilyType>> cmdStream(new CommandStreamReceiverHwMock<FamilyType>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield()));
    cmdStream->initializeTagAllocation();
    cmdStream->waitForTaskCountAndCleanTemporaryAllocationListReturnValue = WaitStatus::gpuHang;

    std::unique_ptr<MyCmdQueue<FamilyType>> cmdQ(new MyCmdQueue<FamilyType>(context.get(), device.get()));
    CommandStreamReceiver *oldCommandStreamReceiver = &cmdQ->getGpgpuCommandStreamReceiver();
    cmdQ->gpgpuEngine->commandStreamReceiver = cmdStream.get();

    constexpr TaskCountType taskCount = 0u;
    constexpr bool cleanTemporaryAllocationList = true;
    StackVec<CopyEngineState, bcsInfoMaskSize> activeBcsStates{};

    const auto waitStatus = cmdQ->waitUntilComplete(taskCount, activeBcsStates, cmdQ->flushStamp->peekStamp(), false, cleanTemporaryAllocationList, false);
    EXPECT_EQ(WaitStatus::gpuHang, waitStatus);
    EXPECT_EQ(1, cmdStream->waitForTaskCountAndCleanTemporaryAllocationListCalledCounter);

    cmdQ->gpgpuEngine->commandStreamReceiver = oldCommandStreamReceiver;
}

HWTEST_F(WaitUntilCompletionTests, givenEmptyBcsStatesAndSkipWaitEqualsTrueWhenWaitingUntilCompleteThenWaitForTaskCountWithKmdNotifyFallbackIsNotCalled) {
    std::unique_ptr<CommandStreamReceiverHwMock<FamilyType>> cmdStream(new CommandStreamReceiverHwMock<FamilyType>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield()));
    cmdStream->initializeTagAllocation();

    std::unique_ptr<MyCmdQueue<FamilyType>> cmdQ(new MyCmdQueue<FamilyType>(context.get(), device.get()));
    CommandStreamReceiver *oldCommandStreamReceiver = &cmdQ->getGpgpuCommandStreamReceiver();
    cmdQ->gpgpuEngine->commandStreamReceiver = cmdStream.get();

    constexpr TaskCountType taskCount = 0u;
    constexpr bool skipWait = true;
    StackVec<CopyEngineState, bcsInfoMaskSize> activeBcsStates{};

    cmdQ->waitUntilComplete(taskCount, activeBcsStates, cmdQ->flushStamp->peekStamp(), false, false, skipWait);
    EXPECT_EQ(0, cmdStream->waitForTaskCountWithKmdNotifyFallbackCounter);

    cmdQ->gpgpuEngine->commandStreamReceiver = oldCommandStreamReceiver;
}

HWTEST_F(WaitUntilCompletionTests, givenGpuHangAndSkipWaitEqualsFalseWhenWaitingUntilCompleteThenOnlyWaitForTaskCountWithKmdNotifyFallbackIsCalledAndGpuHangIsReturned) {
    std::unique_ptr<CommandStreamReceiverHwMock<FamilyType>> cmdStream(new CommandStreamReceiverHwMock<FamilyType>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield()));
    cmdStream->initializeTagAllocation();
    cmdStream->waitForTaskCountWithKmdNotifyFallbackReturnValue = WaitStatus::gpuHang;

    std::unique_ptr<MyCmdQueue<FamilyType>> cmdQ(new MyCmdQueue<FamilyType>(context.get(), device.get()));
    CommandStreamReceiver *oldCommandStreamReceiver = &cmdQ->getGpgpuCommandStreamReceiver();
    cmdQ->gpgpuEngine->commandStreamReceiver = cmdStream.get();

    constexpr TaskCountType taskCount = 0u;
    constexpr bool skipWait = false;
    StackVec<CopyEngineState, bcsInfoMaskSize> activeBcsStates{};

    const auto waitStatus = cmdQ->waitUntilComplete(taskCount, activeBcsStates, cmdQ->flushStamp->peekStamp(), false, false, skipWait);
    EXPECT_EQ(WaitStatus::gpuHang, waitStatus);

    EXPECT_EQ(0, cmdStream->waitForTaskCountCalledCounter);
    EXPECT_EQ(1, cmdStream->waitForTaskCountWithKmdNotifyFallbackCounter);
    EXPECT_EQ(0, cmdStream->waitForTaskCountAndCleanTemporaryAllocationListCalledCounter);

    cmdQ->gpgpuEngine->commandStreamReceiver = oldCommandStreamReceiver;
}

HWTEST_F(WaitUntilCompletionTests, givenGpuHangOnBcsCsrWhenWaitingUntilCompleteThenOnlyWaitForTaskCountWithKmdNotifyFallbackIsCalledOnBcsCsrAndGpuHangIsReturned) {
    std::unique_ptr<CommandStreamReceiverHwMock<FamilyType>> gpgpuCmdStream(new CommandStreamReceiverHwMock<FamilyType>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield()));
    gpgpuCmdStream->initializeTagAllocation();
    gpgpuCmdStream->waitForTaskCountWithKmdNotifyFallbackReturnValue = WaitStatus::ready;

    std::unique_ptr<CommandStreamReceiverHwMock<FamilyType>> bcsCmdStream(new CommandStreamReceiverHwMock<FamilyType>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield()));
    bcsCmdStream->initializeTagAllocation();
    bcsCmdStream->waitForTaskCountWithKmdNotifyFallbackReturnValue = WaitStatus::gpuHang;

    std::unique_ptr<MyCmdQueue<FamilyType>> cmdQ(new MyCmdQueue<FamilyType>(context.get(), device.get()));
    CommandStreamReceiver *oldCommandStreamReceiver = &cmdQ->getGpgpuCommandStreamReceiver();
    cmdQ->gpgpuEngine->commandStreamReceiver = gpgpuCmdStream.get();
    cmdQ->bcsCsrToReturn = bcsCmdStream.get();

    constexpr TaskCountType taskCount = 0u;
    constexpr bool skipWait = false;
    StackVec<CopyEngineState, bcsInfoMaskSize> activeBcsStates{CopyEngineState{}};

    const auto waitStatus = cmdQ->waitUntilComplete(taskCount, activeBcsStates, cmdQ->flushStamp->peekStamp(), false, false, skipWait);
    EXPECT_EQ(WaitStatus::gpuHang, waitStatus);

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
    gpgpuCmdStream->waitForTaskCountWithKmdNotifyFallbackReturnValue = WaitStatus::ready;

    std::unique_ptr<CommandStreamReceiverHwMock<FamilyType>> bcsCmdStream(new CommandStreamReceiverHwMock<FamilyType>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield()));
    bcsCmdStream->initializeTagAllocation();
    bcsCmdStream->waitForTaskCountWithKmdNotifyFallbackReturnValue = WaitStatus::ready;
    bcsCmdStream->waitForTaskCountAndCleanTemporaryAllocationListReturnValue = WaitStatus::gpuHang;

    std::unique_ptr<MyCmdQueue<FamilyType>> cmdQ(new MyCmdQueue<FamilyType>(context.get(), device.get()));
    CommandStreamReceiver *oldCommandStreamReceiver = &cmdQ->getGpgpuCommandStreamReceiver();
    cmdQ->gpgpuEngine->commandStreamReceiver = gpgpuCmdStream.get();
    cmdQ->bcsCsrToReturn = bcsCmdStream.get();

    constexpr TaskCountType taskCount = 0u;
    constexpr bool skipWait = false;
    StackVec<CopyEngineState, bcsInfoMaskSize> activeBcsStates{CopyEngineState{}};

    const auto waitStatus = cmdQ->waitUntilComplete(taskCount, activeBcsStates, cmdQ->flushStamp->peekStamp(), false, false, skipWait);
    EXPECT_EQ(WaitStatus::gpuHang, waitStatus);

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
    gpgpuCmdStream->waitForTaskCountWithKmdNotifyFallbackReturnValue = WaitStatus::ready;
    gpgpuCmdStream->waitForTaskCountReturnValue = WaitStatus::ready;

    std::unique_ptr<CommandStreamReceiverHwMock<FamilyType>> bcsCmdStream(new CommandStreamReceiverHwMock<FamilyType>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield()));
    bcsCmdStream->initializeTagAllocation();
    bcsCmdStream->waitForTaskCountWithKmdNotifyFallbackReturnValue = WaitStatus::ready;
    bcsCmdStream->waitForTaskCountAndCleanTemporaryAllocationListReturnValue = WaitStatus::ready;

    std::unique_ptr<MyCmdQueue<FamilyType>> cmdQ(new MyCmdQueue<FamilyType>(context.get(), device.get()));
    CommandStreamReceiver *oldCommandStreamReceiver = &cmdQ->getGpgpuCommandStreamReceiver();
    cmdQ->gpgpuEngine->commandStreamReceiver = gpgpuCmdStream.get();
    cmdQ->bcsCsrToReturn = bcsCmdStream.get();

    constexpr TaskCountType taskCount = 0u;
    constexpr bool skipWait = false;
    StackVec<CopyEngineState, bcsInfoMaskSize> activeBcsStates{CopyEngineState{}};

    const auto waitStatus = cmdQ->waitUntilComplete(taskCount, activeBcsStates, cmdQ->flushStamp->peekStamp(), false, false, skipWait);
    EXPECT_EQ(WaitStatus::ready, waitStatus);

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

using CommandQueueTests = ::testing::Test;
HWTEST_F(CommandQueueTests, givenEnqueuesForSharedObjectsWithImageWhenUsingSharingHandlerThenReturnSuccess) {
    MockContext context;
    MockCommandQueue cmdQ(&context, context.getDevice(0), 0, false);
    MockSharingHandler *mockSharingHandler = new MockSharingHandler;

    auto image = std::unique_ptr<Image>(ImageHelperUlt<Image2dDefaults>::create(&context));
    image->setSharingHandler(mockSharingHandler);
    image->getGraphicsAllocation(0u)->setAllocationType(AllocationType::sharedImage);

    cl_mem memObject = image.get();
    cl_uint numObjects = 1;
    cl_mem *memObjects = &memObject;

    cl_int result = cmdQ.enqueueAcquireSharedObjects(numObjects, memObjects, 0, nullptr, nullptr, 0);
    EXPECT_EQ(result, CL_SUCCESS);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(&cmdQ.getGpgpuCommandStreamReceiver());
    EXPECT_FALSE(ultCsr->renderStateCacheFlushed);

    result = cmdQ.enqueueReleaseSharedObjects(numObjects, memObjects, 0, nullptr, nullptr, 0);
    EXPECT_EQ(result, CL_SUCCESS);
    EXPECT_FALSE(ultCsr->renderStateCacheFlushed);
}

HWTEST_F(CommandQueueTests, givenDirectSubmissionAndSharedImageWhenReleasingSharedObjectThenFlushRenderStateCache) {
    MockContext context;
    MockCommandQueue cmdQ(&context, context.getDevice(0), 0, false);
    MockSharingHandler *mockSharingHandler = new MockSharingHandler;

    auto image = std::unique_ptr<Image>(ImageHelperUlt<Image2dDefaults>::create(&context));
    image->setSharingHandler(mockSharingHandler);
    image->getGraphicsAllocation(0u)->setAllocationType(AllocationType::sharedImage);

    cl_mem memObject = image.get();
    cl_uint numObjects = 1;
    cl_mem *memObjects = &memObject;

    cl_int result = cmdQ.enqueueAcquireSharedObjects(numObjects, memObjects, 0, nullptr, nullptr, 0);
    EXPECT_EQ(result, CL_SUCCESS);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(&cmdQ.getGpgpuCommandStreamReceiver());
    VariableBackup<bool> directSubmissionAvailable{&ultCsr->directSubmissionAvailable, true};
    ultCsr->callBaseSendRenderStateCacheFlush = false;
    ultCsr->flushReturnValue = SubmissionStatus::success;
    EXPECT_FALSE(ultCsr->renderStateCacheFlushed);

    result = cmdQ.enqueueReleaseSharedObjects(numObjects, memObjects, 0, nullptr, nullptr, 0);
    EXPECT_EQ(result, CL_SUCCESS);
    EXPECT_TRUE(ultCsr->renderStateCacheFlushed);
}

TEST(CommandQueue, givenEnqueuesForSharedObjectsWithImageWhenUsingSharingHandlerWithEventThenReturnSuccess) {
    auto mockDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext context;
    MockCommandQueue cmdQ(&context, mockDevice.get(), 0, false);
    MockSharingHandler *mockSharingHandler = new MockSharingHandler;
    constexpr cl_command_type expectedCmd = CL_COMMAND_NDRANGE_KERNEL;

    auto image = std::unique_ptr<Image>(ImageHelperUlt<Image2dDefaults>::create(&context));
    image->setSharingHandler(mockSharingHandler);

    cl_mem memObject = image.get();
    cl_uint numObjects = 1;
    cl_mem *memObjects = &memObject;

    cl_event clEventAquire = nullptr;
    cl_int result = cmdQ.enqueueAcquireSharedObjects(numObjects, memObjects, 0, nullptr, &clEventAquire, expectedCmd);
    EXPECT_EQ(result, CL_SUCCESS);
    EXPECT_NE(clEventAquire, nullptr);
    cl_command_type actualCmd = castToObjectOrAbort<Event>(clEventAquire)->getCommandType();
    EXPECT_EQ(expectedCmd, actualCmd);
    clReleaseEvent(clEventAquire);

    cl_event clEventRelease = nullptr;
    result = cmdQ.enqueueReleaseSharedObjects(numObjects, memObjects, 0, nullptr, &clEventRelease, expectedCmd);
    EXPECT_EQ(result, CL_SUCCESS);
    EXPECT_NE(clEventRelease, nullptr);
    actualCmd = castToObjectOrAbort<Event>(clEventRelease)->getCommandType();
    EXPECT_EQ(expectedCmd, actualCmd);
    clReleaseEvent(clEventRelease);
}

TEST(CommandQueue, givenEventWaitlistWhenEnqueueReleaseSharedObjectsThenWaitForNonUserEvents) {
    auto mockDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext context;
    MockCommandQueue cmdQ(&context, mockDevice.get(), 0, false);
    MockSharingHandler *mockSharingHandler = new MockSharingHandler;

    auto image = std::unique_ptr<Image>(ImageHelperUlt<Image2dDefaults>::create(&context));
    image->setSharingHandler(mockSharingHandler);

    cl_mem memObject = image.get();
    cl_uint numObjects = 1;
    cl_mem *memObjects = &memObject;

    MockEvent<Event> events[] = {
        {&cmdQ, CL_COMMAND_READ_BUFFER, 0, 0},
        {&cmdQ, CL_COMMAND_READ_BUFFER, 0, 0},
        {&cmdQ, CL_COMMAND_READ_BUFFER, 0, 0},
    };
    MockEvent<UserEvent> userEvent{&context};
    const cl_event waitList[] = {events, events + 1, events + 2, &userEvent};
    const cl_uint waitListSize = static_cast<cl_uint>(arrayCount(waitList));

    events[0].waitReturnValue = WaitStatus::ready;
    events[1].waitReturnValue = WaitStatus::ready;
    events[2].waitReturnValue = WaitStatus::ready;
    userEvent.waitReturnValue = WaitStatus::ready;

    cmdQ.enqueueReleaseSharedObjects(numObjects, memObjects, waitListSize, waitList, nullptr, CL_COMMAND_NDRANGE_KERNEL);

    EXPECT_EQ(events[0].waitCalled, 1);
    EXPECT_EQ(events[1].waitCalled, 1);
    EXPECT_EQ(events[2].waitCalled, 1);
    EXPECT_EQ(userEvent.waitCalled, 0);
}

TEST(CommandQueue, givenEnqueueAcquireSharedObjectsWhenIncorrectArgumentsThenReturnProperError) {
    MockContext context;
    MockCommandQueue cmdQ(&context, context.getDevice(0), 0, false);

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
    MockCommandQueue cmdQ(&context, context.getDevice(0), 0, false);

    cl_uint numObjects = 0;
    cl_mem *memObjects = nullptr;

    cl_int result = cmdQ.enqueueReleaseSharedObjects(numObjects, memObjects, 0, nullptr, nullptr, 0);
    EXPECT_EQ(result, CL_SUCCESS);
}

TEST(CommandQueue, givenEnqueueReleaseSharedObjectsWhenIncorrectArgumentsThenReturnProperError) {
    MockContext context;
    MockCommandQueue cmdQ(&context, context.getDevice(0), 0, false);

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

HWTEST_F(CommandQueueCommandStreamTest, WhenSetupDebugSurfaceIsCalledThenSurfaceStateIsCorrectlySet) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    MockProgram program(toClDeviceVector(*pClDevice));
    std::unique_ptr<MockDebugKernel> kernel(MockKernel::create<MockDebugKernel>(*pDevice, &program));
    MockCommandQueue cmdQ(context.get(), pClDevice, 0, false);

    const auto &systemThreadSurfaceAddress = kernel->getAllocatedKernelInfo()->kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful;
    kernel->setSshLocal(nullptr, sizeof(RENDER_SURFACE_STATE) + systemThreadSurfaceAddress);
    auto &commandStreamReceiver = cmdQ.getGpgpuCommandStreamReceiver();

    cmdQ.getGpgpuCommandStreamReceiver().allocateDebugSurface(MemoryConstants::pageSize);
    cmdQ.setupDebugSurface(kernel.get());

    auto debugSurface = commandStreamReceiver.getDebugSurfaceAllocation();
    ASSERT_NE(nullptr, debugSurface);
    RENDER_SURFACE_STATE *surfaceState = (RENDER_SURFACE_STATE *)kernel->getSurfaceStateHeap();
    EXPECT_EQ(debugSurface->getGpuAddress(), surfaceState->getSurfaceBaseAddress());
}

HWTEST_F(CommandQueueCommandStreamTest, WhenSetupDebugSurfaceIsCalledThenDebugSurfaceIsReused) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    MockProgram program(toClDeviceVector(*pClDevice));
    std::unique_ptr<MockDebugKernel> kernel(MockKernel::create<MockDebugKernel>(*pDevice, &program));
    MockCommandQueue cmdQ(context.get(), pClDevice, 0, false);

    const auto &systemThreadSurfaceAddress = kernel->getAllocatedKernelInfo()->kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful;
    kernel->setSshLocal(nullptr, sizeof(RENDER_SURFACE_STATE) + systemThreadSurfaceAddress);
    auto &commandStreamReceiver = cmdQ.getGpgpuCommandStreamReceiver();
    commandStreamReceiver.allocateDebugSurface(MemoryConstants::pageSize);
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
    EXPECT_EQ(1, context->getRefInternalCount()); // NOLINT(clang-analyzer-cplusplus.NewDelete)
}

TEST(CommandQueuePropertiesTests, whenGetEngineIsCalledThenQueueEngineIsReturned) {
    MockCommandQueue queue;
    EngineControl engineControl;
    queue.gpgpuEngine = &engineControl;
    EXPECT_EQ(queue.gpgpuEngine, &queue.getGpgpuEngine());
}

TEST(CommandQueue, GivenCommandQueueWhenEnqueueResourceBarrierCalledThenSuccessReturned) {
    MockContext context;
    MockCommandQueue cmdQ(&context, context.getDevice(0), 0, false);

    cl_int result = cmdQ.enqueueResourceBarrier(
        nullptr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, result);
}

TEST(CommandQueue, GivenCommandQueueWhenCheckingIfIsCacheFlushCommandCalledThenFalseReturned) {
    MockContext context;
    MockCommandQueue cmdQ(&context, context.getDevice(0), 0, false);

    bool isCommandCacheFlush = cmdQ.isCacheFlushCommand(0u);
    EXPECT_FALSE(isCommandCacheFlush);
}

TEST(CommandQueue, givenBlitterOperationsSupportedWhenCreatingQueueThenTimestampPacketIsCreated) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableTimestampPacket.set(0);

    MockContext context{};
    HardwareInfo *hwInfo = context.getDevice(0)->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto &productHelper = context.getDevice(0)->getProductHelper();
    if (!productHelper.isBlitterFullySupported(*defaultHwInfo.get())) {
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
    alloc.memoryPool = MemoryPool::system4KBPages;
    CsrSelectionArgs selectionArgs{CL_COMMAND_READ_BUFFER, &multiAlloc, &multiAlloc, 0u, nullptr};

    queue.isCopyOnly = false;
    EXPECT_EQ(queue.getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled(), queue.blitEnqueueAllowed(selectionArgs));

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
    alloc.memoryPool = MemoryPool::system4KBPages;

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
    debugManager.flags.EnableBlitterForEnqueueOperations.set(1);
    debugManager.flags.EnableBlitterForEnqueueImageOperations.set(1);

    MockContext context{};
    MockCommandQueue queue(&context, context.getDevice(0), 0, false);
    if (queue.countBcsEngines() == 0) {
        queue.bcsEngines[0] = &context.getDevice(0)->getDefaultEngine();
    }

    MockImageBase image{};
    auto alloc = static_cast<MockGraphicsAllocation *>(image.getGraphicsAllocation(0));
    alloc->memoryPool = MemoryPool::system4KBPages;

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
        dstAlloc->memoryPool = MemoryPool::system4KBPages;

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
    alloc.memoryPool = MemoryPool::system4KBPages;

    CsrSelectionArgs args{CL_COMMAND_COPY_IMAGE_TO_BUFFER, &multiAlloc, &multiAlloc, 0u, nullptr};
    EXPECT_FALSE(queue.blitEnqueueAllowed(args));
}

TEST(CommandQueue, givenWriteToImageFromBufferWhenCallingBlitEnqueueAllowedThenReturnCorrectValue) {
    MockContext context{};
    MockCommandQueue queue(&context, context.getDevice(0), 0, false);
    if (queue.countBcsEngines() == 0) {
        queue.bcsEngines[0] = &context.getDevice(0)->getDefaultEngine();
    }

    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create(&context));
    size_t origin[3] = {0, 0, 0};
    size_t region[3] = {1, 1, 1};
    MockImageBase dstImage{};
    dstImage.associatedMemObject = buffer.get();

    CsrSelectionArgs args{CL_COMMAND_WRITE_IMAGE, nullptr, &dstImage, 0u, region, nullptr, origin};

    bool expectedValue = queue.getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled() && context.getDevice(0)->getProductHelper().isBlitterForImagesSupported();
    EXPECT_EQ(queue.blitEnqueueAllowed(args), expectedValue);
}

TEST(CommandQueue, givenLowPriorityQueueWhenBlitEnqueueAllowedIsCalledThenReturnFalse) {
    MockContext context{};
    MockCommandQueue queue(&context, context.getDevice(0), 0, false);
    queue.priority = QueuePriority::low;
    if (queue.countBcsEngines() == 0) {
        queue.bcsEngines[0] = &context.getDevice(0)->getDefaultEngine();
    }

    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create(&context));
    size_t origin[3] = {0, 0, 0};
    size_t region[3] = {1, 1, 1};
    MockImageBase dstImage{};
    dstImage.associatedMemObject = buffer.get();

    CsrSelectionArgs args{CL_COMMAND_WRITE_IMAGE, nullptr, &dstImage, 0u, region, nullptr, origin};

    EXPECT_FALSE(queue.blitEnqueueAllowed(args));
}

TEST(CommandQueue, givenBufferWhenMultiStorageIsNotSetThenDontRequireMigrations) {
    MockDefaultContext context{true};
    MockCommandQueue queue(&context, context.getDevice(1), 0, false);
    MockCommandStreamReceiver csr(*context.getDevice(1)->getExecutionEnvironment(), context.getDevice(1)->getRootDeviceIndex(), 1);

    ASSERT_TRUE(context.getRootDeviceIndices().size() > 1);

    std::unique_ptr<Buffer> srcBuffer(BufferHelper<>::create(&context));
    const_cast<MultiGraphicsAllocation &>(srcBuffer->getMultiGraphicsAllocation()).setMultiStorage(false);
    size_t size = MemoryConstants::kiloByte;
    auto dstPtr = alignedMalloc(size, MemoryConstants::cacheLineSize);

    BuiltinOpParams operationParams;
    operationParams.srcMemObj = srcBuffer.get();
    operationParams.dstPtr = dstPtr;
    operationParams.size = {size, 0, 0};

    EXPECT_FALSE(srcBuffer->getMultiGraphicsAllocation().requiresMigrations());

    queue.migrateMultiGraphicsAllocationsIfRequired(operationParams, csr);

    alignedFree(dstPtr);
}

using MultiRootDeviceCommandQueueTest = ::testing::Test;
HWTEST2_F(MultiRootDeviceCommandQueueTest, givenBuffersInLocalMemoryWhenMultiGraphicsAllocationsRequireMigrationsThenMigrateTheAllocations, MatchAny) {
    MockDefaultContext context{true};
    ASSERT_TRUE(context.getNumDevices() > 1);
    ASSERT_TRUE(context.getRootDeviceIndices().size() > 1);

    auto memoryManager = static_cast<MockMemoryManager *>(context.getMemoryManager());
    for (auto &rootDeviceIndex : context.getRootDeviceIndices()) {

        memoryManager->localMemorySupported[rootDeviceIndex] = true;
    }

    auto sourceRootDeviceIndex = context.getDevice(0)->getRootDeviceIndex();
    EXPECT_EQ(0u, sourceRootDeviceIndex);

    auto targetRootDeviceIndex = context.getDevice(1)->getRootDeviceIndex();
    EXPECT_EQ(1u, targetRootDeviceIndex);

    MockCommandQueue queue(&context, context.getDevice(1), 0, false);
    MockCommandStreamReceiver csr(*context.getDevice(1)->getExecutionEnvironment(), targetRootDeviceIndex, 1);

    std::unique_ptr<Buffer> srcBuffer(BufferHelper<>::create(&context));
    size_t size = MemoryConstants::kiloByte;
    auto dstPtr = alignedMalloc(size, MemoryConstants::cacheLineSize);

    BuiltinOpParams operationParams;
    operationParams.srcMemObj = srcBuffer.get();
    operationParams.dstPtr = dstPtr;
    operationParams.size = {size, 0, 0};

    EXPECT_TRUE(srcBuffer->getMultiGraphicsAllocation().requiresMigrations());
    ASSERT_NE(nullptr, srcBuffer->getMultiGraphicsAllocation().getMigrationSyncData());

    srcBuffer->getMultiGraphicsAllocation().getMigrationSyncData()->setCurrentLocation(sourceRootDeviceIndex);
    EXPECT_EQ(sourceRootDeviceIndex, srcBuffer->getMultiGraphicsAllocation().getMigrationSyncData()->getCurrentLocation());

    queue.migrateMultiGraphicsAllocationsIfRequired(operationParams, csr);

    EXPECT_EQ(targetRootDeviceIndex, srcBuffer->getMultiGraphicsAllocation().getMigrationSyncData()->getCurrentLocation());

    alignedFree(dstPtr);
}

template <bool blitter, bool selectBlitterWithQueueFamilies>
struct CsrSelectionCommandQueueTests : ::testing::Test {
    void SetUp() override {
        HardwareInfo hwInfo = *::defaultHwInfo;
        hwInfo.capabilityTable.blitterOperationsSupported = blitter;

        device = MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo);
        if (blitter) {
            REQUIRE_FULL_BLITTER_OR_SKIP(device->getRootDeviceEnvironment());
        }
        clDevice = std::make_unique<MockClDevice>(device);
        context = std::make_unique<MockContext>(clDevice.get());

        cl_command_queue_properties queueProperties[5] = {};
        if (selectBlitterWithQueueFamilies) {
            queueProperties[0] = CL_QUEUE_FAMILY_INTEL;
            queueProperties[1] = device->getEngineGroupIndexFromEngineGroupType(EngineGroupType::copy);
            queueProperties[2] = CL_QUEUE_INDEX_INTEL;
            queueProperties[3] = 0;
        }

        queue = std::make_unique<MockCommandQueue>(context.get(), clDevice.get(), queueProperties, false);
    }

    void TearDown() override {
        NEO::SipKernel::freeSipKernels(&device->getRootDeviceEnvironmentRef(), device->getMemoryManager());
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
    debugManager.flags.EnableBlitterForEnqueueOperations.set(1);

    MockGraphicsAllocation srcGraphicsAllocation{};
    MockGraphicsAllocation dstGraphicsAllocation{};
    MockBuffer srcMemObj{srcGraphicsAllocation};
    MockBuffer dstMemObj{dstGraphicsAllocation};

    {
        srcGraphicsAllocation.memoryPool = MemoryPool::system4KBPages;
        dstGraphicsAllocation.memoryPool = MemoryPool::system4KBPages;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        EXPECT_EQ(&queue->getGpgpuCommandStreamReceiver(), &queue->selectCsrForBuiltinOperation(args));
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::system4KBPages;
        dstGraphicsAllocation.memoryPool = MemoryPool::localMemory;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        EXPECT_EQ(&queue->getGpgpuCommandStreamReceiver(), &queue->selectCsrForBuiltinOperation(args));
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::localMemory;
        dstGraphicsAllocation.memoryPool = MemoryPool::system4KBPages;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        EXPECT_EQ(&queue->getGpgpuCommandStreamReceiver(), &queue->selectCsrForBuiltinOperation(args));
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::localMemory;
        dstGraphicsAllocation.memoryPool = MemoryPool::localMemory;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        EXPECT_EQ(&queue->getGpgpuCommandStreamReceiver(), &queue->selectCsrForBuiltinOperation(args));
    }
}

TEST_F(CsrSelectionCommandQueueWithBlitterTests, givenBlitterPresentButDisabledWithDebugFlagWhenSelectingBlitterThenReturnGpgpuCsr) {
    DebugManagerStateRestore restore{};
    debugManager.flags.EnableBlitterForEnqueueOperations.set(0);

    MockGraphicsAllocation srcGraphicsAllocation{};
    MockGraphicsAllocation dstGraphicsAllocation{};
    MockBuffer srcMemObj{srcGraphicsAllocation};
    MockBuffer dstMemObj{dstGraphicsAllocation};

    {
        srcGraphicsAllocation.memoryPool = MemoryPool::system4KBPages;
        dstGraphicsAllocation.memoryPool = MemoryPool::system4KBPages;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        EXPECT_EQ(&queue->getGpgpuCommandStreamReceiver(), &queue->selectCsrForBuiltinOperation(args));
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::system4KBPages;
        dstGraphicsAllocation.memoryPool = MemoryPool::localMemory;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        EXPECT_EQ(&queue->getGpgpuCommandStreamReceiver(), &queue->selectCsrForBuiltinOperation(args));
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::localMemory;
        dstGraphicsAllocation.memoryPool = MemoryPool::system4KBPages;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        EXPECT_EQ(&queue->getGpgpuCommandStreamReceiver(), &queue->selectCsrForBuiltinOperation(args));
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::localMemory;
        dstGraphicsAllocation.memoryPool = MemoryPool::localMemory;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        EXPECT_EQ(&queue->getGpgpuCommandStreamReceiver(), &queue->selectCsrForBuiltinOperation(args));
    }
}

TEST_F(CsrSelectionCommandQueueWithBlitterTests, givenBlitterPresentAndLocalToLocalCopyBufferCommandWhenSelectingBlitterThenReturnValueBasedOnDebugFlagAndHwPreference) {
    DebugManagerStateRestore restore{};
    debugManager.flags.EnableBlitterForEnqueueOperations.set(1);

    auto &clGfxCoreHelper = clDevice->getRootDeviceEnvironment().getHelper<ClGfxCoreHelper>();
    const bool hwPreference = clGfxCoreHelper.preferBlitterForLocalToLocalTransfers();
    const auto &hwPreferenceCsr = hwPreference ? *queue->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS) : queue->getGpgpuCommandStreamReceiver();

    MockGraphicsAllocation srcGraphicsAllocation{};
    MockGraphicsAllocation dstGraphicsAllocation{};
    MockBuffer srcMemObj{srcGraphicsAllocation};
    MockBuffer dstMemObj{dstGraphicsAllocation};

    srcGraphicsAllocation.memoryPool = MemoryPool::localMemory;
    dstGraphicsAllocation.memoryPool = MemoryPool::localMemory;
    CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};

    debugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(-1);
    EXPECT_EQ(&hwPreferenceCsr, &queue->selectCsrForBuiltinOperation(args));
    debugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(0);
    EXPECT_EQ(&queue->getGpgpuCommandStreamReceiver(), &queue->selectCsrForBuiltinOperation(args));
    debugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(1);
    EXPECT_EQ(queue->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS), &queue->selectCsrForBuiltinOperation(args));
}

TEST_F(CsrSelectionCommandQueueWithBlitterTests, givenBlitterPresentAndNotLocalToLocalCopyBufferCommandWhenSelectingCsrThenUseBcsRegardlessOfDebugFlag) {
    DebugManagerStateRestore restore{};
    debugManager.flags.EnableBlitterForEnqueueOperations.set(1);

    const auto &bcsCsr = *queue->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS);

    MockGraphicsAllocation srcGraphicsAllocation{};
    MockGraphicsAllocation dstGraphicsAllocation{};
    MockBuffer srcMemObj{srcGraphicsAllocation};
    MockBuffer dstMemObj{dstGraphicsAllocation};

    {
        srcGraphicsAllocation.memoryPool = MemoryPool::system4KBPages;
        dstGraphicsAllocation.memoryPool = MemoryPool::localMemory;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        debugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(-1);
        EXPECT_EQ(&bcsCsr, &queue->selectCsrForBuiltinOperation(args));
        debugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(0);
        EXPECT_EQ(&bcsCsr, &queue->selectCsrForBuiltinOperation(args));
        debugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(1);
        EXPECT_EQ(&bcsCsr, &queue->selectCsrForBuiltinOperation(args));
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::localMemory;
        dstGraphicsAllocation.memoryPool = MemoryPool::system4KBPages;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        debugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(-1);
        EXPECT_EQ(&bcsCsr, &queue->selectCsrForBuiltinOperation(args));
        debugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(0);
        EXPECT_EQ(&bcsCsr, &queue->selectCsrForBuiltinOperation(args));
        debugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(1);
        EXPECT_EQ(&bcsCsr, &queue->selectCsrForBuiltinOperation(args));
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::system4KBPages;
        dstGraphicsAllocation.memoryPool = MemoryPool::system4KBPages;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        debugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(-1);
        EXPECT_EQ(&bcsCsr, &queue->selectCsrForBuiltinOperation(args));
        debugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(0);
        EXPECT_EQ(&bcsCsr, &queue->selectCsrForBuiltinOperation(args));
        debugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(1);
        EXPECT_EQ(&bcsCsr, &queue->selectCsrForBuiltinOperation(args));
    }
}

TEST_F(CsrSelectionCommandQueueWithBlitterTests, givenInvalidTransferDirectionWhenSelectingCsrThenThrowError) {
    DebugManagerStateRestore restore{};
    debugManager.flags.EnableBlitterForEnqueueOperations.set(1);

    MockGraphicsAllocation srcGraphicsAllocation{};
    MockGraphicsAllocation dstGraphicsAllocation{};
    MockBuffer srcMemObj{srcGraphicsAllocation};
    MockBuffer dstMemObj{dstGraphicsAllocation};

    CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
    args.direction = static_cast<TransferDirection>(0xFF); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
    EXPECT_ANY_THROW(queue->selectCsrForBuiltinOperation(args));
}

TEST_F(CsrSelectionCommandQueueWithBlitterTests, givenBlitterAndAssignBCSAtEnqueueSetToFalseWhenSelectCsrThenDefaultBcsReturned) {
    DebugManagerStateRestore restore{};
    debugManager.flags.EnableBlitterForEnqueueOperations.set(1);
    debugManager.flags.AssignBCSAtEnqueue.set(0);

    MockGraphicsAllocation srcGraphicsAllocation{};
    MockGraphicsAllocation dstGraphicsAllocation{};
    MockBuffer srcMemObj{srcGraphicsAllocation};
    MockBuffer dstMemObj{dstGraphicsAllocation};

    CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
    args.direction = TransferDirection::localToHost;

    auto &csr = queue->selectCsrForBuiltinOperation(args);

    EXPECT_EQ(&csr, queue->getBcsCommandStreamReceiver(*queue->bcsQueueEngineType));
}

TEST_F(CsrSelectionCommandQueueWithQueueFamiliesBlitterTests, givenBlitterSelectedWithQueueFamiliesWhenSelectingBlitterThenSelectBlitter) {
    DebugManagerStateRestore restore{};

    MockGraphicsAllocation srcGraphicsAllocation{};
    MockGraphicsAllocation dstGraphicsAllocation{};
    MockBuffer srcMemObj{srcGraphicsAllocation};
    MockBuffer dstMemObj{dstGraphicsAllocation};

    {
        srcGraphicsAllocation.memoryPool = MemoryPool::system4KBPages;
        dstGraphicsAllocation.memoryPool = MemoryPool::system4KBPages;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS), &queue->selectCsrForBuiltinOperation(args));
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::system4KBPages;
        dstGraphicsAllocation.memoryPool = MemoryPool::localMemory;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS), &queue->selectCsrForBuiltinOperation(args));
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::localMemory;
        dstGraphicsAllocation.memoryPool = MemoryPool::system4KBPages;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS), &queue->selectCsrForBuiltinOperation(args));
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::localMemory;
        dstGraphicsAllocation.memoryPool = MemoryPool::localMemory;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS), &queue->selectCsrForBuiltinOperation(args));
    }
}

TEST_F(CsrSelectionCommandQueueWithQueueFamiliesBlitterTests, givenBlitterSelectedWithQueueFamiliesButDisabledWithDebugFlagWhenSelectingBlitterThenIgnoreDebugFlagAndSelectBlitter) {
    DebugManagerStateRestore restore{};
    debugManager.flags.EnableBlitterForEnqueueOperations.set(0);

    MockGraphicsAllocation srcGraphicsAllocation{};
    MockGraphicsAllocation dstGraphicsAllocation{};
    MockBuffer srcMemObj{srcGraphicsAllocation};
    MockBuffer dstMemObj{dstGraphicsAllocation};

    {
        srcGraphicsAllocation.memoryPool = MemoryPool::system4KBPages;
        dstGraphicsAllocation.memoryPool = MemoryPool::system4KBPages;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS), &queue->selectCsrForBuiltinOperation(args));
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::system4KBPages;
        dstGraphicsAllocation.memoryPool = MemoryPool::localMemory;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS), &queue->selectCsrForBuiltinOperation(args));
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::localMemory;
        dstGraphicsAllocation.memoryPool = MemoryPool::system4KBPages;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS), &queue->selectCsrForBuiltinOperation(args));
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::localMemory;
        dstGraphicsAllocation.memoryPool = MemoryPool::localMemory;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS), &queue->selectCsrForBuiltinOperation(args));
    }
}

TEST(CommandQueue, givenMipMappedImageWhenCallingBlitEnqueueImageAllowedThenCorrectResultIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableBlitterForEnqueueImageOperations.set(1);
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
    debugManager.flags.EnableBlitterForEnqueueImageOperations.set(1);
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

TEST(CommandQueue, givenImageWithDepthTypeWhenDepthNotAllowedForBlitThenBlitEnqueueForImageNotAllowed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableBlitterForEnqueueImageOperations.set(1);
    MockContext context{};
    MockCommandQueue queue(&context, context.getDevice(0), 0, false);
    auto releaseHelper = std::unique_ptr<MockReleaseHelper>(new MockReleaseHelper());
    releaseHelper->isBlitImageAllowedForDepthFormatResult = false;
    context.getDevice(0)->getDevice().getRootDeviceEnvironmentRef().releaseHelper.reset(releaseHelper.release());
    size_t correctRegion[3] = {10u, 10u, 0};
    size_t correctOrigin[3] = {1u, 1u, 0};
    MockImageBase image;
    image.imageFormat = {CL_DEPTH, CL_UNSIGNED_INT8};

    EXPECT_FALSE(queue.blitEnqueueImageAllowed(correctOrigin, correctRegion, image));
}

TEST(CommandQueue, givenImageWithNotDepthTypeWhenDepthNotAllowedForBlitThenBlitEnqueueForImageIsAllowed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableBlitterForEnqueueImageOperations.set(1);
    MockContext context{};
    MockCommandQueue queue(&context, context.getDevice(0), 0, false);
    auto releaseHelper = std::unique_ptr<MockReleaseHelper>(new MockReleaseHelper());
    releaseHelper->isBlitImageAllowedForDepthFormatResult = false;
    context.getDevice(0)->getDevice().getRootDeviceEnvironmentRef().releaseHelper.reset(releaseHelper.release());
    size_t correctRegion[3] = {10u, 10u, 0};
    size_t correctOrigin[3] = {1u, 1u, 0};
    MockImageBase image;
    image.imageFormat = {CL_R, CL_UNSIGNED_INT8};

    EXPECT_TRUE(queue.blitEnqueueImageAllowed(correctOrigin, correctRegion, image));
}

TEST(CommandQueue, givenImageWithDepthTypeWhenDepthAllowedForBlitThenBlitEnqueueForImageIsAllowed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableBlitterForEnqueueImageOperations.set(1);
    MockContext context{};
    MockCommandQueue queue(&context, context.getDevice(0), 0, false);
    auto releaseHelper = std::unique_ptr<MockReleaseHelper>(new MockReleaseHelper());
    releaseHelper->isBlitImageAllowedForDepthFormatResult = true;
    context.getDevice(0)->getDevice().getRootDeviceEnvironmentRef().releaseHelper.reset(releaseHelper.release());
    size_t correctRegion[3] = {10u, 10u, 0};
    size_t correctOrigin[3] = {1u, 1u, 0};
    MockImageBase image;
    image.imageFormat = {CL_DEPTH, CL_UNSIGNED_INT8};

    EXPECT_TRUE(queue.blitEnqueueImageAllowed(correctOrigin, correctRegion, image));
}

TEST(CommandQueue, givenImageWithDepthTypeWhenReleaseHelperNotAvailableThenBlitEnqueueForImageIsAllowed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableBlitterForEnqueueImageOperations.set(1);
    MockContext context{};
    MockCommandQueue queue(&context, context.getDevice(0), 0, false);
    context.getDevice(0)->getDevice().getRootDeviceEnvironmentRef().releaseHelper.reset();
    size_t correctRegion[3] = {10u, 10u, 0};
    size_t correctOrigin[3] = {1u, 1u, 0};
    MockImageBase image;
    image.imageFormat = {CL_DEPTH, CL_UNSIGNED_INT8};

    EXPECT_TRUE(queue.blitEnqueueImageAllowed(correctOrigin, correctRegion, image));
}

TEST(CommandQueue, given64KBTileWith3DImageTypeWhenCallingBlitEnqueueImageAllowedThenCorrectResultIsReturned) {
    DebugManagerStateRestore restorer;
    MockContext context{};
    MockCommandQueue queue(&context, context.getDevice(0), 0, false);
    const auto &hwInfo = *defaultHwInfo;
    const auto &productHelper = context.getDevice(0)->getProductHelper();

    size_t correctRegion[3] = {10u, 10u, 0};
    size_t correctOrigin[3] = {1u, 1u, 0};
    std::array<std::unique_ptr<Image>, 5> images = {
        std::unique_ptr<Image>(ImageHelperUlt<Image1dDefaults>::create(&context)),
        std::unique_ptr<Image>(ImageHelperUlt<Image1dArrayDefaults>::create(&context)),
        std::unique_ptr<Image>(ImageHelperUlt<Image2dDefaults>::create(&context)),
        std::unique_ptr<Image>(ImageHelperUlt<Image2dArrayDefaults>::create(&context)),
        std::unique_ptr<Image>(ImageHelperUlt<Image3dDefaults>::create(&context))};

    for (auto blitterEnabled : {0, 1}) {
        debugManager.flags.EnableBlitterForEnqueueImageOperations.set(blitterEnabled);
        for (auto isTile64 : {0, 1}) {
            for (const auto &image : images) {
                auto imageType = image->getImageDesc().image_type;
                auto gfxAllocation = image->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex());
                auto mockGmmResourceInfo = reinterpret_cast<MockGmmResourceInfo *>(gfxAllocation->getDefaultGmm()->gmmResourceInfo.get());
                mockGmmResourceInfo->getResourceFlags()->Info.Tile64 = isTile64;

                if (isTile64 && (imageType == CL_MEM_OBJECT_IMAGE3D)) {
                    auto supportExpected = productHelper.isTile64With3DSurfaceOnBCSSupported(hwInfo) && blitterEnabled;
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
        debugManager.flags.EnableTimestampPacket.set(1);
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
    queue.setStallingCommandsOnNextFlush(true);
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
    queue.setStallingCommandsOnNextFlush(true);
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
    queue.setStallingCommandsOnNextFlush(true);
    for (auto &containers : queue.bcsTimestampPacketContainers) {
        EXPECT_TRUE(containers.lastBarrierToWaitFor.peekNodes().empty());
    }

    queue.setupBarrierTimestampForBcsEngines(aub_stream::EngineType::ENGINE_BCS, dependencies);
    EXPECT_EQ(1u, dependencies.barrierNodes.peekNodes().size());
    auto barrierNode = dependencies.barrierNodes.peekNodes()[0];
    EXPECT_EQ(1u, barrierNode->getContextEndValue(0u));
    dependencies.moveNodesToNewContainer(*queue.getDeferredTimestampPackets());
    queue.setStallingCommandsOnNextFlush(true);
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
    queue.setStallingCommandsOnNextFlush(true);

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
    queue.setStallingCommandsOnNextFlush(true);
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
    queue.setStallingCommandsOnNextFlush(true);

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
    queue.setStallingCommandsOnNextFlush(true);

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

HWTEST_F(CommandQueueWithTimestampPacketTests, givedDependencyBetweenCsrWhenPrepareDependencyUpdateCalledThenNewTagAddedToTimestampDependencies) {
    MockContext context{};
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(&context, context.getDevice(0), nullptr);
    auto dependentCsr = std::make_unique<MockCommandStreamReceiver>(*context.getDevice(0)->getExecutionEnvironment(), context.getDevice(0)->getRootDeviceIndex(), 1);
    TimestampPacketDependencies dependencies{};
    CsrDependencies csrDeps;
    csrDeps.csrWithMultiEngineDependencies.insert(dependentCsr.get());
    CsrDependencyContainer dependencyMap;
    TagAllocatorBase *allocator = mockCmdQ->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator();
    bool blockQueue = false;
    mockCmdQ->prepareCsrDependency(csrDeps, dependencyMap, dependencies, allocator, blockQueue);
    EXPECT_EQ(dependencies.multiCsrDependencies.peekNodes().size(), 1u);
}

HWTEST_F(CommandQueueWithTimestampPacketTests, givedNoDependencyBetweenCsrWhenPrepareDependencyUpdateCalledThenTagIsNotAddedToTimestampDependencies) {
    MockContext context{};
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(&context, context.getDevice(0), nullptr);
    TimestampPacketDependencies dependencies{};
    CsrDependencies csrDeps;
    CsrDependencyContainer dependencyMap;
    TagAllocatorBase *allocator = mockCmdQ->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator();
    bool blockQueue = false;
    mockCmdQ->prepareCsrDependency(csrDeps, dependencyMap, dependencies, allocator, blockQueue);
    EXPECT_EQ(dependencies.multiCsrDependencies.peekNodes().size(), 0u);
}

HWTEST_F(CommandQueueWithTimestampPacketTests, givedDependencyBetweenCsrWhenPrepareDependencyUpdateCalledForNonBlockedQueueThenSubmitDependencyUpdateCalled) {
    MockContext context{};
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(&context, context.getDevice(0), nullptr);
    auto dependentCsr = std::make_unique<MockCommandStreamReceiver>(*context.getDevice(0)->getExecutionEnvironment(), context.getDevice(0)->getRootDeviceIndex(), 1);
    TimestampPacketDependencies dependencies{};
    CsrDependencies csrDeps;
    csrDeps.csrWithMultiEngineDependencies.insert(dependentCsr.get());
    CsrDependencyContainer dependencyMap;
    TagAllocatorBase *allocator = mockCmdQ->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator();
    bool blockQueue = false;
    mockCmdQ->prepareCsrDependency(csrDeps, dependencyMap, dependencies, allocator, blockQueue);
    EXPECT_EQ(dependentCsr->submitDependencyUpdateCalledTimes, 1u);
    EXPECT_EQ(dependencyMap.size(), 0u);
}

HWTEST_F(CommandQueueWithTimestampPacketTests, givedDependencyBetweenCsrWhenPrepareDependencyUpdateCalledForBlockedQueueThenDependencyMapHasOneItem) {
    MockContext context{};
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(&context, context.getDevice(0), nullptr);
    auto dependentCsr = std::make_unique<MockCommandStreamReceiver>(*context.getDevice(0)->getExecutionEnvironment(), context.getDevice(0)->getRootDeviceIndex(), 1);
    TimestampPacketDependencies dependencies{};
    CsrDependencies csrDeps;
    csrDeps.csrWithMultiEngineDependencies.insert(dependentCsr.get());
    CsrDependencyContainer dependencyMap;
    TagAllocatorBase *allocator = mockCmdQ->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator();
    bool blockQueue = true;
    mockCmdQ->prepareCsrDependency(csrDeps, dependencyMap, dependencies, allocator, blockQueue);
    EXPECT_EQ(dependentCsr->submitDependencyUpdateCalledTimes, 0u);
    EXPECT_EQ(dependencyMap.size(), 1u);
}

HWTEST_F(CommandQueueWithTimestampPacketTests, givedDependencyBetweenCsrWhenSubmitDependencyUpdateReturnsFalseThenProcessDependencyReturnsFalse) {
    MockContext context{};
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(&context, context.getDevice(0), nullptr);
    auto dependentCsr = std::make_unique<MockCommandStreamReceiver>(*context.getDevice(0)->getExecutionEnvironment(), context.getDevice(0)->getRootDeviceIndex(), 1);
    TimestampPacketDependencies dependencies{};
    CsrDependencies csrDeps;
    csrDeps.csrWithMultiEngineDependencies.insert(dependentCsr.get());
    CsrDependencyContainer dependencyMap;
    TagAllocatorBase *allocator = mockCmdQ->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator();
    bool blockQueue = false;
    dependentCsr->submitDependencyUpdateReturnValue = false;
    EXPECT_FALSE(mockCmdQ->prepareCsrDependency(csrDeps, dependencyMap, dependencies, allocator, blockQueue));
}

using KernelExecutionTypesTests = DispatchFlagsTests;
HWTEST_F(KernelExecutionTypesTests, givenConcurrentKernelWhileDoingNonBlockedEnqueueThenCorrectKernelTypeIsSetInCSR) {

    using CsrType = MockCsrHw2<FamilyType>;
    setUpImpl<CsrType>();

    auto &compilerProductHelper = device->getCompilerProductHelper();
    if (compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo)) {
        GTEST_SKIP();
    }
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    MockKernelWithInternals mockKernelWithInternals(*device.get());
    auto pKernel = mockKernelWithInternals.mockKernel;

    pKernel->setKernelExecutionType(CL_KERNEL_EXEC_INFO_CONCURRENT_TYPE_INTEL);
    size_t gws[3] = {63, 0, 0};

    mockCmdQ->enqueueKernel(pKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    auto &mockCsr = device->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(mockCsr.lastKernelExecutionType, KernelExecutionType::concurrent);
}

HWTEST_F(KernelExecutionTypesTests, givenKernelWithDifferentExecutionTypeWhileDoingNonBlockedEnqueueThenKernelTypeInCSRIsChanging) {

    using CsrType = MockCsrHw2<FamilyType>;
    setUpImpl<CsrType>();

    auto &compilerProductHelper = device->getCompilerProductHelper();
    if (compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo)) {
        GTEST_SKIP();
    }
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    MockKernelWithInternals mockKernelWithInternals(*device.get());
    auto pKernel = mockKernelWithInternals.mockKernel;
    size_t gws[3] = {63, 0, 0};
    auto &mockCsr = device->getUltCommandStreamReceiver<FamilyType>();

    mockCsr.feSupportFlags.computeDispatchAllWalker = true;

    pKernel->setKernelExecutionType(CL_KERNEL_EXEC_INFO_CONCURRENT_TYPE_INTEL);
    mockCmdQ->enqueueKernel(pKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(mockCsr.lastKernelExecutionType, KernelExecutionType::concurrent);

    mockCmdQ->enqueueMarkerWithWaitList(0, nullptr, nullptr);
    EXPECT_EQ(mockCsr.lastKernelExecutionType, KernelExecutionType::concurrent);

    pKernel->setKernelExecutionType(CL_KERNEL_EXEC_INFO_DEFAULT_TYPE_INTEL);
    mockCmdQ->enqueueKernel(pKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_EQ(mockCsr.lastKernelExecutionType, KernelExecutionType::defaultType);
}

HWTEST_F(KernelExecutionTypesTests, givenConcurrentKernelWhileDoingBlockedEnqueueThenCorrectKernelTypeIsSetInCSR) {

    using CsrType = MockCsrHw2<FamilyType>;
    setUpImpl<CsrType>();

    auto &compilerProductHelper = device->getCompilerProductHelper();
    if (compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo)) {
        GTEST_SKIP();
    }
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
    EXPECT_EQ(mockCsr.lastKernelExecutionType, KernelExecutionType::concurrent);
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
    class MockGfxCoreHelper : public GfxCoreHelperHw<GfxFamily> {
      public:
        const EngineInstancesContainer getGpgpuEngineInstances(const RootDeviceEnvironment &rootDeviceEnvironment) const override {
            EngineInstancesContainer result{};
            for (int i = 0; i < rcsCount; i++) {
                result.push_back({aub_stream::ENGINE_RCS, EngineUsage::regular});
            }
            for (int i = 0; i < ccsCount; i++) {
                result.push_back({aub_stream::ENGINE_CCS, EngineUsage::regular});
            }
            for (int i = 0; i < bcsCount; i++) {
                result.push_back({aub_stream::ENGINE_BCS, EngineUsage::regular});
            }

            return result;
        }

        EngineGroupType getEngineGroupType(aub_stream::EngineType engineType, EngineUsage engineUsage, const HardwareInfo &hwInfo) const override {
            switch (engineType) {
            case aub_stream::ENGINE_RCS:
                return EngineGroupType::renderCompute;
            case aub_stream::ENGINE_CCS:
            case aub_stream::ENGINE_CCS1:
            case aub_stream::ENGINE_CCS2:
            case aub_stream::ENGINE_CCS3:
                return EngineGroupType::compute;
            case aub_stream::ENGINE_BCS:
                return EngineGroupType::copy;
            default:
                UNRECOVERABLE_IF(true);
            }
        }
    };

    template <typename GfxFamily, typename GfxCoreHelperType>
    auto overrideGfxCoreHelper(RootDeviceEnvironment &rootDeviceEnvironment) {
        return RAIIGfxCoreHelperFactory<GfxCoreHelperType>{rootDeviceEnvironment};
    }
};

HWTEST_F(CommandQueueOnSpecificEngineTests, givenMultipleFamiliesWhenCreatingQueueOnSpecificEngineThenUseCorrectEngine) {

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    MockExecutionEnvironment mockExecutionEnvironment{&hwInfo};
    auto raiiGfxCoreHelper = overrideGfxCoreHelper<FamilyType, MockGfxCoreHelper<FamilyType, 0, 1, 1>>(*mockExecutionEnvironment.rootDeviceEnvironments[0]);

    MockDevice *device = MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0);
    MockClDevice clDevice{device};
    MockContext context{&clDevice};
    cl_command_queue_properties properties[5] = {};

    fillProperties(properties, 0, 0);
    EngineControl &engineCcs = context.getDevice(0)->getEngine(aub_stream::ENGINE_CCS, EngineUsage::regular);
    MockCommandQueue queueRcs(&context, context.getDevice(0), properties, false);
    EXPECT_EQ(engineCcs.osContext, queueRcs.getGpgpuEngine().osContext);
    EXPECT_EQ(engineCcs.commandStreamReceiver, queueRcs.getGpgpuEngine().commandStreamReceiver);
    EXPECT_FALSE(queueRcs.isCopyOnly);
    EXPECT_TRUE(queueRcs.isQueueFamilySelected());
    EXPECT_EQ(properties[1], queueRcs.getQueueFamilyIndex());
    EXPECT_EQ(properties[3], queueRcs.getQueueIndexWithinFamily());

    fillProperties(properties, 1, 0);
    EngineControl &engineBcs = context.getDevice(0)->getEngine(aub_stream::ENGINE_BCS, EngineUsage::regular);

    device->disableSecondaryEngines = true;
    MockCommandQueue queueBcs(&context, context.getDevice(0), properties, false);
    EXPECT_EQ(engineBcs.commandStreamReceiver, queueBcs.getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS));
    EXPECT_TRUE(queueBcs.isCopyOnly);
    EXPECT_TRUE(queueBcs.isQueueFamilySelected());
    EXPECT_EQ(properties[1], queueBcs.getQueueFamilyIndex());
    EXPECT_EQ(properties[3], queueBcs.getQueueIndexWithinFamily());
    EXPECT_NE(nullptr, queueBcs.getTimestampPacketContainer());
}

HWTEST_F(CommandQueueOnSpecificEngineTests, givenContextGroupWhenCreatingQueuesThenAssignDifferentCsr) {
    DebugManagerStateRestore restore;
    debugManager.flags.ContextGroupSize.set(8);

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    MockExecutionEnvironment mockExecutionEnvironment{&hwInfo};
    auto raiiGfxCoreHelper = overrideGfxCoreHelper<FamilyType, MockGfxCoreHelper<FamilyType, 0, 1, 1>>(*mockExecutionEnvironment.rootDeviceEnvironments[0]);

    MockDevice *device = MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0);
    MockClDevice clDevice{device};
    MockContext context{&clDevice};
    cl_command_queue_properties properties[5] = {};

    fillProperties(properties, 1, 0);

    MockCommandQueue queueBcs0(&context, context.getDevice(0), properties, false);
    MockCommandQueue queueBcs1(&context, context.getDevice(0), properties, false);
    MockCommandQueue queueBcs2(&context, context.getDevice(0), properties, false);

    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS, queueBcs1.bcsEngines[0]->osContext->getEngineType());
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS, queueBcs2.bcsEngines[0]->osContext->getEngineType());

    auto csr1 = static_cast<UltCommandStreamReceiver<FamilyType> *>(queueBcs1.bcsEngines[0]->commandStreamReceiver);
    auto csr2 = static_cast<UltCommandStreamReceiver<FamilyType> *>(queueBcs2.bcsEngines[0]->commandStreamReceiver);

    EXPECT_NE(csr1, csr2);

    EXPECT_NE(nullptr, csr1->primaryCsr);
    EXPECT_NE(nullptr, csr2->primaryCsr);

    EXPECT_EQ(csr1->primaryCsr, csr2->primaryCsr);
}

HWTEST_F(CommandQueueOnSpecificEngineTests, givenDebugFlagSetWhenSubmittingThenDeferFirstDeviceSubmission) {
    DebugManagerStateRestore restore;
    debugManager.flags.ContextGroupSize.set(8);
    debugManager.flags.DeferStateInitSubmissionToFirstRegularUsage.set(1);

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    MockExecutionEnvironment mockExecutionEnvironment{&hwInfo};
    auto raiiGfxCoreHelper = overrideGfxCoreHelper<FamilyType, MockGfxCoreHelper<FamilyType, 0, 1, 1>>(*mockExecutionEnvironment.rootDeviceEnvironments[0]);

    MockDevice *device = MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0);
    MockClDevice clDevice{device};
    MockContext context{&clDevice};
    cl_command_queue_properties computeProperties[5] = {};
    cl_command_queue_properties copyProperties[5] = {};

    fillProperties(computeProperties, 1, 0);
    fillProperties(copyProperties, 1, 0);

    auto buffer = std::unique_ptr<Buffer>{BufferHelper<>::create(&context)};

    MockCommandQueueHw<FamilyType> queueBcs0(&context, context.getDevice(0), copyProperties, false);
    MockCommandQueueHw<FamilyType> queueBcs1(&context, context.getDevice(0), copyProperties, false);
    MockCommandQueueHw<FamilyType> queueCompute0(&context, context.getDevice(0), computeProperties, false);
    MockCommandQueueHw<FamilyType> queueCompute1(&context, context.getDevice(0), computeProperties, false);

    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS, queueBcs0.bcsEngines[0]->osContext->getEngineType());
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS, queueBcs1.bcsEngines[0]->osContext->getEngineType());
    EXPECT_TRUE(EngineHelpers::isComputeEngine(queueCompute0.getGpgpuCommandStreamReceiver().getOsContext().getEngineType()));
    EXPECT_TRUE(EngineHelpers::isComputeEngine(queueCompute1.getGpgpuCommandStreamReceiver().getOsContext().getEngineType()));

    auto copyCsr1 = static_cast<UltCommandStreamReceiver<FamilyType> *>(queueBcs1.bcsEngines[0]->commandStreamReceiver);
    auto computeCsr1 = static_cast<UltCommandStreamReceiver<FamilyType> *>(&queueCompute1.getGpgpuCommandStreamReceiver());

    auto copyPrimaryCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(copyCsr1->primaryCsr);
    auto computePrimaryCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(computeCsr1->primaryCsr);

    EXPECT_EQ(0u, copyPrimaryCsr->peekTaskCount());
    EXPECT_EQ(0u, computePrimaryCsr->peekTaskCount());
    EXPECT_EQ(0u, copyPrimaryCsr->initializeDeviceWithFirstSubmissionCalled);
    EXPECT_EQ(0u, computePrimaryCsr->initializeDeviceWithFirstSubmissionCalled);

    queueBcs1.enqueueCopyBuffer(buffer.get(), buffer.get(), 0, 0, 1, 0, nullptr, nullptr);

    EXPECT_EQ(1u, copyPrimaryCsr->peekTaskCount());
    EXPECT_EQ(0u, computePrimaryCsr->peekTaskCount());
    EXPECT_EQ(1u, copyPrimaryCsr->initializeDeviceWithFirstSubmissionCalled);
    EXPECT_EQ(0u, computePrimaryCsr->initializeDeviceWithFirstSubmissionCalled);

    MockKernelWithInternals mockKernelWithInternals(clDevice);
    size_t gws[3] = {1, 0, 0};

    queueCompute1.enqueueKernel(mockKernelWithInternals.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_EQ(1u, copyPrimaryCsr->peekTaskCount());
    EXPECT_EQ(1u, computePrimaryCsr->peekTaskCount());
    EXPECT_EQ(1u, copyPrimaryCsr->initializeDeviceWithFirstSubmissionCalled);
    EXPECT_EQ(1u, computePrimaryCsr->initializeDeviceWithFirstSubmissionCalled);
}

HWTEST_F(CommandQueueOnSpecificEngineTests, givenRootDeviceAndMultipleFamiliesWhenCreatingQueueOnSpecificEngineThenUseDefaultEngine) {
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo->capabilityTable.blitterOperationsSupported = true;
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto raiiGfxCoreHelper = overrideGfxCoreHelper<FamilyType, MockGfxCoreHelper<FamilyType, 0, 1, 1>>(*mockExecutionEnvironment.rootDeviceEnvironments[0]);
    UltClDeviceFactory deviceFactory{1, 2};
    MockContext context{deviceFactory.rootDevices[0]};
    cl_command_queue_properties properties[5] = {};

    fillProperties(properties, 0, 0);
    EngineControl &defaultEngine = context.getDevice(0)->getDefaultEngine();

    deviceFactory.rootDevices[0]->device.disableSecondaryEngines = true;
    MockCommandQueue defaultQueue(&context, context.getDevice(0), properties, false);
    EXPECT_EQ(&defaultEngine, &defaultQueue.getGpgpuEngine());
    EXPECT_FALSE(defaultQueue.isCopyOnly);
    EXPECT_TRUE(defaultQueue.isQueueFamilySelected());
    EXPECT_EQ(properties[1], defaultQueue.getQueueFamilyIndex());
    EXPECT_EQ(properties[3], defaultQueue.getQueueIndexWithinFamily());
}

HWTEST_F(CommandQueueOnSpecificEngineTests, givenSubDeviceAndMultipleFamiliesWhenCreatingQueueOnSpecificEngineThenUseDefaultEngine) {
    DebugManagerStateRestore restore{};
    debugManager.flags.ContextGroupSize.set(0);

    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo->capabilityTable.blitterOperationsSupported = true;
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto raiiGfxCoreHelper = overrideGfxCoreHelper<FamilyType, MockGfxCoreHelper<FamilyType, 0, 1, 1>>(*mockExecutionEnvironment.rootDeviceEnvironments[0]);

    UltClDeviceFactory deviceFactory{1, 2};
    MockContext context{deviceFactory.subDevices[0]};
    cl_command_queue_properties properties[5] = {};

    fillProperties(properties, 0, 0);
    EngineControl &engineCcs = context.getDevice(0)->getEngine(aub_stream::ENGINE_CCS, EngineUsage::regular);
    MockCommandQueue queueRcs(&context, context.getDevice(0), properties, false);
    EXPECT_EQ(engineCcs.osContext, queueRcs.getGpgpuEngine().osContext);
    EXPECT_FALSE(queueRcs.isCopyOnly);
    EXPECT_TRUE(queueRcs.isQueueFamilySelected());
    EXPECT_EQ(properties[1], queueRcs.getQueueFamilyIndex());
    EXPECT_EQ(properties[3], queueRcs.getQueueIndexWithinFamily());

    fillProperties(properties, 1, 0);
    EngineControl &engineBcs = context.getDevice(0)->getEngine(aub_stream::ENGINE_BCS, EngineUsage::regular);

    MockCommandQueue queueBcs(&context, context.getDevice(0), properties, false);
    EXPECT_EQ(engineBcs.commandStreamReceiver, queueBcs.getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS));
    EXPECT_TRUE(queueBcs.isCopyOnly);
    EXPECT_NE(nullptr, queueBcs.getTimestampPacketContainer());
    EXPECT_TRUE(queueBcs.isQueueFamilySelected());
    EXPECT_EQ(properties[1], queueBcs.getQueueFamilyIndex());
    EXPECT_EQ(properties[3], queueBcs.getQueueIndexWithinFamily());
}

HWTEST_F(CommandQueueOnSpecificEngineTests, givenBcsFamilySelectedWhenCreatingQueueOnSpecificEngineThenInitializeBcsProperly) {
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo->capabilityTable.blitterOperationsSupported = true;
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto raiiGfxCoreHelper = overrideGfxCoreHelper<FamilyType, MockGfxCoreHelper<FamilyType, 0, 0, 1>>(*mockExecutionEnvironment.rootDeviceEnvironments[0]);

    auto mockDevice = std::make_unique<MockClDevice>(MockClDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext context{mockDevice.get(), false};
    cl_command_queue_properties properties[5] = {};

    fillProperties(properties, 0, 0);
    EngineControl &engineBcs = context.getDevice(0)->getEngine(aub_stream::ENGINE_BCS, EngineUsage::regular);

    mockDevice->device.disableSecondaryEngines = true;
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
    debugManager.flags.DeferOsContextInitialization.set(1);
    debugManager.flags.NodeOrdinal.set(static_cast<int32_t>(aub_stream::EngineType::ENGINE_CCS));

    MockExecutionEnvironment mockExecutionEnvironment{};

    auto raiiGfxCoreHelper = overrideGfxCoreHelper<FamilyType, MockGfxCoreHelper<FamilyType, 1, 1, 1>>(*mockExecutionEnvironment.rootDeviceEnvironments[0]);
    MockContext context{};
    cl_command_queue_properties properties[5] = {};

    OsContext &osContext = *context.getDevice(0)->getEngine(aub_stream::ENGINE_RCS, EngineUsage::regular).osContext;
    EXPECT_FALSE(osContext.isInitialized());

    debugManager.flags.NodeOrdinal.set(static_cast<int32_t>(aub_stream::EngineType::ENGINE_RCS));
    const auto ccsFamilyIndex = static_cast<cl_uint>(context.getDevice(0)->getDevice().getEngineGroupIndexFromEngineGroupType(EngineGroupType::compute));
    fillProperties(properties, ccsFamilyIndex, 0);
    MockCommandQueueHw<FamilyType> queue(&context, context.getDevice(0), properties);
    ASSERT_EQ(&osContext, queue.gpgpuEngine->osContext);
    EXPECT_TRUE(osContext.isInitialized());
}

HWTEST_F(CommandQueueOnSpecificEngineTests, givenNotInitializedCcsOsContextWhenCreatingQueueThenInitializeOsContext) {
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo->capabilityTable.blitterOperationsSupported = true;
    DebugManagerStateRestore restore{};
    debugManager.flags.NodeOrdinal.set(static_cast<int32_t>(aub_stream::EngineType::ENGINE_RCS));
    debugManager.flags.DeferOsContextInitialization.set(1);

    MockExecutionEnvironment mockExecutionEnvironment{};

    auto raiiGfxCoreHelper = overrideGfxCoreHelper<FamilyType, MockGfxCoreHelper<FamilyType, 1, 1, 1>>(*mockExecutionEnvironment.rootDeviceEnvironments[0]);
    MockContext context{};
    cl_command_queue_properties properties[5] = {};

    auto &compilerProductHelper = context.getDevice(0)->getCompilerProductHelper();
    auto heaplessModeEnabled = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo);
    auto heaplessStateInit = compilerProductHelper.isHeaplessStateInitEnabled(heaplessModeEnabled);

    OsContext &osContext = *context.getDevice(0)->getEngine(aub_stream::ENGINE_CCS, EngineUsage::regular).osContext;
    if (heaplessStateInit) {
        EXPECT_TRUE(osContext.isInitialized());
    } else {
        EXPECT_FALSE(osContext.isInitialized());
    }

    debugManager.flags.NodeOrdinal.set(static_cast<int32_t>(aub_stream::EngineType::ENGINE_CCS));
    const auto rcsFamilyIndex = static_cast<cl_uint>(context.getDevice(0)->getDevice().getEngineGroupIndexFromEngineGroupType(EngineGroupType::renderCompute));
    fillProperties(properties, rcsFamilyIndex, 0);
    MockCommandQueueHw<FamilyType> queue(&context, context.getDevice(0), properties);

    if (queue.gpgpuEngine->osContext->getPrimaryContext() == nullptr) {
        ASSERT_EQ(&osContext, queue.gpgpuEngine->osContext);
    }

    EXPECT_TRUE(queue.gpgpuEngine->osContext->isInitialized());
}

TEST_F(MultiTileFixture, givenMultiSubDeviceAndCommandQueueUsingMainCopyEngineWhenReleaseMainCopyEngineThenDeviceAndSubdeviceSelectorReset) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];
    REQUIRE_BLITTER_OR_SKIP(rootDeviceEnvironment);

    auto device = platform()->getClDevice(0);
    auto subDevice = device->getSubDevice(0);

    const cl_device_id deviceId = device;
    auto returnStatus = CL_SUCCESS;
    auto context = clCreateContext(nullptr, 1, &deviceId, nullptr, nullptr, &returnStatus);
    EXPECT_EQ(CL_SUCCESS, returnStatus);
    EXPECT_NE(nullptr, context);

    auto commandQueue = clCreateCommandQueueWithProperties(context, device, nullptr, &returnStatus);
    EXPECT_EQ(CL_SUCCESS, returnStatus);
    EXPECT_NE(nullptr, commandQueue);

    auto neoQueue = castToObject<CommandQueue>(commandQueue);

    device->getSelectorCopyEngine().isMainUsed.store(true);
    subDevice->getSelectorCopyEngine().isMainUsed.store(true);

    neoQueue->releaseMainCopyEngine();

    EXPECT_FALSE(device->getSelectorCopyEngine().isMainUsed.load());
    EXPECT_FALSE(subDevice->getSelectorCopyEngine().isMainUsed.load());

    clReleaseCommandQueue(commandQueue);
    clReleaseContext(context);
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
        device = MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get());
        typeUsageRcs.first = EngineHelpers::remapEngineTypeToHwSpecific(typeUsageRcs.first, device->getRootDeviceEnvironment());

        auto copyEngineGroup = std::find_if(device->regularEngineGroups.begin(), device->regularEngineGroups.end(), [](const auto &engineGroup) {
            return engineGroup.engineGroupType == EngineGroupType::copy;
        });
        if (copyEngineGroup == device->regularEngineGroups.end()) {
            GTEST_SKIP();
        }
        device->regularEngineGroups.clear();
        device->allEngines.clear();

        device->createEngine(typeUsageRcs);
        device->createEngine(typeUsageBcs);
        bcsEngine = &device->getAllEngines().back();

        clDevice = std::make_unique<MockClDevice>(device);

        context = std::make_unique<MockContext>(clDevice.get());

        properties[1] = device->getEngineGroupIndexFromEngineGroupType(EngineGroupType::copy);
    }

    void TearDown() override {
        NEO::SipKernel::freeSipKernels(&device->getRootDeviceEnvironmentRef(), device->getMemoryManager());
    }

    EngineTypeUsage typeUsageBcs = EngineTypeUsage{aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular};
    EngineTypeUsage typeUsageRcs = EngineTypeUsage{aub_stream::EngineType::ENGINE_RCS, EngineUsage::regular};

    std::unique_ptr<MockClDevice> clDevice{};
    std::unique_ptr<MockContext> context{};
    std::unique_ptr<MockCommandQueue> queue{};
    const EngineControl *bcsEngine = nullptr;
    MockDevice *device = nullptr;

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

HWTEST_F(CopyOnlyQueueTests, givenBcsSelectedWhenEnqueuingCopyThenRegisterClient) {
    auto srcBuffer = std::unique_ptr<Buffer>{BufferHelper<>::create(context.get())};
    auto dstBuffer = std::unique_ptr<Buffer>{BufferHelper<>::create(context.get())};

    auto bcsCSr = bcsEngine->commandStreamReceiver;

    auto baseNumClients = bcsCSr->getNumClients();

    {
        MockCommandQueueHw<FamilyType> queue0(context.get(), clDevice.get(), properties);
        EXPECT_EQ(baseNumClients, bcsCSr->getNumClients());

        MockCommandQueueHw<FamilyType> queue1(context.get(), clDevice.get(), properties);
        EXPECT_EQ(baseNumClients, bcsCSr->getNumClients());

        EXPECT_EQ(CL_SUCCESS, queue1.enqueueCopyBuffer(srcBuffer.get(), dstBuffer.get(), 0, 0, 1, 0, nullptr, nullptr));
        EXPECT_EQ(baseNumClients + 1, bcsCSr->getNumClients());

        EXPECT_EQ(CL_SUCCESS, queue1.enqueueCopyBuffer(srcBuffer.get(), dstBuffer.get(), 0, 0, 1, 0, nullptr, nullptr));
        EXPECT_EQ(baseNumClients + 1, bcsCSr->getNumClients());

        {
            MockCommandQueueHw<FamilyType> queue2(context.get(), clDevice.get(), properties);
            EXPECT_EQ(baseNumClients + 1, bcsCSr->getNumClients());

            EXPECT_EQ(CL_SUCCESS, queue2.enqueueCopyBuffer(srcBuffer.get(), dstBuffer.get(), 0, 0, 1, 0, nullptr, nullptr));
            EXPECT_EQ(baseNumClients + 2, bcsCSr->getNumClients());

            EXPECT_EQ(CL_SUCCESS, queue2.enqueueCopyBuffer(srcBuffer.get(), dstBuffer.get(), 0, 0, 1, 0, nullptr, nullptr));
            EXPECT_EQ(baseNumClients + 2, bcsCSr->getNumClients());
        }

        EXPECT_EQ(baseNumClients + 1, bcsCSr->getNumClients());
    }

    EXPECT_EQ(baseNumClients, bcsCSr->getNumClients());
}

HWTEST_F(CopyOnlyQueueTests, givenBlitterEnabledWhenCreatingBcsCommandQueueThenReturnSuccess) {
    DebugManagerStateRestore restore{};
    debugManager.flags.EnableBlitterOperationsSupport.set(1);

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

    bool renderStreamerFound = false;
    for (auto &engine : device->allEngines) {
        if (engine.osContext->getEngineType() == EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_RCS, device->getRootDeviceEnvironment())) {
            renderStreamerFound = true;
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
    addTestValueIfAvailable(commandQueueTestValues, EngineGroupType::renderCompute, 0, EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_RCS, device->getRootDeviceEnvironment()), renderStreamerFound);
    addTestValueIfAvailable(commandQueueTestValues, EngineGroupType::compute, 0, aub_stream::ENGINE_CCS, ccsFound);
    addTestValueIfAvailable(commandQueueTestValues, EngineGroupType::compute, 1, aub_stream::ENGINE_CCS1, ccsInstances.CCS1Enabled);
    addTestValueIfAvailable(commandQueueTestValues, EngineGroupType::compute, 2, aub_stream::ENGINE_CCS2, ccsInstances.CCS2Enabled);
    addTestValueIfAvailable(commandQueueTestValues, EngineGroupType::compute, 3, aub_stream::ENGINE_CCS3, ccsInstances.CCS3Enabled);

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
    ASSERT_NE(nullptr, &queue.getGpgpuEngine());
    EXPECT_EQ(rootCsr->isMultiOsContextCapable(), queue.getGpgpuCommandStreamReceiver().isMultiOsContextCapable());
    EXPECT_EQ(rootCsr, queue.gpgpuEngine->commandStreamReceiver);
}

TEST_F(MultiTileFixture, givenDefaultContextWithSubdeviceWhenQueueIsCreatedThenQueueIsNotMultiEngine) {
    auto subdevice = platform()->getClDevice(0)->getSubDevice(0);
    MockContext context(subdevice);
    context.contextType = ContextType::CONTEXT_TYPE_DEFAULT;

    MockCommandQueue queue(&context, subdevice, nullptr, false);
    ASSERT_NE(nullptr, &queue.getGpgpuEngine());
    EXPECT_FALSE(queue.getGpgpuCommandStreamReceiver().isMultiOsContextCapable());
}

TEST_F(MultiTileFixture, givenUnrestrictiveContextWithRootDeviceWhenQueueIsCreatedThenQueueIsMultiEngine) {
    auto rootDevice = platform()->getClDevice(0);
    MockContext context(rootDevice);
    context.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;

    auto rootCsr = rootDevice->getDefaultEngine().commandStreamReceiver;

    MockCommandQueue queue(&context, rootDevice, nullptr, false);
    ASSERT_NE(nullptr, &queue.getGpgpuEngine());
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
    ASSERT_NE(nullptr, &queue.getGpgpuEngine());
    EXPECT_EQ(rootCsr->isMultiOsContextCapable(), queue.getGpgpuCommandStreamReceiver().isMultiOsContextCapable());
    EXPECT_EQ(rootCsr, queue.gpgpuEngine->commandStreamReceiver);
}

HWTEST_F(CsrSelectionCommandQueueWithBlitterTests, givenBlitterAndBcsEnqueueNotPreferredThenOverrideIfConditionsMet) {
    auto ccsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(&queue->getGpgpuCommandStreamReceiver());
    auto bcsCsr = queue->getBcsCommandStreamReceiver(*queue->bcsQueueEngineType);

    MockGraphicsAllocation srcGraphicsAllocation{};
    MockGraphicsAllocation dstGraphicsAllocation{};
    MockBuffer srcMemObj{srcGraphicsAllocation};
    MockBuffer dstMemObj{dstGraphicsAllocation};
    CsrSelectionArgs args{CL_COMMAND_SVM_MEMCPY, &srcMemObj, &dstMemObj, 0u, nullptr};
    args.direction = TransferDirection::hostToLocal;
    cl_command_queue_properties queueProperties[5] = {};

    struct {
        bool isOOQ;
        bool isIdle;
        CommandStreamReceiver *csr;
    } queueState[4]{
        {false, false, ccsCsr}, // IOQ -> use CCS
        {false, true, ccsCsr},  // IOQ -> use CCS
        {true, false, bcsCsr},  // OOQ & CCS busy -> BCS
        {true, true, ccsCsr}    // OOQ & CCS idle -> CCS
    };

    const auto initialTagAddress = queue->heaplessStateInitEnabled ? 1 : 0;

    for (auto &state : queueState) {
        auto queue = std::make_unique<MockCommandQueue>(context.get(), clDevice.get(), queueProperties, false);
        if (state.isOOQ) {
            queue->setOoqEnabled();
        }

        *ccsCsr->tagAddress = initialTagAddress;
        ccsCsr->taskCount = 1u;
        if (state.isIdle) {
            ccsCsr->taskCount = 0u;
        }

        if (device->getProductHelper().blitEnqueuePreferred(false)) {
            // if BCS is preferred, it will be always selected
            EXPECT_EQ(bcsCsr, &queue->selectCsrForBuiltinOperation(args));
        } else {
            EXPECT_EQ(state.csr, &queue->selectCsrForBuiltinOperation(args));
        }
    }
}

HWTEST_F(CsrSelectionCommandQueueWithBlitterTests, givenDebugFlagSetThenBcsAlwaysPreferred) {
    DebugManagerStateRestore restore{};
    debugManager.flags.EnableBlitterForEnqueueOperations.set(1);

    MockGraphicsAllocation srcGraphicsAllocation{};
    MockGraphicsAllocation dstGraphicsAllocation{};
    MockBuffer srcMemObj{srcGraphicsAllocation};
    MockBuffer dstMemObj{dstGraphicsAllocation};
    CsrSelectionArgs args{CL_COMMAND_SVM_MEMCPY, &srcMemObj, &dstMemObj, 0u, nullptr};
    args.direction = TransferDirection::hostToLocal;

    cl_command_queue_properties queueProperties[5] = {};
    auto queue = std::make_unique<MockCommandQueue>(context.get(), clDevice.get(), queueProperties, false);

    auto bcsCsr = queue->getBcsCommandStreamReceiver(*queue->bcsQueueEngineType);
    EXPECT_EQ(bcsCsr, &queue->selectCsrForBuiltinOperation(args));
}

HWTEST_F(CsrSelectionCommandQueueWithBlitterTests, givenWddmOnLinuxThenBcsAlwaysPreferred) {
    MockGraphicsAllocation srcGraphicsAllocation{};
    MockGraphicsAllocation dstGraphicsAllocation{};
    MockBuffer srcMemObj{srcGraphicsAllocation};
    MockBuffer dstMemObj{dstGraphicsAllocation};
    CsrSelectionArgs args{CL_COMMAND_SVM_MEMCPY, &srcMemObj, &dstMemObj, 0u, nullptr};

    cl_command_queue_properties queueProperties[5] = {};
    auto queue = std::make_unique<MockCommandQueue>(context.get(), clDevice.get(), queueProperties, false);

    reinterpret_cast<MockRootDeviceEnvironment *>(&device->getRootDeviceEnvironmentRef())->isWddmOnLinuxEnable = true;

    auto bcsCsr = queue->getBcsCommandStreamReceiver(*queue->bcsQueueEngineType);
    EXPECT_EQ(bcsCsr, &queue->selectCsrForBuiltinOperation(args));
}

HWTEST_F(CsrSelectionCommandQueueWithBlitterTests, givenDstImageFromBufferThenBcsAlwaysPreferred) {
    DebugManagerStateRestore restore{};
    debugManager.flags.EnableBlitterForEnqueueImageOperations.set(1);

    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create(context.get()));
    size_t origin[3] = {0, 0, 0};
    size_t region[3] = {1, 1, 1};
    MockImageBase dstImage{};
    dstImage.associatedMemObject = buffer.get();

    cl_command_queue_properties queueProperties[5] = {};
    auto queue = std::make_unique<MockCommandQueue>(context.get(), clDevice.get(), queueProperties, false);

    CsrSelectionArgs args{CL_COMMAND_WRITE_IMAGE, nullptr, &dstImage, 0u, region, nullptr, origin};
    EXPECT_TRUE(args.dstResource.image && args.dstResource.image->isImageFromBuffer());

    auto ccsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(&queue->getGpgpuCommandStreamReceiver());
    auto bcsCsr = queue->getBcsCommandStreamReceiver(*queue->bcsQueueEngineType);
    if (device->getProductHelper().blitEnqueuePreferred(true)) {
        EXPECT_EQ(bcsCsr, &queue->selectCsrForBuiltinOperation(args));
    } else {
        EXPECT_EQ(ccsCsr, &queue->selectCsrForBuiltinOperation(args));
    }
}

HWTEST_F(CsrSelectionCommandQueueWithBlitterTests, givenSrcImageFromBufferThenBcsAlwaysPreferred) {
    DebugManagerStateRestore restore{};
    debugManager.flags.EnableBlitterForEnqueueImageOperations.set(1);

    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create(context.get()));
    size_t origin[3] = {0, 0, 0};
    size_t region[3] = {1, 1, 1};
    MockImageBase srcImage{};
    srcImage.associatedMemObject = buffer.get();

    cl_command_queue_properties queueProperties[5] = {};
    auto queue = std::make_unique<MockCommandQueue>(context.get(), clDevice.get(), queueProperties, false);

    CsrSelectionArgs args{CL_COMMAND_READ_IMAGE, &srcImage, nullptr, 0u, region, nullptr, origin};
    EXPECT_TRUE(args.srcResource.image && args.srcResource.image->isImageFromBuffer());

    auto ccsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(&queue->getGpgpuCommandStreamReceiver());
    auto bcsCsr = queue->getBcsCommandStreamReceiver(*queue->bcsQueueEngineType);
    if (device->getProductHelper().blitEnqueuePreferred(true)) {
        EXPECT_EQ(bcsCsr, &queue->selectCsrForBuiltinOperation(args));
    } else {
        EXPECT_EQ(ccsCsr, &queue->selectCsrForBuiltinOperation(args));
    }
}

HWTEST_F(CsrSelectionCommandQueueWithBlitterTests, givenEnableBlitterForEnqueueOperationsSetToTwoWhenSelectCsrForImageFromBufferThenBcsAlwaysPreferredOtherwiseUseCcs) {
    DebugManagerStateRestore restore{};
    debugManager.flags.EnableBlitterForEnqueueOperations.set(2);
    debugManager.flags.EnableBlitterForEnqueueImageOperations.set(1);

    cl_command_queue_properties queueProperties[5] = {};
    auto queue = std::make_unique<MockCommandQueue>(context.get(), clDevice.get(), queueProperties, false);
    auto ccsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(&queue->getGpgpuCommandStreamReceiver());
    auto bcsCsr = queue->getBcsCommandStreamReceiver(*queue->bcsQueueEngineType);

    {
        auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create(context.get()));
        size_t origin[3] = {0, 0, 0};
        size_t region[3] = {1, 1, 1};
        MockImageBase dstImage{};
        dstImage.associatedMemObject = buffer.get();

        CsrSelectionArgs args{CL_COMMAND_WRITE_IMAGE, nullptr, &dstImage, 0u, region, nullptr, origin};
        EXPECT_TRUE(args.dstResource.image->isImageFromBuffer());

        EXPECT_EQ(bcsCsr, &queue->selectCsrForBuiltinOperation(args));
    }
    {
        auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create(context.get()));
        size_t origin[3] = {0, 0, 0};
        size_t region[3] = {1, 1, 1};
        MockImageBase dstImage{};

        CsrSelectionArgs args{CL_COMMAND_WRITE_IMAGE, nullptr, &dstImage, 0u, region, nullptr, origin};
        EXPECT_FALSE(args.dstResource.image->isImageFromBuffer());

        EXPECT_EQ(ccsCsr, &queue->selectCsrForBuiltinOperation(args));
    }
}

HWTEST_F(CommandQueueTests, GivenOOQCommandQueueWhenIsGpgpuSubmissionForBcsRequiredCalledThenReturnCorrectValue) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    MockContext context(device.get());
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(&context, context.getDevice(0), nullptr);
    mockCmdQ->latestSentEnqueueType = EnqueueProperties::Operation::gpuKernel;
    mockCmdQ->overrideIsCacheFlushForBcsRequired.enabled = true;
    mockCmdQ->overrideIsCacheFlushForBcsRequired.returnValue = true;
    TimestampPacketDependencies dependencies{};
    auto containsCrossEngineDependency = false;
    EXPECT_TRUE(mockCmdQ->isGpgpuSubmissionForBcsRequired(false, dependencies, containsCrossEngineDependency, false));

    mockCmdQ->setOoqEnabled();
    EXPECT_FALSE(mockCmdQ->isGpgpuSubmissionForBcsRequired(false, dependencies, containsCrossEngineDependency, false));

    containsCrossEngineDependency = true;
    EXPECT_TRUE(mockCmdQ->isGpgpuSubmissionForBcsRequired(false, dependencies, containsCrossEngineDependency, false));
}

HWTEST2_F(CommandQueueTests, givenCmdQueueWhenNotStatelessPlatformThenStatelessIsDisabled, IsAtMostXeHpgCore) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    MockContext context(device.get());
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(&context, context.getDevice(0), nullptr);

    EXPECT_FALSE(mockCmdQ->isForceStateless);
}

HWTEST2_F(CommandQueueTests, givenCmdQueueWhenPlatformWithStatelessNotDisableWithDebugKeyThenStatelessIsDisabled, IsAtMostXeHpgCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DisableForceToStateless.set(1);

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    MockContext context(device.get());
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(&context, context.getDevice(0), nullptr);

    EXPECT_FALSE(mockCmdQ->isForceStateless);
}
