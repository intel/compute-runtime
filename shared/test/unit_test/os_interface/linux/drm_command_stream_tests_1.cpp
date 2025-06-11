/*
 * Copyright (C) 2018-2025 Intel Corporation
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
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_command_stream.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler_default.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/sys_calls_common.h"
#include "shared/test/common/helpers/batch_buffer_helper.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/linux/mock_drm_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_gmm_page_table_mngr.h"
#include "shared/test/common/mocks/mock_submissions_aggregator.h"
#include "shared/test/common/os_interface/linux/drm_command_stream_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

using namespace NEO;

namespace NEO {
namespace SysCalls {
extern bool exitCalled;
extern int latestExitCode;
} // namespace SysCalls
} // namespace NEO

HWTEST_TEMPLATED_F(DrmCommandStreamTest, givenFlushStampWhenWaitCalledThenWaitForSpecifiedBoHandle) {
    FlushStamp handleToWait = 123;
    GemWait expectedWait = {};
    expectedWait.boHandle = static_cast<uint32_t>(handleToWait);
    expectedWait.timeoutNs = -1;

    csr->waitForFlushStamp(handleToWait);
    EXPECT_TRUE(memcmp(&expectedWait, &mock->receivedGemWait, sizeof(GemWait)) == 0);
    EXPECT_EQ(1, mock->ioctlCount.gemWait);
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, givenDebugFlagSetWhenSubmittingThenCallExit) {
    uint32_t expectedExitCounter = 13;

    debugManager.flags.ExitOnSubmissionNumber.set(expectedExitCounter);
    debugManager.flags.ForcePreemptionMode.set(PreemptionMode::Disabled);

    csr->initializeTagAllocation();

    auto &cs = csr->getCS();
    IndirectHeap ih(cs.getGraphicsAllocation());
    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

    executionEnvironment.incRefInternal();
    std::unique_ptr<MockDevice> device(MockDevice::create<MockDevice>(&executionEnvironment, 0));

    bool bcsSupported = false;
    for (auto &engine : csr->getGfxCoreHelper().getGpgpuEngineInstances(device->getRootDeviceEnvironment())) {
        if (engine.first == aub_stream::EngineType::ENGINE_BCS) {
            bcsSupported = true;
            break;
        }
    }

    for (int32_t mode : {0, 1, 2}) {
        debugManager.flags.ExitOnSubmissionMode.set(mode);

        for (auto engineType : {aub_stream::ENGINE_BCS, EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::ENGINE_RCS, device->getRootDeviceEnvironment())}) {
            if (engineType == aub_stream::ENGINE_BCS && !bcsSupported) {
                continue;
            }

            osContext = std::make_unique<OsContextLinux>(*mock, 0, 0,
                                                         EngineDescriptorHelper::getDefaultDescriptor({engineType, EngineUsage::regular}, PreemptionMode::ThreadGroup));

            osContext->ensureContextInitialized(false);

            csr->setupContext(*osContext);
            static_cast<MockDrmCsr<FamilyType> *>(csr)->taskCount = 0;

            for (uint32_t i = 0; i <= expectedExitCounter + 3; i++) {
                SysCalls::exitCalled = false;

                csr->flushTask(cs, 16u, &ih, &ih, &ih, 0u, dispatchFlags, *device);

                bool enabled = (i >= expectedExitCounter);

                if (mode == 1 && !EngineHelpers::isComputeEngine(engineType)) {
                    enabled = false;
                }

                if (mode == 2 && !EngineHelpers::isBcs(engineType)) {
                    enabled = false;
                }

                if (enabled) {
                    EXPECT_TRUE(SysCalls::exitCalled);
                    EXPECT_EQ(0, SysCalls::latestExitCode);
                } else {
                    EXPECT_FALSE(SysCalls::exitCalled);
                }
            }
        }
    }
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, WhenMakingResidentThenSucceeds) {
    DrmAllocation graphicsAllocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, nullptr, 1024, static_cast<osHandle>(1u), MemoryPool::memoryNull);
    csr->makeResident(graphicsAllocation);

    EXPECT_EQ(0, mock->ioctlCount.gemUserptr);
    EXPECT_EQ(0, mock->ioctlCount.execbuffer2);

    mock->ioctlTearDownExpected.gemWait = 0;
    mock->ioctlTearDownExpected.gemClose = 0;
    mock->ioctlTearDownExpects = true;
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, WhenMakingResidentTwiceThenSucceeds) {
    DrmAllocation graphicsAllocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, nullptr, 1024, static_cast<osHandle>(1u), MemoryPool::memoryNull);

    csr->makeResident(graphicsAllocation);
    csr->makeResident(graphicsAllocation);

    EXPECT_EQ(0, mock->ioctlCount.gemUserptr);
    EXPECT_EQ(0, mock->ioctlCount.execbuffer2);

    mock->ioctlTearDownExpected.gemWait = 0;
    mock->ioctlTearDownExpected.gemClose = 0;
    mock->ioctlTearDownExpects = true;
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, GivenSizeZeroWhenMakingResidentTwiceThenSucceeds) {
    DrmAllocation graphicsAllocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, nullptr, 0, static_cast<osHandle>(1u), MemoryPool::memoryNull);

    csr->makeResident(graphicsAllocation);

    EXPECT_EQ(0, mock->ioctlCount.gemUserptr);
    EXPECT_EQ(0, mock->ioctlCount.execbuffer2);

    mock->ioctlTearDownExpected.gemWait = 0;
    mock->ioctlTearDownExpected.gemClose = 0;
    mock->ioctlTearDownExpects = true;
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, GivenResizedWhenMakingResidentTwiceThenSucceeds) {
    DrmAllocation graphicsAllocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, nullptr, 1024, static_cast<osHandle>(1u), MemoryPool::memoryNull);
    DrmAllocation graphicsAllocation2(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, nullptr, 8192, static_cast<osHandle>(1u), MemoryPool::memoryNull);

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
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    auto availableSpacePriorToFlush = cs.getAvailableSpace();
    csr->flush(batchBuffer, csr->getResidencyAllocations());
    EXPECT_EQ(static_cast<uint64_t>(boHandle), csr->obtainCurrentFlushStamp());
    EXPECT_NE(cs.getCpuBase(), nullptr);
    EXPECT_EQ(availableSpacePriorToFlush, cs.getAvailableSpace());

    mock->ioctlTearDownExpected.gemWait = 1;
    mock->ioctlTearDownExpected.gemClose = 1;
    mock->ioctlTearDownExpects = true;

    EXPECT_EQ(1, mock->ioctlCount.gemUserptr);
    EXPECT_EQ(1, mock->ioctlCount.execbuffer2);

    EXPECT_EQ(0u, mock->execBuffers.back().getBatchStartOffset());
    EXPECT_EQ(expectedSize, mock->execBuffers.back().getBatchLen());
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, givenPrintIndicesEnabledWhenFlushThenPrintIndices) {
    DebugManagerStateRestore restorer;
    debugManager.flags.PrintDeviceAndEngineIdOnSubmission.set(true);

    auto &cs = csr->getCS();
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    StreamCapture capture;
    capture.captureStdout();
    csr->flush(batchBuffer, csr->getResidencyAllocations());
    const std::string engineType = EngineHelpers::engineTypeToString(csr->getOsContext().getEngineType());
    const std::string engineUsage = EngineHelpers::engineUsageToString(csr->getOsContext().getEngineUsage());
    std::ostringstream expectedValue;
    expectedValue << SysCalls::getProcessId() << ": Submission to RootDevice Index: " << csr->getRootDeviceIndex()
                  << ", Sub-Devices Mask: " << csr->getOsContext().getDeviceBitfield().to_ulong()
                  << ", EngineId: " << csr->getOsContext().getEngineType()
                  << " (" << engineType << ", " << engineUsage << ")\n";
    EXPECT_TRUE(hasSubstr(capture.getCapturedStdout(), expectedValue.str()));
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, givenDrmContextIdWhenFlushingThenSetIdToAllExecBuffersAndObjects) {
    uint32_t expectedDrmContextId = 321;
    uint32_t numAllocations = 3;

    auto allocation1 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    auto allocation2 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    csr->makeResident(*allocation1);
    csr->makeResident(*allocation2);

    mock->storedDrmContextId = expectedDrmContextId;

    auto &gfxCoreHelper = csr->getGfxCoreHelper();
    osContext = std::make_unique<OsContextLinux>(*mock, csr->getRootDeviceIndex(), 1,
                                                 EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*csr->peekExecutionEnvironment().rootDeviceEnvironments[0])[0],
                                                                                              PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo)));
    osContext->ensureContextInitialized(false);
    csr->setupContext(*osContext);

    auto &cs = csr->getCS();

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
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
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    csr->flush(batchBuffer, csr->getResidencyAllocations());
    EXPECT_NE(cs.getCpuBase(), nullptr);

    EXPECT_EQ(1, mock->ioctlCount.gemUserptr);
    EXPECT_EQ(1, mock->ioctlCount.execbuffer2);

    mock->ioctlTearDownExpected.gemWait = 1;
    mock->ioctlTearDownExpected.gemClose = 1;
    mock->ioctlTearDownExpects = true;

    EXPECT_EQ(0u, mock->execBuffers.back().getBatchStartOffset());
    EXPECT_EQ(expectedSize, mock->execBuffers.back().getBatchLen());
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, GivenInvalidAddressWhenFlushingThenSucceeds) {
    // allocate command buffer manually
    char *commandBuffer = new (std::nothrow) char[1024];
    ASSERT_NE(nullptr, commandBuffer); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(commandBuffer));
    DrmAllocation commandBufferAllocation(0, 1u /*num gmms*/, AllocationType::commandBuffer, nullptr, commandBuffer, 1024, static_cast<osHandle>(1u), MemoryPool::memoryNull, canonizedGpuAddress);
    LinearStream cs(&commandBufferAllocation);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
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
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    csr->flush(batchBuffer, csr->getResidencyAllocations());

    EXPECT_EQ(1, mock->ioctlCount.gemUserptr);
    EXPECT_EQ(1, mock->ioctlCount.execbuffer2);

    mock->ioctlTearDownExpected.gemWait = 1;
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
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    csr->flush(batchBuffer, csr->getResidencyAllocations());

    EXPECT_EQ(1, mock->ioctlCount.gemUserptr);
    EXPECT_EQ(1, mock->ioctlCount.execbuffer2);

    mock->ioctlTearDownExpected.gemWait = 1;
    mock->ioctlTearDownExpected.gemClose = 1;
    mock->ioctlTearDownExpects = true;

    EXPECT_EQ(0u, mock->execBuffers.back().getBatchStartOffset());
    EXPECT_EQ(bbUsed + 4, mock->execBuffers.back().getBatchLen());
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, GivenNotAlignedWhenFlushingThenSucceeds) {
    auto &cs = csr->getCS();
    auto commandBuffer = static_cast<DrmAllocation *>(cs.getGraphicsAllocation());

    // make sure command buffer with offset is not page aligned
    ASSERT_NE(0u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & (this->alignment - 1));
    ASSERT_EQ(4u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & 0x7F);

    auto expectedBatchStartOffset = (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & (this->alignment - 1);
    auto expectedSize = alignUp(8u, MemoryConstants::cacheLineSize); // bbEnd

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    batchBuffer.startOffset = 4;
    csr->flush(batchBuffer, csr->getResidencyAllocations());

    EXPECT_EQ(1, mock->ioctlCount.gemUserptr);
    EXPECT_EQ(1, mock->ioctlCount.execbuffer2);

    mock->ioctlTearDownExpected.gemWait = 1;
    mock->ioctlTearDownExpected.gemClose = 1;
    mock->ioctlTearDownExpects = true;

    EXPECT_EQ(expectedBatchStartOffset, mock->execBuffers.back().getBatchStartOffset());
    EXPECT_EQ(expectedSize, mock->execBuffers.back().getBatchLen());
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, GivenCheckFlagsWhenFlushingThenSucceeds) {
    auto &cs = csr->getCS();

    DrmAllocation allocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, (void *)0x7FFFFFFF, 1024, static_cast<osHandle>(0u), MemoryPool::memoryNull);
    DrmAllocation allocation2(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, (void *)0x307FFFFFFF, 1024, static_cast<osHandle>(0u), MemoryPool::memoryNull);
    csr->makeResident(allocation);
    csr->makeResident(allocation2);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    csr->flush(batchBuffer, csr->getResidencyAllocations());

    EXPECT_EQ(1, mock->ioctlCount.execbuffer2);
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, GivenCheckDrmFreeWhenFlushingThenSucceeds) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableL3FlushAfterPostSync.set(0);
    mock->returnHandle = 17;

    auto &cs = csr->getCS();
    auto commandBuffer = static_cast<DrmAllocation *>(cs.getGraphicsAllocation());

    // make sure command buffer with offset is not page aligned
    ASSERT_NE(0u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & (this->alignment - 1));
    ASSERT_EQ(4u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & 0x7F);

    auto expectedBatchStartOffset = (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & (this->alignment - 1);
    auto expectedSize = alignUp(8u, MemoryConstants::cacheLineSize); // bbEnd

    DrmAllocation allocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, nullptr, 1024, static_cast<osHandle>(0u), MemoryPool::memoryNull);

    csr->makeResident(allocation);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    batchBuffer.startOffset = 4;
    csr->flush(batchBuffer, csr->getResidencyAllocations());

    EXPECT_EQ(1, mock->ioctlCount.gemUserptr);
    EXPECT_EQ(1, mock->ioctlCount.execbuffer2);

    mock->ioctlTearDownExpected.gemClose = 1;
    mock->ioctlTearDownExpected.gemWait = 1;
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

    // make sure command buffer with offset is not page aligned
    ASSERT_NE(0u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & (this->alignment - 1));
    ASSERT_EQ(4u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & 0x7F);

    auto expectedBatchStartOffset = (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & (this->alignment - 1);
    auto expectedSize = alignUp(8u, MemoryConstants::cacheLineSize); // bbEnd

    mock->storedRetValForGemClose = -1;

    DrmAllocation allocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, nullptr, 1024, static_cast<osHandle>(0u), MemoryPool::memoryNull);

    csr->makeResident(allocation);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    batchBuffer.startOffset = 4;
    csr->flush(batchBuffer, csr->getResidencyAllocations());

    EXPECT_EQ(1, mock->ioctlCount.gemUserptr);
    EXPECT_EQ(1, mock->ioctlCount.execbuffer2);

    mock->ioctlTearDownExpected.gemClose = 1;
    mock->ioctlTearDownExpected.gemWait = 1;
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

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    csr->flush(batchBuffer, csr->getResidencyAllocations());

    int ioctlExecCnt = 1;
    int ioctlUserPtrCnt = 2;

    auto engineFlag = static_cast<OsContextLinux &>(csr->getOsContext()).getEngineFlag();

    EXPECT_EQ(ioctlExecCnt + ioctlUserPtrCnt, this->mock->ioctlCnt.total);
    EXPECT_EQ(ioctlExecCnt, this->mock->ioctlCnt.execbuffer2);
    EXPECT_EQ(ioctlUserPtrCnt, this->mock->ioctlCnt.gemUserptr);
    uint64_t flags = engineFlag | I915_EXEC_NO_RELOC;
    EXPECT_EQ(flags, this->mock->execBuffer.getFlags());

    mm->freeGraphicsMemory(dummyAllocation);
    mm->freeGraphicsMemory(commandBuffer);
}

