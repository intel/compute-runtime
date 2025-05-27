/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/tag_allocation_layout.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/test/common/helpers/batch_buffer_helper.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_host_ptr_manager.h"
#include "shared/test/common/os_interface/linux/drm_buffer_object_fixture.h"
#include "shared/test/common/os_interface/linux/drm_command_stream_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {
extern ApiSpecificConfig::ApiType apiTypeForUlts;
} // namespace NEO
using namespace NEO;

HWTEST_TEMPLATED_F(DrmCommandStreamTest, givenL0ApiConfigWhenCreatingDrmCsrThenEnableImmediateDispatch) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableL3FlushAfterPostSync.set(0);
    VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::L0);
    MockDrmCsr<FamilyType> csr(executionEnvironment, 0, 1);
    EXPECT_EQ(DispatchMode::immediateDispatch, csr.dispatchMode);
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, whenGettingCompletionValueThenTaskCountOfAllocationIsReturned) {
    MockGraphicsAllocation allocation{};
    uint32_t expectedValue = 0x1234;
    allocation.updateTaskCount(expectedValue, osContext->getContextId());
    EXPECT_EQ(expectedValue, csr->getCompletionValue(allocation));
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, givenEnabledDirectSubmissionWhenGettingCompletionValueThenCompletionFenceValueIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDrmCompletionFence.set(1);
    debugManager.flags.EnableDirectSubmission.set(1);
    debugManager.flags.DirectSubmissionDisableMonitorFence.set(0);
    MockDrmCsr<FamilyType> csr(executionEnvironment, 0, 1);
    csr.setupContext(*osContext);
    EXPECT_EQ(nullptr, csr.completionFenceValuePointer);

    auto hwInfo = executionEnvironment.rootDeviceEnvironments[0]->getMutableHardwareInfo();
    hwInfo->capabilityTable.directSubmissionEngines.data[osContext->getEngineType()].engineSupported = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[osContext->getEngineType()].submitOnInit = true;
    hwInfo->capabilityTable.directSubmissionEngines.data[osContext->getEngineType()].useNonDefault = true;
    csr.createGlobalFenceAllocation();
    csr.initializeTagAllocation();
    csr.initDirectSubmission();

    EXPECT_NE(nullptr, csr.completionFenceValuePointer);

    uint32_t expectedValue = 0x5678;
    *csr.completionFenceValuePointer = expectedValue;
    MockGraphicsAllocation allocation{};
    uint32_t notExpectedValue = 0x1234;
    allocation.updateTaskCount(notExpectedValue, osContext->getContextId());
    EXPECT_EQ(expectedValue, csr.getCompletionValue(allocation));
    *csr.completionFenceValuePointer = 0;
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, givenDisabledDirectSubmissionWhenCheckingIsKmdWaitOnTaskCountAllowedThenFalseIsReturned) {
    EXPECT_FALSE(csr->isDirectSubmissionEnabled());
    EXPECT_FALSE(csr->isKmdWaitOnTaskCountAllowed());
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, whenGettingCompletionAddressThenOffsettedTagAddressIsReturned) {
    csr->initializeTagAllocation();
    EXPECT_NE(nullptr, csr->getTagAddress());
    uint64_t tagAddress = castToUint64(const_cast<TagAddressType *>(csr->getTagAddress()));
    auto expectedAddress = tagAddress + TagAllocationLayout::completionFenceOffset;
    EXPECT_EQ(expectedAddress, csr->getCompletionAddress());
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, givenNoTagAddressWhenGettingCompletionAddressThenZeroIsReturned) {
    EXPECT_EQ(nullptr, csr->getTagAddress());
    EXPECT_EQ(0u, csr->getCompletionAddress());
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, GivenExecBufferErrorWhenFlushInternalThenProperErrorIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableL3FlushAfterPostSync.set(0);

    mock->execBufferResult = -1;
    mock->baseErrno = false;
    mock->errnoRetVal = EWOULDBLOCK;
    auto rootDeviceIndex = csr->getRootDeviceIndex();
    TestedBufferObject bo(rootDeviceIndex, mock, 128);
    MockDrmAllocation cmdBuffer(rootDeviceIndex, AllocationType::commandBuffer, MemoryPool::system4KBPages);
    cmdBuffer.bufferObjects[0] = &bo;
    uint8_t buff[128]{};

    LinearStream cs(&cmdBuffer, buff, 128);

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    auto ret = static_cast<MockDrmCsr<FamilyType> *>(csr)->flushInternal(batchBuffer, csr->getResidencyAllocations());
    EXPECT_EQ(SubmissionStatus::outOfHostMemory, ret);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenCommandStreamWhenItIsFlushedWithGemCloseWorkerInDefaultModeThenWorkerDecreasesTheRefCount) {
    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    auto storedBase = cs.getCpuBase();
    auto storedGraphicsAllocation = cs.getGraphicsAllocation();
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    csr->flush(batchBuffer, csr->getResidencyAllocations());
    EXPECT_EQ(cs.getCpuBase(), storedBase);
    EXPECT_EQ(cs.getGraphicsAllocation(), storedGraphicsAllocation);

    auto drmAllocation = static_cast<DrmAllocation *>(storedGraphicsAllocation);
    auto bo = drmAllocation->getBO();

    // spin until gem close worker finishes execution
    while (bo->getRefCount() > 1)
        ;

    mm->freeGraphicsMemory(commandBuffer);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenTaskThatRequiresLargeResourceCountWhenItIsFlushedThenExecStorageIsResized) {
    std::vector<GraphicsAllocation *> graphicsAllocations;

    auto &execStorage = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->execObjectsStorage;
    execStorage.resize(0);

    for (auto id = 0; id < 10; id++) {
        auto graphicsAllocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
        csr->makeResident(*graphicsAllocation);
        graphicsAllocations.push_back(graphicsAllocation);
    }
    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});

    LinearStream cs(commandBuffer);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    csr->flush(batchBuffer, csr->getResidencyAllocations());

    EXPECT_EQ(11u, this->mock->execBuffer.getBufferCount());
    mm->freeGraphicsMemory(commandBuffer);
    for (auto graphicsAllocation : graphicsAllocations) {
        mm->freeGraphicsMemory(graphicsAllocation);
    }
    EXPECT_EQ(11u, execStorage.size());
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenGemCloseWorkerInactiveModeWhenMakeResidentIsCalledThenRefCountsAreNotUpdated) {
    auto dummyAllocation = static_cast<DrmAllocation *>(mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize}));

    auto bo = dummyAllocation->getBO();
    EXPECT_EQ(1u, bo->getRefCount());

    csr->makeResident(*dummyAllocation);
    EXPECT_EQ(1u, bo->getRefCount());

    csr->processResidency(csr->getResidencyAllocations(), 0u);

    csr->makeNonResident(*dummyAllocation);
    EXPECT_EQ(1u, bo->getRefCount());

    mm->freeGraphicsMemory(dummyAllocation);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, GivenTwoAllocationsWhenBackingStorageIsDifferentThenMakeResidentShouldAddTwoLocations) {
    auto allocation = static_cast<DrmAllocation *>(mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize}));
    auto allocation2 = static_cast<DrmAllocation *>(mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize}));

    csr->makeResident(*allocation);
    csr->makeResident(*allocation2);

    auto osContextId = csr->getOsContext().getContextId();

    EXPECT_TRUE(allocation->isResident(osContextId));
    EXPECT_TRUE(allocation2->isResident(osContextId));

    csr->processResidency(csr->getResidencyAllocations(), 0u);

    EXPECT_TRUE(allocation->isResident(osContextId));
    EXPECT_TRUE(allocation2->isResident(osContextId));

    EXPECT_EQ(getResidencyVector<FamilyType>().size(), 2u);

    csr->makeNonResident(*allocation);
    csr->makeNonResident(*allocation2);

    EXPECT_FALSE(allocation->isResident(osContextId));
    EXPECT_FALSE(allocation2->isResident(osContextId));

    EXPECT_EQ(getResidencyVector<FamilyType>().size(), 0u);
    mm->freeGraphicsMemory(allocation);
    mm->freeGraphicsMemory(allocation2);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenCommandStreamWithDuplicatesWhenItIsFlushedWithGemCloseWorkerInactiveModeThenCsIsNotNulled) {
    auto commandBuffer = static_cast<DrmAllocation *>(mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize}));
    auto dummyAllocation = static_cast<DrmAllocation *>(mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize}));
    ASSERT_NE(nullptr, commandBuffer);
    ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) & 0xFFF);
    LinearStream cs(commandBuffer);

    csr->makeResident(*dummyAllocation);
    csr->makeResident(*dummyAllocation);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    auto storedBase = cs.getCpuBase();
    auto storedGraphicsAllocation = cs.getGraphicsAllocation();
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    csr->flush(batchBuffer, csr->getResidencyAllocations());
    EXPECT_EQ(cs.getCpuBase(), storedBase);
    EXPECT_EQ(cs.getGraphicsAllocation(), storedGraphicsAllocation);

    mm->freeGraphicsMemory(dummyAllocation);
    mm->freeGraphicsMemory(commandBuffer);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenDebugFlagSetWhenFlushingThenReadBackCommandBufferPointerIfRequired) {
    auto commandBuffer = static_cast<DrmAllocation *>(mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize}));
    LinearStream cs(commandBuffer);

    auto testedCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    {
        debugManager.flags.ReadBackCommandBufferAllocation.set(1);

        csr->flush(batchBuffer, csr->getResidencyAllocations());

        if (commandBuffer->isAllocatedInLocalMemoryPool()) {
            EXPECT_EQ(commandBuffer->getUnderlyingBuffer(), testedCsr->latestReadBackAddress);
        } else {
            EXPECT_EQ(nullptr, testedCsr->latestReadBackAddress);
        }

        testedCsr->latestReadBackAddress = nullptr;
    }

    {
        debugManager.flags.ReadBackCommandBufferAllocation.set(2);

        csr->flush(batchBuffer, csr->getResidencyAllocations());

        EXPECT_EQ(commandBuffer->getUnderlyingBuffer(), testedCsr->latestReadBackAddress);
    }

    mm->freeGraphicsMemory(commandBuffer);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenDrmAllocationWhenGetBufferObjectToModifyIsCalledForAGivenHandleIdThenTheCorrespondingBufferObjectGetsModified) {
    auto size = 1024u;
    auto allocation = new DrmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, nullptr, size, static_cast<osHandle>(0u), MemoryPool::memoryNull);

    auto &bos = allocation->getBOs();
    for (auto handleId = 0u; handleId < EngineLimits::maxHandleCount; handleId++) {
        EXPECT_EQ(nullptr, bos[handleId]);
    }

    for (auto handleId = 0u; handleId < EngineLimits::maxHandleCount; handleId++) {
        allocation->getBufferObjectToModify(handleId) = this->createBO(size);
    }

    for (auto handleId = 0u; handleId < EngineLimits::maxHandleCount; handleId++) {
        EXPECT_NE(nullptr, bos[handleId]);
    }

    mm->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, WhenMakingResidentThenSucceeds) {
    auto buffer = this->createBO(1024);
    auto allocation = new DrmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, buffer, nullptr, buffer->peekSize(), static_cast<osHandle>(0u), MemoryPool::memoryNull);
    EXPECT_EQ(nullptr, allocation->getUnderlyingBuffer());

    csr->makeResident(*allocation);
    csr->processResidency(csr->getResidencyAllocations(), 0u);

    EXPECT_TRUE(isResident<FamilyType>(buffer));
    EXPECT_EQ(1u, buffer->getRefCount());

    csr->makeNonResident(*allocation);
    EXPECT_FALSE(isResident<FamilyType>(buffer));
    EXPECT_EQ(1u, buffer->getRefCount());
    mm->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, GivenMultipleAllocationsWhenMakingResidentThenEachSucceeds) {
    BufferObject *buffer1 = this->createBO(4096);
    BufferObject *buffer2 = this->createBO(4096);
    auto allocation1 = new DrmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, buffer1, nullptr, buffer1->peekSize(), static_cast<osHandle>(0u), MemoryPool::memoryNull);
    auto allocation2 = new DrmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, buffer2, nullptr, buffer2->peekSize(), static_cast<osHandle>(0u), MemoryPool::memoryNull);
    EXPECT_EQ(nullptr, allocation1->getUnderlyingBuffer());
    EXPECT_EQ(nullptr, allocation2->getUnderlyingBuffer());

    csr->makeResident(*allocation1);
    csr->makeResident(*allocation2);
    csr->processResidency(csr->getResidencyAllocations(), 0u);

    EXPECT_TRUE(isResident<FamilyType>(buffer1));
    EXPECT_TRUE(isResident<FamilyType>(buffer2));
    EXPECT_EQ(1u, buffer1->getRefCount());
    EXPECT_EQ(1u, buffer2->getRefCount());

    // dont call makeNonResident on allocation2, any other makeNonResident call will clean this
    // we want to keep all makeResident calls before flush and makeNonResident everyting after flush
    csr->makeNonResident(*allocation1);

    mm->freeGraphicsMemory(allocation1);
    mm->freeGraphicsMemory(allocation2);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, WhenMakingResidentTwiceThenRefCountIsOne) {
    auto buffer = this->createBO(1024);
    auto allocation = new DrmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, buffer, nullptr, buffer->peekSize(), static_cast<osHandle>(0u), MemoryPool::memoryNull);

    csr->makeResident(*allocation);
    csr->processResidency(csr->getResidencyAllocations(), 0u);

    EXPECT_TRUE(isResident<FamilyType>(buffer));
    EXPECT_EQ(1u, buffer->getRefCount());

    csr->getResidencyAllocations().clear();
    csr->makeResident(*allocation);
    csr->processResidency(csr->getResidencyAllocations(), 0u);

    EXPECT_TRUE(isResident<FamilyType>(buffer));
    EXPECT_EQ(1u, buffer->getRefCount());

    csr->makeNonResident(*allocation);
    EXPECT_FALSE(isResident<FamilyType>(buffer));
    EXPECT_EQ(1u, buffer->getRefCount());
    mm->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, GivenFragmentStorageWhenMakingResidentTwiceThenRefCountIsOne) {
    auto ptr = (void *)0x1001;
    auto size = MemoryConstants::pageSize * 10;
    auto reqs = MockHostPtrManager::getAllocationRequirements(csr->getRootDeviceIndex(), ptr, size);
    auto allocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, size}, ptr);

    ASSERT_EQ(3u, allocation->fragmentsStorage.fragmentCount);

    csr->makeResident(*allocation);
    csr->makeResident(*allocation);

    csr->processResidency(csr->getResidencyAllocations(), 0u);
    for (int i = 0; i < maxFragmentsCount; i++) {
        ASSERT_EQ(allocation->fragmentsStorage.fragmentStorageData[i].cpuPtr,
                  reqs.allocationFragments[i].allocationPtr);
        auto bo = static_cast<OsHandleLinux *>(allocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage)->bo;
        EXPECT_TRUE(isResident<FamilyType>(bo));
        EXPECT_EQ(1u, bo->getRefCount());
    }

    csr->makeNonResident(*allocation);
    for (int i = 0; i < maxFragmentsCount; i++) {
        auto bo = static_cast<OsHandleLinux *>(allocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage)->bo;
        EXPECT_FALSE(isResident<FamilyType>(bo));
        EXPECT_EQ(1u, bo->getRefCount());
    }
    mm->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenFragmentedAllocationsWithResuedFragmentsWhenTheyAreMadeResidentThenFragmentsDoNotDuplicate) {
    mock->ioctlExpected.total = 9;
    // 3 fragments
    auto ptr = (void *)0x1001;
    auto size = MemoryConstants::pageSize * 10;
    auto graphicsAllocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, size}, ptr);

    auto offsetedPtr = (void *)((uintptr_t)ptr + size);
    auto size2 = MemoryConstants::pageSize - 1;

    auto graphicsAllocation2 = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, size2}, offsetedPtr);

    // graphicsAllocation2 reuses one fragment from graphicsAllocation
    EXPECT_EQ(graphicsAllocation->fragmentsStorage.fragmentStorageData[2].residency, graphicsAllocation2->fragmentsStorage.fragmentStorageData[0].residency);

    csr->makeResident(*graphicsAllocation);
    csr->makeResident(*graphicsAllocation2);

    csr->processResidency(csr->getResidencyAllocations(), 0u);

    auto &osContext = csr->getOsContext();

    EXPECT_TRUE(graphicsAllocation->fragmentsStorage.fragmentStorageData[0].residency->resident[osContext.getContextId()]);
    EXPECT_TRUE(graphicsAllocation->fragmentsStorage.fragmentStorageData[1].residency->resident[osContext.getContextId()]);
    EXPECT_TRUE(graphicsAllocation->fragmentsStorage.fragmentStorageData[2].residency->resident[osContext.getContextId()]);
    EXPECT_TRUE(graphicsAllocation2->fragmentsStorage.fragmentStorageData[0].residency->resident[osContext.getContextId()]);

    auto &residency = getResidencyVector<FamilyType>();

    EXPECT_EQ(3u, residency.size());

    csr->makeSurfacePackNonResident(csr->getResidencyAllocations(), true);

    // check that each packet is not resident
    EXPECT_FALSE(graphicsAllocation->fragmentsStorage.fragmentStorageData[0].residency->resident[osContext.getContextId()]);
    EXPECT_FALSE(graphicsAllocation->fragmentsStorage.fragmentStorageData[1].residency->resident[osContext.getContextId()]);
    EXPECT_FALSE(graphicsAllocation->fragmentsStorage.fragmentStorageData[2].residency->resident[osContext.getContextId()]);
    EXPECT_FALSE(graphicsAllocation2->fragmentsStorage.fragmentStorageData[0].residency->resident[osContext.getContextId()]);

    EXPECT_EQ(0u, residency.size());

    csr->makeResident(*graphicsAllocation);
    csr->makeResident(*graphicsAllocation2);

    csr->processResidency(csr->getResidencyAllocations(), 0u);

    EXPECT_TRUE(graphicsAllocation->fragmentsStorage.fragmentStorageData[0].residency->resident[osContext.getContextId()]);
    EXPECT_TRUE(graphicsAllocation->fragmentsStorage.fragmentStorageData[1].residency->resident[osContext.getContextId()]);
    EXPECT_TRUE(graphicsAllocation->fragmentsStorage.fragmentStorageData[2].residency->resident[osContext.getContextId()]);
    EXPECT_TRUE(graphicsAllocation2->fragmentsStorage.fragmentStorageData[0].residency->resident[osContext.getContextId()]);

    EXPECT_EQ(3u, residency.size());

    csr->makeSurfacePackNonResident(csr->getResidencyAllocations(), true);

    EXPECT_EQ(0u, residency.size());

    EXPECT_FALSE(graphicsAllocation->fragmentsStorage.fragmentStorageData[0].residency->resident[osContext.getContextId()]);
    EXPECT_FALSE(graphicsAllocation->fragmentsStorage.fragmentStorageData[1].residency->resident[osContext.getContextId()]);
    EXPECT_FALSE(graphicsAllocation->fragmentsStorage.fragmentStorageData[2].residency->resident[osContext.getContextId()]);
    EXPECT_FALSE(graphicsAllocation2->fragmentsStorage.fragmentStorageData[0].residency->resident[osContext.getContextId()]);

    mm->freeGraphicsMemory(graphicsAllocation);
    mm->freeGraphicsMemory(graphicsAllocation2);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, GivenAllocationCreatedFromThreeFragmentsWhenMakeResidentIsBeingCalledThenAllFragmentsAreMadeResident) {
    auto ptr = (void *)0x1001;
    auto size = MemoryConstants::pageSize * 10;

    auto reqs = MockHostPtrManager::getAllocationRequirements(csr->getRootDeviceIndex(), ptr, size);

    auto allocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, size}, ptr);

    ASSERT_EQ(3u, allocation->fragmentsStorage.fragmentCount);

    csr->makeResident(*allocation);
    csr->processResidency(csr->getResidencyAllocations(), 0u);

    for (int i = 0; i < maxFragmentsCount; i++) {
        ASSERT_EQ(allocation->fragmentsStorage.fragmentStorageData[i].cpuPtr,
                  reqs.allocationFragments[i].allocationPtr);
        auto bo = static_cast<OsHandleLinux *>(allocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage)->bo;
        EXPECT_TRUE(isResident<FamilyType>(bo));
        EXPECT_EQ(1u, bo->getRefCount());
    }
    csr->makeNonResident(*allocation);
    for (int i = 0; i < maxFragmentsCount; i++) {
        auto bo = static_cast<OsHandleLinux *>(allocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage)->bo;
        EXPECT_FALSE(isResident<FamilyType>(bo));
        EXPECT_EQ(1u, bo->getRefCount());
    }
    mm->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, GivenAllocationsContainingDifferentCountOfFragmentsWhenAllocationIsMadeResidentThenAllFragmentsAreMadeResident) {
    auto ptr = (void *)0x1001;
    auto size = MemoryConstants::pageSize;
    auto size2 = 100u;

    auto reqs = MockHostPtrManager::getAllocationRequirements(csr->getRootDeviceIndex(), ptr, size);

    auto allocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, size}, ptr);

    ASSERT_EQ(2u, allocation->fragmentsStorage.fragmentCount);
    ASSERT_EQ(2u, reqs.requiredFragmentsCount);

    csr->makeResident(*allocation);
    csr->processResidency(csr->getResidencyAllocations(), 0u);

    for (unsigned int i = 0; i < reqs.requiredFragmentsCount; i++) {
        ASSERT_EQ(allocation->fragmentsStorage.fragmentStorageData[i].cpuPtr,
                  reqs.allocationFragments[i].allocationPtr);
        auto bo = static_cast<OsHandleLinux *>(allocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage)->bo;
        EXPECT_TRUE(isResident<FamilyType>(bo));
        EXPECT_EQ(1u, bo->getRefCount());
    }
    csr->makeNonResident(*allocation);
    for (unsigned int i = 0; i < reqs.requiredFragmentsCount; i++) {
        auto bo = static_cast<OsHandleLinux *>(allocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage)->bo;
        EXPECT_FALSE(isResident<FamilyType>(bo));
        EXPECT_EQ(1u, bo->getRefCount());
    }
    mm->freeGraphicsMemory(allocation);
    csr->getResidencyAllocations().clear();

    auto allocation2 = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, size2}, ptr);
    reqs = MockHostPtrManager::getAllocationRequirements(csr->getRootDeviceIndex(), ptr, size2);

    ASSERT_EQ(1u, allocation2->fragmentsStorage.fragmentCount);
    ASSERT_EQ(1u, reqs.requiredFragmentsCount);

    csr->makeResident(*allocation2);
    csr->processResidency(csr->getResidencyAllocations(), 0u);

    for (unsigned int i = 0; i < reqs.requiredFragmentsCount; i++) {
        ASSERT_EQ(allocation2->fragmentsStorage.fragmentStorageData[i].cpuPtr,
                  reqs.allocationFragments[i].allocationPtr);
        auto bo = static_cast<OsHandleLinux *>(allocation2->fragmentsStorage.fragmentStorageData[i].osHandleStorage)->bo;
        EXPECT_TRUE(isResident<FamilyType>(bo));
        EXPECT_EQ(1u, bo->getRefCount());
    }
    csr->makeNonResident(*allocation2);
    for (unsigned int i = 0; i < reqs.requiredFragmentsCount; i++) {
        auto bo = static_cast<OsHandleLinux *>(allocation2->fragmentsStorage.fragmentStorageData[i].osHandleStorage)->bo;
        EXPECT_FALSE(isResident<FamilyType>(bo));
        EXPECT_EQ(1u, bo->getRefCount());
    }
    mm->freeGraphicsMemory(allocation2);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, GivenTwoAllocationsWhenBackingStorageIsTheSameThenMakeResidentShouldAddOnlyOneLocation) {
    auto ptr = (void *)0x1000;
    auto size = MemoryConstants::pageSize;
    auto ptr2 = (void *)0x1000;

    auto allocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, size}, ptr);
    auto allocation2 = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, size}, ptr2);

    csr->makeResident(*allocation);
    csr->makeResident(*allocation2);

    csr->processResidency(csr->getResidencyAllocations(), 0u);

    EXPECT_EQ(getResidencyVector<FamilyType>().size(), 1u);

    csr->makeNonResident(*allocation);
    csr->makeNonResident(*allocation2);

    mm->freeGraphicsMemory(allocation);
    mm->freeGraphicsMemory(allocation2);
    csr->getResidencyAllocations().clear();
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, WhenFlushingThenSucceeds) {
    auto &cs = csr->getCS();
    auto commandBuffer = static_cast<DrmAllocation *>(cs.getGraphicsAllocation());
    ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) & 0xFFF);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    csr->flush(batchBuffer, csr->getResidencyAllocations());
    EXPECT_NE(cs.getCpuBase(), nullptr);
    EXPECT_NE(cs.getGraphicsAllocation(), nullptr);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, WhenFlushNotCalledThenClearResidency) {
    auto allocation1 = static_cast<DrmAllocation *>(mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize}));
    auto allocation2 = static_cast<DrmAllocation *>(mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize}));
    ASSERT_NE(nullptr, allocation1);
    ASSERT_NE(nullptr, allocation2);

    EXPECT_EQ(getResidencyVector<FamilyType>().size(), 0u);
    csr->makeResident(*allocation1);
    csr->makeResident(*allocation2);
    csr->processResidency(csr->getResidencyAllocations(), 0u);

    EXPECT_TRUE(isResident<FamilyType>(allocation1->getBO()));
    EXPECT_TRUE(isResident<FamilyType>(allocation2->getBO()));
    EXPECT_EQ(getResidencyVector<FamilyType>().size(), 2u);

    EXPECT_EQ(allocation1->getBO()->getRefCount(), 1u);
    EXPECT_EQ(allocation2->getBO()->getRefCount(), 1u);

    // makeNonResident without flush
    csr->makeNonResident(*allocation1);
    EXPECT_EQ(getResidencyVector<FamilyType>().size(), 0u);

    // everything is nonResident after first call
    EXPECT_FALSE(isResident<FamilyType>(allocation1->getBO()));
    EXPECT_FALSE(isResident<FamilyType>(allocation2->getBO()));
    EXPECT_EQ(allocation1->getBO()->getRefCount(), 1u);
    EXPECT_EQ(allocation2->getBO()->getRefCount(), 1u);

    mm->freeGraphicsMemory(allocation1);
    mm->freeGraphicsMemory(allocation2);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, GivenFlushMultipleTimesThenSucceeds) {
    auto &cs = csr->getCS();
    auto commandBuffer = static_cast<DrmAllocation *>(cs.getGraphicsAllocation());

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    csr->flush(batchBuffer, csr->getResidencyAllocations());

    cs.replaceBuffer(commandBuffer->getUnderlyingBuffer(), commandBuffer->getUnderlyingBufferSize());
    cs.replaceGraphicsAllocation(commandBuffer);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer2 = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    batchBuffer.startOffset = 8;
    csr->flush(batchBuffer2, csr->getResidencyAllocations());

    auto allocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, allocation);
    auto allocation2 = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, allocation2);

    csr->makeResident(*allocation);
    csr->makeResident(*allocation2);

    csr->getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(commandBuffer), REUSABLE_ALLOCATION);

    auto commandBuffer2 = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer2);
    cs.replaceBuffer(commandBuffer2->getUnderlyingBuffer(), commandBuffer2->getUnderlyingBufferSize());
    cs.replaceGraphicsAllocation(commandBuffer2);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer3 = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    batchBuffer.startOffset = 16;
    csr->flush(batchBuffer3, csr->getResidencyAllocations());
    csr->makeSurfacePackNonResident(csr->getResidencyAllocations(), true);
    mm->freeGraphicsMemory(allocation);
    mm->freeGraphicsMemory(allocation2);

    csr->getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(commandBuffer2), REUSABLE_ALLOCATION);
    commandBuffer2 = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer2);
    cs.replaceBuffer(commandBuffer2->getUnderlyingBuffer(), commandBuffer2->getUnderlyingBufferSize());
    cs.replaceGraphicsAllocation(commandBuffer2);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer4 = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    batchBuffer.startOffset = 24;
    csr->flush(batchBuffer4, csr->getResidencyAllocations());
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, GivenNotEmptyBbWhenFlushingThenSucceeds) {
    int bbUsed = 16 * sizeof(uint32_t);

    auto &cs = csr->getCS();

    cs.getSpace(bbUsed);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    csr->flush(batchBuffer, csr->getResidencyAllocations());
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, GivenNotEmptyNotPaddedBbWhenFlushingThenSucceeds) {
    int bbUsed = 15 * sizeof(uint32_t);

    auto &cs = csr->getCS();

    cs.getSpace(bbUsed);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    csr->flush(batchBuffer, csr->getResidencyAllocations());
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedWithFailingExec, GivenFailingExecThenCSRFlushFails) {
    int bbUsed = 15 * sizeof(uint32_t);

    auto &cs = csr->getCS();

    cs.getSpace(bbUsed);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    NEO::SubmissionStatus ret = csr->flush(batchBuffer, csr->getResidencyAllocations());
    EXPECT_EQ(ret, NEO::SubmissionStatus::failed);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, GivenNotAlignedWhenFlushingThenSucceeds) {
    auto &cs = csr->getCS();
    auto commandBuffer = static_cast<DrmAllocation *>(cs.getGraphicsAllocation());

    // make sure command buffer with offset is not page aligned
    ASSERT_NE(0u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & 0xFFF);
    ASSERT_EQ(4u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & 0x7F);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    batchBuffer.startOffset = 4;
    csr->flush(batchBuffer, csr->getResidencyAllocations());
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, GivenCheckDrmFreeWhenFlushingThenSucceeds) {
    auto &cs = csr->getCS();
    auto commandBuffer = static_cast<DrmAllocation *>(cs.getGraphicsAllocation());

    // make sure command buffer with offset is not page aligned
    ASSERT_NE(0u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & 0xFFF);
    ASSERT_EQ(4u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & 0x7F);

    auto allocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});

    csr->makeResident(*allocation);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    batchBuffer.startOffset = 4;
    csr->flush(batchBuffer, csr->getResidencyAllocations());
    csr->makeNonResident(*allocation);
    mm->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, WhenMakingResidentThenClearResidencyAllocationsInCommandStreamReceiver) {
    auto allocation1 = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    auto allocation2 = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});

    ASSERT_NE(nullptr, allocation1);
    ASSERT_NE(nullptr, allocation2);

    csr->makeResident(*allocation1);
    csr->makeResident(*allocation2);
    EXPECT_NE(0u, csr->getResidencyAllocations().size());

    csr->processResidency(csr->getResidencyAllocations(), 0u);
    csr->makeSurfacePackNonResident(csr->getResidencyAllocations(), true);
    EXPECT_EQ(0u, csr->getResidencyAllocations().size());

    mm->freeGraphicsMemory(allocation1);
    mm->freeGraphicsMemory(allocation2);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, WhenMakingResidentThenNotClearResidencyAllocationsInCommandStreamReceiver) {
    auto allocation1 = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    auto allocation2 = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});

    ASSERT_NE(nullptr, allocation1);
    ASSERT_NE(nullptr, allocation2);

    csr->makeResident(*allocation1);
    csr->makeResident(*allocation2);
    EXPECT_NE(0u, csr->getResidencyAllocations().size());

    csr->processResidency(csr->getResidencyAllocations(), 0u);
    csr->makeSurfacePackNonResident(csr->getResidencyAllocations(), false);
    EXPECT_NE(0u, csr->getResidencyAllocations().size());

    mm->freeGraphicsMemory(allocation1);
    mm->freeGraphicsMemory(allocation2);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenMultipleMakeResidentWhenMakeNonResidentIsCalledOnlyOnceThenSurfaceIsMadeNonResident) {
    auto allocation1 = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});

    ASSERT_NE(nullptr, allocation1);

    csr->makeResident(*allocation1);
    csr->makeResident(*allocation1);

    EXPECT_NE(0u, csr->getResidencyAllocations().size());

    csr->processResidency(csr->getResidencyAllocations(), 0u);
    csr->makeSurfacePackNonResident(csr->getResidencyAllocations(), true);

    EXPECT_EQ(0u, csr->getResidencyAllocations().size());
    EXPECT_FALSE(allocation1->isResident(csr->getOsContext().getContextId()));

    mm->freeGraphicsMemory(allocation1);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenAllocationThatIsAlwaysResidentWhenMakeNonResidentIsCalledThenItIsNotMadeNonResident) {
    auto allocation1 = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});

    ASSERT_NE(nullptr, allocation1);

    csr->makeResident(*allocation1);

    allocation1->updateResidencyTaskCount(GraphicsAllocation::objectAlwaysResident, csr->getOsContext().getContextId());

    EXPECT_NE(0u, csr->getResidencyAllocations().size());

    csr->processResidency(csr->getResidencyAllocations(), 0u);
    csr->makeSurfacePackNonResident(csr->getResidencyAllocations(), true);

    EXPECT_EQ(0u, csr->getResidencyAllocations().size());
    EXPECT_TRUE(allocation1->isResident(csr->getOsContext().getContextId()));

    mm->freeGraphicsMemory(allocation1);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, GivenMemObjectCallsDrmCsrWhenMakingNonResidentThenMakeNonResidentWithGraphicsAllocation) {
    auto allocation1 = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), 0x1000});
    ASSERT_NE(nullptr, allocation1);

    auto &makeNonResidentResult = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->makeNonResidentResult;
    csr->makeResident(*allocation1);

    makeNonResidentResult.called = false;
    makeNonResidentResult.allocation = nullptr;

    csr->makeNonResident(*allocation1);

    EXPECT_TRUE(makeNonResidentResult.called);
    EXPECT_EQ(allocation1, makeNonResidentResult.allocation);
    EXPECT_EQ(0u, csr->getEvictionAllocations().size());

    mm->freeGraphicsMemory(allocation1);
}
HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenDrmCommandStreamReceiverWhenMemoryManagerIsCreatedThenItHasHostMemoryValidationEnabledByDefault) {
    EXPECT_TRUE(mm->isValidateHostMemoryEnabled());
}

