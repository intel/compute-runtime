/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "hw_cmds.h"
#include "runtime/command_stream/device_command_stream.h"
#include "runtime/command_stream/linear_stream.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/options.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/os_interface/os_context.h"
#include "runtime/os_interface/linux/drm_allocation.h"
#include "runtime/os_interface/linux/drm_buffer_object.h"
#include "runtime/os_interface/linux/drm_command_stream.h"
#include "runtime/os_interface/linux/drm_memory_manager.h"
#include "runtime/os_interface/linux/os_interface.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/gen_common/gen_cmd_parse_base.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/mocks/mock_program.h"
#include "unit_tests/mocks/mock_submissions_aggregator.h"
#include "unit_tests/os_interface/linux/device_command_stream_fixture.h"
#include "test.h"
#include "drm/i915_drm.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <iostream>
#include <memory>

using namespace OCLRT;

class DrmCommandStreamFixture {
  public:
    DeviceCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME> *csr = nullptr;
    DrmMemoryManager *mm = nullptr;
    DrmMockImpl *mock;
    const int mockFd = 33;

    void SetUp() {
        osContext = std::make_unique<OsContext>(nullptr);
        this->dbgState = new DebugManagerStateRestore();
        //make sure this is disabled, we don't want test this now
        DebugManager.flags.EnableForcePin.set(false);

        this->mock = new DrmMockImpl(mockFd);

        csr = new DrmCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>(*platformDevices[0], mock, executionEnvironment, gemCloseWorkerMode::gemCloseWorkerActive);
        ASSERT_NE(nullptr, csr);

        // Memory manager creates pinBB with ioctl, expect one call
        EXPECT_CALL(*mock, ioctl(::testing::_, ::testing::_))
            .Times(1);
        mm = static_cast<DrmMemoryManager *>(csr->createMemoryManager(false));
        ::testing::Mock::VerifyAndClearExpectations(mock);

        //assert we have memory manager
        ASSERT_NE(nullptr, mm);
    }

    void TearDown() {
        mm->waitForDeletions();
        mm->peekGemCloseWorker()->close(true);
        delete csr;
        ::testing::Mock::VerifyAndClearExpectations(mock);
        // Memory manager closes pinBB with ioctl, expect one call
        EXPECT_CALL(*mock, ioctl(::testing::_, ::testing::_))
            .Times(::testing::AtLeast(1));
        delete mm;
        delete this->mock;
        this->mock = 0;
        delete dbgState;
    }
    static const uint64_t alignment = MemoryConstants::allocationAlignment;
    DebugManagerStateRestore *dbgState;
    ExecutionEnvironment executionEnvironment;
    std::unique_ptr<OsContext> osContext;
};

typedef Test<DrmCommandStreamFixture> DrmCommandStreamTest;

ACTION_P(copyIoctlParam, dstValue) {
    *dstValue = *static_cast<decltype(dstValue)>(arg1);
    return 0;
};
TEST_F(DrmCommandStreamTest, givenFlushStampWhenWaitCalledThenWaitForSpecifiedBoHandle) {
    FlushStamp handleToWait = 123;
    drm_i915_gem_wait expectedWait = {};
    drm_i915_gem_wait calledWait = {};
    expectedWait.bo_handle = static_cast<uint32_t>(handleToWait);
    expectedWait.timeout_ns = -1;

    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_WAIT, ::testing::_))
        .Times(1)
        .WillRepeatedly(copyIoctlParam(&calledWait));

    csr->waitForFlushStamp(handleToWait, *osContext);
    EXPECT_TRUE(memcmp(&expectedWait, &calledWait, sizeof(drm_i915_gem_wait)) == 0);
}

TEST_F(DrmCommandStreamTest, makeResident) {
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_USERPTR, ::testing::_))
        .Times(0);
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_EXECBUFFER2, ::testing::_))
        .Times(0);
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_GEM_CLOSE, ::testing::_))
        .Times(0);
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_WAIT, ::testing::_))
        .Times(0);
    DrmAllocation graphicsAllocation(nullptr, nullptr, 1024, MemoryPool::MemoryNull);
    csr->makeResident(graphicsAllocation);
}

TEST_F(DrmCommandStreamTest, makeResidentTwiceTheSame) {
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_USERPTR, ::testing::_))
        .Times(0);
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_EXECBUFFER2, ::testing::_))
        .Times(0);
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_GEM_CLOSE, ::testing::_))
        .Times(0);
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_WAIT, ::testing::_))
        .Times(0);

    DrmAllocation graphicsAllocation(nullptr, nullptr, 1024, MemoryPool::MemoryNull);

    csr->makeResident(graphicsAllocation);
    csr->makeResident(graphicsAllocation);
}

TEST_F(DrmCommandStreamTest, makeResidentSizeZero) {
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_USERPTR, ::testing::_))
        .Times(0);
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_EXECBUFFER2, ::testing::_))
        .Times(0);
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_GEM_CLOSE, ::testing::_))
        .Times(0);
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_WAIT, ::testing::_))
        .Times(0);

    DrmAllocation graphicsAllocation(nullptr, nullptr, 0, MemoryPool::MemoryNull);

    csr->makeResident(graphicsAllocation);
}

TEST_F(DrmCommandStreamTest, makeResidentResized) {
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_USERPTR, ::testing::_))
        .Times(0);
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_EXECBUFFER2, ::testing::_))
        .Times(0);
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_GEM_CLOSE, ::testing::_))
        .Times(0);
    EXPECT_CALL(*mock, ioctl(DRM_IOCTL_I915_GEM_WAIT, ::testing::_))
        .Times(0);

    DrmAllocation graphicsAllocation(nullptr, nullptr, 1024, MemoryPool::MemoryNull);
    DrmAllocation graphicsAllocation2(nullptr, nullptr, 8192, MemoryPool::MemoryNull);

    csr->makeResident(graphicsAllocation);
    csr->makeResident(graphicsAllocation2);
}

// matcher to check batch buffer offset and len on execbuffer2 call
MATCHER_P2(BoExecFlushEq, batch_start_offset, batch_len, "") {
    drm_i915_gem_execbuffer2 *exec2 = (drm_i915_gem_execbuffer2 *)arg;

    return (exec2->batch_start_offset == batch_start_offset) && (exec2->batch_len == batch_len);
}

TEST_F(DrmCommandStreamTest, Flush) {
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

    csr->addBatchBufferEnd(cs, nullptr);
    csr->alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto availableSpacePriorToFlush = cs.getAvailableSpace();
    auto flushStamp = csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr, *osContext);
    EXPECT_EQ(static_cast<uint64_t>(boHandle), flushStamp);
    EXPECT_NE(cs.getCpuBase(), nullptr);
    EXPECT_EQ(availableSpacePriorToFlush, cs.getAvailableSpace());
}

TEST_F(DrmCommandStreamTest, FlushWithLowPriorityContext) {
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

    csr->addBatchBufferEnd(cs, nullptr);
    csr->alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, true, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr, *osContext);
    EXPECT_NE(cs.getCpuBase(), nullptr);
}

TEST_F(DrmCommandStreamTest, FlushInvalidAddress) {
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
    DrmAllocation commandBufferAllocation(nullptr, commandBuffer, 1024, MemoryPool::MemoryNull);
    LinearStream cs(&commandBufferAllocation);

    csr->addBatchBufferEnd(cs, nullptr);
    csr->alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr, *osContext);
    delete[] commandBuffer;
}

TEST_F(DrmCommandStreamTest, FlushNotEmptyBB) {
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

    csr->addBatchBufferEnd(cs, nullptr);
    csr->alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr, *osContext);
}

TEST_F(DrmCommandStreamTest, FlushNotEmptyNotPaddedBB) {
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

    csr->addBatchBufferEnd(cs, nullptr);
    csr->alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr, *osContext);
}

TEST_F(DrmCommandStreamTest, FlushNotAligned) {
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

    csr->addBatchBufferEnd(cs, nullptr);
    csr->alignToCacheLine(cs);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 4, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr, *osContext);
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
        if (exec_objects[i].offset > 0xFFFFFFFF) {
            EXPECT_TRUE(exec_objects[i].flags == (EXEC_OBJECT_PINNED | EXEC_OBJECT_SUPPORTS_48B_ADDRESS));
        } else {
            EXPECT_TRUE(exec_objects[i].flags == (EXEC_OBJECT_PINNED));
        }
    }

    return true;
}

