/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/direct_submission/linux/drm_direct_submission.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/page_table_mngr.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/residency.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_command_stream.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler_default.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/sys_calls_common.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/execution_environment_helper.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_gmm_page_table_mngr.h"
#include "shared/test/common/mocks/mock_host_ptr_manager.h"
#include "shared/test/common/mocks/mock_submissions_aggregator.h"
#include "shared/test/common/os_interface/linux/drm_command_stream_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

using namespace NEO;

HWTEST_TEMPLATED_F(DrmCommandStreamTest, givenFlushStampWhenWaitCalledThenWaitForSpecifiedBoHandle) {
    FlushStamp handleToWait = 123;
    GemWait expectedWait = {};
    expectedWait.boHandle = static_cast<uint32_t>(handleToWait);
    expectedWait.timeoutNs = -1;

    csr->waitForFlushStamp(handleToWait);
    EXPECT_TRUE(memcmp(&expectedWait, &mock->receivedGemWait, sizeof(GemWait)) == 0);
    EXPECT_EQ(1, mock->ioctlCount.gemWait);
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, WhenMakingResidentThenSucceeds) {
    DrmAllocation graphicsAllocation(0, AllocationType::UNKNOWN, nullptr, nullptr, 1024, static_cast<osHandle>(1u), MemoryPool::MemoryNull);
    csr->makeResident(graphicsAllocation);

    EXPECT_EQ(0, mock->ioctlCount.gemUserptr);
    EXPECT_EQ(0, mock->ioctlCount.execbuffer2);

    mock->ioctlTearDownExpected.gemWait = 0;
    mock->ioctlTearDownExpected.gemClose = 0;
    mock->ioctlTearDownExpects = true;
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, WhenMakingResidentTwiceThenSucceeds) {
    DrmAllocation graphicsAllocation(0, AllocationType::UNKNOWN, nullptr, nullptr, 1024, static_cast<osHandle>(1u), MemoryPool::MemoryNull);

    csr->makeResident(graphicsAllocation);
    csr->makeResident(graphicsAllocation);

    EXPECT_EQ(0, mock->ioctlCount.gemUserptr);
    EXPECT_EQ(0, mock->ioctlCount.execbuffer2);

    mock->ioctlTearDownExpected.gemWait = 0;
    mock->ioctlTearDownExpected.gemClose = 0;
    mock->ioctlTearDownExpects = true;
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, GivenSizeZeroWhenMakingResidentTwiceThenSucceeds) {
    DrmAllocation graphicsAllocation(0, AllocationType::UNKNOWN, nullptr, nullptr, 0, static_cast<osHandle>(1u), MemoryPool::MemoryNull);

    csr->makeResident(graphicsAllocation);

    EXPECT_EQ(0, mock->ioctlCount.gemUserptr);
    EXPECT_EQ(0, mock->ioctlCount.execbuffer2);

    mock->ioctlTearDownExpected.gemWait = 0;
    mock->ioctlTearDownExpected.gemClose = 0;
    mock->ioctlTearDownExpects = true;
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, GivenResizedWhenMakingResidentTwiceThenSucceeds) {
    DrmAllocation graphicsAllocation(0, AllocationType::UNKNOWN, nullptr, nullptr, 1024, static_cast<osHandle>(1u), MemoryPool::MemoryNull);
    DrmAllocation graphicsAllocation2(0, AllocationType::UNKNOWN, nullptr, nullptr, 8192, static_cast<osHandle>(1u), MemoryPool::MemoryNull);

    csr->makeResident(graphicsAllocation);
    csr->makeResident(graphicsAllocation2);

    EXPECT_EQ(0, mock->ioctlCount.gemUserptr);
    EXPECT_EQ(0, mock->ioctlCount.execbuffer2);

    mock->ioctlTearDownExpected.gemWait = 0;
    mock->ioctlTearDownExpected.gemClose = 0;
    mock->ioctlTearDownExpects = true;
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, WhenFlushingThenAvailableSpaceDoesNotChange) {
    auto expectedSize = alignUp(8u, MemoryConstants::cacheLineSize); // bbEnd
    int boHandle = 123;

    mock->returnHandle = boHandle;

    auto &cs = csr->getCS();
    auto commandBuffer = static_cast<DrmAllocation *>(cs.getGraphicsAllocation());
    ASSERT_NE(nullptr, commandBuffer);
    ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) & 0xFFF);
    EXPECT_EQ(boHandle, commandBuffer->getBO()->peekHandle());

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    auto availableSpacePriorToFlush = cs.getAvailableSpace();
    csr->flush(batchBuffer, csr->getResidencyAllocations());
    EXPECT_EQ(static_cast<uint64_t>(boHandle), csr->obtainCurrentFlushStamp());
    EXPECT_NE(cs.getCpuBase(), nullptr);
    EXPECT_EQ(availableSpacePriorToFlush, cs.getAvailableSpace());

    mock->ioctlTearDownExpected.gemWait = 2;
    mock->ioctlTearDownExpected.gemClose = 1;
    mock->ioctlTearDownExpects = true;

    EXPECT_EQ(1, mock->ioctlCount.gemUserptr);
    EXPECT_EQ(1, mock->ioctlCount.execbuffer2);

    EXPECT_EQ(0u, mock->execBuffers.back().getBatchStartOffset());
    EXPECT_EQ(expectedSize, mock->execBuffers.back().getBatchLen());
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, givenPrintIndicesEnabledWhenFlushThenPrintIndices) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.PrintDeviceAndEngineIdOnSubmission.set(true);

    auto &cs = csr->getCS();
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};

    ::testing::internal::CaptureStdout();
    csr->flush(batchBuffer, csr->getResidencyAllocations());
    const std::string engineType = EngineHelpers::engineTypeToString(csr->getOsContext().getEngineType());
    const std::string engineUsage = EngineHelpers::engineUsageToString(csr->getOsContext().getEngineUsage());
    std::ostringstream expectedValue;
    expectedValue << SysCalls::getProcessId() << ": Submission to RootDevice Index: " << csr->getRootDeviceIndex()
                  << ", Sub-Devices Mask: " << csr->getOsContext().getDeviceBitfield().to_ulong()
                  << ", EngineId: " << csr->getOsContext().getEngineType()
                  << " (" << engineType << ", " << engineUsage << ")\n";
    EXPECT_TRUE(hasSubstr(::testing::internal::GetCapturedStdout(), expectedValue.str()));
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, givenDrmContextIdWhenFlushingThenSetIdToAllExecBuffersAndObjects) {
    uint32_t expectedDrmContextId = 321;
    uint32_t numAllocations = 3;

    auto allocation1 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    auto allocation2 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    csr->makeResident(*allocation1);
    csr->makeResident(*allocation2);

    mock->storedDrmContextId = expectedDrmContextId;

    osContext = std::make_unique<OsContextLinux>(*mock, csr->getRootDeviceIndex(), 1,
                                                 EngineDescriptorHelper::getDefaultDescriptor(HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*defaultHwInfo)[0],
                                                                                              PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo)));
    osContext->ensureContextInitialized();
    csr->setupContext(*osContext);

    auto &cs = csr->getCS();

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    csr->flush(batchBuffer, csr->getResidencyAllocations());

    memoryManager->freeGraphicsMemory(allocation1);
    memoryManager->freeGraphicsMemory(allocation2);

    EXPECT_EQ(1, mock->ioctlCount.contextCreate);
    EXPECT_EQ(1, mock->ioctlCount.execbuffer2);

    EXPECT_EQ(numAllocations, mock->execBuffers.back().getBufferCount());
    EXPECT_EQ(expectedDrmContextId, mock->execBuffers.back().getReserved());

    for (uint32_t i = 0; i < mock->receivedBos.size(); i++) {
        EXPECT_EQ(expectedDrmContextId, mock->receivedBos[i].getReserved());
    }
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, GivenLowPriorityContextWhenFlushingThenSucceeds) {
    auto expectedSize = alignUp(8u, MemoryConstants::cacheLineSize); // bbEnd

    auto &cs = csr->getCS();
    auto commandBuffer = static_cast<DrmAllocation *>(cs.getGraphicsAllocation());

    ASSERT_NE(nullptr, commandBuffer);
    ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) & 0xFFF);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, true, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    csr->flush(batchBuffer, csr->getResidencyAllocations());
    EXPECT_NE(cs.getCpuBase(), nullptr);

    EXPECT_EQ(1, mock->ioctlCount.gemUserptr);
    EXPECT_EQ(1, mock->ioctlCount.execbuffer2);

    mock->ioctlTearDownExpected.gemWait = 2;
    mock->ioctlTearDownExpected.gemClose = 1;
    mock->ioctlTearDownExpects = true;

    EXPECT_EQ(0u, mock->execBuffers.back().getBatchStartOffset());
    EXPECT_EQ(expectedSize, mock->execBuffers.back().getBatchLen());
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, GivenInvalidAddressWhenFlushingThenSucceeds) {
    //allocate command buffer manually
    char *commandBuffer = new (std::nothrow) char[1024];
    ASSERT_NE(nullptr, commandBuffer); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    GmmHelper gmmHelper(nullptr, defaultHwInfo.get());
    auto canonizedGpuAddress = gmmHelper.canonize(castToUint64(commandBuffer));
    DrmAllocation commandBufferAllocation(0, AllocationType::COMMAND_BUFFER, nullptr, commandBuffer, 1024, static_cast<osHandle>(1u), MemoryPool::MemoryNull, canonizedGpuAddress);
    LinearStream cs(&commandBufferAllocation);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    csr->flush(batchBuffer, csr->getResidencyAllocations());
    delete[] commandBuffer;

    EXPECT_EQ(0, mock->ioctlCount.gemUserptr);
    EXPECT_EQ(0, mock->ioctlCount.execbuffer2);

    mock->ioctlTearDownExpected.gemWait = 0;
    mock->ioctlTearDownExpected.gemClose = 0;
    mock->ioctlTearDownExpects = true;
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, GivenNotEmptyBbWhenFlushingThenSucceeds) {
    uint32_t bbUsed = 16 * sizeof(uint32_t);
    auto expectedSize = alignUp(bbUsed + 8, MemoryConstants::cacheLineSize); // bbUsed + bbEnd

    auto &cs = csr->getCS();
    cs.getSpace(bbUsed);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    csr->flush(batchBuffer, csr->getResidencyAllocations());

    EXPECT_EQ(1, mock->ioctlCount.gemUserptr);
    EXPECT_EQ(1, mock->ioctlCount.execbuffer2);

    mock->ioctlTearDownExpected.gemWait = 2;
    mock->ioctlTearDownExpected.gemClose = 1;
    mock->ioctlTearDownExpects = true;

    EXPECT_EQ(0u, mock->execBuffers.back().getBatchStartOffset());
    EXPECT_EQ(expectedSize, mock->execBuffers.back().getBatchLen());
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, GivenNotEmptyNotPaddedBbWhenFlushingThenSucceeds) {
    uint32_t bbUsed = 15 * sizeof(uint32_t);

    auto &cs = csr->getCS();
    cs.getSpace(bbUsed);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    csr->flush(batchBuffer, csr->getResidencyAllocations());

    EXPECT_EQ(1, mock->ioctlCount.gemUserptr);
    EXPECT_EQ(1, mock->ioctlCount.execbuffer2);

    mock->ioctlTearDownExpected.gemWait = 2;
    mock->ioctlTearDownExpected.gemClose = 1;
    mock->ioctlTearDownExpects = true;

    EXPECT_EQ(0u, mock->execBuffers.back().getBatchStartOffset());
    EXPECT_EQ(bbUsed + 4, mock->execBuffers.back().getBatchLen());
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, GivenNotAlignedWhenFlushingThenSucceeds) {
    auto &cs = csr->getCS();
    auto commandBuffer = static_cast<DrmAllocation *>(cs.getGraphicsAllocation());

    //make sure command buffer with offset is not page aligned
    ASSERT_NE(0u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & (this->alignment - 1));
    ASSERT_EQ(4u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & 0x7F);

    auto expectedBatchStartOffset = (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & (this->alignment - 1);
    auto expectedSize = alignUp(8u, MemoryConstants::cacheLineSize); // bbEnd

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 4, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    csr->flush(batchBuffer, csr->getResidencyAllocations());

    EXPECT_EQ(1, mock->ioctlCount.gemUserptr);
    EXPECT_EQ(1, mock->ioctlCount.execbuffer2);

    mock->ioctlTearDownExpected.gemWait = 2;
    mock->ioctlTearDownExpected.gemClose = 1;
    mock->ioctlTearDownExpects = true;

    EXPECT_EQ(expectedBatchStartOffset, mock->execBuffers.back().getBatchStartOffset());
    EXPECT_EQ(expectedSize, mock->execBuffers.back().getBatchLen());
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, GivenCheckFlagsWhenFlushingThenSucceeds) {
    auto &cs = csr->getCS();

    DrmAllocation allocation(0, AllocationType::UNKNOWN, nullptr, (void *)0x7FFFFFFF, 1024, static_cast<osHandle>(0u), MemoryPool::MemoryNull);
    DrmAllocation allocation2(0, AllocationType::UNKNOWN, nullptr, (void *)0x307FFFFFFF, 1024, static_cast<osHandle>(0u), MemoryPool::MemoryNull);
    csr->makeResident(allocation);
    csr->makeResident(allocation2);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    csr->flush(batchBuffer, csr->getResidencyAllocations());

    EXPECT_EQ(1, mock->ioctlCount.execbuffer2);
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, GivenCheckDrmFreeWhenFlushingThenSucceeds) {
    mock->returnHandle = 17;

    auto &cs = csr->getCS();
    auto commandBuffer = static_cast<DrmAllocation *>(cs.getGraphicsAllocation());

    //make sure command buffer with offset is not page aligned
    ASSERT_NE(0u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & (this->alignment - 1));
    ASSERT_EQ(4u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & 0x7F);

    auto expectedBatchStartOffset = (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & (this->alignment - 1);
    auto expectedSize = alignUp(8u, MemoryConstants::cacheLineSize); // bbEnd

    DrmAllocation allocation(0, AllocationType::UNKNOWN, nullptr, nullptr, 1024, static_cast<osHandle>(0u), MemoryPool::MemoryNull);

    csr->makeResident(allocation);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 4, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    csr->flush(batchBuffer, csr->getResidencyAllocations());

    EXPECT_EQ(1, mock->ioctlCount.gemUserptr);
    EXPECT_EQ(1, mock->ioctlCount.execbuffer2);

    mock->ioctlTearDownExpected.gemClose = 1;
    mock->ioctlTearDownExpected.gemWait = 2;
    mock->ioctlTearDownExpects = true;

    EXPECT_EQ(expectedBatchStartOffset, mock->execBuffers.back().getBatchStartOffset());
    EXPECT_EQ(expectedSize, mock->execBuffers.back().getBatchLen());
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, WhenGettingDrmThenNonNullPointerIsReturned) {
    Drm *pDrm = nullptr;
    if (csr->getOSInterface()) {
        pDrm = csr->getOSInterface()->getDriverModel()->as<Drm>();
    }
    ASSERT_NE(nullptr, pDrm);
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, GivenCheckDrmFreeCloseFailedWhenFlushingThenSucceeds) {
    mock->returnHandle = 17;

    auto &cs = csr->getCS();
    auto commandBuffer = static_cast<DrmAllocation *>(cs.getGraphicsAllocation());

    //make sure command buffer with offset is not page aligned
    ASSERT_NE(0u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & (this->alignment - 1));
    ASSERT_EQ(4u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & 0x7F);

    auto expectedBatchStartOffset = (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & (this->alignment - 1);
    auto expectedSize = alignUp(8u, MemoryConstants::cacheLineSize); // bbEnd

    mock->storedRetValForGemClose = -1;

    DrmAllocation allocation(0, AllocationType::UNKNOWN, nullptr, nullptr, 1024, static_cast<osHandle>(0u), MemoryPool::MemoryNull);

    csr->makeResident(allocation);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 4, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    csr->flush(batchBuffer, csr->getResidencyAllocations());

    EXPECT_EQ(1, mock->ioctlCount.gemUserptr);
    EXPECT_EQ(1, mock->ioctlCount.execbuffer2);

    mock->ioctlTearDownExpected.gemClose = 1;
    mock->ioctlTearDownExpected.gemWait = 2;
    mock->ioctlTearDownExpects = true;

    EXPECT_EQ(expectedBatchStartOffset, mock->execBuffers.back().getBatchStartOffset());
    EXPECT_EQ(expectedSize, mock->execBuffers.back().getBatchLen());
}

class DrmCommandStreamBatchingTests : public DrmCommandStreamEnhancedTest {
  public:
    DrmAllocation *preemptionAllocation;

    template <typename GfxFamily>
    void setUpT() {
        DrmCommandStreamEnhancedTest::setUpT<GfxFamily>();
        preemptionAllocation = static_cast<DrmAllocation *>(device->getDefaultEngine().commandStreamReceiver->getPreemptionAllocation());
    }

    template <typename GfxFamily>
    void tearDownT() {
        DrmCommandStreamEnhancedTest::tearDownT<GfxFamily>();
    }
};

HWTEST_TEMPLATED_F(DrmCommandStreamBatchingTests, givenCsrWhenFlushIsCalledThenProperFlagsArePassed) {
    mock->reset();
    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    auto dummyAllocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) & 0xFFF);
    LinearStream cs(commandBuffer);

    csr->makeResident(*dummyAllocation);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    csr->flush(batchBuffer, csr->getResidencyAllocations());

    int ioctlExecCnt = 1;
    int ioctlUserPtrCnt = 2;

    auto engineFlag = static_cast<OsContextLinux &>(csr->getOsContext()).getEngineFlag();

    EXPECT_EQ(ioctlExecCnt + ioctlUserPtrCnt, this->mock->ioctl_cnt.total);
    EXPECT_EQ(ioctlExecCnt, this->mock->ioctl_cnt.execbuffer2);
    EXPECT_EQ(ioctlUserPtrCnt, this->mock->ioctl_cnt.gemUserptr);
    uint64_t flags = engineFlag | I915_EXEC_NO_RELOC;
    EXPECT_EQ(flags, this->mock->execBuffer.getFlags());

    mm->freeGraphicsMemory(dummyAllocation);
    mm->freeGraphicsMemory(commandBuffer);
}

HWTEST_TEMPLATED_F(DrmCommandStreamBatchingTests, givenCsrWhenDispatchPolicyIsSetToBatchingThenCommandBufferIsNotSubmitted) {
    mock->reset();

    csr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    auto testedCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    testedCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);
    testedCsr->useNewResourceImplicitFlush = false;
    testedCsr->useGpuIdleImplicitFlush = false;

    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    auto dummyAllocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) & 0xFFF);
    IndirectHeap cs(commandBuffer);

    csr->makeResident(*dummyAllocation);

    auto allocations = device->getDefaultEngine().commandStreamReceiver->getTagsMultiAllocation();

    csr->setTagAllocation(static_cast<DrmAllocation *>(allocations->getGraphicsAllocation(csr->getRootDeviceIndex())));

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(device->getHardwareInfo());

    csr->flushTask(cs, 0u, &cs, &cs, &cs, 0u, dispatchFlags, *device);

    //make sure command buffer is recorded
    auto &cmdBuffers = mockedSubmissionsAggregator->peekCommandBuffers();
    EXPECT_FALSE(cmdBuffers.peekIsEmpty());
    EXPECT_NE(nullptr, cmdBuffers.peekHead());

    //preemption allocation
    size_t csrSurfaceCount = (device->getPreemptionMode() == PreemptionMode::MidThread) ? 2 : 0;
    csrSurfaceCount += testedCsr->globalFenceAllocation ? 1 : 0;
    csrSurfaceCount += testedCsr->clearColorAllocation ? 1 : 0;
    csrSurfaceCount += testedCsr->getKernelArgsBufferAllocation() ? 1 : 0;

    auto recordedCmdBuffer = cmdBuffers.peekHead();
    EXPECT_EQ(3u + csrSurfaceCount, recordedCmdBuffer->surfaces.size());

    //try to find all allocations
    auto elementInVector = std::find(recordedCmdBuffer->surfaces.begin(), recordedCmdBuffer->surfaces.end(), dummyAllocation);
    EXPECT_NE(elementInVector, recordedCmdBuffer->surfaces.end());

    elementInVector = std::find(recordedCmdBuffer->surfaces.begin(), recordedCmdBuffer->surfaces.end(), commandBuffer);
    EXPECT_NE(elementInVector, recordedCmdBuffer->surfaces.end());

    elementInVector = std::find(recordedCmdBuffer->surfaces.begin(), recordedCmdBuffer->surfaces.end(), allocations->getGraphicsAllocation(0u));
    EXPECT_NE(elementInVector, recordedCmdBuffer->surfaces.end());

    EXPECT_EQ(testedCsr->commandStream.getGraphicsAllocation(), recordedCmdBuffer->batchBuffer.commandBufferAllocation);

    int ioctlUserPtrCnt = (device->getPreemptionMode() == PreemptionMode::MidThread) ? 4 : 3;
    ioctlUserPtrCnt += testedCsr->clearColorAllocation ? 1 : 0;

    EXPECT_EQ(ioctlUserPtrCnt, this->mock->ioctl_cnt.total);
    EXPECT_EQ(ioctlUserPtrCnt, this->mock->ioctl_cnt.gemUserptr);

    EXPECT_EQ(0u, this->mock->execBuffer.getFlags());

    csr->flushBatchedSubmissions();

    mm->freeGraphicsMemory(dummyAllocation);
    mm->freeGraphicsMemory(commandBuffer);
}

