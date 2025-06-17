/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/command_stream_receiver_simulated_hw.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/scratch_space_controller_base.h"
#include "shared/source/command_stream/tag_allocation_layout.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/page_table_mngr.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/surface.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/os_thread.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/command_stream_receiver_fixture.inl"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/batch_buffer_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_bindless_heaps_helper.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_internal_allocation_storage.h"
#include "shared/test/common/mocks/mock_kmd_notify_helper.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/mocks/mock_scratch_space_controller_xehp_and_later.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test_checks_shared.h"
#include "shared/test/unit_test/direct_submission/direct_submission_controller_mock.h"
#include "shared/test/unit_test/fixtures/preemption_fixture.h"

#include "gtest/gtest.h"

#include <chrono>
#include <functional>
#include <limits>

StreamCapture capture;

namespace NEO {
extern ApiSpecificConfig::ApiType apiTypeForUlts;
} // namespace NEO
using namespace NEO;
#include "shared/test/common/test_macros/header/heapful_test_definitions.h"
#include "shared/test/common/test_macros/header/heapless_matchers.h"
using namespace std::chrono_literals;

struct CommandStreamReceiverTest : public DeviceFixture,
                                   public ::testing::Test {
    void SetUp() override {
        DeviceFixture::setUp();

        commandStreamReceiver = &pDevice->getGpgpuCommandStreamReceiver();
        ASSERT_NE(nullptr, commandStreamReceiver);
        memoryManager = commandStreamReceiver->getMemoryManager();
        internalAllocationStorage = commandStreamReceiver->getInternalAllocationStorage();

        auto &compilerProductHelper = pDevice->getCompilerProductHelper();
        auto heaplessEnabled = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo);
        this->heaplessStateInit = compilerProductHelper.isHeaplessStateInitEnabled(heaplessEnabled);
    }

    void TearDown() override {
        DeviceFixture::tearDown();
    }

    CommandStreamReceiver *commandStreamReceiver = nullptr;
    MemoryManager *memoryManager = nullptr;
    InternalAllocationStorage *internalAllocationStorage = nullptr;
    bool heaplessStateInit = false;
};

TEST_F(CommandStreamReceiverTest, givenOsAgnosticCsrWhenGettingCompletionValueThenProperTaskCountIsReturned) {
    MockGraphicsAllocation allocation{};
    uint32_t expectedValue = 0x1234;

    auto &osContext = commandStreamReceiver->getOsContext();
    allocation.updateTaskCount(expectedValue, osContext.getContextId());
    EXPECT_EQ(expectedValue, commandStreamReceiver->getCompletionValue(allocation));
}

TEST_F(CommandStreamReceiverTest, givenOsAgnosticCsrWhenSubmitingCsrDependencyWithNoTagFlushThenFalseRturned) {
    EXPECT_FALSE(commandStreamReceiver->submitDependencyUpdate(nullptr));
}

TEST_F(CommandStreamReceiverTest, givenCsrWhenGettingCompletionAddressThenProperAddressIsReturned) {
    auto expectedAddress = castToUint64(const_cast<TagAddressType *>(commandStreamReceiver->getTagAddress()));
    EXPECT_EQ(expectedAddress + TagAllocationLayout::completionFenceOffset, commandStreamReceiver->getCompletionAddress());
}

TEST_F(CommandStreamReceiverTest, givenCsrWhenGettingCompletionAddressThenUnderlyingMemoryIsZeroed) {
    auto completionFence = reinterpret_cast<TaskCountType *>(commandStreamReceiver->getCompletionAddress());
    EXPECT_EQ(0u, *completionFence);
}

TEST_F(CommandStreamReceiverTest, givenBaseCsrWhenCallingWaitUserFenceThenReturnFalse) {
    EXPECT_FALSE(commandStreamReceiver->waitUserFence(1, commandStreamReceiver->getCompletionAddress(), -1, false, InterruptId::notUsed, nullptr));
    EXPECT_FALSE(commandStreamReceiver->waitUserFenceSupported());
}

HWTEST_F(CommandStreamReceiverTest, WhenCreatingCsrThenDefaultValuesAreSet) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_EQ(0u, csr.peekTaskLevel());
    EXPECT_EQ(csr.heaplessStateInitialized ? 1u : 0u, csr.peekTaskCount());
    EXPECT_FALSE(csr.isPreambleSent);
}

HWTEST_F(CommandStreamReceiverTest, WhenInitializeResourcesThenCallFillReusableAllocationsListOnce) {
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.fillReusableAllocationsListCalled = 0u;
    ultCsr.resourcesInitialized = false;

    commandStreamReceiver->initializeResources(false, pDevice->getPreemptionMode());
    EXPECT_EQ(1u, pDevice->getUltCommandStreamReceiver<FamilyType>().fillReusableAllocationsListCalled);
    commandStreamReceiver->initializeResources(false, pDevice->getPreemptionMode());
    EXPECT_EQ(1u, pDevice->getUltCommandStreamReceiver<FamilyType>().fillReusableAllocationsListCalled);
}

HWTEST_F(CommandStreamReceiverTest, whenContextCreateReturnsFalseThenExpectCSRInitializeResourcesFail) {
    struct MyOsContext : OsContext {
        MyOsContext(uint32_t contextId,
                    const EngineDescriptor &engineDescriptor) : OsContext(0, contextId, engineDescriptor) {}

        bool initializeContext(bool allocateInterrupt) override {
            initializeContextCalled++;
            return false;
        }

        size_t initializeContextCalled = 0u;
    };

    const EngineTypeUsage engineTypeUsageRegular{aub_stream::ENGINE_RCS, EngineUsage::regular};
    MyOsContext osContext{0, EngineDescriptorHelper::getDefaultDescriptor(engineTypeUsageRegular)};
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.resourcesInitialized = false;
    ultCsr.setupContext(osContext);
    bool ret = ultCsr.initializeResources(false, pDevice->getPreemptionMode());
    EXPECT_FALSE(ret);
}

HWTEST_F(CommandStreamReceiverTest, givenFlagsDisabledWhenCallFillReusableAllocationsListThenDoNotAllocateCommandBuffer) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.SetAmountOfReusableAllocations.set(0);
    debugManager.flags.SetAmountOfInternalHeapsToPreallocate.set(0);
    pDevice->getUltCommandStreamReceiver<FamilyType>().callBaseFillReusableAllocationsList = true;
    EXPECT_TRUE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
    EXPECT_EQ(0u, commandStreamReceiver->getResidencyAllocations().size());

    commandStreamReceiver->fillReusableAllocationsList();

    EXPECT_TRUE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
    EXPECT_EQ(0u, commandStreamReceiver->getResidencyAllocations().size());
}

HWTEST_F(CommandStreamReceiverTest, givenFlagEnabledForCommandBuffersWhenCallFillReusableAllocationsListThenAllocateCommandBufferAndMakeItResident) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.SetAmountOfReusableAllocations.set(1);
    debugManager.flags.SetAmountOfInternalHeapsToPreallocate.set(0);
    pDevice->getUltCommandStreamReceiver<FamilyType>().callBaseFillReusableAllocationsList = true;
    EXPECT_TRUE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
    EXPECT_EQ(0u, commandStreamReceiver->getResidencyAllocations().size());

    commandStreamReceiver->fillReusableAllocationsList();

    EXPECT_FALSE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
    EXPECT_EQ(1u, commandStreamReceiver->getResidencyAllocations().size());

    auto allocation = internalAllocationStorage->getAllocationsForReuse().peekHead();
    EXPECT_EQ(AllocationType::commandBuffer, allocation->getAllocationType());
}

HWTEST_F(CommandStreamReceiverTest, givenFlagEnabledForInternalHeapsWhenCallFillReusableAllocationsListThenAllocateInternalHeapAndMakeItResident) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.SetAmountOfReusableAllocations.set(0);
    debugManager.flags.SetAmountOfInternalHeapsToPreallocate.set(1);
    pDevice->getUltCommandStreamReceiver<FamilyType>().callBaseFillReusableAllocationsList = true;
    EXPECT_TRUE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
    EXPECT_EQ(0u, commandStreamReceiver->getResidencyAllocations().size());

    commandStreamReceiver->fillReusableAllocationsList();

    EXPECT_FALSE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
    EXPECT_EQ(1u, commandStreamReceiver->getResidencyAllocations().size());

    auto allocation = internalAllocationStorage->getAllocationsForReuse().peekHead();
    EXPECT_EQ(AllocationType::internalHeap, allocation->getAllocationType());
}

HWTEST_F(CommandStreamReceiverTest, givenUnsetPreallocationsPerQueueWhenRequestPreallocationCalledThenPreallocateCommandBufferCorrectly) {
    EXPECT_TRUE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
    EXPECT_EQ(0u, commandStreamReceiver->getResidencyAllocations().size());

    auto &productHelper = getHelper<ProductHelper>();
    const auto expectedPreallocations = productHelper.getCommandBuffersPreallocatedPerCommandQueue();

    commandStreamReceiver->requestPreallocation();
    if (expectedPreallocations > 0) {
        EXPECT_FALSE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
        EXPECT_EQ(expectedPreallocations, commandStreamReceiver->getResidencyAllocations().size());
    } else {
        EXPECT_TRUE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
        EXPECT_EQ(0u, commandStreamReceiver->getResidencyAllocations().size());
    }

    commandStreamReceiver->releasePreallocationRequest();
    if (expectedPreallocations > 0) {
        EXPECT_FALSE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
        EXPECT_EQ(expectedPreallocations, commandStreamReceiver->getResidencyAllocations().size());
    } else {
        EXPECT_TRUE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
        EXPECT_EQ(0u, commandStreamReceiver->getResidencyAllocations().size());
    }
}

HWTEST_F(CommandStreamReceiverTest, givenPreallocationsPerQueueEqualZeroWhenRequestPreallocationCalledThenDoNotAllocateCommandBuffer) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SetAmountOfReusableAllocationsPerCmdQueue.set(0);
    EXPECT_TRUE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
    EXPECT_EQ(0u, commandStreamReceiver->getResidencyAllocations().size());

    commandStreamReceiver->requestPreallocation();
    EXPECT_TRUE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
    EXPECT_EQ(0u, commandStreamReceiver->getResidencyAllocations().size());
}

HWTEST_F(CommandStreamReceiverTest, givenPreallocationsPerQueueWhenRequestPreallocationCalledThenAllocateCommandBufferIfNeeded) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SetAmountOfReusableAllocationsPerCmdQueue.set(1);
    EXPECT_TRUE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
    EXPECT_EQ(0u, commandStreamReceiver->getResidencyAllocations().size());

    commandStreamReceiver->requestPreallocation();
    EXPECT_FALSE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
    EXPECT_EQ(1u, commandStreamReceiver->getResidencyAllocations().size());

    commandStreamReceiver->releasePreallocationRequest();
    EXPECT_FALSE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
    EXPECT_EQ(1u, commandStreamReceiver->getResidencyAllocations().size());

    commandStreamReceiver->requestPreallocation();
    EXPECT_FALSE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
    EXPECT_EQ(1u, commandStreamReceiver->getResidencyAllocations().size());

    commandStreamReceiver->requestPreallocation();
    EXPECT_FALSE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
    EXPECT_EQ(2u, commandStreamReceiver->getResidencyAllocations().size());
}

HWTEST_F(CommandStreamReceiverTest, givenPreallocationsPerQueueWhenRequestPreallocationCalledButAllocationFailedThenRequestIsIgnored) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SetAmountOfReusableAllocationsPerCmdQueue.set(1);
    EXPECT_TRUE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
    EXPECT_EQ(0u, commandStreamReceiver->getResidencyAllocations().size());

    // make allocation fail
    ExecutionEnvironment &executionEnvironment = *pDevice->getExecutionEnvironment();
    auto memoryManagerBackup = executionEnvironment.memoryManager.release();
    executionEnvironment.memoryManager.reset(new FailMemoryManager(executionEnvironment));

    commandStreamReceiver->requestPreallocation();
    EXPECT_TRUE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
    EXPECT_EQ(0u, commandStreamReceiver->getResidencyAllocations().size());

    // make allocation succeed
    executionEnvironment.memoryManager.reset(memoryManagerBackup);
    commandStreamReceiver->requestPreallocation();
    EXPECT_FALSE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
    EXPECT_EQ(1u, commandStreamReceiver->getResidencyAllocations().size());
}

HWTEST_F(CommandStreamReceiverTest, whenRegisterClientThenIncrementClientNum) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto numClients = csr.getNumClients();

    int client1, client2;
    csr.registerClient(&client1);
    EXPECT_EQ(csr.getNumClients(), numClients + 1);

    csr.registerClient(&client1);
    EXPECT_EQ(csr.getNumClients(), numClients + 1);

    csr.registerClient(&client2);
    EXPECT_EQ(csr.getNumClients(), numClients + 2);

    csr.registerClient(&client2);
    EXPECT_EQ(csr.getNumClients(), numClients + 2);

    csr.unregisterClient(&client1);
    EXPECT_EQ(csr.getNumClients(), numClients + 1);

    csr.unregisterClient(&client1);
    EXPECT_EQ(csr.getNumClients(), numClients + 1);

    csr.unregisterClient(&client2);
    EXPECT_EQ(csr.getNumClients(), numClients);

    csr.unregisterClient(&client2);
    EXPECT_EQ(csr.getNumClients(), numClients);
}

HWTEST_F(CommandStreamReceiverTest, WhenCreatingCsrThenTimestampTypeIs32b) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto allocator = csr.getTimestampPacketAllocator();
    auto tag = allocator->getTag();

    auto expectedOffset = sizeof(typename FamilyType::TimestampPacketType);

    EXPECT_EQ(expectedOffset, tag->getGlobalStartOffset());
}

HWTEST_F(CommandStreamReceiverTest, WhenCreatingCsrThenFlagsAreSetCorrectly) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.initProgrammingFlags();
    EXPECT_FALSE(csr.isPreambleSent);
    EXPECT_FALSE(csr.gsbaFor32BitProgrammed);
    EXPECT_TRUE(csr.mediaVfeStateDirty);
    EXPECT_TRUE(csr.stateComputeModeDirty);
    EXPECT_FALSE(csr.lastVmeSubslicesConfig);
    EXPECT_EQ(0u, csr.lastSentL3Config);
    EXPECT_EQ(-1, csr.lastMediaSamplerConfig);
    EXPECT_EQ(PreemptionMode::Initial, csr.lastPreemptionMode);
    EXPECT_EQ(static_cast<uint32_t>(-1), csr.latestSentStatelessMocsConfig);
}

TEST_F(CommandStreamReceiverTest, givenBaseDownloadAllocationCalledThenDoesNotChangeAnything) {
    auto *memoryManager = commandStreamReceiver->getMemoryManager();

    GraphicsAllocation *graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{commandStreamReceiver->getRootDeviceIndex(), MemoryConstants::pageSize});

    ASSERT_NE(nullptr, graphicsAllocation);
    auto numEvictionAllocsBefore = commandStreamReceiver->getEvictionAllocations().size();
    commandStreamReceiver->CommandStreamReceiver::downloadAllocations(true);
    auto numEvictionAllocsAfter = commandStreamReceiver->getEvictionAllocations().size();
    EXPECT_EQ(numEvictionAllocsBefore, numEvictionAllocsAfter);
    EXPECT_EQ(0u, numEvictionAllocsAfter);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(CommandStreamReceiverTest, WhenCommandStreamReceiverIsCreatedThenItHasATagValue) {
    EXPECT_NE(nullptr, const_cast<TagAddressType *>(commandStreamReceiver->getTagAddress()));
}

TEST_F(CommandStreamReceiverTest, WhenGettingCommandStreamerThenValidPointerIsReturned) {
    auto &cs = commandStreamReceiver->getCS();
    EXPECT_NE(nullptr, &cs);
}

TEST_F(CommandStreamReceiverTest, WhenCommandStreamReceiverIsCreatedThenAvailableMemoryIsGreaterOrEqualRequiredSize) {
    size_t requiredSize = 16384;
    const auto &commandStream = commandStreamReceiver->getCS(requiredSize);
    ASSERT_NE(nullptr, &commandStream);
    EXPECT_GE(commandStream.getAvailableSpace(), requiredSize);
}

TEST_F(CommandStreamReceiverTest, WhenCommandStreamReceiverIsCreatedThenCsOverfetchSizeIsIncludedInGraphicsAllocation) {
    size_t sizeRequested = 560;
    const auto &commandStream = commandStreamReceiver->getCS(sizeRequested);
    ASSERT_NE(nullptr, &commandStream);
    auto *allocation = commandStream.getGraphicsAllocation();
    ASSERT_NE(nullptr, allocation);

    size_t expectedTotalSize = alignUp(sizeRequested + MemoryConstants::cacheLineSize + CSRequirements::csOverfetchSize, MemoryConstants::pageSize64k);

    EXPECT_LT(commandStream.getAvailableSpace(), expectedTotalSize);
    EXPECT_LE(commandStream.getAvailableSpace(), expectedTotalSize - CSRequirements::csOverfetchSize);
    EXPECT_EQ(expectedTotalSize, allocation->getUnderlyingBufferSize());
}

TEST_F(CommandStreamReceiverTest, WhenRequestingAdditionalSpaceThenCsrGetsAdditionalSpace) {
    auto &commandStreamInitial = commandStreamReceiver->getCS();
    size_t requiredSize = commandStreamInitial.getMaxAvailableSpace() + 42;

    const auto &commandStream = commandStreamReceiver->getCS(requiredSize);
    ASSERT_NE(nullptr, &commandStream);
    EXPECT_GE(commandStream.getMaxAvailableSpace(), requiredSize);
}

TEST_F(CommandStreamReceiverTest, givenCommandStreamReceiverWhenGetCSIsCalledThenCommandStreamAllocationTypeShouldBeSetToLinearStream) {
    const auto &commandStream = commandStreamReceiver->getCS();
    auto commandStreamAllocation = commandStream.getGraphicsAllocation();
    ASSERT_NE(nullptr, commandStreamAllocation);

    EXPECT_EQ(AllocationType::commandBuffer, commandStreamAllocation->getAllocationType());
}

HWTEST_F(CommandStreamReceiverTest, whenStoreAllocationThenStoredAllocationHasTaskCountFromCsr) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto *memoryManager = csr.getMemoryManager();
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    EXPECT_FALSE(allocation->isUsed());

    csr.taskCount = 2u;

    csr.getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION);

    EXPECT_EQ(csr.peekTaskCount(), allocation->getTaskCount(csr.getOsContext().getContextId()));
}

HWTEST_F(CommandStreamReceiverTest, givenDisableGpuHangDetectionFlagWhenCheckingGpuHangThenDriverModelIsNotCalledAndFalseIsReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.DisableGpuHangDetection.set(true);

    auto driverModelMock = std::make_unique<MockDriverModel>();
    driverModelMock->isGpuHangDetectedToReturn = true;

    auto osInterface = std::make_unique<OSInterface>();
    osInterface->setDriverModel(std::move(driverModelMock));

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.executionEnvironment.rootDeviceEnvironments[csr.rootDeviceIndex]->osInterface = std::move(osInterface);

    EXPECT_FALSE(csr.isGpuHangDetected());
}

HWTEST_F(CommandStreamReceiverTest, givenGpuHangWhenWaititingForCompletionWithTimeoutThenGpuHangIsReturned) {
    auto driverModelMock = std::make_unique<MockDriverModel>();
    driverModelMock->isGpuHangDetectedToReturn = true;

    auto osInterface = std::make_unique<OSInterface>();
    osInterface->setDriverModel(std::move(driverModelMock));

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.executionEnvironment.rootDeviceEnvironments[csr.rootDeviceIndex]->osInterface = std::move(osInterface);
    csr.callBaseWaitForCompletionWithTimeout = true;
    csr.activePartitions = 1;
    csr.gpuHangCheckPeriod = 0us;

    TagAddressType tasksCount[16] = {};
    csr.tagAddress = tasksCount;

    constexpr auto enableTimeout = false;
    constexpr auto timeoutMicroseconds = std::numeric_limits<std::int64_t>::max();
    constexpr auto taskCountToWait = 1;

    const auto waitStatus = csr.waitForCompletionWithTimeout(enableTimeout, timeoutMicroseconds, taskCountToWait);
    EXPECT_EQ(WaitStatus::gpuHang, waitStatus);
}

HWTEST_F(CommandStreamReceiverTest, givenNoGpuHangWhenWaititingForCompletionWithTimeoutThenReadyIsReturned) {
    auto driverModelMock = std::make_unique<MockDriverModel>();
    driverModelMock->isGpuHangDetectedToReturn = false;

    TagAddressType tasksCount[16] = {};
    driverModelMock->isGpuHangDetectedSideEffect = [&tasksCount] {
        tasksCount[0]++;
    };

    auto osInterface = std::make_unique<OSInterface>();
    osInterface->setDriverModel(std::move(driverModelMock));

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.executionEnvironment.rootDeviceEnvironments[csr.rootDeviceIndex]->osInterface = std::move(osInterface);
    csr.callBaseWaitForCompletionWithTimeout = true;
    csr.tagAddress = tasksCount;
    csr.activePartitions = 1;
    csr.gpuHangCheckPeriod = 0us;

    constexpr auto enableTimeout = false;
    constexpr auto timeoutMicroseconds = std::numeric_limits<std::int64_t>::max();
    constexpr auto taskCountToWait = 1;

    const auto waitStatus = csr.waitForCompletionWithTimeout(enableTimeout, timeoutMicroseconds, taskCountToWait);
    EXPECT_EQ(WaitStatus::ready, waitStatus);
}

HWTEST_F(CommandStreamReceiverTest, givenFailingFlushSubmissionsAndGpuHangWhenWaititingForCompletionWithTimeoutThenGpuHangIsReturned) {
    auto driverModelMock = std::make_unique<MockDriverModel>();
    driverModelMock->isGpuHangDetectedToReturn = true;

    auto osInterface = std::make_unique<OSInterface>();
    osInterface->setDriverModel(std::move(driverModelMock));

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.latestFlushedTaskCount = 0;
    csr.shouldFailFlushBatchedSubmissions = true;
    csr.executionEnvironment.rootDeviceEnvironments[csr.rootDeviceIndex]->osInterface = std::move(osInterface);
    csr.callBaseWaitForCompletionWithTimeout = true;

    constexpr auto enableTimeout = false;
    constexpr auto timeoutMicroseconds = std::numeric_limits<std::int64_t>::max();
    constexpr auto taskCountToWait = 1;

    const auto waitStatus = csr.waitForCompletionWithTimeout(enableTimeout, timeoutMicroseconds, taskCountToWait);
    EXPECT_EQ(WaitStatus::gpuHang, waitStatus);
}

HWTEST_F(CommandStreamReceiverTest, givenFailingFlushSubmissionsAndNoGpuHangWhenWaititingForCompletionWithTimeoutThenNotReadyIsReturned) {
    auto driverModelMock = std::make_unique<MockDriverModel>();
    driverModelMock->isGpuHangDetectedToReturn = false;

    auto osInterface = std::make_unique<OSInterface>();
    osInterface->setDriverModel(std::move(driverModelMock));

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.latestFlushedTaskCount = 0;
    csr.shouldFailFlushBatchedSubmissions = true;
    csr.executionEnvironment.rootDeviceEnvironments[csr.rootDeviceIndex]->osInterface = std::move(osInterface);
    csr.callBaseWaitForCompletionWithTimeout = true;

    constexpr auto enableTimeout = false;
    constexpr auto timeoutMicroseconds = std::numeric_limits<std::int64_t>::max();
    constexpr auto taskCountToWait = 1;

    const auto waitStatus = csr.waitForCompletionWithTimeout(enableTimeout, timeoutMicroseconds, taskCountToWait);
    EXPECT_EQ(WaitStatus::notReady, waitStatus);
}

HWTEST_F(CommandStreamReceiverTest, givenGpuHangWhenWaitingForTaskCountThenGpuHangIsReturned) {
    auto driverModelMock = std::make_unique<MockDriverModel>();
    driverModelMock->isGpuHangDetectedToReturn = true;

    auto osInterface = std::make_unique<OSInterface>();
    osInterface->setDriverModel(std::move(driverModelMock));

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.executionEnvironment.rootDeviceEnvironments[csr.rootDeviceIndex]->osInterface = std::move(osInterface);
    csr.activePartitions = 1;
    csr.gpuHangCheckPeriod = 0us;

    TagAddressType tasksCount[16] = {};
    csr.tagAddress = tasksCount;

    constexpr auto taskCountToWait = 1;
    const auto waitStatus = csr.waitForTaskCount(taskCountToWait);
    EXPECT_EQ(WaitStatus::gpuHang, waitStatus);
    EXPECT_TRUE(csr.downloadAllocationCalled);
}

HWTEST_F(CommandStreamReceiverTest, givenFlushUnsuccessWhenWaitingForTaskCountThenNotReadyIsReturned) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.activePartitions = 1;

    auto taskCountToWait = csr.heaplessStateInitialized ? 2 : 1;
    csr.flushReturnValue = SubmissionStatus::failed;
    auto waitStatus = csr.waitForTaskCount(taskCountToWait);
    EXPECT_EQ(WaitStatus::notReady, waitStatus);

    csr.flushReturnValue = SubmissionStatus::outOfMemory;
    waitStatus = csr.waitForTaskCount(taskCountToWait);
    EXPECT_EQ(WaitStatus::notReady, waitStatus);

    csr.flushReturnValue = SubmissionStatus::outOfHostMemory;
    waitStatus = csr.waitForTaskCount(taskCountToWait);
    EXPECT_EQ(WaitStatus::notReady, waitStatus);

    csr.flushReturnValue = SubmissionStatus::unsupported;
    waitStatus = csr.waitForTaskCount(taskCountToWait);
    EXPECT_EQ(WaitStatus::notReady, waitStatus);

    csr.flushReturnValue = SubmissionStatus::deviceUninitialized;
    waitStatus = csr.waitForTaskCount(taskCountToWait);
    EXPECT_EQ(WaitStatus::notReady, waitStatus);
}

HWTEST_F(CommandStreamReceiverTest, whenDownloadTagAllocationThenDonwloadOnlyIfTagAllocationWasFlushed) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.activePartitions = 1;
    *csr.tagAddress = 1u;

    EXPECT_EQ(0u, csr.downloadAllocationsCalledCount);

    auto ret = csr.testTaskCountReady(csr.tagAddress, 0u);
    EXPECT_TRUE(ret);
    EXPECT_EQ(1u, csr.downloadAllocationsCalledCount);

    constexpr auto taskCountToWait = 1;
    ret = csr.testTaskCountReady(csr.tagAddress, taskCountToWait);
    EXPECT_TRUE(ret);
    EXPECT_EQ(2u, csr.downloadAllocationsCalledCount);

    csr.getTagAllocation()->updateTaskCount(taskCountToWait, csr.osContext->getContextId());
    ret = csr.testTaskCountReady(csr.tagAddress, taskCountToWait);
    EXPECT_TRUE(ret);
    EXPECT_EQ(3u, csr.downloadAllocationsCalledCount);

    csr.setLatestFlushedTaskCount(taskCountToWait);
    ret = csr.testTaskCountReady(csr.tagAddress, taskCountToWait);
    EXPECT_TRUE(ret);
    EXPECT_EQ(4u, csr.downloadAllocationsCalledCount);

    ret = csr.testTaskCountReady(csr.tagAddress, taskCountToWait + 1);
    EXPECT_FALSE(ret);
    EXPECT_EQ(4u, csr.downloadAllocationsCalledCount);
}

HWTEST_F(CommandStreamReceiverTest, givenGpuHangAndNonEmptyAllocationsListWhenCallingWaitForTaskCountAndCleanAllocationListThenWaitIsCalledAndGpuHangIsReturned) {
    auto driverModelMock = std::make_unique<MockDriverModel>();
    driverModelMock->isGpuHangDetectedToReturn = true;

    auto osInterface = std::make_unique<OSInterface>();
    osInterface->setDriverModel(std::move(driverModelMock));

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.executionEnvironment.rootDeviceEnvironments[csr.rootDeviceIndex]->osInterface = std::move(osInterface);
    csr.activePartitions = 1;
    csr.gpuHangCheckPeriod = 0us;

    TagAddressType tasksCount[16] = {};
    VariableBackup<volatile TagAddressType *> csrTagAddressBackup(&csr.tagAddress);
    csr.tagAddress = tasksCount;

    auto hostPtr = reinterpret_cast<void *>(0x1234);
    size_t size = 100;
    auto gmmHelper = pDevice->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(hostPtr));
    auto temporaryAllocation = std::make_unique<MemoryAllocation>(0, 1u /*num gmms*/, AllocationType::externalHostPtr, hostPtr, size, 0,
                                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount, canonizedGpuAddress);
    temporaryAllocation->updateTaskCount(0u, 0u);
    csr.getInternalAllocationStorage()->storeAllocationWithTaskCount(std::move(temporaryAllocation), TEMPORARY_ALLOCATION, 2u);

    constexpr auto taskCountToWait = 1;
    constexpr auto allocationUsage = TEMPORARY_ALLOCATION;
    const auto waitStatus = csr.waitForTaskCountAndCleanAllocationList(taskCountToWait, allocationUsage);

    EXPECT_EQ(WaitStatus::gpuHang, waitStatus);
}

HWTEST_F(CommandStreamReceiverTest, givenCommandStreamReceiverWhenCheckedForInitialStatusOfStatelessMocsIndexThenUnknownMocsIsReturend) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(CacheSettings::unknownMocs, csr.latestSentStatelessMocsConfig);
}

TEST_F(CommandStreamReceiverTest, WhenMakingResidentThenAllocationIsPushedToMemoryManagerResidencyList) {
    auto *memoryManager = commandStreamReceiver->getMemoryManager();

    GraphicsAllocation *graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{commandStreamReceiver->getRootDeviceIndex(), MemoryConstants::pageSize});

    ASSERT_NE(nullptr, graphicsAllocation);

    commandStreamReceiver->makeResident(*graphicsAllocation);

    auto &residencyAllocations = commandStreamReceiver->getResidencyAllocations();

    ASSERT_EQ(1u, residencyAllocations.size());
    EXPECT_EQ(graphicsAllocation, residencyAllocations[0]);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(CommandStreamReceiverTest, GivenNoParamatersWhenMakingResidentThenResidencyDoesNotOccur) {
    commandStreamReceiver->processResidency(commandStreamReceiver->getResidencyAllocations(), 0u);
    auto &residencyAllocations = commandStreamReceiver->getResidencyAllocations();
    EXPECT_EQ(0u, residencyAllocations.size());
}

TEST_F(CommandStreamReceiverTest, WhenDebugSurfaceIsAllocatedThenCorrectTypeIsSet) {
    auto allocation = commandStreamReceiver->allocateDebugSurface(1024);
    EXPECT_EQ(AllocationType::debugContextSaveArea, allocation->getAllocationType());
}

TEST_F(CommandStreamReceiverTest, givenForced32BitAddressingWhenDebugSurfaceIsAllocatedThenRegularAllocationIsReturned) {
    auto *memoryManager = commandStreamReceiver->getMemoryManager();
    memoryManager->setForce32BitAllocations(true);
    auto allocation = commandStreamReceiver->allocateDebugSurface(1024);
    EXPECT_FALSE(allocation->is32BitAllocation());
}

HWTEST_F(CommandStreamReceiverTest, givenDefaultCommandStreamReceiverThenDefaultDispatchingPolicyIsImmediateSubmission) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(DispatchMode::immediateDispatch, csr.dispatchMode);
}

HWTEST_F(CommandStreamReceiverTest, givenL0CommandStreamReceiverThenDefaultDispatchingPolicyIsImmediateSubmission) {
    VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::L0);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(DispatchMode::immediateDispatch, csr.dispatchMode);
}

HWTEST_F(CommandStreamReceiverTest, givenCsrWhenGetIndirectHeapIsCalledThenHeapIsReturned) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &heap = csr.getIndirectHeap(IndirectHeap::Type::dynamicState, 10u);
    EXPECT_NE(nullptr, heap.getGraphicsAllocation());
    EXPECT_NE(nullptr, csr.indirectHeap[IndirectHeap::Type::dynamicState]);
    EXPECT_EQ(&heap, csr.indirectHeap[IndirectHeap::Type::dynamicState]);
}

HWTEST_F(CommandStreamReceiverTest, givenCsrWhenReleaseIndirectHeapIsCalledThenHeapAllocationIsNull) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &heap = csr.getIndirectHeap(IndirectHeap::Type::dynamicState, 10u);
    csr.releaseIndirectHeap(IndirectHeap::Type::dynamicState);
    EXPECT_EQ(nullptr, heap.getGraphicsAllocation());
    EXPECT_EQ(0u, heap.getMaxAvailableSpace());
}

HWTEST_F(CommandStreamReceiverTest, givenCsrWhenAllocateHeapMemoryIsCalledThenHeapMemoryIsAllocated) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    IndirectHeap *dsh = nullptr;
    csr.allocateHeapMemory(IndirectHeap::Type::dynamicState, 4096u, dsh);
    EXPECT_NE(nullptr, dsh);
    ASSERT_NE(nullptr, dsh->getGraphicsAllocation());
    csr.getMemoryManager()->freeGraphicsMemory(dsh->getGraphicsAllocation());
    delete dsh;
}

HWTEST_F(CommandStreamReceiverTest, givenSurfaceStateHeapTypeWhenAllocateHeapMemoryIsCalledThenSSHHasInitialSpaceReserevedForBindlessOffsets) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    IndirectHeap *ssh = nullptr;
    csr.allocateHeapMemory(IndirectHeap::Type::surfaceState, 4096u, ssh);
    EXPECT_NE(nullptr, ssh);
    ASSERT_NE(nullptr, ssh->getGraphicsAllocation());

    auto sshReservedSize = UnitTestHelper<FamilyType>::getDefaultSshUsage();
    EXPECT_EQ(sshReservedSize, ssh->getUsed());

    csr.getMemoryManager()->freeGraphicsMemory(ssh->getGraphicsAllocation());
    delete ssh;
}

TEST(CommandStreamReceiverSimpleTest, givenCsrWithoutTagAllocationWhenGetTagAllocationIsCalledThenNullptrIsReturned) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);
    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);
    EXPECT_EQ(nullptr, csr.getTagAllocation());
}

TEST(CommandStreamReceiverSimpleTest, givenCsrWhenSubmitingBatchBufferThenTaskCountIsIncrementedAndLatestsValuesSetCorrectly) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);
    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);

    GraphicsAllocation *commandBuffer = executionEnvironment.memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr.getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    ResidencyContainer residencyList;

    auto expectedTaskCount = csr.peekTaskCount() + 1;
    csr.submitBatchBuffer(batchBuffer, residencyList);

    EXPECT_EQ(expectedTaskCount, csr.peekTaskCount());
    EXPECT_EQ(expectedTaskCount, csr.peekLatestFlushedTaskCount());
    EXPECT_EQ(expectedTaskCount, csr.peekLatestSentTaskCount());

    executionEnvironment.memoryManager->freeGraphicsMemoryImpl(commandBuffer);
}

