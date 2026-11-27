/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/command_stream/command_stream_receiver_tests.h"

#include "shared/source/built_ins/sip.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/command_stream_receiver_simulated_hw.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/scratch_space_controller_base.h"
#include "shared/source/command_stream/tag_allocation_layout.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/gmm_helper/gmm_callbacks.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/page_table_mngr.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/surface.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/os_thread.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/utilities/pool_allocators.h"
#include "shared/source/utilities/software_tags_manager.h"
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
#include "shared/test/common/mocks/mock_allocations_list.h"
#include "shared/test/common/mocks/mock_bindless_heaps_helper.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_debugger.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gmm_page_table_mngr.h"
#include "shared/test/common/mocks/mock_host_function_allocator.h"
#include "shared/test/common/mocks/mock_internal_allocation_storage.h"
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
#include <vector>

StreamCapture capture;

namespace NEO {
extern ApiSpecificConfig::ApiType apiTypeForUlts;
} // namespace NEO
using namespace NEO;
#include "shared/source/helpers/kmd_notify_helper.h"
#include "shared/test/common/test_macros/heapless_matchers.h"

template <typename GfxFamily>
struct SimulationWriteTrackingCsr : public UltCommandStreamReceiver<GfxFamily> {
    using BaseClass = UltCommandStreamReceiver<GfxFamily>;
    using BaseClass::BaseClass;

    bool isChunkCopySupportedForSimulation() const override {
        return chunkCopySupported;
    }

    bool writeMemory(GraphicsAllocation &gfxAllocation, bool isChunkCopy, uint64_t gpuVaChunkOffset, size_t chunkSize) override {
        writeMemoryCallCount++;
        lastWriteAllocation = &gfxAllocation;
        lastChunkCopyMode = isChunkCopy;
        lastChunkOffset = gpuVaChunkOffset;
        lastChunkSize = chunkSize;
        return writeMemoryReturnValue;
    }

    void setWritableForSimulation(bool writable, GraphicsAllocation &gfxAllocation) override {
        setWritableCallCount++;
        lastWritableAllocation = &gfxAllocation;
        writableTransitions.push_back(writable);
    }

    bool isAubSubCaptureInactive() const override {
        return aubSubCaptureInactive;
    }

    bool chunkCopySupported = false;
    bool aubSubCaptureInactive = false;
    bool writeMemoryReturnValue = true;
    uint32_t writeMemoryCallCount = 0;
    uint32_t setWritableCallCount = 0;
    GraphicsAllocation *lastWriteAllocation = nullptr;
    GraphicsAllocation *lastWritableAllocation = nullptr;
    uint64_t lastChunkOffset = 0;
    size_t lastChunkSize = 0;
    bool lastChunkCopyMode = false;
    std::vector<bool> writableTransitions = {};
};

using SimulationUploadTagType = TimestampPackets<uint32_t, TimestampPacketConstants::preferredPacketCount>;

struct MockTagNodeForSimulationUpload : public TagNode<SimulationUploadTagType> {
    MockTagNodeForSimulationUpload(MultiGraphicsAllocation *allocation, uint64_t gpuAddress, void *cpuBase) {
        this->gfxAllocation = allocation;
        this->gpuAddress = gpuAddress;
        this->tagForCpuAccess = reinterpret_cast<SimulationUploadTagType *>(cpuBase);
    }
};

TEST_F(CommandStreamReceiverTest, givenOsAgnosticCsrWhenGettingCompletionValueThenProperTaskCountIsReturned) {
    MockGraphicsAllocation allocation{};
    uint32_t expectedValue = 0x1234;

    auto &osContext = commandStreamReceiver->getOsContext();
    allocation.updateTaskCount(expectedValue, osContext.getContextId());
    EXPECT_EQ(expectedValue, commandStreamReceiver->getCompletionValue(allocation));
}

TEST_F(CommandStreamReceiverTest, givenBaseCommandStreamReceiverWhenQueryingResidencyContainerDuplicateRemovalRequiredThenDefaultsToFalse) {
    EXPECT_FALSE(commandStreamReceiver->isResidencyContainerDuplicateRemovalRequired());
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

HWTEST_F(CommandStreamReceiverTest, givenCsrWhenCheckingGpuHangBeforeCheckPeriodElapsedThenFalseIsReturnedAndLastHangCheckTimeIsNotUpdated) {
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.gpuHangCheckPeriodUs = 500;

    CommandStreamReceiver::TimeType lastHangCheckTime = 0;
    const auto currentTime = lastHangCheckTime + std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::microseconds{ultCsr.gpuHangCheckPeriodUs - 1}).count();

    EXPECT_FALSE(ultCsr.checkGpuHangDetected(currentTime, lastHangCheckTime));
    EXPECT_EQ(0, lastHangCheckTime);
}

HWTEST_F(CommandStreamReceiverTest, givenCsrWhenCheckingGpuHangAfterCheckPeriodElapsedAndNoHangDetectedThenFalseIsReturnedAndLastHangCheckTimeIsUpdated) {
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.gpuHangCheckPeriodUs = 500;
    ultCsr.isGpuHangDetectedReturnValue = false;

    CommandStreamReceiver::TimeType lastHangCheckTime = 0;
    const auto currentTime = lastHangCheckTime + std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::microseconds{ultCsr.gpuHangCheckPeriodUs + 1}).count();

    EXPECT_FALSE(ultCsr.checkGpuHangDetected(currentTime, lastHangCheckTime));
    EXPECT_EQ(currentTime, lastHangCheckTime);
}

HWTEST_F(CommandStreamReceiverTest, givenCsrWhenCheckingGpuHangAfterCheckPeriodElapsedAndHangDetectedThenTrueIsReturned) {
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.gpuHangCheckPeriodUs = 500;
    ultCsr.isGpuHangDetectedReturnValue = true;

    CommandStreamReceiver::TimeType lastHangCheckTime = 0;
    const auto currentTime = lastHangCheckTime + std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::microseconds{ultCsr.gpuHangCheckPeriodUs + 1}).count();

    EXPECT_TRUE(ultCsr.checkGpuHangDetected(currentTime, lastHangCheckTime));
}

TEST_F(CommandStreamReceiverTest, givenBaseCsrWhenCallingWaitUserFenceThenReturnFalse) {
    EXPECT_FALSE(commandStreamReceiver->waitUserFence(1, commandStreamReceiver->getCompletionAddress(), -1, false, InterruptId::notUsed, nullptr, nullptr));
    EXPECT_FALSE(commandStreamReceiver->waitUserFence(UserFenceWaitOperation::notEqual, 0u, commandStreamReceiver->getCompletionAddress(), UserFenceValueWidth::u32, -1, false, InterruptId::notUsed, nullptr, nullptr));
    EXPECT_FALSE(commandStreamReceiver->waitUserFenceSupported(nullptr));
}

HWTEST_F(CommandStreamReceiverTest, WhenCreatingCsrThenDefaultValuesAreSet) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_EQ(0u, csr.peekTaskLevel());
    EXPECT_EQ(csr.heaplessPrologProgrammed ? 1u : 0u, csr.peekTaskCount());
    EXPECT_FALSE(csr.isPreambleSent);
}

HWTEST_F(CommandStreamReceiverTest, WhenCheckingPerQueuePrologueRequirementsThenCorrectValueAreReturned) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    if (!csr.isPerQueuePrologueEnabled()) {
        EXPECT_FALSE(csr.isPerQueuePrologueRequired());
    }
}