struct MockDrmAllocationBindBO : public DrmAllocation {
    MockDrmAllocationBindBO(uint32_t rootDeviceIndex, AllocationType allocationType, BufferObjects &bos, void *ptrIn, uint64_t gpuAddress, size_t sizeIn, MemoryPool pool)
        : DrmAllocation(rootDeviceIndex, 1u /*num gmms*/, allocationType, bos, ptrIn, gpuAddress, sizeIn, pool) {
    }

    ADDMETHOD_NOBASE(bindBO, int, 0,
                     (BufferObject * bo, OsContext *osContext, uint32_t vmHandleId, std::vector<BufferObject *> *bufferObjects, bool bind, const bool forcePagingFence));
};

struct MockDrmAllocationBindBOs : public DrmAllocation {
    MockDrmAllocationBindBOs(uint32_t rootDeviceIndex, AllocationType allocationType, BufferObjects &bos, void *ptrIn, uint64_t gpuAddress, size_t sizeIn, MemoryPool pool)
        : DrmAllocation(rootDeviceIndex, 1u /*num gmms*/, allocationType, bos, ptrIn, gpuAddress, sizeIn, pool) {
    }

    ADDMETHOD_NOBASE(bindBOs, int, 0,
                     (OsContext * osContext, uint32_t vmHandleId, std::vector<BufferObject *> *bufferObjects, bool bind, const bool forcePagingFence));
};

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenBindBOsFailsThenMakeBOsResidentReturnsError) {
    auto size = 1024u;
    auto bo = this->createBO(size);
    BufferObjects bos{bo};

    auto allocation = new MockDrmAllocationBindBOs(0, AllocationType::unknown, bos, nullptr, 0u, size, MemoryPool::localMemory);
    allocation->bindBOsResult = -1;

    auto res = allocation->makeBOsResident(&csr->getOsContext(), 0, nullptr, true, false);
    EXPECT_NE(res, 0);
    EXPECT_EQ(allocation->fragmentsStorage.fragmentCount, 0u);
    EXPECT_GT(allocation->bindBOsCalled, 0u);

    mm->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenFragmentStorageAndBindBOFailsThenMakeBOsResidentReturnsError) {
    auto size = 1024u;
    auto bo = this->createBO(size);
    BufferObjects bos{bo};

    auto allocation = new MockDrmAllocationBindBO(0, AllocationType::unknown, bos, nullptr, 0u, size, MemoryPool::localMemory);
    allocation->bindBOResult = -1;

    OsHandleStorage prevStorage;
    OsHandleStorage storage;
    OsHandleLinux osHandleStorage;
    ResidencyData *residency = new ResidencyData(1);

    storage.fragmentCount = 1;
    storage.fragmentStorageData[0].osHandleStorage = &osHandleStorage;
    storage.fragmentStorageData[0].residency = residency;
    storage.fragmentStorageData[0].residency->resident[csr->getOsContext().getContextId()] = false;

    memcpy(&prevStorage, &allocation->fragmentsStorage, sizeof(OsHandleStorage));
    memcpy(&allocation->fragmentsStorage, &storage, sizeof(OsHandleStorage));

    auto res = allocation->makeBOsResident(&csr->getOsContext(), 0, nullptr, true, false);
    EXPECT_NE(res, 0);
    EXPECT_EQ(allocation->fragmentsStorage.fragmentCount, 1u);
    EXPECT_GT(allocation->bindBOCalled, 0u);

    memcpy(&allocation->fragmentsStorage, &prevStorage, sizeof(OsHandleStorage));
    mm->freeGraphicsMemory(allocation);
    delete residency;
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenBindBOFailsThenBindBOsReturnsError) {
    auto size = 1024u;
    auto bo = this->createBO(size);
    BufferObjects bos{bo};

    auto allocation = new MockDrmAllocationBindBO(0, AllocationType::unknown, bos, nullptr, 0u, size, MemoryPool::localMemory);
    allocation->bindBOResult = -1;

    auto res = allocation->bindBOs(&csr->getOsContext(), 0u, &static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->residency, false, false);
    EXPECT_NE(res, 0);
    EXPECT_GT(allocation->bindBOCalled, 0u);

    mm->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenBindBOFailsWithMultipleMemoryBanksThenBindBOsReturnsError) {
    auto size = 1024u;
    auto bo = this->createBO(size);
    auto bo2 = this->createBO(size);
    BufferObjects bos{bo, bo2};

    auto allocation = new MockDrmAllocationBindBO(0, AllocationType::unknown, bos, nullptr, 0u, size, MemoryPool::localMemory);
    allocation->bindBOResult = -1;
    allocation->storageInfo.memoryBanks = 0b11;
    EXPECT_EQ(allocation->storageInfo.getNumBanks(), 2u);

    auto res = allocation->bindBOs(&csr->getOsContext(), 0u, &static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->residency, false, false);
    EXPECT_NE(res, 0);
    EXPECT_GT(allocation->bindBOCalled, 0u);

    mm->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenBindBOFailsWithMultipleMemoryBanksWithTileInstancedThenBindBOsReturnsError) {
    auto size = 1024u;
    auto bo = this->createBO(size);
    auto bo2 = this->createBO(size);
    BufferObjects bos{bo, bo2};

    auto allocation = new MockDrmAllocationBindBO(0, AllocationType::unknown, bos, nullptr, 0u, size, MemoryPool::localMemory);
    allocation->bindBOResult = -1;
    allocation->storageInfo.tileInstanced = true;
    allocation->storageInfo.memoryBanks = 0b11;
    EXPECT_EQ(allocation->storageInfo.getNumBanks(), 2u);

    auto res = allocation->bindBOs(&csr->getOsContext(), 0u, &static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->residency, false, false);
    EXPECT_NE(res, 0);
    EXPECT_GT(allocation->bindBOCalled, 0u);

    mm->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenAllocationWithSingleBufferObjectWhenMakeResidentBufferObjectsIsCalledThenTheBufferObjectIsMadeResident) {
    auto size = 1024u;
    auto bo = this->createBO(size);
    BufferObjects bos{bo};
    auto allocation = new DrmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, bos, nullptr, 0u, size, MemoryPool::localMemory);
    EXPECT_EQ(bo, allocation->getBO());

    makeResidentBufferObjects<FamilyType>(&csr->getOsContext(), allocation);
    EXPECT_TRUE(isResident<FamilyType>(bo));

    mm->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest,
                   givenWaitUserFenceFlagAndVmBindAvailableSetWhenDrmCsrFlushedThenExpectLatestSentTaskCountStoredAsFlushStamp) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableUserFenceForCompletionWait.set(1);

    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    TestedDrmCommandStreamReceiver<FamilyType> *testedCsr =
        new TestedDrmCommandStreamReceiver<FamilyType>(
            *this->executionEnvironment,
            1);
    EXPECT_TRUE(testedCsr->useUserFenceWait);
    device->resetCommandStreamReceiver(testedCsr);

    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{testedCsr->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    testedCsr->latestSentTaskCount = 160u;
    testedCsr->flush(batchBuffer, testedCsr->getResidencyAllocations());

    EXPECT_EQ(160u, testedCsr->flushStamp->peekStamp());

    mm->freeGraphicsMemory(commandBuffer);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest,
                   givenWaitUserFenceFlagNotSetAndVmBindAvailableSetWhenDrmCsrFlushedThenExpectCommandBufferBoHandleAsFlushStamp) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableUserFenceForCompletionWait.set(0);

    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    TestedDrmCommandStreamReceiver<FamilyType> *testedCsr =
        new TestedDrmCommandStreamReceiver<FamilyType>(
            *this->executionEnvironment,
            1);
    EXPECT_FALSE(testedCsr->useUserFenceWait);
    device->resetCommandStreamReceiver(testedCsr);

    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{testedCsr->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    DrmAllocation *alloc = static_cast<DrmAllocation *>(cs.getGraphicsAllocation());
    auto boHandle = static_cast<FlushStamp>(alloc->getBO()->peekHandle());
    testedCsr->latestSentTaskCount = 160u;

    testedCsr->flush(batchBuffer, testedCsr->getResidencyAllocations());

    EXPECT_EQ(boHandle, testedCsr->flushStamp->peekStamp());

    mm->freeGraphicsMemory(commandBuffer);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest,
                   givenWaitUserFenceFlagAndNoVmBindAvailableSetWhenDrmCsrFlushedThenExpectCommandBufferBoHandleAsFlushStamp) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableUserFenceForCompletionWait.set(1);

    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = false;

    TestedDrmCommandStreamReceiver<FamilyType> *testedCsr =
        new TestedDrmCommandStreamReceiver<FamilyType>(
            *this->executionEnvironment,
            1);
    EXPECT_TRUE(testedCsr->useUserFenceWait);
    device->resetCommandStreamReceiver(testedCsr);

    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{testedCsr->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    DrmAllocation *alloc = static_cast<DrmAllocation *>(cs.getGraphicsAllocation());
    auto boHandle = static_cast<FlushStamp>(alloc->getBO()->peekHandle());
    testedCsr->latestSentTaskCount = 160u;

    testedCsr->flush(batchBuffer, testedCsr->getResidencyAllocations());

    EXPECT_EQ(boHandle, testedCsr->flushStamp->peekStamp());

    mm->freeGraphicsMemory(commandBuffer);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenWaitUserFenceFlagNotSetWhenDrmCsrWaitsForFlushStampThenExpectUseDrmGemWaitCall) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableUserFenceForCompletionWait.set(0);

    TestedDrmCommandStreamReceiver<FamilyType> *testedCsr =
        new TestedDrmCommandStreamReceiver<FamilyType>(
            *this->executionEnvironment,
            1);
    EXPECT_FALSE(testedCsr->useUserFenceWait);
    EXPECT_FALSE(testedCsr->isUsedNotifyEnableForPostSync());
    device->resetCommandStreamReceiver(testedCsr);
    mock->ioctlCnt.gemWait = 0;

    FlushStamp handleToWait = 123;
    testedCsr->waitForFlushStamp(handleToWait);

    EXPECT_EQ(1, mock->ioctlCnt.gemWait);
    EXPECT_EQ(-1, mock->gemWaitTimeout);
    EXPECT_EQ(0u, testedCsr->waitUserFenceResult.called);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenDirectSubmissionLightWhenWaitForFlushStampThenStopDirectSubmission) {
    EXPECT_FALSE(static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->stopDirectSubmissionCalled);

    csr->getOsContext().setDirectSubmissionActive();
    FlushStamp handleToWait = 123;
    csr->waitForFlushStamp(handleToWait);

    EXPECT_TRUE(static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->stopDirectSubmissionCalled);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, whenWaitForFlushStampThenDoNotStopDirectSubmission) {
    EXPECT_FALSE(static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->stopDirectSubmissionCalled);

    FlushStamp handleToWait = 123;
    csr->waitForFlushStamp(handleToWait);

    EXPECT_FALSE(static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->stopDirectSubmissionCalled);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenGemWaitUsedWhenKmdTimeoutUsedWhenDrmCsrWaitsForFlushStampThenExpectUseDrmGemWaitCallAndOverrideTimeout) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SetKmdWaitTimeout.set(1000);
    debugManager.flags.EnableUserFenceForCompletionWait.set(0);

    TestedDrmCommandStreamReceiver<FamilyType> *testedCsr =
        new TestedDrmCommandStreamReceiver<FamilyType>(
            *this->executionEnvironment,
            1);
    EXPECT_FALSE(testedCsr->useUserFenceWait);
    EXPECT_FALSE(testedCsr->isUsedNotifyEnableForPostSync());
    device->resetCommandStreamReceiver(testedCsr);
    mock->ioctlCnt.gemWait = 0;

    FlushStamp handleToWait = 123;
    testedCsr->waitForFlushStamp(handleToWait);

    EXPECT_EQ(1, mock->ioctlCnt.gemWait);
    EXPECT_EQ(1000, mock->gemWaitTimeout);
    EXPECT_EQ(0u, testedCsr->waitUserFenceResult.called);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest,
                   givenWaitUserFenceFlagSetAndVmBindAvailableAndUseDrmCtxWhenDrmCsrWaitsForFlushStampThenExpectUseDrmWaitUserFenceCallWithNonZeroContext) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableUserFenceForCompletionWait.set(1);

    TestedDrmCommandStreamReceiver<FamilyType> *testedCsr =
        new TestedDrmCommandStreamReceiver<FamilyType>(
            *this->executionEnvironment,
            1);
    *const_cast<bool *>(&testedCsr->vmBindAvailable) = true;
    EXPECT_TRUE(testedCsr->useUserFenceWait);
    EXPECT_TRUE(testedCsr->isUsedNotifyEnableForPostSync());
    device->resetCommandStreamReceiver(testedCsr);
    mock->ioctlCnt.gemWait = 0;

    auto osContextLinux = static_cast<const OsContextLinux *>(device->getDefaultEngine().osContext);
    std::vector<uint32_t> &drmCtxIds = const_cast<std::vector<uint32_t> &>(osContextLinux->getDrmContextIds());
    size_t drmCtxSize = drmCtxIds.size();
    for (uint32_t i = 0; i < drmCtxSize; i++) {
        drmCtxIds[i] = 5u + i;
    }

    const auto hasFirstSubmission = device->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo) ? 1 : 0;
    FlushStamp handleToWait = 123;
    *testedCsr->getTagAddress() = hasFirstSubmission;
    testedCsr->waitForFlushStamp(handleToWait);

    EXPECT_EQ(0, mock->ioctlCnt.gemWait);
    EXPECT_EQ(1u, testedCsr->waitUserFenceResult.called);
    EXPECT_EQ(123u, testedCsr->waitUserFenceResult.waitValue);

    EXPECT_EQ(1u, mock->waitUserFenceCall.called);

    EXPECT_NE(0u, mock->waitUserFenceCall.ctxId);
    EXPECT_EQ(-1, mock->waitUserFenceCall.timeout);
    EXPECT_EQ(Drm::ValueWidth::u64, mock->waitUserFenceCall.dataWidth);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest,
                   givenWaitUserFenceFlagSetAndVmBindAvailableAndUseDrmCtxWhenDrmCsrWaitsForFlushStampiAndDifferentScratchPageOptionsThenCallResetStatusOnlyScratchPageDisabled) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableUserFenceForCompletionWait.set(1);

    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    for (int err : {EIO, ETIME}) {
        for (bool disableScratchPage : {false, true}) {
            for (int gpuFaultCheckThreshold : {0, 10}) {
                debugManager.flags.DisableScratchPages.set(disableScratchPage);
                debugManager.flags.GpuFaultCheckThreshold.set(gpuFaultCheckThreshold);
                mock->configureScratchPagePolicy();
                mock->configureGpuFaultCheckThreshold();

                TestedDrmCommandStreamReceiver<FamilyType> *testedCsr =
                    new TestedDrmCommandStreamReceiver<FamilyType>(
                        *this->executionEnvironment,
                        1);
                EXPECT_TRUE(testedCsr->useUserFenceWait);
                EXPECT_TRUE(testedCsr->isUsedNotifyEnableForPostSync());

                device->resetCommandStreamReceiver(testedCsr);
                mock->ioctlCnt.reset();
                mock->waitUserFenceCall.called = 0u;
                mock->checkResetStatusCalled = 0u;

                mock->waitUserFenceCall.failOnWaitUserFence = true;
                mock->errnoValue = err;

                testedCsr->waitUserFenceResult.callParent = true;

                auto osContextLinux = static_cast<const OsContextLinux *>(device->getDefaultEngine().osContext);
                std::vector<uint32_t> &drmCtxIds = const_cast<std::vector<uint32_t> &>(osContextLinux->getDrmContextIds());
                size_t drmCtxSize = drmCtxIds.size();
                for (uint32_t i = 0; i < drmCtxSize; i++) {
                    drmCtxIds[i] = 5u + i;
                }

                TaskCountType waitValue = 2;
                TaskCountType currentValue = 1;
                uint64_t addr = castToUint64(&currentValue);
                testedCsr->waitUserFence(waitValue, addr, -1, false, NEO::InterruptId::notUsed, nullptr);

                EXPECT_EQ(0, mock->ioctlCnt.gemWait);
                EXPECT_EQ(1u, testedCsr->waitUserFenceResult.called);
                EXPECT_EQ(2u, testedCsr->waitUserFenceResult.waitValue);

                EXPECT_EQ(1u, mock->waitUserFenceCall.called);
                if (err == EIO && disableScratchPage && gpuFaultCheckThreshold != 0) {
                    EXPECT_EQ(1u, mock->checkResetStatusCalled);
                } else {
                    EXPECT_EQ(0u, mock->checkResetStatusCalled);
                }
            }
        }
    }
}
HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest,
                   givenWaitUserFenceFlagSetAndVmBindNotAvailableWhenDrmCsrWaitsForFlushStampThenExpectUseDrmGemWaitCall) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableUserFenceForCompletionWait.set(1);

    TestedDrmCommandStreamReceiver<FamilyType> *testedCsr =
        new TestedDrmCommandStreamReceiver<FamilyType>(
            *this->executionEnvironment,
            1);
    *const_cast<bool *>(&testedCsr->vmBindAvailable) = false;
    EXPECT_TRUE(testedCsr->useUserFenceWait);
    EXPECT_TRUE(testedCsr->isUsedNotifyEnableForPostSync());
    device->resetCommandStreamReceiver(testedCsr);
    mock->ioctlCnt.gemWait = 0;

    FlushStamp handleToWait = 123;
    testedCsr->waitForFlushStamp(handleToWait);

    EXPECT_EQ(1, mock->ioctlCnt.gemWait);
    EXPECT_EQ(0u, testedCsr->waitUserFenceResult.called);
    EXPECT_EQ(0u, mock->waitUserFenceCall.called);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest,
                   givenWaitUserFenceFlagNotSetAndVmBindAvailableWhenDrmCsrWaitsForFlushStampThenExpectUseDrmGemWaitCall) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableUserFenceForCompletionWait.set(0);

    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    TestedDrmCommandStreamReceiver<FamilyType> *testedCsr =
        new TestedDrmCommandStreamReceiver<FamilyType>(
            *this->executionEnvironment,
            1);
    *const_cast<bool *>(&testedCsr->vmBindAvailable) = true;
    EXPECT_FALSE(testedCsr->useUserFenceWait);
    EXPECT_FALSE(testedCsr->isUsedNotifyEnableForPostSync());
    device->resetCommandStreamReceiver(testedCsr);
    mock->ioctlCnt.gemWait = 0;

    FlushStamp handleToWait = 123;
    EXPECT_ANY_THROW(testedCsr->waitForFlushStamp(handleToWait));

    EXPECT_EQ(0, mock->ioctlCnt.gemWait);
    EXPECT_EQ(0u, testedCsr->waitUserFenceResult.called);

    EXPECT_EQ(0u, mock->waitUserFenceCall.called);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest,
                   givenWaitUserFenceSetAndUseCtxFlagsNotSetAndVmBindAvailableWhenDrmCsrWaitsForFlushStampThenExpectUseDrmWaitUserFenceCallWithZeroContext) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableUserFenceForCompletionWait.set(1);
    debugManager.flags.SetKmdWaitTimeout.set(1000);

    TestedDrmCommandStreamReceiver<FamilyType> *testedCsr =
        new TestedDrmCommandStreamReceiver<FamilyType>(
            *this->executionEnvironment,
            1);
    *const_cast<bool *>(&testedCsr->vmBindAvailable) = true;
    EXPECT_TRUE(testedCsr->useUserFenceWait);
    EXPECT_TRUE(testedCsr->isUsedNotifyEnableForPostSync());
    device->resetCommandStreamReceiver(testedCsr);
    mock->ioctlCnt.gemWait = 0;

    const auto hasFirstSubmission = device->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo) ? 1 : 0;
    FlushStamp handleToWait = 123;
    *testedCsr->getTagAddress() = hasFirstSubmission;
    testedCsr->waitForFlushStamp(handleToWait);

    EXPECT_EQ(0, mock->ioctlCnt.gemWait);
    EXPECT_EQ(1u, testedCsr->waitUserFenceResult.called);
    EXPECT_EQ(123u, testedCsr->waitUserFenceResult.waitValue);

    EXPECT_EQ(1u, mock->waitUserFenceCall.called);

    EXPECT_NE(0u, mock->waitUserFenceCall.ctxId);
    EXPECT_EQ(1000, mock->waitUserFenceCall.timeout);
    EXPECT_EQ(Drm::ValueWidth::u64, mock->waitUserFenceCall.dataWidth);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest,
                   givenNoDebugFlagWaitUserFenceSetWhenDrmCsrIsCreatedThenUseNotifyEnableFlagIsSet) {
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    std::unique_ptr<TestedDrmCommandStreamReceiver<FamilyType>> testedCsr =
        std::make_unique<TestedDrmCommandStreamReceiver<FamilyType>>(
            *this->executionEnvironment,
            1);

    EXPECT_TRUE(testedCsr->useUserFenceWait);
    EXPECT_TRUE(testedCsr->isUsedNotifyEnableForPostSync());
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest,
                   givenWaitUserFenceSetWhenDrmCsrIsCreatedThenUseNotifyEnableFlagIsSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableUserFenceForCompletionWait.set(1);

    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    std::unique_ptr<TestedDrmCommandStreamReceiver<FamilyType>> testedCsr =
        std::make_unique<TestedDrmCommandStreamReceiver<FamilyType>>(
            *this->executionEnvironment,
            1);

    EXPECT_TRUE(testedCsr->useUserFenceWait);
    EXPECT_TRUE(testedCsr->isUsedNotifyEnableForPostSync());
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest,
                   givenWaitUserFenceNotSetWhenDrmCsrIsCreatedThenUseNotifyEnableFlagIsNotSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableUserFenceForCompletionWait.set(0);

    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    std::unique_ptr<TestedDrmCommandStreamReceiver<FamilyType>> testedCsr =
        std::make_unique<TestedDrmCommandStreamReceiver<FamilyType>>(
            *this->executionEnvironment,
            1);

    EXPECT_FALSE(testedCsr->useUserFenceWait);
    EXPECT_FALSE(testedCsr->isUsedNotifyEnableForPostSync());
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest,
                   givenWaitUserFenceNotSetAndOverrideNotifyEnableSetWhenDrmCsrIsCreatedThenUseNotifyEnableFlagIsSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableUserFenceForCompletionWait.set(0);
    debugManager.flags.OverrideNotifyEnableForTagUpdatePostSync.set(1);
    debugManager.flags.EnableL3FlushAfterPostSync.set(0);

    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    std::unique_ptr<TestedDrmCommandStreamReceiver<FamilyType>> testedCsr =
        std::make_unique<TestedDrmCommandStreamReceiver<FamilyType>>(
            *this->executionEnvironment,
            1);

    EXPECT_FALSE(testedCsr->useUserFenceWait);
    EXPECT_TRUE(testedCsr->isUsedNotifyEnableForPostSync());
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest,
                   givenWaitUserFenceSetAndOverrideNotifyEnableNotSetWhenDrmCsrIsCreatedThenUseNotifyEnableFlagIsNotSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableUserFenceForCompletionWait.set(1);
    debugManager.flags.OverrideNotifyEnableForTagUpdatePostSync.set(0);

    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    std::unique_ptr<TestedDrmCommandStreamReceiver<FamilyType>> testedCsr =
        std::make_unique<TestedDrmCommandStreamReceiver<FamilyType>>(
            *this->executionEnvironment,
            1);

    EXPECT_TRUE(testedCsr->useUserFenceWait);
    EXPECT_FALSE(testedCsr->isUsedNotifyEnableForPostSync());
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenVmBindNotAvailableWhenCheckingForKmdWaitModeActiveThenReturnTrue) {
    auto testDrmCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    *const_cast<bool *>(&testDrmCsr->vmBindAvailable) = false;

    EXPECT_TRUE(testDrmCsr->isKmdWaitModeActive());
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenVmBindAvailableUseWaitCallTrueWhenCheckingForKmdWaitModeActiveThenReturnTrue) {
    auto testDrmCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    *const_cast<bool *>(&testDrmCsr->vmBindAvailable) = true;
    testDrmCsr->useUserFenceWait = true;

    EXPECT_TRUE(testDrmCsr->isKmdWaitModeActive());
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenVmBindAvailableUseWaitCallFalseWhenCheckingForKmdWaitModeActiveThenReturnFalse) {
    auto testDrmCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    *const_cast<bool *>(&testDrmCsr->vmBindAvailable) = true;
    testDrmCsr->useUserFenceWait = false;

    EXPECT_FALSE(testDrmCsr->isKmdWaitModeActive());
}