TEST(CommandStreamReceiverSimpleTest, givenCsrWhenSubmittingBatchBufferAndFlushFailThenTaskCountIsNotIncremented) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);
    MockCommandStreamReceiverWithFailingFlush csr(executionEnvironment, 0, deviceBitfield);
    GraphicsAllocation *commandBuffer = executionEnvironment.memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr.getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    ResidencyContainer residencyList;

    auto expectedTaskCount = csr.peekTaskCount();
    csr.submitBatchBuffer(batchBuffer, residencyList);

    EXPECT_EQ(expectedTaskCount, csr.peekTaskCount());
    EXPECT_EQ(expectedTaskCount, csr.peekLatestFlushedTaskCount());

    executionEnvironment.memoryManager->freeGraphicsMemoryImpl(commandBuffer);
}

TEST(CommandStreamReceiverSimpleTest, givenCsrWhenTaskCountGreaterThanTagAddressThenIsBusyReturnsTrue) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);
    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);
    csr.taskCount = 0u;
    *csr.tagAddress = 0u;
    EXPECT_FALSE(csr.isBusy());
    csr.taskCount = 1u;
    EXPECT_TRUE(csr.isBusy());
}

HWTEST_F(CommandStreamReceiverTest, givenUpdateTaskCountFromWaitWhenSubmitiingBatchBufferThenTaskCountIsIncrementedAndLatestsValuesSetCorrectly) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UpdateTaskCountFromWait.set(3);

    MockCsrHw<FamilyType> csr(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr.getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    ResidencyContainer residencyList;

    auto previousTaskCount = csr.peekTaskCount();
    auto currentTaskCount = previousTaskCount + 1;
    csr.submitBatchBuffer(batchBuffer, residencyList);

    EXPECT_EQ(currentTaskCount, csr.peekTaskCount());
    EXPECT_EQ(previousTaskCount, csr.peekLatestFlushedTaskCount());
    EXPECT_EQ(currentTaskCount, csr.peekLatestSentTaskCount());

    memoryManager->freeGraphicsMemoryImpl(commandBuffer);
}

HWTEST_F(CommandStreamReceiverTest, givenOverrideCsrAllocationSizeWhenCreatingCommandStreamCsrGraphicsAllocationThenAllocationHasCorrectSize) {
    DebugManagerStateRestore restore;

    int32_t overrideSize = 10 * MemoryConstants::pageSize;
    debugManager.flags.OverrideCsrAllocationSize.set(overrideSize);

    auto defaultEngine = defaultHwInfo->capabilityTable.defaultEngineType;

    MockOsContext mockOsContext(0, EngineDescriptorHelper::getDefaultDescriptor({defaultEngine, EngineUsage::regular}));
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(mockOsContext);

    bool ret = commandStreamReceiver.createPreemptionAllocation();
    ASSERT_TRUE(ret);
    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    auto aligment = gfxCoreHelper.getPreemptionAllocationAlignment();
    size_t expectedAlignedSize = alignUp(overrideSize, aligment);
    EXPECT_EQ(expectedAlignedSize, commandStreamReceiver.preemptionAllocation->getUnderlyingBufferSize());
}

HWTEST_F(CommandStreamReceiverTest, whenCreatingPreemptionAllocationForBcsThenNoAllocationIsCreated) {
    MockOsContext mockOsContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular}));
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(mockOsContext);

    bool ret = commandStreamReceiver.createPreemptionAllocation();
    EXPECT_TRUE(ret);
    EXPECT_EQ(nullptr, commandStreamReceiver.preemptionAllocation);
}

HWTEST_F(CommandStreamReceiverTest, givenCommandStreamReceiverWhenCallingGetMemoryCompressionStateThenReturnNotApplicable) {
    CommandStreamReceiverHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

    for (bool auxTranslationRequired : {false, true}) {
        auto memoryCompressionState = commandStreamReceiver.getMemoryCompressionState(auxTranslationRequired);
        EXPECT_EQ(MemoryCompressionState::notApplicable, memoryCompressionState);
    }
}

HWTEST_F(CommandStreamReceiverTest, givenDebugVariableEnabledWhenCreatingCsrThenEnableTimestampPacketWriteMode) {
    DebugManagerStateRestore restore;

    debugManager.flags.EnableTimestampPacket.set(true);
    CommandStreamReceiverHw<FamilyType> csr1(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    EXPECT_TRUE(csr1.peekTimestampPacketWriteEnabled());

    debugManager.flags.EnableTimestampPacket.set(false);
    CommandStreamReceiverHw<FamilyType> csr2(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    EXPECT_FALSE(csr2.peekTimestampPacketWriteEnabled());
}

HWTEST_F(CommandStreamReceiverTest, whenDirectSubmissionDisabledThenExpectNoFeatureAvailable) {
    DeviceFactory::prepareDeviceEnvironments(*pDevice->getExecutionEnvironment());
    CommandStreamReceiverHw<FamilyType> csr(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular},
                                                                                                        PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    osContext->setDefaultContext(true);
    csr.setupContext(*osContext);
    csr.initializeTagAllocation();
    bool ret = csr.initDirectSubmission();
    EXPECT_TRUE(ret);
    EXPECT_FALSE(csr.isDirectSubmissionEnabled());
    EXPECT_FALSE(csr.isBlitterDirectSubmissionEnabled());
}

HWTEST_F(CommandStreamReceiverTest, whenClearColorAllocationIsCreatedThenItIsDestroyedInCleanupResources) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto mockClearColorAllocation = std::make_unique<MockGraphicsAllocation>();
    csr.clearColorAllocation = mockClearColorAllocation.release();
    EXPECT_NE(nullptr, csr.clearColorAllocation);
    csr.cleanupResources();
    EXPECT_EQ(nullptr, csr.clearColorAllocation);
}

HWTEST_F(CommandStreamReceiverTest, givenCsrWhenUllsEnabledAndStopDirectSubmissionCalledThenObtainOwnershipIsCalled) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto ownershipCalledBefore = csr.recursiveLockCounter.load();
    csr.isAnyDirectSubmissionEnabledResult = true;
    csr.isAnyDirectSubmissionEnabledCallBase = false;
    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(csr);
    csr.directSubmission.reset(directSubmission);
    csr.stopDirectSubmission(false, true);
    EXPECT_EQ(csr.recursiveLockCounter, ownershipCalledBefore + 1u);
}

HWTEST_F(CommandStreamReceiverTest, givenCsrWhenUllsEnabledAndStopDirectSubmissionCalledWithLockNotNeededThenObtainOwnershipIsNotCalled) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto ownershipCalledBefore = csr.recursiveLockCounter.load();
    csr.isAnyDirectSubmissionEnabledResult = true;
    csr.isAnyDirectSubmissionEnabledCallBase = false;
    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(csr);
    csr.directSubmission.reset(directSubmission);
    csr.stopDirectSubmission(false, false);
    EXPECT_EQ(csr.recursiveLockCounter, ownershipCalledBefore);
}

HWTEST_F(CommandStreamReceiverTest, givenCsrWhenUllsDisabledAndStopDirectSubmissionCalledThenObtainOwnershipIsNotCalled) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto ownershipCalledBefore = csr.recursiveLockCounter.load();
    csr.isAnyDirectSubmissionEnabledResult = false;
    csr.isAnyDirectSubmissionEnabledCallBase = false;
    csr.stopDirectSubmission(false, true);
    EXPECT_EQ(csr.recursiveLockCounter, ownershipCalledBefore);
}

HWTEST_F(CommandStreamReceiverTest, givenNoDirectSubmissionWhenCheckTaskCountFromWaitEnabledThenReturnsFalse) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableL3FlushAfterPostSync.set(0);

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_FALSE(csr.isUpdateTagFromWaitEnabled());
}

HWTEST_F(CommandStreamReceiverTest, givenCsrWhenCheckKmdWaitOnTaskCountEnabledThenReturnsFalse) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.callBaseIsKmdWaitOnTaskCountAllowed = true;
    EXPECT_FALSE(csr.isKmdWaitOnTaskCountAllowed());
}

HWTEST_F(CommandStreamReceiverTest, givenUpdateTaskCountFromWaitWhenCheckTaskCountFromWaitEnabledThenProperValueReturned) {
    DebugManagerStateRestore restorer;
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    {
        debugManager.flags.UpdateTaskCountFromWait.set(0);
        EXPECT_FALSE(csr.isUpdateTagFromWaitEnabled());
    }

    {
        debugManager.flags.UpdateTaskCountFromWait.set(1);
        EXPECT_EQ(csr.isUpdateTagFromWaitEnabled(), csr.isDirectSubmissionEnabled());
    }

    {
        debugManager.flags.UpdateTaskCountFromWait.set(2);
        EXPECT_EQ(csr.isUpdateTagFromWaitEnabled(), csr.isAnyDirectSubmissionEnabled());
    }

    {
        debugManager.flags.UpdateTaskCountFromWait.set(3);
        EXPECT_TRUE(csr.isUpdateTagFromWaitEnabled());
    }
}

HWTEST_F(CommandStreamReceiverTest, givenUpdateTaskCountFromWaitWhenCheckIfEnabledThenCanBeEnabledOnlyWithDirectSubmission) {

    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableL3FlushAfterPostSync.set(0);

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();

    {
        csr.directSubmissionAvailable = true;
        EXPECT_EQ(csr.isUpdateTagFromWaitEnabled(), gfxCoreHelper.isUpdateTaskCountFromWaitSupported());
    }

    {
        csr.directSubmissionAvailable = false;
        EXPECT_FALSE(csr.isUpdateTagFromWaitEnabled());
    }
}

HWTEST_F(CommandStreamReceiverTest, givenUpdateTaskCountFromWaitInMultiRootDeviceEnvironmentWhenCheckIfEnabledThenCanBeEnabledOnlyWithDirectSubmission) {

    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleRootDevices.set(2);
    NEO::debugManager.flags.EnableL3FlushAfterPostSync.set(0);

    TearDown();
    SetUp();
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();

    {
        csr.directSubmissionAvailable = true;
        EXPECT_EQ(csr.isUpdateTagFromWaitEnabled(), gfxCoreHelper.isUpdateTaskCountFromWaitSupported());
    }

    {
        csr.directSubmissionAvailable = false;
        EXPECT_FALSE(csr.isUpdateTagFromWaitEnabled());
    }
}

struct InitDirectSubmissionFixture {
    void setUp() {
        debugManager.flags.EnableDirectSubmission.set(1);
        executionEnvironment = new MockExecutionEnvironment();
        DeviceFactory::prepareDeviceEnvironments(*executionEnvironment);
        VariableBackup<UltHwConfig> backup(&ultHwConfig);
        ultHwConfig.forceOsAgnosticMemoryManager = false;
        executionEnvironment->initializeMemoryManager();
        device.reset(new MockDevice(executionEnvironment, 0u));
    }

    void tearDown() {}

    DebugManagerStateRestore restore;
    MockExecutionEnvironment *executionEnvironment;
    std::unique_ptr<MockDevice> device;
};

using InitDirectSubmissionTest = Test<InitDirectSubmissionFixture>;

HWTEST_F(InitDirectSubmissionTest, givenDirectSubmissionControllerEnabledWhenInitDirectSubmissionThenCsrIsRegistered) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDirectSubmissionController.set(1);
    VariableBackup<decltype(NEO::Thread::createFunc)> funcBackup{&NEO::Thread::createFunc, [](void *(*func)(void *), void *arg) -> std::unique_ptr<Thread> { return nullptr; }};

    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular},
                                                                                                        PreemptionMode::ThreadGroup, device->getDeviceBitfield())));

    auto controller = static_cast<DirectSubmissionControllerMock *>(device->executionEnvironment->initializeDirectSubmissionController());
    controller->keepControlling.store(false);
    EXPECT_EQ(controller->directSubmissions.size(), 0u);

    osContext->ensureContextInitialized(false);
    osContext->setDefaultContext(true);
    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].submitOnInit = false;
    csr->setupContext(*osContext);
    csr->initializeTagAllocation();

    bool ret = csr->initDirectSubmission();
    EXPECT_TRUE(ret);
    EXPECT_TRUE(csr->isDirectSubmissionEnabled());
    EXPECT_FALSE(csr->isBlitterDirectSubmissionEnabled());

    EXPECT_EQ(controller->directSubmissions.size(), 1u);
    EXPECT_TRUE(controller->directSubmissions.find(csr.get()) != controller->directSubmissions.end());

    csr.reset();
    EXPECT_EQ(controller->directSubmissions.size(), 0u);
}

HWTEST_F(InitDirectSubmissionTest, givenDirectSubmissionControllerDisabledWhenInitDirectSubmissionThenControllerIsNotCreatedAndCsrIsNotRegistered) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDirectSubmissionController.set(0);

    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular},
                                                                                                        PreemptionMode::ThreadGroup, device->getDeviceBitfield())));

    osContext->ensureContextInitialized(false);
    osContext->setDefaultContext(true);
    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].submitOnInit = false;

    csr->setupContext(*osContext);
    csr->initializeTagAllocation();
    bool ret = csr->initDirectSubmission();
    EXPECT_TRUE(ret);
    EXPECT_TRUE(csr->isDirectSubmissionEnabled());

    auto controller = static_cast<DirectSubmissionControllerMock *>(device->executionEnvironment->initializeDirectSubmissionController());
    EXPECT_EQ(controller, nullptr);
}

HWTEST_F(InitDirectSubmissionTest, givenSetCsrFlagSetWhenInitDirectSubmissionThenControllerIsNotCreatedAndCsrIsNotRegistered) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SetCommandStreamReceiver.set(1);

    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular},
                                                                                                        PreemptionMode::ThreadGroup, device->getDeviceBitfield())));

    osContext->ensureContextInitialized(false);
    osContext->setDefaultContext(true);
    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].submitOnInit = false;

    csr->setupContext(*osContext);
    csr->initializeTagAllocation();
    bool ret = csr->initDirectSubmission();
    EXPECT_TRUE(ret);
    EXPECT_TRUE(csr->isDirectSubmissionEnabled());

    auto controller = static_cast<DirectSubmissionControllerMock *>(device->executionEnvironment->initializeDirectSubmissionController());
    EXPECT_EQ(controller, nullptr);
}

HWTEST_F(InitDirectSubmissionTest, whenDirectSubmissionEnabledOnRcsThenExpectFeatureAvailable) {
    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular},
                                                                                                        PreemptionMode::ThreadGroup, device->getDeviceBitfield())));
    osContext->ensureContextInitialized(false);
    osContext->setDefaultContext(true);
    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].submitOnInit = false;

    csr->setupContext(*osContext);
    csr->initializeTagAllocation();
    bool ret = csr->initDirectSubmission();
    EXPECT_TRUE(ret);
    EXPECT_TRUE(csr->isDirectSubmissionEnabled());
    EXPECT_FALSE(csr->isBlitterDirectSubmissionEnabled());

    csr.reset();
}

template <class Type>
class CommandStreamReceiverHwDirectSubmissionMock : public CommandStreamReceiverHw<Type> {
  public:
    using CommandStreamReceiverHw<Type>::CommandStreamReceiverHw;
    using CommandStreamReceiverHw<Type>::directSubmission;

    std::unique_lock<CommandStreamReceiver::MutexType> obtainUniqueOwnership() override {
        recursiveLockCounter++;
        return CommandStreamReceiverHw<Type>::obtainUniqueOwnership();
    }

    void startControllingDirectSubmissions() override {
        startControllingDirectSubmissionsCalled = true;
    }

    uint32_t recursiveLockCounter = 0;
    bool startControllingDirectSubmissionsCalled = false;
};

HWTEST_F(InitDirectSubmissionTest, whenCallInitDirectSubmissionAgainThenItIsNotReinitialized) {
    auto csr = std::make_unique<CommandStreamReceiverHwDirectSubmissionMock<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular},
                                                                                                        PreemptionMode::ThreadGroup, device->getDeviceBitfield())));
    osContext->ensureContextInitialized(false);
    osContext->setDefaultContext(true);
    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].submitOnInit = false;

    csr->setupContext(*osContext);
    csr->initializeTagAllocation();
    bool ret = csr->initDirectSubmission();
    auto directSubmission = csr->directSubmission.get();
    EXPECT_TRUE(ret);
    EXPECT_TRUE(csr->isDirectSubmissionEnabled());
    EXPECT_FALSE(csr->isBlitterDirectSubmissionEnabled());

    ret = csr->initDirectSubmission();
    EXPECT_TRUE(ret);
    EXPECT_TRUE(csr->isDirectSubmissionEnabled());
    EXPECT_FALSE(csr->isBlitterDirectSubmissionEnabled());
    EXPECT_EQ(directSubmission, csr->directSubmission.get());

    csr.reset();
}

HWTEST_F(InitDirectSubmissionTest, whenCallInitDirectSubmissionThenObtainLockAndInitController) {
    auto csr = std::make_unique<CommandStreamReceiverHwDirectSubmissionMock<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular},
                                                                                                        PreemptionMode::ThreadGroup, device->getDeviceBitfield())));
    osContext->ensureContextInitialized(false);
    osContext->setDefaultContext(true);
    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].submitOnInit = false;
    csr->setupContext(*osContext);
    csr->initializeTagAllocation();
    csr->initDirectSubmission();
    EXPECT_EQ(1u, csr->recursiveLockCounter);
    EXPECT_TRUE(csr->startControllingDirectSubmissionsCalled);

    csr.reset();
}

HWTEST_F(InitDirectSubmissionTest, givenDirectSubmissionEnabledWhenPlatformNotSupportsRcsThenExpectFeatureNotAvailable) {
    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular},
                                                                                                        PreemptionMode::ThreadGroup, device->getDeviceBitfield())));
    osContext->ensureContextInitialized(false);
    osContext->setDefaultContext(true);
    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].engineSupported = false;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].submitOnInit = false;

    csr->setupContext(*osContext);
    csr->initializeTagAllocation();
    bool ret = csr->initDirectSubmission();
    EXPECT_TRUE(ret);
    EXPECT_FALSE(csr->isDirectSubmissionEnabled());

    csr.reset();
}

HWTEST_F(InitDirectSubmissionTest, whenDirectSubmissionEnabledOnBcsThenExpectFeatureAvailable) {
    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular},
                                                                                                        PreemptionMode::ThreadGroup, device->getDeviceBitfield())));
    osContext->ensureContextInitialized(false);
    osContext->setDefaultContext(true);

    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_BCS].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_BCS].submitOnInit = false;

    csr->setupContext(*osContext);
    csr->initializeTagAllocation();
    bool ret = csr->initDirectSubmission();
    EXPECT_TRUE(ret);
    EXPECT_FALSE(csr->isDirectSubmissionEnabled());
    EXPECT_TRUE(csr->isBlitterDirectSubmissionEnabled());

    csr.reset();
}

HWTEST_F(InitDirectSubmissionTest, givenDirectSubmissionEnabledWhenPlatformNotSupportsBcsThenExpectFeatureNotAvailable) {
    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular},
                                                                                                        PreemptionMode::ThreadGroup, device->getDeviceBitfield())));
    osContext->ensureContextInitialized(false);
    osContext->setDefaultContext(true);

    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_BCS].engineSupported = false;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_BCS].submitOnInit = false;

    csr->setupContext(*osContext);
    csr->initializeTagAllocation();
    bool ret = csr->initDirectSubmission();
    EXPECT_TRUE(ret);
    EXPECT_FALSE(csr->isDirectSubmissionEnabled());
    EXPECT_FALSE(csr->isBlitterDirectSubmissionEnabled());

    csr.reset();
}

HWTEST_F(InitDirectSubmissionTest, givenLowPriorityContextWhenDirectSubmissionDisabledOnLowPriorityThenExpectFeatureNotAvailable) {
    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::lowPriority},
                                                                                                        PreemptionMode::ThreadGroup, device->getDeviceBitfield())));
    osContext->ensureContextInitialized(false);
    osContext->setDefaultContext(true);

    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].useLowPriority = false;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].submitOnInit = false;

    csr->setupContext(*osContext);
    csr->initializeTagAllocation();
    bool ret = csr->initDirectSubmission();
    EXPECT_TRUE(ret);
    EXPECT_FALSE(csr->isDirectSubmissionEnabled());

    csr.reset();
}

HWTEST_F(InitDirectSubmissionTest, givenLowPriorityContextWhenDirectSubmissionEnabledOnLowPriorityThenExpectFeatureAvailable) {
    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::lowPriority},
                                                                                                        PreemptionMode::ThreadGroup, device->getDeviceBitfield())));
    osContext->ensureContextInitialized(false);

    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].useLowPriority = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].submitOnInit = false;
    csr->setupContext(*osContext);
    csr->initializeTagAllocation();
    bool ret = csr->initDirectSubmission();
    EXPECT_TRUE(ret);
    EXPECT_TRUE(csr->isDirectSubmissionEnabled());

    csr.reset();
}

HWTEST_F(InitDirectSubmissionTest, givenInternalContextWhenDirectSubmissionDisabledOnInternalThenExpectFeatureNotAvailable) {
    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::internal}, PreemptionMode::ThreadGroup,
                                                                                                        device->getDeviceBitfield())));
    osContext->ensureContextInitialized(false);
    osContext->setDefaultContext(true);

    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].useInternal = false;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].submitOnInit = false;

    csr->setupContext(*osContext);
    csr->initializeTagAllocation();
    bool ret = csr->initDirectSubmission();
    EXPECT_TRUE(ret);
    EXPECT_FALSE(csr->isDirectSubmissionEnabled());

    csr.reset();
}

HWTEST_F(InitDirectSubmissionTest, givenInternalContextWhenDirectSubmissionEnabledOnInternalThenExpectFeatureAvailable) {
    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::internal}, PreemptionMode::ThreadGroup,
                                                                                                        device->getDeviceBitfield())));
    osContext->ensureContextInitialized(false);

    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].useInternal = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].submitOnInit = false;

    csr->setupContext(*osContext);
    csr->initializeTagAllocation();
    bool ret = csr->initDirectSubmission();
    EXPECT_TRUE(ret);
    EXPECT_TRUE(csr->isDirectSubmissionEnabled());

    csr.reset();
}

HWTEST_F(InitDirectSubmissionTest, givenRootDeviceContextWhenDirectSubmissionDisabledOnRootDeviceThenExpectFeatureNotAvailable) {
    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::internal}, PreemptionMode::ThreadGroup,
                                                                                                        device->getDeviceBitfield(), true)));
    osContext->ensureContextInitialized(false);
    osContext->setDefaultContext(true);

    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].useRootDevice = false;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].submitOnInit = false;

    csr->setupContext(*osContext);
    csr->initializeTagAllocation();
    bool ret = csr->initDirectSubmission();
    EXPECT_TRUE(ret);
    EXPECT_FALSE(csr->isDirectSubmissionEnabled());

    csr.reset();
}

HWTEST_F(InitDirectSubmissionTest, givenRootDeviceContextWhenDirectSubmissionEnabledOnRootDeviceThenExpectFeatureAvailable) {
    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::internal}, PreemptionMode::ThreadGroup,
                                                                                                        device->getDeviceBitfield(), true)));
    osContext->ensureContextInitialized(false);

    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].useRootDevice = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].submitOnInit = false;

    csr->setupContext(*osContext);
    csr->initializeTagAllocation();
    bool ret = csr->initDirectSubmission();
    EXPECT_TRUE(ret);
    EXPECT_TRUE(csr->isDirectSubmissionEnabled());

    csr.reset();
}

HWTEST_F(InitDirectSubmissionTest, givenNonDefaultContextWhenDirectSubmissionDisabledOnNonDefaultThenExpectFeatureNotAvailable) {
    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::internal}, PreemptionMode::ThreadGroup,
                                                                                                        device->getDeviceBitfield())));
    osContext->ensureContextInitialized(false);
    osContext->setDefaultContext(false);

    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].useNonDefault = false;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].submitOnInit = false;

    csr->setupContext(*osContext);
    csr->initializeTagAllocation();
    bool ret = csr->initDirectSubmission();
    EXPECT_TRUE(ret);
    EXPECT_FALSE(csr->isDirectSubmissionEnabled());

    csr.reset();
}

HWTEST_F(InitDirectSubmissionTest, givenNonDefaultContextContextWhenDirectSubmissionEnabledOnNonDefaultContextThenExpectFeatureAvailable) {
    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular}, PreemptionMode::ThreadGroup,
                                                                                                        device->getDeviceBitfield())));
    osContext->ensureContextInitialized(false);
    osContext->setDefaultContext(false);

    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].useNonDefault = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].submitOnInit = false;

    csr->setupContext(*osContext);
    csr->initializeTagAllocation();
    bool ret = csr->initDirectSubmission();
    EXPECT_TRUE(ret);
    EXPECT_TRUE(csr->isDirectSubmissionEnabled());

    csr.reset();
}

HWTEST_F(InitDirectSubmissionTest, GivenBlitterOverrideEnabledWhenBlitterIsNonDefaultContextThenExpectDirectSubmissionStarted) {
    debugManager.flags.DirectSubmissionOverrideBlitterSupport.set(1);
    debugManager.flags.DirectSubmissionDisableMonitorFence.set(0);
    debugManager.flags.DirectSubmissionInsertExtraMiMemFenceCommands.set(0);

    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::internal}, PreemptionMode::ThreadGroup,
                                                                                                        device->getDeviceBitfield())));
    osContext->ensureContextInitialized(false);
    osContext->setDefaultContext(false);

    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_BCS].engineSupported = false;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_BCS].useNonDefault = false;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_BCS].submitOnInit = false;

    csr->setupContext(*osContext);
    csr->initializeTagAllocation();
    bool ret = csr->initDirectSubmission();
    EXPECT_TRUE(ret);
    EXPECT_FALSE(csr->isDirectSubmissionEnabled());
    EXPECT_TRUE(csr->isBlitterDirectSubmissionEnabled());
    EXPECT_TRUE(osContext->isDirectSubmissionActive());
}

HWTEST_F(CommandStreamReceiverTest, whenCsrIsCreatedThenUseTimestampPacketWriteIfPossible) {
    CommandStreamReceiverHw<FamilyType> csr(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    EXPECT_EQ(UnitTestHelper<FamilyType>::isTimestampPacketWriteSupported(), csr.peekTimestampPacketWriteEnabled());
}

TEST_F(CommandStreamReceiverTest, whenGettingEventTsAllocatorThenSameTagAllocatorIsReturned) {
    TagAllocatorBase *allocator = commandStreamReceiver->getEventTsAllocator();
    EXPECT_NE(nullptr, allocator);
    TagAllocatorBase *allocator2 = commandStreamReceiver->getEventTsAllocator();
    EXPECT_EQ(allocator2, allocator);
}

TEST_F(CommandStreamReceiverTest, whenGettingEventPerfCountAllocatorThenSameTagAllocatorIsReturned) {
    const uint32_t gpuReportSize = 100;
    TagAllocatorBase *allocator = commandStreamReceiver->getEventPerfCountAllocator(gpuReportSize);
    EXPECT_NE(nullptr, allocator);
    TagAllocatorBase *allocator2 = commandStreamReceiver->getEventPerfCountAllocator(gpuReportSize);
    EXPECT_EQ(allocator2, allocator);
}

HWTEST_F(CommandStreamReceiverTest, givenUltCommandStreamReceiverWhenAddAubCommentIsCalledThenCallAddAubCommentOnCsr) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.addAubComment("message");
    EXPECT_TRUE(csr.addAubCommentCalled);
}

TEST(CommandStreamReceiverSimpleTest, givenCommandStreamReceiverWhenInitializeTagAllocationsThenSingleTagAllocationIsBeingAllocated) {
    uint32_t numRootDevices = 10u;
    UltDeviceFactory deviceFactory{numRootDevices, 0};

    for (auto rootDeviceIndex = 0u; rootDeviceIndex < numRootDevices; rootDeviceIndex++) {
        auto tagAllocation = deviceFactory.rootDevices[rootDeviceIndex]->commandStreamReceivers[0]->getTagAllocation();
        EXPECT_NE(nullptr, tagAllocation);
        EXPECT_EQ(AllocationType::tagBuffer, deviceFactory.rootDevices[rootDeviceIndex]->commandStreamReceivers[0]->getTagAllocation()->getAllocationType());
        EXPECT_TRUE(deviceFactory.rootDevices[rootDeviceIndex]->commandStreamReceivers[0]->getTagAddress() != nullptr);
        EXPECT_EQ(*deviceFactory.rootDevices[rootDeviceIndex]->commandStreamReceivers[0]->getTagAddress(), initialHardwareTag);
        auto tagsMultiAllocation = deviceFactory.rootDevices[rootDeviceIndex]->commandStreamReceivers[0]->getTagsMultiAllocation();
        EXPECT_EQ(tagsMultiAllocation->getGraphicsAllocations().size(), numRootDevices);

        for (auto i = 0u; i < numRootDevices; i++) {
            auto allocation = tagsMultiAllocation->getGraphicsAllocation(i);
            if (rootDeviceIndex == i) {
                EXPECT_EQ(allocation, tagAllocation);
            } else {
                EXPECT_EQ(nullptr, allocation);
            }
        }
    }
}
TEST(CommandStreamReceiverSimpleTest, givenCommandStreamReceiverWhenEnsureTagAllocationIsCalledForIncorrectRootDeviceIndexThenFailureIsReturned) {
    uint32_t numRootDevices = 1u;
    UltDeviceFactory deviceFactory{numRootDevices, 0};

    EXPECT_FALSE(deviceFactory.rootDevices[0]->commandStreamReceivers[0]->ensureTagAllocationForRootDeviceIndex(1));
}

TEST(CommandStreamReceiverSimpleTest, givenCommandStreamReceiverWhenEnsureTagAllocationIsCalledForRootDeviceIndexWhichHasTagAllocationThenReturnEarlySucess) {
    uint32_t numRootDevices = 1u;
    UltDeviceFactory deviceFactory{numRootDevices, 0};

    EXPECT_TRUE(deviceFactory.rootDevices[0]->commandStreamReceivers[0]->ensureTagAllocationForRootDeviceIndex(0));
}

TEST(CommandStreamReceiverSimpleTest, givenCommandStreamReceiverWhenItIsDestroyedThenItDestroysTagAllocation) {
    struct MockGraphicsAllocationWithDestructorTracing : public MockGraphicsAllocation {
        using MockGraphicsAllocation::MockGraphicsAllocation;
        ~MockGraphicsAllocationWithDestructorTracing() override { *destructorCalled = true; }
        bool *destructorCalled = nullptr;
    };

    bool destructorCalled = false;
    int gpuTag = 0;

    auto mockGraphicsAllocation = new MockGraphicsAllocationWithDestructorTracing(0, 1u /*num gmms*/, AllocationType::unknown, &gpuTag, 0llu, 0llu, 1u, MemoryPool::memoryNull, MemoryManager::maxOsContextCount);
    mockGraphicsAllocation->destructorCalled = &destructorCalled;
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    auto csr = std::make_unique<MockCommandStreamReceiver>(executionEnvironment, 0, 1);
    executionEnvironment.memoryManager.reset(new OsAgnosticMemoryManager(executionEnvironment));

    auto allocations = new MultiGraphicsAllocation(0u);
    allocations->addAllocation(mockGraphicsAllocation);

    csr->tagsMultiAllocation = allocations;
    csr->setTagAllocation(allocations->getGraphicsAllocation(0u));

    EXPECT_FALSE(destructorCalled);
    csr.reset(nullptr);
    EXPECT_TRUE(destructorCalled);
}

TEST(CommandStreamReceiverSimpleTest, givenCommandStreamReceiverWhenInitializeTagAllocationIsCalledThenTagAllocationIsBeingAllocated) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    DeviceBitfield devices(0b11);
    auto csr = std::make_unique<MockCommandStreamReceiver>(executionEnvironment, 0, devices);
    csr->immWritePostSyncWriteOffset = 32u;
    executionEnvironment.memoryManager.reset(new OsAgnosticMemoryManager(executionEnvironment));
    EXPECT_EQ(nullptr, csr->getTagAllocation());
    csr->initializeTagAllocation();
    EXPECT_NE(nullptr, csr->getTagAllocation());
    EXPECT_EQ(AllocationType::tagBuffer, csr->getTagAllocation()->getAllocationType());
    EXPECT_EQ(csr->getTagAllocation()->getUnderlyingBuffer(), csr->getTagAddress());
    auto tagAddress = csr->getTagAddress();
    for (uint32_t i = 0; i < 2; i++) {
        EXPECT_EQ(*tagAddress, initialHardwareTag);
        tagAddress = ptrOffset(tagAddress, csr->getImmWritePostSyncWriteOffset());
    }
}

TEST(CommandStreamReceiverSimpleTest, givenCommandStreamReceiverWhenEnsureTagAllocationForRootDeviceIndexIsCalledThenProperAllocationIsBeingAllocated) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get(), true, 10u);
    DeviceBitfield devices(0b1111);
    auto csr = std::make_unique<MockCommandStreamReceiver>(executionEnvironment, 0, devices);
    csr->immWritePostSyncWriteOffset = 32u;

    executionEnvironment.memoryManager.reset(new OsAgnosticMemoryManager(executionEnvironment));
    EXPECT_EQ(nullptr, csr->getTagAllocation());

    csr->initializeTagAllocation();
    EXPECT_NE(nullptr, csr->getTagAllocation());
    EXPECT_EQ(AllocationType::tagBuffer, csr->getTagAllocation()->getAllocationType());
    EXPECT_EQ(csr->getTagAllocation()->getUnderlyingBuffer(), csr->getTagAddress());

    auto tagAddress = csr->getTagAddress();
    for (uint32_t i = 0; i < 4; i++) {
        EXPECT_EQ(*tagAddress, initialHardwareTag);
        tagAddress = ptrOffset(tagAddress, csr->getImmWritePostSyncWriteOffset());
    }

    auto tagsMultiAllocation = csr->getTagsMultiAllocation();
    auto graphicsAllocation0 = tagsMultiAllocation->getGraphicsAllocation(0);

    for (auto graphicsAllocation : tagsMultiAllocation->getGraphicsAllocations()) {
        if (graphicsAllocation != graphicsAllocation0) {
            EXPECT_EQ(nullptr, graphicsAllocation);
        }
    }

    EXPECT_TRUE(csr->ensureTagAllocationForRootDeviceIndex(1));
    auto graphicsAllocation = tagsMultiAllocation->getGraphicsAllocation(1);
    EXPECT_NE(nullptr, graphicsAllocation);
    EXPECT_EQ(graphicsAllocation->getUnderlyingBuffer(), graphicsAllocation0->getUnderlyingBuffer());
}

TEST(CommandStreamReceiverSimpleTest, givenMemoryAllocationFailureWhenEnsuringTagAllocationThenFailureIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get(), true, 2u);
    DeviceBitfield devices(0b11);
    auto csr = std::make_unique<MockCommandStreamReceiver>(executionEnvironment, 0, devices);

    EXPECT_EQ(nullptr, csr->getTagAllocation());
    executionEnvironment.memoryManager.reset(new OsAgnosticMemoryManager(executionEnvironment));

    csr->initializeTagAllocation();
    EXPECT_NE(nullptr, csr->getTagAllocation());
    executionEnvironment.memoryManager.reset(new FailMemoryManager(executionEnvironment));

    EXPECT_FALSE(csr->ensureTagAllocationForRootDeviceIndex(1));
}