HWTEST_TEMPLATED_F(DrmCommandStreamBatchingTests, givenCsrWhenDispatchPolicyIsSetToBatchingThenCommandBufferIsNotSubmitted) {
    mock->reset();

    csr->overrideDispatchPolicy(DispatchMode::batchedDispatch);

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    auto testedCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    testedCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);
    testedCsr->useNewResourceImplicitFlush = false;
    testedCsr->useGpuIdleImplicitFlush = false;
    testedCsr->heaplessStateInitialized = true;

    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    auto dummyAllocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) & 0xFFF);
    IndirectHeap cs(commandBuffer);
    cs.getSpace(4u); // use some bytes

    csr->makeResident(*dummyAllocation);

    auto allocations = device->getDefaultEngine().commandStreamReceiver->getTagsMultiAllocation();

    csr->setTagAllocation(static_cast<DrmAllocation *>(allocations->getGraphicsAllocation(csr->getRootDeviceIndex())));

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(device->getHardwareInfo());

    csr->flushTask(cs, 0u, &cs, &cs, &cs, 0u, dispatchFlags, *device);

    // make sure command buffer is recorded
    auto &cmdBuffers = mockedSubmissionsAggregator->peekCommandBuffers();
    EXPECT_FALSE(cmdBuffers.peekIsEmpty());
    EXPECT_NE(nullptr, cmdBuffers.peekHead());

    // preemption allocation
    size_t csrSurfaceCount = (device->getPreemptionMode() == PreemptionMode::MidThread) ? 2 : 0;
    csrSurfaceCount += testedCsr->globalFenceAllocation ? 1 : 0;
    csrSurfaceCount += testedCsr->clearColorAllocation ? 1 : 0;
    csrSurfaceCount += device->getRTMemoryBackedBuffer() ? 1u : 0u;

    auto recordedCmdBuffer = cmdBuffers.peekHead();
    EXPECT_EQ(3u + csrSurfaceCount, recordedCmdBuffer->surfaces.size());

    // try to find all allocations
    auto elementInVector = std::find(recordedCmdBuffer->surfaces.begin(), recordedCmdBuffer->surfaces.end(), dummyAllocation);
    EXPECT_NE(elementInVector, recordedCmdBuffer->surfaces.end());

    elementInVector = std::find(recordedCmdBuffer->surfaces.begin(), recordedCmdBuffer->surfaces.end(), commandBuffer);
    EXPECT_NE(elementInVector, recordedCmdBuffer->surfaces.end());

    elementInVector = std::find(recordedCmdBuffer->surfaces.begin(), recordedCmdBuffer->surfaces.end(), allocations->getGraphicsAllocation(0u));
    EXPECT_NE(elementInVector, recordedCmdBuffer->surfaces.end());

    if (testedCsr->getHeaplessStateInitEnabled()) {
        EXPECT_EQ(cs.getGraphicsAllocation(), recordedCmdBuffer->batchBuffer.commandBufferAllocation);
    } else {
        EXPECT_EQ(testedCsr->commandStream.getGraphicsAllocation(), recordedCmdBuffer->batchBuffer.commandBufferAllocation);
    }

    int ioctlUserPtrCnt = 3;
    ioctlUserPtrCnt += testedCsr->clearColorAllocation ? 1 : 0;

    EXPECT_EQ(ioctlUserPtrCnt, this->mock->ioctlCnt.total);
    EXPECT_EQ(ioctlUserPtrCnt, this->mock->ioctlCnt.gemUserptr);

    EXPECT_EQ(0u, this->mock->execBuffer.getFlags());

    csr->flushBatchedSubmissions();

    mm->freeGraphicsMemory(dummyAllocation);
    mm->freeGraphicsMemory(commandBuffer);
}

