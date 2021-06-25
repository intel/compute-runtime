/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

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

HWTEST_TEMPLATED_F(DrmCommandStreamTest, WhenMakingResidentThenSucceeds) {
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

HWTEST_TEMPLATED_F(DrmCommandStreamTest, WhenMakingResidentTwiceThenSucceeds) {
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

HWTEST_TEMPLATED_F(DrmCommandStreamTest, GivenSizeZeroWhenMakingResidentTwiceThenSucceeds) {
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

HWTEST_TEMPLATED_F(DrmCommandStreamTest, GivenResizedWhenMakingResidentTwiceThenSucceeds) {
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

HWTEST_TEMPLATED_F(DrmCommandStreamTest, WhenFlushingThenAvailableSpaceDoesNotChange) {
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
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    auto availableSpacePriorToFlush = cs.getAvailableSpace();
    csr->flush(batchBuffer, csr->getResidencyAllocations());
    EXPECT_EQ(static_cast<uint64_t>(boHandle), csr->obtainCurrentFlushStamp());
    EXPECT_NE(cs.getCpuBase(), nullptr);
    EXPECT_EQ(availableSpacePriorToFlush, cs.getAvailableSpace());
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, givenPrintIndicesEnabledWhenFlushThenPrintIndices) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.PrintDeviceAndEngineIdOnSubmission.set(true);

    auto &cs = csr->getCS();
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};

    ::testing::internal::CaptureStdout();
    csr->flush(batchBuffer, csr->getResidencyAllocations());
    const std::string engineType = EngineHelpers::engineTypeToString(csr->getOsContext().getEngineType());
    const std::string engineUsage = EngineHelpers::engineUsageToString(csr->getOsContext().getEngineUsage());
    std::ostringstream expectedValue;
    expectedValue << "Submission to RootDevice Index: " << csr->getRootDeviceIndex()
                  << ", Sub-Devices Mask: " << csr->getOsContext().getDeviceBitfield().to_ulong()
                  << ", EngineId: " << csr->getOsContext().getEngineType()
                  << " (" << engineType << ", " << engineUsage << ")\n";
    EXPECT_THAT(::testing::internal::GetCapturedStdout(), ::testing::HasSubstr(expectedValue.str()));
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, givenDrmContextIdWhenFlushingThenSetIdToAllExecBuffersAndObjects) {
    uint32_t expectedDrmContextId = 321;
    uint32_t numAllocations = 3;

    auto createdContextId = [&expectedDrmContextId](unsigned long request, void *arg) {
        auto contextCreate = static_cast<drm_i915_gem_context_create *>(arg);
        contextCreate->ctx_id = expectedDrmContextId;
        return 0;
    };

    auto allocation1 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    auto allocation2 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    csr->makeResident(*allocation1);
    csr->makeResident(*allocation2);

    EXPECT_CALL(*mock, ioctl(::testing::_, ::testing::_)).WillRepeatedly(::testing::Return(0)).RetiresOnSaturation();

    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT, ::testing::_))
        .Times(1)
        .WillRepeatedly(::testing::Invoke(createdContextId))
        .RetiresOnSaturation();

    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_EXECBUFFER2, BoExecFlushContextEq(expectedDrmContextId, numAllocations)))
        .Times(1)
        .WillRepeatedly(::testing::Return(0))
        .RetiresOnSaturation();

    osContext = std::make_unique<OsContextLinux>(*mock, 1, 1,
                                                 HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*defaultHwInfo)[0],
                                                 PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo),
                                                 false);
    osContext->ensureContextInitialized();
    csr->setupContext(*osContext);

    auto &cs = csr->getCS();

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    csr->flush(batchBuffer, csr->getResidencyAllocations());

    memoryManager->freeGraphicsMemory(allocation1);
    memoryManager->freeGraphicsMemory(allocation2);
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, GivenLowPriorityContextWhenFlushingThenSucceeds) {
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
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, true, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    csr->flush(batchBuffer, csr->getResidencyAllocations());
    EXPECT_NE(cs.getCpuBase(), nullptr);
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, GivenInvalidAddressWhenFlushingThenSucceeds) {
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
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    csr->flush(batchBuffer, csr->getResidencyAllocations());
    delete[] commandBuffer;
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, GivenNotEmptyBbWhenFlushingThenSucceeds) {
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
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    csr->flush(batchBuffer, csr->getResidencyAllocations());
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, GivenNotEmptyNotPaddedBbWhenFlushingThenSucceeds) {
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
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    csr->flush(batchBuffer, csr->getResidencyAllocations());
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, GivenNotAlignedWhenFlushingThenSucceeds) {
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

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 4, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
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

HWTEST_TEMPLATED_F(DrmCommandStreamTest, GivenCheckFlagsWhenFlushingThenSucceeds) {
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
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    csr->flush(batchBuffer, csr->getResidencyAllocations());
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, GivenCheckDrmFreeWhenFlushingThenSucceeds) {
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
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 4, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    csr->flush(batchBuffer, csr->getResidencyAllocations());
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, WhenGettingDrmThenNonNullPointerIsReturned) {
    Drm *pDrm = nullptr;
    if (csr->getOSInterface()) {
        pDrm = csr->getOSInterface()->getDriverModel()->as<Drm>();
    }
    ASSERT_NE(nullptr, pDrm);
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, GivenCheckDrmFreeCloseFailedWhenFlushingThenSucceeds) {
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
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 4, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    csr->flush(batchBuffer, csr->getResidencyAllocations());
}

class DrmCommandStreamBatchingTests : public DrmCommandStreamEnhancedTest {
  public:
    DrmAllocation *preemptionAllocation;

    template <typename GfxFamily>
    void SetUpT() {
        DrmCommandStreamEnhancedTest::SetUpT<GfxFamily>();
        preemptionAllocation = static_cast<DrmAllocation *>(device->getDefaultEngine().commandStreamReceiver->getPreemptionAllocation());
    }

    template <typename GfxFamily>
    void TearDownT() {
        DrmCommandStreamEnhancedTest::TearDownT<GfxFamily>();
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
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
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

    csr->flushTask(cs, 0u, cs, cs, cs, 0u, dispatchFlags, *device);

    //make sure command buffer is recorded
    auto &cmdBuffers = mockedSubmissionsAggregator->peekCommandBuffers();
    EXPECT_FALSE(cmdBuffers.peekIsEmpty());
    EXPECT_NE(nullptr, cmdBuffers.peekHead());

    //preemption allocation
    size_t csrSurfaceCount = (device->getPreemptionMode() == PreemptionMode::MidThread) ? 2 : 0;
    csrSurfaceCount += testedCsr->globalFenceAllocation ? 1 : 0;
    csrSurfaceCount += testedCsr->clearColorAllocation ? 1 : 0;

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

    EXPECT_EQ(0u, this->mock->execBuffer.flags);

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

    csr->flushTask(cs, 0u, cs, cs, cs, 0u, dispatchFlags, *device);

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
    int ioctlUserPtrCnt = (device->getPreemptionMode() == PreemptionMode::MidThread) ? 3 : 2;
    ioctlUserPtrCnt += testedCsr->clearColorAllocation ? 1 : 0;
    EXPECT_EQ(ioctlExecCnt, this->mock->ioctl_cnt.execbuffer2);
    EXPECT_EQ(ioctlUserPtrCnt, this->mock->ioctl_cnt.gemUserptr);
    EXPECT_EQ(ioctlExecCnt + ioctlUserPtrCnt, this->mock->ioctl_cnt.total);

    mm->freeGraphicsMemory(commandBuffer);
}

struct DrmCommandStreamDirectSubmissionTest : public DrmCommandStreamEnhancedTest {
    template <typename GfxFamily>
    void SetUpT() {
        DebugManager.flags.EnableDirectSubmission.set(1u);
        DebugManager.flags.DirectSubmissionDisableMonitorFence.set(0);
        DrmCommandStreamEnhancedTest::SetUpT<GfxFamily>();
        auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
        auto engineType = device->getDefaultEngine().osContext->getEngineType();
        hwInfo->capabilityTable.directSubmissionEngines.data[engineType].engineSupported = true;
        csr->initDirectSubmission(*device.get(), *device->getDefaultEngine().osContext);
    }

    template <typename GfxFamily>
    void TearDownT() {
        this->dbgState.reset();
        DrmCommandStreamEnhancedTest::TearDownT<GfxFamily>();
    }

    DebugManagerStateRestore restorer;
};

struct DrmCommandStreamBlitterDirectSubmissionTest : public DrmCommandStreamDirectSubmissionTest {
    template <typename GfxFamily>
    void SetUpT() {
        DebugManager.flags.DirectSubmissionOverrideBlitterSupport.set(1u);
        DebugManager.flags.DirectSubmissionOverrideRenderSupport.set(0u);
        DebugManager.flags.DirectSubmissionOverrideComputeSupport.set(0u);

        DrmCommandStreamDirectSubmissionTest::SetUpT<GfxFamily>();

        osContext.reset(OsContext::create(device->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(),
                                          0, device->getDeviceBitfield(), EngineTypeUsage{aub_stream::ENGINE_BCS, EngineUsage::Regular}, PreemptionMode::ThreadGroup,
                                          false));
        osContext->ensureContextInitialized();
        csr->initDirectSubmission(*device.get(), *osContext.get());
    }

    template <typename GfxFamily>
    void TearDownT() {
        DrmCommandStreamDirectSubmissionTest::TearDownT<GfxFamily>();
    }

    std::unique_ptr<OsContext> osContext;
};

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
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
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

HWTEST_TEMPLATED_F(DrmCommandStreamDirectSubmissionTest, givenEnabledDirectSubmissionWhenFlushThenCommandBufferAllocationIsResident) {
    mock->bindAvailable = true;
    auto &cs = csr->getCS();
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 4, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    uint8_t bbStart[64];
    batchBuffer.endCmdPtr = &bbStart[0];

    csr->flush(batchBuffer, csr->getResidencyAllocations());

    auto memoryOperationsInterface = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface.get();
    EXPECT_EQ(memoryOperationsInterface->isResident(device.get(), *batchBuffer.commandBufferAllocation), MemoryOperationsStatus::SUCCESS);

    auto directSubmission = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->directSubmission.get();
    ASSERT_NE(nullptr, directSubmission);
    static_cast<MockDrmDirectSubmission<FamilyType> *>(directSubmission)->currentTagData.tagValue = 0u;
}

HWTEST_TEMPLATED_F(DrmCommandStreamBlitterDirectSubmissionTest, givenEnabledDirectSubmissionOnBlitterWhenFlushThenFlushStampIsNotUpdated) {
    auto &cs = csr->getCS();
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
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

HWTEST_TEMPLATED_F(DrmCommandStreamBlitterDirectSubmissionTest, givenEnabledDirectSubmissionOnBlitterWhenFlushThenCommandBufferAllocationIsResident) {
    mock->bindAvailable = true;
    auto &cs = csr->getCS();
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 4, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    uint8_t bbStart[64];
    batchBuffer.endCmdPtr = &bbStart[0];

    csr->flush(batchBuffer, csr->getResidencyAllocations());

    auto memoryOperationsInterface = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface.get();
    EXPECT_EQ(memoryOperationsInterface->isResident(device.get(), *batchBuffer.commandBufferAllocation), MemoryOperationsStatus::SUCCESS);

    auto directSubmission = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->blitterDirectSubmission.get();
    ASSERT_NE(nullptr, directSubmission);
    static_cast<MockDrmBlitterDirectSubmission<FamilyType> *>(directSubmission)->currentTagData.tagValue = 0u;

    EXPECT_EQ(nullptr, static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr)->directSubmission.get());
}

template <typename GfxFamily>
struct MockDrmCsr : public DrmCommandStreamReceiver<GfxFamily> {
    using DrmCommandStreamReceiver<GfxFamily>::DrmCommandStreamReceiver;
    using DrmCommandStreamReceiver<GfxFamily>::dispatchMode;
};

HWTEST_TEMPLATED_F(DrmCommandStreamTest, givenDrmCommandStreamReceiverWhenCreatePageTableManagerIsCalledThenCreatePageTableManager) {
    executionEnvironment.prepareRootDeviceEnvironments(2);
    executionEnvironment.rootDeviceEnvironments[1]->setHwInfo(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[1]->initGmm();
    executionEnvironment.rootDeviceEnvironments[1]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment.rootDeviceEnvironments[1]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(new DrmMockCustom()));
    auto csr = std::make_unique<MockDrmCsr<FamilyType>>(executionEnvironment, 1, 1, gemCloseWorkerMode::gemCloseWorkerActive);
    auto pageTableManager = csr->createPageTableManager();
    EXPECT_EQ(executionEnvironment.rootDeviceEnvironments[1]->pageTableManager.get(), pageTableManager);
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