TEST(CommandStreamReceiverSimpleTest, givenVariousDataSetsWhenVerifyingMemoryThenCorrectValueIsReturned) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);
    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);

    constexpr size_t setSize = 6;
    uint8_t setA1[setSize] = {4, 3, 2, 1, 2, 10};
    uint8_t setA2[setSize] = {4, 3, 2, 1, 2, 10};
    uint8_t setB1[setSize] = {40, 15, 3, 11, 17, 4};
    uint8_t setB2[setSize] = {40, 15, 3, 11, 17, 4};

    constexpr auto compareEqual = AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual;
    constexpr auto compareNotEqual = AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareNotEqual;

    EXPECT_TRUE(csr.expectMemory(setA1, setA2, setSize, compareEqual));
    EXPECT_TRUE(csr.expectMemory(setB1, setB2, setSize, compareEqual));
    EXPECT_FALSE(csr.expectMemory(setA1, setA2, setSize, compareNotEqual));
    EXPECT_FALSE(csr.expectMemory(setB1, setB2, setSize, compareNotEqual));

    EXPECT_FALSE(csr.expectMemory(setA1, setB1, setSize, compareEqual));
    EXPECT_FALSE(csr.expectMemory(setA2, setB2, setSize, compareEqual));
    EXPECT_TRUE(csr.expectMemory(setA1, setB1, setSize, compareNotEqual));
    EXPECT_TRUE(csr.expectMemory(setA2, setB2, setSize, compareNotEqual));
}

TEST(CommandStreamReceiverSimpleTest, givenBaseCsrWhenWritingMemoryThenReturnFalse) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();

    DeviceBitfield deviceBitfield(1);
    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);

    MockGraphicsAllocation mockAllocation;

    EXPECT_FALSE(csr.writeMemory(mockAllocation));
}

TEST(CommandStreamReceiverSimpleTest, givenNewResourceFlushDisabledWhenProvidingNeverUsedAllocationTaskCountThenDoNotMarkNewResourceTrue) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);
    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);
    MockGraphicsAllocation mockAllocation;

    csr.useNewResourceImplicitFlush = false;
    csr.newResources = false;
    csr.checkForNewResources(10u, GraphicsAllocation::objectNotUsed, mockAllocation);
    EXPECT_FALSE(csr.newResources);
}

TEST(CommandStreamReceiverSimpleTest, givenNewResourceFlushEnabledWhenProvidingNeverUsedAllocationTaskCountThenMarkNewResourceTrue) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);
    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);
    MockGraphicsAllocation mockAllocation;

    csr.useNewResourceImplicitFlush = true;
    csr.newResources = false;
    csr.checkForNewResources(10u, GraphicsAllocation::objectNotUsed, mockAllocation);
    EXPECT_TRUE(csr.newResources);
}

TEST(CommandStreamReceiverSimpleTest, givenNewResourceFlushEnabledWhenProvidingNeverUsedAllocationThatIsKernelIsaThenMarkNewResourceFalse) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);
    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);
    MockGraphicsAllocation mockAllocation;
    mockAllocation.setAllocationType(AllocationType::kernelIsa);

    csr.useNewResourceImplicitFlush = true;
    csr.newResources = false;
    csr.checkForNewResources(10u, GraphicsAllocation::objectNotUsed, mockAllocation);
    EXPECT_FALSE(csr.newResources);
}

TEST(CommandStreamReceiverSimpleTest, givenNewResourceFlushEnabledWhenProvidingAlreadyUsedAllocationTaskCountThenDoNotMarkNewResource) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);
    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);
    MockGraphicsAllocation mockAllocation;

    csr.useNewResourceImplicitFlush = true;
    csr.newResources = false;
    csr.checkForNewResources(10u, 10u, mockAllocation);
    EXPECT_FALSE(csr.newResources);
}

TEST(CommandStreamReceiverSimpleTest, givenNewResourceFlushEnabledWhenProvidingNewAllocationAndVerbosityEnabledThenProvidePrintOfNewAllocationType) {
    DebugManagerStateRestore restore;
    debugManager.flags.ProvideVerboseImplicitFlush.set(true);

    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);
    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);
    MockGraphicsAllocation mockAllocation;

    csr.useNewResourceImplicitFlush = true;
    csr.newResources = false;
    StreamCapture capture;
    capture.captureStdout();
    csr.checkForNewResources(10u, GraphicsAllocation::objectNotUsed, mockAllocation);
    EXPECT_TRUE(csr.newResources);

    std::string output = capture.getCapturedStdout();
    EXPECT_NE(0u, output.size());
    EXPECT_STREQ("New resource detected of type 0\n", output.c_str());
}

TEST(CommandStreamReceiverSimpleTest, givenPrintfTagAllocationAddressFlagEnabledWhenCreatingTagAllocationThenPrintItsAddress) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintTagAllocationAddress.set(true);
    DeviceBitfield deviceBitfield(1);
    auto osContext = std::unique_ptr<OsContext>(OsContext::create(nullptr, 0, 0,
                                                                  EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular})));

    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();

    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);
    csr.setupContext(*osContext);

    StreamCapture capture;
    capture.captureStdout();

    csr.initializeTagAllocation();

    std::string output = capture.getCapturedStdout();
    EXPECT_NE(0u, output.size());

    char expectedStr[128];
    snprintf(expectedStr, 128, "\nCreated tag allocation %p for engine %u\n", csr.getTagAddress(), csr.getOsContext().getEngineType());

    EXPECT_TRUE(hasSubstr(output, std::string(expectedStr)));
}

TEST(CommandStreamReceiverSimpleTest, whenInitializeTagAllocationThenBarrierCountAddressAreSet) {
    DeviceBitfield deviceBitfield(1);
    auto osContext = std::unique_ptr<OsContext>(OsContext::create(nullptr, 0, 0,
                                                                  EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular})));

    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();

    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);
    csr.setupContext(*osContext);

    csr.initializeTagAllocation();

    EXPECT_EQ(csr.getBarrierCountTagAddress(), ptrOffset(csr.getTagAddress(), TagAllocationLayout::barrierCountOffset));
    EXPECT_EQ(csr.getBarrierCountGpuAddress(), ptrOffset(csr.getTagAllocation()->getGpuAddress(), TagAllocationLayout::barrierCountOffset));
    EXPECT_EQ(csr.peekBarrierCount(), 0u);
    EXPECT_EQ(csr.getNextBarrierCount(), 0u);
    EXPECT_EQ(csr.peekBarrierCount(), 1u);
}

TEST(CommandStreamReceiverSimpleTest, givenGpuIdleImplicitFlushCheckDisabledWhenGpuIsIdleThenReturnFalse) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);
    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);

    csr.useGpuIdleImplicitFlush = false;
    csr.mockTagAddress[0] = 1u;
    csr.taskCount = 1u;
    EXPECT_FALSE(csr.checkImplicitFlushForGpuIdle());
}

TEST(CommandStreamReceiverSimpleTest, givenGpuIdleImplicitFlushCheckEnabledWhenGpuIsIdleThenReturnTrue) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);
    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);

    csr.useGpuIdleImplicitFlush = true;
    csr.mockTagAddress[0] = 1u;
    csr.taskCount = 1u;
    EXPECT_TRUE(csr.checkImplicitFlushForGpuIdle());
}

TEST(CommandStreamReceiverSimpleTest, givenGpuNotIdleImplicitFlushCheckEnabledWhenGpuIsIdleThenReturnFalse) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);
    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);

    csr.useGpuIdleImplicitFlush = true;
    csr.mockTagAddress[0] = 1u;
    csr.taskCount = 2u;
    EXPECT_FALSE(csr.checkImplicitFlushForGpuIdle());

    csr.mockTagAddress[0] = 2u;
}

namespace CpuIntrinsicsTests {
extern std::atomic<uint32_t> pauseCounter;
extern volatile TagAddressType *pauseAddress;
extern TaskCountType pauseValue;
extern uint32_t pauseOffset;
} // namespace CpuIntrinsicsTests

TEST(CommandStreamReceiverSimpleTest, givenMultipleActivePartitionsWhenWaitingForTaskCountForCleaningTemporaryAllocationsThenExpectAllPartitionTaskCountsAreChecked) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableWaitpkg.set(0);

    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(0b11);
    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);
    auto osContext = std::unique_ptr<OsContext>(OsContext::create(nullptr, 0, 0,
                                                                  EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular})));
    csr.setupContext(*osContext);

    auto hostPtr = reinterpret_cast<void *>(0x1234);
    size_t size = 100;
    auto gmmHelper = executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(hostPtr));
    auto temporaryAllocation = std::make_unique<MemoryAllocation>(0, 1u /*num gmms*/, AllocationType::externalHostPtr, hostPtr, size, 0,
                                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount, canonizedGpuAddress);
    temporaryAllocation->updateTaskCount(0u, 0u);
    csr.getInternalAllocationStorage()->storeAllocationWithTaskCount(std::move(temporaryAllocation), TEMPORARY_ALLOCATION, 2u);

    csr.immWritePostSyncWriteOffset = 32u;
    csr.mockTagAddress[0] = 0u;
    auto nextPartitionTagAddress = ptrOffset(&csr.mockTagAddress[0], csr.getImmWritePostSyncWriteOffset());
    *nextPartitionTagAddress = 0u;

    csr.taskCount = 3u;
    csr.activePartitions = 2;

    VariableBackup<volatile TagAddressType *> backupPauseAddress(&CpuIntrinsicsTests::pauseAddress);
    VariableBackup<TaskCountType> backupPauseValue(&CpuIntrinsicsTests::pauseValue);
    VariableBackup<uint32_t> backupPauseOffset(&CpuIntrinsicsTests::pauseOffset);

    CpuIntrinsicsTests::pauseAddress = &csr.mockTagAddress[0];
    CpuIntrinsicsTests::pauseValue = 3u;
    CpuIntrinsicsTests::pauseOffset = csr.getImmWritePostSyncWriteOffset();

    CpuIntrinsicsTests::pauseCounter = 0;

    const auto waitStatus = csr.waitForTaskCountAndCleanTemporaryAllocationList(3u);
    EXPECT_EQ(2u, CpuIntrinsicsTests::pauseCounter);
    EXPECT_EQ(WaitStatus::ready, waitStatus);

    CpuIntrinsicsTests::pauseAddress = nullptr;
}

TEST(CommandStreamReceiverSimpleTest, givenEmptyTemporaryAllocationListWhenWaitingForTaskCountForCleaningTemporaryAllocationsThenDoNotWait) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableWaitpkg.set(0);

    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);
    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);

    csr.mockTagAddress[0] = 0u;
    csr.taskCount = 3u;

    VariableBackup<volatile TagAddressType *> backupPauseAddress(&CpuIntrinsicsTests::pauseAddress);
    VariableBackup<TaskCountType> backupPauseValue(&CpuIntrinsicsTests::pauseValue);
    VariableBackup<uint32_t> backupPauseOffset(&CpuIntrinsicsTests::pauseOffset);

    CpuIntrinsicsTests::pauseAddress = &csr.mockTagAddress[0];
    CpuIntrinsicsTests::pauseValue = 3u;

    CpuIntrinsicsTests::pauseCounter = 0;

    const auto waitStatus = csr.waitForTaskCountAndCleanTemporaryAllocationList(3u);
    EXPECT_EQ(0u, CpuIntrinsicsTests::pauseCounter);
    EXPECT_EQ(WaitStatus::ready, waitStatus);

    CpuIntrinsicsTests::pauseAddress = nullptr;
}

TEST(CommandStreamReceiverMultiContextTests, givenMultipleCsrsWhenSameResourcesAreUsedThenResidencyIsProperlyHandled) {
    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get(), 0u));

    auto &commandStreamReceiver0 = *device->commandStreamReceivers[0];
    auto &commandStreamReceiver1 = *device->commandStreamReceivers[1];

    auto csr0ContextId = commandStreamReceiver0.getOsContext().getContextId();
    auto csr1ContextId = commandStreamReceiver1.getOsContext().getContextId();

    MockGraphicsAllocation graphicsAllocation;

    commandStreamReceiver0.makeResident(graphicsAllocation);
    EXPECT_EQ(1u, commandStreamReceiver0.getResidencyAllocations().size());
    EXPECT_EQ(0u, commandStreamReceiver1.getResidencyAllocations().size());

    commandStreamReceiver1.makeResident(graphicsAllocation);
    EXPECT_EQ(1u, commandStreamReceiver0.getResidencyAllocations().size());
    EXPECT_EQ(1u, commandStreamReceiver1.getResidencyAllocations().size());

    auto expectedTaskCount0 = commandStreamReceiver0.peekTaskCount() + 1;
    auto expectedTaskCount1 = commandStreamReceiver1.peekTaskCount() + 1;

    EXPECT_EQ(expectedTaskCount0, graphicsAllocation.getResidencyTaskCount(csr0ContextId));
    EXPECT_EQ(expectedTaskCount1, graphicsAllocation.getResidencyTaskCount(csr1ContextId));

    commandStreamReceiver0.makeNonResident(graphicsAllocation);
    EXPECT_FALSE(graphicsAllocation.isResident(csr0ContextId));
    EXPECT_TRUE(graphicsAllocation.isResident(csr1ContextId));

    commandStreamReceiver1.makeNonResident(graphicsAllocation);
    EXPECT_FALSE(graphicsAllocation.isResident(csr0ContextId));
    EXPECT_FALSE(graphicsAllocation.isResident(csr1ContextId));

    EXPECT_EQ(1u, commandStreamReceiver0.getEvictionAllocations().size());
    EXPECT_EQ(1u, commandStreamReceiver1.getEvictionAllocations().size());
}

struct CreateAllocationForHostSurfaceTest : public ::testing::Test {
    void SetUp() override {
        executionEnvironment.incRefInternal();
        mockMemoryManager = new MockMemoryManager(executionEnvironment);
        executionEnvironment.memoryManager.reset(mockMemoryManager);
        device.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(&hwInfo, &executionEnvironment, 0u));
        commandStreamReceiver = &device->getGpgpuCommandStreamReceiver();
    }
    MockExecutionEnvironment executionEnvironment;
    HardwareInfo hwInfo = *defaultHwInfo;
    MockMemoryManager *mockMemoryManager = nullptr;
    std::unique_ptr<MockDevice> device;
    CommandStreamReceiver *commandStreamReceiver = nullptr;
};

TEST_F(CreateAllocationForHostSurfaceTest, givenTemporaryAllocationWhenCreateAllocationForHostSurfaceThenReuseTemporaryAllocationWhenSizeAndAddressMatch) {
    auto hostPtr = reinterpret_cast<void *>(0x1234);
    size_t size = 100;
    auto gmmHelper = executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(hostPtr));
    auto temporaryAllocation = std::make_unique<MemoryAllocation>(0, 1u /*num gmms*/, AllocationType::externalHostPtr, hostPtr, size, 0,
                                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount, canonizedGpuAddress);
    auto allocationPtr = temporaryAllocation.get();
    temporaryAllocation->updateTaskCount(0u, 0u);
    commandStreamReceiver->getInternalAllocationStorage()->storeAllocation(std::move(temporaryAllocation), TEMPORARY_ALLOCATION);
    *commandStreamReceiver->getTagAddress() = 1u;
    HostPtrSurface hostSurface(hostPtr, size);

    commandStreamReceiver->createAllocationForHostSurface(hostSurface, false);

    auto hostSurfaceAllocationPtr = hostSurface.getAllocation();
    EXPECT_EQ(allocationPtr, hostSurfaceAllocationPtr);
}

class MockCommandStreamReceiverHostPtrCreate : public MockCommandStreamReceiver {
  public:
    MockCommandStreamReceiverHostPtrCreate(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex, const DeviceBitfield deviceBitfield)
        : MockCommandStreamReceiver(executionEnvironment, rootDeviceIndex, deviceBitfield) {}
    bool createAllocationForHostSurface(HostPtrSurface &surface, bool requiresL3Flush) override {
        return CommandStreamReceiver::createAllocationForHostSurface(surface, requiresL3Flush);
    }
};
TEST_F(CreateAllocationForHostSurfaceTest, givenTemporaryAllocationWhenCreateAllocationForHostSurfaceThenHostPtrTaskCountAssignmentWillIncrease) {
    auto mockCsr = std::make_unique<MockCommandStreamReceiverHostPtrCreate>(executionEnvironment, 0u, device->getDeviceBitfield());
    mockCsr->internalAllocationStorage = std::make_unique<InternalAllocationStorage>(*mockCsr.get());
    mockCsr->osContext = &commandStreamReceiver->getOsContext();
    auto hostPtr = reinterpret_cast<void *>(0x1234);
    size_t size = 100;
    auto gmmHelper = executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(hostPtr));
    auto temporaryAllocation = std::make_unique<MemoryAllocation>(0, 1u /*num gmms*/, AllocationType::externalHostPtr, hostPtr, size, 0,
                                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount, canonizedGpuAddress);
    auto allocationPtr = temporaryAllocation.get();
    temporaryAllocation->updateTaskCount(0u, 0u);
    mockCsr->getInternalAllocationStorage()->storeAllocation(std::move(temporaryAllocation), TEMPORARY_ALLOCATION);
    *mockCsr->getTagAddress() = 1u;
    HostPtrSurface hostSurface(hostPtr, size);

    uint32_t valueBefore = allocationPtr->hostPtrTaskCountAssignment;
    mockCsr->createAllocationForHostSurface(hostSurface, false);
    EXPECT_EQ(valueBefore + 1, hostSurface.getAllocation()->hostPtrTaskCountAssignment);
    allocationPtr->hostPtrTaskCountAssignment--;
}

TEST_F(CreateAllocationForHostSurfaceTest, givenTemporaryAllocationWhenCreateAllocationForHostSurfaceThenAllocTaskCountEqualZero) {
    auto hostPtr = reinterpret_cast<void *>(0x1234);
    size_t size = 100;
    auto gmmHelper = executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(hostPtr));
    auto temporaryAllocation = std::make_unique<MemoryAllocation>(0, 1u /*num gmms*/, AllocationType::externalHostPtr, hostPtr, size, 0,
                                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount, canonizedGpuAddress);
    auto allocationPtr = temporaryAllocation.get();
    temporaryAllocation->updateTaskCount(10u, 0u);
    commandStreamReceiver->getInternalAllocationStorage()->storeAllocation(std::move(temporaryAllocation), TEMPORARY_ALLOCATION);
    *commandStreamReceiver->getTagAddress() = 1u;
    HostPtrSurface hostSurface(hostPtr, size);

    EXPECT_EQ(allocationPtr->getTaskCount(0u), 10u);
    commandStreamReceiver->createAllocationForHostSurface(hostSurface, false);
    EXPECT_EQ(allocationPtr->getTaskCount(0u), 0u);
}

TEST_F(CreateAllocationForHostSurfaceTest, whenCreatingAllocationFromHostPtrSurfaceThenLockMutex) {
    const char memory[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    size_t size = sizeof(memory);
    HostPtrSurface surface(const_cast<char *>(memory), size, true);

    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);
    MockCommandStreamReceiver commandStreamReceiver(executionEnvironment, 0u, deviceBitfield);
    auto osContext = executionEnvironment.memoryManager->createAndRegisterOsContext(&commandStreamReceiver, EngineDescriptorHelper::getDefaultDescriptor(deviceBitfield));
    commandStreamReceiver.osContext = osContext;
    EXPECT_EQ(0, commandStreamReceiver.hostPtrSurfaceCreationMutexLockCount);
    commandStreamReceiver.createAllocationForHostSurface(surface, true);
    EXPECT_EQ(1, commandStreamReceiver.hostPtrSurfaceCreationMutexLockCount);
}

TEST_F(CreateAllocationForHostSurfaceTest, givenReadOnlyHostPointerWhenAllocationForHostSurfaceWithPtrCopyAllowedIsCreatedThenCopyAllocationIsCreatedAndMemoryCopied) {
    const char memory[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    size_t size = sizeof(memory);
    HostPtrSurface surface(const_cast<char *>(memory), size, true);
    mockMemoryManager->callBasePopulateOsHandles = false;
    mockMemoryManager->callBaseAllocateGraphicsMemoryForNonSvmHostPtr = false;
    bool runPopulateOsHandlesExpects = false;
    bool runAllocateGraphicsMemoryForNonSvmHostPtrExpects = false;

    if (!mockMemoryManager->useNonSvmHostPtrAlloc(AllocationType::externalHostPtr, device->getRootDeviceIndex())) {
        runPopulateOsHandlesExpects = true;
        mockMemoryManager->populateOsHandlesResult = MemoryManager::AllocationStatus::InvalidHostPointer;
    } else {
        runAllocateGraphicsMemoryForNonSvmHostPtrExpects = true;
    }

    bool result = commandStreamReceiver->createAllocationForHostSurface(surface, false);
    EXPECT_TRUE(result);

    auto allocation = surface.getAllocation();
    ASSERT_NE(nullptr, allocation);

    EXPECT_EQ(1u, allocation->hostPtrTaskCountAssignment.load());
    allocation->hostPtrTaskCountAssignment--;

    EXPECT_NE(memory, allocation->getUnderlyingBuffer());
    EXPECT_EQ(0, memcmp(allocation->getUnderlyingBuffer(), memory, size));

    allocation->updateTaskCount(commandStreamReceiver->peekLatestFlushedTaskCount(), commandStreamReceiver->getOsContext().getContextId());

    if (runPopulateOsHandlesExpects) {
        EXPECT_EQ(1u, mockMemoryManager->populateOsHandlesCalled);
        EXPECT_EQ(device->getRootDeviceIndex(), mockMemoryManager->populateOsHandlesParamsPassed[0].rootDeviceIndex);
    }
    if (runAllocateGraphicsMemoryForNonSvmHostPtrExpects) {
        EXPECT_EQ(1u, mockMemoryManager->allocateGraphicsMemoryForNonSvmHostPtrCalled);
    }
}

TEST_F(CreateAllocationForHostSurfaceTest, givenReadOnlyHostPointerWhenAllocationForHostSurfaceWithPtrCopyNotAllowedIsCreatedThenCopyAllocationIsNotCreated) {
    const char memory[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    size_t size = sizeof(memory);
    HostPtrSurface surface(const_cast<char *>(memory), size, false);
    mockMemoryManager->callBasePopulateOsHandles = false;
    mockMemoryManager->callBaseAllocateGraphicsMemoryForNonSvmHostPtr = false;
    bool runPopulateOsHandlesExpects = false;
    bool runAllocateGraphicsMemoryForNonSvmHostPtrExpects = false;

    if (!mockMemoryManager->useNonSvmHostPtrAlloc(AllocationType::externalHostPtr, device->getRootDeviceIndex())) {
        runPopulateOsHandlesExpects = true;
        mockMemoryManager->populateOsHandlesResult = MemoryManager::AllocationStatus::InvalidHostPointer;
    } else {
        runAllocateGraphicsMemoryForNonSvmHostPtrExpects = true;
    }

    bool result = commandStreamReceiver->createAllocationForHostSurface(surface, false);
    EXPECT_FALSE(result);

    auto allocation = surface.getAllocation();
    EXPECT_EQ(nullptr, allocation);

    if (runPopulateOsHandlesExpects) {
        EXPECT_EQ(1u, mockMemoryManager->populateOsHandlesCalled);
        EXPECT_EQ(device->getRootDeviceIndex(), mockMemoryManager->populateOsHandlesParamsPassed[0].rootDeviceIndex);
    }
    if (runAllocateGraphicsMemoryForNonSvmHostPtrExpects) {
        EXPECT_EQ(1u, mockMemoryManager->allocateGraphicsMemoryForNonSvmHostPtrCalled);
    }
}

struct ReducedAddrSpaceCommandStreamReceiverTest : public CreateAllocationForHostSurfaceTest {
    void SetUp() override {
        hwInfo.capabilityTable.gpuAddressSpace = MemoryConstants::max32BitAddress;
        CreateAllocationForHostSurfaceTest::SetUp();
    }
};

TEST_F(ReducedAddrSpaceCommandStreamReceiverTest,
       givenReducedGpuAddressSpaceWhenAllocationForHostSurfaceIsCreatedThenAllocateGraphicsMemoryForNonSvmHostPtrIsCalled) {
    if (is32bit) {
        GTEST_SKIP();
    }
    mockMemoryManager->callBaseAllocateGraphicsMemoryForNonSvmHostPtr = false;
    mockMemoryManager->callBasePopulateOsHandles = false;

    char memory[8] = {};
    HostPtrSurface surface(memory, sizeof(memory), false);
    bool result = commandStreamReceiver->createAllocationForHostSurface(surface, false);
    EXPECT_FALSE(result);
    EXPECT_EQ(1u, mockMemoryManager->allocateGraphicsMemoryForNonSvmHostPtrCalled);
}

TEST_F(CommandStreamReceiverTest, givenMinimumSizeDoesNotExceedCurrentWhenCallingEnsureCommandBufferAllocationThenDoNotReallocate) {
    GraphicsAllocation *allocation = memoryManager->allocateGraphicsMemoryWithProperties({commandStreamReceiver->getRootDeviceIndex(), 128u, AllocationType::commandBuffer, pDevice->getDeviceBitfield()});
    LinearStream commandStream{allocation};

    commandStreamReceiver->ensureCommandBufferAllocation(commandStream, 100u, 0u);
    EXPECT_EQ(allocation, commandStream.getGraphicsAllocation());
    EXPECT_EQ(MemoryConstants::pageSize, commandStream.getMaxAvailableSpace());

    commandStreamReceiver->ensureCommandBufferAllocation(commandStream, 128u, 0u);
    EXPECT_EQ(allocation, commandStream.getGraphicsAllocation());
    EXPECT_EQ(MemoryConstants::pageSize, commandStream.getMaxAvailableSpace());

    memoryManager->freeGraphicsMemory(commandStream.getGraphicsAllocation());
}

TEST_F(CommandStreamReceiverTest, givenMinimumSizeExceedsCurrentWhenCallingEnsureCommandBufferAllocationThenReallocate) {
    GraphicsAllocation *allocation = memoryManager->allocateGraphicsMemoryWithProperties({commandStreamReceiver->getRootDeviceIndex(), 128u, AllocationType::commandBuffer, pDevice->getDeviceBitfield()});
    LinearStream commandStream{allocation};

    commandStreamReceiver->ensureCommandBufferAllocation(commandStream, MemoryConstants::pageSize + 1, 0u);
    EXPECT_NE(allocation, commandStream.getGraphicsAllocation());
    memoryManager->freeGraphicsMemory(commandStream.getGraphicsAllocation());
}

TEST_F(CommandStreamReceiverTest, givenMinimumSizeExceedsCurrentWhenCallingEnsureCommandBufferAllocationThenReallocateAndAlignSizeTo64kb) {
    GraphicsAllocation *allocation = memoryManager->allocateGraphicsMemoryWithProperties({commandStreamReceiver->getRootDeviceIndex(), 128u, AllocationType::commandBuffer, pDevice->getDeviceBitfield()});
    LinearStream commandStream{allocation};

    commandStreamReceiver->ensureCommandBufferAllocation(commandStream, MemoryConstants::pageSize + 1, 0u);

    EXPECT_EQ(MemoryConstants::pageSize64k, commandStream.getGraphicsAllocation()->getUnderlyingBufferSize());
    EXPECT_EQ(MemoryConstants::pageSize64k, commandStream.getMaxAvailableSpace());

    commandStreamReceiver->ensureCommandBufferAllocation(commandStream, MemoryConstants::pageSize64k + 1u, 0u);

    EXPECT_EQ(2 * MemoryConstants::pageSize64k, commandStream.getGraphicsAllocation()->getUnderlyingBufferSize());
    EXPECT_EQ(2 * MemoryConstants::pageSize64k, commandStream.getMaxAvailableSpace());

    memoryManager->freeGraphicsMemory(commandStream.getGraphicsAllocation());
}

TEST_F(CommandStreamReceiverTest, givenForceCommandBufferAlignmentWhenEnsureCommandBufferAllocationThenItHasProperAlignment) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceCommandBufferAlignment.set(2048);

    GraphicsAllocation *allocation = memoryManager->allocateGraphicsMemoryWithProperties({commandStreamReceiver->getRootDeviceIndex(), 128u, AllocationType::commandBuffer, pDevice->getDeviceBitfield()});
    LinearStream commandStream{allocation};

    commandStreamReceiver->ensureCommandBufferAllocation(commandStream, MemoryConstants::pageSize + 1, 0u);
    EXPECT_EQ(2 * MemoryConstants::megaByte, commandStream.getGraphicsAllocation()->getUnderlyingBufferSize());
    EXPECT_EQ(2 * MemoryConstants::megaByte, commandStream.getMaxAvailableSpace());

    memoryManager->freeGraphicsMemory(commandStream.getGraphicsAllocation());
}

TEST_F(CommandStreamReceiverTest, givenAdditionalAllocationSizeWhenCallingEnsureCommandBufferAllocationThenSizesOfAllocationAndCommandBufferAreCorrect) {
    GraphicsAllocation *allocation = memoryManager->allocateGraphicsMemoryWithProperties({commandStreamReceiver->getRootDeviceIndex(), 128u, AllocationType::commandBuffer, pDevice->getDeviceBitfield()});
    LinearStream commandStream{allocation};

    commandStreamReceiver->ensureCommandBufferAllocation(commandStream, MemoryConstants::pageSize + 1, 350u);
    EXPECT_NE(allocation, commandStream.getGraphicsAllocation());
    EXPECT_EQ(MemoryConstants::pageSize64k, commandStream.getGraphicsAllocation()->getUnderlyingBufferSize());
    EXPECT_EQ(MemoryConstants::pageSize64k - 350u, commandStream.getMaxAvailableSpace());

    memoryManager->freeGraphicsMemory(commandStream.getGraphicsAllocation());
}

TEST_F(CommandStreamReceiverTest, givenMinimumSizeExceedsCurrentAndNoAllocationsForReuseWhenCallingEnsureCommandBufferAllocationThenAllocateMemoryFromMemoryManager) {
    LinearStream commandStream;

    EXPECT_TRUE(internalAllocationStorage->getAllocationsForReuse().peekIsEmpty());
    commandStreamReceiver->ensureCommandBufferAllocation(commandStream, 1u, 0u);
    EXPECT_NE(nullptr, commandStream.getGraphicsAllocation());

    memoryManager->freeGraphicsMemory(commandStream.getGraphicsAllocation());
}

TEST_F(CommandStreamReceiverTest, givenMinimumSizeExceedsCurrentAndAllocationsForReuseWhenCallingEnsureCommandBufferAllocationThenObtainAllocationFromInternalAllocationStorage) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties({commandStreamReceiver->getRootDeviceIndex(), MemoryConstants::pageSize64k, AllocationType::commandBuffer, pDevice->getDeviceBitfield()});
    internalAllocationStorage->storeAllocation(std::unique_ptr<GraphicsAllocation>{allocation}, REUSABLE_ALLOCATION);
    LinearStream commandStream;

    EXPECT_FALSE(internalAllocationStorage->getAllocationsForReuse().peekIsEmpty());
    commandStreamReceiver->ensureCommandBufferAllocation(commandStream, 1u, 0u);
    EXPECT_EQ(allocation, commandStream.getGraphicsAllocation());
    EXPECT_TRUE(internalAllocationStorage->getAllocationsForReuse().peekIsEmpty());

    memoryManager->freeGraphicsMemory(commandStream.getGraphicsAllocation());
}

HWTEST_F(CommandStreamReceiverTest, givenMinimumSizeExceedsCurrentAndEarlyPreallocatedAllocationInReuseListWhenCallingEnsureCommandBufferAllocationThenObtainAllocationFromInternalAllocationStorage) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.SetAmountOfReusableAllocations.set(1);
    debugManager.flags.SetAmountOfInternalHeapsToPreallocate.set(0);
    pDevice->getUltCommandStreamReceiver<FamilyType>().callBaseFillReusableAllocationsList = true;

    commandStreamReceiver->fillReusableAllocationsList();
    auto allocation = internalAllocationStorage->getAllocationsForReuse().peekHead();

    LinearStream commandStream;

    EXPECT_FALSE(internalAllocationStorage->getAllocationsForReuse().peekIsEmpty());
    commandStreamReceiver->ensureCommandBufferAllocation(commandStream, 1u, 0u);
    EXPECT_EQ(allocation, commandStream.getGraphicsAllocation());
    EXPECT_TRUE(internalAllocationStorage->getAllocationsForReuse().peekIsEmpty());

    memoryManager->freeGraphicsMemory(commandStream.getGraphicsAllocation());
}

TEST_F(CommandStreamReceiverTest, givenMinimumSizeExceedsCurrentAndNoSuitableReusableAllocationWhenCallingEnsureCommandBufferAllocationThenObtainAllocationMemoryManager) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties({commandStreamReceiver->getRootDeviceIndex(), MemoryConstants::pageSize64k, AllocationType::commandBuffer, pDevice->getDeviceBitfield()});
    internalAllocationStorage->storeAllocation(std::unique_ptr<GraphicsAllocation>{allocation}, REUSABLE_ALLOCATION);
    LinearStream commandStream;

    EXPECT_FALSE(internalAllocationStorage->getAllocationsForReuse().peekIsEmpty());
    commandStreamReceiver->ensureCommandBufferAllocation(commandStream, MemoryConstants::pageSize64k + 1, 0u);
    EXPECT_NE(allocation, commandStream.getGraphicsAllocation());
    EXPECT_NE(nullptr, commandStream.getGraphicsAllocation());
    EXPECT_FALSE(internalAllocationStorage->getAllocationsForReuse().peekIsEmpty());

    memoryManager->freeGraphicsMemory(commandStream.getGraphicsAllocation());
}