HWTEST_TEMPLATED_F(DrmCommandStreamBatchingTests, givenRecordedCommandBufferWhenItIsSubmittedThenFlushTaskIsProperlyCalled) {
    mock->reset();
    csr->overrideDispatchPolicy(DispatchMode::batchedDispatch);

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    auto testedCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    testedCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);
    testedCsr->useNewResourceImplicitFlush = false;
    testedCsr->useGpuIdleImplicitFlush = false;
    testedCsr->heaplessStateInitialized = true;

    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    IndirectHeap cs(commandBuffer);
    // use some bytes
    cs.getSpace(4u);
    auto allocations = device->getDefaultEngine().commandStreamReceiver->getTagsMultiAllocation();

    csr->setTagAllocation(static_cast<DrmAllocation *>(allocations->getGraphicsAllocation(csr->getRootDeviceIndex())));

    auto &submittedCommandBuffer = csr->getCS(1024);
    // use some bytes
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

    if (!csr->getHeaplessStateInitEnabled()) {
        auto commandBufferGraphicsAllocation = submittedCommandBuffer.getGraphicsAllocation();
        EXPECT_TRUE(commandBufferGraphicsAllocation->isResident(csr->getOsContext().getContextId()));
    }

    // preemption allocation
    size_t csrSurfaceCount = (device->getPreemptionMode() == PreemptionMode::MidThread) ? 2 : 0;
    csrSurfaceCount += testedCsr->globalFenceAllocation ? 1 : 0;
    csrSurfaceCount += testedCsr->clearColorAllocation ? 1 : 0;
    csrSurfaceCount += device->getRTMemoryBackedBuffer() ? 1u : 0u;

    // validate that submited command buffer has what we want
    EXPECT_EQ(3u + csrSurfaceCount, this->mock->execBuffer.getBufferCount());

    EXPECT_EQ(csr->getHeaplessStateInitEnabled() ? 0u : 4u, this->mock->execBuffer.getBatchStartOffset());
    EXPECT_EQ(csr->getHeaplessStateInitEnabled() ? cs.getUsed() : submittedCommandBuffer.getUsed(), this->mock->execBuffer.getBatchLen());

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
    int ioctlUserPtrCnt = 2;
    ioctlUserPtrCnt += testedCsr->clearColorAllocation ? 1 : 0;
    EXPECT_EQ(ioctlExecCnt, this->mock->ioctlCnt.execbuffer2);
    EXPECT_EQ(ioctlUserPtrCnt, this->mock->ioctlCnt.gemUserptr);
    EXPECT_EQ(ioctlExecCnt + ioctlUserPtrCnt, this->mock->ioctlCnt.total);

    mm->freeGraphicsMemory(commandBuffer);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenProcessResidencyFailingOnOutOfMemoryWhenFlushingThenFlushReturnsOutOfMemory) {
    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    LinearStream cs(commandBuffer);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    auto allocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    executionEnvironment->rootDeviceEnvironments[csr->getRootDeviceIndex()]->memoryOperationsInterface->makeResident(device.get(), ArrayRef<GraphicsAllocation *>(&allocation, 1), false, false);

    auto testedCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    testedCsr->processResidencyCallBase = false;
    testedCsr->processResidencyResult = SubmissionStatus::outOfMemory;
    auto flushInternalCalledBeforeFlush = testedCsr->flushInternalCalled;
    auto processResidencyCalledBeforeFlush = testedCsr->processResidencyCalled;

    SubmissionStatus ret = csr->flush(batchBuffer, csr->getResidencyAllocations());
    EXPECT_EQ(SubmissionStatus::outOfMemory, ret);
    EXPECT_EQ(testedCsr->flushInternalCalled, flushInternalCalledBeforeFlush + 1);
    EXPECT_EQ(testedCsr->processResidencyCalled, processResidencyCalledBeforeFlush + 1);
    mm->freeGraphicsMemory(allocation);
    mm->freeGraphicsMemory(commandBuffer);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenProcessResidencyFailingOnOutOfHostMemoryWhenFlushingThenFlushReturnsOutOfHostMemory) {
    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    LinearStream cs(commandBuffer);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    auto allocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    executionEnvironment->rootDeviceEnvironments[csr->getRootDeviceIndex()]->memoryOperationsInterface->makeResident(device.get(), ArrayRef<GraphicsAllocation *>(&allocation, 1), false, false);

    auto testedCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    testedCsr->processResidencyCallBase = false;
    testedCsr->processResidencyResult = SubmissionStatus::outOfHostMemory;
    auto flushInternalCalledBeforeFlush = testedCsr->flushInternalCalled;
    auto processResidencyCalledBeforeFlush = testedCsr->processResidencyCalled;

    SubmissionStatus ret = csr->flush(batchBuffer, csr->getResidencyAllocations());
    EXPECT_EQ(SubmissionStatus::outOfHostMemory, ret);
    EXPECT_EQ(testedCsr->flushInternalCalled, flushInternalCalledBeforeFlush + 1);
    EXPECT_EQ(testedCsr->processResidencyCalled, processResidencyCalledBeforeFlush + 1);
    mm->freeGraphicsMemory(allocation);
    mm->freeGraphicsMemory(commandBuffer);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenFailingExecWhenFlushingThenFlushReturnsFailed) {
    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    LinearStream cs(commandBuffer);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    auto allocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    executionEnvironment->rootDeviceEnvironments[csr->getRootDeviceIndex()]->memoryOperationsInterface->makeResident(device.get(), ArrayRef<GraphicsAllocation *>(&allocation, 1), false, false);

    auto testedCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    testedCsr->processResidencyCallBase = true;
    testedCsr->processResidencyResult = SubmissionStatus::success;
    testedCsr->execCallBase = false;
    testedCsr->execResult = -1;
    auto flushInternalCalledBeforeFlush = testedCsr->flushInternalCalled;
    auto processResidencyCalledBeforeFlush = testedCsr->processResidencyCalled;
    auto execCalledBeforeFlush = testedCsr->execCalled;

    SubmissionStatus ret = csr->flush(batchBuffer, csr->getResidencyAllocations());
    EXPECT_EQ(SubmissionStatus::failed, ret);
    EXPECT_EQ(testedCsr->flushInternalCalled, flushInternalCalledBeforeFlush + 1);
    EXPECT_EQ(testedCsr->processResidencyCalled, processResidencyCalledBeforeFlush + 1);
    EXPECT_EQ(testedCsr->execCalled, execCalledBeforeFlush + 1);
    mm->freeGraphicsMemory(allocation);
    mm->freeGraphicsMemory(commandBuffer);
}

