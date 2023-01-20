/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/command_stream_receiver_simulated_hw.h"
#include "shared/source/command_stream/scratch_space_controller_base.h"
#include "shared/source/command_stream/tag_allocation_layout.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/page_table_mngr.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/surface.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/common/fixtures/command_stream_receiver_fixture.inl"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/batch_buffer_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_internal_allocation_storage.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test_checks_shared.h"
#include "shared/test/unit_test/direct_submission/direct_submission_controller_mock.h"

#include "gtest/gtest.h"

#include <chrono>
#include <functional>
#include <limits>

namespace NEO {
extern ApiSpecificConfig::ApiType apiTypeForUlts;
} // namespace NEO
using namespace NEO;
using namespace std::chrono_literals;

struct CommandStreamReceiverTest : public DeviceFixture,
                                   public ::testing::Test {
    void SetUp() override {
        DeviceFixture::setUp();

        commandStreamReceiver = &pDevice->getGpgpuCommandStreamReceiver();
        ASSERT_NE(nullptr, commandStreamReceiver);
        memoryManager = commandStreamReceiver->getMemoryManager();
        internalAllocationStorage = commandStreamReceiver->getInternalAllocationStorage();
    }

    void TearDown() override {
        DeviceFixture::tearDown();
    }

    CommandStreamReceiver *commandStreamReceiver = nullptr;
    MemoryManager *memoryManager = nullptr;
    InternalAllocationStorage *internalAllocationStorage = nullptr;
};

TEST_F(CommandStreamReceiverTest, givenOsAgnosticCsrWhenGettingCompletionValueThenProperTaskCountIsReturned) {
    MockGraphicsAllocation allocation{};
    uint32_t expectedValue = 0x1234;

    auto &osContext = commandStreamReceiver->getOsContext();
    allocation.updateTaskCount(expectedValue, osContext.getContextId());
    EXPECT_EQ(expectedValue, commandStreamReceiver->getCompletionValue(allocation));
}

TEST_F(CommandStreamReceiverTest, givenCsrWhenGettingCompletionAddressThenProperAddressIsReturned) {
    auto expectedAddress = castToUint64(const_cast<TagAddressType *>(commandStreamReceiver->getTagAddress()));
    EXPECT_EQ(expectedAddress + TagAllocationLayout::completionFenceOffset, commandStreamReceiver->getCompletionAddress());
}

TEST_F(CommandStreamReceiverTest, givenCsrWhenGettingCompletionAddressThenUnderlyingMemoryIsZeroed) {
    auto completionFence = reinterpret_cast<TaskCountType *>(commandStreamReceiver->getCompletionAddress());
    EXPECT_EQ(0u, *completionFence);
}

HWTEST_F(CommandStreamReceiverTest, WhenCreatingCsrThenDefaultValuesAreSet) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(0u, csr.peekTaskLevel());
    EXPECT_EQ(0u, csr.peekTaskCount());
    EXPECT_FALSE(csr.isPreambleSent);
}

HWTEST_F(CommandStreamReceiverTest, WhenInitializeResourcesThenCallFillReusableAllocationsListOnce) {
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.fillReusableAllocationsListCalled = 0u;
    ultCsr.resourcesInitialized = false;

    commandStreamReceiver->initializeResources();
    EXPECT_EQ(1u, pDevice->getUltCommandStreamReceiver<FamilyType>().fillReusableAllocationsListCalled);
    commandStreamReceiver->initializeResources();
    EXPECT_EQ(1u, pDevice->getUltCommandStreamReceiver<FamilyType>().fillReusableAllocationsListCalled);
}

HWTEST_F(CommandStreamReceiverTest, givenCsrWhenCallFillReusableAllocationsListThenAllocateCommandBufferAndMakeItResident) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.SetAmountOfReusableAllocations.set(1);
    pDevice->getUltCommandStreamReceiver<FamilyType>().callBaseFillReusableAllocationsList = true;
    EXPECT_TRUE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
    EXPECT_EQ(0u, commandStreamReceiver->getResidencyAllocations().size());

    commandStreamReceiver->fillReusableAllocationsList();

    EXPECT_FALSE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
    EXPECT_EQ(1u, commandStreamReceiver->getResidencyAllocations().size());
}

HWTEST_F(CommandStreamReceiverTest, givenFlagDisabledWhenCallFillReusableAllocationsListThenAllocateCommandBufferAndMakeItResident) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.SetAmountOfReusableAllocations.set(0);
    pDevice->getUltCommandStreamReceiver<FamilyType>().callBaseFillReusableAllocationsList = true;
    EXPECT_TRUE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
    EXPECT_EQ(0u, commandStreamReceiver->getResidencyAllocations().size());

    commandStreamReceiver->fillReusableAllocationsList();

    EXPECT_TRUE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
    EXPECT_EQ(0u, commandStreamReceiver->getResidencyAllocations().size());
}

HWTEST_F(CommandStreamReceiverTest, whenRegisterClientThenIncrementClientNum) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto numClients = csr.getNumClients();

    csr.registerClient();
    EXPECT_EQ(csr.getNumClients(), numClients + 1);

    csr.registerClient();
    EXPECT_EQ(csr.getNumClients(), numClients + 2);

    csr.unregisterClient();
    EXPECT_EQ(csr.getNumClients(), numClients + 1);

    csr.unregisterClient();
    EXPECT_EQ(csr.getNumClients(), numClients);
}

HWTEST_F(CommandStreamReceiverTest, WhenCreatingCsrThenTimestampTypeIs32b) {
    using ExpectedType = TimestampPackets<typename FamilyType::TimestampPacketType>;

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
    EXPECT_FALSE(csr.GSBAFor32BitProgrammed);
    EXPECT_TRUE(csr.mediaVfeStateDirty);
    EXPECT_FALSE(csr.lastVmeSubslicesConfig);
    EXPECT_EQ(0u, csr.lastSentL3Config);
    EXPECT_EQ(-1, csr.lastMediaSamplerConfig);
    EXPECT_EQ(PreemptionMode::Initial, csr.lastPreemptionMode);
    EXPECT_EQ(0u, csr.latestSentStatelessMocsConfig);
    EXPECT_FALSE(csr.lastSentUseGlobalAtomics);
}

TEST_F(CommandStreamReceiverTest, givenBaseDownloadAllocationCalledThenDoesNotChangeAnything) {
    auto *memoryManager = commandStreamReceiver->getMemoryManager();

    GraphicsAllocation *graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{commandStreamReceiver->getRootDeviceIndex(), MemoryConstants::pageSize});

    ASSERT_NE(nullptr, graphicsAllocation);
    auto numEvictionAllocsBefore = commandStreamReceiver->getEvictionAllocations().size();
    commandStreamReceiver->CommandStreamReceiver::downloadAllocations();
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

    EXPECT_EQ(AllocationType::COMMAND_BUFFER, commandStreamAllocation->getAllocationType());
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
    DebugManager.flags.DisableGpuHangDetection.set(true);

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

    volatile TagAddressType tasksCount[16] = {};
    csr.tagAddress = tasksCount;

    constexpr auto enableTimeout = false;
    constexpr auto timeoutMicroseconds = std::numeric_limits<std::int64_t>::max();
    constexpr auto taskCountToWait = 1;

    const auto waitStatus = csr.waitForCompletionWithTimeout(enableTimeout, timeoutMicroseconds, taskCountToWait);
    EXPECT_EQ(WaitStatus::GpuHang, waitStatus);
}