HWTEST_F(CommandStreamReceiverTest, whenContextCreateReturnsFalseThenExpectCSRInitializeResourcesFail) {
    struct MyOsContext : OsContext {
        MyOsContext(uint32_t contextId,
                    const EngineDescriptor &engineDescriptor) : OsContext(0, contextId, engineDescriptor) {}

        bool initializeContext() override {
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
    bool ret = ultCsr.initializeResources(pDevice->getPreemptionMode());
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

HWTEST_F(CommandStreamReceiverTest, givenFlagEnabledForCommandBuffersWhenCallFillReusableAllocationsListThenAllocateCommandBufferOnceAndMakeItResident) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.SetAmountOfReusableAllocations.set(1);
    debugManager.flags.EnableCommandBufferPoolAllocator.set(0);
    debugManager.flags.SetAmountOfInternalHeapsToPreallocate.set(0);
    pDevice->getUltCommandStreamReceiver<FamilyType>().callBaseFillReusableAllocationsList = true;
    EXPECT_TRUE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
    EXPECT_EQ(0u, commandStreamReceiver->getResidencyAllocations().size());

    commandStreamReceiver->fillReusableAllocationsList();

    EXPECT_FALSE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
    EXPECT_EQ(1u, commandStreamReceiver->getResidencyAllocations().size());

    auto allocation = MockAllocationsList::whiteboxCast(internalAllocationStorage->getAllocationsForReuse()).peekFront();
    EXPECT_EQ(AllocationType::commandBuffer, allocation->getAllocationType());

    commandStreamReceiver->fillReusableAllocationsList();
    EXPECT_EQ(1u, commandStreamReceiver->getResidencyAllocations().size());
}

HWTEST_F(CommandStreamReceiverTest, givenFlagEnabledForInternalHeapsWhenCallFillReusableAllocationsListThenAllocateInternalHeapAndMakeItResident) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.SetAmountOfReusableAllocations.set(0);
    debugManager.flags.SetAmountOfInternalHeapsToPreallocate.set(1);
    debugManager.flags.EnableInternalHeapPoolAllocator.set(0);
    pDevice->getUltCommandStreamReceiver<FamilyType>().callBaseFillReusableAllocationsList = true;
    EXPECT_TRUE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
    EXPECT_EQ(0u, commandStreamReceiver->getResidencyAllocations().size());

    commandStreamReceiver->fillReusableAllocationsList();

    EXPECT_FALSE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
    EXPECT_EQ(1u, commandStreamReceiver->getResidencyAllocations().size());

    auto allocation = MockAllocationsList::whiteboxCast(internalAllocationStorage->getAllocationsForReuse()).peekFront();
    EXPECT_EQ(AllocationType::internalHeap, allocation->getAllocationType());
}

HWTEST_F(CommandStreamReceiverTest, givenBcsEngineTypeWhenCallFillReusableAllocationsListThenDoNotAllocateInternalHeap) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.SetAmountOfReusableAllocations.set(0);
    pDevice->getUltCommandStreamReceiver<FamilyType>().callBaseFillReusableAllocationsList = true;

    reinterpret_cast<MockOsContext *>(pDevice->getUltCommandStreamReceiver<FamilyType>().osContext)->engineType = aub_stream::ENGINE_BCS;
    EXPECT_TRUE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
    EXPECT_EQ(0u, commandStreamReceiver->getResidencyAllocations().size());

    commandStreamReceiver->fillReusableAllocationsList();

    EXPECT_TRUE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
    EXPECT_EQ(0u, commandStreamReceiver->getResidencyAllocations().size());
}

HWTEST_F(CommandStreamReceiverTest, givenUnsetPreallocationsPerQueueWhenRequestPreallocationCalledThenPreallocateCommandBufferCorrectly) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableCommandBufferPoolAllocator.set(0);

    EXPECT_TRUE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
    EXPECT_EQ(0u, commandStreamReceiver->getResidencyAllocations().size());

    const auto &hwInfo = pDevice->getHardwareInfo();
    const auto expectedPreallocations = hwInfo.caps.commandBuffersPreallocatedPerCommandQueue;

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
    debugManager.flags.EnableCommandBufferPoolAllocator.set(0);
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

HWTEST_F(CommandStreamReceiverTest, givenPoolAllocatorEnabledWhenReleasePreallocationRequestThenReturnsEarlyWithoutModifyingState) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableCommandBufferPoolAllocator.set(1);
    debugManager.flags.SetAmountOfReusableAllocationsPerCmdQueue.set(1);

    EXPECT_TRUE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
    commandStreamReceiver->releasePreallocationRequest();
    EXPECT_TRUE(commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
}

HWTEST_F(CommandStreamReceiverTest, givenPreallocationsPerQueueWhenRequestPreallocationCalledButAllocationFailedThenRequestIsIgnored) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableCommandBufferPoolAllocator.set(0);
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
    EXPECT_EQ(PreemptionMode::Initial, csr.lastPreemptionMode);
    EXPECT_EQ(static_cast<uint32_t>(-1), csr.latestSentStatelessMocsConfig);
}

TEST_F(CommandStreamReceiverTest, WhenCommandStreamReceiverIsCreatedThenItHasATagValue) {
    EXPECT_NE(nullptr, const_cast<TagAddressType *>(commandStreamReceiver->getTagAddress()));
}

TEST_F(CommandStreamReceiverTest, WhenGettingCommandStreamerThenValidPointerIsReturned) {
    auto &cs = commandStreamReceiver->getCS();
    EXPECT_NE(nullptr, &cs);
}