HWTEST_F(CommandStreamReceiverTest, whenCreatingCommandStreamReceiverThenLastAddtionalKernelExecInfoValueIsCorrect) {
    int32_t executionStamp = 0;
    std::unique_ptr<MockCsr<FamilyType>> mockCSR(new MockCsr<FamilyType>(executionStamp, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    EXPECT_EQ(AdditionalKernelExecInfo::notSet, mockCSR->lastAdditionalKernelExecInfo);
}

HWTEST_F(CommandStreamReceiverTest, givenDebugFlagWhenCreatingCsrThenSetEnableStaticPartitioningAccordingly) {
    DebugManagerStateRestore restore{};
    debugManager.flags.EnableImplicitScaling.set(1);

    {
        UltDeviceFactory deviceFactory{1, 2};
        MockDevice &device = *deviceFactory.rootDevices[0];
        EXPECT_TRUE(device.getGpgpuCommandStreamReceiver().isStaticWorkPartitioningEnabled());
        EXPECT_NE(0u, device.getGpgpuCommandStreamReceiver().getWorkPartitionAllocationGpuAddress());
        const auto gpuVa = device.getGpgpuCommandStreamReceiver().getWorkPartitionAllocation()->getGpuAddress();
        EXPECT_EQ(gpuVa, device.getGpgpuCommandStreamReceiver().getWorkPartitionAllocationGpuAddress());
    }
    {
        debugManager.flags.EnableStaticPartitioning.set(0);
        UltDeviceFactory deviceFactory{1, 2};
        MockDevice &device = *deviceFactory.rootDevices[0];
        EXPECT_FALSE(device.getGpgpuCommandStreamReceiver().isStaticWorkPartitioningEnabled());
        EXPECT_EQ(0u, device.getGpgpuCommandStreamReceiver().getWorkPartitionAllocationGpuAddress());
    }
    {
        debugManager.flags.EnableStaticPartitioning.set(1);
        UltDeviceFactory deviceFactory{1, 2};
        MockDevice &device = *deviceFactory.rootDevices[0];
        EXPECT_TRUE(device.getGpgpuCommandStreamReceiver().isStaticWorkPartitioningEnabled());
        EXPECT_NE(0u, device.getGpgpuCommandStreamReceiver().getWorkPartitionAllocationGpuAddress());
        const auto gpuVa = device.getGpgpuCommandStreamReceiver().getWorkPartitionAllocation()->getGpuAddress();
        EXPECT_EQ(gpuVa, device.getGpgpuCommandStreamReceiver().getWorkPartitionAllocationGpuAddress());
    }
}

HWTEST_F(CommandStreamReceiverTest, whenCreatingWorkPartitionAllocationThenInitializeContentsWithCopyEngine) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    REQUIRE_BLITTER_OR_SKIP(*mockExecutionEnvironment.rootDeviceEnvironments[0].get());
    DebugManagerStateRestore restore{};
    debugManager.flags.EnableStaticPartitioning.set(0);

    constexpr size_t subDeviceCount = 3;
    UltDeviceFactory deviceFactory{1, subDeviceCount};
    MockDevice &rootDevice = *deviceFactory.rootDevices[0];
    rootDevice.getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.blitterOperationsSupported = true;
    rootDevice.getRootDeviceEnvironment().getMutableHardwareInfo()->featureTable.ftrBcsInfo = 1;
    UltCommandStreamReceiver<FamilyType> &csr = rootDevice.getUltCommandStreamReceiver<FamilyType>();
    UltCommandStreamReceiver<FamilyType> *bcsCsrs[] = {
        reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(rootDevice.getSubDevice(0)->getEngine(aub_stream::ENGINE_BCS, EngineUsage::regular).commandStreamReceiver),
        reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(rootDevice.getSubDevice(1)->getEngine(aub_stream::ENGINE_BCS, EngineUsage::regular).commandStreamReceiver),
        reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(rootDevice.getSubDevice(2)->getEngine(aub_stream::ENGINE_BCS, EngineUsage::regular).commandStreamReceiver),
    };
    const size_t bcsStarts[] = {
        bcsCsrs[0]->commandStream.getUsed(),
        bcsCsrs[1]->commandStream.getUsed(),
        bcsCsrs[2]->commandStream.getUsed(),
    };

    csr.staticWorkPartitioningEnabled = true;
    EXPECT_TRUE(csr.createWorkPartitionAllocation(rootDevice));
    EXPECT_NE(nullptr, csr.getWorkPartitionAllocation());

    EXPECT_LT(bcsStarts[0], bcsCsrs[0]->commandStream.getUsed());
    EXPECT_LT(bcsStarts[1], bcsCsrs[1]->commandStream.getUsed());
    EXPECT_LT(bcsStarts[2], bcsCsrs[2]->commandStream.getUsed());
}

HWTEST_F(CommandStreamReceiverTest, givenFailingMemoryManagerWhenCreatingWorkPartitionAllocationThenReturnFalse) {
    struct FailingMemoryManager : OsAgnosticMemoryManager {
        using OsAgnosticMemoryManager::OsAgnosticMemoryManager;
        GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties) override {
            return nullptr;
        }
    };

    DebugManagerStateRestore restore{};
    debugManager.flags.EnableStaticPartitioning.set(0);
    UltDeviceFactory deviceFactory{1, 2};
    MockDevice &rootDevice = *deviceFactory.rootDevices[0];
    UltCommandStreamReceiver<FamilyType> &csr = rootDevice.getUltCommandStreamReceiver<FamilyType>();

    ExecutionEnvironment &executionEnvironment = *deviceFactory.rootDevices[0]->executionEnvironment;
    executionEnvironment.memoryManager = std::make_unique<FailingMemoryManager>(executionEnvironment);

    csr.staticWorkPartitioningEnabled = true;
    debugManager.flags.EnableStaticPartitioning.set(1);
    EXPECT_FALSE(csr.createWorkPartitionAllocation(rootDevice));
    EXPECT_EQ(nullptr, csr.getWorkPartitionAllocation());
}

class CommandStreamReceiverWithAubSubCaptureTest : public CommandStreamReceiverTest,
                                                   public ::testing::WithParamInterface<std::pair<bool, bool>> {};

HWTEST_P(CommandStreamReceiverWithAubSubCaptureTest, givenCommandStreamReceiverWhenProgramForAubSubCaptureIsCalledThenProgramCsrDependsOnAubSubCaptureStatus) {
    class MyMockCsr : public MockCommandStreamReceiver {
      public:
        using MockCommandStreamReceiver::MockCommandStreamReceiver;

        void initProgrammingFlags() override {
            initProgrammingFlagsCalled = true;
        }
        bool flushBatchedSubmissions() override {
            flushBatchedSubmissionsCalled = true;
            return true;
        }
        bool initProgrammingFlagsCalled = false;
        bool flushBatchedSubmissionsCalled = false;
    };

    auto status = GetParam();
    bool wasActiveInPreviousEnqueue = status.first;
    bool isActive = status.second;

    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    MyMockCsr mockCsr(executionEnvironment, 0, 1);

    mockCsr.programForAubSubCapture(wasActiveInPreviousEnqueue, isActive);

    EXPECT_EQ(!wasActiveInPreviousEnqueue && isActive, mockCsr.initProgrammingFlagsCalled);
    EXPECT_EQ(wasActiveInPreviousEnqueue && !isActive, mockCsr.flushBatchedSubmissionsCalled);
}

std::pair<bool, bool> aubSubCaptureStatus[] = {
    {false, false},
    {false, true},
    {true, false},
    {true, true},
};

INSTANTIATE_TEST_SUITE_P(
    CommandStreamReceiverWithAubSubCaptureTest_program,
    CommandStreamReceiverWithAubSubCaptureTest,
    testing::ValuesIn(aubSubCaptureStatus));

using SimulatedCommandStreamReceiverTest = ::testing::Test;

template <typename FamilyType>
struct MockSimulatedCsrHw : public CommandStreamReceiverSimulatedHw<FamilyType> {
    using CommandStreamReceiverSimulatedHw<FamilyType>::CommandStreamReceiverSimulatedHw;
    using CommandStreamReceiverSimulatedHw<FamilyType>::getDeviceIndex;
    void pollForCompletion(bool skipTaskCountCheck) override {}
    void initializeEngine() override {}
    bool writeMemory(GraphicsAllocation &gfxAllocation) override { return true; }
    void writeMemory(uint64_t gpuAddress, void *cpuAddress, size_t size, uint32_t memoryBank, uint64_t entryBits) override {}
    void writeMMIO(uint32_t offset, uint32_t value) override {}
    void dumpAllocation(GraphicsAllocation &gfxAllocation) override {}
};

HWTEST_F(SimulatedCommandStreamReceiverTest, givenCsrWithOsContextWhenGetDeviceIndexThenGetHighestEnabledBitInDeviceBitfield) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(0b11);
    MockSimulatedCsrHw<FamilyType> csr(executionEnvironment, 0, deviceBitfield);
    auto osContext = executionEnvironment.memoryManager->createAndRegisterOsContext(&csr, EngineDescriptorHelper::getDefaultDescriptor(deviceBitfield));

    csr.setupContext(*osContext);
    EXPECT_EQ(1u, csr.getDeviceIndex());
}

HWTEST_F(SimulatedCommandStreamReceiverTest, givenOsContextWithNoDeviceBitfieldWhenGettingDeviceIndexThenZeroIsReturned) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(0b00);
    MockSimulatedCsrHw<FamilyType> csr(executionEnvironment, 0, deviceBitfield);
    auto osContext = executionEnvironment.memoryManager->createAndRegisterOsContext(&csr, EngineDescriptorHelper::getDefaultDescriptor(deviceBitfield));

    csr.setupContext(*osContext);
    EXPECT_EQ(0u, csr.getDeviceIndex());
}

using CommandStreamReceiverPageTableManagerTest = ::testing::Test;

TEST_F(CommandStreamReceiverPageTableManagerTest, givenExistingPageTableManagerWhenNeedsPageTableManagerIsCalledThenFalseIsReturned) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);
    MockCommandStreamReceiver commandStreamReceiver(executionEnvironment, 0u, deviceBitfield);

    GmmPageTableMngr *dummyPageTableManager = reinterpret_cast<GmmPageTableMngr *>(0x1234);

    commandStreamReceiver.pageTableManager.reset(dummyPageTableManager);
    EXPECT_FALSE(commandStreamReceiver.needsPageTableManager());
    commandStreamReceiver.pageTableManager.release();
}

TEST_F(CommandStreamReceiverPageTableManagerTest, givenNonExisitingPageTableManagerWhenNeedsPageTableManagerIsCalledThenSupportOfPageTableManagerIsReturned) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);
    MockCommandStreamReceiver commandStreamReceiver(executionEnvironment, 0u, deviceBitfield);
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo();
    auto &productHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    bool supportsPageTableManager = productHelper.isPageTableManagerSupported(*hwInfo);
    EXPECT_EQ(nullptr, commandStreamReceiver.pageTableManager.get());

    EXPECT_EQ(supportsPageTableManager, commandStreamReceiver.needsPageTableManager());
}

TEST(CreateWorkPartitionAllocationTest, givenDisabledBlitterWhenInitializingWorkPartitionAllocationThenFallbackToCpuCopy) {
    DebugManagerStateRestore restore{};
    debugManager.flags.EnableImplicitScaling.set(1);

    UltDeviceFactory deviceFactory{1, 2};
    MockDevice &device = *deviceFactory.rootDevices[0];

    auto memoryManager = static_cast<MockMemoryManager *>(device.getMemoryManager());
    auto commandStreamReceiver = device.getDefaultEngine().commandStreamReceiver;
    memoryManager->freeGraphicsMemory(commandStreamReceiver->getWorkPartitionAllocation());

    debugManager.flags.EnableBlitterOperationsSupport.set(0);
    memoryManager->copyMemoryToAllocationBanksCalled = 0u;
    memoryManager->copyMemoryToAllocationBanksParamsPassed.clear();
    auto retVal = commandStreamReceiver->createWorkPartitionAllocation(device);
    EXPECT_TRUE(retVal);
    EXPECT_EQ(2u, memoryManager->copyMemoryToAllocationBanksCalled);
    EXPECT_EQ(deviceFactory.subDevices[0]->getDeviceBitfield(), memoryManager->copyMemoryToAllocationBanksParamsPassed[0].handleMask);
    EXPECT_EQ(deviceFactory.subDevices[1]->getDeviceBitfield(), memoryManager->copyMemoryToAllocationBanksParamsPassed[1].handleMask);
    for (auto i = 0; i < 2; i++) {
        EXPECT_EQ(commandStreamReceiver->getWorkPartitionAllocation(), memoryManager->copyMemoryToAllocationBanksParamsPassed[i].graphicsAllocation);
        EXPECT_EQ(2 * sizeof(uint32_t), memoryManager->copyMemoryToAllocationBanksParamsPassed[i].sizeToCopy);
        EXPECT_NE(nullptr, memoryManager->copyMemoryToAllocationBanksParamsPassed[i].memoryToCopy);
    }
}

TEST(CreateWorkPartitionAllocationTest, givenEnabledBlitterWhenInitializingWorkPartitionAllocationThenDontCopyOnCpu) {
    DebugManagerStateRestore restore{};
    debugManager.flags.EnableImplicitScaling.set(1);
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());

    defaultHwInfo->capabilityTable.blitterOperationsSupported = true;

    UltDeviceFactory deviceFactory{1, 2};
    MockDevice &device = *deviceFactory.rootDevices[0];
    auto memoryManager = static_cast<MockMemoryManager *>(device.getMemoryManager());
    auto commandStreamReceiver = device.getDefaultEngine().commandStreamReceiver;

    REQUIRE_BLITTER_OR_SKIP(device.getRootDeviceEnvironment());
    memoryManager->freeGraphicsMemory(commandStreamReceiver->getWorkPartitionAllocation());

    memoryManager->copyMemoryToAllocationBanksCalled = 0u;
    memoryManager->copyMemoryToAllocationBanksParamsPassed.clear();
    auto retVal = commandStreamReceiver->createWorkPartitionAllocation(device);
    EXPECT_TRUE(retVal);
    EXPECT_EQ(0u, memoryManager->copyMemoryToAllocationBanksCalled);
}

HWTEST_F(CommandStreamReceiverTest, givenMultipleActivePartitionsWhenWaitLogIsEnabledThenPrintTagValueForAllPartitions) {
    DebugManagerStateRestore restorer;
    debugManager.flags.LogWaitingForCompletion.set(true);

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.activePartitions = 2;

    volatile TagAddressType *tagAddress = csr.tagAddress;
    constexpr TagAddressType tagValue = 2;
    *tagAddress = tagValue;
    tagAddress = ptrOffset(tagAddress, csr.immWritePostSyncWriteOffset);
    *tagAddress = tagValue;

    WaitParams waitParams;
    waitParams.waitTimeout = std::numeric_limits<int64_t>::max();
    constexpr TaskCountType taskCount = 1;

    StreamCapture capture;
    capture.captureStdout();

    WaitStatus status = csr.waitForCompletionWithTimeout(waitParams, taskCount);
    EXPECT_EQ(WaitStatus::ready, status);

    std::string output = capture.getCapturedStdout();

    std::stringstream expectedOutput;

    expectedOutput << std::endl
                   << "Waiting for task count " << taskCount
                   << " at location " << const_cast<TagAddressType *>(csr.tagAddress)
                   << " with timeout " << std::hex << waitParams.waitTimeout
                   << ". Current value: " << std::dec << tagValue
                   << " " << tagValue
                   << std::endl
                   << std::endl
                   << "Waiting completed. Current value: " << tagValue
                   << " " << tagValue << std::endl;

    EXPECT_STREQ(expectedOutput.str().c_str(), output.c_str());
}

HWTEST_F(CommandStreamReceiverTest, givenAubCsrWhenLogWaitingForCompletionEnabledThenSkipFullPrint) {
    DebugManagerStateRestore restorer;
    debugManager.flags.LogWaitingForCompletion.set(true);

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.commandStreamReceiverType = NEO::CommandStreamReceiverType::aub;

    WaitParams waitParams;
    waitParams.waitTimeout = std::numeric_limits<int64_t>::max();

    StreamCapture capture;
    capture.captureStdout();

    WaitStatus status = csr.waitForCompletionWithTimeout(waitParams, 0);
    EXPECT_EQ(WaitStatus::ready, status);

    std::string output = capture.getCapturedStdout();

    std::string expectedOutput1 = "Aub dump wait for task count";
    std::string expectedOutput2 = "Aub dump wait completed";
    std::string notExpectedOutput = "Waiting for task count";

    EXPECT_NE(std::string::npos, output.find(expectedOutput1));
    EXPECT_NE(std::string::npos, output.find(expectedOutput2));
    EXPECT_EQ(std::string::npos, output.find(notExpectedOutput));
}

TEST_F(CommandStreamReceiverTest, givenPreambleFlagIsSetWhenGettingFlagStateThenExpectCorrectState) {
    EXPECT_FALSE(commandStreamReceiver->getPreambleSetFlag());
    commandStreamReceiver->setPreambleSetFlag(true);
    EXPECT_TRUE(commandStreamReceiver->getPreambleSetFlag());
}

TEST_F(CommandStreamReceiverTest, givenPreemptionSentIsInitialWhenSettingPreemptionToNewModeThenExpectCorrectPreemption) {
    PreemptionMode mode = PreemptionMode::Initial;
    if (!heaplessStateInit) {
        EXPECT_EQ(mode, commandStreamReceiver->getPreemptionMode());
    }
    mode = PreemptionMode::ThreadGroup;
    commandStreamReceiver->setPreemptionMode(mode);
    EXPECT_EQ(mode, commandStreamReceiver->getPreemptionMode());
}

using CommandStreamReceiverSystolicTests = Test<CommandStreamReceiverSystolicFixture>;
using SystolicSupport = IsAnyProducts<IGFX_ALDERLAKE_P, IGFX_DG2, IGFX_PVC>;

HWTEST2_F(CommandStreamReceiverSystolicTests, givenSystolicModeChangedWhenFlushTaskCalledThenSystolicStateIsUpdated, SystolicSupport) {
    testBody<FamilyType>();
}

HWTEST_F(CommandStreamReceiverTest, givenSshDirtyStateWhenUpdatingStateWithNewHeapThenExpectDirtyStateTrue) {
    MockGraphicsAllocation allocation{};
    allocation.gpuAddress = 0xABC000;
    allocation.size = 0x1000;

    IndirectHeap dummyHeap(&allocation, false);

    auto dirtyStateCopy = static_cast<CommandStreamReceiverHw<FamilyType> *>(commandStreamReceiver)->getSshState();

    bool check = dirtyStateCopy.updateAndCheck(&dummyHeap);
    EXPECT_TRUE(check);

    check = dirtyStateCopy.updateAndCheck(&dummyHeap);
    EXPECT_FALSE(check);

    auto &dirtyState = static_cast<CommandStreamReceiverHw<FamilyType> *>(commandStreamReceiver)->getSshState();

    check = dirtyState.updateAndCheck(&dummyHeap);
    EXPECT_TRUE(check);

    check = dirtyState.updateAndCheck(&dummyHeap);
    EXPECT_FALSE(check);
}

HWTEST_F(CommandStreamReceiverTest, givenDshDirtyStateWhenUpdatingStateWithNewHeapThenExpectDirtyStateTrue) {
    MockGraphicsAllocation allocation{};
    allocation.gpuAddress = 0xABCD00;
    allocation.size = 0x1000;

    IndirectHeap dummyHeap(&allocation, false);

    auto dirtyStateCopy = static_cast<CommandStreamReceiverHw<FamilyType> *>(commandStreamReceiver)->getDshState();

    bool check = dirtyStateCopy.updateAndCheck(&dummyHeap);
    EXPECT_TRUE(check);

    check = dirtyStateCopy.updateAndCheck(&dummyHeap);
    EXPECT_FALSE(check);

    auto &dirtyState = static_cast<CommandStreamReceiverHw<FamilyType> *>(commandStreamReceiver)->getDshState();

    check = dirtyState.updateAndCheck(&dummyHeap);
    EXPECT_TRUE(check);

    check = dirtyState.updateAndCheck(&dummyHeap);
    EXPECT_FALSE(check);
}

HWTEST_F(CommandStreamReceiverTest, givenIohDirtyStateWhenUpdatingStateWithNewHeapThenExpectDirtyStateTrue) {
    MockGraphicsAllocation allocation{};
    allocation.gpuAddress = 0xABC000;
    allocation.size = 0x1000;

    IndirectHeap dummyHeap(&allocation, false);

    auto dirtyStateCopy = static_cast<CommandStreamReceiverHw<FamilyType> *>(commandStreamReceiver)->getIohState();

    bool check = dirtyStateCopy.updateAndCheck(&dummyHeap);
    EXPECT_TRUE(check);

    check = dirtyStateCopy.updateAndCheck(&dummyHeap);
    EXPECT_FALSE(check);

    auto &dirtyState = static_cast<CommandStreamReceiverHw<FamilyType> *>(commandStreamReceiver)->getIohState();

    check = dirtyState.updateAndCheck(&dummyHeap);
    EXPECT_TRUE(check);

    check = dirtyState.updateAndCheck(&dummyHeap);
    EXPECT_FALSE(check);
}

HWTEST_F(CommandStreamReceiverTest, givenFrontEndStateNotInitedWhenTransitionFrontEndPropertiesThenExpectCorrectValuesStored) {
    auto dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.feSupportFlags.computeDispatchAllWalker = false;
    commandStreamReceiver.feSupportFlags.disableEuFusion = false;
    commandStreamReceiver.setMediaVFEStateDirty(false);

    commandStreamReceiver.feSupportFlags.disableOverdispatch = true;
    dispatchFlags.additionalKernelExecInfo = AdditionalKernelExecInfo::notApplicable;
    commandStreamReceiver.handleFrontEndStateTransition(dispatchFlags);
    EXPECT_FALSE(commandStreamReceiver.getMediaVFEStateDirty());

    dispatchFlags.additionalKernelExecInfo = AdditionalKernelExecInfo::notSet;
    commandStreamReceiver.handleFrontEndStateTransition(dispatchFlags);
    EXPECT_FALSE(commandStreamReceiver.getMediaVFEStateDirty());

    dispatchFlags.additionalKernelExecInfo = AdditionalKernelExecInfo::disableOverdispatch;
    commandStreamReceiver.handleFrontEndStateTransition(dispatchFlags);
    EXPECT_TRUE(commandStreamReceiver.getMediaVFEStateDirty());
    commandStreamReceiver.setMediaVFEStateDirty(false);

    commandStreamReceiver.feSupportFlags.disableOverdispatch = false;
    commandStreamReceiver.lastAdditionalKernelExecInfo = AdditionalKernelExecInfo::notSet;
    commandStreamReceiver.handleFrontEndStateTransition(dispatchFlags);
    EXPECT_FALSE(commandStreamReceiver.getMediaVFEStateDirty());

    commandStreamReceiver.feSupportFlags.computeDispatchAllWalker = true;
    dispatchFlags.kernelExecutionType = KernelExecutionType::notApplicable;
    commandStreamReceiver.handleFrontEndStateTransition(dispatchFlags);
    EXPECT_FALSE(commandStreamReceiver.getMediaVFEStateDirty());

    dispatchFlags.kernelExecutionType = KernelExecutionType::defaultType;
    commandStreamReceiver.handleFrontEndStateTransition(dispatchFlags);
    EXPECT_FALSE(commandStreamReceiver.getMediaVFEStateDirty());

    dispatchFlags.kernelExecutionType = KernelExecutionType::concurrent;
    commandStreamReceiver.handleFrontEndStateTransition(dispatchFlags);
    EXPECT_TRUE(commandStreamReceiver.getMediaVFEStateDirty());
    commandStreamReceiver.setMediaVFEStateDirty(false);

    commandStreamReceiver.feSupportFlags.computeDispatchAllWalker = false;
    commandStreamReceiver.lastKernelExecutionType = KernelExecutionType::defaultType;
    commandStreamReceiver.handleFrontEndStateTransition(dispatchFlags);
    EXPECT_FALSE(commandStreamReceiver.getMediaVFEStateDirty());

    commandStreamReceiver.feSupportFlags.disableEuFusion = true;
    dispatchFlags.disableEUFusion = false;
    commandStreamReceiver.handleFrontEndStateTransition(dispatchFlags);
    EXPECT_TRUE(commandStreamReceiver.getMediaVFEStateDirty());
    commandStreamReceiver.setMediaVFEStateDirty(false);

    commandStreamReceiver.streamProperties.frontEndState.disableEUFusion.value = 0;
    dispatchFlags.disableEUFusion = true;
    commandStreamReceiver.handleFrontEndStateTransition(dispatchFlags);
    EXPECT_TRUE(commandStreamReceiver.getMediaVFEStateDirty());
    commandStreamReceiver.setMediaVFEStateDirty(false);

    dispatchFlags.disableEUFusion = false;
    commandStreamReceiver.handleFrontEndStateTransition(dispatchFlags);
    EXPECT_FALSE(commandStreamReceiver.getMediaVFEStateDirty());

    commandStreamReceiver.feSupportFlags.disableEuFusion = false;
    commandStreamReceiver.streamProperties.frontEndState.disableEUFusion.value = -1;
    dispatchFlags.disableEUFusion = false;
    commandStreamReceiver.handleFrontEndStateTransition(dispatchFlags);
    EXPECT_FALSE(commandStreamReceiver.getMediaVFEStateDirty());
}

HWTEST_F(CommandStreamReceiverTest, givenFrontEndStateInitedWhenTransitionFrontEndPropertiesThenExpectCorrectValuesStored) {
    auto dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.feSupportFlags.computeDispatchAllWalker = false;
    commandStreamReceiver.feSupportFlags.disableEuFusion = false;
    commandStreamReceiver.setMediaVFEStateDirty(false);

    commandStreamReceiver.feSupportFlags.disableOverdispatch = true;

    commandStreamReceiver.streamProperties.frontEndState.disableOverdispatch.value = 0;
    dispatchFlags.additionalKernelExecInfo = AdditionalKernelExecInfo::notSet;
    commandStreamReceiver.handleFrontEndStateTransition(dispatchFlags);
    EXPECT_FALSE(commandStreamReceiver.getMediaVFEStateDirty());

    dispatchFlags.additionalKernelExecInfo = AdditionalKernelExecInfo::disableOverdispatch;
    commandStreamReceiver.handleFrontEndStateTransition(dispatchFlags);
    EXPECT_TRUE(commandStreamReceiver.getMediaVFEStateDirty());
    commandStreamReceiver.setMediaVFEStateDirty(false);

    commandStreamReceiver.streamProperties.frontEndState.disableOverdispatch.value = 1;
    dispatchFlags.additionalKernelExecInfo = AdditionalKernelExecInfo::notSet;
    commandStreamReceiver.handleFrontEndStateTransition(dispatchFlags);
    EXPECT_TRUE(commandStreamReceiver.getMediaVFEStateDirty());
    commandStreamReceiver.setMediaVFEStateDirty(false);

    commandStreamReceiver.feSupportFlags.disableOverdispatch = false;
    commandStreamReceiver.feSupportFlags.computeDispatchAllWalker = true;

    commandStreamReceiver.streamProperties.frontEndState.computeDispatchAllWalkerEnable.value = 0;
    dispatchFlags.kernelExecutionType = KernelExecutionType::defaultType;
    commandStreamReceiver.handleFrontEndStateTransition(dispatchFlags);
    EXPECT_FALSE(commandStreamReceiver.getMediaVFEStateDirty());

    dispatchFlags.kernelExecutionType = KernelExecutionType::concurrent;
    commandStreamReceiver.handleFrontEndStateTransition(dispatchFlags);
    EXPECT_TRUE(commandStreamReceiver.getMediaVFEStateDirty());
    commandStreamReceiver.setMediaVFEStateDirty(false);

    commandStreamReceiver.streamProperties.frontEndState.computeDispatchAllWalkerEnable.value = 1;
    dispatchFlags.kernelExecutionType = KernelExecutionType::defaultType;
    commandStreamReceiver.handleFrontEndStateTransition(dispatchFlags);
    EXPECT_TRUE(commandStreamReceiver.getMediaVFEStateDirty());
    commandStreamReceiver.setMediaVFEStateDirty(false);
}

HWTEST_F(CommandStreamReceiverTest, givenPipelineSelectStateNotInitedWhenTransitionPipelineSelectPropertiesThenExpectCorrectValuesStored) {
    auto dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.pipelineSupportFlags.systolicMode = false;
    commandStreamReceiver.pipelineSupportFlags.mediaSamplerDopClockGate = true;

    dispatchFlags.pipelineSelectArgs.mediaSamplerRequired = false;
    commandStreamReceiver.handlePipelineSelectStateTransition(dispatchFlags);
    EXPECT_TRUE(commandStreamReceiver.csrSizeRequestFlags.mediaSamplerConfigChanged);

    commandStreamReceiver.pipelineSupportFlags.mediaSamplerDopClockGate = false;
    commandStreamReceiver.lastMediaSamplerConfig = -1;
    commandStreamReceiver.handlePipelineSelectStateTransition(dispatchFlags);
    EXPECT_FALSE(commandStreamReceiver.csrSizeRequestFlags.mediaSamplerConfigChanged);

    commandStreamReceiver.pipelineSupportFlags.mediaSamplerDopClockGate = true;
    commandStreamReceiver.lastMediaSamplerConfig = 0;
    commandStreamReceiver.handlePipelineSelectStateTransition(dispatchFlags);
    EXPECT_FALSE(commandStreamReceiver.csrSizeRequestFlags.mediaSamplerConfigChanged);

    dispatchFlags.pipelineSelectArgs.mediaSamplerRequired = true;
    commandStreamReceiver.handlePipelineSelectStateTransition(dispatchFlags);
    EXPECT_TRUE(commandStreamReceiver.csrSizeRequestFlags.mediaSamplerConfigChanged);

    commandStreamReceiver.pipelineSupportFlags.mediaSamplerDopClockGate = false;
    commandStreamReceiver.pipelineSupportFlags.systolicMode = true;

    commandStreamReceiver.lastSystolicPipelineSelectMode = false;
    dispatchFlags.pipelineSelectArgs.systolicPipelineSelectMode = true;
    commandStreamReceiver.handlePipelineSelectStateTransition(dispatchFlags);
    EXPECT_TRUE(commandStreamReceiver.csrSizeRequestFlags.systolicPipelineSelectMode);

    commandStreamReceiver.pipelineSupportFlags.systolicMode = false;
    commandStreamReceiver.lastSystolicPipelineSelectMode = false;
    dispatchFlags.pipelineSelectArgs.systolicPipelineSelectMode = true;
    commandStreamReceiver.handlePipelineSelectStateTransition(dispatchFlags);
    EXPECT_FALSE(commandStreamReceiver.csrSizeRequestFlags.systolicPipelineSelectMode);

    commandStreamReceiver.pipelineSupportFlags.systolicMode = true;
    commandStreamReceiver.lastSystolicPipelineSelectMode = false;
    dispatchFlags.pipelineSelectArgs.systolicPipelineSelectMode = false;
    commandStreamReceiver.handlePipelineSelectStateTransition(dispatchFlags);
    EXPECT_FALSE(commandStreamReceiver.csrSizeRequestFlags.systolicPipelineSelectMode);
}

HWTEST_F(CommandStreamReceiverTest,
         givenPipelineSelectStateInitedWhenTransitionPipelineSelectPropertiesThenExpectCorrectValuesStored) {
    auto dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.pipelineSupportFlags.systolicMode = false;
    commandStreamReceiver.pipelineSupportFlags.mediaSamplerDopClockGate = true;

    commandStreamReceiver.streamProperties.pipelineSelect.mediaSamplerDopClockGate.value = 1;
    commandStreamReceiver.lastMediaSamplerConfig = -1;
    dispatchFlags.pipelineSelectArgs.mediaSamplerRequired = false;
    commandStreamReceiver.handlePipelineSelectStateTransition(dispatchFlags);
    EXPECT_TRUE(commandStreamReceiver.csrSizeRequestFlags.mediaSamplerConfigChanged);

    commandStreamReceiver.streamProperties.pipelineSelect.mediaSamplerDopClockGate.value = 0;
    dispatchFlags.pipelineSelectArgs.mediaSamplerRequired = true;
    commandStreamReceiver.handlePipelineSelectStateTransition(dispatchFlags);
    EXPECT_TRUE(commandStreamReceiver.csrSizeRequestFlags.mediaSamplerConfigChanged);

    commandStreamReceiver.streamProperties.pipelineSelect.mediaSamplerDopClockGate.value = 0;
    commandStreamReceiver.lastMediaSamplerConfig = 1;
    dispatchFlags.pipelineSelectArgs.mediaSamplerRequired = false;
    commandStreamReceiver.handlePipelineSelectStateTransition(dispatchFlags);
    EXPECT_FALSE(commandStreamReceiver.csrSizeRequestFlags.mediaSamplerConfigChanged);

    commandStreamReceiver.pipelineSupportFlags.mediaSamplerDopClockGate = false;
    commandStreamReceiver.pipelineSupportFlags.systolicMode = true;

    commandStreamReceiver.streamProperties.pipelineSelect.systolicMode.value = 1;
    commandStreamReceiver.lastSystolicPipelineSelectMode = false;
    dispatchFlags.pipelineSelectArgs.systolicPipelineSelectMode = false;
    commandStreamReceiver.handlePipelineSelectStateTransition(dispatchFlags);
    EXPECT_TRUE(commandStreamReceiver.csrSizeRequestFlags.systolicPipelineSelectMode);

    commandStreamReceiver.streamProperties.pipelineSelect.systolicMode.value = 0;
    dispatchFlags.pipelineSelectArgs.systolicPipelineSelectMode = true;
    commandStreamReceiver.handlePipelineSelectStateTransition(dispatchFlags);
    EXPECT_TRUE(commandStreamReceiver.csrSizeRequestFlags.systolicPipelineSelectMode);

    commandStreamReceiver.streamProperties.pipelineSelect.systolicMode.value = 0;
    commandStreamReceiver.lastSystolicPipelineSelectMode = true;
    dispatchFlags.pipelineSelectArgs.systolicPipelineSelectMode = false;
    commandStreamReceiver.handlePipelineSelectStateTransition(dispatchFlags);
    EXPECT_FALSE(commandStreamReceiver.csrSizeRequestFlags.systolicPipelineSelectMode);
}

using CommandStreamReceiverHwTest = Test<CommandStreamReceiverFixture>;

HWTEST2_F(CommandStreamReceiverHwTest, givenSshHeapNotProvidedWhenFlushTaskPerformedThenSbaProgammedSurfaceBaseAddressToZero, IsHeapfulSupportedAndAtLeastXeCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using _3DSTATE_BINDING_TABLE_POOL_ALLOC = typename FamilyType::_3DSTATE_BINDING_TABLE_POOL_ALLOC;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.flushTask(commandStream,
                                    0,
                                    &dsh,
                                    &ioh,
                                    nullptr,
                                    taskLevel,
                                    flushTaskFlags,
                                    *pDevice);

    auto &cmdStream = commandStreamReceiver.commandStream;

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands,
                                             cmdStream.getCpuBase(),
                                             cmdStream.getUsed());

    auto itorCmd = find<STATE_BASE_ADDRESS *>(commands.begin(), commands.end());
    ASSERT_NE(commands.end(), itorCmd);
    auto sbaCmd = genCmdCast<STATE_BASE_ADDRESS *>(*itorCmd);

    EXPECT_EQ(0u, sbaCmd->getSurfaceStateBaseAddress());

    itorCmd = find<_3DSTATE_BINDING_TABLE_POOL_ALLOC *>(commands.begin(), commands.end());
    EXPECT_EQ(commands.end(), itorCmd);
}

HWTEST_F(CommandStreamReceiverHwTest, givenDcFlushFlagSetWhenGettingCsrFlagValueThenCsrValueMatchesHelperValue) {

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    bool csrValue = commandStreamReceiver.getDcFlushSupport();
    bool helperValue = MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, pDevice->getRootDeviceEnvironment());
    EXPECT_EQ(helperValue, csrValue);
}

HWTEST_F(CommandStreamReceiverHwTest, givenBarrierTimestampPacketNodesWhenGetCmdSizeForStallingCommandsCalledThenReturnCorrectSize) {

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    const auto expectedCmdSizeNoPostSync = commandStreamReceiver.getCmdSizeForStallingNoPostSyncCommands();
    {
        DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
        dispatchFlags.barrierTimestampPacketNodes = nullptr;
        EXPECT_EQ(expectedCmdSizeNoPostSync, commandStreamReceiver.getCmdSizeForStallingCommands(dispatchFlags));
    }
    {
        DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
        TimestampPacketContainer emptyContainer;
        dispatchFlags.barrierTimestampPacketNodes = &emptyContainer;
        EXPECT_EQ(expectedCmdSizeNoPostSync, commandStreamReceiver.getCmdSizeForStallingCommands(dispatchFlags));
    }

    const auto expectedCmdSizePostSync = commandStreamReceiver.getCmdSizeForStallingPostSyncCommands();
    {
        DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
        TimestampPacketContainer barrierNodes;
        barrierNodes.add(commandStreamReceiver.getTimestampPacketAllocator()->getTag());
        dispatchFlags.barrierTimestampPacketNodes = &barrierNodes;
        EXPECT_EQ(expectedCmdSizePostSync, commandStreamReceiver.getCmdSizeForStallingCommands(dispatchFlags));
    }
}