struct MockMergeResidencyContainerMemoryOperationsHandler : public DrmMemoryOperationsHandlerDefault {
    using DrmMemoryOperationsHandlerDefault::DrmMemoryOperationsHandlerDefault;
    MockMergeResidencyContainerMemoryOperationsHandler(uint32_t rootDeviceIndex)
        : DrmMemoryOperationsHandlerDefault(rootDeviceIndex){};

    ADDMETHOD_NOBASE(mergeWithResidencyContainer, NEO::MemoryOperationsStatus, NEO::MemoryOperationsStatus::success,
                     (OsContext * osContext, ResidencyContainer &residencyContainer));

    ADDMETHOD_NOBASE(makeResidentWithinOsContext, NEO::MemoryOperationsStatus, NEO::MemoryOperationsStatus::success,
                     (OsContext * osContext, ArrayRef<GraphicsAllocation *> gfxAllocations, bool evictable, const bool forcePagingFence, const bool acquireLock));
};

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenMergeWithResidencyContainerFailsThenFlushReturnsError) {
    struct MockDrmCsr : public DrmCommandStreamReceiver<FamilyType> {
        using DrmCommandStreamReceiver<FamilyType>::DrmCommandStreamReceiver;
        SubmissionStatus flushInternal(const BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override {
            return SubmissionStatus::success;
        }
    };

    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface.reset(new MockMergeResidencyContainerMemoryOperationsHandler(rootDeviceIndex));
    auto operationHandler = static_cast<MockMergeResidencyContainerMemoryOperationsHandler *>(executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface.get());
    operationHandler->mergeWithResidencyContainerResult = NEO::MemoryOperationsStatus::failed;

    auto &gfxCoreHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    auto osContext = std::make_unique<OsContextLinux>(*mock, rootDeviceIndex, 0u,
                                                      EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*executionEnvironment->rootDeviceEnvironments[0])[0],
                                                                                                   PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo)));

    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    LinearStream cs(commandBuffer);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    MockDrmCsr mockCsr(*executionEnvironment, rootDeviceIndex, 1);
    mockCsr.setupContext(*osContext.get());
    auto res = mockCsr.flush(batchBuffer, mockCsr.getResidencyAllocations());
    EXPECT_GT(operationHandler->mergeWithResidencyContainerCalled, 0u);
    EXPECT_EQ(NEO::SubmissionStatus::failed, res);

    mm->freeGraphicsMemory(commandBuffer);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenMergeWithResidencyContainerReturnsOutOfMemoryThenFlushReturnsError) {
    struct MockDrmCsr : public DrmCommandStreamReceiver<FamilyType> {
        using DrmCommandStreamReceiver<FamilyType>::DrmCommandStreamReceiver;
        SubmissionStatus flushInternal(const BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override {
            return SubmissionStatus::success;
        }
    };

    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface.reset(new MockMergeResidencyContainerMemoryOperationsHandler(rootDeviceIndex));
    auto operationHandler = static_cast<MockMergeResidencyContainerMemoryOperationsHandler *>(executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface.get());
    operationHandler->mergeWithResidencyContainerResult = NEO::MemoryOperationsStatus::outOfMemory;
    auto &gfxCoreHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    auto osContext = std::make_unique<OsContextLinux>(*mock, rootDeviceIndex, 0u,
                                                      EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*executionEnvironment->rootDeviceEnvironments[0])[0],
                                                                                                   PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo)));

    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    LinearStream cs(commandBuffer);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    MockDrmCsr mockCsr(*executionEnvironment, rootDeviceIndex, 1);
    mockCsr.setupContext(*osContext.get());
    auto res = mockCsr.flush(batchBuffer, mockCsr.getResidencyAllocations());
    EXPECT_GT(operationHandler->mergeWithResidencyContainerCalled, 0u);
    EXPECT_EQ(NEO::SubmissionStatus::outOfMemory, res);

    mm->freeGraphicsMemory(commandBuffer);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenNoAllocsInMemoryOperationHandlerDefaultWhenFlushThenDrmMemoryOperationHandlerIsNotLocked) {
    struct MockDrmMemoryOperationsHandler : public DrmMemoryOperationsHandler {
        using DrmMemoryOperationsHandler::mutex;
    };

    struct MockDrmCsr : public DrmCommandStreamReceiver<FamilyType> {
        using DrmCommandStreamReceiver<FamilyType>::DrmCommandStreamReceiver;
        SubmissionStatus flushInternal(const BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override {
            auto memoryOperationsInterface = static_cast<MockDrmMemoryOperationsHandler *>(this->executionEnvironment.rootDeviceEnvironments[this->rootDeviceIndex]->memoryOperationsInterface.get());
            EXPECT_TRUE(memoryOperationsInterface->mutex.try_lock());
            memoryOperationsInterface->mutex.unlock();
            return SubmissionStatus::success;
        }
    };
    auto &gfxCoreHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    auto osContext = std::make_unique<OsContextLinux>(*mock, rootDeviceIndex, 0u,
                                                      EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*executionEnvironment->rootDeviceEnvironments[0])[0],
                                                                                                   PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo)));

    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    LinearStream cs(commandBuffer);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    MockDrmCsr mockCsr(*executionEnvironment, rootDeviceIndex, 1);
    mockCsr.setupContext(*osContext.get());
    mockCsr.flush(batchBuffer, mockCsr.getResidencyAllocations());

    mm->freeGraphicsMemory(commandBuffer);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenAllocsInMemoryOperationHandlerDefaultWhenFlushThenDrmMemoryOperationHandlerIsLocked) {
    struct MockDrmMemoryOperationsHandler : public DrmMemoryOperationsHandler {
        using DrmMemoryOperationsHandler::mutex;
    };

    struct MockDrmCsr : public DrmCommandStreamReceiver<FamilyType> {
        using DrmCommandStreamReceiver<FamilyType>::DrmCommandStreamReceiver;
        SubmissionStatus flushInternal(const BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override {
            auto memoryOperationsInterface = static_cast<MockDrmMemoryOperationsHandler *>(this->executionEnvironment.rootDeviceEnvironments[this->rootDeviceIndex]->memoryOperationsInterface.get());
            EXPECT_FALSE(memoryOperationsInterface->mutex.try_lock());
            return SubmissionStatus::success;
        }
    };
    auto &gfxCoreHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    auto osContext = std::make_unique<OsContextLinux>(*mock, rootDeviceIndex, 0u,
                                                      EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*executionEnvironment->rootDeviceEnvironments[0])[0],
                                                                                                   PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo)));

    auto allocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    executionEnvironment->rootDeviceEnvironments[csr->getRootDeviceIndex()]->memoryOperationsInterface->makeResident(device.get(), ArrayRef<GraphicsAllocation *>(&allocation, 1), false, false);

    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    LinearStream cs(commandBuffer);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    MockDrmCsr mockCsr(*executionEnvironment, rootDeviceIndex, 1);
    mockCsr.setupContext(*osContext.get());
    mockCsr.flush(batchBuffer, mockCsr.getResidencyAllocations());

    mm->freeGraphicsMemory(commandBuffer);
    mm->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenAllocInMemoryOperationsInterfaceWhenFlushThenAllocIsResident) {
    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    LinearStream cs(commandBuffer);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    auto allocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    executionEnvironment->rootDeviceEnvironments[csr->getRootDeviceIndex()]->memoryOperationsInterface->makeResident(device.get(), ArrayRef<GraphicsAllocation *>(&allocation, 1), false, false);

    csr->flush(batchBuffer, csr->getResidencyAllocations());

    const auto execObjectRequirements = [&allocation](const auto &execObject) {
        auto mockExecObject = static_cast<const MockExecObject &>(execObject);
        return (static_cast<int>(mockExecObject.getHandle()) == static_cast<DrmAllocation *>(allocation)->getBO()->peekHandle() &&
                mockExecObject.getOffset() == static_cast<DrmAllocation *>(allocation)->getBO()->peekAddress());
    };

    auto &residency = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->execObjectsStorage;
    EXPECT_TRUE(std::find_if(residency.begin(), residency.end(), execObjectRequirements) != residency.end());
    EXPECT_EQ(residency.size(), 2u);
    residency.clear();

    csr->flush(batchBuffer, csr->getResidencyAllocations());
    EXPECT_TRUE(std::find_if(residency.begin(), residency.end(), execObjectRequirements) != residency.end());
    EXPECT_EQ(residency.size(), 2u);
    residency.clear();

    csr->getResidencyAllocations().clear();
    executionEnvironment->rootDeviceEnvironments[csr->getRootDeviceIndex()]->memoryOperationsInterface->evict(device.get(), *allocation);

    csr->flush(batchBuffer, csr->getResidencyAllocations());

    EXPECT_FALSE(std::find_if(residency.begin(), residency.end(), execObjectRequirements) != residency.end());
    EXPECT_EQ(residency.size(), 1u);

    mm->freeGraphicsMemory(allocation);
    mm->freeGraphicsMemory(commandBuffer);
}