HWTEST_F(CommandStreamReceiverTest, givenNoGpuHangWhenWaititingForCompletionWithTimeoutThenReadyIsReturned) {
    auto driverModelMock = std::make_unique<MockDriverModel>();
    driverModelMock->isGpuHangDetectedToReturn = false;

    volatile TagAddressType tasksCount[16] = {};
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
    EXPECT_EQ(WaitStatus::Ready, waitStatus);
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
    EXPECT_EQ(WaitStatus::GpuHang, waitStatus);
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
    EXPECT_EQ(WaitStatus::NotReady, waitStatus);
}

HWTEST_F(CommandStreamReceiverTest, givenGpuHangWhenWaititingForTaskCountThenGpuHangIsReturned) {
    auto driverModelMock = std::make_unique<MockDriverModel>();
    driverModelMock->isGpuHangDetectedToReturn = true;

    auto osInterface = std::make_unique<OSInterface>();
    osInterface->setDriverModel(std::move(driverModelMock));

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.executionEnvironment.rootDeviceEnvironments[csr.rootDeviceIndex]->osInterface = std::move(osInterface);
    csr.activePartitions = 1;
    csr.gpuHangCheckPeriod = 0us;

    volatile TagAddressType tasksCount[16] = {};
    csr.tagAddress = tasksCount;

    constexpr auto taskCountToWait = 1;
    const auto waitStatus = csr.waitForTaskCount(taskCountToWait);
    EXPECT_EQ(WaitStatus::GpuHang, waitStatus);
    EXPECT_TRUE(csr.downloadAllocationCalled);
}

HWTEST_F(CommandStreamReceiverTest, whenDownloadTagAllocationThenDonwloadOnlyIfTagAllocationWasFlushed) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.activePartitions = 1;
    *csr.tagAddress = 1u;

    auto ret = csr.testTaskCountReady(csr.tagAddress, 0u);
    EXPECT_TRUE(ret);
    EXPECT_FALSE(csr.downloadAllocationCalled);

    constexpr auto taskCountToWait = 1;
    ret = csr.testTaskCountReady(csr.tagAddress, taskCountToWait);
    EXPECT_TRUE(ret);
    EXPECT_FALSE(csr.downloadAllocationCalled);

    csr.getTagAllocation()->updateTaskCount(taskCountToWait, csr.osContext->getContextId());
    ret = csr.testTaskCountReady(csr.tagAddress, taskCountToWait);
    EXPECT_TRUE(ret);
    EXPECT_FALSE(csr.downloadAllocationCalled);

    csr.setLatestFlushedTaskCount(taskCountToWait);
    ret = csr.testTaskCountReady(csr.tagAddress, taskCountToWait);
    EXPECT_TRUE(ret);
    EXPECT_TRUE(csr.downloadAllocationCalled);
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

    volatile TagAddressType tasksCount[16] = {};
    VariableBackup<volatile TagAddressType *> csrTagAddressBackup(&csr.tagAddress);
    csr.tagAddress = tasksCount;

    auto hostPtr = reinterpret_cast<void *>(0x1234);
    size_t size = 100;
    auto gmmHelper = pDevice->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(hostPtr));
    auto temporaryAllocation = std::make_unique<MemoryAllocation>(0, AllocationType::EXTERNAL_HOST_PTR, hostPtr, size, 0,
                                                                  MemoryPool::System4KBPages, MemoryManager::maxOsContextCount, canonizedGpuAddress);
    temporaryAllocation->updateTaskCount(0u, 0u);
    csr.getInternalAllocationStorage()->storeAllocationWithTaskCount(std::move(temporaryAllocation), TEMPORARY_ALLOCATION, 2u);

    constexpr auto taskCountToWait = 1;
    constexpr auto allocationUsage = TEMPORARY_ALLOCATION;
    const auto waitStatus = csr.waitForTaskCountAndCleanAllocationList(taskCountToWait, allocationUsage);

    EXPECT_EQ(WaitStatus::GpuHang, waitStatus);
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
    EXPECT_EQ(AllocationType::DEBUG_CONTEXT_SAVE_AREA, allocation->getAllocationType());
}

TEST_F(CommandStreamReceiverTest, givenForced32BitAddressingWhenDebugSurfaceIsAllocatedThenRegularAllocationIsReturned) {
    auto *memoryManager = commandStreamReceiver->getMemoryManager();
    memoryManager->setForce32BitAllocations(true);
    auto allocation = commandStreamReceiver->allocateDebugSurface(1024);
    EXPECT_FALSE(allocation->is32BitAllocation());
}

HWTEST_F(CommandStreamReceiverTest, givenDefaultCommandStreamReceiverThenDefaultDispatchingPolicyIsImmediateSubmission) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(DispatchMode::ImmediateDispatch, csr.dispatchMode);
}

HWTEST_F(CommandStreamReceiverTest, givenL0CommandStreamReceiverThenDefaultDispatchingPolicyIsImmediateSubmission) {
    VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::L0);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(DispatchMode::ImmediateDispatch, csr.dispatchMode);
}

HWTEST_F(CommandStreamReceiverTest, givenCsrWhenGetIndirectHeapIsCalledThenHeapIsReturned) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &heap = csr.getIndirectHeap(IndirectHeap::Type::DYNAMIC_STATE, 10u);
    EXPECT_NE(nullptr, heap.getGraphicsAllocation());
    EXPECT_NE(nullptr, csr.indirectHeap[IndirectHeap::Type::DYNAMIC_STATE]);
    EXPECT_EQ(&heap, csr.indirectHeap[IndirectHeap::Type::DYNAMIC_STATE]);
}

HWTEST_F(CommandStreamReceiverTest, givenCsrWhenReleaseIndirectHeapIsCalledThenHeapAllocationIsNull) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &heap = csr.getIndirectHeap(IndirectHeap::Type::DYNAMIC_STATE, 10u);
    csr.releaseIndirectHeap(IndirectHeap::Type::DYNAMIC_STATE);
    EXPECT_EQ(nullptr, heap.getGraphicsAllocation());
    EXPECT_EQ(0u, heap.getMaxAvailableSpace());
}

HWTEST_F(CommandStreamReceiverTest, givenCsrWhenAllocateHeapMemoryIsCalledThenHeapMemoryIsAllocated) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    IndirectHeap *dsh = nullptr;
    csr.allocateHeapMemory(IndirectHeap::Type::DYNAMIC_STATE, 4096u, dsh);
    EXPECT_NE(nullptr, dsh);
    ASSERT_NE(nullptr, dsh->getGraphicsAllocation());
    csr.getMemoryManager()->freeGraphicsMemory(dsh->getGraphicsAllocation());
    delete dsh;
}

HWTEST_F(CommandStreamReceiverTest, givenSurfaceStateHeapTypeWhenAllocateHeapMemoryIsCalledThenSSHHasInitialSpaceReserevedForBindlessOffsets) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    IndirectHeap *ssh = nullptr;
    csr.allocateHeapMemory(IndirectHeap::Type::SURFACE_STATE, 4096u, ssh);
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

