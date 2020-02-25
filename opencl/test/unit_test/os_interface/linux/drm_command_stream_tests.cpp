/*
 * Copyright (C) 2017-2020 Intel Corporation
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
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/linux/os_interface.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/source/helpers/memory_properties_flags_helpers.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/os_interface/linux/drm_command_stream.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/helpers/dispatch_flags_helper.h"
#include "opencl/test/unit_test/helpers/execution_environment_helper.h"
#include "opencl/test/unit_test/helpers/hw_parse.h"
#include "opencl/test/unit_test/mocks/linux/mock_drm_command_stream_receiver.h"
#include "opencl/test/unit_test/mocks/mock_gmm.h"
#include "opencl/test/unit_test/mocks/mock_gmm_page_table_mngr.h"
#include "opencl/test/unit_test/mocks/mock_host_ptr_manager.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/mocks/mock_submissions_aggregator.h"
#include "opencl/test/unit_test/os_interface/linux/drm_command_stream_fixture.h"
#include "test.h"

#include "drm/i915_drm.h"
#include "gmock/gmock.h"

using namespace NEO;

ACTION_P(copyIoctlParam, dstValue) {
    *dstValue = *static_cast<decltype(dstValue)>(arg1);
    return 0;
};

HWTEST_TEMPLATED_F(DrmCommandStreamTest, givenFlushStampWhenWaitCalledThenWaitForSpecifiedBoHandle) {
    FlushStamp handleToWait = 123;
    drm_i915_gem_wait expectedWait = {};
    drm_i915_gem_wait calledWait = {};
    expectedWait.bo_handle = static_cast<uint32_t>(handleToWait);
    expectedWait.timeout_ns = -1;

    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_WAIT, ::testing::_))
        .Times(1)
        .WillRepeatedly(copyIoctlParam(&calledWait));

    csr->waitForFlushStamp(handleToWait);
    EXPECT_TRUE(memcmp(&expectedWait, &calledWait, sizeof(drm_i915_gem_wait)) == 0);
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, makeResident) {
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_USERPTR, ::testing::_))
        .Times(0);
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_EXECBUFFER2, ::testing::_))
        .Times(0);
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_GEM_CLOSE, ::testing::_))
        .Times(0);
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_WAIT, ::testing::_))
        .Times(0);
    DrmAllocation graphicsAllocation(0, GraphicsAllocation::AllocationType::UNKNOWN, nullptr, nullptr, 1024, (osHandle)1u, MemoryPool::MemoryNull);
    csr->makeResident(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, makeResidentTwiceTheSame) {
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_USERPTR, ::testing::_))
        .Times(0);
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_EXECBUFFER2, ::testing::_))
        .Times(0);
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_GEM_CLOSE, ::testing::_))
        .Times(0);
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_WAIT, ::testing::_))
        .Times(0);

    DrmAllocation graphicsAllocation(0, GraphicsAllocation::AllocationType::UNKNOWN, nullptr, nullptr, 1024, (osHandle)1u, MemoryPool::MemoryNull);

    csr->makeResident(graphicsAllocation);
    csr->makeResident(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, makeResidentSizeZero) {
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_USERPTR, ::testing::_))
        .Times(0);
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_EXECBUFFER2, ::testing::_))
        .Times(0);
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_GEM_CLOSE, ::testing::_))
        .Times(0);
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_WAIT, ::testing::_))
        .Times(0);

    DrmAllocation graphicsAllocation(0, GraphicsAllocation::AllocationType::UNKNOWN, nullptr, nullptr, 0, (osHandle)1u, MemoryPool::MemoryNull);

    csr->makeResident(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, makeResidentResized) {
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_USERPTR, ::testing::_))
        .Times(0);
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_EXECBUFFER2, ::testing::_))
        .Times(0);
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_GEM_CLOSE, ::testing::_))
        .Times(0);
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_WAIT, ::testing::_))
        .Times(0);

    DrmAllocation graphicsAllocation(0, GraphicsAllocation::AllocationType::UNKNOWN, nullptr, nullptr, 1024, (osHandle)1u, MemoryPool::MemoryNull);
    DrmAllocation graphicsAllocation2(0, GraphicsAllocation::AllocationType::UNKNOWN, nullptr, nullptr, 8192, (osHandle)1u, MemoryPool::MemoryNull);

    csr->makeResident(graphicsAllocation);
    csr->makeResident(graphicsAllocation2);
}

// matcher to check batch buffer offset and len on execbuffer2 call
MATCHER_P2(BoExecFlushEq, batch_start_offset, batch_len, "") {
    drm_i915_gem_execbuffer2 *exec2 = (drm_i915_gem_execbuffer2 *)arg;

    return (exec2->batch_start_offset == batch_start_offset) && (exec2->batch_len == batch_len);
}

// matcher to check DrmContextId
MATCHER_P2(BoExecFlushContextEq, drmContextId, numExecs, "") {
    auto execBuff2 = reinterpret_cast<drm_i915_gem_execbuffer2 *>(arg);

    bool allExecsWithTheSameId = (execBuff2->buffer_count == numExecs);
    allExecsWithTheSameId &= (execBuff2->rsvd1 == drmContextId);

    auto execObjects = reinterpret_cast<drm_i915_gem_exec_object2 *>(execBuff2->buffers_ptr);
    for (uint32_t i = 0; i < execBuff2->buffer_count - 1; i++) {
        allExecsWithTheSameId &= (execObjects[i].rsvd1 == drmContextId);
    }

    return allExecsWithTheSameId;
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, Flush) {
    auto expectedSize = alignUp(8u, MemoryConstants::cacheLineSize); // bbEnd
    int boHandle = 123;
    auto setBoHandle = [&](unsigned long request, void *arg) {
        auto userptr = static_cast<drm_i915_gem_userptr *>(arg);
        userptr->handle = boHandle;
        return 0;
    };

    ::testing::InSequence inSequence;
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_USERPTR, ::testing::_))
        .Times(1)
        .WillRepeatedly(::testing::Invoke(setBoHandle))
        .RetiresOnSaturation();
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_EXECBUFFER2, BoExecFlushEq(0u, expectedSize)))
        .Times(1)
        .WillRepeatedly(::testing::Return(0))
        .RetiresOnSaturation();
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_WAIT, ::testing::_))
        .Times(2)
        .RetiresOnSaturation();
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_GEM_CLOSE, ::testing::_))
        .Times(1)
        .RetiresOnSaturation();

    auto &cs = csr->getCS();
    auto commandBuffer = static_cast<DrmAllocation *>(cs.getGraphicsAllocation());
    ASSERT_NE(nullptr, commandBuffer);
    ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) & 0xFFF);
    EXPECT_EQ(boHandle, commandBuffer->getBO()->peekHandle());

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};
    auto availableSpacePriorToFlush = cs.getAvailableSpace();
    csr->flush(batchBuffer, csr->getResidencyAllocations());
    EXPECT_EQ(static_cast<uint64_t>(boHandle), csr->obtainCurrentFlushStamp());
    EXPECT_NE(cs.getCpuBase(), nullptr);
    EXPECT_EQ(availableSpacePriorToFlush, cs.getAvailableSpace());
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, givenDrmContextIdWhenFlushingThenSetIdToAllExecBuffersAndObjects) {
    uint32_t expectedDrmContextId = 321;
    uint32_t numAllocations = 3;

    auto createdContextId = [&expectedDrmContextId](unsigned long request, void *arg) {
        auto contextCreate = static_cast<drm_i915_gem_context_create *>(arg);
        contextCreate->ctx_id = expectedDrmContextId;
        return 0;
    };

    auto allocation1 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    auto allocation2 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    csr->makeResident(*allocation1);
    csr->makeResident(*allocation2);

    EXPECT_CALL(*mock, ioctl(::testing::_, ::testing::_)).WillRepeatedly(::testing::Return(0)).RetiresOnSaturation();

    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_CONTEXT_CREATE, ::testing::_))
        .Times(1)
        .WillRepeatedly(::testing::Invoke(createdContextId))
        .RetiresOnSaturation();

    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_EXECBUFFER2, BoExecFlushContextEq(expectedDrmContextId, numAllocations)))
        .Times(1)
        .WillRepeatedly(::testing::Return(0))
        .RetiresOnSaturation();

    osContext = std::make_unique<OsContextLinux>(*mock, 1, 1,
                                                 HwHelper::get(platformDevices[0]->platform.eRenderCoreFamily).getGpgpuEngineInstances(*platformDevices[0])[0],
                                                 PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]), false);
    csr->setupContext(*osContext);

    auto &cs = csr->getCS();

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};
    csr->flush(batchBuffer, csr->getResidencyAllocations());

    memoryManager->freeGraphicsMemory(allocation1);
    memoryManager->freeGraphicsMemory(allocation2);
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, FlushWithLowPriorityContext) {
    auto expectedSize = alignUp(8u, MemoryConstants::cacheLineSize); // bbEnd

    ::testing::InSequence inSequence;

    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_USERPTR, ::testing::_))
        .Times(1)
        .WillRepeatedly(::testing::Return(0))
        .RetiresOnSaturation();
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_EXECBUFFER2, BoExecFlushEq(0u, expectedSize)))
        .Times(1)
        .WillRepeatedly(::testing::Return(0))
        .RetiresOnSaturation();
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_WAIT, ::testing::_))
        .Times(2)
        .RetiresOnSaturation();
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_GEM_CLOSE, ::testing::_))
        .Times(1)
        .RetiresOnSaturation();

    auto &cs = csr->getCS();
    auto commandBuffer = static_cast<DrmAllocation *>(cs.getGraphicsAllocation());

    ASSERT_NE(nullptr, commandBuffer);
    ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) & 0xFFF);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, true, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};
    csr->flush(batchBuffer, csr->getResidencyAllocations());
    EXPECT_NE(cs.getCpuBase(), nullptr);
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, FlushInvalidAddress) {
    ::testing::InSequence inSequence;

    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_USERPTR, ::testing::_))
        .Times(0)
        .RetiresOnSaturation();
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_EXECBUFFER2, BoExecFlushEq(0u, 8u)))
        .Times(0)
        .RetiresOnSaturation();
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_WAIT, ::testing::_))
        .Times(0)
        .RetiresOnSaturation();
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_GEM_CLOSE, ::testing::_))
        .Times(0)
        .RetiresOnSaturation();

    //allocate command buffer manually
    char *commandBuffer = new (std::nothrow) char[1024];
    ASSERT_NE(nullptr, commandBuffer);
    DrmAllocation commandBufferAllocation(0, GraphicsAllocation::AllocationType::COMMAND_BUFFER, nullptr, commandBuffer, 1024, (osHandle)1u, MemoryPool::MemoryNull);
    LinearStream cs(&commandBufferAllocation);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};
    csr->flush(batchBuffer, csr->getResidencyAllocations());
    delete[] commandBuffer;
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, FlushNotEmptyBB) {
    uint32_t bbUsed = 16 * sizeof(uint32_t);
    auto expectedSize = alignUp(bbUsed + 8, MemoryConstants::cacheLineSize); // bbUsed + bbEnd

    ::testing::InSequence inSequence;

    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_USERPTR, ::testing::_))
        .Times(1)
        .WillRepeatedly(::testing::Return(0))
        .RetiresOnSaturation();
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_EXECBUFFER2, BoExecFlushEq(0u, expectedSize)))
        .Times(1)
        .WillRepeatedly(::testing::Return(0))
        .RetiresOnSaturation();
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_WAIT, ::testing::_))
        .Times(2)
        .RetiresOnSaturation();
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_GEM_CLOSE, ::testing::_))
        .Times(1)
        .RetiresOnSaturation();

    auto &cs = csr->getCS();
    cs.getSpace(bbUsed);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};
    csr->flush(batchBuffer, csr->getResidencyAllocations());
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, FlushNotEmptyNotPaddedBB) {
    uint32_t bbUsed = 15 * sizeof(uint32_t);

    ::testing::InSequence inSequence;

    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_USERPTR, ::testing::_))
        .Times(1)
        .WillRepeatedly(::testing::Return(0))
        .RetiresOnSaturation();
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_EXECBUFFER2, BoExecFlushEq(0u, bbUsed + 4)))
        .Times(1)
        .WillRepeatedly(::testing::Return(0))
        .RetiresOnSaturation();
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_WAIT, ::testing::_))
        .Times(2)
        .RetiresOnSaturation();
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_GEM_CLOSE, ::testing::_))
        .Times(1)
        .RetiresOnSaturation();

    auto &cs = csr->getCS();
    cs.getSpace(bbUsed);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};
    csr->flush(batchBuffer, csr->getResidencyAllocations());
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, FlushNotAligned) {
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_USERPTR, ::testing::_))
        .Times(1)
        .WillRepeatedly(::testing::Return(0));

    auto &cs = csr->getCS();
    auto commandBuffer = static_cast<DrmAllocation *>(cs.getGraphicsAllocation());

    //make sure command buffer with offset is not page aligned
    ASSERT_NE(0u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & (this->alignment - 1));
    ASSERT_EQ(4u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & 0x7F);

    auto expectedSize = alignUp(8u, MemoryConstants::cacheLineSize); // bbEnd

    EXPECT_CALL(*mock, ioctl(
                           DRM_IOCTL_I915_GEM_EXECBUFFER2,
                           BoExecFlushEq((reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & (this->alignment - 1), expectedSize)))
        .Times(1)
        .WillRepeatedly(::testing::Return(0));
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_GEM_CLOSE, ::testing::_))
        .Times(1);
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_WAIT, ::testing::_))
        .Times(2);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 4, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};
    csr->flush(batchBuffer, csr->getResidencyAllocations());
}

ACTION_P(UserptrSetHandle, _set_handle) {
    struct drm_i915_gem_userptr *userptr = reinterpret_cast<drm_i915_gem_userptr *>(arg1);
    userptr->handle = _set_handle;
}

MATCHER_P(GemCloseEq, handle, "") {
    drm_gem_close *gemClose = (drm_gem_close *)arg;

    return (gemClose->handle == handle);
}

MATCHER(BoExecFlushCheckFlags, "") {
    drm_i915_gem_execbuffer2 *exec2 = (drm_i915_gem_execbuffer2 *)arg;
    drm_i915_gem_exec_object2 *exec_objects = (drm_i915_gem_exec_object2 *)exec2->buffers_ptr;

    for (unsigned int i = 0; i < exec2->buffer_count; i++) {
        EXPECT_TRUE(exec_objects[i].flags == (EXEC_OBJECT_PINNED | EXEC_OBJECT_SUPPORTS_48B_ADDRESS));
    }

    return true;
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, FlushCheckFlags) {
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_USERPTR, ::testing::_))
        .WillRepeatedly(::testing::Return(0));
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_GEM_CLOSE, ::testing::_))
        .WillRepeatedly(::testing::Return(0));
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_WAIT, ::testing::_))
        .WillRepeatedly(::testing::Return(0));

    auto &cs = csr->getCS();

    EXPECT_CALL(*mock, ioctl(
                           DRM_IOCTL_I915_GEM_EXECBUFFER2,
                           BoExecFlushCheckFlags()))
        .Times(1)
        .WillRepeatedly(::testing::Return(0));

    DrmAllocation allocation(0, GraphicsAllocation::AllocationType::UNKNOWN, nullptr, (void *)0x7FFFFFFF, 1024, (osHandle)0u, MemoryPool::MemoryNull);
    DrmAllocation allocation2(0, GraphicsAllocation::AllocationType::UNKNOWN, nullptr, (void *)0x307FFFFFFF, 1024, (osHandle)0u, MemoryPool::MemoryNull);
    csr->makeResident(allocation);
    csr->makeResident(allocation2);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};
    csr->flush(batchBuffer, csr->getResidencyAllocations());
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, CheckDrmFree) {
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_USERPTR, ::testing::_))
        .Times(1)
        .WillOnce(::testing::DoAll(UserptrSetHandle(17), ::testing::Return(0)));

    auto &cs = csr->getCS();
    auto commandBuffer = static_cast<DrmAllocation *>(cs.getGraphicsAllocation());

    //make sure command buffer with offset is not page aligned
    ASSERT_NE(0u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & (this->alignment - 1));
    ASSERT_EQ(4u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & 0x7F);

    auto expectedSize = alignUp(8u, MemoryConstants::cacheLineSize); // bbEnd

    EXPECT_CALL(*mock, ioctl(
                           DRM_IOCTL_I915_GEM_EXECBUFFER2,
                           BoExecFlushEq((reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & (this->alignment - 1), expectedSize)))
        .Times(1)
        .WillRepeatedly(::testing::Return(0));
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_GEM_CLOSE, GemCloseEq(17u)))
        .Times(1);
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_WAIT, ::testing::_))
        .Times(2);

    DrmAllocation allocation(0, GraphicsAllocation::AllocationType::UNKNOWN, nullptr, nullptr, 1024, (osHandle)0u, MemoryPool::MemoryNull);

    csr->makeResident(allocation);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 4, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};
    csr->flush(batchBuffer, csr->getResidencyAllocations());
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, GIVENCSRWHENgetDMTHENNotNull) {
    Drm *pDrm = nullptr;
    if (csr->getOSInterface()) {
        pDrm = csr->getOSInterface()->get()->getDrm();
    }
    ASSERT_NE(nullptr, pDrm);
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, CheckDrmFreeCloseFailed) {
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_USERPTR, ::testing::_))
        .Times(1)
        .WillOnce(::testing::DoAll(UserptrSetHandle(17), ::testing::Return(0)));

    auto &cs = csr->getCS();
    auto commandBuffer = static_cast<DrmAllocation *>(cs.getGraphicsAllocation());

    //make sure command buffer with offset is not page aligned
    ASSERT_NE(0u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & (this->alignment - 1));
    ASSERT_EQ(4u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & 0x7F);

    auto expectedSize = alignUp(8u, MemoryConstants::cacheLineSize); // bbEnd

    EXPECT_CALL(*mock, ioctl(
                           DRM_IOCTL_I915_GEM_EXECBUFFER2,
                           BoExecFlushEq((reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & (this->alignment - 1), expectedSize)))
        .Times(1)
        .WillRepeatedly(::testing::Return(0));
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_GEM_CLOSE, GemCloseEq(17u)))
        .Times(1)
        .WillOnce(::testing::Return(-1));
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_WAIT, ::testing::_))
        .Times(2);
    DrmAllocation allocation(0, GraphicsAllocation::AllocationType::UNKNOWN, nullptr, nullptr, 1024, (osHandle)0u, MemoryPool::MemoryNull);

    csr->makeResident(allocation);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 4, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};
    csr->flush(batchBuffer, csr->getResidencyAllocations());
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenDefaultDrmCSRWhenItIsCreatedThenGemCloseWorkerModeIsInactive) {
    EXPECT_EQ(gemCloseWorkerMode::gemCloseWorkerInactive, static_cast<const DrmCommandStreamReceiver<FamilyType> *>(csr)->peekGemCloseWorkerOperationMode());
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenCommandStreamWhenItIsFlushedWithGemCloseWorkerInDefaultModeThenWorkerDecreasesTheRefCount) {
    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    auto storedBase = cs.getCpuBase();
    auto storedGraphicsAllocation = cs.getGraphicsAllocation();
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};
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
        auto graphicsAllocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
        csr->makeResident(*graphicsAllocation);
        graphicsAllocations.push_back(graphicsAllocation);
    }
    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

    LinearStream cs(commandBuffer);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};
    csr->flush(batchBuffer, csr->getResidencyAllocations());

    EXPECT_EQ(11u, this->mock->execBuffer.buffer_count);
    mm->freeGraphicsMemory(commandBuffer);
    for (auto graphicsAllocation : graphicsAllocations) {
        mm->freeGraphicsMemory(graphicsAllocation);
    }
    EXPECT_EQ(11u, execStorage.size());
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenGemCloseWorkerInactiveModeWhenMakeResidentIsCalledThenRefCountsAreNotUpdated) {
    auto dummyAllocation = static_cast<DrmAllocation *>(mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize}));

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
    auto allocation = static_cast<DrmAllocation *>(mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize}));
    auto allocation2 = static_cast<DrmAllocation *>(mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize}));

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
    auto commandBuffer = static_cast<DrmAllocation *>(mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize}));
    auto dummyAllocation = static_cast<DrmAllocation *>(mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize}));
    ASSERT_NE(nullptr, commandBuffer);
    ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) & 0xFFF);
    LinearStream cs(commandBuffer);

    csr->makeResident(*dummyAllocation);
    csr->makeResident(*dummyAllocation);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    auto storedBase = cs.getCpuBase();
    auto storedGraphicsAllocation = cs.getGraphicsAllocation();
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};
    csr->flush(batchBuffer, csr->getResidencyAllocations());
    EXPECT_EQ(cs.getCpuBase(), storedBase);
    EXPECT_EQ(cs.getGraphicsAllocation(), storedGraphicsAllocation);

    mm->freeGraphicsMemory(dummyAllocation);
    mm->freeGraphicsMemory(commandBuffer);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenDrmCsrCreatedWithInactiveGemCloseWorkerPolicyThenThreadIsNotCreated) {
    TestedDrmCommandStreamReceiver<FamilyType> testedCsr(gemCloseWorkerMode::gemCloseWorkerInactive,
                                                         *this->executionEnvironment);
    EXPECT_EQ(gemCloseWorkerMode::gemCloseWorkerInactive, testedCsr.peekGemCloseWorkerOperationMode());
}

class DrmCommandStreamBatchingTests : public DrmCommandStreamEnhancedTest {
  public:
    DrmAllocation *tagAllocation;
    DrmAllocation *preemptionAllocation;
    GraphicsAllocation *tmpAllocation;

    template <typename GfxFamily>
    void SetUpT() {
        DrmCommandStreamEnhancedTest::SetUpT<GfxFamily>();
        if (PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]) == PreemptionMode::MidThread) {
            tmpAllocation = GlobalMockSipProgram::sipProgram->getAllocation();
            GlobalMockSipProgram::sipProgram->resetAllocation(device->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize}));
        }
        tagAllocation = static_cast<DrmAllocation *>(device->getDefaultEngine().commandStreamReceiver->getTagAllocation());
        preemptionAllocation = static_cast<DrmAllocation *>(device->getDefaultEngine().commandStreamReceiver->getPreemptionAllocation());
    }

    template <typename GfxFamily>
    void TearDownT() {
        if (PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]) == PreemptionMode::MidThread) {
            device->getMemoryManager()->freeGraphicsMemory((GlobalMockSipProgram::sipProgram)->getAllocation());
            GlobalMockSipProgram::sipProgram->resetAllocation(tmpAllocation);
        }
        DrmCommandStreamEnhancedTest::TearDownT<GfxFamily>();
    }
};

HWTEST_TEMPLATED_F(DrmCommandStreamBatchingTests, givenCSRWhenFlushIsCalledThenProperFlagsArePassed) {
    mock->reset();
    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    auto dummyAllocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) & 0xFFF);
    LinearStream cs(commandBuffer);

    csr->makeResident(*dummyAllocation);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};
    csr->flush(batchBuffer, csr->getResidencyAllocations());

    int ioctlExecCnt = 1;
    int ioctlUserPtrCnt = 2;

    auto engineFlag = static_cast<OsContextLinux &>(csr->getOsContext()).getEngineFlag();

    EXPECT_EQ(ioctlExecCnt + ioctlUserPtrCnt, this->mock->ioctl_cnt.total);
    EXPECT_EQ(ioctlExecCnt, this->mock->ioctl_cnt.execbuffer2);
    EXPECT_EQ(ioctlUserPtrCnt, this->mock->ioctl_cnt.gemUserptr);
    uint64_t flags = engineFlag | I915_EXEC_NO_RELOC;
    EXPECT_EQ(flags, this->mock->execBuffer.flags);

    mm->freeGraphicsMemory(dummyAllocation);
    mm->freeGraphicsMemory(commandBuffer);
}

HWTEST_TEMPLATED_F(DrmCommandStreamBatchingTests, givenCsrWhenDispatchPolicyIsSetToBatchingThenCommandBufferIsNotSubmitted) {
    mock->reset();
    csr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    auto dummyAllocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) & 0xFFF);
    IndirectHeap cs(commandBuffer);

    csr->makeResident(*dummyAllocation);

    csr->setTagAllocation(tagAllocation);
    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(device->getHardwareInfo());

    csr->flushTask(cs, 0u, cs, cs, cs, 0u, dispatchFlags, *device);

    //make sure command buffer is recorded
    auto &cmdBuffers = mockedSubmissionsAggregator->peekCommandBuffers();
    EXPECT_FALSE(cmdBuffers.peekIsEmpty());
    EXPECT_NE(nullptr, cmdBuffers.peekHead());

    //preemption allocation
    size_t csrSurfaceCount = (device->getPreemptionMode() == PreemptionMode::MidThread) ? 2 : 0;

    auto recordedCmdBuffer = cmdBuffers.peekHead();
    EXPECT_EQ(3u + csrSurfaceCount, recordedCmdBuffer->surfaces.size());

    //try to find all allocations
    auto elementInVector = std::find(recordedCmdBuffer->surfaces.begin(), recordedCmdBuffer->surfaces.end(), dummyAllocation);
    EXPECT_NE(elementInVector, recordedCmdBuffer->surfaces.end());

    elementInVector = std::find(recordedCmdBuffer->surfaces.begin(), recordedCmdBuffer->surfaces.end(), commandBuffer);
    EXPECT_NE(elementInVector, recordedCmdBuffer->surfaces.end());

    elementInVector = std::find(recordedCmdBuffer->surfaces.begin(), recordedCmdBuffer->surfaces.end(), tagAllocation);
    EXPECT_NE(elementInVector, recordedCmdBuffer->surfaces.end());

    EXPECT_EQ(static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->commandStream.getGraphicsAllocation(), recordedCmdBuffer->batchBuffer.commandBufferAllocation);

    int ioctlUserPtrCnt = 3;

    EXPECT_EQ(ioctlUserPtrCnt, this->mock->ioctl_cnt.total);
    EXPECT_EQ(ioctlUserPtrCnt, this->mock->ioctl_cnt.gemUserptr);

    EXPECT_EQ(0u, this->mock->execBuffer.flags);

    csr->flushBatchedSubmissions();

    mm->freeGraphicsMemory(dummyAllocation);
    mm->freeGraphicsMemory(commandBuffer);
}

HWTEST_TEMPLATED_F(DrmCommandStreamBatchingTests, givenRecordedCommandBufferWhenItIsSubmittedThenFlushTaskIsProperlyCalled) {
    mock->reset();
    csr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    IndirectHeap cs(commandBuffer);

    csr->setTagAllocation(tagAllocation);
    auto &submittedCommandBuffer = csr->getCS(1024);
    //use some bytes
    submittedCommandBuffer.getSpace(4);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(device->getHardwareInfo());
    dispatchFlags.guardCommandBufferWithPipeControl = true;

    csr->flushTask(cs, 0u, cs, cs, cs, 0u, dispatchFlags, *device);

    auto &cmdBuffers = mockedSubmissionsAggregator->peekCommandBuffers();
    auto storedCommandBuffer = cmdBuffers.peekHead();

    ResidencyContainer copyOfResidency = storedCommandBuffer->surfaces;
    copyOfResidency.push_back(storedCommandBuffer->batchBuffer.commandBufferAllocation);

    csr->flushBatchedSubmissions();

    EXPECT_TRUE(cmdBuffers.peekIsEmpty());

    auto commandBufferGraphicsAllocation = submittedCommandBuffer.getGraphicsAllocation();
    EXPECT_FALSE(commandBufferGraphicsAllocation->isResident(csr->getOsContext().getContextId()));

    //preemption allocation
    size_t csrSurfaceCount = (device->getPreemptionMode() == PreemptionMode::MidThread) ? 2 : 0;

    //validate that submited command buffer has what we want
    EXPECT_EQ(3u + csrSurfaceCount, this->mock->execBuffer.buffer_count);
    EXPECT_EQ(4u, this->mock->execBuffer.batch_start_offset);
    EXPECT_EQ(submittedCommandBuffer.getUsed(), this->mock->execBuffer.batch_len);

    drm_i915_gem_exec_object2 *exec_objects = (drm_i915_gem_exec_object2 *)this->mock->execBuffer.buffers_ptr;

    for (unsigned int i = 0; i < this->mock->execBuffer.buffer_count; i++) {
        int handle = exec_objects[i].handle;

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
    EXPECT_EQ(ioctlExecCnt, this->mock->ioctl_cnt.execbuffer2);
    EXPECT_EQ(ioctlUserPtrCnt, this->mock->ioctl_cnt.gemUserptr);
    EXPECT_EQ(ioctlExecCnt + ioctlUserPtrCnt, this->mock->ioctl_cnt.total);

    mm->freeGraphicsMemory(commandBuffer);
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

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, makeResident) {
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

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, makeResidentOnly) {
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

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, makeResidentTwice) {
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

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, makeResidentTwiceWhenFragmentStorage) {
    auto ptr = (void *)0x1001;
    auto size = MemoryConstants::pageSize * 10;
    auto reqs = MockHostPtrManager::getAllocationRequirements(ptr, size);
    auto allocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, size}, ptr);

    ASSERT_EQ(3u, allocation->fragmentsStorage.fragmentCount);

    csr->makeResident(*allocation);
    csr->makeResident(*allocation);

    csr->processResidency(csr->getResidencyAllocations(), 0u);
    for (int i = 0; i < maxFragmentsCount; i++) {
        ASSERT_EQ(allocation->fragmentsStorage.fragmentStorageData[i].cpuPtr,
                  reqs.allocationFragments[i].allocationPtr);
        auto bo = allocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->bo;
        EXPECT_TRUE(isResident<FamilyType>(bo));
        EXPECT_EQ(1u, bo->getRefCount());
    }

    csr->makeNonResident(*allocation);
    for (int i = 0; i < maxFragmentsCount; i++) {
        auto bo = allocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->bo;
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
    auto graphicsAllocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, size}, ptr);

    auto offsetedPtr = (void *)((uintptr_t)ptr + size);
    auto size2 = MemoryConstants::pageSize - 1;

    auto graphicsAllocation2 = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, size2}, offsetedPtr);

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

    auto reqs = MockHostPtrManager::getAllocationRequirements(ptr, size);

    auto allocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, size}, ptr);

    ASSERT_EQ(3u, allocation->fragmentsStorage.fragmentCount);

    csr->makeResident(*allocation);
    csr->processResidency(csr->getResidencyAllocations(), 0u);

    for (int i = 0; i < maxFragmentsCount; i++) {
        ASSERT_EQ(allocation->fragmentsStorage.fragmentStorageData[i].cpuPtr,
                  reqs.allocationFragments[i].allocationPtr);
        auto bo = allocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->bo;
        EXPECT_TRUE(isResident<FamilyType>(bo));
        EXPECT_EQ(1u, bo->getRefCount());
    }
    csr->makeNonResident(*allocation);
    for (int i = 0; i < maxFragmentsCount; i++) {
        auto bo = allocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->bo;
        EXPECT_FALSE(isResident<FamilyType>(bo));
        EXPECT_EQ(1u, bo->getRefCount());
    }
    mm->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, GivenAllocationsContainingDifferentCountOfFragmentsWhenAllocationIsMadeResidentThenAllFragmentsAreMadeResident) {
    auto ptr = (void *)0x1001;
    auto size = MemoryConstants::pageSize;
    auto size2 = 100u;

    auto reqs = MockHostPtrManager::getAllocationRequirements(ptr, size);

    auto allocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, size}, ptr);

    ASSERT_EQ(2u, allocation->fragmentsStorage.fragmentCount);
    ASSERT_EQ(2u, reqs.requiredFragmentsCount);

    csr->makeResident(*allocation);
    csr->processResidency(csr->getResidencyAllocations(), 0u);

    for (unsigned int i = 0; i < reqs.requiredFragmentsCount; i++) {
        ASSERT_EQ(allocation->fragmentsStorage.fragmentStorageData[i].cpuPtr,
                  reqs.allocationFragments[i].allocationPtr);
        auto bo = allocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->bo;
        EXPECT_TRUE(isResident<FamilyType>(bo));
        EXPECT_EQ(1u, bo->getRefCount());
    }
    csr->makeNonResident(*allocation);
    for (unsigned int i = 0; i < reqs.requiredFragmentsCount; i++) {
        auto bo = allocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->bo;
        EXPECT_FALSE(isResident<FamilyType>(bo));
        EXPECT_EQ(1u, bo->getRefCount());
    }
    mm->freeGraphicsMemory(allocation);
    csr->getResidencyAllocations().clear();

    auto allocation2 = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, size2}, ptr);
    reqs = MockHostPtrManager::getAllocationRequirements(ptr, size2);

    ASSERT_EQ(1u, allocation2->fragmentsStorage.fragmentCount);
    ASSERT_EQ(1u, reqs.requiredFragmentsCount);

    csr->makeResident(*allocation2);
    csr->processResidency(csr->getResidencyAllocations(), 0u);

    for (unsigned int i = 0; i < reqs.requiredFragmentsCount; i++) {
        ASSERT_EQ(allocation2->fragmentsStorage.fragmentStorageData[i].cpuPtr,
                  reqs.allocationFragments[i].allocationPtr);
        auto bo = allocation2->fragmentsStorage.fragmentStorageData[i].osHandleStorage->bo;
        EXPECT_TRUE(isResident<FamilyType>(bo));
        EXPECT_EQ(1u, bo->getRefCount());
    }
    csr->makeNonResident(*allocation2);
    for (unsigned int i = 0; i < reqs.requiredFragmentsCount; i++) {
        auto bo = allocation2->fragmentsStorage.fragmentStorageData[i].osHandleStorage->bo;
        EXPECT_FALSE(isResident<FamilyType>(bo));
        EXPECT_EQ(1u, allocation2->fragmentsStorage.fragmentStorageData[i].osHandleStorage->bo->getRefCount());
    }
    mm->freeGraphicsMemory(allocation2);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, GivenTwoAllocationsWhenBackingStorageIsTheSameThenMakeResidentShouldAddOnlyOneLocation) {
    auto ptr = (void *)0x1000;
    auto size = MemoryConstants::pageSize;
    auto ptr2 = (void *)0x1000;

    auto allocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, size}, ptr);
    auto allocation2 = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, size}, ptr2);

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

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, Flush) {
    auto &cs = csr->getCS();
    auto commandBuffer = static_cast<DrmAllocation *>(cs.getGraphicsAllocation());
    ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) & 0xFFF);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};
    csr->flush(batchBuffer, csr->getResidencyAllocations());
    EXPECT_NE(cs.getCpuBase(), nullptr);
    EXPECT_NE(cs.getGraphicsAllocation(), nullptr);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, ClearResidencyWhenFlushNotCalled) {
    auto allocation1 = static_cast<DrmAllocation *>(mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize}));
    auto allocation2 = static_cast<DrmAllocation *>(mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize}));
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

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, FlushMultipleTimes) {
    auto &cs = csr->getCS();
    auto commandBuffer = static_cast<DrmAllocation *>(cs.getGraphicsAllocation());

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};
    csr->flush(batchBuffer, csr->getResidencyAllocations());

    cs.replaceBuffer(commandBuffer->getUnderlyingBuffer(), commandBuffer->getUnderlyingBufferSize());
    cs.replaceGraphicsAllocation(commandBuffer);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer2{cs.getGraphicsAllocation(), 8, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};
    csr->flush(batchBuffer2, csr->getResidencyAllocations());

    auto allocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    ASSERT_NE(nullptr, allocation);
    auto allocation2 = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    ASSERT_NE(nullptr, allocation2);

    csr->makeResident(*allocation);
    csr->makeResident(*allocation2);

    csr->getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(commandBuffer), REUSABLE_ALLOCATION);

    auto commandBuffer2 = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer2);
    cs.replaceBuffer(commandBuffer2->getUnderlyingBuffer(), commandBuffer2->getUnderlyingBufferSize());
    cs.replaceGraphicsAllocation(commandBuffer2);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer3{cs.getGraphicsAllocation(), 16, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};
    csr->flush(batchBuffer3, csr->getResidencyAllocations());
    csr->makeSurfacePackNonResident(csr->getResidencyAllocations());
    mm->freeGraphicsMemory(allocation);
    mm->freeGraphicsMemory(allocation2);

    csr->getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(commandBuffer2), REUSABLE_ALLOCATION);
    commandBuffer2 = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer2);
    cs.replaceBuffer(commandBuffer2->getUnderlyingBuffer(), commandBuffer2->getUnderlyingBufferSize());
    cs.replaceGraphicsAllocation(commandBuffer2);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer4{cs.getGraphicsAllocation(), 24, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};
    csr->flush(batchBuffer4, csr->getResidencyAllocations());
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, FlushNotEmptyBB) {
    int bbUsed = 16 * sizeof(uint32_t);

    auto &cs = csr->getCS();

    cs.getSpace(bbUsed);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};
    csr->flush(batchBuffer, csr->getResidencyAllocations());
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, FlushNotEmptyNotPaddedBB) {
    int bbUsed = 15 * sizeof(uint32_t);

    auto &cs = csr->getCS();

    cs.getSpace(bbUsed);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};
    csr->flush(batchBuffer, csr->getResidencyAllocations());
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, FlushNotAligned) {
    auto &cs = csr->getCS();
    auto commandBuffer = static_cast<DrmAllocation *>(cs.getGraphicsAllocation());

    //make sure command buffer with offset is not page aligned
    ASSERT_NE(0u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & 0xFFF);
    ASSERT_EQ(4u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & 0x7F);

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 4, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};
    csr->flush(batchBuffer, csr->getResidencyAllocations());
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, CheckDrmFree) {
    auto &cs = csr->getCS();
    auto commandBuffer = static_cast<DrmAllocation *>(cs.getGraphicsAllocation());

    //make sure command buffer with offset is not page aligned
    ASSERT_NE(0u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & 0xFFF);
    ASSERT_EQ(4u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & 0x7F);

    auto allocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

    csr->makeResident(*allocation);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 4, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};
    csr->flush(batchBuffer, csr->getResidencyAllocations());
    csr->makeNonResident(*allocation);
    mm->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, MakeResidentClearResidencyAllocationsInCommandStreamReceiver) {
    auto allocation1 = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    auto allocation2 = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

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
    auto allocation1 = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

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

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, makeNonResidentOnMemObjectCallsDrmCSMakeNonResidentWithGraphicsAllocation) {
    auto allocation1 = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{0x1000});
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

class DrmMockBuffer : public Buffer {
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

    DrmMockBuffer(char *data, size_t size, DrmAllocation *alloc) : Buffer(nullptr, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_USE_HOST_PTR, 0, 0), CL_MEM_USE_HOST_PTR, 0, size, data, data, alloc, true, false, false),
                                                                   data(data),
                                                                   gfxAllocation(alloc) {
    }

    void setArgStateful(void *memory, bool forceNonAuxMode, bool disableL3, bool alignSizeForAuxTranslation, bool isReadOnly) override {
    }

  protected:
    char *data;
    DrmAllocation *gfxAllocation;
};

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, BufferResidency) {
    std::unique_ptr<Buffer> buffer(DrmMockBuffer::create());

    auto osContextId = csr->getOsContext().getContextId();

    ASSERT_FALSE(buffer->getGraphicsAllocation()->isResident(osContextId));
    ASSERT_GT(buffer->getSize(), 0u);

    //make it resident 8 times
    for (int c = 0; c < 8; c++) {
        csr->makeResident(*buffer->getGraphicsAllocation());
        csr->processResidency(csr->getResidencyAllocations(), 0u);
        EXPECT_TRUE(buffer->getGraphicsAllocation()->isResident(osContextId));
        EXPECT_EQ(buffer->getGraphicsAllocation()->getResidencyTaskCount(osContextId), csr->peekTaskCount() + 1);
    }

    csr->makeNonResident(*buffer->getGraphicsAllocation());
    EXPECT_FALSE(buffer->getGraphicsAllocation()->isResident(osContextId));

    csr->makeNonResident(*buffer->getGraphicsAllocation());
    EXPECT_FALSE(buffer->getGraphicsAllocation()->isResident(osContextId));
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

    makeResidentBufferObjects<FamilyType>(allocation);
    EXPECT_TRUE(isResident<FamilyType>(bo));

    mm->freeGraphicsMemory(allocation);
}

template <typename GfxFamily>
struct MockDrmCsr : public DrmCommandStreamReceiver<GfxFamily> {
    using DrmCommandStreamReceiver<GfxFamily>::DrmCommandStreamReceiver;
};

HWTEST_TEMPLATED_F(DrmCommandStreamTest, givenDrmCommandStreamReceiverWhenCreatePageTableMngrIsCalledThenCreatePageTableManager) {
    executionEnvironment.prepareRootDeviceEnvironments(2);
    executionEnvironment.rootDeviceEnvironments[1]->initGmm();
    executionEnvironment.rootDeviceEnvironments[1]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment.rootDeviceEnvironments[1]->osInterface->get()->setDrm(new DrmMockCustom());
    auto csr = std::make_unique<MockDrmCsr<FamilyType>>(executionEnvironment, 1, gemCloseWorkerMode::gemCloseWorkerActive);
    auto pageTableManager = csr->createPageTableManager();
    EXPECT_EQ(executionEnvironment.rootDeviceEnvironments[1]->pageTableManager.get(), pageTableManager);
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, givenPageTableManagerAndMapTrueWhenUpdateAuxTableIsCalledThenItReturnsTrue) {
    auto mockMngr = new MockGmmPageTableMngr();
    executionEnvironment.rootDeviceEnvironments[0]->pageTableManager.reset(mockMngr);
    auto gmm = std::make_unique<MockGmm>();
    GMM_DDI_UPDATEAUXTABLE ddiUpdateAuxTable = {};
    EXPECT_CALL(*mockMngr, updateAuxTable(::testing::_)).Times(1).WillOnce(::testing::Invoke([&](const GMM_DDI_UPDATEAUXTABLE *arg) {ddiUpdateAuxTable = *arg; return GMM_SUCCESS; }));
    auto result = executionEnvironment.rootDeviceEnvironments[0]->pageTableManager->updateAuxTable(0, gmm.get(), true);
    EXPECT_EQ(ddiUpdateAuxTable.BaseGpuVA, 0ull);
    EXPECT_EQ(ddiUpdateAuxTable.BaseResInfo, gmm->gmmResourceInfo->peekHandle());
    EXPECT_EQ(ddiUpdateAuxTable.DoNotWait, true);
    EXPECT_EQ(ddiUpdateAuxTable.Map, 1u);

    EXPECT_TRUE(result);
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, givenPageTableManagerAndMapFalseWhenUpdateAuxTableIsCalledThenItReturnsTrue) {
    auto mockMngr = new MockGmmPageTableMngr();
    executionEnvironment.rootDeviceEnvironments[0]->pageTableManager.reset(mockMngr);
    auto gmm = std::make_unique<MockGmm>();
    GMM_DDI_UPDATEAUXTABLE ddiUpdateAuxTable = {};
    EXPECT_CALL(*mockMngr, updateAuxTable(::testing::_)).Times(1).WillOnce(::testing::Invoke([&](const GMM_DDI_UPDATEAUXTABLE *arg) {ddiUpdateAuxTable = *arg; return GMM_SUCCESS; }));
    auto result = executionEnvironment.rootDeviceEnvironments[0]->pageTableManager->updateAuxTable(0, gmm.get(), false);
    EXPECT_EQ(ddiUpdateAuxTable.BaseGpuVA, 0ull);
    EXPECT_EQ(ddiUpdateAuxTable.BaseResInfo, gmm->gmmResourceInfo->peekHandle());
    EXPECT_EQ(ddiUpdateAuxTable.DoNotWait, true);
    EXPECT_EQ(ddiUpdateAuxTable.Map, 0u);

    EXPECT_TRUE(result);
}