struct DrmCommandStreamBlitterDirectSubmissionTest : public DrmCommandStreamDirectSubmissionTest {
    template <typename GfxFamily>
    void setUpT() {
        debugManager.flags.DirectSubmissionOverrideBlitterSupport.set(1u);
        debugManager.flags.DirectSubmissionOverrideRenderSupport.set(0u);
        debugManager.flags.DirectSubmissionOverrideComputeSupport.set(0u);

        DrmCommandStreamDirectSubmissionTest::setUpT<GfxFamily>();
        executionEnvironment->incRefInternal();

        osContext.reset(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), 0, 0,
                                          EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular}, PreemptionMode::ThreadGroup, device->getDeviceBitfield())));
        osContext->ensureContextInitialized(false);

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
    MockDrmDirectSubmissionToTestDtor(const CommandStreamReceiver &commandStreamReceiver, DrmDirectSubmissionFunctionsCalled &functionsCalled)
        : DrmDirectSubmission<GfxFamily, RenderDispatcher<GfxFamily>>(commandStreamReceiver), functionsCalled(functionsCalled) {
    }
    ~MockDrmDirectSubmissionToTestDtor() override {
        if (ringStart) {
            stopRingBuffer(true); // NOLINT(clang-analyzer-optin.cplusplus.VirtualCall)
        }
        deallocateResources(); // NOLINT(clang-analyzer-optin.cplusplus.VirtualCall)
    }
    using DrmDirectSubmission<GfxFamily, RenderDispatcher<GfxFamily>>::ringStart;
    bool stopRingBuffer(bool blocking) override {
        functionsCalled.stopRingBuffer = true;
        return true;
    }
    void wait(TaskCountType taskCountToWait) override {
        functionsCalled.wait = true;
    }
    void deallocateResources() override {
        functionsCalled.deallocateResources = true;
    }
    DrmDirectSubmissionFunctionsCalled &functionsCalled;
};