TEST(CommandStreamReceiverSimpleTest, givenCsrWhenSubmitiingBatchBufferThenTaskCountIsIncrementedAndLatestsValuesSetCorrectly) {
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

HWTEST_F(CommandStreamReceiverTest, givenUpdateTaskCountFromWaitWhenSubmitiingBatchBufferThenTaskCountIsIncrementedAndLatestsValuesSetCorrectly) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UpdateTaskCountFromWait.set(3);

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
    DebugManager.flags.OverrideCsrAllocationSize.set(overrideSize);

    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

    bool ret = commandStreamReceiver.createPreemptionAllocation();
    ASSERT_TRUE(ret);
    EXPECT_EQ(static_cast<size_t>(overrideSize), commandStreamReceiver.preemptionAllocation->getUnderlyingBufferSize());
}

HWTEST_F(CommandStreamReceiverTest, givenCommandStreamReceiverWhenCallingGetMemoryCompressionStateThenReturnNotApplicable) {
    CommandStreamReceiverHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

    for (bool auxTranslationRequired : {false, true}) {
        auto memoryCompressionState = commandStreamReceiver.getMemoryCompressionState(auxTranslationRequired);
        EXPECT_EQ(MemoryCompressionState::NotApplicable, memoryCompressionState);
    }
}

HWTEST_F(CommandStreamReceiverTest, givenDebugVariableEnabledWhenCreatingCsrThenEnableTimestampPacketWriteMode) {
    DebugManagerStateRestore restore;

    DebugManager.flags.EnableTimestampPacket.set(true);
    CommandStreamReceiverHw<FamilyType> csr1(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    EXPECT_TRUE(csr1.peekTimestampPacketWriteEnabled());

    DebugManager.flags.EnableTimestampPacket.set(false);
    CommandStreamReceiverHw<FamilyType> csr2(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    EXPECT_FALSE(csr2.peekTimestampPacketWriteEnabled());
}

HWTEST_F(CommandStreamReceiverTest, whenDirectSubmissionDisabledThenExpectNoFeatureAvailable) {
    DeviceFactory::prepareDeviceEnvironments(*pDevice->getExecutionEnvironment());
    CommandStreamReceiverHw<FamilyType> csr(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular},
                                                                                                        PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    osContext->setDefaultContext(true);
    csr.setupContext(*osContext);
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

HWTEST_F(CommandStreamReceiverTest, givenNoDirectSubmissionWhenCheckTaskCountFromWaitEnabledThenReturnsFalse) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_FALSE(csr.isUpdateTagFromWaitEnabled());
}

HWTEST_F(CommandStreamReceiverTest, givenUpdateTaskCountFromWaitWhenCheckTaskCountFromWaitEnabledThenProperValueReturned) {
    DebugManagerStateRestore restorer;
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    {
        DebugManager.flags.UpdateTaskCountFromWait.set(0);
        EXPECT_FALSE(csr.isUpdateTagFromWaitEnabled());
    }

    {
        DebugManager.flags.UpdateTaskCountFromWait.set(1);
        EXPECT_EQ(csr.isUpdateTagFromWaitEnabled(), csr.isDirectSubmissionEnabled());
    }

    {
        DebugManager.flags.UpdateTaskCountFromWait.set(2);
        EXPECT_EQ(csr.isUpdateTagFromWaitEnabled(), csr.isAnyDirectSubmissionEnabled());
    }

    {
        DebugManager.flags.UpdateTaskCountFromWait.set(3);
        EXPECT_TRUE(csr.isUpdateTagFromWaitEnabled());
    }
}

HWTEST_F(CommandStreamReceiverTest, givenUpdateTaskCountFromWaitWhenCheckIfEnabledThenCanBeEnabledOnlyWithDirectSubmission) {
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
    DebugManager.flags.CreateMultipleRootDevices.set(2);
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
        DebugManager.flags.EnableDirectSubmission.set(1);
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
    DebugManager.flags.EnableDirectSubmissionController.set(1);

    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular},
                                                                                                        PreemptionMode::ThreadGroup, device->getDeviceBitfield())));

    auto controller = static_cast<DirectSubmissionControllerMock *>(device->executionEnvironment->initializeDirectSubmissionController());
    controller->keepControlling.store(false);
    EXPECT_EQ(controller->directSubmissions.size(), 0u);

    osContext->ensureContextInitialized();
    osContext->setDefaultContext(true);
    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].submitOnInit = false;
    csr->setupContext(*osContext);

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
    DebugManager.flags.EnableDirectSubmissionController.set(0);

    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular},
                                                                                                        PreemptionMode::ThreadGroup, device->getDeviceBitfield())));

    osContext->ensureContextInitialized();
    osContext->setDefaultContext(true);
    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].submitOnInit = false;

    csr->setupContext(*osContext);
    bool ret = csr->initDirectSubmission();
    EXPECT_TRUE(ret);
    EXPECT_TRUE(csr->isDirectSubmissionEnabled());

    auto controller = static_cast<DirectSubmissionControllerMock *>(device->executionEnvironment->initializeDirectSubmissionController());
    EXPECT_EQ(controller, nullptr);
}

HWTEST_F(InitDirectSubmissionTest, givenSetCsrFlagSetWhenInitDirectSubmissionThenControllerIsNotCreatedAndCsrIsNotRegistered) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.SetCommandStreamReceiver.set(1);

    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular},
                                                                                                        PreemptionMode::ThreadGroup, device->getDeviceBitfield())));

    osContext->ensureContextInitialized();
    osContext->setDefaultContext(true);
    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].submitOnInit = false;

    csr->setupContext(*osContext);
    bool ret = csr->initDirectSubmission();
    EXPECT_TRUE(ret);
    EXPECT_TRUE(csr->isDirectSubmissionEnabled());

    auto controller = static_cast<DirectSubmissionControllerMock *>(device->executionEnvironment->initializeDirectSubmissionController());
    EXPECT_EQ(controller, nullptr);
}

HWTEST_F(InitDirectSubmissionTest, whenDirectSubmissionEnabledOnRcsThenExpectFeatureAvailable) {
    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular},
                                                                                                        PreemptionMode::ThreadGroup, device->getDeviceBitfield())));
    osContext->ensureContextInitialized();
    osContext->setDefaultContext(true);
    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].submitOnInit = false;

    csr->setupContext(*osContext);
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
    uint32_t recursiveLockCounter = 0;
};

HWTEST_F(InitDirectSubmissionTest, whenCallInitDirectSubmissionAgainThenItIsNotReinitialized) {
    auto csr = std::make_unique<CommandStreamReceiverHwDirectSubmissionMock<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular},
                                                                                                        PreemptionMode::ThreadGroup, device->getDeviceBitfield())));
    osContext->ensureContextInitialized();
    osContext->setDefaultContext(true);
    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].submitOnInit = false;

    csr->setupContext(*osContext);
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