TEST_F(CommandStreamReceiverTest, WhenCommandStreamReceiverIsCreatedThenAvailableMemoryIsGreaterOrEqualRequiredSize) {
    size_t requiredSize = 16 * MemoryConstants::kiloByte;
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

HWTEST_F(CommandStreamReceiverTest, givenCheckingGpuHangWhenGpuHangDetectedThenGetDeviceStateIsCalled) {
    auto driverModelMock = std::make_unique<MockDriverModel>();
    driverModelMock->isGpuHangDetectedToReturn = true;
    auto driverModel = driverModelMock.get();
    auto osInterface = std::make_unique<OSInterface>();
    osInterface->setDriverModel(std::move(driverModelMock));

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.executionEnvironment.rootDeviceEnvironments[csr.rootDeviceIndex]->osInterface = std::move(osInterface);

    EXPECT_TRUE(csr.isGpuHangDetected());
    EXPECT_EQ(1u, driverModel->getDeviceStateCalledCount);
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
    csr.gpuHangCheckPeriodUs = 0;

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
    csr.gpuHangCheckPeriodUs = 0;

    constexpr auto enableTimeout = false;
    constexpr auto timeoutMicroseconds = std::numeric_limits<std::int64_t>::max();
    constexpr auto taskCountToWait = 1;

    const auto waitStatus = csr.waitForCompletionWithTimeout(enableTimeout, timeoutMicroseconds, taskCountToWait);
    EXPECT_EQ(WaitStatus::ready, waitStatus);
}

HWTEST_F(CommandStreamReceiverTest, givenSimulationModeWhenWritingAllocationChunkWithZeroSizeThenSkipUpload) {
    SimulationWriteTrackingCsr<FamilyType> csr(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr.commandStreamReceiverType = CommandStreamReceiverType::tbx;

    uint8_t storage[32] = {};
    MockGraphicsAllocation allocation(storage, 0x1000, sizeof(storage));

    csr.writeAllocationChunkToSimulation(allocation, 4u, 0u);

    EXPECT_EQ(0u, csr.writeMemoryCallCount);
    EXPECT_EQ(0u, csr.setWritableCallCount);
}

HWTEST_F(CommandStreamReceiverTest, givenNonSimulationModeWhenWritingAllocationChunkThenSkipUpload) {
    SimulationWriteTrackingCsr<FamilyType> csr(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr.commandStreamReceiverType = CommandStreamReceiverType::hardware;

    uint8_t storage[32] = {};
    MockGraphicsAllocation allocation(storage, 0x1800, sizeof(storage));

    csr.writeAllocationChunkToSimulation(allocation, 4u, 8u);

    EXPECT_EQ(0u, csr.writeMemoryCallCount);
    EXPECT_EQ(0u, csr.setWritableCallCount);
}

HWTEST_F(CommandStreamReceiverTest, givenAubModeWhenWritingAllocationChunkThenUploadIsAllowed) {
    SimulationWriteTrackingCsr<FamilyType> csr(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr.commandStreamReceiverType = CommandStreamReceiverType::aub;
    csr.chunkCopySupported = true;

    uint8_t storage[32] = {};
    MockGraphicsAllocation allocation(storage, 0x1a00, sizeof(storage));

    csr.writeAllocationChunkToSimulation(allocation, 4u, 8u);

    EXPECT_EQ(1u, csr.writeMemoryCallCount);
    EXPECT_TRUE(csr.lastChunkCopyMode);
    EXPECT_EQ(4u, csr.lastChunkOffset);
    EXPECT_EQ(8u, csr.lastChunkSize);
}

HWTEST_F(CommandStreamReceiverTest, givenDefaultCommandStreamReceiverWhenCheckingIfAubSubCaptureIsInactiveThenReturnFalse) {
    EXPECT_FALSE(pDevice->getUltCommandStreamReceiver<FamilyType>().isAubSubCaptureInactive());
}

HWTEST_F(CommandStreamReceiverTest, givenInactiveAubSubCaptureWhenWritingAllocationChunkThenUploadIsSkippedInAubModeOnly) {
    SimulationWriteTrackingCsr<FamilyType> csr(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr.commandStreamReceiverType = CommandStreamReceiverType::aub;
    csr.chunkCopySupported = true;
    csr.aubSubCaptureInactive = true;

    uint8_t storage[32] = {};
    MockGraphicsAllocation allocation(storage, 0x1a00, sizeof(storage));
    allocation.allocationType = AllocationType::commandBuffer;

    csr.writeAllocationChunkToSimulation(allocation, 4u, 8u);

    EXPECT_EQ(0u, csr.writeMemoryCallCount);
    EXPECT_EQ(0u, csr.setWritableCallCount);
    EXPECT_FALSE(allocation.isSimulationInitialUploadDone());

    csr.commandStreamReceiverType = CommandStreamReceiverType::tbxWithAub;
    csr.writeAllocationChunkToSimulation(allocation, 4u, 8u);

    EXPECT_EQ(1u, csr.writeMemoryCallCount);
    EXPECT_EQ(0u, csr.lastChunkOffset);
    EXPECT_EQ(sizeof(storage), csr.lastChunkSize);
    EXPECT_TRUE(allocation.isSimulationInitialUploadDone());
}

HWTEST_F(CommandStreamReceiverTest, givenSimulationModeWhenWritingAllocationChunkThenUseChunkOffsetAndRestoreWritableForOneTimeAllocation) {
    SimulationWriteTrackingCsr<FamilyType> csr(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr.commandStreamReceiverType = CommandStreamReceiverType::tbx;
    csr.chunkCopySupported = true;

    uint8_t storage[64] = {};
    MockGraphicsAllocation allocation(storage, 0x2000, sizeof(storage));
    allocation.allocationType = AllocationType::tagBuffer;

    csr.writeAllocationChunkToSimulation(allocation, 8u, 16u);

    EXPECT_EQ(1u, csr.writeMemoryCallCount);
    EXPECT_EQ(&allocation, csr.lastWriteAllocation);
    EXPECT_TRUE(csr.lastChunkCopyMode);
    EXPECT_EQ(8u, csr.lastChunkOffset);
    EXPECT_EQ(16u, csr.lastChunkSize);
    EXPECT_EQ(2u, csr.setWritableCallCount);
    ASSERT_EQ(2u, csr.writableTransitions.size());
    EXPECT_EQ(true, csr.writableTransitions[0]);
    EXPECT_EQ(false, csr.writableTransitions[1]);
}

HWTEST_F(CommandStreamReceiverTest, givenSimulationModeWhenWritingBatchBufferThenWriteRangeFromStartOffsetToUsedSize) {
    SimulationWriteTrackingCsr<FamilyType> csr(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr.commandStreamReceiverType = CommandStreamReceiverType::tbx;
    csr.chunkCopySupported = true;

    uint8_t storage[64] = {};
    MockGraphicsAllocation allocation(storage, 0x2000, sizeof(storage));
    BatchBuffer batchBuffer{};
    batchBuffer.commandBufferAllocation = &allocation;
    batchBuffer.startOffset = 8u;
    batchBuffer.usedSize = 40u;

    csr.writeBatchBufferToSimulation(batchBuffer);

    EXPECT_EQ(1u, csr.writeMemoryCallCount);
    EXPECT_EQ(&allocation, csr.lastWriteAllocation);
    EXPECT_EQ(8u, csr.lastChunkOffset);
    EXPECT_EQ(32u, csr.lastChunkSize);
}

HWTEST_F(CommandStreamReceiverTest, givenSimulationModeWhenWritingStreamChunkThenWriteRangeFromChunkStartToStreamEnd) {
    SimulationWriteTrackingCsr<FamilyType> csr(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr.commandStreamReceiverType = CommandStreamReceiverType::tbx;
    csr.chunkCopySupported = true;

    uint8_t storage[64] = {};
    MockGraphicsAllocation allocation(storage, 0x2000, sizeof(storage));
    LinearStream stream(&allocation);
    stream.getSpace(48u);

    csr.writeStreamChunkToSimulation(stream, 16u);

    EXPECT_EQ(1u, csr.writeMemoryCallCount);
    EXPECT_EQ(&allocation, csr.lastWriteAllocation);
    EXPECT_EQ(16u, csr.lastChunkOffset);
    EXPECT_EQ(32u, csr.lastChunkSize);
}

HWTEST_F(CommandStreamReceiverTest, givenCommandBufferAllocationWhenWritingChunksToSimulationThenWholeAllocationIsWrittenOnFirstUploadAndExactChunkAfterwards) {
    for (auto simulationType : {CommandStreamReceiverType::tbx, CommandStreamReceiverType::aub}) {
        SimulationWriteTrackingCsr<FamilyType> csr(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
        csr.commandStreamReceiverType = simulationType;
        csr.chunkCopySupported = true;

        uint8_t storage[64] = {};
        MockGraphicsAllocation allocation(storage, 0x2000, sizeof(storage));
        allocation.allocationType = AllocationType::commandBuffer;

        csr.writeAllocationChunkToSimulation(allocation, 8u, 16u);

        EXPECT_EQ(1u, csr.writeMemoryCallCount);
        EXPECT_EQ(&allocation, csr.lastWriteAllocation);
        EXPECT_EQ(0u, csr.lastChunkOffset);
        EXPECT_EQ(sizeof(storage), csr.lastChunkSize);
        EXPECT_TRUE(allocation.isSimulationInitialUploadDone());

        csr.writeAllocationChunkToSimulation(allocation, 8u, 16u);

        EXPECT_EQ(2u, csr.writeMemoryCallCount);
        EXPECT_EQ(8u, csr.lastChunkOffset);
        EXPECT_EQ(16u, csr.lastChunkSize);
    }
}

HWTEST_F(CommandStreamReceiverTest, givenCommandBufferAllocationWhenWritingEmptyChunkOrNotInSimulationModeThenInitialUploadIsNotDone) {
    SimulationWriteTrackingCsr<FamilyType> csr(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr.commandStreamReceiverType = CommandStreamReceiverType::hardware;
    csr.chunkCopySupported = true;

    uint8_t storage[64] = {};
    MockGraphicsAllocation allocation(storage, 0x2000, sizeof(storage));
    allocation.allocationType = AllocationType::commandBuffer;

    csr.writeAllocationChunkToSimulation(allocation, 8u, 16u);
    csr.commandStreamReceiverType = CommandStreamReceiverType::tbx;
    csr.writeAllocationChunkToSimulation(allocation, 8u, 0u);

    EXPECT_EQ(0u, csr.writeMemoryCallCount);
    EXPECT_FALSE(allocation.isSimulationInitialUploadDone());
}

HWTEST_F(CommandStreamReceiverTest, givenNonCommandBufferAllocationWhenWritingChunkToSimulationThenExactChunkIsWrittenWithoutInitialUpload) {
    SimulationWriteTrackingCsr<FamilyType> csr(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr.commandStreamReceiverType = CommandStreamReceiverType::aub;
    csr.chunkCopySupported = true;

    uint8_t storage[64] = {};
    MockGraphicsAllocation allocation(storage, 0x2000, sizeof(storage));
    allocation.allocationType = AllocationType::linearStream;

    csr.writeAllocationChunkToSimulation(allocation, 8u, 16u);

    EXPECT_EQ(1u, csr.writeMemoryCallCount);
    EXPECT_EQ(8u, csr.lastChunkOffset);
    EXPECT_EQ(16u, csr.lastChunkSize);
    EXPECT_FALSE(allocation.isSimulationInitialUploadDone());
}

struct CommandBufferPatchAllocations {
    CommandBufferPatchAllocations()
        : otherTypeAllocation(storage, 0x2000, allocationSize),
          commandBufferAllocation(storage + allocationSize, 0x3000, allocationSize),
          candidateAllocation(storage + 2 * allocationSize, 0x4000, allocationSize) {
        candidateAllocation.allocationType = AllocationType::commandBuffer;
        candidateAllocation.setSimulationInitialUploadDone(true);
        candidateAllocations = {&otherTypeAllocation, &candidateAllocation};
    }

    static constexpr size_t allocationSize = 64u;
    uint8_t storage[3 * allocationSize] = {};
    MockGraphicsAllocation otherTypeAllocation;
    MockGraphicsAllocation commandBufferAllocation;
    MockGraphicsAllocation candidateAllocation;
    ResidencyContainer candidateAllocations;
};

HWTEST_F(CommandStreamReceiverTest, givenPatchWithinCommandBufferAllocationWhenWritingCommandBufferPatchThenWritePatchRangeOfCommandBufferAllocation) {
    SimulationWriteTrackingCsr<FamilyType> csr(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr.commandStreamReceiverType = CommandStreamReceiverType::tbx;
    csr.chunkCopySupported = true;
    CommandBufferPatchAllocations allocations;

    csr.writeCommandBufferPatchToSimulation(allocations.commandBufferAllocation, &allocations.candidateAllocations, ptrOffset(allocations.commandBufferAllocation.getUnderlyingBuffer(), 24u), 8u);

    EXPECT_EQ(1u, csr.writeMemoryCallCount);
    EXPECT_EQ(&allocations.commandBufferAllocation, csr.lastWriteAllocation);
    EXPECT_EQ(24u, csr.lastChunkOffset);
    EXPECT_EQ(8u, csr.lastChunkSize);
}

HWTEST_F(CommandStreamReceiverTest, givenPatchWithinCandidateCommandBufferAllocationWhenWritingCommandBufferPatchThenWritePatchRangeOfCandidateAllocation) {
    SimulationWriteTrackingCsr<FamilyType> csr(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr.commandStreamReceiverType = CommandStreamReceiverType::aub;
    csr.chunkCopySupported = true;
    CommandBufferPatchAllocations allocations;

    csr.writeCommandBufferPatchToSimulation(allocations.commandBufferAllocation, &allocations.candidateAllocations, ptrOffset(allocations.candidateAllocation.getUnderlyingBuffer(), 56u), 8u);

    EXPECT_EQ(1u, csr.writeMemoryCallCount);
    EXPECT_EQ(&allocations.candidateAllocation, csr.lastWriteAllocation);
    EXPECT_EQ(56u, csr.lastChunkOffset);
    EXPECT_EQ(8u, csr.lastChunkSize);
}

HWTEST_F(CommandStreamReceiverTest, givenPatchOutsideOfCommandBufferAllocationsWhenWritingCommandBufferPatchThenSkipUpload) {
    SimulationWriteTrackingCsr<FamilyType> csr(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr.commandStreamReceiverType = CommandStreamReceiverType::tbx;
    csr.chunkCopySupported = true;
    CommandBufferPatchAllocations allocations;
    auto patchWithinOtherTypeAllocation = ptrOffset(allocations.otherTypeAllocation.getUnderlyingBuffer(), 8u);
    auto patchCrossingCommandBufferAllocationEnd = ptrOffset(allocations.commandBufferAllocation.getUnderlyingBuffer(), 60u);

    csr.writeCommandBufferPatchToSimulation(allocations.commandBufferAllocation, nullptr, patchWithinOtherTypeAllocation, 8u);
    csr.writeCommandBufferPatchToSimulation(allocations.commandBufferAllocation, &allocations.candidateAllocations, patchWithinOtherTypeAllocation, 8u);
    csr.writeCommandBufferPatchToSimulation(allocations.commandBufferAllocation, &allocations.candidateAllocations, patchCrossingCommandBufferAllocationEnd, 8u);

    EXPECT_EQ(0u, csr.writeMemoryCallCount);
}
HWTEST_F(CommandStreamReceiverTest, givenHardwareModeWhenWritingCommandBufferPatchThenSkipUpload) {
    SimulationWriteTrackingCsr<FamilyType> csr(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr.commandStreamReceiverType = CommandStreamReceiverType::hardware;
    csr.chunkCopySupported = true;
    CommandBufferPatchAllocations allocations;

    csr.writeCommandBufferPatchToSimulation(allocations.commandBufferAllocation, &allocations.candidateAllocations, ptrOffset(allocations.commandBufferAllocation.getUnderlyingBuffer(), 24u), 8u);

    EXPECT_EQ(0u, csr.writeMemoryCallCount);
}

HWTEST_F(CommandStreamReceiverTest, givenTagNodeInAdditionalAllocationWhenUploadingTagChunkThenWriteExpectedChunkRange) {
    SimulationWriteTrackingCsr<FamilyType> csr(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr.commandStreamReceiverType = CommandStreamReceiverType::tbx;
    csr.chunkCopySupported = true;

    uint8_t rootStorage[64] = {};
    uint8_t additionalStorage[64] = {};
    MockGraphicsAllocation rootAllocation(rootStorage, 0x3000, sizeof(rootStorage));
    MockGraphicsAllocation additionalAllocation(1u, additionalStorage, sizeof(additionalStorage));
    additionalAllocation.gpuAddress = 0x4000;

    MultiGraphicsAllocation multiAllocation(1);
    multiAllocation.addAllocation(&rootAllocation);
    multiAllocation.addAllocation(&additionalAllocation);

    MockTagNodeForSimulationUpload tagNode(&multiAllocation, 0x4020, additionalStorage + 0x20);

    csr.writeTagAllocationChunkToSimulation(tagNode, 4u, 8u);

    EXPECT_EQ(1u, csr.writeMemoryCallCount);
    EXPECT_EQ(&additionalAllocation, csr.lastWriteAllocation);
    EXPECT_TRUE(csr.lastChunkCopyMode);
    EXPECT_EQ(0x24u, csr.lastChunkOffset);
    EXPECT_EQ(8u, csr.lastChunkSize);
}

HWTEST_F(CommandStreamReceiverTest, givenEmptyMultiAllocationWhenUploadingTagChunkThenSkipWrite) {
    SimulationWriteTrackingCsr<FamilyType> csr(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr.commandStreamReceiverType = CommandStreamReceiverType::tbx;
    csr.chunkCopySupported = true;

    MultiGraphicsAllocation multiAllocation(0);
    uint8_t storage[64] = {};
    MockTagNodeForSimulationUpload tagNode(&multiAllocation, 0x7000, storage);

    csr.writeTagAllocationChunkToSimulation(tagNode, 0u, 8u);

    EXPECT_EQ(0u, csr.writeMemoryCallCount);
}

HWTEST_F(CommandStreamReceiverTest, givenTagNodeOutsideAllocationRangeWhenUploadingTagChunkThenSkipWrite) {
    SimulationWriteTrackingCsr<FamilyType> csr(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr.commandStreamReceiverType = CommandStreamReceiverType::tbx;
    csr.chunkCopySupported = true;

    uint8_t storage[64] = {};
    MockGraphicsAllocation rootAllocation(storage, 0x5000, sizeof(storage));

    MultiGraphicsAllocation multiAllocation(0);
    multiAllocation.addAllocation(&rootAllocation);

    MockTagNodeForSimulationUpload tagNode(&multiAllocation, 0x4ff0, nullptr);

    csr.writeTagAllocationChunkToSimulation(tagNode, 0u, 8u);
    EXPECT_EQ(0u, csr.writeMemoryCallCount);
}

HWTEST_F(CommandStreamReceiverTest, givenTagNodeWithNoCpuBackedAllocationWhenUploadingTagChunkThenUseGpuOffsetPath) {
    SimulationWriteTrackingCsr<FamilyType> csr(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr.commandStreamReceiverType = CommandStreamReceiverType::tbx;
    csr.chunkCopySupported = true;

    MockGraphicsAllocation rootAllocation(nullptr, 0x5800, 64u);

    MultiGraphicsAllocation multiAllocation(0);
    multiAllocation.addAllocation(&rootAllocation);

    uint8_t storage[64] = {};
    MockTagNodeForSimulationUpload tagNode(&multiAllocation, 0x5810, storage);

    csr.writeTagAllocationChunkToSimulation(tagNode, 4u, 8u);

    EXPECT_EQ(1u, csr.writeMemoryCallCount);
    EXPECT_EQ(&rootAllocation, csr.lastWriteAllocation);
    EXPECT_TRUE(csr.lastChunkCopyMode);
    EXPECT_EQ(0x14u, csr.lastChunkOffset);
    EXPECT_EQ(8u, csr.lastChunkSize);
}

HWTEST_F(CommandStreamReceiverTest, givenNoRootAllocationAndNoMatchingRangeWhenUploadingTagChunkThenFallbackAllocationStillSkipsWrite) {
    SimulationWriteTrackingCsr<FamilyType> csr(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr.commandStreamReceiverType = CommandStreamReceiverType::tbx;
    csr.chunkCopySupported = true;

    uint8_t storage[64] = {};
    MockGraphicsAllocation nonRootAllocation(1u, storage, sizeof(storage));
    nonRootAllocation.gpuAddress = 0x9000;

    MultiGraphicsAllocation multiAllocation(1);
    multiAllocation.addAllocation(&nonRootAllocation);

    MockTagNodeForSimulationUpload tagNode(&multiAllocation, 0x8ff0, nullptr);

    csr.writeTagAllocationChunkToSimulation(tagNode, 0u, 8u);

    EXPECT_EQ(0u, csr.writeMemoryCallCount);
}

HWTEST_F(CommandStreamReceiverTest, givenChunkOffsetOutsideAllocationWhenUploadingTagChunkThenSkipWrite) {
    SimulationWriteTrackingCsr<FamilyType> csr(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr.commandStreamReceiverType = CommandStreamReceiverType::tbx;

    uint8_t storage[64] = {};
    MockGraphicsAllocation rootAllocation(storage, 0x6000, sizeof(storage));

    MultiGraphicsAllocation multiAllocation(0);
    multiAllocation.addAllocation(&rootAllocation);

    MockTagNodeForSimulationUpload tagNode(&multiAllocation, 0x6000, storage);

    csr.writeTagAllocationChunkToSimulation(tagNode, 80u, 8u);

    EXPECT_EQ(0u, csr.writeMemoryCallCount);
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
    csr.gpuHangCheckPeriodUs = 0;

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

    auto taskCountToWait = csr.heaplessPrologProgrammed ? 2 : 1;
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
    csr.gpuHangCheckPeriodUs = 0;

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

HWTEST2_F(CommandStreamReceiverTest, WhenMakingResidentPreemptionAllocationThenAllocationIsPushedToMemoryManagerResidencyListWhenRequired, IsAtLeastXe2HpgCore) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    VariableBackup<bool> skipPreemptionAllocationBackup(&csr.skipPreemptionAllocation);
    VariableBackup<GraphicsAllocation *> preemptionAllocationBackup(&csr.preemptionAllocation);
    auto &residencyAllocations = csr.getResidencyAllocations();
    auto mockPreemptionAllocation = csr.getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr.getRootDeviceIndex(), MemoryConstants::pageSize});

    skipPreemptionAllocationBackup = true;
    preemptionAllocationBackup = nullptr;
    csr.makeResidentPreemptionAllocation();
    EXPECT_EQ(0u, residencyAllocations.size());

    preemptionAllocationBackup = mockPreemptionAllocation;
    csr.makeResidentPreemptionAllocation();
    EXPECT_EQ(0u, residencyAllocations.size());

    skipPreemptionAllocationBackup = false;
    preemptionAllocationBackup = nullptr;
    csr.makeResidentPreemptionAllocation();
    EXPECT_EQ(0u, residencyAllocations.size());

    preemptionAllocationBackup = mockPreemptionAllocation;
    csr.makeResidentPreemptionAllocation();
    ASSERT_EQ(1u, residencyAllocations.size());
    EXPECT_EQ(mockPreemptionAllocation, residencyAllocations.back());

    memoryManager->freeGraphicsMemory(mockPreemptionAllocation);
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

    csr.indirectHeap[IndirectHeap::Type::dynamicState] = dsh;
}

HWTEST_F(CommandStreamReceiverTest, givenSurfaceStateHeapTypeWhenAllocateHeapMemoryIsCalledThenSSHHasInitialSpaceReserevedForBindlessOffsets) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    IndirectHeap *ssh = nullptr;
    csr.allocateHeapMemory(IndirectHeap::Type::surfaceState, 4096u, ssh);
    EXPECT_NE(nullptr, ssh);
    ASSERT_NE(nullptr, ssh->getGraphicsAllocation());

    auto sshReservedSize = UnitTestHelper<FamilyType>::getDefaultSshUsage(pDevice->getProductHelper());
    EXPECT_EQ(sshReservedSize, ssh->getUsed());

    csr.indirectHeap[IndirectHeap::Type::surfaceState] = ssh;
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

HWTEST_F(CommandStreamReceiverTest, givenUpdateTagFromWaitWhenUpdateTaskCountFromWaitForcedThenFlushBatchedSubmissionsAndFlushTagUpdateAreCalled) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UpdateTaskCountFromWait.set(3);

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.flushBatchedSubmissionsCalled = false;
    csr.flushTagUpdateCalled = false;

    csr.updateTagFromWait();

    EXPECT_TRUE(csr.flushBatchedSubmissionsCalled);
    EXPECT_TRUE(csr.flushTagUpdateCalled);
}

HWTEST_F(CommandStreamReceiverTest, givenUpdateTagFromWaitWhenUpdateTaskCountFromWaitDisabledThenFlushBatchedSubmissionsIsCalledAndFlushTagUpdateIsNot) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UpdateTaskCountFromWait.set(0);

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.flushBatchedSubmissionsCalled = false;
    csr.flushTagUpdateCalled = false;

    csr.updateTagFromWait();

    EXPECT_TRUE(csr.flushBatchedSubmissionsCalled);
    EXPECT_FALSE(csr.flushTagUpdateCalled);
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
    auto alignment = gfxCoreHelper.getPreemptionAllocationAlignment();
    size_t expectedAlignedSize = alignUp(overrideSize, alignment);
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

    osContext->ensureContextInitialized();
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
    EXPECT_TRUE(controller->directSubmissions.contains(csr.get()));

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

    osContext->ensureContextInitialized();
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

    osContext->ensureContextInitialized();
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
    osContext->ensureContextInitialized();
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

    uint32_t recursiveLockCounter = 0;
};

HWTEST_F(InitDirectSubmissionTest, whenCallInitDirectSubmissionAgainThenItIsNotReinitialized) {
    auto csr = std::make_unique<CommandStreamReceiverHwDirectSubmissionMock<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular},
                                                                                                        PreemptionMode::ThreadGroup, device->getDeviceBitfield())));
    osContext->ensureContextInitialized();
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

HWTEST_F(InitDirectSubmissionTest, whenCallInitDirectSubmissionThenObtainLock) {
    auto csr = std::make_unique<CommandStreamReceiverHwDirectSubmissionMock<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular},
                                                                                                        PreemptionMode::ThreadGroup, device->getDeviceBitfield())));
    osContext->ensureContextInitialized();
    osContext->setDefaultContext(true);
    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].submitOnInit = false;
    csr->setupContext(*osContext);
    csr->initializeTagAllocation();
    csr->initDirectSubmission();
    EXPECT_EQ(1u, csr->recursiveLockCounter);

    csr.reset();
}

HWTEST_F(InitDirectSubmissionTest, givenDirectSubmissionEnabledWhenPlatformNotSupportsRcsThenExpectFeatureNotAvailable) {
    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular},
                                                                                                        PreemptionMode::ThreadGroup, device->getDeviceBitfield())));
    osContext->ensureContextInitialized();
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
    osContext->ensureContextInitialized();
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
    osContext->ensureContextInitialized();
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
    osContext->ensureContextInitialized();
    osContext->setDefaultContext(true);

    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].submitOnInit = false;

    csr->setupContext(*osContext);
    csr->initializeTagAllocation();
    bool ret = csr->initDirectSubmission();
    EXPECT_TRUE(ret);
    EXPECT_FALSE(csr->isDirectSubmissionEnabled());

    csr.reset();
}

HWTEST_F(InitDirectSubmissionTest, givenInternalContextWhenDirectSubmissionDisabledOnInternalThenExpectFeatureNotAvailable) {
    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::internal}, PreemptionMode::ThreadGroup,
                                                                                                        device->getDeviceBitfield())));
    osContext->ensureContextInitialized();
    osContext->setDefaultContext(true);

    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_RCS].submitOnInit = false;

    csr->setupContext(*osContext);
    csr->initializeTagAllocation();
    bool ret = csr->initDirectSubmission();
    EXPECT_TRUE(ret);
    EXPECT_FALSE(csr->isDirectSubmissionEnabled());

    csr.reset();
}