TEST_F(DrmCommandStreamTest, FlushCheckFlags) {
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

    DrmAllocation allocation(nullptr, (void *)0x7FFFFFFF, 1024, MemoryPool::MemoryNull);
    DrmAllocation allocation2(nullptr, (void *)0x307FFFFFFF, 1024, MemoryPool::MemoryNull);
    csr->makeResident(allocation);
    csr->makeResident(allocation2);
    csr->addBatchBufferEnd(cs, nullptr);
    csr->alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr, *osContext);
}

TEST_F(DrmCommandStreamTest, CheckDrmFree) {
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

    DrmAllocation allocation(nullptr, nullptr, 1024, MemoryPool::MemoryNull);

    csr->makeResident(allocation);
    csr->addBatchBufferEnd(cs, nullptr);
    csr->alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 4, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr, *osContext);
}

TEST_F(DrmCommandStreamTest, GIVENCSRWHENgetDMTHENNotNull) {
    Drm *pDrm = nullptr;
    if (csr->getOSInterface()) {
        pDrm = csr->getOSInterface()->get()->getDrm();
    }
    ASSERT_NE(nullptr, pDrm);
}

TEST_F(DrmCommandStreamTest, CheckDrmFreeCloseFailed) {
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
    DrmAllocation allocation(nullptr, nullptr, 1024, MemoryPool::MemoryNull);

    csr->makeResident(allocation);
    csr->addBatchBufferEnd(cs, nullptr);
    csr->alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 4, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr, *osContext);
}

struct DrmCsrVfeTests : ::testing::Test {
    template <typename FamilyType>
    struct MyCsr : public DrmCommandStreamReceiver<FamilyType> {
        using CommandStreamReceiver::mediaVfeStateDirty;
        using DrmCommandStreamReceiver<FamilyType>::mediaVfeStateLowPriorityDirty;
        using CommandStreamReceiver::commandStream;

        MyCsr(ExecutionEnvironment &executionEnvironment) : DrmCommandStreamReceiver<FamilyType>(*platformDevices[0], nullptr, executionEnvironment, gemCloseWorkerMode::gemCloseWorkerInactive) {}
        FlushStamp flush(BatchBuffer &batchBuffer, EngineType engineType, ResidencyContainer *allocationsForResidency, OsContext &osContext) override {
            return (FlushStamp)0;
        }
        bool peekDefaultMediaVfeStateDirty() {
            return mediaVfeStateDirty;
        }
        bool peekLowPriorityMediaVfeStateDirty() {
            return mediaVfeStateLowPriorityDirty;
        }
    };

    void flushTask(CommandStreamReceiver &csr, IndirectHeap &stream, bool lowPriority, Device &device) {
        dispatchFlags.lowPriority = lowPriority;
        csr.flushTask(stream, 0, stream, stream, stream, 0, dispatchFlags, device);
    }

    HardwareParse hwParser;
    DispatchFlags dispatchFlags = {};
    CommandStreamReceiver *oldCsr = nullptr;
};

HWCMDTEST_F(IGFX_GEN8_CORE, DrmCsrVfeTests, givenNonDirtyVfeForDefaultContextWhenLowPriorityIsFlushedThenReprogram) {
    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto mockCsr = new MyCsr<FamilyType>(*device->executionEnvironment);

    device->resetCommandStreamReceiver(mockCsr);

    auto graphicAlloc = mockCsr->getMemoryManager()->allocateGraphicsMemory(1024);
    IndirectHeap stream(graphicAlloc);

    EXPECT_TRUE(mockCsr->peekDefaultMediaVfeStateDirty());
    EXPECT_TRUE(mockCsr->peekLowPriorityMediaVfeStateDirty());
    flushTask(*mockCsr, stream, false, *device); //default priority
    EXPECT_FALSE(mockCsr->peekDefaultMediaVfeStateDirty());
    EXPECT_TRUE(mockCsr->peekLowPriorityMediaVfeStateDirty());

    auto startOffset = mockCsr->commandStream.getUsed();

    flushTask(*mockCsr, stream, true, *device); // low priority
    EXPECT_FALSE(mockCsr->peekDefaultMediaVfeStateDirty());
    EXPECT_FALSE(mockCsr->peekLowPriorityMediaVfeStateDirty());

    hwParser.parseCommands<FamilyType>(mockCsr->commandStream, startOffset);
    auto cmd = hwParser.getCommand<typename FamilyType::MEDIA_VFE_STATE>();
    EXPECT_NE(nullptr, cmd);

    mockCsr->getMemoryManager()->freeGraphicsMemory(graphicAlloc);
}

HWCMDTEST_F(IGFX_GEN8_CORE, DrmCsrVfeTests, givenNonDirtyVfeForLowPriorityContextWhenDefaultPriorityIsFlushedThenReprogram) {
    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto mockCsr = new MyCsr<FamilyType>(*device->executionEnvironment);

    device->resetCommandStreamReceiver(mockCsr);

    auto graphicAlloc = mockCsr->getMemoryManager()->allocateGraphicsMemory(1024);
    IndirectHeap stream(graphicAlloc);

    EXPECT_TRUE(mockCsr->peekDefaultMediaVfeStateDirty());
    EXPECT_TRUE(mockCsr->peekLowPriorityMediaVfeStateDirty());
    flushTask(*mockCsr, stream, true, *device); //low priority
    EXPECT_TRUE(mockCsr->peekDefaultMediaVfeStateDirty());
    EXPECT_FALSE(mockCsr->peekLowPriorityMediaVfeStateDirty());

    auto startOffset = mockCsr->commandStream.getUsed();

    flushTask(*mockCsr, stream, false, *device); // default priority
    EXPECT_FALSE(mockCsr->peekDefaultMediaVfeStateDirty());
    EXPECT_FALSE(mockCsr->peekLowPriorityMediaVfeStateDirty());

    hwParser.parseCommands<FamilyType>(mockCsr->commandStream, startOffset);
    auto cmd = hwParser.getCommand<typename FamilyType::MEDIA_VFE_STATE>();
    EXPECT_NE(nullptr, cmd);

    mockCsr->getMemoryManager()->freeGraphicsMemory(graphicAlloc);
}

HWCMDTEST_F(IGFX_GEN8_CORE, DrmCsrVfeTests, givenNonDirtyVfeForLowPriorityContextWhenLowPriorityIsFlushedThenDontReprogram) {
    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto mockCsr = new MyCsr<FamilyType>(*device->executionEnvironment);

    device->resetCommandStreamReceiver(mockCsr);

    auto graphicAlloc = mockCsr->getMemoryManager()->allocateGraphicsMemory(1024);
    IndirectHeap stream(graphicAlloc);

    EXPECT_TRUE(mockCsr->peekDefaultMediaVfeStateDirty());
    EXPECT_TRUE(mockCsr->peekLowPriorityMediaVfeStateDirty());
    flushTask(*mockCsr, stream, true, *device); //low priority
    EXPECT_TRUE(mockCsr->peekDefaultMediaVfeStateDirty());
    EXPECT_FALSE(mockCsr->peekLowPriorityMediaVfeStateDirty());

    auto startOffset = mockCsr->commandStream.getUsed();

    flushTask(*mockCsr, stream, true, *device); // low priority
    EXPECT_TRUE(mockCsr->peekDefaultMediaVfeStateDirty());
    EXPECT_FALSE(mockCsr->peekLowPriorityMediaVfeStateDirty());

    hwParser.parseCommands<FamilyType>(mockCsr->commandStream, startOffset);
    auto cmd = hwParser.getCommand<typename FamilyType::MEDIA_VFE_STATE>();
    EXPECT_EQ(nullptr, cmd);

    mockCsr->getMemoryManager()->freeGraphicsMemory(graphicAlloc);
}