HWTEST_F(InitDirectSubmissionTest, whenCallInitDirectSubmissionThenObtainLock) {
    auto csr = std::make_unique<CommandStreamReceiverHwDirectSubmissionMock<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular},
                                                                                                        PreemptionMode::ThreadGroup, device->getDeviceBitfield())));
    osContext->ensureContextInitialized();
    osContext->setDefaultContext(true);
    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].submitOnInit = false;
    csr->setupContext(*osContext);
    csr->initDirectSubmission();
    EXPECT_EQ(1u, csr->recursiveLockCounter);

    csr.reset();
}
HWTEST_F(InitDirectSubmissionTest, givenDirectSubmissionEnabledWhenPlatformNotSupportsRcsThenExpectFeatureNotAvailable) {
    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular},
                                                                                                        PreemptionMode::ThreadGroup, device->getDeviceBitfield())));
    osContext->ensureContextInitialized();
    osContext->setDefaultContext(true);
    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].engineSupported = false;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].submitOnInit = false;

    csr->setupContext(*osContext);
    bool ret = csr->initDirectSubmission();
    EXPECT_TRUE(ret);
    EXPECT_FALSE(csr->isDirectSubmissionEnabled());

    csr.reset();
}

HWTEST_F(InitDirectSubmissionTest, whenDirectSubmissionEnabledOnBcsThenExpectFeatureAvailable) {
    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular},
                                                                                                        PreemptionMode::ThreadGroup, device->getDeviceBitfield())));
    osContext->ensureContextInitialized();
    osContext->setDefaultContext(true);

    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_BCS].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_BCS].submitOnInit = false;

    csr->setupContext(*osContext);
    bool ret = csr->initDirectSubmission();
    EXPECT_TRUE(ret);
    EXPECT_FALSE(csr->isDirectSubmissionEnabled());
    EXPECT_TRUE(csr->isBlitterDirectSubmissionEnabled());

    csr.reset();
}

HWTEST_F(InitDirectSubmissionTest, givenDirectSubmissionEnabledWhenPlatformNotSupportsBcsThenExpectFeatureNotAvailable) {
    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular},
                                                                                                        PreemptionMode::ThreadGroup, device->getDeviceBitfield())));
    osContext->ensureContextInitialized();
    osContext->setDefaultContext(true);

    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_BCS].engineSupported = false;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_BCS].submitOnInit = false;

    csr->setupContext(*osContext);
    bool ret = csr->initDirectSubmission();
    EXPECT_TRUE(ret);
    EXPECT_FALSE(csr->isDirectSubmissionEnabled());
    EXPECT_FALSE(csr->isBlitterDirectSubmissionEnabled());

    csr.reset();
}

HWTEST_F(InitDirectSubmissionTest, givenLowPriorityContextWhenDirectSubmissionDisabledOnLowPriorityThenExpectFeatureNotAvailable) {
    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::LowPriority},
                                                                                                        PreemptionMode::ThreadGroup, device->getDeviceBitfield())));
    osContext->ensureContextInitialized();
    osContext->setDefaultContext(true);

    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].useLowPriority = false;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].submitOnInit = false;

    csr->setupContext(*osContext);
    bool ret = csr->initDirectSubmission();
    EXPECT_TRUE(ret);
    EXPECT_FALSE(csr->isDirectSubmissionEnabled());

    csr.reset();
}

HWTEST_F(InitDirectSubmissionTest, givenLowPriorityContextWhenDirectSubmissionEnabledOnLowPriorityThenExpectFeatureAvailable) {
    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::LowPriority},
                                                                                                        PreemptionMode::ThreadGroup, device->getDeviceBitfield())));
    osContext->ensureContextInitialized();

    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].useLowPriority = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].submitOnInit = false;
    csr->setupContext(*osContext);
    bool ret = csr->initDirectSubmission();
    EXPECT_TRUE(ret);
    EXPECT_TRUE(csr->isDirectSubmissionEnabled());

    csr.reset();
}

HWTEST_F(InitDirectSubmissionTest, givenInternalContextWhenDirectSubmissionDisabledOnInternalThenExpectFeatureNotAvailable) {
    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Internal}, PreemptionMode::ThreadGroup,
                                                                                                        device->getDeviceBitfield())));
    osContext->ensureContextInitialized();
    osContext->setDefaultContext(true);

    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].useInternal = false;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].submitOnInit = false;

    csr->setupContext(*osContext);
    bool ret = csr->initDirectSubmission();
    EXPECT_TRUE(ret);
    EXPECT_FALSE(csr->isDirectSubmissionEnabled());

    csr.reset();
}

HWTEST_F(InitDirectSubmissionTest, givenInternalContextWhenDirectSubmissionEnabledOnInternalThenExpectFeatureAvailable) {
    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Internal}, PreemptionMode::ThreadGroup,
                                                                                                        device->getDeviceBitfield())));
    osContext->ensureContextInitialized();

    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].useInternal = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].submitOnInit = false;

    csr->setupContext(*osContext);
    bool ret = csr->initDirectSubmission();
    EXPECT_TRUE(ret);
    EXPECT_TRUE(csr->isDirectSubmissionEnabled());

    csr.reset();
}

HWTEST_F(InitDirectSubmissionTest, givenRootDeviceContextWhenDirectSubmissionDisabledOnRootDeviceThenExpectFeatureNotAvailable) {
    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Internal}, PreemptionMode::ThreadGroup,
                                                                                                        device->getDeviceBitfield(), true)));
    osContext->ensureContextInitialized();
    osContext->setDefaultContext(true);

    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].useRootDevice = false;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].submitOnInit = false;

    csr->setupContext(*osContext);
    bool ret = csr->initDirectSubmission();
    EXPECT_TRUE(ret);
    EXPECT_FALSE(csr->isDirectSubmissionEnabled());

    csr.reset();
}

HWTEST_F(InitDirectSubmissionTest, givenRootDeviceContextWhenDirectSubmissionEnabledOnRootDeviceThenExpectFeatureAvailable) {
    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Internal}, PreemptionMode::ThreadGroup,
                                                                                                        device->getDeviceBitfield(), true)));
    osContext->ensureContextInitialized();

    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].useRootDevice = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].submitOnInit = false;

    csr->setupContext(*osContext);
    bool ret = csr->initDirectSubmission();
    EXPECT_TRUE(ret);
    EXPECT_TRUE(csr->isDirectSubmissionEnabled());

    csr.reset();
}

HWTEST_F(InitDirectSubmissionTest, givenNonDefaultContextWhenDirectSubmissionDisabledOnNonDefaultThenExpectFeatureNotAvailable) {
    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Internal}, PreemptionMode::ThreadGroup,
                                                                                                        device->getDeviceBitfield())));
    osContext->ensureContextInitialized();
    osContext->setDefaultContext(false);

    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].useNonDefault = false;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].submitOnInit = false;

    csr->setupContext(*osContext);
    bool ret = csr->initDirectSubmission();
    EXPECT_TRUE(ret);
    EXPECT_FALSE(csr->isDirectSubmissionEnabled());

    csr.reset();
}

HWTEST_F(InitDirectSubmissionTest, givenNonDefaultContextContextWhenDirectSubmissionEnabledOnNonDefaultContextThenExpectFeatureAvailable) {
    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular}, PreemptionMode::ThreadGroup,
                                                                                                        device->getDeviceBitfield())));
    osContext->ensureContextInitialized();
    osContext->setDefaultContext(false);

    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].useNonDefault = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].submitOnInit = false;

    csr->setupContext(*osContext);
    bool ret = csr->initDirectSubmission();
    EXPECT_TRUE(ret);
    EXPECT_TRUE(csr->isDirectSubmissionEnabled());

    csr.reset();
}