HWTEST_TEMPLATED_F(DrmCommandStreamBatchingTests, givenRecordedCommandBufferWhenItIsSubmittedThenFlushTaskIsProperlyCalled) {
    mock->reset();
    csr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    auto testedCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    testedCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);
    testedCsr->useNewResourceImplicitFlush = false;
    testedCsr->useGpuIdleImplicitFlush = false;

    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    IndirectHeap cs(commandBuffer);

    auto allocations = device->getDefaultEngine().commandStreamReceiver->getTagsMultiAllocation();

    csr->setTagAllocation(static_cast<DrmAllocation *>(allocations->getGraphicsAllocation(csr->getRootDeviceIndex())));

    auto &submittedCommandBuffer = csr->getCS(1024);
    //use some bytes
    submittedCommandBuffer.getSpace(4);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(device->getHardwareInfo());
    dispatchFlags.guardCommandBufferWithPipeControl = true;

    csr->flushTask(cs, 0u, &cs, &cs, &cs, 0u, dispatchFlags, *device);

    auto &cmdBuffers = mockedSubmissionsAggregator->peekCommandBuffers();
    auto storedCommandBuffer = cmdBuffers.peekHead();

    ResidencyContainer copyOfResidency = storedCommandBuffer->surfaces;
    copyOfResidency.push_back(storedCommandBuffer->batchBuffer.commandBufferAllocation);

    csr->flushBatchedSubmissions();

    EXPECT_TRUE(cmdBuffers.peekIsEmpty());

    auto commandBufferGraphicsAllocation = submittedCommandBuffer.getGraphicsAllocation();
    EXPECT_TRUE(commandBufferGraphicsAllocation->isResident(csr->getOsContext().getContextId()));

    //preemption allocation
    size_t csrSurfaceCount = (device->getPreemptionMode() == PreemptionMode::MidThread) ? 2 : 0;
    csrSurfaceCount += testedCsr->globalFenceAllocation ? 1 : 0;
    csrSurfaceCount += testedCsr->clearColorAllocation ? 1 : 0;
    csrSurfaceCount += testedCsr->getKernelArgsBufferAllocation() ? 1 : 0;

    //validate that submited command buffer has what we want
    EXPECT_EQ(3u + csrSurfaceCount, this->mock->execBuffer.getBufferCount());
    EXPECT_EQ(4u, this->mock->execBuffer.getBatchStartOffset());
    EXPECT_EQ(submittedCommandBuffer.getUsed(), this->mock->execBuffer.getBatchLen());

    auto *execObjects = reinterpret_cast<MockExecObject *>(this->mock->execBuffer.getBuffersPtr());

    for (unsigned int i = 0; i < this->mock->execBuffer.getBufferCount(); i++) {
        int handle = execObjects[i].getHandle();

        auto handleFound = false;
        for (auto &graphicsAllocation : copyOfResidency) {
            auto bo = static_cast<DrmAllocation *>(graphicsAllocation)->getBO();
            if (bo->peekHandle() == handle) {
                handleFound = true;
            }
        }
        EXPECT_TRUE(handleFound);
    }

    int ioctlExecCnt = 1;
    int ioctlUserPtrCnt = (device->getPreemptionMode() == PreemptionMode::MidThread) ? 3 : 2;
    ioctlUserPtrCnt += testedCsr->clearColorAllocation ? 1 : 0;
    EXPECT_EQ(ioctlExecCnt, this->mock->ioctl_cnt.execbuffer2);
    EXPECT_EQ(ioctlUserPtrCnt, this->mock->ioctl_cnt.gemUserptr);
    EXPECT_EQ(ioctlExecCnt + ioctlUserPtrCnt, this->mock->ioctl_cnt.total);

    mm->freeGraphicsMemory(commandBuffer);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenFailingProcessResidencyWhenFlushingThenFlushReturnsOutOfMemory) {
    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    LinearStream cs(commandBuffer);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};

    auto allocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    executionEnvironment->rootDeviceEnvironments[csr->getRootDeviceIndex()]->memoryOperationsInterface->makeResident(device.get(), ArrayRef<GraphicsAllocation *>(&allocation, 1));

    auto testedCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    testedCsr->processResidencyCallBase = false;
    testedCsr->processResidencyResult = false;

    SubmissionStatus ret = csr->flush(batchBuffer, csr->getResidencyAllocations());
    EXPECT_EQ(SubmissionStatus::OUT_OF_MEMORY, ret);
    EXPECT_EQ(testedCsr->flushInternalCalled, 1u);
    EXPECT_EQ(testedCsr->processResidencyCalled, 1u);
    mm->freeGraphicsMemory(allocation);
    mm->freeGraphicsMemory(commandBuffer);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenFailingExecWhenFlushingThenFlushReturnsFailed) {
    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    LinearStream cs(commandBuffer);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};

    auto allocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    executionEnvironment->rootDeviceEnvironments[csr->getRootDeviceIndex()]->memoryOperationsInterface->makeResident(device.get(), ArrayRef<GraphicsAllocation *>(&allocation, 1));

    auto testedCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    testedCsr->processResidencyCallBase = true;
    testedCsr->processResidencyResult = true;
    testedCsr->execCallBase = false;
    testedCsr->execResult = -1;

    SubmissionStatus ret = csr->flush(batchBuffer, csr->getResidencyAllocations());
    EXPECT_EQ(SubmissionStatus::FAILED, ret);
    EXPECT_EQ(testedCsr->flushInternalCalled, 1u);
    EXPECT_EQ(testedCsr->processResidencyCalled, 1u);
    EXPECT_EQ(testedCsr->execCalled, 1u);
    mm->freeGraphicsMemory(allocation);
    mm->freeGraphicsMemory(commandBuffer);
}