HWTEST_F(DrmCsrVfeTests, givenNonDirtyVfeForBothPriorityContextWhenFlushedLowWithScratchRequirementThenMakeDefaultDirty) {
    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto mockCsr = new MyCsr<FamilyType>(*device->executionEnvironment);

    device->resetCommandStreamReceiver(mockCsr);

    auto graphicAlloc = mockCsr->getMemoryManager()->allocateGraphicsMemory(1024);
    IndirectHeap stream(graphicAlloc);

    mockCsr->overrideMediaVFEStateDirty(false);
    EXPECT_FALSE(mockCsr->peekDefaultMediaVfeStateDirty());
    EXPECT_FALSE(mockCsr->peekLowPriorityMediaVfeStateDirty());
    mockCsr->setRequiredScratchSize(123);

    flushTask(*mockCsr, stream, true, *device); //low priority
    EXPECT_TRUE(mockCsr->peekDefaultMediaVfeStateDirty());
    EXPECT_FALSE(mockCsr->peekLowPriorityMediaVfeStateDirty());

    mockCsr->getMemoryManager()->freeGraphicsMemory(graphicAlloc);
}

HWTEST_F(DrmCsrVfeTests, givenNonDirtyVfeForBothPriorityContextWhenFlushedDefaultWithScratchRequirementThenMakeLowDirty) {
    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto mockCsr = new MyCsr<FamilyType>(*device->executionEnvironment);

    device->resetCommandStreamReceiver(mockCsr);

    auto graphicAlloc = mockCsr->getMemoryManager()->allocateGraphicsMemory(1024);
    IndirectHeap stream(graphicAlloc);

    mockCsr->overrideMediaVFEStateDirty(false);
    EXPECT_FALSE(mockCsr->peekDefaultMediaVfeStateDirty());
    EXPECT_FALSE(mockCsr->peekLowPriorityMediaVfeStateDirty());
    mockCsr->setRequiredScratchSize(123);

    flushTask(*mockCsr, stream, false, *device); //default priority
    EXPECT_FALSE(mockCsr->peekDefaultMediaVfeStateDirty());
    EXPECT_TRUE(mockCsr->peekLowPriorityMediaVfeStateDirty());

    mockCsr->getMemoryManager()->freeGraphicsMemory(graphicAlloc);
}

/* **** **** **** **** **** **** **** **** **** **** **** **** **** **** **** ****
 * **** **** **** **** **** **** **** **** **** **** **** **** **** **** **** ****
 * **** **** **** **** **** **** **** **** **** **** **** **** **** **** **** **** */
class DrmCommandStreamEnhancedFixture

{
  public:
    DrmMockCustom *mock;
    DeviceCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME> *csr = nullptr;
    DrmMemoryManager *mm = nullptr;
    MockDevice *device = nullptr;
    OsContext *osContext;
    DebugManagerStateRestore *dbgState;
    ExecutionEnvironment *executionEnvironment;

    void SetUp() {
        executionEnvironment = new ExecutionEnvironment;
        executionEnvironment->initGmm(*platformDevices);
        this->dbgState = new DebugManagerStateRestore();
        //make sure this is disabled, we don't want test this now
        DebugManager.flags.EnableForcePin.set(false);

        mock = new DrmMockCustom();
        tCsr = new TestedDrmCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>(mock, *executionEnvironment);
        csr = tCsr;
        ASSERT_NE(nullptr, csr);
        mm = reinterpret_cast<DrmMemoryManager *>(csr->createMemoryManager(false));
        ASSERT_NE(nullptr, mm);
        executionEnvironment->memoryManager.reset(mm);
        device = Device::create<MockDevice>(platformDevices[0], executionEnvironment);
        ASSERT_NE(nullptr, device);
        osContext = device->getOsContext();
    }

    void TearDown() {
        //And close at destruction
        delete csr;
        delete device;
        delete mock;
        delete dbgState;
    }

    bool isResident(BufferObject *bo) {
        return tCsr->isResident(bo) && bo->peekIsResident();
    }

    const BufferObject *getResident(BufferObject *bo) {
        return tCsr->getResident(bo);
    }

  protected:
    template <typename GfxFamily>
    class TestedDrmCommandStreamReceiver : public DrmCommandStreamReceiver<GfxFamily> {
      public:
        using CommandStreamReceiver::commandStream;

        TestedDrmCommandStreamReceiver(Drm *drm, gemCloseWorkerMode mode, ExecutionEnvironment &executionEnvironment) : DrmCommandStreamReceiver<GfxFamily>(*platformDevices[0], drm, executionEnvironment, mode) {
        }
        TestedDrmCommandStreamReceiver(Drm *drm, ExecutionEnvironment &executionEnvironment) : DrmCommandStreamReceiver<GfxFamily>(*platformDevices[0], drm, executionEnvironment, gemCloseWorkerMode::gemCloseWorkerInactive) {
        }

        void overrideGemCloseWorkerOperationMode(gemCloseWorkerMode overrideValue) {
            this->gemCloseWorkerOperationMode = overrideValue;
        }

        void overrideDispatchPolicy(DispatchMode overrideValue) {
            this->dispatchMode = overrideValue;
        }

        bool isResident(BufferObject *bo) {
            bool resident = false;
            for (auto it : this->residency) {
                if (it == bo) {
                    resident = true;
                    break;
                }
            }
            return resident;
        }

        void makeNonResident(GraphicsAllocation &gfxAllocation) override {
            makeNonResidentResult.called = true;
            makeNonResidentResult.allocation = &gfxAllocation;
            DrmCommandStreamReceiver<GfxFamily>::makeNonResident(gfxAllocation);
        }

        const BufferObject *getResident(BufferObject *bo) {
            BufferObject *ret = nullptr;
            for (auto it : this->residency) {
                if (it == bo) {
                    ret = it;
                    break;
                }
            }
            return ret;
        }

        struct MakeResidentNonResidentResult {
            bool called;
            GraphicsAllocation *allocation;
        };

        MakeResidentNonResidentResult makeNonResidentResult;
        std::vector<BufferObject *> *getResidencyVector() { return &this->residency; }

        SubmissionAggregator *peekSubmissionAggregator() {
            return this->submissionAggregator.get();
        }
        void overrideSubmissionAggregator(SubmissionAggregator *newSubmissionsAggregator) {
            this->submissionAggregator.reset(newSubmissionsAggregator);
        }
        std::vector<drm_i915_gem_exec_object2> &getExecStorage() {
            return this->execObjectsStorage;
        }
    };
    TestedDrmCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME> *tCsr = nullptr;

    class MockBufferObject : public BufferObject {
        friend DrmCommandStreamEnhancedFixture;

      protected:
        MockBufferObject(Drm *drm, size_t size) : BufferObject(drm, 1, false) {
            this->isSoftpin = true;
            this->size = alignUp(size, 4096);
        }
    };

    MockBufferObject *createBO(size_t size) {
        return new MockBufferObject(this->mock, size);
    }
};
typedef Test<DrmCommandStreamEnhancedFixture> DrmCommandStreamGemWorkerTests;

TEST_F(DrmCommandStreamGemWorkerTests, givenDefaultDrmCSRWhenItIsCreatedThenGemCloseWorkerModeIsInactive) {
    EXPECT_EQ(gemCloseWorkerMode::gemCloseWorkerInactive, tCsr->peekGemCloseWorkerOperationMode());
}

TEST_F(DrmCommandStreamGemWorkerTests, givenCommandStreamWhenItIsFlushedWithGemCloseWorkerInDefaultModeThenWorkerDecreasesTheRefCount) {
    auto commandBuffer = mm->allocateGraphicsMemory(static_cast<size_t>(1024));
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    csr->addBatchBufferEnd(cs, nullptr);
    csr->alignToCacheLine(cs);
    auto storedBase = cs.getCpuBase();
    auto storedGraphicsAllocation = cs.getGraphicsAllocation();
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr, *osContext);
    EXPECT_EQ(cs.getCpuBase(), storedBase);
    EXPECT_EQ(cs.getGraphicsAllocation(), storedGraphicsAllocation);

    auto drmAllocation = (DrmAllocation *)storedGraphicsAllocation;
    auto bo = drmAllocation->getBO();

    //no allocations should be connected
    EXPECT_EQ(bo->getResidency()->size(), 0u);

    //spin until gem close worker finishes execution
    while (bo->getRefCount() > 1)
        ;

    mm->freeGraphicsMemory(commandBuffer);
}