HWTEST_F(InitDirectSubmissionTest, GivenBlitterOverrideEnabledWhenBlitterIsNonDefaultContextThenExpectDirectSubmissionStarted) {
    DebugManager.flags.DirectSubmissionOverrideBlitterSupport.set(1);
    DebugManager.flags.DirectSubmissionDisableMonitorFence.set(0);
    DebugManager.flags.DirectSubmissionInsertExtraMiMemFenceCommands.set(0);

    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Internal}, PreemptionMode::ThreadGroup,
                                                                                                        device->getDeviceBitfield())));
    osContext->ensureContextInitialized();
    osContext->setDefaultContext(false);

    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_BCS].engineSupported = false;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_BCS].useNonDefault = false;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_BCS].submitOnInit = false;

    csr->setupContext(*osContext);
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
        EXPECT_EQ(AllocationType::TAG_BUFFER, deviceFactory.rootDevices[rootDeviceIndex]->commandStreamReceivers[0]->getTagAllocation()->getAllocationType());
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

    auto mockGraphicsAllocation = new MockGraphicsAllocationWithDestructorTracing(0, AllocationType::UNKNOWN, &gpuTag, 0llu, 0llu, 1u, MemoryPool::MemoryNull, MemoryManager::maxOsContextCount);
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
    csr->postSyncWriteOffset = 32u;
    executionEnvironment.memoryManager.reset(new OsAgnosticMemoryManager(executionEnvironment));
    EXPECT_EQ(nullptr, csr->getTagAllocation());
    csr->initializeTagAllocation();
    EXPECT_NE(nullptr, csr->getTagAllocation());
    EXPECT_EQ(AllocationType::TAG_BUFFER, csr->getTagAllocation()->getAllocationType());
    EXPECT_EQ(csr->getTagAllocation()->getUnderlyingBuffer(), csr->getTagAddress());
    auto tagAddress = csr->getTagAddress();
    for (uint32_t i = 0; i < 2; i++) {
        EXPECT_EQ(*tagAddress, initialHardwareTag);
        tagAddress = ptrOffset(tagAddress, csr->getPostSyncWriteOffset());
    }
}

TEST(CommandStreamReceiverSimpleTest, givenCommandStreamReceiverWhenEnsureTagAllocationForRootDeviceIndexIsCalledThenProperAllocationIsBeingAllocated) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get(), true, 10u);
    DeviceBitfield devices(0b1111);
    auto csr = std::make_unique<MockCommandStreamReceiver>(executionEnvironment, 0, devices);
    csr->postSyncWriteOffset = 32u;

    executionEnvironment.memoryManager.reset(new OsAgnosticMemoryManager(executionEnvironment));
    EXPECT_EQ(nullptr, csr->getTagAllocation());

    csr->initializeTagAllocation();
    EXPECT_NE(nullptr, csr->getTagAllocation());
    EXPECT_EQ(AllocationType::TAG_BUFFER, csr->getTagAllocation()->getAllocationType());
    EXPECT_EQ(csr->getTagAllocation()->getUnderlyingBuffer(), csr->getTagAddress());

    auto tagAddress = csr->getTagAddress();
    for (uint32_t i = 0; i < 4; i++) {
        EXPECT_EQ(*tagAddress, initialHardwareTag);
        tagAddress = ptrOffset(tagAddress, csr->getPostSyncWriteOffset());
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
    mockAllocation.setAllocationType(AllocationType::KERNEL_ISA);

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
    DebugManager.flags.ProvideVerboseImplicitFlush.set(true);

    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);
    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);
    MockGraphicsAllocation mockAllocation;

    csr.useNewResourceImplicitFlush = true;
    csr.newResources = false;
    testing::internal::CaptureStdout();
    csr.checkForNewResources(10u, GraphicsAllocation::objectNotUsed, mockAllocation);
    EXPECT_TRUE(csr.newResources);

    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(0u, output.size());
    EXPECT_STREQ("New resource detected of type 0\n", output.c_str());
}

TEST(CommandStreamReceiverSimpleTest, givenPrintfTagAllocationAddressFlagEnabledWhenCreatingTagAllocationThenPrintItsAddress) {
    DebugManagerStateRestore restore;
    DebugManager.flags.PrintTagAllocationAddress.set(true);
    DeviceBitfield deviceBitfield(1);
    auto osContext = std::unique_ptr<OsContext>(OsContext::create(nullptr, 0, 0,
                                                                  EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular})));

    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();

    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);
    csr.setupContext(*osContext);

    testing::internal::CaptureStdout();

    csr.initializeTagAllocation();

    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(0u, output.size());

    char expectedStr[128];
    snprintf(expectedStr, 128, "\nCreated tag allocation %p for engine %u\n", csr.getTagAddress(), csr.getOsContext().getEngineType());

    EXPECT_TRUE(hasSubstr(output, std::string(expectedStr)));
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
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(0b11);
    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);
    auto osContext = std::unique_ptr<OsContext>(OsContext::create(nullptr, 0, 0,
                                                                  EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular})));
    csr.setupContext(*osContext);

    auto hostPtr = reinterpret_cast<void *>(0x1234);
    size_t size = 100;
    auto gmmHelper = executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(hostPtr));
    auto temporaryAllocation = std::make_unique<MemoryAllocation>(0, AllocationType::EXTERNAL_HOST_PTR, hostPtr, size, 0,
                                                                  MemoryPool::System4KBPages, MemoryManager::maxOsContextCount, canonizedGpuAddress);
    temporaryAllocation->updateTaskCount(0u, 0u);
    csr.getInternalAllocationStorage()->storeAllocationWithTaskCount(std::move(temporaryAllocation), TEMPORARY_ALLOCATION, 2u);

    csr.postSyncWriteOffset = 32u;
    csr.mockTagAddress[0] = 0u;
    auto nextPartitionTagAddress = ptrOffset(&csr.mockTagAddress[0], csr.getPostSyncWriteOffset());
    *nextPartitionTagAddress = 0u;

    csr.taskCount = 3u;
    csr.activePartitions = 2;

    VariableBackup<volatile TagAddressType *> backupPauseAddress(&CpuIntrinsicsTests::pauseAddress);
    VariableBackup<TaskCountType> backupPauseValue(&CpuIntrinsicsTests::pauseValue);
    VariableBackup<uint32_t> backupPauseOffset(&CpuIntrinsicsTests::pauseOffset);

    CpuIntrinsicsTests::pauseAddress = &csr.mockTagAddress[0];
    CpuIntrinsicsTests::pauseValue = 3u;
    CpuIntrinsicsTests::pauseOffset = csr.getPostSyncWriteOffset();

    CpuIntrinsicsTests::pauseCounter = 0;

    const auto waitStatus = csr.waitForTaskCountAndCleanTemporaryAllocationList(3u);
    EXPECT_EQ(2u, CpuIntrinsicsTests::pauseCounter);
    EXPECT_EQ(WaitStatus::Ready, waitStatus);

    CpuIntrinsicsTests::pauseAddress = nullptr;
}