struct DrmCommandStreamDirectSubmissionTest : public DrmCommandStreamEnhancedTest {
    template <typename GfxFamily>
    void setUpT() {
        DebugManager.flags.EnableDirectSubmission.set(1u);
        DebugManager.flags.DirectSubmissionDisableMonitorFence.set(0);
        DrmCommandStreamEnhancedTest::setUpT<GfxFamily>();
        auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
        auto engineType = device->getDefaultEngine().osContext->getEngineType();
        hwInfo->capabilityTable.directSubmissionEngines.data[engineType].engineSupported = true;
        csr->initDirectSubmission();
    }

    template <typename GfxFamily>
    void tearDownT() {
        this->dbgState.reset();
        DrmCommandStreamEnhancedTest::tearDownT<GfxFamily>();
    }

    DebugManagerStateRestore restorer;
};

struct DrmCommandStreamBlitterDirectSubmissionTest : public DrmCommandStreamDirectSubmissionTest {
    template <typename GfxFamily>
    void setUpT() {
        DebugManager.flags.DirectSubmissionOverrideBlitterSupport.set(1u);
        DebugManager.flags.DirectSubmissionOverrideRenderSupport.set(0u);
        DebugManager.flags.DirectSubmissionOverrideComputeSupport.set(0u);

        DrmCommandStreamDirectSubmissionTest::setUpT<GfxFamily>();
        executionEnvironment->incRefInternal();

        osContext.reset(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), 0, 0,
                                          EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular}, PreemptionMode::ThreadGroup, device->getDeviceBitfield())));
        osContext->ensureContextInitialized();

        device->allEngines.emplace_back(csr, osContext.get());
        csr->setupContext(*osContext);
        csr->initDirectSubmission();
    }

    template <typename GfxFamily>
    void tearDownT() {
        DrmCommandStreamDirectSubmissionTest::tearDownT<GfxFamily>();
        osContext.reset();
        executionEnvironment->decRefInternal(); // NOLINT(clang-analyzer-cplusplus.NewDelete)
    }

    std::unique_ptr<OsContext> osContext;
};