struct MockRequiredScratchSpaceController : public ScratchSpaceControllerBase {
    MockRequiredScratchSpaceController(uint32_t rootDeviceIndex,
                                       ExecutionEnvironment &environment,
                                       InternalAllocationStorage &allocationStorage) : ScratchSpaceControllerBase(rootDeviceIndex, environment, allocationStorage) {}
    void setRequiredScratchSpace(void *sshBaseAddress,
                                 uint32_t scratchSlot,
                                 uint32_t requiredPerThreadScratchSizeSlot0,
                                 uint32_t requiredPerThreadScratchSizeSlot1,

                                 OsContext &osContext,
                                 bool &stateBaseAddressDirty,
                                 bool &vfeStateDirty) override {
        setRequiredScratchSpaceCalled = true;
    }
    bool setRequiredScratchSpaceCalled = false;
};

HWTEST_F(CommandStreamReceiverHwTest, givenSshHeapNotProvidedWhenFlushTaskPerformedThenDontSetRequiredScratchSpace) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto scratchController = new MockRequiredScratchSpaceController(pDevice->getRootDeviceIndex(),
                                                                    *pDevice->getExecutionEnvironment(),
                                                                    *pDevice->getGpgpuCommandStreamReceiver().getInternalAllocationStorage());

    commandStreamReceiver.scratchSpaceController.reset(scratchController);
    commandStreamReceiver.flushTask(commandStream,
                                    0,
                                    &dsh,
                                    &ioh,
                                    nullptr,
                                    taskLevel,
                                    flushTaskFlags,
                                    *pDevice);

    EXPECT_FALSE(scratchController->setRequiredScratchSpaceCalled);
}

TEST(CommandStreamReceiverSimpleTest, whenTranslatingSubmissionStatusToTaskCountValueThenProperValueIsReturned) {
    EXPECT_EQ(0u, CompletionStamp::getTaskCountFromSubmissionStatusError(SubmissionStatus::success));
    EXPECT_EQ(CompletionStamp::outOfHostMemory, CompletionStamp::getTaskCountFromSubmissionStatusError(SubmissionStatus::outOfHostMemory));
    EXPECT_EQ(CompletionStamp::outOfDeviceMemory, CompletionStamp::getTaskCountFromSubmissionStatusError(SubmissionStatus::outOfMemory));
    EXPECT_EQ(CompletionStamp::failed, CompletionStamp::getTaskCountFromSubmissionStatusError(SubmissionStatus::failed));
    EXPECT_EQ(CompletionStamp::unsupported, CompletionStamp::getTaskCountFromSubmissionStatusError(SubmissionStatus::unsupported));
}

HWTEST_F(CommandStreamReceiverHwTest, givenFailureOnFlushWhenFlushingBcsTaskThenErrorIsPropagated) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::bufferToHostPtr,
                                                                          commandStreamReceiver, commandStreamReceiver.getTagAllocation(), nullptr,
                                                                          commandStreamReceiver.getTagAllocation()->getUnderlyingBuffer(),
                                                                          commandStreamReceiver.getTagAllocation()->getGpuAddress(), 0,
                                                                          0, 0, 0, 0, 0, 0, 0);

    BlitPropertiesContainer container;
    container.push_back(blitProperties);

    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfHostMemory;
    EXPECT_EQ(CompletionStamp::outOfHostMemory, commandStreamReceiver.flushBcsTask(container, true, *pDevice));
    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfMemory;
    EXPECT_EQ(CompletionStamp::outOfDeviceMemory, commandStreamReceiver.flushBcsTask(container, true, *pDevice));
    commandStreamReceiver.flushReturnValue = SubmissionStatus::failed;
    EXPECT_EQ(CompletionStamp::failed, commandStreamReceiver.flushBcsTask(container, true, *pDevice));
}

HWTEST_F(CommandStreamReceiverHwTest, givenFlushBcsTaskVerifyLatestSentTaskCountUpdated) {

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>(defaultHwInfo.get(), true, 2u);
    auto devices = DeviceFactory::createDevices(*executionEnvironment.release());

    char commandBuffer[MemoryConstants::pageSize];
    memset(commandBuffer, 0, sizeof(commandBuffer));
    MockGraphicsAllocation mockCmdBufferAllocation(commandBuffer, 0x4000, MemoryConstants::pageSize);
    LinearStream commandStream(&mockCmdBufferAllocation);

    auto csr = devices[0]->getDefaultEngine().commandStreamReceiver;
    DispatchBcsFlags dispatchFlags = {false, false, false};
    TaskCountType taskCount = csr->peekTaskCount();
    csr->flushBcsTask(commandStream, commandStream.getUsed(), dispatchFlags, devices[0]->getHardwareInfo());
    TaskCountType latestSentTaskCount = csr->peekLatestSentTaskCount();
    EXPECT_EQ(latestSentTaskCount, taskCount + 1);
}

HWTEST2_F(CommandStreamReceiverHwTest, givenDeviceToHostCopyWhenFenceIsRequiredThenProgramMiMemFence, IsAtLeastXeHpcCore) {
    auto &bcsCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto hostAllocationPtr = allocateAlignedMemory(1, 1);
    void *hostPtr = reinterpret_cast<void *>(hostAllocationPtr.get());

    MockGraphicsAllocation mockAllocation;

    MockTimestampPacketContainer timestamp(*bcsCsr.getTimestampPacketAllocator(), 1u);

    size_t offset = 0;

    auto verify = [&](bool fenceExpected) {
        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(bcsCsr.commandStream, offset);
        auto &cmdList = hwParser.cmdList;

        auto cmdIterator = find<typename FamilyType::XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
        EXPECT_NE(cmdList.end(), cmdIterator);
        if (::testing::Test::HasFailure()) {
            return false;
        }

        cmdIterator = find<typename FamilyType::MI_ARB_CHECK *>(++cmdIterator, cmdList.end());
        EXPECT_NE(cmdList.end(), cmdIterator);
        if (::testing::Test::HasFailure()) {
            return false;
        }

        auto miMemFence = genCmdCast<typename FamilyType::MI_MEM_FENCE *>(*++cmdIterator);

        fenceExpected &= getHelper<ProductHelper>().isDeviceToHostCopySignalingFenceRequired();
        size_t expectedFenceCount = fenceExpected ? 1 : 0;
        if (!pDevice->getHardwareInfo().capabilityTable.isIntegratedDevice) {
            expectedFenceCount += 2;
        }

        auto fences = findAll<typename FamilyType::MI_MEM_FENCE *>(cmdIterator, cmdList.end());

        if (pDevice->getProductHelper().isReleaseGlobalFenceInCommandStreamRequired(pDevice->getHardwareInfo())) {
            EXPECT_EQ(expectedFenceCount, fences.size());
            if (fenceExpected) {
                EXPECT_NE(miMemFence, nullptr);
            }
        } else {
            EXPECT_EQ(0u, fences.size());
        }

        return !::testing::Test::HasFailure();
    };

    // device to host
    {
        mockAllocation.memoryPool = MemoryPool::localMemory;

        auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::bufferToHostPtr,
                                                                              bcsCsr, &mockAllocation, nullptr, hostPtr,
                                                                              mockAllocation.getGpuAddress(), 0,
                                                                              0, 0, {1, 1, 1}, 0, 0, 0, 0);
        blitProperties.blitSyncProperties.outputTimestampPacket = timestamp.getNode(0);

        BlitPropertiesContainer blitPropertiesContainer;
        blitPropertiesContainer.push_back(blitProperties);

        offset = bcsCsr.commandStream.getUsed();
        bcsCsr.flushBcsTask(blitPropertiesContainer, true, *pDevice);

        EXPECT_TRUE(verify(true));
    }

    // device to host without output timestamp packet
    {
        mockAllocation.memoryPool = MemoryPool::localMemory;

        auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::bufferToHostPtr,
                                                                              bcsCsr, &mockAllocation, nullptr, hostPtr,
                                                                              mockAllocation.getGpuAddress(), 0,
                                                                              0, 0, {1, 1, 1}, 0, 0, 0, 0);

        BlitPropertiesContainer blitPropertiesContainer;
        blitPropertiesContainer.push_back(blitProperties);

        offset = bcsCsr.commandStream.getUsed();
        bcsCsr.flushBcsTask(blitPropertiesContainer, true, *pDevice);

        EXPECT_TRUE(verify(false));
    }

    // host to device
    {
        mockAllocation.memoryPool = MemoryPool::localMemory;

        auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::hostPtrToBuffer,
                                                                              bcsCsr, &mockAllocation, nullptr, hostPtr,
                                                                              mockAllocation.getGpuAddress(), 0,
                                                                              0, 0, {1, 1, 1}, 0, 0, 0, 0);
        blitProperties.blitSyncProperties.outputTimestampPacket = timestamp.getNode(0);

        BlitPropertiesContainer blitPropertiesContainer;
        blitPropertiesContainer.push_back(blitProperties);

        offset = bcsCsr.commandStream.getUsed();
        bcsCsr.flushBcsTask(blitPropertiesContainer, true, *pDevice);

        EXPECT_TRUE(verify(false));
    }

    // host to host
    {
        mockAllocation.memoryPool = MemoryPool::system64KBPages;

        auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::bufferToHostPtr,
                                                                              bcsCsr, &mockAllocation, nullptr, hostPtr,
                                                                              mockAllocation.getGpuAddress(), 0,
                                                                              0, 0, {1, 1, 1}, 0, 0, 0, 0);
        blitProperties.blitSyncProperties.outputTimestampPacket = timestamp.getNode(0);

        BlitPropertiesContainer blitPropertiesContainer;
        blitPropertiesContainer.push_back(blitProperties);

        offset = bcsCsr.commandStream.getUsed();
        bcsCsr.flushBcsTask(blitPropertiesContainer, true, *pDevice);

        EXPECT_TRUE(verify(false));
    }

    // device to device
    {
        mockAllocation.memoryPool = MemoryPool::localMemory;

        auto blitProperties = BlitProperties::constructPropertiesForCopy(&mockAllocation, &mockAllocation, {0, 0, 0}, {0, 0, 0}, {1, 1, 1}, 0, 0, 0, 0, nullptr);
        blitProperties.blitSyncProperties.outputTimestampPacket = timestamp.getNode(0);

        BlitPropertiesContainer blitPropertiesContainer;
        blitPropertiesContainer.push_back(blitProperties);

        offset = bcsCsr.commandStream.getUsed();
        bcsCsr.flushBcsTask(blitPropertiesContainer, true, *pDevice);

        EXPECT_TRUE(verify(false));
    }
}

HWTEST_F(CommandStreamReceiverHwTest, givenOutOfHostMemoryFailureOnFlushWhenFlushingTaskThenErrorIsPropagated) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfHostMemory;

    auto completionStamp = commandStreamReceiver.flushTask(commandStream,
                                                           64,
                                                           &dsh,
                                                           &ioh,
                                                           nullptr,
                                                           taskLevel,
                                                           flushTaskFlags,
                                                           *pDevice);
    EXPECT_EQ(CompletionStamp::outOfHostMemory, completionStamp.taskCount);
}

HWTEST_F(CommandStreamReceiverHwTest, givenOutOfDeviceMemoryFailureOnFlushWhenFlushingTaskThenErrorIsPropagated) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfMemory;

    auto completionStamp = commandStreamReceiver.flushTask(commandStream,
                                                           64,
                                                           &dsh,
                                                           &ioh,
                                                           nullptr,
                                                           taskLevel,
                                                           flushTaskFlags,
                                                           *pDevice);

    EXPECT_EQ(CompletionStamp::outOfDeviceMemory, completionStamp.taskCount);
}

HWTEST_F(CommandStreamReceiverHwTest, givenFailedFailureOnFlushWhenFlushingTaskThenErrorIsPropagated) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.flushReturnValue = SubmissionStatus::failed;

    auto completionStamp = commandStreamReceiver.flushTask(commandStream,
                                                           64,
                                                           &dsh,
                                                           &ioh,
                                                           nullptr,
                                                           taskLevel,
                                                           flushTaskFlags,
                                                           *pDevice);

    EXPECT_EQ(CompletionStamp::failed, completionStamp.taskCount);
}

HWTEST_F(CommandStreamReceiverHwTest, givenUnsuccessOnFlushWhenFlushingSmallTaskThenTaskCountIsNotIncreased) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &stream = commandStreamReceiver.getCS(4096u);

    commandStreamReceiver.taskCount = 1u;
    commandStreamReceiver.flushReturnValue = SubmissionStatus::failed;
    commandStreamReceiver.flushSmallTask(stream, stream.getUsed());
    EXPECT_EQ(1u, commandStreamReceiver.taskCount);

    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfMemory;
    commandStreamReceiver.flushSmallTask(stream, stream.getUsed());
    EXPECT_EQ(1u, commandStreamReceiver.taskCount);

    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfHostMemory;
    commandStreamReceiver.flushSmallTask(stream, stream.getUsed());
    EXPECT_EQ(1u, commandStreamReceiver.taskCount);

    commandStreamReceiver.flushReturnValue = SubmissionStatus::unsupported;
    commandStreamReceiver.flushSmallTask(stream, stream.getUsed());
    EXPECT_EQ(1u, commandStreamReceiver.taskCount);

    commandStreamReceiver.flushReturnValue = SubmissionStatus::deviceUninitialized;
    commandStreamReceiver.flushSmallTask(stream, stream.getUsed());
    EXPECT_EQ(1u, commandStreamReceiver.taskCount);
}

HWTEST_F(CommandStreamReceiverHwTest, givenOutOfMemoryFailureOnFlushWhenFlushingMiDWThenErrorIsPropagated) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfMemory;

    EXPECT_EQ(SubmissionStatus::outOfMemory, commandStreamReceiver.flushMiFlushDW(false));

    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfHostMemory;
    EXPECT_EQ(SubmissionStatus::outOfHostMemory, commandStreamReceiver.flushMiFlushDW(false));
}

HWTEST_F(CommandStreamReceiverHwTest, givenOutOfMemoryFailureOnFlushWhenFlushingPipeControlThenErrorIsPropagated) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfMemory;

    EXPECT_EQ(SubmissionStatus::outOfMemory, commandStreamReceiver.flushPipeControl(false));

    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfHostMemory;
    EXPECT_EQ(SubmissionStatus::outOfHostMemory, commandStreamReceiver.flushPipeControl(false));
}

HWTEST_F(CommandStreamReceiverHwTest, givenOutOfMemoryFailureOnFlushWhenFlushingTagUpdateThenErrorIsPropagated) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfMemory;

    EXPECT_EQ(SubmissionStatus::outOfMemory, commandStreamReceiver.flushTagUpdate());

    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfHostMemory;
    EXPECT_EQ(SubmissionStatus::outOfHostMemory, commandStreamReceiver.flushTagUpdate());
}

HWTEST_F(CommandStreamReceiverHwTest, givenOutOfMemoryFailureOnFlushWhenInitializingDeviceWithFirstSubmissionThenErrorIsPropagated) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.latestFlushedTaskCount = 0;
    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfMemory;

    EXPECT_EQ(SubmissionStatus::outOfMemory, commandStreamReceiver.initializeDeviceWithFirstSubmission(*pDevice));

    commandStreamReceiver.latestFlushedTaskCount = 0;
    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfHostMemory;
    EXPECT_EQ(SubmissionStatus::outOfHostMemory, commandStreamReceiver.initializeDeviceWithFirstSubmission(*pDevice));
}

HWTEST_F(CommandStreamReceiverHwTest, whenFlushTagUpdateThenSetStallingCmdsFlag) {
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    ultCsr.recordFlushedBatchBuffer = true;

    EXPECT_EQ(SubmissionStatus::success, ultCsr.flushTagUpdate());

    EXPECT_TRUE(ultCsr.latestFlushedBatchBuffer.hasStallingCmds);
}

HWTEST_F(CommandStreamReceiverHwTest, whenFlushTagUpdateThenSetPassNumClients) {
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    ultCsr.recordFlushedBatchBuffer = true;

    int client1, client2;
    ultCsr.registerClient(&client1);
    ultCsr.registerClient(&client2);

    EXPECT_EQ(SubmissionStatus::success, ultCsr.flushTagUpdate());

    EXPECT_EQ(ultCsr.getNumClients(), ultCsr.latestFlushedBatchBuffer.numCsrClients);
}

HWTEST_F(CommandStreamReceiverHwTest, whenFlushTaskCalledThenSetPassNumClients) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.recordFlushedBatchBuffer = true;

    int client1, client2;
    commandStreamReceiver.registerClient(&client1);
    commandStreamReceiver.registerClient(&client2);

    commandStreamReceiver.flushTask(commandStream,
                                    64,
                                    &dsh,
                                    &ioh,
                                    nullptr,
                                    taskLevel,
                                    flushTaskFlags,
                                    *pDevice);

    EXPECT_EQ(commandStreamReceiver.getNumClients(), commandStreamReceiver.latestFlushedBatchBuffer.numCsrClients);
}

HWTEST_F(CommandStreamReceiverHwTest, givenVariousCsrModeWhenGettingTbxModeThenExpectOnlyWhenModeIsTbxOrTbxWithAub) {
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::hardware;
    EXPECT_FALSE(ultCsr.isTbxMode());

    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::hardwareWithAub;
    EXPECT_FALSE(ultCsr.isTbxMode());

    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::aub;
    EXPECT_FALSE(ultCsr.isTbxMode());

    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::tbx;
    EXPECT_TRUE(ultCsr.isTbxMode());

    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::tbxWithAub;
    EXPECT_TRUE(ultCsr.isTbxMode());
}

HWTEST_F(CommandStreamReceiverHwTest, GivenTwoRootDevicesWhengetMultiRootDeviceTimestampPacketAllocatorCalledThenAllocatorForTwoDevicesCreated) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>(defaultHwInfo.get(), true, 2u);
    auto devices = DeviceFactory::createDevices(*executionEnvironment.release());
    const RootDeviceIndicesContainer indices = {0u, 1u};
    auto csr = devices[0]->getDefaultEngine().commandStreamReceiver;
    auto allocator = csr->createMultiRootDeviceTimestampPacketAllocator(indices);
    class MockTagAllocatorBase : public TagAllocatorBase {
      public:
        using TagAllocatorBase::maxRootDeviceIndex;
    };
    EXPECT_EQ(reinterpret_cast<MockTagAllocatorBase *>(allocator.get())->maxRootDeviceIndex, 1u);
}
HWTEST_F(CommandStreamReceiverHwTest, GivenFiveRootDevicesWhengetMultiRootDeviceTimestampPacketAllocatorCalledThenAllocatorForFiveDevicesCreated) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>(defaultHwInfo.get(), true, 4u);
    auto devices = DeviceFactory::createDevices(*executionEnvironment.release());
    const RootDeviceIndicesContainer indices = {0u, 1u, 2u, 3u};
    auto csr = devices[0]->getDefaultEngine().commandStreamReceiver;
    auto allocator = csr->createMultiRootDeviceTimestampPacketAllocator(indices);
    class MockTagAllocatorBase : public TagAllocatorBase {
      public:
        using TagAllocatorBase::maxRootDeviceIndex;
    };
    EXPECT_EQ(reinterpret_cast<MockTagAllocatorBase *>(allocator.get())->maxRootDeviceIndex, 3u);
}
HWTEST_F(CommandStreamReceiverHwTest, givenMultiRootDeviceSyncNodeWhenFlushBcsTaskThenMiFlushAdded) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto mockTagAllocator = std::make_unique<MockTagAllocator<>>(pDevice->getRootDeviceIndex(), pDevice->getExecutionEnvironment()->memoryManager.get(), 10u);

    auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::bufferToHostPtr,
                                                                          commandStreamReceiver, commandStreamReceiver.getTagAllocation(), nullptr,
                                                                          commandStreamReceiver.getTagAllocation()->getUnderlyingBuffer(),
                                                                          commandStreamReceiver.getTagAllocation()->getGpuAddress(), 0,
                                                                          0, 0, 0, 0, 0, 0, 0);
    auto tag = mockTagAllocator->getTag();
    blitProperties.multiRootDeviceEventSync = tag;

    BlitPropertiesContainer container;
    container.push_back(blitProperties);
    commandStreamReceiver.flushBcsTask(container, true, *pDevice);
    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    auto cmdIterator = find<typename FamilyType::MI_FLUSH_DW *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    bool nodeAddressFound = false;
    while (cmdIterator != hwParser.cmdList.end()) {
        auto flush = genCmdCast<MI_FLUSH_DW *>(*cmdIterator);
        if (flush->getDestinationAddress() == tag->getGpuAddress() + tag->getContextEndOffset()) {
            nodeAddressFound = true;
            break;
        }
        cmdIterator = find<typename FamilyType::MI_FLUSH_DW *>(++cmdIterator, hwParser.cmdList.end());
    }
    EXPECT_TRUE(nodeAddressFound);
}
HWTEST_F(CommandStreamReceiverHwTest, givenNullPtrAsMultiRootDeviceSyncNodeWhenFlushBcsTAskThenMiFlushNotAdded) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto mockTagAllocator = std::make_unique<MockTagAllocator<>>(pDevice->getRootDeviceIndex(), pDevice->getExecutionEnvironment()->memoryManager.get(), 10u);

    auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::bufferToHostPtr,
                                                                          commandStreamReceiver, commandStreamReceiver.getTagAllocation(), nullptr,
                                                                          commandStreamReceiver.getTagAllocation()->getUnderlyingBuffer(),
                                                                          commandStreamReceiver.getTagAllocation()->getGpuAddress(), 0,
                                                                          0, 0, 0, 0, 0, 0, 0);
    auto tag = mockTagAllocator->getTag();

    BlitPropertiesContainer container;
    container.push_back(blitProperties);
    commandStreamReceiver.flushBcsTask(container, true, *pDevice);
    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    auto cmdIterator = find<typename FamilyType::MI_FLUSH_DW *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    bool nodeAddressFound = false;
    while (cmdIterator != hwParser.cmdList.end()) {
        auto flush = genCmdCast<MI_FLUSH_DW *>(*cmdIterator);
        if (flush->getDestinationAddress() == tag->getGpuAddress() + tag->getContextEndOffset()) {
            nodeAddressFound = true;
            break;
        }
        cmdIterator = find<typename FamilyType::MI_FLUSH_DW *>(++cmdIterator, hwParser.cmdList.end());
    }
    EXPECT_FALSE(nodeAddressFound);
}

HWTEST_F(CommandStreamReceiverTest, givenL1CachePolicyInitializedInCsrWhenGettingPolicySettingsThenExpectValueMatchProductHelper) {
    auto &productHelper = pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    auto l1CachePolicy = commandStreamReceiver->getStoredL1CachePolicy();
    EXPECT_EQ(productHelper.getL1CachePolicy(true), l1CachePolicy->getL1CacheValue(true));
    EXPECT_EQ(productHelper.getL1CachePolicy(false), l1CachePolicy->getL1CacheValue(false));
}

HWTEST_F(CommandStreamReceiverHwTest, givenCreateGlobalStatelessHeapAllocationWhenGettingIndirectHeapObjectThenHeapAndAllocationAreInitialized) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(nullptr, commandStreamReceiver.getGlobalStatelessHeap());

    commandStreamReceiver.createGlobalStatelessHeap();
    auto heap = commandStreamReceiver.getGlobalStatelessHeap();
    ASSERT_NE(nullptr, heap);
    EXPECT_EQ(commandStreamReceiver.getGlobalStatelessHeapAllocation(), heap->getGraphicsAllocation());

    auto heapAllocation = commandStreamReceiver.getGlobalStatelessHeapAllocation();
    commandStreamReceiver.createGlobalStatelessHeap();
    EXPECT_EQ(commandStreamReceiver.getGlobalStatelessHeap(), heap);
    EXPECT_EQ(commandStreamReceiver.getGlobalStatelessHeapAllocation(), heap->getGraphicsAllocation());
    EXPECT_EQ(commandStreamReceiver.getGlobalStatelessHeapAllocation(), heapAllocation);
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenCreateGlobalStatelessHeapAllocationWhenFlushingTaskThenGlobalStatelessHeapAllocationIsResidentAndNoBindingTableCommandDispatched,
          IsAtLeastXeCore) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }

    commandStreamReceiver.storeMakeResidentAllocations = true;
    EXPECT_EQ(nullptr, commandStreamReceiver.getGlobalStatelessHeap());

    commandStreamReceiver.createGlobalStatelessHeap();
    auto statelessHeap = commandStreamReceiver.getGlobalStatelessHeap();
    ASSERT_NE(nullptr, statelessHeap);

    commandStreamReceiver.flushTask(commandStream,
                                    0,
                                    &dsh,
                                    &ioh,
                                    statelessHeap,
                                    taskLevel,
                                    flushTaskFlags,
                                    *pDevice);
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(commandStreamReceiver.getGlobalStatelessHeapAllocation()));

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    hwParserCsr.findHardwareCommands<FamilyType>();
    ASSERT_NE(nullptr, hwParserCsr.cmdStateBaseAddress);
    EXPECT_EQ(nullptr, hwParserCsr.cmdBindingTableBaseAddress);
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenStateComputeModeDirtyWhenFlushingFirstTimeThenCleanDirtyFlagToDispatchStateComputeMode,
          IsAtLeastXeCore) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }

    EXPECT_TRUE(commandStreamReceiver.getStateComputeModeDirty());

    commandStreamReceiver.flushTask(commandStream,
                                    0,
                                    &dsh,
                                    &ioh,
                                    &ssh,
                                    taskLevel,
                                    flushTaskFlags,
                                    *pDevice);
    EXPECT_FALSE(commandStreamReceiver.getStateComputeModeDirty());

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    hwParserCsr.findHardwareCommands<FamilyType>();
    auto scmCmd = hwParserCsr.getCommand<STATE_COMPUTE_MODE>();
    EXPECT_NE(nullptr, scmCmd);
}

HWTEST_F(CommandStreamReceiverHwTest, givenFlushPipeControlWhenFlushWithoutStateCacheFlushThenExpectNoStateCacheFlushFlagsSet) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.flushPipeControl(false);

    EXPECT_FALSE(UnitTestHelper<FamilyType>::findStateCacheFlushPipeControl(commandStreamReceiver, commandStreamReceiver.commandStream));
}

HWTEST_F(CommandStreamReceiverHwTest, givenFCommandStreamWhenSubmitingDependencyUpdateThenPCWithTagAddresIsDispatched) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto mockTagAllocator = std::make_unique<MockTagAllocator<>>(pDevice->getRootDeviceIndex(), pDevice->getExecutionEnvironment()->memoryManager.get(), 10u);
    auto tag = mockTagAllocator->getTag();
    auto usedSizeBeforeSubmit = commandStreamReceiver.commandStream.getUsed();
    if (MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(commandStreamReceiver.peekRootDeviceEnvironment())) {
        usedSizeBeforeSubmit += sizeof(PIPE_CONTROL);
    }
    commandStreamReceiver.submitDependencyUpdate(tag);
    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSizeBeforeSubmit);
    const auto pipeControlItor = find<PIPE_CONTROL *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    const auto pipeControl = genCmdCast<PIPE_CONTROL *>(*pipeControlItor);
    EXPECT_NE(nullptr, pipeControl);
    auto cacheFlushTimestampPacketGpuAddress = TimestampPacketHelper::getContextEndGpuAddress(*tag);
    EXPECT_EQ(UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl), cacheFlushTimestampPacketGpuAddress);
    EXPECT_EQ(pipeControl->getDcFlushEnable(), MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, commandStreamReceiver.peekRootDeviceEnvironment()));
}

