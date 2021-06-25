/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/page_table_mngr.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/residency.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler_default.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"

#include "opencl/source/helpers/memory_properties_helpers.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/os_interface/linux/drm_command_stream.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/helpers/execution_environment_helper.h"
#include "opencl/test/unit_test/mocks/linux/mock_drm_command_stream_receiver.h"
#include "opencl/test/unit_test/mocks/mock_allocation_properties.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_gmm.h"
#include "opencl/test/unit_test/mocks/mock_gmm_page_table_mngr.h"
#include "opencl/test/unit_test/mocks/mock_host_ptr_manager.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/mocks/mock_submissions_aggregator.h"
#include "opencl/test/unit_test/os_interface/linux/drm_command_stream_fixture.h"
#include "test.h"

#include "drm/i915_drm.h"

using namespace NEO;

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenDefaultDrmCSRWhenItIsCreatedThenGemCloseWorkerModeIsInactive) {
    EXPECT_EQ(gemCloseWorkerMode::gemCloseWorkerInactive, static_cast<const DrmCommandStreamReceiver<FamilyType> *>(csr)->peekGemCloseWorkerOperationMode());
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenCommandStreamWhenItIsFlushedWithGemCloseWorkerInDefaultModeThenWorkerDecreasesTheRefCount) {
    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    auto storedBase = cs.getCpuBase();
    auto storedGraphicsAllocation = cs.getGraphicsAllocation();
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    csr->flush(batchBuffer, csr->getResidencyAllocations());
    EXPECT_EQ(cs.getCpuBase(), storedBase);
    EXPECT_EQ(cs.getGraphicsAllocation(), storedGraphicsAllocation);

    auto drmAllocation = static_cast<DrmAllocation *>(storedGraphicsAllocation);
    auto bo = drmAllocation->getBO();

    //spin until gem close worker finishes execution
    while (bo->getRefCount() > 1)
        ;

    mm->freeGraphicsMemory(commandBuffer);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenTaskThatRequiresLargeResourceCountWhenItIsFlushedThenExecStorageIsResized) {
    std::vector<GraphicsAllocation *> graphicsAllocations;

    auto &execStorage = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->getExecStorage();
    execStorage.resize(0);

    for (auto id = 0; id < 10; id++) {
        auto graphicsAllocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
        csr->makeResident(*graphicsAllocation);
        graphicsAllocations.push_back(graphicsAllocation);
    }
    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});

    LinearStream cs(commandBuffer);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    csr->flush(batchBuffer, csr->getResidencyAllocations());

    EXPECT_EQ(11u, this->mock->execBuffer.buffer_count);
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
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    auto storedBase = cs.getCpuBase();
    auto storedGraphicsAllocation = cs.getGraphicsAllocation();
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    csr->flush(batchBuffer, csr->getResidencyAllocations());
    EXPECT_EQ(cs.getCpuBase(), storedBase);
    EXPECT_EQ(cs.getGraphicsAllocation(), storedGraphicsAllocation);

    mm->freeGraphicsMemory(dummyAllocation);
    mm->freeGraphicsMemory(commandBuffer);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenDrmCsrCreatedWithInactiveGemCloseWorkerPolicyThenThreadIsNotCreated) {
    TestedDrmCommandStreamReceiver<FamilyType> testedCsr(gemCloseWorkerMode::gemCloseWorkerInactive,
                                                         *this->executionEnvironment,
                                                         1);
    EXPECT_EQ(gemCloseWorkerMode::gemCloseWorkerInactive, testedCsr.peekGemCloseWorkerOperationMode());
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenDrmAllocationWhenGetBufferObjectToModifyIsCalledForAGivenHandleIdThenTheCorrespondingBufferObjectGetsModified) {
    auto size = 1024u;
    auto allocation = new DrmAllocation(0, GraphicsAllocation::AllocationType::UNKNOWN, nullptr, nullptr, size, (osHandle)0u, MemoryPool::MemoryNull);

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
    auto allocation = new DrmAllocation(0, GraphicsAllocation::AllocationType::UNKNOWN, buffer, nullptr, buffer->peekSize(), (osHandle)0u, MemoryPool::MemoryNull);
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
    auto allocation1 = new DrmAllocation(0, GraphicsAllocation::AllocationType::UNKNOWN, buffer1, nullptr, buffer1->peekSize(), (osHandle)0u, MemoryPool::MemoryNull);
    auto allocation2 = new DrmAllocation(0, GraphicsAllocation::AllocationType::UNKNOWN, buffer2, nullptr, buffer2->peekSize(), (osHandle)0u, MemoryPool::MemoryNull);
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
    auto allocation = new DrmAllocation(0, GraphicsAllocation::AllocationType::UNKNOWN, buffer, nullptr, buffer->peekSize(), (osHandle)0u, MemoryPool::MemoryNull);

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
    mock->ioctl_expected.total = 9;
    //3 fragments
    auto ptr = (void *)0x1001;
    auto size = MemoryConstants::pageSize * 10;
    auto graphicsAllocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, size}, ptr);

    auto offsetedPtr = (void *)((uintptr_t)ptr + size);
    auto size2 = MemoryConstants::pageSize - 1;

    auto graphicsAllocation2 = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, size2}, offsetedPtr);

    //graphicsAllocation2 reuses one fragment from graphicsAllocation
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

    csr->makeSurfacePackNonResident(csr->getResidencyAllocations());

    //check that each packet is not resident
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

    csr->makeSurfacePackNonResident(csr->getResidencyAllocations());

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
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
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

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenPrintBOsForSubmitWhenPrintThenProperValuesArePrinted) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.PrintBOsForSubmit.set(true);

    auto allocation1 = static_cast<DrmAllocation *>(mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize}));
    auto allocation2 = static_cast<DrmAllocation *>(mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize}));
    auto buffer = static_cast<DrmAllocation *>(mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize}));
    ASSERT_NE(nullptr, allocation1);
    ASSERT_NE(nullptr, allocation2);
    ASSERT_NE(nullptr, buffer);

    csr->makeResident(*allocation1);
    csr->makeResident(*allocation2);

    ResidencyContainer residency;
    residency.push_back(allocation1);
    residency.push_back(allocation2);

    testing::internal::CaptureStdout();

    static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->printBOsForSubmit(residency, *buffer);

    std::string output = testing::internal::GetCapturedStdout();

    std::vector<BufferObject *> bos;
    allocation1->makeBOsResident(&csr->getOsContext(), 0, &bos, true);
    allocation2->makeBOsResident(&csr->getOsContext(), 0, &bos, true);
    buffer->makeBOsResident(&csr->getOsContext(), 0, &bos, true);

    std::stringstream expected;
    expected << "Buffer object for submit\n";
    for (const auto &bo : bos) {
        expected << "BO-" << bo->peekHandle() << ", range: " << std::hex << bo->peekAddress() << " - " << ptrOffset(bo->peekAddress(), bo->peekSize()) << ", size: " << std::dec << bo->peekSize() << "\n";
    }
    expected << "\n";

    EXPECT_FALSE(output.compare(expected.str()));

    mm->freeGraphicsMemory(allocation1);
    mm->freeGraphicsMemory(allocation2);
    mm->freeGraphicsMemory(buffer);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, GivenFlushMultipleTimesThenSucceeds) {
    auto &cs = csr->getCS();
    auto commandBuffer = static_cast<DrmAllocation *>(cs.getGraphicsAllocation());

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    csr->flush(batchBuffer, csr->getResidencyAllocations());

    cs.replaceBuffer(commandBuffer->getUnderlyingBuffer(), commandBuffer->getUnderlyingBufferSize());
    cs.replaceGraphicsAllocation(commandBuffer);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer2{cs.getGraphicsAllocation(), 8, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
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
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer3{cs.getGraphicsAllocation(), 16, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    csr->flush(batchBuffer3, csr->getResidencyAllocations());
    csr->makeSurfacePackNonResident(csr->getResidencyAllocations());
    mm->freeGraphicsMemory(allocation);
    mm->freeGraphicsMemory(allocation2);

    csr->getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(commandBuffer2), REUSABLE_ALLOCATION);
    commandBuffer2 = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer2);
    cs.replaceBuffer(commandBuffer2->getUnderlyingBuffer(), commandBuffer2->getUnderlyingBufferSize());
    cs.replaceGraphicsAllocation(commandBuffer2);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer4{cs.getGraphicsAllocation(), 24, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    csr->flush(batchBuffer4, csr->getResidencyAllocations());
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, GivenNotEmptyBbWhenFlushingThenSucceeds) {
    int bbUsed = 16 * sizeof(uint32_t);

    auto &cs = csr->getCS();

    cs.getSpace(bbUsed);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    csr->flush(batchBuffer, csr->getResidencyAllocations());
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, GivenNotEmptyNotPaddedBbWhenFlushingThenSucceeds) {
    int bbUsed = 15 * sizeof(uint32_t);

    auto &cs = csr->getCS();

    cs.getSpace(bbUsed);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    csr->flush(batchBuffer, csr->getResidencyAllocations());
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, GivenNotAlignedWhenFlushingThenSucceeds) {
    auto &cs = csr->getCS();
    auto commandBuffer = static_cast<DrmAllocation *>(cs.getGraphicsAllocation());

    //make sure command buffer with offset is not page aligned
    ASSERT_NE(0u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & 0xFFF);
    ASSERT_EQ(4u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & 0x7F);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 4, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    csr->flush(batchBuffer, csr->getResidencyAllocations());
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, GivenCheckDrmFreeWhenFlushingThenSucceeds) {
    auto &cs = csr->getCS();
    auto commandBuffer = static_cast<DrmAllocation *>(cs.getGraphicsAllocation());

    //make sure command buffer with offset is not page aligned
    ASSERT_NE(0u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & 0xFFF);
    ASSERT_EQ(4u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & 0x7F);

    auto allocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});

    csr->makeResident(*allocation);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 4, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
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
    csr->makeSurfacePackNonResident(csr->getResidencyAllocations());
    EXPECT_EQ(0u, csr->getResidencyAllocations().size());

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
    csr->makeSurfacePackNonResident(csr->getResidencyAllocations());

    EXPECT_EQ(0u, csr->getResidencyAllocations().size());
    EXPECT_FALSE(allocation1->isResident(csr->getOsContext().getContextId()));

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