struct DrmDirectSubmissionFunctionsCalled {
    bool stopRingBuffer;
    bool wait;
    bool deallocateResources;
};

template <typename GfxFamily>
struct MockDrmDirectSubmissionToTestDtor : public DrmDirectSubmission<GfxFamily, RenderDispatcher<GfxFamily>> {
    MockDrmDirectSubmissionToTestDtor<GfxFamily>(const CommandStreamReceiver &commandStreamReceiver, DrmDirectSubmissionFunctionsCalled &functionsCalled)
        : DrmDirectSubmission<GfxFamily, RenderDispatcher<GfxFamily>>(commandStreamReceiver), functionsCalled(functionsCalled) {
    }
    ~MockDrmDirectSubmissionToTestDtor() override {
        if (ringStart) {
            stopRingBuffer();                                           // NOLINT(clang-analyzer-optin.cplusplus.VirtualCall)
            wait(static_cast<uint32_t>(this->currentTagData.tagValue)); // NOLINT(clang-analyzer-optin.cplusplus.VirtualCall)
        }
        deallocateResources(); // NOLINT(clang-analyzer-optin.cplusplus.VirtualCall)
    }
    using DrmDirectSubmission<GfxFamily, RenderDispatcher<GfxFamily>>::ringStart;
    bool stopRingBuffer() override {
        functionsCalled.stopRingBuffer = true;
        return true;
    }
    void wait(uint32_t taskCountToWait) override {
        functionsCalled.wait = true;
    }
    void deallocateResources() override {
        functionsCalled.deallocateResources = true;
    }
    DrmDirectSubmissionFunctionsCalled &functionsCalled;
};