TEST(CommandStreamReceiverSimpleTest, givenEmptyTemporaryAllocationListWhenWaitingForTaskCountForCleaningTemporaryAllocationsThenDoNotWait) {
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
    EXPECT_EQ(WaitStatus::Ready, waitStatus);

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

    EXPECT_EQ(1u, graphicsAllocation.getResidencyTaskCount(csr0ContextId));
    EXPECT_EQ(1u, graphicsAllocation.getResidencyTaskCount(csr1ContextId));

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
    auto temporaryAllocation = std::make_unique<MemoryAllocation>(0, AllocationType::EXTERNAL_HOST_PTR, hostPtr, size, 0,
                                                                  MemoryPool::System4KBPages, MemoryManager::maxOsContextCount, canonizedGpuAddress);
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
    auto temporaryAllocation = std::make_unique<MemoryAllocation>(0, AllocationType::EXTERNAL_HOST_PTR, hostPtr, size, 0,
                                                                  MemoryPool::System4KBPages, MemoryManager::maxOsContextCount, canonizedGpuAddress);
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
    auto temporaryAllocation = std::make_unique<MemoryAllocation>(0, AllocationType::EXTERNAL_HOST_PTR, hostPtr, size, 0,
                                                                  MemoryPool::System4KBPages, MemoryManager::maxOsContextCount, canonizedGpuAddress);
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

    if (!mockMemoryManager->useNonSvmHostPtrAlloc(AllocationType::EXTERNAL_HOST_PTR, device->getRootDeviceIndex())) {
        runPopulateOsHandlesExpects = true;
        mockMemoryManager->populateOsHandlesResult = MemoryManager::AllocationStatus::InvalidHostPointer;
    } else {
        runAllocateGraphicsMemoryForNonSvmHostPtrExpects = true;
    }

    bool result = commandStreamReceiver->createAllocationForHostSurface(surface, false);
    EXPECT_TRUE(result);

    auto allocation = surface.getAllocation();
    ASSERT_NE(nullptr, allocation);

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

    if (!mockMemoryManager->useNonSvmHostPtrAlloc(AllocationType::EXTERNAL_HOST_PTR, device->getRootDeviceIndex())) {
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
    GraphicsAllocation *allocation = memoryManager->allocateGraphicsMemoryWithProperties({commandStreamReceiver->getRootDeviceIndex(), 128u, AllocationType::COMMAND_BUFFER, pDevice->getDeviceBitfield()});
    LinearStream commandStream{allocation};

    commandStreamReceiver->ensureCommandBufferAllocation(commandStream, 100u, 0u);
    EXPECT_EQ(allocation, commandStream.getGraphicsAllocation());
    EXPECT_EQ(128u, commandStream.getMaxAvailableSpace());

    commandStreamReceiver->ensureCommandBufferAllocation(commandStream, 128u, 0u);
    EXPECT_EQ(allocation, commandStream.getGraphicsAllocation());
    EXPECT_EQ(128u, commandStream.getMaxAvailableSpace());

    memoryManager->freeGraphicsMemory(commandStream.getGraphicsAllocation());
}

TEST_F(CommandStreamReceiverTest, givenMinimumSizeExceedsCurrentWhenCallingEnsureCommandBufferAllocationThenReallocate) {
    GraphicsAllocation *allocation = memoryManager->allocateGraphicsMemoryWithProperties({commandStreamReceiver->getRootDeviceIndex(), 128u, AllocationType::COMMAND_BUFFER, pDevice->getDeviceBitfield()});
    LinearStream commandStream{allocation};

    commandStreamReceiver->ensureCommandBufferAllocation(commandStream, 129u, 0u);
    EXPECT_NE(allocation, commandStream.getGraphicsAllocation());
    memoryManager->freeGraphicsMemory(commandStream.getGraphicsAllocation());
}

TEST_F(CommandStreamReceiverTest, givenMinimumSizeExceedsCurrentWhenCallingEnsureCommandBufferAllocationThenReallocateAndAlignSizeTo64kb) {
    GraphicsAllocation *allocation = memoryManager->allocateGraphicsMemoryWithProperties({commandStreamReceiver->getRootDeviceIndex(), 128u, AllocationType::COMMAND_BUFFER, pDevice->getDeviceBitfield()});
    LinearStream commandStream{allocation};

    commandStreamReceiver->ensureCommandBufferAllocation(commandStream, 129u, 0u);
    EXPECT_EQ(MemoryConstants::pageSize64k, commandStream.getGraphicsAllocation()->getUnderlyingBufferSize());
    EXPECT_EQ(MemoryConstants::pageSize64k, commandStream.getMaxAvailableSpace());

    commandStreamReceiver->ensureCommandBufferAllocation(commandStream, MemoryConstants::pageSize64k + 1u, 0u);
    EXPECT_EQ(2 * MemoryConstants::pageSize64k, commandStream.getGraphicsAllocation()->getUnderlyingBufferSize());
    EXPECT_EQ(2 * MemoryConstants::pageSize64k, commandStream.getMaxAvailableSpace());

    memoryManager->freeGraphicsMemory(commandStream.getGraphicsAllocation());
}

TEST_F(CommandStreamReceiverTest, givenForceCommandBufferAlignmentWhenEnsureCommandBufferAllocationThenItHasProperAlignment) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ForceCommandBufferAlignment.set(2048);

    GraphicsAllocation *allocation = memoryManager->allocateGraphicsMemoryWithProperties({commandStreamReceiver->getRootDeviceIndex(), 128u, AllocationType::COMMAND_BUFFER, pDevice->getDeviceBitfield()});
    LinearStream commandStream{allocation};

    commandStreamReceiver->ensureCommandBufferAllocation(commandStream, 129u, 0u);
    EXPECT_EQ(2 * MemoryConstants::megaByte, commandStream.getGraphicsAllocation()->getUnderlyingBufferSize());
    EXPECT_EQ(2 * MemoryConstants::megaByte, commandStream.getMaxAvailableSpace());

    memoryManager->freeGraphicsMemory(commandStream.getGraphicsAllocation());
}

TEST_F(CommandStreamReceiverTest, givenAdditionalAllocationSizeWhenCallingEnsureCommandBufferAllocationThenSizesOfAllocationAndCommandBufferAreCorrect) {
    GraphicsAllocation *allocation = memoryManager->allocateGraphicsMemoryWithProperties({commandStreamReceiver->getRootDeviceIndex(), 128u, AllocationType::COMMAND_BUFFER, pDevice->getDeviceBitfield()});
    LinearStream commandStream{allocation};

    commandStreamReceiver->ensureCommandBufferAllocation(commandStream, 129u, 350u);
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
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties({commandStreamReceiver->getRootDeviceIndex(), MemoryConstants::pageSize64k, AllocationType::COMMAND_BUFFER, pDevice->getDeviceBitfield()});
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
    DebugManager.flags.SetAmountOfReusableAllocations.set(1);
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
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties({commandStreamReceiver->getRootDeviceIndex(), MemoryConstants::pageSize64k, AllocationType::COMMAND_BUFFER, pDevice->getDeviceBitfield()});
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
    EXPECT_EQ(AdditionalKernelExecInfo::NotSet, mockCSR->lastAdditionalKernelExecInfo);
}