HWTEST_F(CommandStreamReceiverHwTest, givenFlushPipeControlWhenFlushWithStateCacheFlushThenExpectStateCacheFlushFlagsSet) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.sendRenderStateCacheFlush();

    EXPECT_TRUE(UnitTestHelper<FamilyType>::findStateCacheFlushPipeControl(commandStreamReceiver, commandStreamReceiver.commandStream));
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenRayTracingAllocationPresentWhenFlushingTaskThenDispatchBtdStateCommandOnceAndResidentAlways,
          IsHeapfulSupportedAndAtLeastXeCore) {
    using _3DSTATE_BTD = typename FamilyType::_3DSTATE_BTD;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;
    if (commandStreamReceiver.heaplessModeEnabled) {
        GTEST_SKIP();
    }

    EXPECT_FALSE(commandStreamReceiver.isRayTracingStateProgramingNeeded(*pDevice));

    pDevice->initializeRayTracing(8);

    auto rtAllocation = pDevice->getRTMemoryBackedBuffer();
    auto rtAllocationAddress = rtAllocation->getGpuAddress();

    EXPECT_TRUE(commandStreamReceiver.isRayTracingStateProgramingNeeded(*pDevice));

    commandStreamReceiver.flushTask(commandStream,
                                    0,
                                    &dsh,
                                    &ioh,
                                    &ssh,
                                    taskLevel,
                                    flushTaskFlags,
                                    *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto btdStateCmd = hwParserCsr.getCommand<_3DSTATE_BTD>();
    ASSERT_NE(nullptr, btdStateCmd);

    EXPECT_EQ(rtAllocationAddress, btdStateCmd->getMemoryBackedBufferBasePointer());

    uint32_t residentCount = 1;
    commandStreamReceiver.isMadeResident(rtAllocation, residentCount);

    EXPECT_FALSE(commandStreamReceiver.isRayTracingStateProgramingNeeded(*pDevice));

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();

    commandStreamReceiver.flushTask(commandStream,
                                    0,
                                    &dsh,
                                    &ioh,
                                    &ssh,
                                    taskLevel,
                                    flushTaskFlags,
                                    *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    btdStateCmd = hwParserCsr.getCommand<_3DSTATE_BTD>();
    EXPECT_EQ(nullptr, btdStateCmd);

    residentCount++;
    commandStreamReceiver.isMadeResident(rtAllocation, residentCount);
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenPlatformNotSupportingRayTracingWhenDispatchingCommandThenNothingDispatched,
          IsGen12LP) {
    pDevice->initializeRayTracing(8);

    constexpr size_t size = 64;
    uint8_t buffer[size];
    LinearStream cmdStream(buffer, size);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_EQ(0u, commandStreamReceiver.getCmdSizeForPerDssBackedBuffer(pDevice->getHardwareInfo()));

    commandStreamReceiver.dispatchRayTracingStateCommand(cmdStream, *pDevice);
    EXPECT_EQ(0u, cmdStream.getUsed());
}

using RayTracingForcedWaPlatform = IsAnyProducts<IGFX_DG2, IGFX_PVC>;

HWTEST2_F(CommandStreamReceiverHwTest,
          givenPlatformSupportingRayTracingWhenForcedPipeControlPriorBtdStateCommandThenPipeControlDispatched,
          RayTracingForcedWaPlatform) {
    using _3DSTATE_BTD = typename FamilyType::_3DSTATE_BTD;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    DebugManagerStateRestore stateRestore;
    debugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(1);

    pDevice->initializeRayTracing(8);

    constexpr size_t size = 256;
    uint8_t buffer[size];
    LinearStream cmdStream(buffer, size);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.dispatchRayTracingStateCommand(cmdStream, *pDevice);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        cmdStream.getCpuBase(),
        cmdStream.getUsed()));

    ASSERT_EQ(2u, cmdList.size());

    auto itCmd = cmdList.begin();

    auto cmdPipeControl = genCmdCast<PIPE_CONTROL *>(*itCmd);
    EXPECT_NE(nullptr, cmdPipeControl);

    itCmd++;
    auto cmdBtdState = genCmdCast<_3DSTATE_BTD *>(*itCmd);
    EXPECT_NE(nullptr, cmdBtdState);
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenPipelineSelectNotInitializedThenDispatchPipelineSelectCommand,
          IsXeCore) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_FALSE(commandStreamReceiver.getPreambleSetFlag());

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto pipelineSelectCmd = hwParserCsr.getCommand<PIPELINE_SELECT>();
    ASSERT_NE(nullptr, pipelineSelectCmd);
    EXPECT_TRUE(commandStreamReceiver.getPreambleSetFlag());

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    pipelineSelectCmd = hwParserCsr.getCommand<PIPELINE_SELECT>();

    EXPECT_EQ(nullptr, pipelineSelectCmd);
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskCmdListDispatchWhenPipelineSelectNotInitializedThenDoNotDispatchPipelineSelectCommand,
          IsXeCore) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_FALSE(commandStreamReceiver.getPreambleSetFlag());

    immediateFlushTaskFlags.dispatchOperation = NEO::AppendOperations::cmdList;
    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    EXPECT_EQ(usedSize, commandStreamReceiver.commandStream.getUsed());
    EXPECT_FALSE(commandStreamReceiver.getPreambleSetFlag());
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskOnSystolicPlatformWhenPipelineSelectAlreadyInitializedAndSystolicRequiredThenDispatchPipelineSelectCommandForSystolic,
          IsXeCore) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_FALSE(commandStreamReceiver.getPreambleSetFlag());

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto pipelineSelectCmd = hwParserCsr.getCommand<PIPELINE_SELECT>();
    ASSERT_NE(nullptr, pipelineSelectCmd);
    EXPECT_TRUE(commandStreamReceiver.getPreambleSetFlag());

    this->requiredStreamProperties.pipelineSelect.setPropertySystolicMode(true);

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    pipelineSelectCmd = hwParserCsr.getCommand<PIPELINE_SELECT>();

    if (commandStreamReceiver.pipelineSupportFlags.systolicMode) {
        ASSERT_NE(nullptr, pipelineSelectCmd);
        EXPECT_TRUE(UnitTestHelper<FamilyType>::getSystolicFlagValueFromPipelineSelectCommand(*pipelineSelectCmd));
    } else {
        EXPECT_EQ(nullptr, pipelineSelectCmd);
    }
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskNonKernelDispatchWhenPipelineSelectAlreadyInitializedAndSystolicRequiredThenIgnoreRequiredStreamProperties,
          IsXeCore) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_FALSE(commandStreamReceiver.getPreambleSetFlag());

    immediateFlushTaskFlags.dispatchOperation = NEO::AppendOperations::nonKernel;
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto pipelineSelectCmd = hwParserCsr.getCommand<PIPELINE_SELECT>();
    ASSERT_NE(nullptr, pipelineSelectCmd);
    EXPECT_TRUE(commandStreamReceiver.getPreambleSetFlag());

    this->requiredStreamProperties.pipelineSelect.setPropertySystolicMode(true);

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    EXPECT_EQ(usedSize, commandStreamReceiver.commandStream.getUsed());
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenFrontEndNotInitializedThenDispatchFrontEndCommand,
          IsHeapfulSupportedAndAtLeastXeCore) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_TRUE(commandStreamReceiver.getMediaVFEStateDirty());

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto frontEndCmd = hwParserCsr.getCommand<CFE_STATE>();
    ASSERT_NE(nullptr, frontEndCmd);
    EXPECT_FALSE(commandStreamReceiver.getMediaVFEStateDirty());

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    frontEndCmd = hwParserCsr.getCommand<CFE_STATE>();

    EXPECT_EQ(nullptr, frontEndCmd);
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskCmdListDispatchWhenFrontEndNotInitializedThenDoNotDispatchFrontEndCommand,
          IsAtLeastXeCore) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_TRUE(commandStreamReceiver.getMediaVFEStateDirty());

    immediateFlushTaskFlags.dispatchOperation = NEO::AppendOperations::cmdList;
    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    EXPECT_EQ(usedSize, commandStreamReceiver.commandStream.getUsed());
    EXPECT_TRUE(commandStreamReceiver.getMediaVFEStateDirty());
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskOnChangingFrontEndPropertiesPlatformWhenFrontEndAlreadyInitializedAndFrontEndPropertyChangeRequiredThenDispatchFrontEndCommand,
          IsXeCore) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_TRUE(commandStreamReceiver.getMediaVFEStateDirty());

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto frontEndCmd = hwParserCsr.getCommand<CFE_STATE>();
    ASSERT_NE(nullptr, frontEndCmd);
    EXPECT_FALSE(commandStreamReceiver.getMediaVFEStateDirty());

    this->requiredStreamProperties.frontEndState.setPropertiesComputeDispatchAllWalkerEnableDisableEuFusion(true, true);

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    frontEndCmd = hwParserCsr.getCommand<CFE_STATE>();

    if (commandStreamReceiver.feSupportFlags.computeDispatchAllWalker || commandStreamReceiver.feSupportFlags.disableEuFusion) {
        ASSERT_NE(nullptr, frontEndCmd);
        if (commandStreamReceiver.feSupportFlags.computeDispatchAllWalker) {
            EXPECT_TRUE(UnitTestHelper<FamilyType>::getComputeDispatchAllWalkerFromFrontEndCommand(*frontEndCmd));
        } else {
            EXPECT_FALSE(UnitTestHelper<FamilyType>::getComputeDispatchAllWalkerFromFrontEndCommand(*frontEndCmd));
        }
        if (commandStreamReceiver.feSupportFlags.disableEuFusion) {
            EXPECT_TRUE(UnitTestHelper<FamilyType>::getDisableFusionStateFromFrontEndCommand(*frontEndCmd));
        } else {
            EXPECT_FALSE(UnitTestHelper<FamilyType>::getDisableFusionStateFromFrontEndCommand(*frontEndCmd));
        }
    } else {
        EXPECT_EQ(nullptr, frontEndCmd);
    }
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskNonKernelDispatchWhenFrontEndAlreadyInitializedAndFrontEndPropertyChangeRequiredThenIgnoreRequiredStreamProperties,
          IsXeCore) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_TRUE(commandStreamReceiver.getMediaVFEStateDirty());

    immediateFlushTaskFlags.dispatchOperation = NEO::AppendOperations::nonKernel;
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto frontEndCmd = hwParserCsr.getCommand<CFE_STATE>();
    ASSERT_NE(nullptr, frontEndCmd);
    EXPECT_FALSE(commandStreamReceiver.getMediaVFEStateDirty());

    this->requiredStreamProperties.frontEndState.setPropertiesComputeDispatchAllWalkerEnableDisableEuFusion(true, true);

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    EXPECT_EQ(usedSize, commandStreamReceiver.commandStream.getUsed());
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenStateComputeModeNotInitializedThenDispatchStateComputeModeCommand,
          IsAtLeastXeCore) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }

    EXPECT_TRUE(commandStreamReceiver.getStateComputeModeDirty());

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto stateComputeModeCmd = hwParserCsr.getCommand<STATE_COMPUTE_MODE>();
    ASSERT_NE(nullptr, stateComputeModeCmd);
    EXPECT_FALSE(commandStreamReceiver.getStateComputeModeDirty());

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    stateComputeModeCmd = hwParserCsr.getCommand<STATE_COMPUTE_MODE>();

    EXPECT_EQ(nullptr, stateComputeModeCmd);
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskCmdListDispatchWhenStateComputeModeNotInitializedThenDoNotDispatchStateComputeModeCommand,
          IsAtLeastXeCore) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }

    EXPECT_TRUE(commandStreamReceiver.getStateComputeModeDirty());

    immediateFlushTaskFlags.dispatchOperation = NEO::AppendOperations::cmdList;
    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    EXPECT_EQ(usedSize, commandStreamReceiver.commandStream.getUsed());
    EXPECT_TRUE(commandStreamReceiver.getStateComputeModeDirty());
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskOnChangingStateComputeModePropertiesPlatformWhenStateComputeModeAlreadyInitializedAndStateComputeModePropertyChangeRequiredThenDispatchStateComputeModeCommand,
          IsAtLeastXeCore) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }

    EXPECT_TRUE(commandStreamReceiver.getStateComputeModeDirty());

    this->requiredStreamProperties.stateComputeMode.setPropertiesAll(false, GrfConfig::defaultGrfNumber, ThreadArbitrationPolicy::AgeBased, NEO::PreemptionMode::ThreadGroup);

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto stateComputeModeCmd = hwParserCsr.getCommand<STATE_COMPUTE_MODE>();
    ASSERT_NE(nullptr, stateComputeModeCmd);
    EXPECT_FALSE(commandStreamReceiver.getStateComputeModeDirty());

    this->requiredStreamProperties.stateComputeMode.setPropertiesGrfNumberThreadArbitration(GrfConfig::largeGrfNumber, ThreadArbitrationPolicy::RoundRobin);

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    stateComputeModeCmd = hwParserCsr.getCommand<STATE_COMPUTE_MODE>();

    StateComputeModePropertiesSupport scmPropertiesSupport;
    auto &productHelper = commandStreamReceiver.getProductHelper();
    productHelper.fillScmPropertiesSupportStructure(scmPropertiesSupport);

    if (scmPropertiesSupport.largeGrfMode || scmPropertiesSupport.threadArbitrationPolicy) {
        ASSERT_NE(nullptr, stateComputeModeCmd);
    } else {
        EXPECT_EQ(nullptr, stateComputeModeCmd);
    }
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskNonKernelDispatchWhenStateComputeModeAlreadyInitializedAndStateComputeModePropertyChangeRequiredThenIgnoreRequiredStreamProperties,
          IsAtLeastXeCore) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }

    EXPECT_TRUE(commandStreamReceiver.getStateComputeModeDirty());

    this->requiredStreamProperties.stateComputeMode.setPropertiesAll(false, GrfConfig::defaultGrfNumber, ThreadArbitrationPolicy::AgeBased, NEO::PreemptionMode::ThreadGroup);

    immediateFlushTaskFlags.dispatchOperation = NEO::AppendOperations::nonKernel;
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto stateComputeModeCmd = hwParserCsr.getCommand<STATE_COMPUTE_MODE>();
    ASSERT_NE(nullptr, stateComputeModeCmd);
    EXPECT_FALSE(commandStreamReceiver.getStateComputeModeDirty());

    this->requiredStreamProperties.stateComputeMode.setPropertiesGrfNumberThreadArbitration(GrfConfig::largeGrfNumber, ThreadArbitrationPolicy::RoundRobin);

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    EXPECT_EQ(usedSize, commandStreamReceiver.commandStream.getUsed());
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskAndBindingPoolBaseAddressNeededWhenStateBaseAddressNotInitializedThenDispatchStateBaseAddressAndBindingPoolBaseAddressCommand,
          IsHeapfulSupportedAndAtLeastXeCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using _3DSTATE_BINDING_TABLE_POOL_ALLOC = typename FamilyType::_3DSTATE_BINDING_TABLE_POOL_ALLOC;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }

    EXPECT_TRUE(commandStreamReceiver.getGSBAStateDirty());

    this->requiredStreamProperties.stateBaseAddress.setPropertiesAll(1, 0x1000, 0x100, 0x2000, 0x100, 0x3000, 0x100, 0x4000, 0x100);
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto stateBaseAddress = hwParserCsr.getCommand<STATE_BASE_ADDRESS>();
    ASSERT_NE(nullptr, stateBaseAddress);
    EXPECT_EQ(0x4000u, stateBaseAddress->getGeneralStateBaseAddress());

    auto bindingTableAddress = hwParserCsr.getCommand<_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    ASSERT_NE(nullptr, bindingTableAddress);

    EXPECT_FALSE(commandStreamReceiver.getGSBAStateDirty());

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    stateBaseAddress = hwParserCsr.getCommand<STATE_BASE_ADDRESS>();
    EXPECT_EQ(nullptr, stateBaseAddress);
    bindingTableAddress = hwParserCsr.getCommand<_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    EXPECT_EQ(nullptr, bindingTableAddress);
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskCmdListDispatchWhenStateBaseAddressNotInitializedThenDoNotDispatchStateBaseAddressAndBindingPoolBaseAddressCommand,
          IsAtLeastXeCore) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }

    EXPECT_TRUE(commandStreamReceiver.getGSBAStateDirty());

    this->requiredStreamProperties.stateBaseAddress.setPropertiesAll(1, 0x1000, 0x100, 0x2000, 0x100, 0x3000, 0x100, 0x4000, 0x100);
    immediateFlushTaskFlags.dispatchOperation = NEO::AppendOperations::cmdList;
    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    EXPECT_EQ(usedSize, commandStreamReceiver.commandStream.getUsed());
    EXPECT_TRUE(commandStreamReceiver.getGSBAStateDirty());
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskAndBindingPoolBaseAddressNeededWhenStateBaseAddressInitializedAndHeapsChangedThenDispatchStateBaseAddressAndBindingPoolBaseAddressCommandTwice,
          IsHeapfulSupportedAndAtLeastXeCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using _3DSTATE_BINDING_TABLE_POOL_ALLOC = typename FamilyType::_3DSTATE_BINDING_TABLE_POOL_ALLOC;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }

    EXPECT_TRUE(commandStreamReceiver.getGSBAStateDirty());

    this->requiredStreamProperties.stateBaseAddress.setPropertiesAll(1, 0x1000, 0x100, 0x2000, 0x100, 0x3000, 0x100, 0x4000, 0x100);
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto stateBaseAddress = hwParserCsr.getCommand<STATE_BASE_ADDRESS>();
    ASSERT_NE(nullptr, stateBaseAddress);
    EXPECT_EQ(0x4000u, stateBaseAddress->getGeneralStateBaseAddress());

    auto bindingTableAddress = hwParserCsr.getCommand<_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    ASSERT_NE(nullptr, bindingTableAddress);

    EXPECT_FALSE(commandStreamReceiver.getGSBAStateDirty());

    this->requiredStreamProperties.stateBaseAddress.setPropertiesBindingTableSurfaceState(0x5000, 0x100, 0x6000, 0x100);

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    stateBaseAddress = hwParserCsr.getCommand<STATE_BASE_ADDRESS>();
    ASSERT_NE(nullptr, stateBaseAddress);
    bindingTableAddress = hwParserCsr.getCommand<_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    ASSERT_NE(nullptr, bindingTableAddress);
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskNonKernelDispatchWhenStateBaseAddressInitializedThenDispatchInitialStateBaseAddressAndIgnoreRequiredStreamProperties,
          IsHeapfulSupportedAndAtLeastXeCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using _3DSTATE_BINDING_TABLE_POOL_ALLOC = typename FamilyType::_3DSTATE_BINDING_TABLE_POOL_ALLOC;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }

    EXPECT_TRUE(commandStreamReceiver.getGSBAStateDirty());

    this->requiredStreamProperties.stateBaseAddress.setPropertiesAll(1, 0x1000, 0x100, 0x2000, 0x100, 0x3000, 0x100, 0x4000, 0x100);

    immediateFlushTaskFlags.dispatchOperation = NEO::AppendOperations::nonKernel;
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto stateBaseAddress = hwParserCsr.getCommand<STATE_BASE_ADDRESS>();
    ASSERT_NE(nullptr, stateBaseAddress);
    EXPECT_EQ(0x4000u, stateBaseAddress->getGeneralStateBaseAddress());

    auto bindingTableAddress = hwParserCsr.getCommand<_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    ASSERT_NE(nullptr, bindingTableAddress);

    EXPECT_FALSE(commandStreamReceiver.getGSBAStateDirty());

    this->requiredStreamProperties.stateBaseAddress.setPropertiesBindingTableSurfaceState(0x5000, 0x100, 0x6000, 0x100);

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    EXPECT_EQ(usedSize, commandStreamReceiver.commandStream.getUsed());
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskAndBindingPoolBaseAddressNeededWhenStateBaseAddressPropertiesNotProvidedForFirstFlushThenDispatchSecondSbaCommandWhenProvided,
          IsHeapfulSupportedAndAtLeastXeCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using _3DSTATE_BINDING_TABLE_POOL_ALLOC = typename FamilyType::_3DSTATE_BINDING_TABLE_POOL_ALLOC;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }

    EXPECT_TRUE(commandStreamReceiver.getGSBAStateDirty());

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto stateBaseAddress = hwParserCsr.getCommand<STATE_BASE_ADDRESS>();
    ASSERT_NE(nullptr, stateBaseAddress);
    EXPECT_EQ(0u, stateBaseAddress->getGeneralStateBaseAddress());

    auto bindingTableAddress = hwParserCsr.getCommand<_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    EXPECT_EQ(nullptr, bindingTableAddress);

    EXPECT_FALSE(commandStreamReceiver.getGSBAStateDirty());

    this->requiredStreamProperties.stateBaseAddress.setPropertiesAll(1, 0x1000, 0x100, 0x2000, 0x100, 0x3000, 0x100, 0x4000, 0x100);

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    stateBaseAddress = hwParserCsr.getCommand<STATE_BASE_ADDRESS>();
    ASSERT_NE(nullptr, stateBaseAddress);
    EXPECT_EQ(0x4000u, stateBaseAddress->getGeneralStateBaseAddress());
    bindingTableAddress = hwParserCsr.getCommand<_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    ASSERT_NE(nullptr, bindingTableAddress);
    EXPECT_EQ(0x1000u, bindingTableAddress->getBindingTablePoolBaseAddress());

    usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    stateBaseAddress = hwParserCsr.getCommand<STATE_BASE_ADDRESS>();
    EXPECT_EQ(nullptr, stateBaseAddress);
    bindingTableAddress = hwParserCsr.getCommand<_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    EXPECT_EQ(nullptr, bindingTableAddress);
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskAndGlobalStatelessHeapWhenStateBaseAddressNotInitializedThenDispatchStateBaseAddressAndNoBindingPoolBaseAddressCommand,
          IsHeapfulSupportedAndAtLeastXeCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using _3DSTATE_BINDING_TABLE_POOL_ALLOC = typename FamilyType::_3DSTATE_BINDING_TABLE_POOL_ALLOC;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }

    commandStreamReceiver.createGlobalStatelessHeap();

    EXPECT_TRUE(commandStreamReceiver.getGSBAStateDirty());

    this->requiredStreamProperties.stateBaseAddress.setPropertiesAll(1, -1, -1, 0x2000, 0x100, 0x3000, 0x100, 0x4000, 0x100);
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    EXPECT_EQ(0x2000, commandStreamReceiver.streamProperties.stateBaseAddress.surfaceStateBaseAddress.value);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto stateBaseAddress = hwParserCsr.getCommand<STATE_BASE_ADDRESS>();
    ASSERT_NE(nullptr, stateBaseAddress);
    EXPECT_EQ(0x4000u, stateBaseAddress->getGeneralStateBaseAddress());

    auto bindingTableAddress = hwParserCsr.getCommand<_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    EXPECT_EQ(nullptr, bindingTableAddress);

    EXPECT_FALSE(commandStreamReceiver.getGSBAStateDirty());

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    stateBaseAddress = hwParserCsr.getCommand<STATE_BASE_ADDRESS>();
    EXPECT_EQ(nullptr, stateBaseAddress);
    bindingTableAddress = hwParserCsr.getCommand<_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    EXPECT_EQ(nullptr, bindingTableAddress);
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskAndGlobalStatelessHeapWhenStateBaseAddressNotInitializedThenDispatchStateBaseAddressCommandTwice,
          IsHeapfulSupportedAndAtLeastXeCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using _3DSTATE_BINDING_TABLE_POOL_ALLOC = typename FamilyType::_3DSTATE_BINDING_TABLE_POOL_ALLOC;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }
    commandStreamReceiver.createGlobalStatelessHeap();

    EXPECT_TRUE(commandStreamReceiver.getGSBAStateDirty());

    this->requiredStreamProperties.stateBaseAddress.setPropertiesAll(1, -1, -1, 0x2000, 0x100, 0x3000, 0x100, 0x4000, 0x100);
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    EXPECT_EQ(0x2000, commandStreamReceiver.streamProperties.stateBaseAddress.surfaceStateBaseAddress.value);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto stateBaseAddress = hwParserCsr.getCommand<STATE_BASE_ADDRESS>();
    ASSERT_NE(nullptr, stateBaseAddress);
    EXPECT_EQ(0x4000u, stateBaseAddress->getGeneralStateBaseAddress());

    auto bindingTableAddress = hwParserCsr.getCommand<_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    EXPECT_EQ(nullptr, bindingTableAddress);

    EXPECT_FALSE(commandStreamReceiver.getGSBAStateDirty());

    this->requiredStreamProperties.stateBaseAddress.setPropertiesBindingTableSurfaceState(-1, -1, 0x6000, 0x100);

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    EXPECT_EQ(0x6000, commandStreamReceiver.streamProperties.stateBaseAddress.surfaceStateBaseAddress.value);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    stateBaseAddress = hwParserCsr.getCommand<STATE_BASE_ADDRESS>();
    ASSERT_NE(nullptr, stateBaseAddress);
    bindingTableAddress = hwParserCsr.getCommand<_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    EXPECT_EQ(nullptr, bindingTableAddress);
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenNextDispatchRequiresScratchSpaceThenFrontEndCommandIsDispatched,
          IsHeapfulSupportedAndAtLeastXeCore) {
    using CFE_STATE = typename FamilyType::CFE_STATE;
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;
    commandStreamReceiver.heaplessModeEnabled = false;

    EXPECT_TRUE(commandStreamReceiver.getMediaVFEStateDirty());

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto frontEndCmd = hwParserCsr.getCommand<CFE_STATE>();
    ASSERT_NE(nullptr, frontEndCmd);
    EXPECT_FALSE(commandStreamReceiver.getMediaVFEStateDirty());

    commandStreamReceiver.setRequiredScratchSizes(0x100, 0);

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    frontEndCmd = hwParserCsr.getCommand<CFE_STATE>();
    ASSERT_NE(nullptr, frontEndCmd);

    size_t expectedScratchOffset = 2 * sizeof(RENDER_SURFACE_STATE);
    EXPECT_EQ(expectedScratchOffset, frontEndCmd->getScratchSpaceBuffer());

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(commandStreamReceiver.getScratchSpaceController()->getScratchSpaceSlot0Allocation()));

    commandStreamReceiver.setRequiredScratchSizes(0x400, 0);

    usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    frontEndCmd = hwParserCsr.getCommand<CFE_STATE>();
    ASSERT_NE(nullptr, frontEndCmd);

    expectedScratchOffset = 4 * sizeof(RENDER_SURFACE_STATE);
    EXPECT_EQ(expectedScratchOffset, frontEndCmd->getScratchSpaceBuffer());
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenNextDispatchRequiresPrivateScratchSpaceThenFrontEndCommandIsDispatched,
          IsHeapfulSupportedAndAtLeastXeCore) {
    using CFE_STATE = typename FamilyType::CFE_STATE;
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;
    commandStreamReceiver.heaplessModeEnabled = false;

    EXPECT_TRUE(commandStreamReceiver.getMediaVFEStateDirty());

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto frontEndCmd = hwParserCsr.getCommand<CFE_STATE>();
    ASSERT_NE(nullptr, frontEndCmd);
    EXPECT_FALSE(commandStreamReceiver.getMediaVFEStateDirty());

    commandStreamReceiver.setRequiredScratchSizes(0, 0x100);

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    frontEndCmd = hwParserCsr.getCommand<CFE_STATE>();
    ASSERT_NE(nullptr, frontEndCmd);

    constexpr size_t expectedScratchOffset = 2 * sizeof(RENDER_SURFACE_STATE);
    EXPECT_EQ(expectedScratchOffset, frontEndCmd->getScratchSpaceBuffer());

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(commandStreamReceiver.getScratchSpaceController()->getScratchSpaceSlot1Allocation()));
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenOneTimeContextSystemFenceRequiredThenExpectOneTimeSystemFenceCommand,
          IsHeapfulSupportedAndAtLeastXeHpcCore) {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename FamilyType::STATE_SYSTEM_MEM_FENCE_ADDRESS;
    if (pDevice->getHardwareInfo().capabilityTable.isIntegratedDevice) {
        GTEST_SKIP();
    }

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;

    EXPECT_TRUE(commandStreamReceiver.getMediaVFEStateDirty());

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto systemFenceCmd = hwParserCsr.getCommand<STATE_SYSTEM_MEM_FENCE_ADDRESS>();
    ASSERT_NE(nullptr, systemFenceCmd);

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(commandStreamReceiver.getGlobalFenceAllocation()));

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    systemFenceCmd = hwParserCsr.getCommand<STATE_SYSTEM_MEM_FENCE_ADDRESS>();
    EXPECT_EQ(nullptr, systemFenceCmd);

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(commandStreamReceiver.getGlobalFenceAllocation()));
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenOneTimeContextPartitionConfigRequiredThenExpectOneTimeWparidRegisterCommand,
          IsXeHpcCore) {
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;
    commandStreamReceiver.staticWorkPartitioningEnabled = true;
    commandStreamReceiver.activePartitions = 2;

    commandStreamReceiver.workPartitionAllocation = commandStreamReceiver.getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{commandStreamReceiver.getRootDeviceIndex(), MemoryConstants::pageSize});

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto loadRegisterMemCmd = hwParserCsr.getCommand<MI_LOAD_REGISTER_MEM>();
    ASSERT_NE(nullptr, loadRegisterMemCmd);
    constexpr uint32_t wparidRegister = 0x221C;
    EXPECT_EQ(wparidRegister, loadRegisterMemCmd->getRegisterAddress());

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(commandStreamReceiver.getWorkPartitionAllocation()));

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    loadRegisterMemCmd = hwParserCsr.getCommand<MI_LOAD_REGISTER_MEM>();
    EXPECT_EQ(nullptr, loadRegisterMemCmd);

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(commandStreamReceiver.getWorkPartitionAllocation()));
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenRayTracingAllocationCreatedThenOneTimeRayTracingCommandDispatched,
          IsHeapfulSupportedAndAtLeastXeCore) {
    using _3DSTATE_BTD = typename FamilyType::_3DSTATE_BTD;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;
    if (commandStreamReceiver.heaplessModeEnabled) {
        GTEST_SKIP();
    }

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto btdStateCmd = hwParserCsr.getCommand<_3DSTATE_BTD>();
    EXPECT_EQ(nullptr, btdStateCmd);

    pDevice->initializeRayTracing(8);
    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    btdStateCmd = hwParserCsr.getCommand<_3DSTATE_BTD>();
    ASSERT_NE(nullptr, btdStateCmd);

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(pDevice->getRTMemoryBackedBuffer()));

    usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    btdStateCmd = hwParserCsr.getCommand<_3DSTATE_BTD>();
    EXPECT_EQ(nullptr, btdStateCmd);

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(pDevice->getRTMemoryBackedBuffer()));
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenCsrHasPreambleCommandsThenDispatchIndirectJumpToImmediateBatchBuffer,
          IsHeapfulSupportedAndAtLeastXeCore) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto startOffset = commandStream.getUsed();
    *commandStream.getSpaceForCmd<DefaultWalkerType>() = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
    uint64_t immediateStartAddress = commandStream.getGpuBase() + startOffset;

    commandStreamReceiver.flushImmediateTask(commandStream, startOffset, immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto bbStartCmd = hwParserCsr.getCommand<MI_BATCH_BUFFER_START>();
    ASSERT_NE(nullptr, bbStartCmd);
    EXPECT_EQ(immediateStartAddress, bbStartCmd->getBatchBufferStartAddress());

    startOffset = commandStream.getUsed();
    *commandStream.getSpaceForCmd<DefaultWalkerType>() = FamilyType::template getInitGpuWalker<DefaultWalkerType>();

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream, startOffset, immediateFlushTaskFlags, *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    bbStartCmd = hwParserCsr.getCommand<MI_BATCH_BUFFER_START>();
    ASSERT_EQ(nullptr, bbStartCmd);
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenBlockingCallSelectedThenDispatchPipeControlPostSyncToImmediateBatchBuffer,
          IsAtLeastXeCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto heapless = commandStreamReceiver.heaplessModeEnabled;
    auto heaplessStateInit = commandStreamReceiver.heaplessStateInitialized;

    bool additionalSyncCmd = NEO::MemorySynchronizationCommands<FamilyType>::getSizeForSingleAdditionalSynchronization(NEO::FenceType::release, commandStreamReceiver.peekRootDeviceEnvironment()) > 0;
    commandStreamReceiver.storeMakeResidentAllocations = true;

    auto startOffset = commandStream.getUsed();
    auto immediateListCmdBufferAllocation = commandStream.getGraphicsAllocation();
    UnitTestHelper<FamilyType>::getSpaceAndInitWalkerCmd(commandStream, heapless);

    auto csrTagAllocation = commandStreamReceiver.getTagAllocation();
    uint64_t postsyncAddress = csrTagAllocation->getGpuAddress();

    immediateFlushTaskFlags.blockingAppend = true;

    auto completionStamp = heaplessStateInit ? commandStreamReceiver.flushImmediateTaskStateless(commandStream, startOffset, immediateFlushTaskFlags, *pDevice)
                                             : commandStreamReceiver.flushImmediateTask(commandStream, startOffset, immediateFlushTaskFlags, *pDevice);

    auto expectedCount = heaplessStateInit ? 2u : 1u;

    EXPECT_EQ(expectedCount, completionStamp.taskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.taskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.latestSentTaskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.latestFlushedTaskCount);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStream, 0);
    auto cmdItor = hwParserCsr.getCommandItor<PIPE_CONTROL>();
    ASSERT_NE(hwParserCsr.cmdList.end(), cmdItor);
    auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*cmdItor);
    ASSERT_NE(nullptr, pipeControlCmd);
    EXPECT_EQ(postsyncAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControlCmd));
    EXPECT_EQ(expectedCount, pipeControlCmd->getImmediateData());

    cmdItor++;
    ASSERT_NE(hwParserCsr.cmdList.end(), cmdItor);
    if (additionalSyncCmd) {
        cmdItor++;
        ASSERT_NE(hwParserCsr.cmdList.end(), cmdItor);
    }
    auto bbEndCmd = genCmdCast<MI_BATCH_BUFFER_END *>(*cmdItor);
    ASSERT_NE(nullptr, bbEndCmd);

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(csrTagAllocation));
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(immediateListCmdBufferAllocation));

    startOffset = commandStream.getUsed();
    EXPECT_EQ(0u, (startOffset % MemoryConstants::cacheLineSize));

    UnitTestHelper<FamilyType>::getSpaceAndInitWalkerCmd(commandStream, heapless);

    if (heaplessStateInit) {
        completionStamp = commandStreamReceiver.flushImmediateTaskStateless(commandStream, startOffset, immediateFlushTaskFlags, *pDevice);
    } else {
        completionStamp = commandStreamReceiver.flushImmediateTask(commandStream, startOffset, immediateFlushTaskFlags, *pDevice);
    }

    expectedCount++;
    EXPECT_EQ(expectedCount, completionStamp.taskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.taskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.latestSentTaskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.latestFlushedTaskCount);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStream, startOffset);
    cmdItor = hwParserCsr.getCommandItor<PIPE_CONTROL>();
    ASSERT_NE(hwParserCsr.cmdList.end(), cmdItor);
    pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*cmdItor);
    ASSERT_NE(nullptr, pipeControlCmd);
    EXPECT_EQ(postsyncAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControlCmd));
    EXPECT_EQ(expectedCount, pipeControlCmd->getImmediateData());

    cmdItor++;
    ASSERT_NE(hwParserCsr.cmdList.end(), cmdItor);
    if (additionalSyncCmd) {
        cmdItor++;
        ASSERT_NE(hwParserCsr.cmdList.end(), cmdItor);
    }
    bbEndCmd = genCmdCast<MI_BATCH_BUFFER_END *>(*cmdItor);
    ASSERT_NE(nullptr, bbEndCmd);

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(csrTagAllocation));
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(immediateListCmdBufferAllocation));

    startOffset = commandStream.getUsed();
    EXPECT_EQ(0u, (startOffset % MemoryConstants::cacheLineSize));
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenRequireTaskCountUpdateSelectedThenDispatchPipeControlPostSyncToImmediateBatchBuffer,
          IsHeapfulSupportedAndAtLeastXeCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto heapless = commandStreamReceiver.heaplessModeEnabled;
    auto heaplessStateInit = commandStreamReceiver.heaplessStateInitialized;

    bool additionalSyncCmd = NEO::MemorySynchronizationCommands<FamilyType>::getSizeForSingleAdditionalSynchronization(NEO::FenceType::release, commandStreamReceiver.peekRootDeviceEnvironment()) > 0;
    commandStreamReceiver.storeMakeResidentAllocations = true;

    auto startOffset = commandStream.getUsed();
    auto immediateListCmdBufferAllocation = commandStream.getGraphicsAllocation();
    UnitTestHelper<FamilyType>::getSpaceAndInitWalkerCmd(commandStream, heapless);
    auto csrTagAllocation = commandStreamReceiver.getTagAllocation();
    uint64_t postsyncAddress = csrTagAllocation->getGpuAddress();

    immediateFlushTaskFlags.requireTaskCountUpdate = true;
    auto completionStamp = heaplessStateInit ? commandStreamReceiver.flushImmediateTaskStateless(commandStream, startOffset, immediateFlushTaskFlags, *pDevice)
                                             : commandStreamReceiver.flushImmediateTask(commandStream, startOffset, immediateFlushTaskFlags, *pDevice);

    auto expectedCount = heaplessStateInit ? 2u : 1u;
    EXPECT_EQ(expectedCount, completionStamp.taskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.taskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.latestSentTaskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.latestFlushedTaskCount);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStream, 0);
    auto cmdItor = hwParserCsr.getCommandItor<PIPE_CONTROL>();
    ASSERT_NE(hwParserCsr.cmdList.end(), cmdItor);
    auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*cmdItor);
    ASSERT_NE(nullptr, pipeControlCmd);
    EXPECT_EQ(postsyncAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControlCmd));
    EXPECT_EQ(expectedCount, pipeControlCmd->getImmediateData());

    cmdItor++;
    ASSERT_NE(hwParserCsr.cmdList.end(), cmdItor);
    if (additionalSyncCmd) {
        cmdItor++;
        ASSERT_NE(hwParserCsr.cmdList.end(), cmdItor);
    }
    auto bbEndCmd = genCmdCast<MI_BATCH_BUFFER_END *>(*cmdItor);
    ASSERT_NE(nullptr, bbEndCmd);

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(csrTagAllocation));
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(immediateListCmdBufferAllocation));

    startOffset = commandStream.getUsed();
    EXPECT_EQ(0u, (startOffset % MemoryConstants::cacheLineSize));

    UnitTestHelper<FamilyType>::getSpaceAndInitWalkerCmd(commandStream, heapless);

    expectedCount++;

    completionStamp = commandStreamReceiver.flushImmediateTask(commandStream, startOffset, immediateFlushTaskFlags, *pDevice);
    EXPECT_EQ(expectedCount, completionStamp.taskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.taskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.latestSentTaskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.latestFlushedTaskCount);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStream, startOffset);
    cmdItor = hwParserCsr.getCommandItor<PIPE_CONTROL>();
    ASSERT_NE(hwParserCsr.cmdList.end(), cmdItor);
    pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*cmdItor);
    ASSERT_NE(nullptr, pipeControlCmd);
    EXPECT_EQ(postsyncAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControlCmd));
    EXPECT_EQ(expectedCount, pipeControlCmd->getImmediateData());

    cmdItor++;
    ASSERT_NE(hwParserCsr.cmdList.end(), cmdItor);
    if (additionalSyncCmd) {
        cmdItor++;
        ASSERT_NE(hwParserCsr.cmdList.end(), cmdItor);
    }
    bbEndCmd = genCmdCast<MI_BATCH_BUFFER_END *>(*cmdItor);
    ASSERT_NE(nullptr, bbEndCmd);

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(csrTagAllocation));
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(immediateListCmdBufferAllocation));

    startOffset = commandStream.getUsed();
    EXPECT_EQ(0u, (startOffset % MemoryConstants::cacheLineSize));
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenPreambleIsUsedOrNotThenCsrBufferIsUsedOrImmediateBufferIsUsed,
          IsAtLeastXeCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;
    commandStreamReceiver.recordFlushedBatchBuffer = true;
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }

    auto startOffset = commandStream.getUsed();
    auto immediateListCmdBufferAllocation = commandStream.getGraphicsAllocation();

    *commandStream.getSpaceForCmd<DefaultWalkerType>() = FamilyType::template getInitGpuWalker<DefaultWalkerType>();

    immediateFlushTaskFlags.hasStallingCmds = true;
    auto completionStamp = commandStreamReceiver.flushImmediateTask(commandStream, startOffset, immediateFlushTaskFlags, *pDevice);

    auto csrCmdBufferAllocation = commandStreamReceiver.commandStream.getGraphicsAllocation();

    TaskCountType currentTaskCountType = 1u;

    EXPECT_EQ(currentTaskCountType, completionStamp.taskCount);
    EXPECT_EQ(currentTaskCountType, commandStreamReceiver.taskCount);
    EXPECT_EQ(currentTaskCountType, commandStreamReceiver.latestSentTaskCount);
    EXPECT_EQ(0u, commandStreamReceiver.latestFlushedTaskCount);

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(csrCmdBufferAllocation, currentTaskCountType));
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(immediateListCmdBufferAllocation, currentTaskCountType));

    BatchBuffer &recordedBatchBuffer = commandStreamReceiver.latestFlushedBatchBuffer;
    EXPECT_EQ(csrCmdBufferAllocation, recordedBatchBuffer.commandBufferAllocation);
    EXPECT_EQ(0u, recordedBatchBuffer.startOffset);
    EXPECT_EQ(true, recordedBatchBuffer.hasStallingCmds);
    EXPECT_EQ(false, recordedBatchBuffer.hasRelaxedOrderingDependencies);

    startOffset = commandStream.getUsed();

    *commandStream.getSpaceForCmd<DefaultWalkerType>() = FamilyType::template getInitGpuWalker<DefaultWalkerType>();

    immediateFlushTaskFlags.hasRelaxedOrderingDependencies = true;
    completionStamp = commandStreamReceiver.flushImmediateTask(commandStream, startOffset, immediateFlushTaskFlags, *pDevice);

    currentTaskCountType = 2u;

    EXPECT_EQ(currentTaskCountType, completionStamp.taskCount);
    EXPECT_EQ(currentTaskCountType, commandStreamReceiver.taskCount);
    EXPECT_EQ(currentTaskCountType, commandStreamReceiver.latestSentTaskCount);
    EXPECT_EQ(0u, commandStreamReceiver.latestFlushedTaskCount);

    EXPECT_FALSE(commandStreamReceiver.isMadeResident(csrCmdBufferAllocation, currentTaskCountType));
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(immediateListCmdBufferAllocation, currentTaskCountType));

    recordedBatchBuffer = commandStreamReceiver.latestFlushedBatchBuffer;
    EXPECT_EQ(immediateListCmdBufferAllocation, recordedBatchBuffer.commandBufferAllocation);
    EXPECT_EQ(startOffset, recordedBatchBuffer.startOffset);
    EXPECT_EQ(true, recordedBatchBuffer.hasStallingCmds);
    EXPECT_EQ(true, recordedBatchBuffer.hasRelaxedOrderingDependencies);
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenFlushOperationFailsThenExpectNoBatchBufferSentAndCorrectFailCompletionReturned,
          IsAtLeastXeCore) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    bool heapless = commandStreamReceiver.heaplessModeEnabled;
    bool heaplessStateInit = commandStreamReceiver.heaplessStateInitialized;
    commandStreamReceiver.storeMakeResidentAllocations = true;
    commandStreamReceiver.recordFlushedBatchBuffer = true;

    auto startOffset = commandStream.getUsed();
    auto immediateListCmdBufferAllocation = commandStream.getGraphicsAllocation();

    UnitTestHelper<FamilyType>::getSpaceAndInitWalkerCmd(commandStream, heapless);

    immediateFlushTaskFlags.blockingAppend = true;
    commandStreamReceiver.flushReturnValue = NEO::SubmissionStatus::failed;

    auto completionStamp = heaplessStateInit ? commandStreamReceiver.flushImmediateTaskStateless(commandStream, startOffset, immediateFlushTaskFlags, *pDevice)
                                             : commandStreamReceiver.flushImmediateTask(commandStream, startOffset, immediateFlushTaskFlags, *pDevice);

    auto csrCmdBufferAllocation = commandStreamReceiver.commandStream.getGraphicsAllocation();

    TaskCountType currentTaskCountType = 1u;

    auto expectedCount = heaplessStateInit ? 1u : 0u;

    EXPECT_EQ(NEO::CompletionStamp::failed, completionStamp.taskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.taskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.latestSentTaskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.latestFlushedTaskCount);

    EXPECT_FALSE(commandStreamReceiver.isMadeResident(csrCmdBufferAllocation, currentTaskCountType));
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(immediateListCmdBufferAllocation, currentTaskCountType));

    BatchBuffer &recordedBatchBuffer = commandStreamReceiver.latestFlushedBatchBuffer;
    EXPECT_EQ(nullptr, recordedBatchBuffer.commandBufferAllocation);
    EXPECT_EQ(0u, recordedBatchBuffer.startOffset);
    EXPECT_EQ(false, recordedBatchBuffer.hasStallingCmds);
    EXPECT_EQ(false, recordedBatchBuffer.hasRelaxedOrderingDependencies);

    completionStamp = heaplessStateInit ? commandStreamReceiver.flushImmediateTaskStateless(commandStream, startOffset, immediateFlushTaskFlags, *pDevice)
                                        : commandStreamReceiver.flushImmediateTask(commandStream, startOffset, immediateFlushTaskFlags, *pDevice);

    EXPECT_EQ(NEO::CompletionStamp::failed, completionStamp.taskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.taskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.latestSentTaskCount);
    EXPECT_EQ(expectedCount, commandStreamReceiver.latestFlushedTaskCount);

    if (heaplessStateInit) {
        EXPECT_TRUE(commandStreamReceiver.isMadeResident(immediateListCmdBufferAllocation, currentTaskCountType));
    } else {
        EXPECT_FALSE(commandStreamReceiver.isMadeResident(immediateListCmdBufferAllocation, currentTaskCountType));
    }
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenPreemptionModeProgrammingNeededThenOneTimePreemptionModeDispatchedOnSupportingPlatform,
          IsAtLeastXeCore) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto heaplessStateInit = commandStreamReceiver.heaplessStateInitialized;

    bool preemptionModeProgramming = NEO::PreemptionHelper::getRequiredCmdStreamSize<FamilyType>(pDevice->getPreemptionMode(), commandStreamReceiver.getPreemptionMode()) > 0;
    auto preemptionDetails = getPreemptionTestHwDetails<FamilyType>();

    if (!heaplessStateInit) {
        EXPECT_EQ(NEO::PreemptionMode::Initial, commandStreamReceiver.getPreemptionMode());
    }

    heaplessStateInit ? commandStreamReceiver.flushImmediateTaskStateless(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice)
                      : commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    EXPECT_EQ(pDevice->getPreemptionMode(), commandStreamReceiver.getPreemptionMode());

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto loadRegisterImmList = hwParserCsr.getCommandsList<MI_LOAD_REGISTER_IMM>();
    bool foundPreemptionMode = false;
    for (const auto &it : loadRegisterImmList) {
        auto loadRegisterImmCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(it);
        if (loadRegisterImmCmd->getRegisterOffset() == preemptionDetails.regAddress) {
            foundPreemptionMode = true;
        }
    }
    EXPECT_EQ(preemptionModeProgramming, foundPreemptionMode);

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();

    heaplessStateInit ? commandStreamReceiver.flushImmediateTaskStateless(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice)
                      : commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    loadRegisterImmList = hwParserCsr.getCommandsList<MI_LOAD_REGISTER_IMM>();

    foundPreemptionMode = false;
    for (const auto &it : loadRegisterImmList) {
        auto loadRegisterImmCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(it);
        if (loadRegisterImmCmd->getRegisterOffset() == preemptionDetails.regAddress) {
            foundPreemptionMode = true;
        }
    }
    EXPECT_EQ(false, foundPreemptionMode);
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenCsrSurfaceProgrammingNeededThenOneTimeCsrSurfaceDispatchedOnSupportingPlatform,
          IsAtLeastXeCore) {

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }
    commandStreamReceiver.storeMakeResidentAllocations = true;
    if (commandStreamReceiver.getPreemptionAllocation() == nullptr) {
        auto createdPreemptionAllocation = commandStreamReceiver.createPreemptionAllocation();
        ASSERT_TRUE(createdPreemptionAllocation);
    }

    bool csrSurfaceProgramming = NEO::PreemptionHelper::getRequiredPreambleSize<FamilyType>(*pDevice) > 0;

    EXPECT_EQ(NEO::PreemptionMode::Initial, commandStreamReceiver.getPreemptionMode());
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);
    EXPECT_EQ(pDevice->getPreemptionMode(), commandStreamReceiver.getPreemptionMode());

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto itCsrCommand = NEO::UnitTestHelper<FamilyType>::findCsrBaseAddressCommand(hwParserCsr.cmdList.begin(), hwParserCsr.cmdList.end());
    if (csrSurfaceProgramming) {
        EXPECT_NE(hwParserCsr.cmdList.end(), itCsrCommand);
    } else {
        EXPECT_EQ(hwParserCsr.cmdList.end(), itCsrCommand);
    }

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(commandStreamReceiver.getPreemptionAllocation()));

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    itCsrCommand = NEO::UnitTestHelper<FamilyType>::findCsrBaseAddressCommand(hwParserCsr.cmdList.begin(), hwParserCsr.cmdList.end());
    EXPECT_EQ(hwParserCsr.cmdList.end(), itCsrCommand);

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(commandStreamReceiver.getPreemptionAllocation()));
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenSipProgrammingNeededThenOneTimeSipStateDispatched,
          IsAtLeastXeCore) {
    using STATE_SIP = typename FamilyType::STATE_SIP;

    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->initDebuggerL0(pDevice);
    pDevice->getL0Debugger()->initialize();

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }

    commandStreamReceiver.storeMakeResidentAllocations = true;

    EXPECT_FALSE(commandStreamReceiver.getSipSentFlag());
    commandStreamReceiver.setSipSentFlag(true);
    EXPECT_TRUE(commandStreamReceiver.getSipSentFlag());
    commandStreamReceiver.setSipSentFlag(false);
    EXPECT_FALSE(commandStreamReceiver.getSipSentFlag());
    auto debugSurfaceSize = 64u;
    NEO::GraphicsAllocation *debugSurface = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {pDevice->getRootDeviceIndex(), true,
         debugSurfaceSize,
         NEO::AllocationType::debugContextSaveArea,
         false,
         false,
         pDevice->getDeviceBitfield()});
    pDevice->setDebugSurface(debugSurface);

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    auto sipAllocation = NEO::SipKernel::getSipKernel(*pDevice, nullptr).getSipAllocation();

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(sipAllocation));

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto stateSipCmd = hwParserCsr.getCommand<STATE_SIP>();
    ASSERT_NE(nullptr, stateSipCmd);

    EXPECT_TRUE(commandStreamReceiver.getSipSentFlag());

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    hwParserCsr.tearDown();
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    stateSipCmd = hwParserCsr.getCommand<STATE_SIP>();
    EXPECT_EQ(nullptr, stateSipCmd);

    EXPECT_TRUE(commandStreamReceiver.getSipSentFlag());

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(sipAllocation));
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenOfflineDebuggingModeWhenFlushTaskImmediateCalledThenCorrectContextSipIsResident,
          IsAtLeastXeCore) {
    using STATE_SIP = typename FamilyType::STATE_SIP;

    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->initDebuggerL0(pDevice);
    pDevice->getL0Debugger()->initialize();
    pDevice->getExecutionEnvironment()->setDebuggingMode(DebuggingMode::offline);
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }
    commandStreamReceiver.storeMakeResidentAllocations = true;

    EXPECT_FALSE(commandStreamReceiver.getSipSentFlag());
    commandStreamReceiver.setSipSentFlag(true);
    EXPECT_TRUE(commandStreamReceiver.getSipSentFlag());
    commandStreamReceiver.setSipSentFlag(false);
    EXPECT_FALSE(commandStreamReceiver.getSipSentFlag());
    auto debugSurfaceSize = 64u;
    NEO::GraphicsAllocation *debugSurface = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {pDevice->getRootDeviceIndex(), true,
         debugSurfaceSize,
         NEO::AllocationType::debugContextSaveArea,
         false,
         false,
         pDevice->getDeviceBitfield()});
    pDevice->setDebugSurface(debugSurface);

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    auto sipAllocation = NEO::SipKernel::getSipKernel(*pDevice, &commandStreamReceiver.getOsContext()).getSipAllocation();

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(sipAllocation));

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto stateSipCmd = hwParserCsr.getCommand<STATE_SIP>();
    ASSERT_NE(nullptr, stateSipCmd);

    auto expectedAddress = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo) ? sipAllocation->getGpuAddress() : sipAllocation->getGpuAddressToPatch();
    EXPECT_EQ(expectedAddress, stateSipCmd->getSystemInstructionPointer());

    EXPECT_TRUE(commandStreamReceiver.getSipSentFlag());

    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    EXPECT_TRUE(commandStreamReceiver.getSipSentFlag());
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(sipAllocation));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTest, givenScratchSpaceSurfaceStateEnabledWhenRequiredScratchSpaceIsSetThenPerThreadScratchSizeIsAlignedNextPow2) {
    auto commandStreamReceiver = std::make_unique<MockCsrHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto scratchController = static_cast<MockScratchSpaceControllerXeHPAndLater *>(commandStreamReceiver->getScratchSpaceController());

    uint32_t perThreadScratchSize = 65;
    uint32_t expectedValue = Math::nextPowerOfTwo(perThreadScratchSize);

    bool stateBaseAddressDirty = false;
    bool cfeStateDirty = false;
    uint8_t surfaceHeap[1000];
    scratchController->setRequiredScratchSpace(surfaceHeap, 0u, perThreadScratchSize, 0u, *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);
    EXPECT_EQ(expectedValue, scratchController->perThreadScratchSpaceSlot0Size);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverHwTest, givenScratchSpaceSurfaceStateEnabledWhenSizeForPrivateScratchSpaceIsMisalignedThenAlignItNextPow2) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    DebugManagerStateRestore restorer;
    debugManager.flags.EnablePrivateScratchSlot1.set(1);
    RENDER_SURFACE_STATE surfaceState[4];
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto scratchController = static_cast<MockScratchSpaceControllerXeHPAndLater *>(commandStreamReceiver.getScratchSpaceController());

    uint32_t misalignedSizeForPrivateScratch = MemoryConstants::pageSize + 1;
    uint32_t alignedSizeForPrivateScratch = Math::nextPowerOfTwo(misalignedSizeForPrivateScratch);

    bool cfeStateDirty = false;
    bool stateBaseAddressDirty = false;
    scratchController->setRequiredScratchSpace(surfaceState, 0u, 0u, misalignedSizeForPrivateScratch,
                                               *pDevice->getDefaultEngine().osContext, stateBaseAddressDirty, cfeStateDirty);

    EXPECT_NE(scratchController->perThreadScratchSpaceSlot1Size, misalignedSizeForPrivateScratch);
    EXPECT_EQ(scratchController->perThreadScratchSpaceSlot1Size, alignedSizeForPrivateScratch);

    size_t scratchSlot1SizeInBytes = alignedSizeForPrivateScratch * scratchController->computeUnitsUsedForScratch;
    auto &productHelper = pDevice->getProductHelper();
    productHelper.adjustScratchSize(scratchSlot1SizeInBytes);

    EXPECT_EQ(scratchController->scratchSlot1SizeInBytes, scratchSlot1SizeInBytes);

    EXPECT_EQ(scratchController->scratchSlot1SizeInBytes, scratchController->getScratchSpaceSlot1Allocation()->getUnderlyingBufferSize());
}