TEST_F(DrmCommandStreamGemWorkerTests, givenTaskThatRequiresLargeResourceCountWhenItIsFlushedThenExecStorageIsResized) {
    std::vector<GraphicsAllocation *> graphicsAllocations;

    auto &execStorage = tCsr->getExecStorage();
    execStorage.resize(0);

    for (auto id = 0; id < 10; id++) {
        auto graphicsAllocation = mm->allocateGraphicsMemory(1);
        csr->makeResident(*graphicsAllocation);
        graphicsAllocations.push_back(graphicsAllocation);
    }
    auto commandBuffer = mm->allocateGraphicsMemory(1024);

    LinearStream cs(commandBuffer);

    csr->addBatchBufferEnd(cs, nullptr);
    csr->alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr, *osContext);

    EXPECT_EQ(11u, this->mock->execBuffer.buffer_count);
    mm->freeGraphicsMemory(commandBuffer);
    for (auto graphicsAllocation : graphicsAllocations) {
        DrmAllocation *drmAlloc = reinterpret_cast<DrmAllocation *>(graphicsAllocation);
        EXPECT_FALSE(drmAlloc->getBO()->peekIsResident());
        mm->freeGraphicsMemory(graphicsAllocation);
    }
    EXPECT_EQ(11u, execStorage.size());
}

TEST_F(DrmCommandStreamGemWorkerTests, givenGemCloseWorkerInactiveModeWhenMakeResidentIsCalledThenRefCountsAreNotUpdated) {
    auto dummyAllocation = reinterpret_cast<DrmAllocation *>(mm->allocateGraphicsMemory(1024));

    auto bo = dummyAllocation->getBO();
    EXPECT_EQ(1u, bo->getRefCount());

    csr->makeResident(*dummyAllocation);
    EXPECT_EQ(1u, bo->getRefCount());

    csr->processResidency(nullptr, *osContext);

    csr->makeNonResident(*dummyAllocation);
    EXPECT_EQ(1u, bo->getRefCount());

    mm->freeGraphicsMemory(dummyAllocation);
}

TEST_F(DrmCommandStreamGemWorkerTests, GivenTwoAllocationsWhenBackingStorageIsDifferentThenMakeResidentShouldAddTwoLocations) {
    auto allocation = reinterpret_cast<DrmAllocation *>(mm->allocateGraphicsMemory(1024));
    auto allocation2 = reinterpret_cast<DrmAllocation *>(mm->allocateGraphicsMemory(1024));

    auto bo1 = allocation->getBO();
    auto bo2 = allocation2->getBO();
    csr->makeResident(*allocation);
    csr->makeResident(*allocation2);

    EXPECT_FALSE(bo1->peekIsResident());
    EXPECT_FALSE(bo2->peekIsResident());

    csr->processResidency(nullptr, *osContext);

    EXPECT_TRUE(bo1->peekIsResident());
    EXPECT_TRUE(bo2->peekIsResident());
    EXPECT_EQ(tCsr->getResidencyVector()->size(), 2u);

    csr->makeNonResident(*allocation);
    csr->makeNonResident(*allocation2);
    EXPECT_FALSE(bo1->peekIsResident());
    EXPECT_FALSE(bo2->peekIsResident());

    EXPECT_EQ(tCsr->getResidencyVector()->size(), 0u);
    mm->freeGraphicsMemory(allocation);
    mm->freeGraphicsMemory(allocation2);
}

TEST_F(DrmCommandStreamGemWorkerTests, givenCommandStreamWithDuplicatesWhenItIsFlushedWithGemCloseWorkerInactiveModeThenCsIsNotNulled) {
    auto commandBuffer = reinterpret_cast<DrmAllocation *>(mm->allocateGraphicsMemory(1024));
    auto dummyAllocation = reinterpret_cast<DrmAllocation *>(mm->allocateGraphicsMemory(1024));
    ASSERT_NE(nullptr, commandBuffer);
    ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) & 0xFFF);
    LinearStream cs(commandBuffer);

    csr->makeResident(*dummyAllocation);
    csr->makeResident(*dummyAllocation);

    csr->addBatchBufferEnd(cs, nullptr);
    csr->alignToCacheLine(cs);
    auto storedBase = cs.getCpuBase();
    auto storedGraphicsAllocation = cs.getGraphicsAllocation();
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr, *osContext);
    EXPECT_EQ(cs.getCpuBase(), storedBase);
    EXPECT_EQ(cs.getGraphicsAllocation(), storedGraphicsAllocation);

    auto drmAllocation = (DrmAllocation *)storedGraphicsAllocation;
    auto bo = drmAllocation->getBO();

    //no allocations should be connected
    EXPECT_EQ(bo->getResidency()->size(), 0u);

    mm->freeGraphicsMemory(dummyAllocation);
    mm->freeGraphicsMemory(commandBuffer);
}

TEST_F(DrmCommandStreamGemWorkerTests, givenDrmCsrCreatedWithInactiveGemCloseWorkerPolicyThenThreadIsNotCreated) {
    TestedDrmCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME> testedCsr(mock, gemCloseWorkerMode::gemCloseWorkerInactive, *this->executionEnvironment);
    EXPECT_EQ(gemCloseWorkerMode::gemCloseWorkerInactive, testedCsr.peekGemCloseWorkerOperationMode());
}

class DrmCommandStreamBatchingTests : public Test<DrmCommandStreamEnhancedFixture> {
  public:
    DrmAllocation *tagAllocation;
    DrmAllocation *preemptionAllocation;
    GraphicsAllocation *tmpAllocation;
    void SetUp() override {
        DrmCommandStreamEnhancedFixture::SetUp();
        if (PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]) == PreemptionMode::MidThread) {
            tmpAllocation = GlobalMockSipProgram::sipProgram->getAllocation();
            GlobalMockSipProgram::sipProgram->resetAllocation(device->getMemoryManager()->allocateGraphicsMemory(1024));
        }
        tagAllocation = static_cast<DrmAllocation *>(device->getTagAllocation());
        preemptionAllocation = static_cast<DrmAllocation *>(device->getPreemptionAllocation());
    }
    void TearDown() override {
        if (PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]) == PreemptionMode::MidThread) {
            device->getMemoryManager()->freeGraphicsMemory((GlobalMockSipProgram::sipProgram)->getAllocation());
            GlobalMockSipProgram::sipProgram->resetAllocation(tmpAllocation);
        }
        DrmCommandStreamEnhancedFixture::TearDown();
    }
};

TEST_F(DrmCommandStreamBatchingTests, givenCSRWhenFlushIsCalledThenProperFlagsArePassed) {
    auto commandBuffer = mm->allocateGraphicsMemory(1024);
    auto dummyAllocation = mm->allocateGraphicsMemory(1024);
    ASSERT_NE(nullptr, commandBuffer);
    ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) & 0xFFF);
    LinearStream cs(commandBuffer);

    csr->makeResident(*dummyAllocation);
    csr->addBatchBufferEnd(cs, nullptr);
    csr->alignToCacheLine(cs);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr, *osContext);

    //preemption allocation + Sip Kernel
    int ioctlExtraCnt = (PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]) == PreemptionMode::MidThread) ? 2 : 0;

    EXPECT_EQ(6 + ioctlExtraCnt, this->mock->ioctl_cnt.total);
    uint64_t flags = I915_EXEC_RENDER | I915_EXEC_NO_RELOC;
    EXPECT_EQ(flags, this->mock->execBuffer.flags);

    mm->freeGraphicsMemory(dummyAllocation);
    mm->freeGraphicsMemory(commandBuffer);
}