HWTEST_TEMPLATED_F(DrmCommandStreamDirectSubmissionTest, givenEnabledDirectSubmissionWhenCheckingIsKmdWaitOnTaskCountAllowedThenTrueIsReturned) {
    *const_cast<bool *>(&static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->vmBindAvailable) = true;
    EXPECT_TRUE(csr->isDirectSubmissionEnabled());
    EXPECT_TRUE(csr->isKmdWaitOnTaskCountAllowed());
}

HWTEST_TEMPLATED_F(DrmCommandStreamDirectSubmissionTest, givenEnabledDirectSubmissionAndDisabledBindWhenCheckingIsKmdWaitOnTaskCountAllowedThenFalseIsReturned) {
    *const_cast<bool *>(&static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->vmBindAvailable) = false;
    EXPECT_TRUE(csr->isDirectSubmissionEnabled());
    EXPECT_FALSE(csr->isKmdWaitOnTaskCountAllowed());
}

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
    MockDrmDirectSubmissionToTestRingStop(const CommandStreamReceiver &commandStreamReceiver)
        : DrmDirectSubmission<GfxFamily, RenderDispatcher<GfxFamily>>(commandStreamReceiver) {
    }
    using DrmDirectSubmission<GfxFamily, RenderDispatcher<GfxFamily>>::ringStart;
};