HWTEST_F(InitDirectSubmissionTest, givenNonDefaultContextWhenDirectSubmissionDisabledOnNonDefaultThenExpectFeatureNotAvailable) {
    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::internal}, PreemptionMode::ThreadGroup,
                                                                                                        device->getDeviceBitfield())));
    osContext->ensureContextInitialized();
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
    osContext->ensureContextInitialized();
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
    debugManager.flags.DirectSubmissionInsertExtraMiMemFenceCommands.set(0);

    auto csr = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), device->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::internal}, PreemptionMode::ThreadGroup,
                                                                                                        device->getDeviceBitfield())));
    osContext->ensureContextInitialized();
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
    csr->stopDirectSubmission(false, false);
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

TEST(CommandStreamReceiverSimpleTest, givenCommandStreamReceiverWhenEnsureTagAllocationIsCalledForRootDeviceIndexWhichHasTagAllocationThenReturnEarlySuccess) {
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

    constexpr auto compareEqual = aub_stream::CompareOperationValues::CompareEqual;
    constexpr auto compareNotEqual = aub_stream::CompareOperationValues::CompareNotEqual;

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
extern std::atomic<uint32_t> yieldCounter;
extern volatile TagAddressType *pauseAddress;
extern TaskCountType pauseValue;
extern uint32_t pauseOffset;
} // namespace CpuIntrinsicsTests