HWTEST_TEMPLATED_F(DrmCommandStreamDirectSubmissionTest, givenEnabledDirectSubmissionWhenDtorIsCalledButRingIsNotStartedThenDontCallStopRingBufferNorWaitForTagValue) {
    DrmDirectSubmissionFunctionsCalled functionsCalled{};
    auto directSubmission = std::make_unique<MockDrmDirectSubmissionToTestDtor<FamilyType>>(*device->getDefaultEngine().commandStreamReceiver, functionsCalled);
    ASSERT_NE(nullptr, directSubmission);

    EXPECT_FALSE(directSubmission->ringStart);

    directSubmission.reset();

    EXPECT_FALSE(functionsCalled.stopRingBuffer);
    EXPECT_FALSE(functionsCalled.wait);
    EXPECT_TRUE(functionsCalled.deallocateResources);
}

template <typename GfxFamily>
struct MockDrmDirectSubmissionToTestRingStop : public DrmDirectSubmission<GfxFamily, RenderDispatcher<GfxFamily>> {
    MockDrmDirectSubmissionToTestRingStop<GfxFamily>(const CommandStreamReceiver &commandStreamReceiver)
        : DrmDirectSubmission<GfxFamily, RenderDispatcher<GfxFamily>>(commandStreamReceiver) {
    }
    using DrmDirectSubmission<GfxFamily, RenderDispatcher<GfxFamily>>::ringStart;
};