HWTEST_F(CommandStreamReceiverTest, givenDebugFlagWhenCreatingCsrThenSetEnableStaticPartitioningAccordingly) {
    DebugManagerStateRestore restore{};
    DebugManager.flags.EnableImplicitScaling.set(1);

    {
        UltDeviceFactory deviceFactory{1, 2};
        MockDevice &device = *deviceFactory.rootDevices[0];
        EXPECT_TRUE(device.getGpgpuCommandStreamReceiver().isStaticWorkPartitioningEnabled());
        EXPECT_NE(0u, device.getGpgpuCommandStreamReceiver().getWorkPartitionAllocationGpuAddress());
        const auto gpuVa = device.getGpgpuCommandStreamReceiver().getWorkPartitionAllocation()->getGpuAddress();
        EXPECT_EQ(gpuVa, device.getGpgpuCommandStreamReceiver().getWorkPartitionAllocationGpuAddress());
    }
    {
        DebugManager.flags.EnableStaticPartitioning.set(0);
        UltDeviceFactory deviceFactory{1, 2};
        MockDevice &device = *deviceFactory.rootDevices[0];
        EXPECT_FALSE(device.getGpgpuCommandStreamReceiver().isStaticWorkPartitioningEnabled());
        EXPECT_EQ(0u, device.getGpgpuCommandStreamReceiver().getWorkPartitionAllocationGpuAddress());
    }
    {
        DebugManager.flags.EnableStaticPartitioning.set(1);
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
    DebugManager.flags.EnableStaticPartitioning.set(0);

    constexpr size_t subDeviceCount = 3;
    UltDeviceFactory deviceFactory{1, subDeviceCount};
    MockDevice &rootDevice = *deviceFactory.rootDevices[0];
    rootDevice.getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.blitterOperationsSupported = true;
    rootDevice.getRootDeviceEnvironment().getMutableHardwareInfo()->featureTable.ftrBcsInfo = 1;
    UltCommandStreamReceiver<FamilyType> &csr = rootDevice.getUltCommandStreamReceiver<FamilyType>();
    UltCommandStreamReceiver<FamilyType> *bcsCsrs[] = {
        reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(rootDevice.getSubDevice(0)->getEngine(aub_stream::ENGINE_BCS, EngineUsage::Regular).commandStreamReceiver),
        reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(rootDevice.getSubDevice(1)->getEngine(aub_stream::ENGINE_BCS, EngineUsage::Regular).commandStreamReceiver),
        reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(rootDevice.getSubDevice(2)->getEngine(aub_stream::ENGINE_BCS, EngineUsage::Regular).commandStreamReceiver),
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
    DebugManager.flags.EnableStaticPartitioning.set(0);
    UltDeviceFactory deviceFactory{1, 2};
    MockDevice &rootDevice = *deviceFactory.rootDevices[0];
    UltCommandStreamReceiver<FamilyType> &csr = rootDevice.getUltCommandStreamReceiver<FamilyType>();

    ExecutionEnvironment &executionEnvironment = *deviceFactory.rootDevices[0]->executionEnvironment;
    executionEnvironment.memoryManager = std::make_unique<FailingMemoryManager>(executionEnvironment);

    csr.staticWorkPartitioningEnabled = true;
    DebugManager.flags.EnableStaticPartitioning.set(1);
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

INSTANTIATE_TEST_CASE_P(
    CommandStreamReceiverWithAubSubCaptureTest_program,
    CommandStreamReceiverWithAubSubCaptureTest,
    testing::ValuesIn(aubSubCaptureStatus));

using SimulatedCommandStreamReceiverTest = ::testing::Test;

template <typename FamilyType>
struct MockSimulatedCsrHw : public CommandStreamReceiverSimulatedHw<FamilyType> {
    using CommandStreamReceiverSimulatedHw<FamilyType>::CommandStreamReceiverSimulatedHw;
    using CommandStreamReceiverSimulatedHw<FamilyType>::getDeviceIndex;
    void pollForCompletion() override {}
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
    DebugManager.flags.EnableImplicitScaling.set(1);

    UltDeviceFactory deviceFactory{1, 2};
    MockDevice &device = *deviceFactory.rootDevices[0];

    auto memoryManager = static_cast<MockMemoryManager *>(device.getMemoryManager());
    auto commandStreamReceiver = device.getDefaultEngine().commandStreamReceiver;
    memoryManager->freeGraphicsMemory(commandStreamReceiver->getWorkPartitionAllocation());

    DebugManager.flags.EnableBlitterOperationsSupport.set(0);
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
    DebugManager.flags.EnableImplicitScaling.set(1);
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
    DebugManager.flags.LogWaitingForCompletion.set(true);

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.activePartitions = 2;

    volatile TagAddressType *tagAddress = csr.tagAddress;
    constexpr TagAddressType tagValue = 2;
    *tagAddress = tagValue;
    tagAddress = ptrOffset(tagAddress, csr.postSyncWriteOffset);
    *tagAddress = tagValue;

    WaitParams waitParams;
    waitParams.waitTimeout = std::numeric_limits<int64_t>::max();
    constexpr TaskCountType taskCount = 1;

    testing::internal::CaptureStdout();

    WaitStatus status = csr.waitForCompletionWithTimeout(waitParams, taskCount);
    EXPECT_EQ(WaitStatus::Ready, status);

    std::string output = testing::internal::GetCapturedStdout();

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

TEST_F(CommandStreamReceiverTest, givenPreambleFlagIsSetWhenGettingFlagStateThenExpectCorrectState) {
    EXPECT_FALSE(commandStreamReceiver->getPreambleSetFlag());
    commandStreamReceiver->setPreambleSetFlag(true);
    EXPECT_TRUE(commandStreamReceiver->getPreambleSetFlag());
}

TEST_F(CommandStreamReceiverTest, givenPreemptionSentIsInitialWhenSettingPreemptionToNewModeThenExpectCorrectPreemption) {
    PreemptionMode mode = PreemptionMode::Initial;
    EXPECT_EQ(mode, commandStreamReceiver->getPreemptionMode());
    mode = PreemptionMode::ThreadGroup;
    commandStreamReceiver->setPreemptionMode(mode);
    EXPECT_EQ(mode, commandStreamReceiver->getPreemptionMode());
}

using CommandStreamReceiverSystolicTests = Test<CommandStreamReceiverSystolicFixture>;
using SystolicSupport = IsAnyProducts<IGFX_ALDERLAKE_P, IGFX_XE_HP_SDV, IGFX_DG2, IGFX_PVC>;

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

    auto dirtyState = static_cast<CommandStreamReceiverHw<FamilyType> *>(commandStreamReceiver)->getSshState();

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

    auto dirtyState = static_cast<CommandStreamReceiverHw<FamilyType> *>(commandStreamReceiver)->getDshState();

    check = dirtyState.updateAndCheck(&dummyHeap);
    EXPECT_TRUE(check);

    check = dirtyState.updateAndCheck(&dummyHeap);
    EXPECT_FALSE(check);
}

using CommandStreamReceiverHwTest = Test<CommandStreamReceiverFixture>;

HWTEST2_F(CommandStreamReceiverHwTest, givenSshHeapNotProvidedWhenFlushTaskPerformedThenSbaProgammedSurfaceBaseAddressToZero, IsAtLeastXeHpCore) {
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
    auto &hwInfo = pDevice->getHardwareInfo();
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    bool csrValue = commandStreamReceiver.getDcFlushSupport();
    bool helperValue = MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, hwInfo);
    EXPECT_EQ(helperValue, csrValue);
}

struct MockRequiredScratchSpaceController : public ScratchSpaceControllerBase {
    MockRequiredScratchSpaceController(uint32_t rootDeviceIndex,
                                       ExecutionEnvironment &environment,
                                       InternalAllocationStorage &allocationStorage) : ScratchSpaceControllerBase(rootDeviceIndex, environment, allocationStorage) {}
    void setRequiredScratchSpace(void *sshBaseAddress,
                                 uint32_t scratchSlot,
                                 uint32_t requiredPerThreadScratchSize,
                                 uint32_t requiredPerThreadPrivateScratchSize,
                                 TaskCountType currentTaskCount,
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
    EXPECT_EQ(0u, CompletionStamp::getTaskCountFromSubmissionStatusError(SubmissionStatus::SUCCESS));
    EXPECT_EQ(CompletionStamp::outOfHostMemory, CompletionStamp::getTaskCountFromSubmissionStatusError(SubmissionStatus::OUT_OF_HOST_MEMORY));
    EXPECT_EQ(CompletionStamp::outOfDeviceMemory, CompletionStamp::getTaskCountFromSubmissionStatusError(SubmissionStatus::OUT_OF_MEMORY));
}

HWTEST_F(CommandStreamReceiverHwTest, givenFailureOnFlushWhenFlushingBcsTaskThenErrorIsPropagated) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::BufferToHostPtr,
                                                                          commandStreamReceiver, commandStreamReceiver.getTagAllocation(), nullptr,
                                                                          commandStreamReceiver.getTagAllocation()->getUnderlyingBuffer(),
                                                                          commandStreamReceiver.getTagAllocation()->getGpuAddress(), 0,
                                                                          0, 0, 0, 0, 0, 0, 0);

    BlitPropertiesContainer container;
    container.push_back(blitProperties);

    commandStreamReceiver.flushReturnValue = SubmissionStatus::OUT_OF_HOST_MEMORY;
    EXPECT_EQ(CompletionStamp::outOfHostMemory, commandStreamReceiver.flushBcsTask(container, true, false, *pDevice));
    commandStreamReceiver.flushReturnValue = SubmissionStatus::OUT_OF_MEMORY;
    EXPECT_EQ(CompletionStamp::outOfDeviceMemory, commandStreamReceiver.flushBcsTask(container, true, false, *pDevice));
}

HWTEST_F(CommandStreamReceiverHwTest, givenOutOfHostMemoryFailureOnFlushWhenFlushingTaskThenErrorIsPropagated) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.flushReturnValue = SubmissionStatus::OUT_OF_HOST_MEMORY;

    auto completionStamp = commandStreamReceiver.flushTask(commandStream,
                                                           0,
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

    commandStreamReceiver.flushReturnValue = SubmissionStatus::OUT_OF_MEMORY;

    auto completionStamp = commandStreamReceiver.flushTask(commandStream,
                                                           0,
                                                           &dsh,
                                                           &ioh,
                                                           nullptr,
                                                           taskLevel,
                                                           flushTaskFlags,
                                                           *pDevice);

    EXPECT_EQ(CompletionStamp::outOfDeviceMemory, completionStamp.taskCount);
}

HWTEST_F(CommandStreamReceiverHwTest, givenOutOfMemoryFailureOnFlushWhenFlushingMiDWThenErrorIsPropagated) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.flushReturnValue = SubmissionStatus::OUT_OF_MEMORY;

    EXPECT_EQ(SubmissionStatus::OUT_OF_MEMORY, commandStreamReceiver.flushMiFlushDW());

    commandStreamReceiver.flushReturnValue = SubmissionStatus::OUT_OF_HOST_MEMORY;
    EXPECT_EQ(SubmissionStatus::OUT_OF_HOST_MEMORY, commandStreamReceiver.flushMiFlushDW());
}