HWTEST_TEMPLATED_F(DrmCommandStreamDirectSubmissionTest, givenEnabledDirectSubmissionWhenStopRingBufferIsCalledThenClearRingStart) {
    auto directSubmission = std::make_unique<MockDrmDirectSubmissionToTestRingStop<FamilyType>>(*device->getDefaultEngine().commandStreamReceiver);
    ASSERT_NE(nullptr, directSubmission);

    directSubmission->stopRingBuffer(false);
    EXPECT_FALSE(directSubmission->ringStart);
}

template <typename GfxFamily>
struct MockDrmDirectSubmissionDispatchCommandBuffer : public DrmDirectSubmission<GfxFamily, RenderDispatcher<GfxFamily>> {
    MockDrmDirectSubmissionDispatchCommandBuffer(const CommandStreamReceiver &commandStreamReceiver)
        : DrmDirectSubmission<GfxFamily, RenderDispatcher<GfxFamily>>(commandStreamReceiver) {
    }

    ADDMETHOD_NOBASE(dispatchCommandBuffer, bool, false,
                     (BatchBuffer & batchBuffer, FlushStampTracker &flushStamp));

    void setDispatchErrorCode(uint32_t errorCode) {
        this->dispatchErrorCode = errorCode;
    }
};

template <typename GfxFamily>
struct MockDrmBlitterDirectSubmissionDispatchCommandBuffer : public DrmDirectSubmission<GfxFamily, BlitterDispatcher<GfxFamily>> {
    MockDrmBlitterDirectSubmissionDispatchCommandBuffer(const CommandStreamReceiver &commandStreamReceiver)
        : DrmDirectSubmission<GfxFamily, BlitterDispatcher<GfxFamily>>(commandStreamReceiver) {
    }

    ADDMETHOD_NOBASE(dispatchCommandBuffer, bool, false,
                     (BatchBuffer & batchBuffer, FlushStampTracker &flushStamp));

    void setDispatchErrorCode(uint32_t errorCode) {
        this->dispatchErrorCode = errorCode;
    }
};

HWTEST_TEMPLATED_F(DrmCommandStreamDirectSubmissionTest, givenDirectSubmissionFailsThenFlushReturnsError) {
    auto testedCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    testedCsr->completionFenceValuePointer = nullptr;
    testedCsr->directSubmission = std::make_unique<MockDrmDirectSubmissionDispatchCommandBuffer<FamilyType>>(*device->getDefaultEngine().commandStreamReceiver);
    auto directSubmission = testedCsr->directSubmission.get();
    static_cast<MockDrmDirectSubmissionDispatchCommandBuffer<FamilyType> *>(directSubmission)->dispatchCommandBufferResult = false;
    static_cast<MockDrmDirectSubmissionDispatchCommandBuffer<FamilyType> *>(directSubmission)->setDispatchErrorCode(ENXIO);

    auto &cs = csr->getCS();
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    batchBuffer.startOffset = 4;
    uint8_t bbStart[64];
    batchBuffer.endCmdPtr = &bbStart[0];

    auto res = csr->flush(batchBuffer, csr->getResidencyAllocations());
    EXPECT_GT(static_cast<MockDrmDirectSubmissionDispatchCommandBuffer<FamilyType> *>(directSubmission)->dispatchCommandBufferCalled, 0u);
    EXPECT_NE(NEO::SubmissionStatus::success, res);
}

HWTEST_TEMPLATED_F(DrmCommandStreamBlitterDirectSubmissionTest, givenBlitterDirectSubmissionFailsThenFlushReturnsError) {
    auto testedCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    testedCsr->completionFenceValuePointer = nullptr;
    testedCsr->blitterDirectSubmission = std::make_unique<MockDrmBlitterDirectSubmissionDispatchCommandBuffer<FamilyType>>(*csr);
    auto blitterDirectSubmission = testedCsr->blitterDirectSubmission.get();
    static_cast<MockDrmBlitterDirectSubmissionDispatchCommandBuffer<FamilyType> *>(blitterDirectSubmission)->dispatchCommandBufferResult = false;
    static_cast<MockDrmBlitterDirectSubmissionDispatchCommandBuffer<FamilyType> *>(blitterDirectSubmission)->setDispatchErrorCode(ENXIO);

    auto &cs = csr->getCS();
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    batchBuffer.startOffset = 4;
    uint8_t bbStart[64];
    batchBuffer.endCmdPtr = &bbStart[0];

    auto res = csr->flush(batchBuffer, csr->getResidencyAllocations());
    EXPECT_GT(static_cast<MockDrmBlitterDirectSubmissionDispatchCommandBuffer<FamilyType> *>(blitterDirectSubmission)->dispatchCommandBufferCalled, 0u);
    EXPECT_NE(NEO::SubmissionStatus::success, res);
}