class DrmMockBuffer : public MockBufferStorage, public Buffer {
    using MockBufferStorage::device;

  public:
    static DrmMockBuffer *create() {
        char *data = static_cast<char *>(::alignedMalloc(128, 64));
        DrmAllocation *alloc = new (std::nothrow) DrmAllocation(0, GraphicsAllocation::AllocationType::UNKNOWN, nullptr, &data, sizeof(data), (osHandle)0, MemoryPool::MemoryNull);
        return new DrmMockBuffer(data, 128, alloc);
    }

    ~DrmMockBuffer() override {
        ::alignedFree(data);
        delete gfxAllocation;
    }

    DrmMockBuffer(char *data, size_t size, DrmAllocation *alloc)
        : Buffer(
              nullptr, MemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, MockBufferStorage::device.get()),
              CL_MEM_USE_HOST_PTR, 0, size, data, data,
              GraphicsAllocationHelper::toMultiGraphicsAllocation(alloc), true, false, false),
          data(data),
          gfxAllocation(alloc) {
    }

    void setArgStateful(void *memory, bool forceNonAuxMode, bool disableL3, bool alignSizeForAuxTranslation, bool isReadOnly, const Device &device, bool useGlobalAtomics, bool areMultipleSubDevicesInContext) override {
    }

  protected:
    char *data;
    DrmAllocation *gfxAllocation;
};

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, GivenMultipleResidencyRequestsWhenMakingNonResidentThenAllocationIsNotResident) {
    std::unique_ptr<Buffer> buffer(DrmMockBuffer::create());

    auto osContextId = csr->getOsContext().getContextId();
    auto graphicsAllocation = buffer->getGraphicsAllocation(rootDeviceIndex);
    ASSERT_FALSE(graphicsAllocation->isResident(osContextId));
    ASSERT_GT(buffer->getSize(), 0u);

    //make it resident 8 times
    for (int c = 0; c < 8; c++) {
        csr->makeResident(*graphicsAllocation);
        csr->processResidency(csr->getResidencyAllocations(), 0u);
        EXPECT_TRUE(graphicsAllocation->isResident(osContextId));
        EXPECT_EQ(graphicsAllocation->getResidencyTaskCount(osContextId), csr->peekTaskCount() + 1);
    }

    csr->makeNonResident(*graphicsAllocation);
    EXPECT_FALSE(graphicsAllocation->isResident(osContextId));

    csr->makeNonResident(*graphicsAllocation);
    EXPECT_FALSE(graphicsAllocation->isResident(osContextId));
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenDrmCommandStreamReceiverWhenMemoryManagerIsCreatedThenItHasHostMemoryValidationEnabledByDefault) {
    EXPECT_TRUE(mm->isValidateHostMemoryEnabled());
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenAllocationWithSingleBufferObjectWhenMakeResidentBufferObjectsIsCalledThenTheBufferObjectIsMadeResident) {
    auto size = 1024u;
    auto bo = this->createBO(size);
    BufferObjects bos{{bo}};
    auto allocation = new DrmAllocation(0, GraphicsAllocation::AllocationType::UNKNOWN, bos, nullptr, 0u, size, MemoryPool::LocalMemory);
    EXPECT_EQ(bo, allocation->getBO());

    makeResidentBufferObjects<FamilyType>(&csr->getOsContext(), allocation);
    EXPECT_TRUE(isResident<FamilyType>(bo));

    mm->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest,
                   givenWaitUserFenceFlagAndVmBindAvailableSetWhenDrmCsrFlushedThenExpectTaskCountStoredAsFlushStamp) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableUserFenceForCompletionWait.set(1);

    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    TestedDrmCommandStreamReceiver<FamilyType> *testedCsr =
        new TestedDrmCommandStreamReceiver<FamilyType>(gemCloseWorkerMode::gemCloseWorkerInactive,
                                                       *this->executionEnvironment,
                                                       1);
    EXPECT_TRUE(testedCsr->useUserFenceWait);
    device->resetCommandStreamReceiver(testedCsr);

    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{testedCsr->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};

    testedCsr->taskCount = 160u;
    testedCsr->flush(batchBuffer, testedCsr->getResidencyAllocations());

    EXPECT_EQ(160u, testedCsr->flushStamp->peekStamp());

    mm->freeGraphicsMemory(commandBuffer);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest,
                   givenNoWaitUserFenceFlagAndVmBindAvailableSetWhenDrmCsrFlushedThenExpectCommandBufferBoHandleAsFlushStamp) {
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    TestedDrmCommandStreamReceiver<FamilyType> *testedCsr =
        new TestedDrmCommandStreamReceiver<FamilyType>(gemCloseWorkerMode::gemCloseWorkerInactive,
                                                       *this->executionEnvironment,
                                                       1);
    EXPECT_FALSE(testedCsr->useUserFenceWait);
    device->resetCommandStreamReceiver(testedCsr);

    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{testedCsr->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};

    DrmAllocation *alloc = static_cast<DrmAllocation *>(cs.getGraphicsAllocation());
    auto boHandle = static_cast<FlushStamp>(alloc->getBO()->peekHandle());
    testedCsr->taskCount = 160u;

    testedCsr->flush(batchBuffer, testedCsr->getResidencyAllocations());

    EXPECT_EQ(boHandle, testedCsr->flushStamp->peekStamp());

    mm->freeGraphicsMemory(commandBuffer);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest,
                   givenWaitUserFenceFlagAndNoVmBindAvailableSetWhenDrmCsrFlushedThenExpectCommandBufferBoHandleAsFlushStamp) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableUserFenceForCompletionWait.set(1);

    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = false;

    TestedDrmCommandStreamReceiver<FamilyType> *testedCsr =
        new TestedDrmCommandStreamReceiver<FamilyType>(gemCloseWorkerMode::gemCloseWorkerInactive,
                                                       *this->executionEnvironment,
                                                       1);
    EXPECT_TRUE(testedCsr->useUserFenceWait);
    device->resetCommandStreamReceiver(testedCsr);

    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{testedCsr->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};

    DrmAllocation *alloc = static_cast<DrmAllocation *>(cs.getGraphicsAllocation());
    auto boHandle = static_cast<FlushStamp>(alloc->getBO()->peekHandle());
    testedCsr->taskCount = 160u;

    testedCsr->flush(batchBuffer, testedCsr->getResidencyAllocations());

    EXPECT_EQ(boHandle, testedCsr->flushStamp->peekStamp());

    mm->freeGraphicsMemory(commandBuffer);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenWaitUserFenceFlagNotSetWhenDrmCsrWaitsForFlushStampThenExpectUseDrmGemWaitCall) {
    TestedDrmCommandStreamReceiver<FamilyType> *testedCsr =
        new TestedDrmCommandStreamReceiver<FamilyType>(gemCloseWorkerMode::gemCloseWorkerInactive,
                                                       *this->executionEnvironment,
                                                       1);
    EXPECT_FALSE(testedCsr->useUserFenceWait);
    EXPECT_FALSE(testedCsr->isUsedNotifyEnableForPostSync());
    EXPECT_TRUE(testedCsr->useContextForUserFenceWait);
    device->resetCommandStreamReceiver(testedCsr);
    mock->ioctl_cnt.gemWait = 0;

    FlushStamp handleToWait = 123;
    testedCsr->waitForFlushStamp(handleToWait);

    EXPECT_EQ(1, mock->ioctl_cnt.gemWait);
    EXPECT_EQ(-1, mock->gemWaitTimeout);
    EXPECT_EQ(0u, testedCsr->waitUserFenceResult.called);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenGemWaitUsedWhenKmdTimeoutUsedWhenDrmCsrWaitsForFlushStampThenExpectUseDrmGemWaitCallAndOverrideTimeout) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.SetKmdWaitTimeout.set(1000);

    TestedDrmCommandStreamReceiver<FamilyType> *testedCsr =
        new TestedDrmCommandStreamReceiver<FamilyType>(gemCloseWorkerMode::gemCloseWorkerInactive,
                                                       *this->executionEnvironment,
                                                       1);
    EXPECT_FALSE(testedCsr->useUserFenceWait);
    EXPECT_FALSE(testedCsr->isUsedNotifyEnableForPostSync());
    EXPECT_TRUE(testedCsr->useContextForUserFenceWait);
    device->resetCommandStreamReceiver(testedCsr);
    mock->ioctl_cnt.gemWait = 0;

    FlushStamp handleToWait = 123;
    testedCsr->waitForFlushStamp(handleToWait);

    EXPECT_EQ(1, mock->ioctl_cnt.gemWait);
    EXPECT_EQ(1000, mock->gemWaitTimeout);
    EXPECT_EQ(0u, testedCsr->waitUserFenceResult.called);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest,
                   givenWaitUserFenceFlagSetAndVmBindAvailableWhenDrmCsrWaitsForFlushStampThenExpectUseDrmWaitUserFenceCallWithNonZeroContext) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableUserFenceForCompletionWait.set(1);

    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    TestedDrmCommandStreamReceiver<FamilyType> *testedCsr =
        new TestedDrmCommandStreamReceiver<FamilyType>(gemCloseWorkerMode::gemCloseWorkerInactive,
                                                       *this->executionEnvironment,
                                                       1);
    EXPECT_TRUE(testedCsr->useUserFenceWait);
    EXPECT_TRUE(testedCsr->isUsedNotifyEnableForPostSync());
    EXPECT_TRUE(testedCsr->useContextForUserFenceWait);
    device->resetCommandStreamReceiver(testedCsr);
    mock->ioctl_cnt.gemWait = 0;
    mock->isVmBindAvailableCall.called = 0u;

    auto osContextLinux = static_cast<const OsContextLinux *>(device->getDefaultEngine().osContext);
    std::vector<uint32_t> &drmCtxIds = const_cast<std::vector<uint32_t> &>(osContextLinux->getDrmContextIds());
    size_t drmCtxSize = drmCtxIds.size();
    for (uint32_t i = 0; i < drmCtxSize; i++) {
        drmCtxIds[i] = 5u + i;
    }

    FlushStamp handleToWait = 123;
    testedCsr->waitForFlushStamp(handleToWait);

    EXPECT_EQ(0, mock->ioctl_cnt.gemWait);
    EXPECT_EQ(1u, testedCsr->waitUserFenceResult.called);
    EXPECT_EQ(123u, testedCsr->waitUserFenceResult.waitValue);

    EXPECT_EQ(1u, mock->isVmBindAvailableCall.called);
    EXPECT_EQ(1u, mock->waitUserFenceCall.called);

    EXPECT_NE(0u, mock->waitUserFenceCall.ctxId);
    EXPECT_EQ(-1, mock->waitUserFenceCall.timeout);
    EXPECT_EQ(Drm::ValueWidth::U32, mock->waitUserFenceCall.dataWidth);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest,
                   givenWaitUserFenceFlagSetAndVmBindNotAvailableWhenDrmCsrWaitsForFlushStampThenExpectUseDrmGemWaitCall) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableUserFenceForCompletionWait.set(1);

    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = false;

    TestedDrmCommandStreamReceiver<FamilyType> *testedCsr =
        new TestedDrmCommandStreamReceiver<FamilyType>(gemCloseWorkerMode::gemCloseWorkerInactive,
                                                       *this->executionEnvironment,
                                                       1);
    EXPECT_TRUE(testedCsr->useUserFenceWait);
    EXPECT_TRUE(testedCsr->isUsedNotifyEnableForPostSync());
    EXPECT_TRUE(testedCsr->useContextForUserFenceWait);
    device->resetCommandStreamReceiver(testedCsr);
    mock->ioctl_cnt.gemWait = 0;
    mock->isVmBindAvailableCall.called = 0u;

    FlushStamp handleToWait = 123;
    testedCsr->waitForFlushStamp(handleToWait);

    EXPECT_EQ(1, mock->ioctl_cnt.gemWait);
    EXPECT_EQ(0u, testedCsr->waitUserFenceResult.called);

    EXPECT_EQ(1u, mock->isVmBindAvailableCall.called);
    EXPECT_EQ(0u, mock->waitUserFenceCall.called);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest,
                   givenNoWaitUserFenceFlagSetAndVmBindAvailableWhenDrmCsrWaitsForFlushStampThenExpectUseDrmGemWaitCall) {
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    TestedDrmCommandStreamReceiver<FamilyType> *testedCsr =
        new TestedDrmCommandStreamReceiver<FamilyType>(gemCloseWorkerMode::gemCloseWorkerInactive,
                                                       *this->executionEnvironment,
                                                       1);
    EXPECT_FALSE(testedCsr->useUserFenceWait);
    EXPECT_FALSE(testedCsr->isUsedNotifyEnableForPostSync());
    EXPECT_TRUE(testedCsr->useContextForUserFenceWait);
    device->resetCommandStreamReceiver(testedCsr);
    mock->ioctl_cnt.gemWait = 0;
    mock->isVmBindAvailableCall.called = 0u;

    FlushStamp handleToWait = 123;
    testedCsr->waitForFlushStamp(handleToWait);

    EXPECT_EQ(1, mock->ioctl_cnt.gemWait);
    EXPECT_EQ(0u, testedCsr->waitUserFenceResult.called);

    EXPECT_EQ(1u, mock->isVmBindAvailableCall.called);
    EXPECT_EQ(0u, mock->waitUserFenceCall.called);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest,
                   givenWaitUserFenceSetAndUseCtxFlagsNotSetAndVmBindAvailableWhenDrmCsrWaitsForFlushStampThenExpectUseDrmWaitUserFenceCallWithZeroContext) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableUserFenceForCompletionWait.set(1);
    DebugManager.flags.EnableUserFenceUseCtxId.set(0);
    DebugManager.flags.SetKmdWaitTimeout.set(1000);

    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    TestedDrmCommandStreamReceiver<FamilyType> *testedCsr =
        new TestedDrmCommandStreamReceiver<FamilyType>(gemCloseWorkerMode::gemCloseWorkerInactive,
                                                       *this->executionEnvironment,
                                                       1);
    EXPECT_TRUE(testedCsr->useUserFenceWait);
    EXPECT_TRUE(testedCsr->isUsedNotifyEnableForPostSync());
    EXPECT_FALSE(testedCsr->useContextForUserFenceWait);
    device->resetCommandStreamReceiver(testedCsr);
    mock->ioctl_cnt.gemWait = 0;
    mock->isVmBindAvailableCall.called = 0u;

    FlushStamp handleToWait = 123;
    testedCsr->waitForFlushStamp(handleToWait);

    EXPECT_EQ(0, mock->ioctl_cnt.gemWait);
    EXPECT_EQ(1u, testedCsr->waitUserFenceResult.called);
    EXPECT_EQ(123u, testedCsr->waitUserFenceResult.waitValue);

    EXPECT_EQ(1u, mock->waitUserFenceCall.called);
    EXPECT_EQ(1u, mock->isVmBindAvailableCall.called);

    EXPECT_EQ(0u, mock->waitUserFenceCall.ctxId);
    EXPECT_EQ(1000, mock->waitUserFenceCall.timeout);
    EXPECT_EQ(Drm::ValueWidth::U32, mock->waitUserFenceCall.dataWidth);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest,
                   givenWaitUserFenceSetWhenDrmCsrIsCreatedThenUseNotifyEnableFlagIsSet) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableUserFenceForCompletionWait.set(1);

    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    std::unique_ptr<TestedDrmCommandStreamReceiver<FamilyType>> testedCsr =
        std::make_unique<TestedDrmCommandStreamReceiver<FamilyType>>(gemCloseWorkerMode::gemCloseWorkerInactive,
                                                                     *this->executionEnvironment,
                                                                     1);

    EXPECT_TRUE(testedCsr->useUserFenceWait);
    EXPECT_TRUE(testedCsr->isUsedNotifyEnableForPostSync());
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest,
                   givenWaitUserFenceNotSetWhenDrmCsrIsCreatedThenUseNotifyEnableFlagIsNotSet) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableUserFenceForCompletionWait.set(0);

    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    std::unique_ptr<TestedDrmCommandStreamReceiver<FamilyType>> testedCsr =
        std::make_unique<TestedDrmCommandStreamReceiver<FamilyType>>(gemCloseWorkerMode::gemCloseWorkerInactive,
                                                                     *this->executionEnvironment,
                                                                     1);

    EXPECT_FALSE(testedCsr->useUserFenceWait);
    EXPECT_FALSE(testedCsr->isUsedNotifyEnableForPostSync());
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest,
                   givenWaitUserFenceNotSetAndOverrideNotifyEnableSetWhenDrmCsrIsCreatedThenUseNotifyEnableFlagIsSet) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableUserFenceForCompletionWait.set(0);
    DebugManager.flags.OverrideNotifyEnableForTagUpdatePostSync.set(1);

    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    std::unique_ptr<TestedDrmCommandStreamReceiver<FamilyType>> testedCsr =
        std::make_unique<TestedDrmCommandStreamReceiver<FamilyType>>(gemCloseWorkerMode::gemCloseWorkerInactive,
                                                                     *this->executionEnvironment,
                                                                     1);

    EXPECT_FALSE(testedCsr->useUserFenceWait);
    EXPECT_TRUE(testedCsr->isUsedNotifyEnableForPostSync());
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest,
                   givenWaitUserFenceSetAndOverrideNotifyEnableNotSetWhenDrmCsrIsCreatedThenUseNotifyEnableFlagIsNotSet) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableUserFenceForCompletionWait.set(1);
    DebugManager.flags.OverrideNotifyEnableForTagUpdatePostSync.set(0);

    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    std::unique_ptr<TestedDrmCommandStreamReceiver<FamilyType>> testedCsr =
        std::make_unique<TestedDrmCommandStreamReceiver<FamilyType>>(gemCloseWorkerMode::gemCloseWorkerInactive,
                                                                     *this->executionEnvironment,
                                                                     1);

    EXPECT_TRUE(testedCsr->useUserFenceWait);
    EXPECT_FALSE(testedCsr->isUsedNotifyEnableForPostSync());
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenVmBindNotAvailableWhenCheckingForKmdWaitModeActiveThenReturnTrue) {
    auto testDrmCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    mock->isVmBindAvailableCall.called = 0u;
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = false;

    EXPECT_TRUE(testDrmCsr->isKmdWaitModeActive());
    EXPECT_EQ(1u, mock->isVmBindAvailableCall.called);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenVmBindAvailableUseWaitCallTrueWhenCheckingForKmdWaitModeActiveThenReturnTrue) {
    auto testDrmCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    mock->isVmBindAvailableCall.called = 0u;
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;
    testDrmCsr->useUserFenceWait = true;

    EXPECT_TRUE(testDrmCsr->isKmdWaitModeActive());
    EXPECT_EQ(1u, mock->isVmBindAvailableCall.called);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenVmBindAvailableUseWaitCallFalseWhenCheckingForKmdWaitModeActiveThenReturnFalse) {
    auto testDrmCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    mock->isVmBindAvailableCall.called = 0u;
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;
    testDrmCsr->useUserFenceWait = false;

    EXPECT_FALSE(testDrmCsr->isKmdWaitModeActive());
    EXPECT_EQ(1u, mock->isVmBindAvailableCall.called);
}