HWTEST_F(CommandStreamReceiverTest, givenNotReadyTaskCountWhenTestingTaskCountReadyThenDoNotYieldTheCore) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.activePartitions = 1;
    *csr.tagAddress = 1u;

    auto oldYieldCount = CpuIntrinsicsTests::yieldCounter.load();

    EXPECT_FALSE(csr.testTaskCountReady(csr.tagAddress, 2u));
    EXPECT_EQ(oldYieldCount, CpuIntrinsicsTests::yieldCounter);

    EXPECT_TRUE(csr.testTaskCountReady(csr.tagAddress, 1u));
    EXPECT_EQ(oldYieldCount, CpuIntrinsicsTests::yieldCounter);
}

HWTEST_F(CommandStreamReceiverTest, givenNotReadyTaskCountWhenTestingTaskCountReadyWithWaitOnMissThenYieldTheCore) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.activePartitions = 1;
    *csr.tagAddress = 1u;

    auto oldYieldCount = CpuIntrinsicsTests::yieldCounter.load();

    EXPECT_FALSE(csr.testTaskCountReady(csr.tagAddress, 2u, true));
    EXPECT_EQ(oldYieldCount + 1, CpuIntrinsicsTests::yieldCounter);

    EXPECT_TRUE(csr.testTaskCountReady(csr.tagAddress, 1u, true));
    EXPECT_EQ(oldYieldCount + 1, CpuIntrinsicsTests::yieldCounter);
}

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
    mockCsr->setupContext(commandStreamReceiver->getOsContext());
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

    uint32_t valueBefore = allocationPtr->getHostPtrTaskCountAssignment();
    mockCsr->createAllocationForHostSurface(hostSurface, false);
    EXPECT_EQ(valueBefore + 1, hostSurface.getAllocation()->getHostPtrTaskCountAssignment());
    allocationPtr->decrementHostPtrTaskCountAssignment();
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
    commandStreamReceiver.setupContext(*osContext);
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

    EXPECT_EQ(1u, allocation->getHostPtrTaskCountAssignment());
    allocation->decrementHostPtrTaskCountAssignment();

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