TEST_F(DrmCommandStreamBatchingTests, givenCsrWhenDispatchPolicyIsSetToBatchingThenCommandBufferIsNotSubmitted) {
    tCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    tCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    auto commandBuffer = mm->allocateGraphicsMemory(1024);
    auto dummyAllocation = mm->allocateGraphicsMemory(1024);
    ASSERT_NE(nullptr, commandBuffer);
    ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) & 0xFFF);
    IndirectHeap cs(commandBuffer);

    tCsr->makeResident(*dummyAllocation);

    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    tCsr->setTagAllocation(tagAllocation);
    tCsr->setPreemptionCsrAllocation(preemptionAllocation);
    DispatchFlags dispatchFlags;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(device->getHardwareInfo());
    tCsr->flushTask(cs, 0u, cs, cs, cs, 0u, dispatchFlags, *device);

    //make sure command buffer is recorded
    auto &cmdBuffers = mockedSubmissionsAggregator->peekCommandBuffers();
    EXPECT_FALSE(cmdBuffers.peekIsEmpty());
    EXPECT_NE(nullptr, cmdBuffers.peekHead());

    //preemption allocation
    size_t csrSurfaceCount = (device->getPreemptionMode() == PreemptionMode::MidThread) ? 2 : 0;

    //preemption allocation + sipKernel
    int ioctlExtraCnt = (PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]) == PreemptionMode::MidThread) ? 2 : 0;

    auto recordedCmdBuffer = cmdBuffers.peekHead();
    EXPECT_EQ(3u + csrSurfaceCount, recordedCmdBuffer->surfaces.size());

    //try to find all allocations
    auto elementInVector = std::find(recordedCmdBuffer->surfaces.begin(), recordedCmdBuffer->surfaces.end(), dummyAllocation);
    EXPECT_NE(elementInVector, recordedCmdBuffer->surfaces.end());

    elementInVector = std::find(recordedCmdBuffer->surfaces.begin(), recordedCmdBuffer->surfaces.end(), commandBuffer);
    EXPECT_NE(elementInVector, recordedCmdBuffer->surfaces.end());

    elementInVector = std::find(recordedCmdBuffer->surfaces.begin(), recordedCmdBuffer->surfaces.end(), tagAllocation);
    EXPECT_NE(elementInVector, recordedCmdBuffer->surfaces.end());

    EXPECT_EQ(tCsr->commandStream.getGraphicsAllocation(), recordedCmdBuffer->batchBuffer.commandBufferAllocation);

    EXPECT_EQ(6 + ioctlExtraCnt, this->mock->ioctl_cnt.total);

    EXPECT_EQ(0u, this->mock->execBuffer.flags);

    mm->freeGraphicsMemory(dummyAllocation);
    mm->freeGraphicsMemory(commandBuffer);
    tCsr->setTagAllocation(nullptr);
    tCsr->setPreemptionCsrAllocation(nullptr);
}

TEST_F(DrmCommandStreamBatchingTests, givenRecordedCommandBufferWhenItIsSubmittedThenFlushTaskIsProperlyCalled) {
    tCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    tCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    auto commandBuffer = mm->allocateGraphicsMemory(1024);
    auto dummyAllocation = mm->allocateGraphicsMemory(1024);
    IndirectHeap cs(commandBuffer);
    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    tCsr->setTagAllocation(tagAllocation);
    tCsr->setPreemptionCsrAllocation(preemptionAllocation);
    auto &submittedCommandBuffer = tCsr->getCS(1024);
    //use some bytes
    submittedCommandBuffer.getSpace(4);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(device->getHardwareInfo());
    tCsr->flushTask(cs, 0u, cs, cs, cs, 0u, dispatchFlags, *device);

    auto &cmdBuffers = mockedSubmissionsAggregator->peekCommandBuffers();
    auto storedCommandBuffer = cmdBuffers.peekHead();

    ResidencyContainer copyOfResidency = storedCommandBuffer->surfaces;
    copyOfResidency.push_back(storedCommandBuffer->batchBuffer.commandBufferAllocation);

    tCsr->flushBatchedSubmissions();

    EXPECT_TRUE(cmdBuffers.peekIsEmpty());

    auto commandBufferGraphicsAllocation = submittedCommandBuffer.getGraphicsAllocation();
    EXPECT_FALSE(commandBufferGraphicsAllocation->isResident());

    //preemption allocation
    size_t csrSurfaceCount = (device->getPreemptionMode() == PreemptionMode::MidThread) ? 2 : 0;

    //preemption allocation +sip Kernel
    int ioctlExtraCnt = (PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]) == PreemptionMode::MidThread) ? 2 : 0;

    //validate that submited command buffer has what we want
    EXPECT_EQ(3u + csrSurfaceCount, this->mock->execBuffer.buffer_count);
    EXPECT_EQ(4u, this->mock->execBuffer.batch_start_offset);
    EXPECT_EQ(submittedCommandBuffer.getUsed(), this->mock->execBuffer.batch_len);

    drm_i915_gem_exec_object2 *exec_objects = (drm_i915_gem_exec_object2 *)this->mock->execBuffer.buffers_ptr;

    for (unsigned int i = 0; i < this->mock->execBuffer.buffer_count; i++) {
        int handle = exec_objects[i].handle;

        auto handleFound = false;
        for (auto &graphicsAllocation : copyOfResidency) {
            auto bo = ((DrmAllocation *)graphicsAllocation)->getBO();
            if (bo->peekHandle() == handle) {
                handleFound = true;
            }
        }
        EXPECT_TRUE(handleFound);
    }

    EXPECT_EQ(7 + ioctlExtraCnt, this->mock->ioctl_cnt.total);

    mm->freeGraphicsMemory(dummyAllocation);
    mm->freeGraphicsMemory(commandBuffer);
    tCsr->setTagAllocation(nullptr);
    tCsr->setPreemptionCsrAllocation(nullptr);
}

typedef Test<DrmCommandStreamEnhancedFixture> DrmCommandStreamLeaksTest;

TEST_F(DrmCommandStreamLeaksTest, makeResident) {
    auto buffer = this->createBO(1024);
    auto allocation = new DrmAllocation(buffer, nullptr, buffer->peekSize(), MemoryPool::MemoryNull);
    EXPECT_EQ(nullptr, allocation->getUnderlyingBuffer());

    csr->makeResident(*allocation);
    csr->processResidency(nullptr, *osContext);

    EXPECT_TRUE(isResident(buffer));
    auto bo = getResident(buffer);
    EXPECT_EQ(bo, buffer);
    EXPECT_EQ(1u, bo->getRefCount());

    csr->makeNonResident(*allocation);
    EXPECT_FALSE(isResident(buffer));
    EXPECT_EQ(1u, bo->getRefCount());
    bo = getResident(buffer);
    EXPECT_EQ(nullptr, bo);
    mm->freeGraphicsMemory(allocation);
}

TEST_F(DrmCommandStreamLeaksTest, makeResidentOnly) {
    BufferObject *buffer1 = this->createBO(4096);
    BufferObject *buffer2 = this->createBO(4096);
    auto allocation1 = new DrmAllocation(buffer1, nullptr, buffer1->peekSize(), MemoryPool::MemoryNull);
    auto allocation2 = new DrmAllocation(buffer2, nullptr, buffer2->peekSize(), MemoryPool::MemoryNull);
    EXPECT_EQ(nullptr, allocation1->getUnderlyingBuffer());
    EXPECT_EQ(nullptr, allocation2->getUnderlyingBuffer());

    csr->makeResident(*allocation1);
    csr->makeResident(*allocation2);
    csr->processResidency(nullptr, *osContext);

    EXPECT_TRUE(isResident(buffer1));
    EXPECT_TRUE(isResident(buffer2));
    auto bo1 = getResident(buffer1);
    auto bo2 = getResident(buffer2);
    EXPECT_EQ(bo1, buffer1);
    EXPECT_EQ(bo2, buffer2);
    EXPECT_EQ(1u, bo1->getRefCount());
    EXPECT_EQ(1u, bo2->getRefCount());

    // dont call makeNonResident on allocation2, any other makeNonResident call will clean this
    // we want to keep all makeResident calls before flush and makeNonResident everyting after flush
    csr->makeNonResident(*allocation1);

    mm->freeGraphicsMemory(allocation1);
    mm->freeGraphicsMemory(allocation2);
}

TEST_F(DrmCommandStreamLeaksTest, makeResidentTwice) {
    auto buffer = this->createBO(1024);
    auto allocation = new DrmAllocation(buffer, nullptr, buffer->peekSize(), MemoryPool::MemoryNull);

    csr->makeResident(*allocation);
    csr->processResidency(nullptr, *osContext);

    EXPECT_TRUE(isResident(buffer));
    auto bo1 = getResident(buffer);
    EXPECT_EQ(buffer, bo1);
    EXPECT_EQ(1u, bo1->getRefCount());

    csr->getMemoryManager()->clearResidencyAllocations();
    csr->makeResident(*allocation);
    csr->processResidency(nullptr, *osContext);

    EXPECT_TRUE(isResident(buffer));
    auto bo2 = getResident(buffer);
    EXPECT_EQ(buffer, bo2);
    EXPECT_EQ(bo1, bo2);
    EXPECT_EQ(1u, bo1->getRefCount());

    csr->makeNonResident(*allocation);
    EXPECT_FALSE(isResident(buffer));
    EXPECT_EQ(1u, bo1->getRefCount());
    bo1 = getResident(buffer);
    EXPECT_EQ(nullptr, bo1);
    mm->freeGraphicsMemory(allocation);
}