HWTEST_TEMPLATED_F(DrmCommandStreamDirectSubmissionTest, givenEnabledDirectSubmissionWhenStopRingBufferIsCalledThenClearRingStart) {
    auto directSubmission = std::make_unique<MockDrmDirectSubmissionToTestRingStop<FamilyType>>(*device->getDefaultEngine().commandStreamReceiver);
    ASSERT_NE(nullptr, directSubmission);

    directSubmission->stopRingBuffer();
    EXPECT_FALSE(directSubmission->ringStart);
}

template <typename GfxFamily>
struct MockDrmDirectSubmissionDispatchCommandBuffer : public DrmDirectSubmission<GfxFamily, RenderDispatcher<GfxFamily>> {
    MockDrmDirectSubmissionDispatchCommandBuffer<GfxFamily>(const CommandStreamReceiver &commandStreamReceiver)
        : DrmDirectSubmission<GfxFamily, RenderDispatcher<GfxFamily>>(commandStreamReceiver) {
    }

    ADDMETHOD_NOBASE(dispatchCommandBuffer, bool, false,
                     (BatchBuffer & batchBuffer, FlushStampTracker &flushStamp));
};

template <typename GfxFamily>
struct MockDrmBlitterDirectSubmissionDispatchCommandBuffer : public DrmDirectSubmission<GfxFamily, BlitterDispatcher<GfxFamily>> {
    MockDrmBlitterDirectSubmissionDispatchCommandBuffer<GfxFamily>(const CommandStreamReceiver &commandStreamReceiver)
        : DrmDirectSubmission<GfxFamily, BlitterDispatcher<GfxFamily>>(commandStreamReceiver) {
    }