TEST_F(CreateAllocationForHostSurfaceTest, givenPointerOutsideCpuVirtualAddressRangeWhenAllocationForHostSurfaceWithPtrCopyAllowedIsCreatedThenCopyAllocationIsNotCreated) {
    REQUIRE_64BIT_OR_SKIP();

    auto foreignDeviceUsmPtr = reinterpret_cast<void *>(maxNBitValue(56) + 1);
    HostPtrSurface surface(foreignDeviceUsmPtr, MemoryConstants::pageSize, true);
    mockMemoryManager->callBasePopulateOsHandles = false;
    mockMemoryManager->callBaseAllocateGraphicsMemoryForNonSvmHostPtr = false;
    mockMemoryManager->populateOsHandlesResult = MemoryManager::AllocationStatus::InvalidHostPointer;

    bool result = commandStreamReceiver->createAllocationForHostSurface(surface, false);
    EXPECT_FALSE(result);
    EXPECT_EQ(nullptr, surface.getAllocation());
}

TEST_F(CreateAllocationForHostSurfaceTest, givenHostPtrTrackingDisabledWhenAllocationForHostSurfaceIsCreatedThenAllocateGraphicsMemoryForNonSvmHostPtrIsCalled) {
    if (is32bit) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restore;
    debugManager.flags.EnableHostPtrTracking.set(0);
    mockMemoryManager->callBaseAllocateGraphicsMemoryForNonSvmHostPtr = false;
    mockMemoryManager->callBasePopulateOsHandles = false;

    char memory[8] = {};
    HostPtrSurface surface(memory, sizeof(memory), false);
    bool result = commandStreamReceiver->createAllocationForHostSurface(surface, false);
    EXPECT_FALSE(result);
    EXPECT_EQ(1u, mockMemoryManager->allocateGraphicsMemoryForNonSvmHostPtrCalled);
}

HWTEST_F(CommandStreamReceiverTest, givenMinimumSizeDoesNotExceedCurrentWhenCallingEnsureCommandBufferAllocationThenDoNotReallocate) {
    GraphicsAllocation *allocation = memoryManager->allocateGraphicsMemoryWithProperties({commandStreamReceiver->getRootDeviceIndex(), 128u, AllocationType::commandBuffer, pDevice->getDeviceBitfield()});
    LinearStream commandStream{allocation};

    commandStreamReceiver->ensureCommandBufferAllocation(commandStream, 100u, 0u);
    EXPECT_EQ(allocation, commandStream.getGraphicsAllocation());
    EXPECT_EQ(MemoryConstants::pageSize, commandStream.getMaxAvailableSpace());

    commandStreamReceiver->ensureCommandBufferAllocation(commandStream, 128u, 0u);
    EXPECT_EQ(allocation, commandStream.getGraphicsAllocation());
    EXPECT_EQ(MemoryConstants::pageSize, commandStream.getMaxAvailableSpace());
    EXPECT_FALSE(pDevice->getUltCommandStreamReceiver<FamilyType>().ucResourceRequiresTagUpdate);

    memoryManager->freeGraphicsMemory(commandStream.getGraphicsAllocation());
}