HWTEST_TEMPLATED_F(DrmCommandStreamDirectSubmissionTest, givenDirectSubmissionLightWhenNewResourcesAddedThenAddToResidencyContainer) {
    auto testedCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    testedCsr->completionFenceValuePointer = nullptr;
    testedCsr->directSubmission = std::make_unique<MockDrmDirectSubmissionDispatchCommandBuffer<FamilyType>>(*device->getDefaultEngine().commandStreamReceiver);
    testedCsr->heaplessStateInitialized = true;
    auto oldMemoryOperationsInterface = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface.release();
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = std::make_unique<DrmMemoryOperationsHandlerDefault>(device->getRootDeviceIndex());

    EXPECT_EQ(csr->getResidencyAllocations().size(), 0u);
    EXPECT_FALSE(static_cast<DrmMemoryOperationsHandlerDefault *>(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface.get())->obtainAndResetNewResourcesSinceLastRingSubmit());

    auto &cs = csr->getCS();
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    batchBuffer.startOffset = 4;
    uint8_t bbStart[64];
    batchBuffer.endCmdPtr = &bbStart[0];

    csr->flush(batchBuffer, csr->getResidencyAllocations());

    EXPECT_NE(csr->getResidencyAllocations().size(), 0u);
    EXPECT_TRUE(static_cast<DrmMemoryOperationsHandlerDefault *>(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface.get())->obtainAndResetNewResourcesSinceLastRingSubmit());

    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface.reset(oldMemoryOperationsInterface);
}

HWTEST_TEMPLATED_F(DrmCommandStreamDirectSubmissionTest, givenDirectSubmissionLightWhenMakeResidentThenDoNotAddToCsrResidencyContainer) {
    auto testedCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    testedCsr->directSubmission.reset();
    csr->initDirectSubmission();
    EXPECT_FALSE(testedCsr->pushAllocationsForMakeResident);
    EXPECT_EQ(testedCsr->getResidencyAllocations().size(), 0u);

    DrmAllocation graphicsAllocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, nullptr, 1024, static_cast<osHandle>(1u), MemoryPool::memoryNull);
    csr->makeResident(graphicsAllocation);

    EXPECT_EQ(testedCsr->getResidencyAllocations().size(), 0u);
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
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    batchBuffer.startOffset = 4;
    uint8_t bbStart[64];
    batchBuffer.endCmdPtr = &bbStart[0];
    static_cast<DrmMockCustom *>(static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->drm)->isVmBindAvailableCall.callParent = false;
    *const_cast<bool *>(&static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->vmBindAvailable) = true;

    auto flushStamp = csr->obtainCurrentFlushStamp();
    csr->flush(batchBuffer, csr->getResidencyAllocations());

    EXPECT_EQ(csr->obtainCurrentFlushStamp(), flushStamp);

    auto directSubmission = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->directSubmission.get();
    ASSERT_NE(nullptr, directSubmission);
    static_cast<MockDrmDirectSubmission<FamilyType> *>(directSubmission)->currentTagData.tagValue = 0u;
}

HWTEST_TEMPLATED_F(DrmCommandStreamDirectSubmissionTest, givenEnabledDirectSubmissionLightWhenFlushThenFlushStampIsUpdated) {
    auto &cs = csr->getCS();
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    batchBuffer.startOffset = 4;
    uint8_t bbStart[64];
    batchBuffer.endCmdPtr = &bbStart[0];

    auto flushStamp = csr->obtainCurrentFlushStamp();
    csr->flush(batchBuffer, csr->getResidencyAllocations());

    EXPECT_NE(csr->obtainCurrentFlushStamp(), flushStamp);

    auto directSubmission = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->directSubmission.get();
    ASSERT_NE(nullptr, directSubmission);
    static_cast<MockDrmDirectSubmission<FamilyType> *>(directSubmission)->currentTagData.tagValue = 0u;
}

HWTEST_TEMPLATED_F(DrmCommandStreamDirectSubmissionTest, givenEnabledDirectSubmissionWhenFlushThenCommandBufferAllocationIsNotAddedToHandlerResidencySet) {
    mock->bindAvailable = true;
    auto &cs = csr->getCS();
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    batchBuffer.startOffset = 4;
    uint8_t bbStart[64];
    batchBuffer.endCmdPtr = &bbStart[0];

    csr->flush(batchBuffer, csr->getResidencyAllocations());

    auto memoryOperationsInterface = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface.get();
    EXPECT_NE(memoryOperationsInterface->isResident(device.get(), *batchBuffer.commandBufferAllocation), MemoryOperationsStatus::success);

    auto directSubmission = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->directSubmission.get();
    ASSERT_NE(nullptr, directSubmission);
    static_cast<MockDrmDirectSubmission<FamilyType> *>(directSubmission)->currentTagData.tagValue = 0u;
}