    ADDMETHOD_NOBASE(dispatchCommandBuffer, bool, false,
                     (BatchBuffer & batchBuffer, FlushStampTracker &flushStamp));
};

HWTEST_TEMPLATED_F(DrmCommandStreamDirectSubmissionTest, givenDirectSubmissionFailsThenFlushReturnsError) {
    auto testedCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    testedCsr->completionFenceValuePointer = nullptr;
    testedCsr->directSubmission = std::make_unique<MockDrmDirectSubmissionDispatchCommandBuffer<FamilyType>>(*device->getDefaultEngine().commandStreamReceiver);
    auto directSubmission = testedCsr->directSubmission.get();
    static_cast<MockDrmDirectSubmissionDispatchCommandBuffer<FamilyType> *>(directSubmission)->dispatchCommandBufferResult = false;

    auto &cs = csr->getCS();
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 4, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    uint8_t bbStart[64];
    batchBuffer.endCmdPtr = &bbStart[0];

    auto res = csr->flush(batchBuffer, csr->getResidencyAllocations());
    EXPECT_GT(static_cast<MockDrmDirectSubmissionDispatchCommandBuffer<FamilyType> *>(directSubmission)->dispatchCommandBufferCalled, 0u);
    EXPECT_EQ(NEO::SubmissionStatus::FAILED, res);
}

HWTEST_TEMPLATED_F(DrmCommandStreamBlitterDirectSubmissionTest, givenBlitterDirectSubmissionFailsThenFlushReturnsError) {
    auto testedCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    testedCsr->completionFenceValuePointer = nullptr;
    testedCsr->blitterDirectSubmission = std::make_unique<MockDrmBlitterDirectSubmissionDispatchCommandBuffer<FamilyType>>(*csr);
    auto blitterDirectSubmission = testedCsr->blitterDirectSubmission.get();
    static_cast<MockDrmBlitterDirectSubmissionDispatchCommandBuffer<FamilyType> *>(blitterDirectSubmission)->dispatchCommandBufferResult = false;

    auto &cs = csr->getCS();
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 4, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    uint8_t bbStart[64];
    batchBuffer.endCmdPtr = &bbStart[0];

    auto res = csr->flush(batchBuffer, csr->getResidencyAllocations());
    EXPECT_GT(static_cast<MockDrmBlitterDirectSubmissionDispatchCommandBuffer<FamilyType> *>(blitterDirectSubmission)->dispatchCommandBufferCalled, 0u);
    EXPECT_EQ(NEO::SubmissionStatus::FAILED, res);
}

template <typename GfxFamily>
struct MockDrmDirectSubmission : public DrmDirectSubmission<GfxFamily, RenderDispatcher<GfxFamily>> {
    using DrmDirectSubmission<GfxFamily, RenderDispatcher<GfxFamily>>::currentTagData;
};

template <typename GfxFamily>
struct MockDrmBlitterDirectSubmission : public DrmDirectSubmission<GfxFamily, BlitterDispatcher<GfxFamily>> {
    using DrmDirectSubmission<GfxFamily, BlitterDispatcher<GfxFamily>>::currentTagData;
};

HWTEST_TEMPLATED_F(DrmCommandStreamDirectSubmissionTest, givenEnabledDirectSubmissionWhenFlushThenFlushStampIsNotUpdated) {
    auto &cs = csr->getCS();
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 4, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    uint8_t bbStart[64];
    batchBuffer.endCmdPtr = &bbStart[0];

    auto flushStamp = csr->obtainCurrentFlushStamp();
    csr->flush(batchBuffer, csr->getResidencyAllocations());

    EXPECT_EQ(csr->obtainCurrentFlushStamp(), flushStamp);

    auto directSubmission = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->directSubmission.get();
    ASSERT_NE(nullptr, directSubmission);
    static_cast<MockDrmDirectSubmission<FamilyType> *>(directSubmission)->currentTagData.tagValue = 0u;
}

HWTEST_TEMPLATED_F(DrmCommandStreamDirectSubmissionTest, givenEnabledDirectSubmissionWhenFlushThenCommandBufferAllocationIsNotAddedToHandlerResidencySet) {
    mock->bindAvailable = true;
    auto &cs = csr->getCS();
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 4, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    uint8_t bbStart[64];
    batchBuffer.endCmdPtr = &bbStart[0];

    csr->flush(batchBuffer, csr->getResidencyAllocations());

    auto memoryOperationsInterface = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface.get();
    EXPECT_NE(memoryOperationsInterface->isResident(device.get(), *batchBuffer.commandBufferAllocation), MemoryOperationsStatus::SUCCESS);

    auto directSubmission = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->directSubmission.get();
    ASSERT_NE(nullptr, directSubmission);
    static_cast<MockDrmDirectSubmission<FamilyType> *>(directSubmission)->currentTagData.tagValue = 0u;
}