HWTEST_F(CommandStreamReceiverHwTest, givenOutOfMemoryFailureOnFlushWhenFlushingPipeControlThenErrorIsPropagated) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.flushReturnValue = SubmissionStatus::OUT_OF_MEMORY;

    EXPECT_EQ(SubmissionStatus::OUT_OF_MEMORY, commandStreamReceiver.flushPipeControl());

    commandStreamReceiver.flushReturnValue = SubmissionStatus::OUT_OF_HOST_MEMORY;
    EXPECT_EQ(SubmissionStatus::OUT_OF_HOST_MEMORY, commandStreamReceiver.flushPipeControl());
}

HWTEST_F(CommandStreamReceiverHwTest, givenOutOfMemoryFailureOnFlushWhenFlushingTagUpdateThenErrorIsPropagated) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.flushReturnValue = SubmissionStatus::OUT_OF_MEMORY;

    EXPECT_EQ(SubmissionStatus::OUT_OF_MEMORY, commandStreamReceiver.flushTagUpdate());

    commandStreamReceiver.flushReturnValue = SubmissionStatus::OUT_OF_HOST_MEMORY;
    EXPECT_EQ(SubmissionStatus::OUT_OF_HOST_MEMORY, commandStreamReceiver.flushTagUpdate());
}

HWTEST_F(CommandStreamReceiverHwTest, givenOutOfMemoryFailureOnFlushWhenInitializingDeviceWithFirstSubmissionThenErrorIsPropagated) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.flushReturnValue = SubmissionStatus::OUT_OF_MEMORY;

    EXPECT_EQ(SubmissionStatus::OUT_OF_MEMORY, commandStreamReceiver.initializeDeviceWithFirstSubmission());

    commandStreamReceiver.flushReturnValue = SubmissionStatus::OUT_OF_HOST_MEMORY;
    EXPECT_EQ(SubmissionStatus::OUT_OF_HOST_MEMORY, commandStreamReceiver.initializeDeviceWithFirstSubmission());
}

HWTEST_F(CommandStreamReceiverHwTest, whenFlushTagUpdateThenSetStallingCmdsFlag) {
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    ultCsr.recordFlusheBatchBuffer = true;

    EXPECT_EQ(SubmissionStatus::SUCCESS, ultCsr.flushTagUpdate());

    EXPECT_TRUE(ultCsr.latestFlushedBatchBuffer.hasStallingCmds);
}

HWTEST_F(CommandStreamReceiverHwTest, givenVariousCsrModeWhenGettingTbxModeThenExpectOnlyWhenModeIsTbxOrTbxWithAub) {
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::CSR_HW;
    EXPECT_FALSE(ultCsr.isTbxMode());

    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::CSR_HW_WITH_AUB;
    EXPECT_FALSE(ultCsr.isTbxMode());

    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::CSR_AUB;
    EXPECT_FALSE(ultCsr.isTbxMode());

    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::CSR_TBX;
    EXPECT_TRUE(ultCsr.isTbxMode());

    ultCsr.commandStreamReceiverType = CommandStreamReceiverType::CSR_TBX_WITH_AUB;
    EXPECT_TRUE(ultCsr.isTbxMode());
}