TEST_F(DrmCommandStreamLeaksTest, makeResidentTwiceWhenFragmentStorage) {
    auto ptr = (void *)0x1001;
    auto size = MemoryConstants::pageSize * 10;
    auto reqs = HostPtrManager::getAllocationRequirements(ptr, size);
    auto allocation = mm->allocateGraphicsMemory(size, ptr);
    auto &hostPtrManager = mm->hostPtrManager;

    EXPECT_EQ(3u, hostPtrManager.getFragmentCount());
    ASSERT_EQ(3u, allocation->fragmentsStorage.fragmentCount);

    csr->makeResident(*allocation);
    csr->makeResident(*allocation);

    csr->processResidency(nullptr, *osContext);
    for (int i = 0; i < max_fragments_count; i++) {
        ASSERT_EQ(allocation->fragmentsStorage.fragmentStorageData[i].cpuPtr,
                  reqs.AllocationFragments[i].allocationPtr);
        auto bo = allocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->bo;
        EXPECT_TRUE(isResident(bo));
        auto bo1 = getResident(bo);
        ASSERT_EQ(bo, bo1);
        EXPECT_EQ(1u, bo1->getRefCount());
    }

    csr->makeNonResident(*allocation);
    for (int i = 0; i < max_fragments_count; i++) {
        auto bo = allocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->bo;
        EXPECT_FALSE(isResident(bo));
        auto bo1 = getResident(bo);
        EXPECT_EQ(bo1, nullptr);
        EXPECT_EQ(1u, bo->getRefCount());
    }
    mm->freeGraphicsMemory(allocation);
}

TEST_F(DrmCommandStreamLeaksTest, givenFragmentedAllocationsWithResuedFragmentsWhenTheyAreMadeResidentThenFragmentsDoNotDuplicate) {
    mock->ioctl_expected.total = 9;
    //3 fragments
    auto ptr = (void *)0x1001;
    auto size = MemoryConstants::pageSize * 10;
    auto graphicsAllocation = mm->allocateGraphicsMemory(size, ptr);

    auto offsetedPtr = (void *)((uintptr_t)ptr + size);
    auto size2 = MemoryConstants::pageSize - 1;

    auto graphicsAllocation2 = mm->allocateGraphicsMemory(size2, offsetedPtr);

    auto &hostPtrManager = mm->hostPtrManager;
    ASSERT_EQ(3u, hostPtrManager.getFragmentCount());

    //graphicsAllocation2 reuses one fragment from graphicsAllocation
    EXPECT_EQ(graphicsAllocation->fragmentsStorage.fragmentStorageData[2].residency, graphicsAllocation2->fragmentsStorage.fragmentStorageData[0].residency);

    tCsr->makeResident(*graphicsAllocation);
    tCsr->makeResident(*graphicsAllocation2);

    tCsr->processResidency(nullptr, *osContext);

    EXPECT_TRUE(graphicsAllocation->fragmentsStorage.fragmentStorageData[0].residency->resident);
    EXPECT_TRUE(graphicsAllocation->fragmentsStorage.fragmentStorageData[1].residency->resident);
    EXPECT_TRUE(graphicsAllocation->fragmentsStorage.fragmentStorageData[2].residency->resident);
    EXPECT_TRUE(graphicsAllocation2->fragmentsStorage.fragmentStorageData[0].residency->resident);

    auto residency = tCsr->getResidencyVector();

    EXPECT_EQ(3u, residency->size());

    tCsr->makeSurfacePackNonResident(nullptr);

    //check that each packet is not resident
    EXPECT_FALSE(graphicsAllocation->fragmentsStorage.fragmentStorageData[0].residency->resident);
    EXPECT_FALSE(graphicsAllocation->fragmentsStorage.fragmentStorageData[1].residency->resident);
    EXPECT_FALSE(graphicsAllocation->fragmentsStorage.fragmentStorageData[2].residency->resident);
    EXPECT_FALSE(graphicsAllocation2->fragmentsStorage.fragmentStorageData[0].residency->resident);

    EXPECT_EQ(0u, residency->size());

    tCsr->makeResident(*graphicsAllocation);
    tCsr->makeResident(*graphicsAllocation2);

    tCsr->processResidency(nullptr, *osContext);

    EXPECT_TRUE(graphicsAllocation->fragmentsStorage.fragmentStorageData[0].residency->resident);
    EXPECT_TRUE(graphicsAllocation->fragmentsStorage.fragmentStorageData[1].residency->resident);
    EXPECT_TRUE(graphicsAllocation->fragmentsStorage.fragmentStorageData[2].residency->resident);
    EXPECT_TRUE(graphicsAllocation2->fragmentsStorage.fragmentStorageData[0].residency->resident);

    EXPECT_EQ(3u, residency->size());

    tCsr->makeSurfacePackNonResident(nullptr);

    EXPECT_EQ(0u, residency->size());

    EXPECT_FALSE(graphicsAllocation->fragmentsStorage.fragmentStorageData[0].residency->resident);
    EXPECT_FALSE(graphicsAllocation->fragmentsStorage.fragmentStorageData[1].residency->resident);
    EXPECT_FALSE(graphicsAllocation->fragmentsStorage.fragmentStorageData[2].residency->resident);
    EXPECT_FALSE(graphicsAllocation2->fragmentsStorage.fragmentStorageData[0].residency->resident);

    mm->freeGraphicsMemory(graphicsAllocation);
    mm->freeGraphicsMemory(graphicsAllocation2);
}

TEST_F(DrmCommandStreamLeaksTest, GivenAllocationCreatedFromThreeFragmentsWhenMakeResidentIsBeingCalledThenAllFragmentsAreMadeResident) {
    auto ptr = (void *)0x1001;
    auto size = MemoryConstants::pageSize * 10;

    auto reqs = HostPtrManager::getAllocationRequirements(ptr, size);

    auto allocation = mm->allocateGraphicsMemory(size, ptr);

    auto &hostPtrManager = mm->hostPtrManager;

    EXPECT_EQ(3u, hostPtrManager.getFragmentCount());
    ASSERT_EQ(3u, allocation->fragmentsStorage.fragmentCount);

    csr->makeResident(*allocation);
    csr->processResidency(nullptr, *osContext);

    for (int i = 0; i < max_fragments_count; i++) {
        ASSERT_EQ(allocation->fragmentsStorage.fragmentStorageData[i].cpuPtr,
                  reqs.AllocationFragments[i].allocationPtr);
        auto bo = allocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->bo;
        EXPECT_TRUE(isResident(bo));
        auto bo1 = getResident(bo);
        ASSERT_EQ(bo, bo1);
        EXPECT_EQ(1u, bo1->getRefCount());
    }
    csr->makeNonResident(*allocation);
    for (int i = 0; i < max_fragments_count; i++) {
        auto bo = allocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->bo;
        EXPECT_FALSE(isResident(bo));
        auto bo1 = getResident(bo);
        EXPECT_EQ(bo1, nullptr);
        EXPECT_EQ(1u, bo->getRefCount());
    }
    mm->freeGraphicsMemory(allocation);
}