HWTEST_F(CommandStreamReceiverHwTest, givenDcFlushRequiredWhenProgramStallingPostSyncCommandsForBarrierCalledThenDcFlushSet) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.dcFlushSupport = true;
    if (ultCsr.isMultiTileOperationEnabled()) {
        GTEST_SKIP();
    }
    char commandBuffer[MemoryConstants::pageSize];
    LinearStream commandStream(commandBuffer, MemoryConstants::pageSize);
    TagNodeBase *tagNode = ultCsr.getTimestampPacketAllocator()->getTag();
    constexpr bool dcFlushRequired = true;
    ultCsr.programStallingPostSyncCommandsForBarrier(commandStream, *tagNode, dcFlushRequired);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        commandStream.getCpuBase(),
        commandStream.getUsed()));
    auto pipeControlIteratorVector = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_GE(pipeControlIteratorVector.size(), 1u);
    auto pipeControlIterator = pipeControlIteratorVector[0];
    const bool barrierWaRequired = MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(pDevice->getRootDeviceEnvironment());
    if (barrierWaRequired) {
        ASSERT_GE(pipeControlIteratorVector.size(), 2u);
        pipeControlIterator = pipeControlIteratorVector[1];
    }
    auto pipeControl = genCmdCast<PIPE_CONTROL *>(*pipeControlIterator);
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_TRUE(pipeControl->getDcFlushEnable());
}

HWTEST_F(CommandStreamReceiverHwTest, givenDcFlushRequiredButNoDcFlushSupportWhenProgramStallingPostSyncCommandsForBarrierCalledThenDcFlushNotSet) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.dcFlushSupport = false;
    if (ultCsr.isMultiTileOperationEnabled()) {
        GTEST_SKIP();
    }
    char commandBuffer[MemoryConstants::pageSize];
    LinearStream commandStream(commandBuffer, MemoryConstants::pageSize);
    TagNodeBase *tagNode = ultCsr.getTimestampPacketAllocator()->getTag();
    constexpr bool dcFlushRequired = true;
    ultCsr.programStallingPostSyncCommandsForBarrier(commandStream, *tagNode, dcFlushRequired);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        commandStream.getCpuBase(),
        commandStream.getUsed()));
    auto pipeControlIteratorVector = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_GE(pipeControlIteratorVector.size(), 1u);
    auto pipeControlIterator = pipeControlIteratorVector[0];
    const bool barrierWaRequired = MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(pDevice->getRootDeviceEnvironment());
    if (barrierWaRequired) {
        ASSERT_GE(pipeControlIteratorVector.size(), 2u);
        pipeControlIterator = pipeControlIteratorVector[1];
    }
    auto pipeControl = genCmdCast<PIPE_CONTROL *>(*pipeControlIterator);
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_FALSE(pipeControl->getDcFlushEnable());
}

HWTEST_F(CommandStreamReceiverHwTest, givenDcFlushRequiredFalseWhenProgramStallingPostSyncCommandsForBarrierCalledThenDcFlushNotSet) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.dcFlushSupport = true;
    if (ultCsr.isMultiTileOperationEnabled()) {
        GTEST_SKIP();
    }
    char commandBuffer[MemoryConstants::pageSize];
    LinearStream commandStream(commandBuffer, MemoryConstants::pageSize);
    TagNodeBase *tagNode = ultCsr.getTimestampPacketAllocator()->getTag();
    constexpr bool dcFlushRequired = false;
    ultCsr.programStallingPostSyncCommandsForBarrier(commandStream, *tagNode, dcFlushRequired);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        commandStream.getCpuBase(),
        commandStream.getUsed()));
    auto pipeControlIteratorVector = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_GE(pipeControlIteratorVector.size(), 1u);
    auto pipeControlIterator = pipeControlIteratorVector[0];
    const bool barrierWaRequired = MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(pDevice->getRootDeviceEnvironment());
    if (barrierWaRequired) {
        ASSERT_GE(pipeControlIteratorVector.size(), 2u);
        pipeControlIterator = pipeControlIteratorVector[1];
    }
    auto pipeControl = genCmdCast<PIPE_CONTROL *>(*pipeControlIterator);
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_FALSE(pipeControl->getDcFlushEnable());
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskWhenNextDispatchRequiresScratchSpaceAndSshPointerIsNullThenFrontEndCommandIsNotDispatched,
          IsHeapfulSupportedAndAtLeastXeCore) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_TRUE(commandStreamReceiver.getMediaVFEStateDirty());

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    commandStreamReceiver.setRequiredScratchSizes(0x100, 0);
    immediateFlushTaskFlags.sshCpuBase = nullptr;

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    auto frontEndCmd = hwParserCsr.getCommand<CFE_STATE>();
    EXPECT_EQ(nullptr, frontEndCmd);
    EXPECT_FALSE(commandStreamReceiver.getMediaVFEStateDirty());
}

HWTEST_F(CommandStreamReceiverTest, givenCsrWhenCleanUpResourcesThenOwnedPrivateAllocationsAreFreed) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto mockGA = std::make_unique<MockGraphicsAllocation>();

    auto mapForReuse = &csr.getOwnedPrivateAllocations();
    mapForReuse->push_back({0x100, mockGA.release()});
    csr.cleanupResources();
    EXPECT_EQ(mapForReuse->size(), 0u);
}

HWTEST2_F(CommandStreamReceiverHwTest, GivenDirtyFlagForContextInBindlessHelperWhenFlushTaskCalledThenStateCacheInvalidateIsSent, IsHeapfulSupported) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }

    auto bindlessHeapsHelper = std::make_unique<MockBindlesHeapsHelper>(pDevice, pDevice->getNumGenericSubDevices() > 1);
    MockBindlesHeapsHelper *bindlessHeapsHelperPtr = bindlessHeapsHelper.get();
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHeapsHelper.release());

    bindlessHeapsHelperPtr->stateCacheDirtyForContext.set(commandStreamReceiver.getOsContext().getContextId());

    flushTaskFlags.implicitFlush = true;
    auto usedSpaceBefore = commandStreamReceiver.commandStream.getUsed();

    commandStreamReceiver.flushTask(commandStream,
                                    0,
                                    &dsh,
                                    &ioh,
                                    nullptr,
                                    taskLevel,
                                    flushTaskFlags,
                                    *pDevice);

    auto usedSpaceAfter = commandStreamReceiver.commandStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandStreamReceiver.commandStream.getCpuBase(), 0), usedSpaceAfter));

    auto pipeControls = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, pipeControls.size());

    bool pcFound = false;
    for (size_t i = 0; i < pipeControls.size(); i++) {
        auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*pipeControls[i]);
        bool csStall = pipeControl->getCommandStreamerStallEnable();
        bool stateCache = pipeControl->getStateCacheInvalidationEnable();
        bool texCache = pipeControl->getTextureCacheInvalidationEnable();
        bool renderTargetCache = pipeControl->getRenderTargetCacheFlushEnable();

        if (csStall && stateCache && texCache && renderTargetCache) {
            pcFound = true;
            break;
        }
    }
    EXPECT_TRUE(pcFound);
    EXPECT_FALSE(bindlessHeapsHelperPtr->getStateDirtyForContext(commandStreamReceiver.getOsContext().getContextId()));
}

HEAPFUL_HWTEST_F(CommandStreamReceiverHwTest, GivenDirtyFlagForContextInBindlessHelperWhenFlushImmediateTaskCalledThenStateCacheInvalidateIsSent) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto bindlessHeapsHelper = std::make_unique<MockBindlesHeapsHelper>(pDevice, pDevice->getNumGenericSubDevices() > 1);
    MockBindlesHeapsHelper *bindlessHeapsHelperPtr = bindlessHeapsHelper.get();
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHeapsHelper.release());

    bindlessHeapsHelperPtr->stateCacheDirtyForContext.set(commandStreamReceiver.getOsContext().getContextId());

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto pcCmd = hwParserCsr.getCommand<PIPE_CONTROL>();
    ASSERT_NE(nullptr, pcCmd);

    EXPECT_TRUE(pcCmd->getCommandStreamerStallEnable());
    EXPECT_TRUE(pcCmd->getStateCacheInvalidationEnable());
    EXPECT_TRUE(pcCmd->getTextureCacheInvalidationEnable());
    EXPECT_TRUE(pcCmd->getRenderTargetCacheFlushEnable());

    EXPECT_FALSE(bindlessHeapsHelperPtr->getStateDirtyForContext(commandStreamReceiver.getOsContext().getContextId()));
}

HWTEST2_F(CommandStreamReceiverHwTest, GivenContextInitializedAndDirtyFlagForContextInBindlessHelperWhenFlushImmediateTaskCalledThenStateCacheInvalidateIsSentBeforeBbStartJumpingToImmediateBuffer, IsHeapfulSupported) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    // 1st flush, dispatch all required n-p state commands
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);
    auto usedAfterFirstFlush = commandStreamReceiver.commandStream.getUsed();

    auto bindlessHeapsHelper = std::make_unique<MockBindlesHeapsHelper>(pDevice, pDevice->getNumGenericSubDevices() > 1);
    MockBindlesHeapsHelper *bindlessHeapsHelperPtr = bindlessHeapsHelper.get();
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHeapsHelper.release());

    bindlessHeapsHelperPtr->stateCacheDirtyForContext.set(commandStreamReceiver.getOsContext().getContextId());

    // only state cache flush is dispatched in dynamic preamble
    auto immediateBufferStartOffset = commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream, immediateBufferStartOffset, immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedAfterFirstFlush);

    auto cmdPtrIt = hwParserCsr.cmdList.begin();
    ASSERT_NE(hwParserCsr.cmdList.end(), cmdPtrIt);

    auto pipeControl = genCmdCast<PIPE_CONTROL *>(*cmdPtrIt);
    ASSERT_NE(nullptr, pipeControl);

    EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
    EXPECT_TRUE(pipeControl->getStateCacheInvalidationEnable());
    EXPECT_TRUE(pipeControl->getTextureCacheInvalidationEnable());
    EXPECT_TRUE(pipeControl->getRenderTargetCacheFlushEnable());

    EXPECT_FALSE(bindlessHeapsHelperPtr->getStateDirtyForContext(commandStreamReceiver.getOsContext().getContextId()));

    cmdPtrIt++;
    ASSERT_NE(hwParserCsr.cmdList.end(), cmdPtrIt);

    auto bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*cmdPtrIt);
    ASSERT_NE(nullptr, bbStart);

    EXPECT_EQ(cmdBufferGpuAddress + immediateBufferStartOffset, bbStart->getBatchBufferStartAddress());
}

HWTEST_F(CommandStreamReceiverHwTest, givenRequiresInstructionCacheFlushWhenFlushImmediateThenInstructionCacheInvalidateEnableIsSent) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }
    commandStreamReceiver.registerInstructionCacheFlush();

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto pcCmd = hwParserCsr.getCommand<PIPE_CONTROL>();
    ASSERT_NE(nullptr, pcCmd);

    EXPECT_TRUE(pcCmd->getInstructionCacheInvalidateEnable());
    EXPECT_FALSE(commandStreamReceiver.requiresInstructionCacheFlush);
}

HWTEST_F(CommandStreamReceiverHwTest, givenContextInitializedAndRequiresInstructionCacheFlushWhenFlushImmediateThenInstructionCacheInvalidateEnableIsSentBeforeBbStartJumpingToImmediateBuffer) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }

    // 1st flush, dispatch all required n-p state commands
    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);
    auto usedAfterFirstFlush = commandStreamReceiver.commandStream.getUsed();

    commandStreamReceiver.registerInstructionCacheFlush();

    // only instruction cache flush is dispatched in dynamic preamble
    auto immediateBufferStartOffset = commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream, immediateBufferStartOffset, immediateFlushTaskFlags, *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedAfterFirstFlush);

    auto cmdPtrIt = hwParserCsr.cmdList.begin();
    ASSERT_NE(hwParserCsr.cmdList.end(), cmdPtrIt);

    auto pipeControl = genCmdCast<PIPE_CONTROL *>(*cmdPtrIt);
    ASSERT_NE(nullptr, pipeControl);

    EXPECT_TRUE(pipeControl->getInstructionCacheInvalidateEnable());
    EXPECT_FALSE(commandStreamReceiver.requiresInstructionCacheFlush);

    cmdPtrIt++;
    ASSERT_NE(hwParserCsr.cmdList.end(), cmdPtrIt);

    auto bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*cmdPtrIt);
    ASSERT_NE(nullptr, bbStart);

    EXPECT_EQ(cmdBufferGpuAddress + immediateBufferStartOffset, bbStart->getBatchBufferStartAddress());
}

HWTEST_F(CommandStreamReceiverHwTest, GivenFlushIsBlockingWhenFlushTaskCalledThenExpectMonitorFenceFlagTrue) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.recordFlushedBatchBuffer = true;

    commandStreamReceiver.taskCount = 5;
    flushTaskFlags.blocking = true;
    commandStreamReceiver.flushTask(commandStream,
                                    0,
                                    &dsh,
                                    &ioh,
                                    nullptr,
                                    taskLevel,
                                    flushTaskFlags,
                                    *pDevice);

    EXPECT_TRUE(commandStreamReceiver.latestFlushedBatchBuffer.dispatchMonitorFence);
    EXPECT_EQ(6u, commandStreamReceiver.peekLatestFlushedTaskCount());
}

HWTEST_F(CommandStreamReceiverHwTest, GivenFlushIsDcFlushWhenFlushTaskCalledThenExpectMonitorFenceFlagTrue) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.recordFlushedBatchBuffer = true;

    commandStreamReceiver.taskCount = 11;
    flushTaskFlags.dcFlush = true;
    commandStreamReceiver.flushTask(commandStream,
                                    0,
                                    &dsh,
                                    &ioh,
                                    nullptr,
                                    taskLevel,
                                    flushTaskFlags,
                                    *pDevice);

    EXPECT_TRUE(commandStreamReceiver.latestFlushedBatchBuffer.dispatchMonitorFence);
    EXPECT_EQ(12u, commandStreamReceiver.peekLatestFlushedTaskCount());
}

HWTEST_F(CommandStreamReceiverHwTest, GivenFlushGuardBufferWithPipeControlWhenFlushTaskCalledThenExpectMonitorFenceFlagTrue) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.recordFlushedBatchBuffer = true;

    commandStreamReceiver.taskCount = 17;
    flushTaskFlags.guardCommandBufferWithPipeControl = true;
    commandStreamReceiver.flushTask(commandStream,
                                    0,
                                    &dsh,
                                    &ioh,
                                    nullptr,
                                    taskLevel,
                                    flushTaskFlags,
                                    *pDevice);

    EXPECT_TRUE(commandStreamReceiver.latestFlushedBatchBuffer.dispatchMonitorFence);
    EXPECT_EQ(18u, commandStreamReceiver.peekLatestFlushedTaskCount());
}

HWTEST_F(CommandStreamReceiverHwTest, GivenFlushHeapStorageRequiresRecyclingTagWhenFlushTaskCalledThenExpectMonitorFenceFlagTrue) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.recordFlushedBatchBuffer = true;

    commandStreamReceiver.taskCount = 23;
    commandStreamReceiver.heapStorageRequiresRecyclingTag = true;
    commandStreamReceiver.flushTask(commandStream,
                                    0,
                                    &dsh,
                                    &ioh,
                                    nullptr,
                                    taskLevel,
                                    flushTaskFlags,
                                    *pDevice);

    EXPECT_TRUE(commandStreamReceiver.latestFlushedBatchBuffer.dispatchMonitorFence);
    EXPECT_EQ(24u, commandStreamReceiver.peekLatestFlushedTaskCount());
}

HWTEST_F(CommandStreamReceiverHwTest, givenEpilogueStreamAvailableWhenFlushTaskCalledThenDispachEpilogueCommandsIntoEpilogueStream) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessModeEnabled) {
        GTEST_SKIP();
    }

    GraphicsAllocation *commandBuffer = commandStreamReceiver.getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{commandStreamReceiver.getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream epilogueStream(commandBuffer);

    commandStreamReceiver.storeMakeResidentAllocations = true;
    flushTaskFlags.guardCommandBufferWithPipeControl = true;
    flushTaskFlags.optionalEpilogueCmdStream = &epilogueStream;

    commandStream.getSpace(4);

    commandStreamReceiver.flushTask(commandStream,
                                    0,
                                    &dsh,
                                    &ioh,
                                    nullptr,
                                    taskLevel,
                                    flushTaskFlags,
                                    *pDevice);

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(commandBuffer));

    HardwareParse hwParser;

    hwParser.parseCommands<FamilyType>(epilogueStream, 0);
    auto cmdIterator = find<typename FamilyType::PIPE_CONTROL *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_NE(hwParser.cmdList.end(), cmdIterator);

    commandStreamReceiver.getMemoryManager()->freeGraphicsMemoryImpl(commandBuffer);
}

HWTEST2_F(CommandStreamReceiverHwTest, givenSpecialPipelineSelectModeChangedWhenGetCmdSizeForPielineSelectIsCalledThenCorrectSizeIsReturned, IsAtMostXeCore) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    CsrSizeRequestFlags csrSizeRequest = {};
    DispatchFlags flags = DispatchFlagsHelper::createDefaultDispatchFlags();

    csrSizeRequest.systolicPipelineSelectMode = true;
    commandStreamReceiver.overrideCsrSizeReqFlags(csrSizeRequest);
    size_t size = commandStreamReceiver.getCmdSizeForPipelineSelect();

    size_t expectedSize = sizeof(PIPELINE_SELECT);
    if (MemorySynchronizationCommands<FamilyType>::isBarrierPriorToPipelineSelectWaRequired(pDevice->getRootDeviceEnvironment())) {
        expectedSize += sizeof(PIPE_CONTROL);
    }
    EXPECT_EQ(expectedSize, size);
}

HWTEST2_F(CommandStreamReceiverHwTest, givenCsrWhenPreambleSentThenRequiredCsrSizeDependsOnmediaSamplerConfigChanged, IsAtMostXeCore) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    CsrSizeRequestFlags csrSizeRequest = {};
    DispatchFlags flags = DispatchFlagsHelper::createDefaultDispatchFlags();

    commandStreamReceiver.isPreambleSent = true;

    csrSizeRequest.mediaSamplerConfigChanged = false;
    commandStreamReceiver.overrideCsrSizeReqFlags(csrSizeRequest);
    auto mediaSamplerConfigNotChangedSize = commandStreamReceiver.getRequiredCmdStreamSize(flags, *pDevice);

    csrSizeRequest.mediaSamplerConfigChanged = true;
    commandStreamReceiver.overrideCsrSizeReqFlags(csrSizeRequest);
    auto mediaSamplerConfigChangedSize = commandStreamReceiver.getRequiredCmdStreamSize(flags, *pDevice);

    EXPECT_NE(mediaSamplerConfigChangedSize, mediaSamplerConfigNotChangedSize);
    auto difference = mediaSamplerConfigChangedSize - mediaSamplerConfigNotChangedSize;

    size_t expectedDifference = sizeof(PIPELINE_SELECT);
    if (MemorySynchronizationCommands<FamilyType>::isBarrierPriorToPipelineSelectWaRequired(pDevice->getRootDeviceEnvironment())) {
        expectedDifference += sizeof(PIPE_CONTROL);
    }

    EXPECT_EQ(expectedDifference, difference);
}

HWTEST_F(CommandStreamReceiverHwTest, givenPreambleSentWhenEstimatingFlushTaskSizeThenResultDependsOnAdditionalCmdsSize) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.isPreambleSent = false;
    auto preambleNotSentPreamble = commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice);
    auto preambleNotSentFlush = commandStreamReceiver.getRequiredCmdStreamSize(flushTaskFlags, *pDevice);

    commandStreamReceiver.isPreambleSent = true;
    auto preambleSentPreamble = commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice);
    auto preambleSentFlush = commandStreamReceiver.getRequiredCmdStreamSize(flushTaskFlags, *pDevice);

    auto actualDifferenceForPreamble = preambleNotSentPreamble - preambleSentPreamble;
    auto actualDifferenceForFlush = preambleNotSentFlush - preambleSentFlush;

    commandStreamReceiver.isPreambleSent = false;
    auto expectedDifferenceForPreamble = PreambleHelper<FamilyType>::getAdditionalCommandsSize(*pDevice);
    auto expectedDifferenceForFlush = expectedDifferenceForPreamble + commandStreamReceiver.getCmdSizeForL3Config() +
                                      PreambleHelper<FamilyType>::getCmdSizeForPipelineSelect(pDevice->getRootDeviceEnvironment());

    EXPECT_EQ(expectedDifferenceForPreamble, actualDifferenceForPreamble);
    EXPECT_EQ(expectedDifferenceForFlush, actualDifferenceForFlush);
}

HWTEST2_F(CommandStreamReceiverHwTest, givenStaticPartitionEnabledWhenOnlySinglePartitionUsedThenExpectSinglePipeControlAsBarrier, IsAtLeastXeCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    constexpr size_t cmdSize = 256;
    std::unique_ptr<char[]> buffer(new char[cmdSize]);
    LinearStream cs(buffer.get(), cmdSize);

    commandStreamReceiver.staticWorkPartitioningEnabled = true;
    commandStreamReceiver.activePartitions = 1;

    size_t estimatedCmdSize = commandStreamReceiver.getCmdSizeForStallingNoPostSyncCommands();
    EXPECT_EQ(sizeof(PIPE_CONTROL), estimatedCmdSize);

    commandStreamReceiver.programStallingNoPostSyncCommandsForBarrier(cs);
    EXPECT_EQ(estimatedCmdSize, cs.getUsed());

    PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(buffer.get());
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_TRUE(UnitTestHelper<FamilyType>::getPipeControlHdcPipelineFlush(*pipeControl));
    EXPECT_TRUE(pipeControl->getUnTypedDataPortCacheFlush());
}

HWTEST2_F(CommandStreamReceiverHwTest, givenStaticPartitionDisabledWhenMultiplePartitionsUsedThenExpectSinglePipeControlAsBarrier, IsAtLeastXeCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    constexpr size_t cmdSize = 256;
    std::unique_ptr<char[]> buffer(new char[cmdSize]);
    LinearStream cs(buffer.get(), cmdSize);

    commandStreamReceiver.staticWorkPartitioningEnabled = false;
    commandStreamReceiver.activePartitions = 2;

    size_t estimatedCmdSize = commandStreamReceiver.getCmdSizeForStallingNoPostSyncCommands();
    EXPECT_EQ(sizeof(PIPE_CONTROL), estimatedCmdSize);

    commandStreamReceiver.programStallingNoPostSyncCommandsForBarrier(cs);
    EXPECT_EQ(estimatedCmdSize, cs.getUsed());

    PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(buffer.get());
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_TRUE(UnitTestHelper<FamilyType>::getPipeControlHdcPipelineFlush(*pipeControl));
    EXPECT_TRUE(pipeControl->getUnTypedDataPortCacheFlush());
}