HWTEST_TEMPLATED_F(DrmCommandStreamBlitterDirectSubmissionTest, givenEnabledDirectSubmissionOnBlitterWhenFlushThenFlushStampIsNotUpdated) {
    auto &cs = csr->getCS();
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    batchBuffer.startOffset = 4;
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
    executionEnvironment.rootDeviceEnvironments[1]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[1]->initGmm();
    executionEnvironment.rootDeviceEnvironments[1]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment.rootDeviceEnvironments[1]->osInterface->setDriverModel(DrmMockCustom::create(*executionEnvironment.rootDeviceEnvironments[0]));
    auto csr = std::make_unique<MockDrmCsr<FamilyType>>(executionEnvironment, 1, 1);
    auto pageTableManager = csr->createPageTableManager();
    EXPECT_EQ(csr->pageTableManager.get(), pageTableManager);
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, givenLocalMemoryEnabledWhenCreatingDrmCsrThenEnableBatching) {
    {
        DebugManagerStateRestore restore;
        debugManager.flags.EnableLocalMemory.set(1);

        MockDrmCsr<FamilyType> csr1(executionEnvironment, 0, 1);
        EXPECT_EQ(DispatchMode::batchedDispatch, csr1.dispatchMode);

        debugManager.flags.CsrDispatchMode.set(static_cast<int32_t>(DispatchMode::immediateDispatch));
        MockDrmCsr<FamilyType> csr2(executionEnvironment, 0, 1);
        EXPECT_EQ(DispatchMode::immediateDispatch, csr2.dispatchMode);
    }

    {
        DebugManagerStateRestore restore;
        debugManager.flags.EnableLocalMemory.set(0);

        MockDrmCsr<FamilyType> csr1(executionEnvironment, 0, 1);
        EXPECT_EQ(DispatchMode::immediateDispatch, csr1.dispatchMode);

        debugManager.flags.CsrDispatchMode.set(static_cast<int32_t>(DispatchMode::batchedDispatch));
        MockDrmCsr<FamilyType> csr2(executionEnvironment, 0, 1);
        EXPECT_EQ(DispatchMode::batchedDispatch, csr2.dispatchMode);
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

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenPrintBOsForSubmitWhenPrintThenProperValuesArePrinted) {
    DebugManagerStateRestore restorer;
    debugManager.flags.PrintBOsForSubmit.set(true);

    MockDrmAllocation allocation1(rootDeviceIndex, AllocationType::buffer, MemoryPool::system4KBPages);
    MockDrmAllocation allocation2(rootDeviceIndex, AllocationType::buffer, MemoryPool::system4KBPages);
    MockDrmAllocation cmdBuffer(rootDeviceIndex, AllocationType::commandBuffer, MemoryPool::system4KBPages);

    csr->makeResident(allocation1);
    csr->makeResident(allocation2);

    ResidencyContainer residency;
    residency.push_back(&allocation1);
    residency.push_back(&allocation2);

    StreamCapture capture;
    capture.captureStdout();

    EXPECT_EQ(SubmissionStatus::success, static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->printBOsForSubmit(residency, cmdBuffer));

    std::string output = capture.getCapturedStdout();

    std::vector<BufferObject *> bos;
    allocation1.makeBOsResident(&csr->getOsContext(), 0, &bos, true, false);
    allocation2.makeBOsResident(&csr->getOsContext(), 0, &bos, true, false);
    cmdBuffer.makeBOsResident(&csr->getOsContext(), 0, &bos, true, false);

    std::stringstream expected;
    expected << "Buffer object for submit\n";
    for (const auto &bo : bos) {
        expected << "BO-" << bo->peekHandle() << ", range: " << std::hex << bo->peekAddress() << " - " << ptrOffset(bo->peekAddress(), bo->peekSize()) << ", size: " << std::dec << bo->peekSize() << "\n";
    }
    expected << "\n";

    EXPECT_FALSE(output.compare(expected.str()));
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenPrintBOsForSubmitAndFailureOnMakeResidentForCmdBufferWhenPrintThenErrorIsReturnedAndNothingIsPrinted) {
    DebugManagerStateRestore restorer;
    debugManager.flags.PrintBOsForSubmit.set(true);

    MockDrmAllocation allocation1(rootDeviceIndex, AllocationType::buffer, MemoryPool::system4KBPages);
    MockDrmAllocation allocation2(rootDeviceIndex, AllocationType::buffer, MemoryPool::system4KBPages);
    MockDrmAllocation cmdBuffer(rootDeviceIndex, AllocationType::commandBuffer, MemoryPool::system4KBPages);

    cmdBuffer.makeBOsResidentResult = ENOSPC;

    csr->makeResident(allocation1);
    csr->makeResident(allocation2);

    ResidencyContainer residency;
    residency.push_back(&allocation1);
    residency.push_back(&allocation2);

    StreamCapture capture;
    capture.captureStdout();

    EXPECT_EQ(SubmissionStatus::outOfHostMemory, static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->printBOsForSubmit(residency, cmdBuffer));

    std::string output = capture.getCapturedStdout();
    EXPECT_TRUE(output.empty());
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenPrintBOsForSubmitAndFailureOnMakeResidentForAllocationToResidencyWhenPrintThenErrorIsReturnedAndNothingIsPrinted) {
    DebugManagerStateRestore restorer;
    debugManager.flags.PrintBOsForSubmit.set(true);

    MockDrmAllocation allocation1(rootDeviceIndex, AllocationType::buffer, MemoryPool::system4KBPages);
    MockDrmAllocation allocation2(rootDeviceIndex, AllocationType::buffer, MemoryPool::system4KBPages);
    MockDrmAllocation cmdBuffer(rootDeviceIndex, AllocationType::commandBuffer, MemoryPool::system4KBPages);

    allocation1.makeBOsResidentResult = ENOSPC;

    csr->makeResident(allocation1);
    csr->makeResident(allocation2);

    ResidencyContainer residency;
    residency.push_back(&allocation1);
    residency.push_back(&allocation2);

    StreamCapture capture;
    capture.captureStdout();

    EXPECT_EQ(SubmissionStatus::outOfHostMemory, static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->printBOsForSubmit(residency, cmdBuffer));

    std::string output = capture.getCapturedStdout();
    EXPECT_TRUE(output.empty());
}