TEST_F(CommandStreamReceiverTest, givenMinimumSizeExceedsCurrentWhenCallingEnsureCommandBufferAllocationThenReallocate) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableCommandBufferPoolAllocator.set(0);
    GraphicsAllocation *allocation = memoryManager->allocateGraphicsMemoryWithProperties({commandStreamReceiver->getRootDeviceIndex(), 128u, AllocationType::commandBuffer, pDevice->getDeviceBitfield()});
    LinearStream commandStream{allocation};

    commandStreamReceiver->ensureCommandBufferAllocation(commandStream, MemoryConstants::pageSize + 1, 0u);
    EXPECT_NE(allocation, commandStream.getGraphicsAllocation());
    memoryManager->freeGraphicsMemory(commandStream.getGraphicsAllocation());
}

HWTEST_F(CommandStreamReceiverTest, givenMinimumSizeExceedsCurrentWhenCallingEnsureCommandBufferAllocationThenReallocateAndAlignSizeTo64kb) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableCommandBufferPoolAllocator.set(0);
    GraphicsAllocation *allocation = memoryManager->allocateGraphicsMemoryWithProperties({commandStreamReceiver->getRootDeviceIndex(), 128u, AllocationType::commandBuffer, pDevice->getDeviceBitfield()});
    LinearStream commandStream{allocation};

    commandStreamReceiver->ensureCommandBufferAllocation(commandStream, MemoryConstants::pageSize + 1, 0u);

    EXPECT_EQ(MemoryConstants::pageSize64k, commandStream.getGraphicsAllocation()->getUnderlyingBufferSize());
    EXPECT_EQ(MemoryConstants::pageSize64k, commandStream.getMaxAvailableSpace());

    commandStreamReceiver->ensureCommandBufferAllocation(commandStream, MemoryConstants::pageSize64k + 1u, 0u);

    EXPECT_EQ(2 * MemoryConstants::pageSize64k, commandStream.getGraphicsAllocation()->getUnderlyingBufferSize());
    EXPECT_EQ(2 * MemoryConstants::pageSize64k, commandStream.getMaxAvailableSpace());
    EXPECT_TRUE(pDevice->getUltCommandStreamReceiver<FamilyType>().ucResourceRequiresTagUpdate);

    memoryManager->freeGraphicsMemory(commandStream.getGraphicsAllocation());
}

TEST_F(CommandStreamReceiverTest, givenForceCommandBufferAlignmentWhenEnsureCommandBufferAllocationThenItHasProperAlignment) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceCommandBufferAlignment.set(2048);
    debugManager.flags.EnableCommandBufferPoolAllocator.set(0);

    GraphicsAllocation *allocation = memoryManager->allocateGraphicsMemoryWithProperties({commandStreamReceiver->getRootDeviceIndex(), 128u, AllocationType::commandBuffer, pDevice->getDeviceBitfield()});
    LinearStream commandStream{allocation};

    commandStreamReceiver->ensureCommandBufferAllocation(commandStream, MemoryConstants::pageSize + 1, 0u);
    EXPECT_EQ(2 * MemoryConstants::megaByte, commandStream.getGraphicsAllocation()->getUnderlyingBufferSize());
    EXPECT_EQ(2 * MemoryConstants::megaByte, commandStream.getMaxAvailableSpace());

    memoryManager->freeGraphicsMemory(commandStream.getGraphicsAllocation());
}

TEST_F(CommandStreamReceiverTest, givenAdditionalAllocationSizeWhenCallingEnsureCommandBufferAllocationThenSizesOfAllocationAndCommandBufferAreCorrect) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableCommandBufferPoolAllocator.set(0);
    GraphicsAllocation *allocation = memoryManager->allocateGraphicsMemoryWithProperties({commandStreamReceiver->getRootDeviceIndex(), 128u, AllocationType::commandBuffer, pDevice->getDeviceBitfield()});
    LinearStream commandStream{allocation};

    commandStreamReceiver->ensureCommandBufferAllocation(commandStream, MemoryConstants::pageSize + 1, 350u);
    EXPECT_NE(allocation, commandStream.getGraphicsAllocation());
    EXPECT_EQ(MemoryConstants::pageSize64k, commandStream.getGraphicsAllocation()->getUnderlyingBufferSize());
    EXPECT_EQ(MemoryConstants::pageSize64k - 350u, commandStream.getMaxAvailableSpace());

    memoryManager->freeGraphicsMemory(commandStream.getGraphicsAllocation());
}

TEST_F(CommandStreamReceiverTest, givenMinimumSizeExceedsCurrentAndNoAllocationsForReuseWhenCallingEnsureCommandBufferAllocationThenAllocateMemoryFromMemoryManager) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableCommandBufferPoolAllocator.set(0);
    LinearStream commandStream;

    EXPECT_TRUE(internalAllocationStorage->getAllocationsForReuse().peekIsEmpty());
    commandStreamReceiver->ensureCommandBufferAllocation(commandStream, 1u, 0u);
    EXPECT_NE(nullptr, commandStream.getGraphicsAllocation());

    memoryManager->freeGraphicsMemory(commandStream.getGraphicsAllocation());
}

TEST_F(CommandStreamReceiverTest, givenMinimumSizeExceedsCurrentAndAllocationsForReuseWhenCallingEnsureCommandBufferAllocationThenObtainAllocationFromInternalAllocationStorage) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableCommandBufferPoolAllocator.set(0);
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
    debugManager.flags.EnableCommandBufferPoolAllocator.set(0);
    debugManager.flags.SetAmountOfReusableAllocations.set(1);
    debugManager.flags.SetAmountOfInternalHeapsToPreallocate.set(0);
    pDevice->getUltCommandStreamReceiver<FamilyType>().callBaseFillReusableAllocationsList = true;

    commandStreamReceiver->fillReusableAllocationsList();
    auto allocation = MockAllocationsList::whiteboxCast(internalAllocationStorage->getAllocationsForReuse()).peekFront();

    LinearStream commandStream;

    EXPECT_FALSE(internalAllocationStorage->getAllocationsForReuse().peekIsEmpty());
    commandStreamReceiver->ensureCommandBufferAllocation(commandStream, 1u, 0u);
    EXPECT_EQ(allocation, commandStream.getGraphicsAllocation());
    EXPECT_TRUE(internalAllocationStorage->getAllocationsForReuse().peekIsEmpty());

    memoryManager->freeGraphicsMemory(commandStream.getGraphicsAllocation());
}