TEST_F(DrmCommandStreamLeaksTest, GivenAllocationsContainingDifferentCountOfFragmentsWhenAllocationIsMadeResidentThenAllFragmentsAreMadeResident) {
    auto ptr = (void *)0x1001;
    auto size = MemoryConstants::pageSize;
    auto size2 = 100;

    auto reqs = HostPtrManager::getAllocationRequirements(ptr, size);

    auto allocation = mm->allocateGraphicsMemory(size, ptr);

    auto &hostPtrManager = mm->hostPtrManager;

    EXPECT_EQ(2u, hostPtrManager.getFragmentCount());
    ASSERT_EQ(2u, allocation->fragmentsStorage.fragmentCount);
    ASSERT_EQ(2u, reqs.requiredFragmentsCount);

    csr->makeResident(*allocation);
    csr->processResidency(nullptr, *osContext);

    for (unsigned int i = 0; i < reqs.requiredFragmentsCount; i++) {
        ASSERT_EQ(allocation->fragmentsStorage.fragmentStorageData[i].cpuPtr,
                  reqs.AllocationFragments[i].allocationPtr);
        auto bo = allocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->bo;
        EXPECT_TRUE(isResident(bo));
        auto bo1 = getResident(bo);
        ASSERT_EQ(bo, bo1);
        EXPECT_EQ(1u, bo1->getRefCount());
    }
    csr->makeNonResident(*allocation);
    for (unsigned int i = 0; i < reqs.requiredFragmentsCount; i++) {
        auto bo = allocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->bo;
        EXPECT_FALSE(isResident(bo));
        auto bo1 = getResident(bo);
        EXPECT_EQ(bo1, nullptr);
        EXPECT_EQ(1u, bo->getRefCount());
    }
    mm->freeGraphicsMemory(allocation);
    mm->clearResidencyAllocations();

    EXPECT_EQ(0u, hostPtrManager.getFragmentCount());

    auto allocation2 = mm->allocateGraphicsMemory(size2, ptr);
    reqs = HostPtrManager::getAllocationRequirements(ptr, size2);

    EXPECT_EQ(1u, hostPtrManager.getFragmentCount());
    ASSERT_EQ(1u, allocation2->fragmentsStorage.fragmentCount);
    ASSERT_EQ(1u, reqs.requiredFragmentsCount);

    csr->makeResident(*allocation2);
    csr->processResidency(nullptr, *osContext);

    for (unsigned int i = 0; i < reqs.requiredFragmentsCount; i++) {
        ASSERT_EQ(allocation2->fragmentsStorage.fragmentStorageData[i].cpuPtr,
                  reqs.AllocationFragments[i].allocationPtr);
        auto bo = allocation2->fragmentsStorage.fragmentStorageData[i].osHandleStorage->bo;
        EXPECT_TRUE(isResident(bo));
        auto bo1 = getResident(bo);
        ASSERT_EQ(bo, bo1);
        EXPECT_EQ(1u, bo1->getRefCount());
    }
    csr->makeNonResident(*allocation2);
    for (unsigned int i = 0; i < reqs.requiredFragmentsCount; i++) {
        auto bo = allocation2->fragmentsStorage.fragmentStorageData[i].osHandleStorage->bo;
        EXPECT_FALSE(isResident(bo));
        auto bo1 = getResident(bo);
        EXPECT_EQ(bo1, nullptr);
        EXPECT_EQ(1u, allocation2->fragmentsStorage.fragmentStorageData[i].osHandleStorage->bo->getRefCount());
    }
    mm->freeGraphicsMemory(allocation2);
    EXPECT_EQ(0u, hostPtrManager.getFragmentCount());
}

TEST_F(DrmCommandStreamLeaksTest, GivenTwoAllocationsWhenBackingStorageIsTheSameThenMakeResidentShouldAddOnlyOneLocation) {
    auto ptr = (void *)0x1000;
    auto size = MemoryConstants::pageSize;
    auto ptr2 = (void *)0x1000;

    auto allocation = mm->allocateGraphicsMemory(size, ptr);
    auto allocation2 = mm->allocateGraphicsMemory(size, ptr2);

    csr->makeResident(*allocation);
    csr->makeResident(*allocation2);

    csr->processResidency(nullptr, *osContext);

    EXPECT_EQ(tCsr->getResidencyVector()->size(), 1u);

    csr->makeNonResident(*allocation);
    csr->makeNonResident(*allocation2);

    mm->freeGraphicsMemory(allocation);
    mm->freeGraphicsMemory(allocation2);
    mm->clearResidencyAllocations();
}

TEST_F(DrmCommandStreamLeaksTest, GivenTwoAllocationsWhenBackingStorageIsDifferentThenMakeResidentShouldAddTwoLocations) {
    auto ptr = (void *)0x1000;
    auto size = MemoryConstants::pageSize;
    auto ptr2 = (void *)0x3000;

    auto allocation = mm->allocateGraphicsMemory(size, ptr);
    auto allocation2 = mm->allocateGraphicsMemory(size, ptr2);

    csr->makeResident(*allocation);
    csr->makeResident(*allocation2);

    csr->processResidency(nullptr, *osContext);

    EXPECT_EQ(tCsr->getResidencyVector()->size(), 2u);

    csr->makeNonResident(*allocation);
    csr->makeNonResident(*allocation2);

    mm->freeGraphicsMemory(allocation);
    mm->freeGraphicsMemory(allocation2);
    mm->clearResidencyAllocations();
}

TEST_F(DrmCommandStreamLeaksTest, makeResidentSizeZero) {
    std::unique_ptr<BufferObject> buffer(this->createBO(0));
    DrmAllocation allocation(buffer.get(), nullptr, buffer->peekSize(), MemoryPool::MemoryNull);
    EXPECT_EQ(nullptr, allocation.getUnderlyingBuffer());
    EXPECT_EQ(buffer->peekSize(), allocation.getUnderlyingBufferSize());

    csr->makeResident(allocation);
    csr->processResidency(nullptr, *osContext);

    EXPECT_FALSE(isResident(buffer.get()));
    auto bo = getResident(buffer.get());
    EXPECT_EQ(nullptr, bo);
}

TEST_F(DrmCommandStreamLeaksTest, Flush) {
    auto &cs = csr->getCS();
    auto commandBuffer = static_cast<DrmAllocation *>(cs.getGraphicsAllocation());
    ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) & 0xFFF);

    csr->addBatchBufferEnd(cs, nullptr);
    csr->alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr, *osContext);
    EXPECT_NE(cs.getCpuBase(), nullptr);
    EXPECT_NE(cs.getGraphicsAllocation(), nullptr);
}

TEST_F(DrmCommandStreamLeaksTest, ClearResidencyWhenFlushNotCalled) {
    auto allocation1 = reinterpret_cast<DrmAllocation *>(mm->allocateGraphicsMemory(1024));
    auto allocation2 = reinterpret_cast<DrmAllocation *>(mm->allocateGraphicsMemory(1024));
    ASSERT_NE(nullptr, allocation1);
    ASSERT_NE(nullptr, allocation2);

    EXPECT_EQ(tCsr->getResidencyVector()->size(), 0u);
    csr->makeResident(*allocation1);
    csr->makeResident(*allocation2);
    csr->processResidency(nullptr, *osContext);

    EXPECT_TRUE(isResident(allocation1->getBO()));
    EXPECT_TRUE(isResident(allocation2->getBO()));
    EXPECT_EQ(tCsr->getResidencyVector()->size(), 2u);

    EXPECT_EQ(allocation1->getBO()->getRefCount(), 1u);
    EXPECT_EQ(allocation2->getBO()->getRefCount(), 1u);

    // makeNonResident without flush
    csr->makeNonResident(*allocation1);
    EXPECT_EQ(tCsr->getResidencyVector()->size(), 0u);

    // everything is nonResident after first call
    EXPECT_FALSE(isResident(allocation1->getBO()));
    EXPECT_FALSE(isResident(allocation2->getBO()));
    EXPECT_EQ(allocation1->getBO()->getRefCount(), 1u);
    EXPECT_EQ(allocation2->getBO()->getRefCount(), 1u);

    mm->freeGraphicsMemory(allocation1);
    mm->freeGraphicsMemory(allocation2);
}