HWTEST2_F(CommandStreamReceiverHwTest, givenStaticPartitionEnabledWhenMultiplePartitionsUsedThenExpectImplicitScalingWithoutSelfCleanupBarrier, IsAtLeastXeCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    constexpr size_t cmdSize = 256;
    std::unique_ptr<char[]> buffer(new char[cmdSize]);
    MockGraphicsAllocation allocation(buffer.get(), cmdSize);
    allocation.gpuAddress = 0xFF000;
    LinearStream cs(buffer.get(), cmdSize);
    cs.replaceGraphicsAllocation(&allocation);

    commandStreamReceiver.staticWorkPartitioningEnabled = true;
    commandStreamReceiver.activePartitions = 2;

    size_t expectedSize = sizeof(PIPE_CONTROL) +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                          sizeof(MI_BATCH_BUFFER_START) +
                          2 * sizeof(uint32_t);
    size_t estimatedCmdSize = commandStreamReceiver.getCmdSizeForStallingNoPostSyncCommands();
    EXPECT_EQ(expectedSize, estimatedCmdSize);

    commandStreamReceiver.programStallingNoPostSyncCommandsForBarrier(cs);
    EXPECT_EQ(estimatedCmdSize, cs.getUsed());

    void *cmdBuffer = buffer.get();
    size_t offset = 0;

    PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(cmdBuffer);
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_TRUE(UnitTestHelper<FamilyType>::getPipeControlHdcPipelineFlush(*pipeControl));
    EXPECT_TRUE(pipeControl->getUnTypedDataPortCacheFlush());

    offset += sizeof(PIPE_CONTROL);

    MI_ATOMIC *miAtomic = genCmdCast<MI_ATOMIC *>(ptrOffset(cmdBuffer, offset));
    ASSERT_NE(nullptr, miAtomic);
    offset += sizeof(MI_ATOMIC);

    MI_SEMAPHORE_WAIT *miSemaphore = genCmdCast<MI_SEMAPHORE_WAIT *>(ptrOffset(cmdBuffer, offset));
    ASSERT_NE(nullptr, miSemaphore);
    offset += NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait();

    MI_BATCH_BUFFER_START *bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(ptrOffset(cmdBuffer, offset));
    ASSERT_NE(nullptr, bbStart);
    offset += sizeof(MI_BATCH_BUFFER_START);

    uint32_t *data = reinterpret_cast<uint32_t *>(ptrOffset(cmdBuffer, offset));
    EXPECT_EQ(0u, *data);
    offset += sizeof(uint32_t);

    data = reinterpret_cast<uint32_t *>(ptrOffset(cmdBuffer, offset));
    EXPECT_EQ(0u, *data);
    offset += sizeof(uint32_t);

    EXPECT_EQ(estimatedCmdSize, offset);
}

HWTEST2_F(CommandStreamReceiverHwTest, givenSingleTileWhenProgrammingPostSyncBarrierThenExpectPipeControlWithCorrectFlags, IsAtLeastXeCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.activePartitions = 1;

    size_t estimatedCmdSize = ultCsr.getCmdSizeForStallingPostSyncCommands();

    char commandBuffer[MemoryConstants::pageSize];
    LinearStream commandStream(commandBuffer, MemoryConstants::pageSize);
    TagNodeBase *tagNode = ultCsr.getTimestampPacketAllocator()->getTag();

    ultCsr.programStallingPostSyncCommandsForBarrier(commandStream, *tagNode, false);
    size_t sizeUsed = commandStream.getUsed();
    ASSERT_EQ(estimatedCmdSize, sizeUsed);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        commandStream.getCpuBase(),
        sizeUsed));
    auto pipeControlIteratorVector = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(1u, pipeControlIteratorVector.size());
    auto pipeControl = genCmdCast<PIPE_CONTROL *>(*pipeControlIteratorVector[0]);
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_TRUE(UnitTestHelper<FamilyType>::getPipeControlHdcPipelineFlush(*pipeControl));
    EXPECT_TRUE(pipeControl->getUnTypedDataPortCacheFlush());
}

HWTEST2_F(CommandStreamReceiverHwTest, givenImplicitScalingEnabledWhenProgrammingPostSyncBarrierThenExpectPipeControlWithCorrectFlags, IsAtLeastXeCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.activePartitions = 2;
    ultCsr.staticWorkPartitioningEnabled = true;

    size_t barrierWithPostSyncOperationSize = NEO::MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(pDevice->getRootDeviceEnvironment(), NEO::PostSyncMode::immediateData);
    size_t expectedSize = barrierWithPostSyncOperationSize +
                          sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                          sizeof(MI_BATCH_BUFFER_START) +
                          2 * sizeof(uint32_t);
    size_t estimatedCmdSize = ultCsr.getCmdSizeForStallingPostSyncCommands();
    EXPECT_EQ(expectedSize, estimatedCmdSize);

    char commandBuffer[MemoryConstants::pageSize];
    MockGraphicsAllocation mockCmdBufferAllocation(commandBuffer, 0x4000, MemoryConstants::pageSize);
    LinearStream commandStream(&mockCmdBufferAllocation);
    TagNodeBase *tagNode = ultCsr.getTimestampPacketAllocator()->getTag();

    ultCsr.programStallingPostSyncCommandsForBarrier(commandStream, *tagNode, false);

    EXPECT_EQ(estimatedCmdSize, commandStream.getUsed());

    void *cmdBuffer = reinterpret_cast<void *>(commandBuffer);
    size_t offset = 0;

    PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(cmdBuffer);
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_TRUE(UnitTestHelper<FamilyType>::getPipeControlHdcPipelineFlush(*pipeControl));
    EXPECT_TRUE(pipeControl->getUnTypedDataPortCacheFlush());

    offset += barrierWithPostSyncOperationSize;

    MI_ATOMIC *miAtomic = genCmdCast<MI_ATOMIC *>(ptrOffset(cmdBuffer, offset));
    ASSERT_NE(nullptr, miAtomic);
    offset += sizeof(MI_ATOMIC);

    MI_SEMAPHORE_WAIT *miSemaphore = genCmdCast<MI_SEMAPHORE_WAIT *>(ptrOffset(cmdBuffer, offset));
    ASSERT_NE(nullptr, miSemaphore);
    offset += NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait();

    MI_BATCH_BUFFER_START *bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(ptrOffset(cmdBuffer, offset));
    ASSERT_NE(nullptr, bbStart);
    offset += sizeof(MI_BATCH_BUFFER_START);

    uint32_t *data = reinterpret_cast<uint32_t *>(ptrOffset(cmdBuffer, offset));
    EXPECT_EQ(0u, *data);
    offset += sizeof(uint32_t);

    data = reinterpret_cast<uint32_t *>(ptrOffset(cmdBuffer, offset));
    EXPECT_EQ(0u, *data);
    offset += sizeof(uint32_t);

    EXPECT_EQ(estimatedCmdSize, offset);
}

HWTEST_F(CommandStreamReceiverHwTest, givenForcePipeControlPriorToWalkerWhenAddPipeControlFlushTaskIfNeededThenStallingPcIsProgrammed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ForcePipeControlPriorToWalker.set(1);

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    csr.addPipeControlFlushTaskIfNeeded(commandStream, 0);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands,
                                             commandStream.getCpuBase(),
                                             commandStream.getUsed());

    auto itorCmd = find<PIPE_CONTROL *>(commands.begin(), commands.end());
    ASSERT_NE(commands.end(), itorCmd);

    auto pc = genCmdCast<PIPE_CONTROL *>(*itorCmd);
    EXPECT_TRUE(pc->getCommandStreamerStallEnable());
}

HWTEST_F(CommandStreamReceiverTest, givenCsrWhenGetAcLineConnectedCalledThenCorrectValueIsReturned) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto mockKmdNotifyHelper = new MockKmdNotifyHelper(&pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties);
    csr.resetKmdNotifyHelper(mockKmdNotifyHelper);

    EXPECT_EQ(1u, mockKmdNotifyHelper->updateAcLineStatusCalled);

    csr.getAcLineConnected(false);
    EXPECT_EQ(1u, mockKmdNotifyHelper->updateAcLineStatusCalled);

    csr.getAcLineConnected(true);
    EXPECT_EQ(2u, mockKmdNotifyHelper->updateAcLineStatusCalled);

    mockKmdNotifyHelper->acLineConnected.store(false);
    EXPECT_FALSE(csr.getAcLineConnected(false));

    mockKmdNotifyHelper->acLineConnected.store(true);
    EXPECT_TRUE(csr.getAcLineConnected(false));
}

HWTEST_F(CommandStreamReceiverTest, givenBcsCsrWhenInitializeDeviceWithFirstSubmissionIsCalledThenSuccessIsReturned) {
    MockOsContext mockOsContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular}));
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(mockOsContext);
    commandStreamReceiver.initializeTagAllocation();

    EXPECT_EQ(SubmissionStatus::success, commandStreamReceiver.initializeDeviceWithFirstSubmission(*pDevice));
}

HWTEST_F(CommandStreamReceiverTest, givenCsrWhenInitializeDeviceWithFirstSubmissionIsCalledThenFlushOnlyForTheFirstTime) {
    MockOsContext mockOsContext(0, EngineDescriptorHelper::getDefaultDescriptor({defaultHwInfo->capabilityTable.defaultEngineType, EngineUsage::regular}));
    pDevice->setPreemptionMode(PreemptionMode::Disabled);

    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(mockOsContext);
    commandStreamReceiver.initializeTagAllocation();
    EXPECT_EQ(0u, commandStreamReceiver.taskCount);

    EXPECT_EQ(SubmissionStatus::success, commandStreamReceiver.initializeDeviceWithFirstSubmission(*pDevice));
    EXPECT_EQ(1u, commandStreamReceiver.taskCount);

    EXPECT_EQ(SubmissionStatus::success, commandStreamReceiver.initializeDeviceWithFirstSubmission(*pDevice));
    EXPECT_EQ(1u, commandStreamReceiver.taskCount);
    EXPECT_EQ(0u, commandStreamReceiver.waitForTaskCountWithKmdNotifyFallbackCalled);
}

HWTEST_F(CommandStreamReceiverTest, givenTbxCsrWhenInitializingThenWaitForCompletion) {
    MockOsContext mockOsContext(0, EngineDescriptorHelper::getDefaultDescriptor({defaultHwInfo->capabilityTable.defaultEngineType, EngineUsage::regular}));
    pDevice->setPreemptionMode(PreemptionMode::Disabled);

    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.commandStreamReceiverType = CommandStreamReceiverType::tbx;
    commandStreamReceiver.setupContext(mockOsContext);
    commandStreamReceiver.initializeTagAllocation();

    EXPECT_EQ(0u, commandStreamReceiver.taskCount);
    EXPECT_EQ(0u, commandStreamReceiver.waitForCompletionWithTimeoutTaskCountCalled);

    EXPECT_EQ(SubmissionStatus::success, commandStreamReceiver.initializeDeviceWithFirstSubmission(*pDevice));
    EXPECT_EQ(1u, commandStreamReceiver.taskCount);
    EXPECT_EQ(1u, commandStreamReceiver.waitForCompletionWithTimeoutTaskCountCalled);
    EXPECT_TRUE(commandStreamReceiver.latestWaitForCompletionWithTimeoutWaitParams.skipTbxDownload);

    EXPECT_EQ(SubmissionStatus::success, commandStreamReceiver.initializeDeviceWithFirstSubmission(*pDevice));
    EXPECT_EQ(1u, commandStreamReceiver.taskCount);
    EXPECT_EQ(1u, commandStreamReceiver.waitForCompletionWithTimeoutTaskCountCalled);

    MockCsrHw<FamilyType> failingCommandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    failingCommandStreamReceiver.commandStreamReceiverType = CommandStreamReceiverType::tbx;
    failingCommandStreamReceiver.setupContext(mockOsContext);
    failingCommandStreamReceiver.initializeTagAllocation();
    failingCommandStreamReceiver.flushReturnValue = SubmissionStatus::outOfMemory;

    EXPECT_EQ(SubmissionStatus::outOfMemory, failingCommandStreamReceiver.initializeDeviceWithFirstSubmission(*pDevice));
    EXPECT_EQ(0u, failingCommandStreamReceiver.waitForTaskCountWithKmdNotifyFallbackCalled);
}

HWTEST_F(CommandStreamReceiverTest, givenCsrWhenGetUmdPowerHintValueCalledThenCorrectValueIsReturned) {
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    EXPECT_EQ(nullptr, commandStreamReceiver.osContext);
    EXPECT_EQ(0u, commandStreamReceiver.getUmdPowerHintValue());

    MockOsContext mockOsContext(0, EngineDescriptorHelper::getDefaultDescriptor({defaultHwInfo->capabilityTable.defaultEngineType, EngineUsage::regular}));
    commandStreamReceiver.setupContext(mockOsContext);
    mockOsContext.setUmdPowerHintValue(1u);
    EXPECT_EQ(1u, commandStreamReceiver.getUmdPowerHintValue());
}

HWTEST_F(CommandStreamReceiverTest, givenCsrWhenMakeResidentCalledThenUpdateTaskCountIfObjectIsAlwaysResident) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto contextId = csr.getOsContext().getContextId();
    MockGraphicsAllocation graphicsAllocation;

    csr.makeResident(graphicsAllocation);
    auto initialAllocTaskCount = graphicsAllocation.getTaskCount(contextId);
    EXPECT_EQ(initialAllocTaskCount, csr.peekTaskCount() + 1);

    graphicsAllocation.updateResidencyTaskCount(GraphicsAllocation::objectAlwaysResident, contextId);
    csr.taskCount = 10;

    csr.makeResident(graphicsAllocation);
    auto updatedTaskCount = graphicsAllocation.getTaskCount(contextId);
    EXPECT_EQ(updatedTaskCount, csr.peekTaskCount() + 1);
    EXPECT_NE(updatedTaskCount, initialAllocTaskCount);
}

using CommandStreamReceiverHwHeaplessTest = Test<DeviceFixture>;

HWTEST_F(CommandStreamReceiverHwHeaplessTest, whenHeaplessCommandStreamReceiverFunctionsAreCalledThenExceptionIsThrown) {
    std::unique_ptr<UltCommandStreamReceiver<FamilyType>> csr = std::make_unique<UltCommandStreamReceiver<FamilyType>>(*pDevice->executionEnvironment, rootDeviceIndex, pDevice->getDeviceBitfield());

    LinearStream commandStream(0, 0);

    EXPECT_ANY_THROW(csr->flushTaskHeapless(commandStream, 0, nullptr, nullptr, nullptr, 0, csr->recordedDispatchFlags, *pDevice));
    EXPECT_ANY_THROW(csr->programHeaplessProlog(*pDevice));
    EXPECT_ANY_THROW(csr->programStateBaseAddressHeapless(*pDevice, commandStream));
    EXPECT_ANY_THROW(csr->programComputeModeHeapless(*pDevice, commandStream));
    EXPECT_ANY_THROW(csr->getCmdSizeForHeaplessPrologue(*pDevice));
    EXPECT_ANY_THROW(csr->handleAllocationsResidencyForHeaplessProlog(commandStream, *pDevice));
    EXPECT_ANY_THROW(csr->programHeaplessStateProlog(*pDevice, commandStream));
    EXPECT_ANY_THROW(csr->handleAllocationsResidencyForflushTaskStateless(nullptr, nullptr, nullptr, *pDevice));
    EXPECT_ANY_THROW(csr->getRequiredCmdStreamHeaplessSize(csr->recordedDispatchFlags, *pDevice));
    EXPECT_ANY_THROW(csr->getRequiredCmdStreamHeaplessSizeAligned(csr->recordedDispatchFlags, *pDevice));
    EXPECT_ANY_THROW(csr->flushImmediateTaskStateless(commandStream, 0, csr->recordedImmediateDispatchFlags, *pDevice));
    EXPECT_ANY_THROW(csr->handleImmediateFlushStatelessAllocationsResidency(0, commandStream, *pDevice));

    EXPECT_FALSE(csr->heaplessStateInitialized);
}

HWTEST2_F(CommandStreamReceiverHwTest,
          givenImmediateFlushTaskInHeaplessModeWhenNextDispatchRequiresScratchSpaceThenNoScratchIsAllocated,
          IsHeapfulSupportedAndAtLeastXeCore) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;
    commandStreamReceiver.heaplessModeEnabled = true;

    commandStreamReceiver.flushImmediateTask(commandStream, commandStream.getUsed(), immediateFlushTaskFlags, *pDevice);

    commandStreamReceiver.setRequiredScratchSizes(0x100, 0);

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedSize);
    auto frontEndCmd = hwParserCsr.getCommand<CFE_STATE>();
    ASSERT_EQ(nullptr, frontEndCmd);

    EXPECT_EQ(nullptr, commandStreamReceiver.getScratchSpaceController()->getScratchSpaceSlot0Allocation());
}

using CommandStreamReceiverContextGroupTest = ::testing::Test;

HWTEST_F(CommandStreamReceiverContextGroupTest, givenSecondaryCsrWhenGettingInternalAllocationsThenAllocationFromPrimnaryCsrAreReturned) {

    HardwareInfo hwInfo = *defaultHwInfo;
    if (hwInfo.capabilityTable.defaultEngineType != aub_stream::EngineType::ENGINE_CCS) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ContextGroupSize.set(5);

    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
    hwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const auto &gfxCoreHelper = device->getRootDeviceEnvironment().getHelper<GfxCoreHelper>();

    const auto ccsIndex = 0;
    auto &secondaryEngines = device->secondaryEngines[EngineHelpers::mapCcsIndexToEngineType(ccsIndex)];
    auto secondaryEnginesCount = secondaryEngines.engines.size();
    ASSERT_EQ(5u, secondaryEnginesCount);

    EXPECT_TRUE(secondaryEngines.engines[0].commandStreamReceiver->isInitialized());

    auto primaryCsr = secondaryEngines.engines[0].commandStreamReceiver;
    primaryCsr->createGlobalStatelessHeap();

    for (uint32_t secondaryIndex = 1; secondaryIndex < secondaryEnginesCount; secondaryIndex++) {
        device->getSecondaryEngineCsr({EngineHelpers::mapCcsIndexToEngineType(ccsIndex), EngineUsage::regular}, 0, false);
    }

    for (uint32_t i = 0; i < secondaryEngines.highPriorityEnginesTotal; i++) {
        device->getSecondaryEngineCsr({EngineHelpers::mapCcsIndexToEngineType(ccsIndex), EngineUsage::highPriority}, 0, false);
    }

    for (uint32_t secondaryIndex = 0; secondaryIndex < secondaryEnginesCount; secondaryIndex++) {

        if (secondaryIndex > 0) {
            EXPECT_NE(primaryCsr->getTagAllocation(), secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getTagAllocation());
        }

        if (gfxCoreHelper.isFenceAllocationRequired(hwInfo, device->getRootDeviceEnvironment().getHelper<ProductHelper>())) {
            EXPECT_EQ(primaryCsr->getGlobalFenceAllocation(), secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getGlobalFenceAllocation());
        }

        EXPECT_EQ(primaryCsr->getPreemptionAllocation(), secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getPreemptionAllocation());
        EXPECT_EQ(primaryCsr->getGlobalStatelessHeapAllocation(), secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getGlobalStatelessHeapAllocation());
        EXPECT_EQ(primaryCsr->getGlobalStatelessHeap(), secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getGlobalStatelessHeap());
        EXPECT_EQ(primaryCsr->getPrimaryScratchSpaceController(), secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getPrimaryScratchSpaceController());
    }
}

HWTEST_F(CommandStreamReceiverContextGroupTest, givenSecondaryRootCsrWhenGettingInternalAllocationsThenAllocationFromPrimnaryCsrAreReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    if (hwInfo.capabilityTable.defaultEngineType != aub_stream::EngineType::ENGINE_CCS) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ContextGroupSize.set(5);

    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
    hwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;

    UltDeviceFactory deviceFactory{1, 2};
    auto device = deviceFactory.rootDevices[0];
    const auto &gfxCoreHelper = device->getRootDeviceEnvironment().getHelper<GfxCoreHelper>();

    const auto ccsIndex = 0;
    auto &secondaryEngines = device->secondaryEngines[EngineHelpers::mapCcsIndexToEngineType(ccsIndex)];
    auto secondaryEnginesCount = secondaryEngines.engines.size();
    ASSERT_EQ(5u, secondaryEnginesCount);

    EXPECT_TRUE(secondaryEngines.engines[0].commandStreamReceiver->isInitialized());

    auto primaryCsr = secondaryEngines.engines[0].commandStreamReceiver;
    primaryCsr->createGlobalStatelessHeap();

    for (uint32_t secondaryIndex = 1; secondaryIndex < secondaryEnginesCount; secondaryIndex++) {
        device->getSecondaryEngineCsr({EngineHelpers::mapCcsIndexToEngineType(ccsIndex), EngineUsage::regular}, 0, false);
    }

    for (uint32_t i = 0; i < secondaryEngines.highPriorityEnginesTotal; i++) {
        device->getSecondaryEngineCsr({EngineHelpers::mapCcsIndexToEngineType(ccsIndex), EngineUsage::highPriority}, 0, false);
    }

    for (uint32_t secondaryIndex = 0; secondaryIndex < secondaryEnginesCount; secondaryIndex++) {

        if (secondaryIndex > 0) {
            EXPECT_NE(primaryCsr->getTagAllocation(), secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getTagAllocation());
        }

        if (gfxCoreHelper.isFenceAllocationRequired(hwInfo, device->getRootDeviceEnvironment().getHelper<ProductHelper>())) {
            EXPECT_EQ(primaryCsr->getGlobalFenceAllocation(), secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getGlobalFenceAllocation());
        }

        EXPECT_EQ(primaryCsr->getPreemptionAllocation(), secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getPreemptionAllocation());
        EXPECT_EQ(primaryCsr->getGlobalStatelessHeapAllocation(), secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getGlobalStatelessHeapAllocation());
        EXPECT_EQ(primaryCsr->getGlobalStatelessHeap(), secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getGlobalStatelessHeap());
        EXPECT_EQ(primaryCsr->getPrimaryScratchSpaceController(), secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getPrimaryScratchSpaceController());
        EXPECT_EQ(primaryCsr->getWorkPartitionAllocation(), secondaryEngines.engines[secondaryIndex].commandStreamReceiver->getWorkPartitionAllocation());
    }
}

HWTEST_F(CommandStreamReceiverContextGroupTest, givenContextGroupWhenCreatingEnginesThenSetCorrectMaxOsContextCount) {

    HardwareInfo hwInfo = *defaultHwInfo;
    if (hwInfo.capabilityTable.defaultEngineType != aub_stream::EngineType::ENGINE_CCS) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ContextGroupSize.set(5);

    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 0b110;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
    hwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));

    device->getExecutionEnvironment()->calculateMaxOsContextCount();

    auto &engineInstances = device->getGfxCoreHelper().getGpgpuEngineInstances(*device->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    uint32_t numRegularEngines = 0;
    uint32_t numHpEngines = 0;

    for (const auto &engine : engineInstances) {
        if (engine.second == EngineUsage::regular) {
            numRegularEngines++;
        }
        if (engine.second == EngineUsage::highPriority) {
            numHpEngines++;
        }
    }

    auto nonGroupCount = static_cast<uint32_t>(engineInstances.size()) - numRegularEngines - numHpEngines;

    auto osContextCount = nonGroupCount +
                          (numRegularEngines * device->getGfxCoreHelper().getContextGroupContextsCount()) +
                          (numHpEngines * device->getGfxCoreHelper().getContextGroupContextsCount());

    EXPECT_EQ(osContextCount, MemoryManager::maxOsContextCount);
}
HWTEST_F(CommandStreamReceiverContextGroupTest, givenMultipleSubDevicesAndContextGroupWhenCreatingEnginesThenSetCorrectMaxOsContextCount) {

    HardwareInfo hwInfo = *defaultHwInfo;
    if (hwInfo.capabilityTable.defaultEngineType != aub_stream::EngineType::ENGINE_CCS) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ContextGroupSize.set(8);
    debugManager.flags.CreateMultipleSubDevices.set(2);

    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 0b110;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
    hwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));

    device->getExecutionEnvironment()->calculateMaxOsContextCount();

    auto &engineInstances = device->getGfxCoreHelper().getGpgpuEngineInstances(*device->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    uint32_t numRegularEngines = 0;
    uint32_t numHpEngines = 0;

    for (const auto &engine : engineInstances) {
        if (engine.second == EngineUsage::regular) {
            numRegularEngines++;
        }
        if (engine.second == EngineUsage::highPriority) {
            numHpEngines++;
        }
    }

    auto nonGroupCount = static_cast<uint32_t>(engineInstances.size()) - numRegularEngines - numHpEngines;

    auto osContextCount = nonGroupCount +
                          (numRegularEngines * device->getGfxCoreHelper().getContextGroupContextsCount()) +
                          (numHpEngines * device->getGfxCoreHelper().getContextGroupContextsCount());

    osContextCount = osContextCount * 2 + 1 * device->getGfxCoreHelper().getContextGroupContextsCount();

    EXPECT_EQ(osContextCount, MemoryManager::maxOsContextCount);
}

HWTEST_F(CommandStreamReceiverContextGroupTest, givenSecondaryCsrsWhenSameResourcesAreUsedThenResidencyIsProperlyHandled) {

    HardwareInfo hwInfo = *defaultHwInfo;
    if (hwInfo.capabilityTable.defaultEngineType != aub_stream::EngineType::ENGINE_CCS) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ContextGroupSize.set(5);

    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));

    const auto ccsIndex = 0;
    auto &commandStreamReceiver0 = *device->getSecondaryEngineCsr({EngineHelpers::mapCcsIndexToEngineType(ccsIndex), EngineUsage::regular}, 0, false)->commandStreamReceiver;
    auto &commandStreamReceiver1 = *device->getSecondaryEngineCsr({EngineHelpers::mapCcsIndexToEngineType(ccsIndex), EngineUsage::regular}, 0, false)->commandStreamReceiver;

    auto csr0ContextId = commandStreamReceiver0.getOsContext().getContextId();
    auto csr1ContextId = commandStreamReceiver1.getOsContext().getContextId();

    auto initialTaskCount0 = commandStreamReceiver0.peekTaskCount();
    auto initialTaskCount1 = commandStreamReceiver1.peekTaskCount();
    MockGraphicsAllocation graphicsAllocation;

    commandStreamReceiver0.makeResident(graphicsAllocation);
    EXPECT_EQ(1u, commandStreamReceiver0.getResidencyAllocations().size());
    EXPECT_EQ(0u, commandStreamReceiver1.getResidencyAllocations().size());

    commandStreamReceiver1.makeResident(graphicsAllocation);
    EXPECT_EQ(1u, commandStreamReceiver0.getResidencyAllocations().size());
    EXPECT_EQ(1u, commandStreamReceiver1.getResidencyAllocations().size());

    EXPECT_EQ(initialTaskCount0 + 1u, graphicsAllocation.getResidencyTaskCount(csr0ContextId));
    EXPECT_EQ(initialTaskCount1 + 1u, graphicsAllocation.getResidencyTaskCount(csr1ContextId));

    commandStreamReceiver0.makeNonResident(graphicsAllocation);
    EXPECT_FALSE(graphicsAllocation.isResident(csr0ContextId));
    EXPECT_TRUE(graphicsAllocation.isResident(csr1ContextId));

    commandStreamReceiver1.makeNonResident(graphicsAllocation);
    EXPECT_FALSE(graphicsAllocation.isResident(csr0ContextId));
    EXPECT_FALSE(graphicsAllocation.isResident(csr1ContextId));

    EXPECT_EQ(1u, commandStreamReceiver0.getEvictionAllocations().size());
    EXPECT_EQ(1u, commandStreamReceiver1.getEvictionAllocations().size());
}

HWTEST_F(CommandStreamReceiverTest, givenCommandStreamReceiverWhenEnqueueWaitForPagingFenceCalledThenEnqueueIfPossibleAndReturnCorrectValue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDirectSubmissionController.set(1);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto executionEnvironment = pDevice->getExecutionEnvironment();
    auto pagingFenceValue = 10u;
    EXPECT_FALSE(csr.enqueueWaitForPagingFence(pagingFenceValue));

    VariableBackup<decltype(NEO::Thread::createFunc)> funcBackup{&NEO::Thread::createFunc, [](void *(*func)(void *), void *arg) -> std::unique_ptr<Thread> { return nullptr; }};
    // enqueue waitForPagingFence is possible only if controller exists and direct submission is enabled
    auto controller = static_cast<DirectSubmissionControllerMock *>(executionEnvironment->initializeDirectSubmissionController());
    controller->stopThread();
    EXPECT_FALSE(csr.enqueueWaitForPagingFence(pagingFenceValue));

    csr.directSubmissionAvailable = true;
    EXPECT_TRUE(csr.enqueueWaitForPagingFence(pagingFenceValue));

    EXPECT_EQ(0u, csr.pagingFenceValueToUnblock);
    std::mutex mtx;
    std::unique_lock<std::mutex> lock(mtx);
    csr.directSubmissionAvailable = false;
    controller->handlePagingFenceRequests(lock, false);
    EXPECT_EQ(10u, csr.pagingFenceValueToUnblock);
}

HWTEST_F(CommandStreamReceiverTest, givenCommandStreamReceiverWhenDrainPagingFenceQueueThenQueueDrained) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDirectSubmissionController.set(1);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(csr);
    csr.directSubmission.reset(directSubmission);

    auto executionEnvironment = pDevice->getExecutionEnvironment();
    auto pagingFenceValue = 10u;
    EXPECT_FALSE(csr.enqueueWaitForPagingFence(pagingFenceValue));

    VariableBackup<decltype(NEO::Thread::createFunc)> funcBackup{&NEO::Thread::createFunc, [](void *(*func)(void *), void *arg) -> std::unique_ptr<Thread> { return nullptr; }};

    csr.drainPagingFenceQueue();
    EXPECT_EQ(0u, csr.pagingFenceValueToUnblock);

    auto controller = static_cast<DirectSubmissionControllerMock *>(executionEnvironment->initializeDirectSubmissionController());
    controller->stopThread();
    csr.directSubmissionAvailable = true;
    EXPECT_TRUE(csr.enqueueWaitForPagingFence(pagingFenceValue));
    EXPECT_EQ(0u, csr.pagingFenceValueToUnblock);

    std::mutex mtx;
    std::unique_lock<std::mutex> lock(mtx);
    csr.directSubmissionAvailable = false;
    csr.drainPagingFenceQueue();
    EXPECT_EQ(0u, csr.pagingFenceValueToUnblock);

    csr.directSubmissionAvailable = true;
    csr.drainPagingFenceQueue();
    EXPECT_EQ(10u, csr.pagingFenceValueToUnblock);
}

HWTEST_F(CommandStreamReceiverHwTest, givenRequiredFlushTaskCountWhenFlushBcsTaskCalledThenSetStallingCommandFlag) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    DispatchBcsFlags dispatchBcsFlags(false, false, false);

    // first flush can carry preamble, no interest in flags here
    commandStreamReceiver.flushBcsTask(commandStream,
                                       commandStream.getUsed(),
                                       dispatchBcsFlags,
                                       pDevice->getHardwareInfo());

    // regular dispatch here
    commandStreamReceiver.recordFlushedBatchBuffer = true;
    dispatchBcsFlags.flushTaskCount = true;

    commandStreamReceiver.flushBcsTask(commandStream,
                                       commandStream.getUsed(),
                                       dispatchBcsFlags,
                                       pDevice->getHardwareInfo());

    EXPECT_TRUE(commandStreamReceiver.latestFlushedBatchBuffer.hasStallingCmds);
}

HWTEST_F(CommandStreamReceiverHwTest, givenEpilogueStreamAvailableWhenFlushBcsTaskCalledThenDispachEpilogueCommandsIntoEpilogueStream) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    DispatchBcsFlags dispatchBcsFlags(false, false, false);

    // first flush can carry preamble, no interest in flags here
    commandStreamReceiver.flushBcsTask(commandStream,
                                       commandStream.getUsed(),
                                       dispatchBcsFlags,
                                       pDevice->getHardwareInfo());

    // regular dispatch here
    GraphicsAllocation *commandBuffer = commandStreamReceiver.getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{commandStreamReceiver.getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream epilogueStream(commandBuffer);

    commandStreamReceiver.storeMakeResidentAllocations = true;
    dispatchBcsFlags.flushTaskCount = true;
    dispatchBcsFlags.optionalEpilogueCmdStream = &epilogueStream;

    commandStreamReceiver.flushBcsTask(commandStream,
                                       commandStream.getUsed(),
                                       dispatchBcsFlags,
                                       pDevice->getHardwareInfo());

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(commandBuffer));

    HardwareParse hwParser;

    hwParser.parseCommands<FamilyType>(epilogueStream, 0);
    auto cmdIterator = find<typename FamilyType::MI_FLUSH_DW *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_NE(hwParser.cmdList.end(), cmdIterator);

    commandStreamReceiver.getMemoryManager()->freeGraphicsMemoryImpl(commandBuffer);
}

HWTEST_F(CommandStreamReceiverHwTest, givenEpilogueStreamAvailableWhenFlushImmediateTaskCalledThenDispachEpilogueCommandsIntoEpilogueStream) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    // first flush can carry preamble, no interest in flags here
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    // regular dispatch here
    GraphicsAllocation *commandBuffer = commandStreamReceiver.getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{commandStreamReceiver.getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream epilogueStream(commandBuffer);

    commandStreamReceiver.storeMakeResidentAllocations = true;
    commandStreamReceiver.recordFlushedBatchBuffer = true;

    immediateFlushTaskFlags.requireTaskCountUpdate = true;
    immediateFlushTaskFlags.optionalEpilogueCmdStream = &epilogueStream;

    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(commandBuffer));
    EXPECT_TRUE(commandStreamReceiver.latestFlushedBatchBuffer.dispatchMonitorFence);

    HardwareParse hwParser;

    hwParser.parseCommands<FamilyType>(epilogueStream, 0);
    auto cmdIterator = find<typename FamilyType::PIPE_CONTROL *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_NE(hwParser.cmdList.end(), cmdIterator);

    commandStreamReceiver.getMemoryManager()->freeGraphicsMemoryImpl(commandBuffer);
}

HWTEST_F(CommandStreamReceiverHwTest, givenFlushBcsTaskCmdListDispatchWhenCalledThenNoPrologueIsSent) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    DispatchBcsFlags dispatchBcsFlags(false, false, false);
    dispatchBcsFlags.dispatchOperation = NEO::AppendOperations::cmdList;

    size_t usedSize = commandStreamReceiver.commandStream.getUsed();

    commandStreamReceiver.recordFlushedBatchBuffer = true;
    commandStreamReceiver.flushBcsTask(commandStream,
                                       commandStream.getUsed(),
                                       dispatchBcsFlags,
                                       pDevice->getHardwareInfo());

    EXPECT_EQ(usedSize, commandStreamReceiver.commandStream.getUsed());
    EXPECT_TRUE(commandStreamReceiver.latestFlushedBatchBuffer.disableFlatRingBuffer);
}

HWTEST_F(CommandStreamReceiverHwTest, givenImmediateFlushTaskCmdListDispatchWhenFlushingBufferThenDisableFlatRingBuffer) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.recordFlushedBatchBuffer = true;

    immediateFlushTaskFlags.dispatchOperation = NEO::AppendOperations::cmdList;
    commandStreamReceiver.flushImmediateTask(commandStream,
                                             commandStream.getUsed(),
                                             immediateFlushTaskFlags,
                                             *pDevice);

    EXPECT_TRUE(commandStreamReceiver.latestFlushedBatchBuffer.disableFlatRingBuffer);
}