TEST_F(CommandStreamReceiverTest, givenMinimumSizeExceedsCurrentAndNoSuitableReusableAllocationWhenCallingEnsureCommandBufferAllocationThenObtainAllocationMemoryManager) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableCommandBufferPoolAllocator.set(0);
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
    if (!heaplessModeEnabled) {
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

template <typename GfxFamily>
struct BoundedKmdWaitCsr : UltCommandStreamReceiver<GfxFamily> {
    using BaseClass = UltCommandStreamReceiver<GfxFamily>;
    using BaseClass::BaseClass;

    WaitStatus waitForCompletionWithTimeout(const WaitParams &params, TaskCountType taskCountToWait) override {
        this->completionWaitParams.push_back(params);
        this->completionTaskCounts.push_back(taskCountToWait);
        return this->completionWaitResults.at(this->completionWaitResultIndex++);
    }

    WaitStatus waitForFlushStamp(FlushStamp &flushStampToWait, uint64_t timeoutNanoseconds) override {
        this->waitForFlushStampCalled++;
        this->waitedFlushStamp = flushStampToWait;
        this->waitedTimeoutNanoseconds = timeoutNanoseconds;
        return this->waitForFlushStampResult;
    }

    WaitStatus callBoundedKmdWait(TaskCountType taskCountToWait, FlushStamp flushStampToWait, uint64_t timeoutNanoseconds) {
        return CommandStreamReceiverHw<GfxFamily>::waitForTaskCountWithKmdNotifyFallback(taskCountToWait, flushStampToWait, false, QueueThrottle::LOW, timeoutNanoseconds);
    }

    std::vector<WaitStatus> completionWaitResults;
    std::vector<WaitParams> completionWaitParams;
    std::vector<TaskCountType> completionTaskCounts;
    size_t completionWaitResultIndex = 0;
    WaitStatus waitForFlushStampResult = WaitStatus::notReady;
    uint32_t waitForFlushStampCalled = 0;
    FlushStamp waitedFlushStamp = 0;
    uint64_t waitedTimeoutNanoseconds = 0;
};

HWTEST_F(CommandStreamReceiverHwTest, givenReadyInitialTaskCountWhenUsingBoundedKmdWaitThenFlushStampIsNotWaitedFor) {
    BoundedKmdWaitCsr<FamilyType> csr(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr.completionWaitResults = {WaitStatus::ready};

    EXPECT_EQ(WaitStatus::ready, csr.callBoundedKmdWait(7u, 11u, 13000000u));
    ASSERT_EQ(1u, csr.completionWaitParams.size());
    EXPECT_TRUE(csr.completionWaitParams[0].enableTimeout);
    EXPECT_EQ(7u, csr.completionTaskCounts[0]);
    EXPECT_EQ(0u, csr.waitForFlushStampCalled);
}

HWTEST_F(CommandStreamReceiverHwTest, givenGpuHangFromInitialTaskCountCheckWhenUsingBoundedKmdWaitThenGpuHangIsReturned) {
    BoundedKmdWaitCsr<FamilyType> csr(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr.completionWaitResults = {WaitStatus::gpuHang};

    EXPECT_EQ(WaitStatus::gpuHang, csr.callBoundedKmdWait(7u, 11u, 13000000u));
    EXPECT_EQ(1u, csr.completionWaitParams.size());
    EXPECT_EQ(0u, csr.waitForFlushStampCalled);
}

HWTEST_F(CommandStreamReceiverHwTest, givenNotReadyFlushStampWaitWhenUsingBoundedKmdWaitThenNotReadyIsReturned) {
    BoundedKmdWaitCsr<FamilyType> csr(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr.completionWaitResults = {WaitStatus::notReady};
    csr.waitForFlushStampResult = WaitStatus::notReady;

    EXPECT_EQ(WaitStatus::notReady, csr.callBoundedKmdWait(7u, 11u, 13000000u));
    EXPECT_EQ(1u, csr.completionWaitParams.size());
    EXPECT_EQ(1u, csr.waitForFlushStampCalled);
    EXPECT_EQ(11u, csr.waitedFlushStamp);
    EXPECT_EQ(13000000u, csr.waitedTimeoutNanoseconds);
}

HWTEST_F(CommandStreamReceiverHwTest, givenGpuHangFromFlushStampWaitWhenUsingBoundedKmdWaitThenGpuHangIsReturned) {
    BoundedKmdWaitCsr<FamilyType> csr(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr.completionWaitResults = {WaitStatus::notReady};
    csr.waitForFlushStampResult = WaitStatus::gpuHang;

    EXPECT_EQ(WaitStatus::gpuHang, csr.callBoundedKmdWait(7u, 11u, 13000000u));
    EXPECT_EQ(1u, csr.completionWaitParams.size());
    EXPECT_EQ(1u, csr.waitForFlushStampCalled);
}

HWTEST_F(CommandStreamReceiverHwTest, givenReadyFlushStampWaitWhenUsingBoundedKmdWaitThenTaskCountIsCheckedAgain) {
    BoundedKmdWaitCsr<FamilyType> csr(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr.completionWaitResults = {WaitStatus::notReady, WaitStatus::ready};
    csr.waitForFlushStampResult = WaitStatus::ready;

    EXPECT_EQ(WaitStatus::ready, csr.callBoundedKmdWait(7u, 11u, 13000000u));
    ASSERT_EQ(2u, csr.completionWaitParams.size());
    EXPECT_TRUE(csr.completionWaitParams[1].enableTimeout);
    EXPECT_EQ(7u, csr.completionTaskCounts[1]);
    EXPECT_EQ(1u, csr.waitForFlushStampCalled);
}

HWTEST_F(CommandStreamReceiverHwTest, givenReadyFlushStampWaitAndIncompleteTaskCountWhenUsingBoundedKmdWaitThenNotReadyIsReturned) {
    BoundedKmdWaitCsr<FamilyType> csr(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr.completionWaitResults = {WaitStatus::notReady, WaitStatus::notReady};
    csr.waitForFlushStampResult = WaitStatus::ready;

    EXPECT_EQ(WaitStatus::notReady, csr.callBoundedKmdWait(7u, 11u, 13000000u));
    EXPECT_EQ(2u, csr.completionWaitParams.size());
    EXPECT_EQ(1u, csr.waitForFlushStampCalled);
}

HWTEST2_F(CommandStreamReceiverHwTest, givenSshHeapNotProvidedWhenFlushTaskPerformedThenSbaProgammedSurfaceBaseAddressToZero, IsHeapfulRequiredAndAtLeastXeCore) {
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
    bool setRequiredScratchSpace(void *sshBaseAddress,
                                 uint32_t scratchSlot,
                                 uint32_t requiredPerThreadScratchSizeSlot0,
                                 uint32_t requiredPerThreadScratchSizeSlot1,

                                 OsContext &osContext,
                                 bool &stateBaseAddressDirty,
                                 bool &vfeStateDirty) override {
        setRequiredScratchSpaceCalled = true;
        return true;
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

struct MockAllocatedScratchSizeController : public ScratchSpaceControllerBase {
    using ScratchSpaceControllerBase::perThreadScratchSpaceSlot0Size;
    using ScratchSpaceControllerBase::ScratchSpaceControllerBase;
};

HWTEST_F(CommandStreamReceiverHwTest, givenScratchSpaceControllerWhenGettingAllocatedPerThreadScratchSizeSlot0ThenSizeFromControllerIsReturned) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto scratchController = new MockAllocatedScratchSizeController(pDevice->getRootDeviceIndex(),
                                                                    *pDevice->getExecutionEnvironment(),
                                                                    *pDevice->getGpgpuCommandStreamReceiver().getInternalAllocationStorage());
    constexpr uint32_t expectedPerThreadScratchSizeSlot0 = 0x400u;
    scratchController->perThreadScratchSpaceSlot0Size = expectedPerThreadScratchSizeSlot0;

    auto originalScratchController = commandStreamReceiver.scratchSpaceController.release();
    commandStreamReceiver.scratchSpaceController.reset(scratchController);

    EXPECT_EQ(expectedPerThreadScratchSizeSlot0, commandStreamReceiver.getPerThreadScratchSizeSlot0Allocated());

    commandStreamReceiver.scratchSpaceController.reset(nullptr);

    EXPECT_EQ(0u, commandStreamReceiver.getPerThreadScratchSizeSlot0Allocated());

    commandStreamReceiver.scratchSpaceController.reset(originalScratchController);
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

        auto blitProperties = BlitProperties::constructPropertiesForCopy(&mockAllocation, 0, &mockAllocation, 0, {0, 0, 0}, {0, 0, 0}, {1, 1, 1}, 0, 0, 0, 0, nullptr, false);
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

HWTEST_F(CommandStreamReceiverHwTest, givenStreamWhenFlushingSmallTaskThenStreamAllocationTaskCountIsUpdated) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &stream = commandStreamReceiver.getCS(4096u);

    EXPECT_EQ(SubmissionStatus::success, commandStreamReceiver.flushSmallTask(stream, stream.getUsed()));

    EXPECT_EQ(commandStreamReceiver.peekTaskCount(), stream.getGraphicsAllocation()->getTaskCount(commandStreamReceiver.getOsContext().getContextId()));
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