TEST_F(DrmCommandStreamLeaksTest, FlushMultipleTimes) {
    auto &cs = csr->getCS();
    auto commandBuffer = static_cast<DrmAllocation *>(cs.getGraphicsAllocation());

    csr->addBatchBufferEnd(cs, nullptr);
    csr->alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr, *osContext);

    cs.replaceBuffer(commandBuffer->getUnderlyingBuffer(), commandBuffer->getUnderlyingBufferSize());
    cs.replaceGraphicsAllocation(commandBuffer);
    csr->addBatchBufferEnd(cs, nullptr);
    csr->alignToCacheLine(cs);
    BatchBuffer batchBuffer2{cs.getGraphicsAllocation(), 8, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    csr->flush(batchBuffer2, EngineType::ENGINE_RCS, nullptr, *osContext);

    auto allocation = mm->allocateGraphicsMemory(1024);
    ASSERT_NE(nullptr, allocation);
    auto allocation2 = mm->allocateGraphicsMemory(1024);
    ASSERT_NE(nullptr, allocation2);

    csr->makeResident(*allocation);
    csr->makeResident(*allocation2);

    mm->storeAllocation(std::unique_ptr<GraphicsAllocation>(commandBuffer), REUSABLE_ALLOCATION);

    auto commandBuffer2 = mm->allocateGraphicsMemory(1024);
    ASSERT_NE(nullptr, commandBuffer2);
    cs.replaceBuffer(commandBuffer2->getUnderlyingBuffer(), commandBuffer2->getUnderlyingBufferSize());
    cs.replaceGraphicsAllocation(commandBuffer2);
    csr->addBatchBufferEnd(cs, nullptr);
    csr->alignToCacheLine(cs);
    BatchBuffer batchBuffer3{cs.getGraphicsAllocation(), 16, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    csr->flush(batchBuffer3, EngineType::ENGINE_RCS, nullptr, *osContext);
    csr->makeSurfacePackNonResident(nullptr);
    mm->freeGraphicsMemory(allocation);
    mm->freeGraphicsMemory(allocation2);

    mm->storeAllocation(std::unique_ptr<GraphicsAllocation>(commandBuffer2), REUSABLE_ALLOCATION);
    commandBuffer2 = mm->allocateGraphicsMemory(1024);
    ASSERT_NE(nullptr, commandBuffer2);
    cs.replaceBuffer(commandBuffer2->getUnderlyingBuffer(), commandBuffer2->getUnderlyingBufferSize());
    cs.replaceGraphicsAllocation(commandBuffer2);
    csr->addBatchBufferEnd(cs, nullptr);
    csr->alignToCacheLine(cs);
    BatchBuffer batchBuffer4{cs.getGraphicsAllocation(), 24, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    csr->flush(batchBuffer4, EngineType::ENGINE_RCS, nullptr, *osContext);
}

TEST_F(DrmCommandStreamLeaksTest, FlushNotEmptyBB) {
    int bbUsed = 16 * sizeof(uint32_t);

    auto &cs = csr->getCS();

    cs.getSpace(bbUsed);

    csr->addBatchBufferEnd(cs, nullptr);
    csr->alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr, *osContext);
}

TEST_F(DrmCommandStreamLeaksTest, FlushNotEmptyNotPaddedBB) {
    int bbUsed = 15 * sizeof(uint32_t);

    auto &cs = csr->getCS();

    cs.getSpace(bbUsed);

    csr->addBatchBufferEnd(cs, nullptr);
    csr->alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr, *osContext);
}

TEST_F(DrmCommandStreamLeaksTest, FlushNotAligned) {
    auto &cs = csr->getCS();
    auto commandBuffer = static_cast<DrmAllocation *>(cs.getGraphicsAllocation());

    //make sure command buffer with offset is not page aligned
    ASSERT_NE(0u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & 0xFFF);
    ASSERT_EQ(4u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & 0x7F);

    csr->addBatchBufferEnd(cs, nullptr);
    csr->alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 4, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr, *osContext);
}

TEST_F(DrmCommandStreamLeaksTest, CheckDrmFree) {
    auto &cs = csr->getCS();
    auto commandBuffer = static_cast<DrmAllocation *>(cs.getGraphicsAllocation());

    //make sure command buffer with offset is not page aligned
    ASSERT_NE(0u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & 0xFFF);
    ASSERT_EQ(4u, (reinterpret_cast<uintptr_t>(commandBuffer->getUnderlyingBuffer()) + 4) & 0x7F);

    auto allocation = mm->allocateGraphicsMemory(1024);

    csr->makeResident(*allocation);
    csr->addBatchBufferEnd(cs, nullptr);
    csr->alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 4, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    csr->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr, *osContext);
    csr->makeNonResident(*allocation);
    mm->freeGraphicsMemory(allocation);
}

TEST_F(DrmCommandStreamLeaksTest, MakeResidentClearResidencyAllocationsInMemoryManager) {
    auto allocation1 = mm->allocateGraphicsMemory(1024);
    auto allocation2 = mm->allocateGraphicsMemory(1024);

    ASSERT_NE(nullptr, allocation1);
    ASSERT_NE(nullptr, allocation2);

    csr->makeResident(*allocation1);
    csr->makeResident(*allocation2);
    EXPECT_NE(0u, mm->getResidencyAllocations().size());

    csr->processResidency(nullptr, *device->getOsContext());
    csr->makeSurfacePackNonResident(nullptr);
    EXPECT_EQ(0u, mm->getResidencyAllocations().size());

    mm->freeGraphicsMemory(allocation1);
    mm->freeGraphicsMemory(allocation2);
}

TEST_F(DrmCommandStreamLeaksTest, givenMultipleMakeResidentWhenMakeNonResidentIsCalledOnlyOnceThenSurfaceIsMadeNonResident) {
    auto allocation1 = mm->allocateGraphicsMemory(1024);

    ASSERT_NE(nullptr, allocation1);

    csr->makeResident(*allocation1);
    csr->makeResident(*allocation1);

    EXPECT_NE(0u, mm->getResidencyAllocations().size());

    csr->processResidency(nullptr, *device->getOsContext());
    csr->makeSurfacePackNonResident(nullptr);

    EXPECT_EQ(0u, mm->getResidencyAllocations().size());
    EXPECT_FALSE(allocation1->isResident());

    mm->freeGraphicsMemory(allocation1);
}

TEST_F(DrmCommandStreamLeaksTest, makeNonResidentOnMemObjectCallsDrmCSMakeNonResidentWithGraphicsAllocation) {
    auto allocation1 = mm->allocateGraphicsMemory(0x1000);
    ASSERT_NE(nullptr, allocation1);

    tCsr->makeResident(*allocation1);

    tCsr->makeNonResidentResult.called = false;
    tCsr->makeNonResidentResult.allocation = nullptr;

    tCsr->makeNonResident(*allocation1);

    EXPECT_TRUE(tCsr->makeNonResidentResult.called);
    EXPECT_EQ(allocation1, tCsr->makeNonResidentResult.allocation);
    EXPECT_EQ(0u, mm->getEvictionAllocations().size());

    mm->freeGraphicsMemory(allocation1);
}

class DrmMockBuffer : public Buffer {
  public:
    static DrmMockBuffer *create() {
        char *data = static_cast<char *>(::alignedMalloc(128, 64));
        DrmAllocation *alloc = new (std::nothrow) DrmAllocation(nullptr, &data, sizeof(data), MemoryPool::MemoryNull);
        return new DrmMockBuffer(data, 128, alloc);
    }

    ~DrmMockBuffer() override {
        ::alignedFree(data);
        delete gfxAllocation;
    }
    DrmMockBuffer(char *data, size_t size, DrmAllocation *alloc) : Buffer(nullptr, CL_MEM_USE_HOST_PTR, size, data, data, alloc, true, false, false),
                                                                   data(data),
                                                                   gfxAllocation(alloc) {
    }

    void setArgStateful(void *memory, bool forceNonAuxMode) override {
    }

  protected:
    char *data;
    DrmAllocation *gfxAllocation;
};

TEST_F(DrmCommandStreamLeaksTest, BufferResidency) {
    std::unique_ptr<Buffer> buffer(DrmMockBuffer::create());

    ASSERT_FALSE(buffer->getGraphicsAllocation()->isResident());
    ASSERT_EQ(ObjectNotResident, buffer->getGraphicsAllocation()->residencyTaskCount);
    ASSERT_GT(buffer->getSize(), 0u);

    //make it resident 8 times
    for (int c = 0; c < 8; c++) {
        csr->makeResident(*buffer->getGraphicsAllocation());
        csr->processResidency(nullptr, *osContext);
        EXPECT_TRUE(buffer->getGraphicsAllocation()->isResident());
        EXPECT_EQ(buffer->getGraphicsAllocation()->residencyTaskCount, (int)csr->peekTaskCount() + 1);
    }

    csr->makeNonResident(*buffer->getGraphicsAllocation());
    EXPECT_FALSE(buffer->getGraphicsAllocation()->isResident());

    csr->makeNonResident(*buffer->getGraphicsAllocation());
    EXPECT_FALSE(buffer->getGraphicsAllocation()->isResident());
    EXPECT_EQ(ObjectNotResident, buffer->getGraphicsAllocation()->residencyTaskCount);
}

typedef Test<DrmCommandStreamEnhancedFixture> DrmCommandStreamMemoryManagerTest;

TEST_F(DrmCommandStreamMemoryManagerTest, givenDrmCommandStreamReceiverWhenMemoryManagerIsCreatedThenItHasHostMemoryValidationEnabledByDefault) {
    EXPECT_TRUE(mm->isValidateHostMemoryEnabled());
}