HWTEST_TEMPLATED_F(DrmCommandStreamBlitterDirectSubmissionTest, givenEnabledDirectSubmissionOnBlitterWhenFlushThenFlushStampIsNotUpdated) {
    auto &cs = csr->getCS();
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 4, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    uint8_t bbStart[64];
    batchBuffer.endCmdPtr = &bbStart[0];

    auto flushStamp = csr->obtainCurrentFlushStamp();
    csr->flush(batchBuffer, csr->getResidencyAllocations());

    EXPECT_EQ(csr->obtainCurrentFlushStamp(), flushStamp);

    auto directSubmission = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->blitterDirectSubmission.get();
    ASSERT_NE(nullptr, directSubmission);
    static_cast<MockDrmBlitterDirectSubmission<FamilyType> *>(directSubmission)->currentTagData.tagValue = 0u;

    EXPECT_EQ(nullptr, static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->directSubmission.get());
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, givenDrmCommandStreamReceiverWhenCreatePageTableManagerIsCalledThenCreatePageTableManager) {
    executionEnvironment.prepareRootDeviceEnvironments(2);
    executionEnvironment.rootDeviceEnvironments[1]->setHwInfo(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[1]->initGmm();
    executionEnvironment.rootDeviceEnvironments[1]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment.rootDeviceEnvironments[1]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(new DrmMockCustom(*executionEnvironment.rootDeviceEnvironments[0])));
    auto csr = std::make_unique<MockDrmCsr<FamilyType>>(executionEnvironment, 1, 1, gemCloseWorkerMode::gemCloseWorkerActive);
    auto pageTableManager = csr->createPageTableManager();
    EXPECT_EQ(csr->pageTableManager.get(), pageTableManager);
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, givenLocalMemoryEnabledWhenCreatingDrmCsrThenEnableBatching) {
    {
        DebugManagerStateRestore restore;
        DebugManager.flags.EnableLocalMemory.set(1);

        MockDrmCsr<FamilyType> csr1(executionEnvironment, 0, 1, gemCloseWorkerMode::gemCloseWorkerInactive);
        EXPECT_EQ(DispatchMode::BatchedDispatch, csr1.dispatchMode);

        DebugManager.flags.CsrDispatchMode.set(static_cast<int32_t>(DispatchMode::ImmediateDispatch));
        MockDrmCsr<FamilyType> csr2(executionEnvironment, 0, 1, gemCloseWorkerMode::gemCloseWorkerInactive);
        EXPECT_EQ(DispatchMode::ImmediateDispatch, csr2.dispatchMode);
    }

    {
        DebugManagerStateRestore restore;
        DebugManager.flags.EnableLocalMemory.set(0);

        MockDrmCsr<FamilyType> csr1(executionEnvironment, 0, 1, gemCloseWorkerMode::gemCloseWorkerInactive);
        EXPECT_EQ(DispatchMode::ImmediateDispatch, csr1.dispatchMode);

        DebugManager.flags.CsrDispatchMode.set(static_cast<int32_t>(DispatchMode::BatchedDispatch));
        MockDrmCsr<FamilyType> csr2(executionEnvironment, 0, 1, gemCloseWorkerMode::gemCloseWorkerInactive);
        EXPECT_EQ(DispatchMode::BatchedDispatch, csr2.dispatchMode);
    }
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, givenPageTableManagerAndMapTrueWhenUpdateAuxTableIsCalledThenItReturnsTrue) {
    auto mockMngr = new MockGmmPageTableMngr();
    csr->pageTableManager.reset(mockMngr);
    executionEnvironment.rootDeviceEnvironments[0]->initGmm();
    auto gmm = std::make_unique<MockGmm>(executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper());
    auto result = csr->pageTableManager->updateAuxTable(0, gmm.get(), true);
    EXPECT_EQ(0ull, mockMngr->updateAuxTableParamsPassed[0].ddiUpdateAuxTable.BaseGpuVA);
    EXPECT_EQ(gmm->gmmResourceInfo->peekHandle(), mockMngr->updateAuxTableParamsPassed[0].ddiUpdateAuxTable.BaseResInfo);
    EXPECT_EQ(true, mockMngr->updateAuxTableParamsPassed[0].ddiUpdateAuxTable.DoNotWait);
    EXPECT_EQ(1u, mockMngr->updateAuxTableParamsPassed[0].ddiUpdateAuxTable.Map);

    EXPECT_TRUE(result);
    EXPECT_EQ(1u, mockMngr->updateAuxTableCalled);
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, givenPageTableManagerAndMapFalseWhenUpdateAuxTableIsCalledThenItReturnsTrue) {
    auto mockMngr = new MockGmmPageTableMngr();
    csr->pageTableManager.reset(mockMngr);
    executionEnvironment.rootDeviceEnvironments[0]->initGmm();
    auto gmm = std::make_unique<MockGmm>(executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper());
    auto result = csr->pageTableManager->updateAuxTable(0, gmm.get(), false);
    EXPECT_EQ(0ull, mockMngr->updateAuxTableParamsPassed[0].ddiUpdateAuxTable.BaseGpuVA);
    EXPECT_EQ(gmm->gmmResourceInfo->peekHandle(), mockMngr->updateAuxTableParamsPassed[0].ddiUpdateAuxTable.BaseResInfo);
    EXPECT_EQ(true, mockMngr->updateAuxTableParamsPassed[0].ddiUpdateAuxTable.DoNotWait);
    EXPECT_EQ(0u, mockMngr->updateAuxTableParamsPassed[0].ddiUpdateAuxTable.Map);

    EXPECT_TRUE(result);
    EXPECT_EQ(1u, mockMngr->updateAuxTableCalled);
}
