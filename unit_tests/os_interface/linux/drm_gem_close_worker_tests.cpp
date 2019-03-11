/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/device_command_stream.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/os_interface/linux/drm_buffer_object.h"
#include "runtime/os_interface/linux/drm_command_stream.h"
#include "runtime/os_interface/linux/drm_gem_close_worker.h"
#include "runtime/os_interface/linux/drm_memory_manager.h"
#include "test.h"
#include "unit_tests/os_interface/linux/device_command_stream_fixture.h"

#include "drm/i915_drm.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "hw_cmds.h"

#include <iostream>
#include <memory>
#include <mutex>
#include <thread>

using namespace OCLRT;

class DrmMockForWorker : public Drm {
  public:
    std::mutex mutex;
    std::atomic<int> gem_close_cnt;
    std::atomic<int> gem_close_expected;
    std::atomic<std::thread::id> ioctl_caller_thread_id;
    DrmMockForWorker() : Drm(33) {
    }
    int ioctl(unsigned long request, void *arg) override {
        if (_IOC_TYPE(request) == DRM_IOCTL_BASE) {
            //when drm ioctl is called, try acquire mutex
            //main thread can hold mutex, to prevent ioctl handling
            std::lock_guard<std::mutex> lock(mutex);
        }
        if (request == DRM_IOCTL_GEM_CLOSE)
            gem_close_cnt++;

        ioctl_caller_thread_id = std::this_thread::get_id();

        return 0;
    };
};

class DrmGemCloseWorkerFixture {
  public:
    //max loop count for while
    static const uint32_t deadCntInit = 10 * 1000 * 1000;

    DrmMemoryManager *mm;
    DrmMockForWorker *drmMock;
    uint32_t deadCnt = deadCntInit;

    void SetUp() {
        this->drmMock = new DrmMockForWorker;

        this->drmMock->gem_close_cnt = 0;
        this->drmMock->gem_close_expected = 0;

        this->mm = new DrmMemoryManager(this->drmMock,
                                        gemCloseWorkerMode::gemCloseWorkerInactive, false,
                                        false,
                                        false,
                                        executionEnvironment);
    }

    void TearDown() {
        if (this->drmMock->gem_close_expected >= 0) {
            EXPECT_EQ(this->drmMock->gem_close_expected, this->drmMock->gem_close_cnt);
        }

        delete this->mm;
        delete this->drmMock;
        this->drmMock = nullptr;
    }

  protected:
    class BufferObjectWrapper : public BufferObject {
      public:
        BufferObjectWrapper(Drm *drm, int handle)
            : BufferObject(drm, handle, false) {
        }
    };
    class DrmAllocationWrapper : public DrmAllocation {
      public:
        DrmAllocationWrapper(BufferObject *bo)
            : DrmAllocation(GraphicsAllocation::AllocationType::UNDECIDED, bo, nullptr, 0, MemoryPool::MemoryNull, 1u, false) {
        }
    };
    ExecutionEnvironment executionEnvironment;
};

typedef Test<DrmGemCloseWorkerFixture> DrmGemCloseWorkerTests;

TEST_F(DrmGemCloseWorkerTests, gemClose) {
    this->drmMock->gem_close_expected = 1;

    auto worker = new DrmGemCloseWorker(*mm);
    auto bo = new BufferObjectWrapper(this->drmMock, 1);

    worker->push(bo);

    delete worker;
}

TEST_F(DrmGemCloseWorkerTests, gemCloseExit) {
    this->drmMock->gem_close_expected = -1;

    auto worker = new DrmGemCloseWorker(*mm);
    auto bo = new BufferObjectWrapper(this->drmMock, 1);

    worker->push(bo);

    //wait for worker to complete or deadCnt drops
    while (!worker->isEmpty() && (deadCnt-- > 0))
        pthread_yield(); //yield to another threads

    worker->close(false);

    //and check if GEM was closed
    EXPECT_EQ(1, this->drmMock->gem_close_cnt.load());

    delete worker;
}

TEST_F(DrmGemCloseWorkerTests, close) {
    this->drmMock->gem_close_expected = -1;

    auto worker = new DrmGemCloseWorker(*mm);
    auto bo = new BufferObjectWrapper(this->drmMock, 1);

    worker->push(bo);
    worker->close(false);

    //wait for worker to complete or deadCnt drops
    while (!worker->isEmpty() && (deadCnt-- > 0))
        pthread_yield(); //yield to another threads

    //and check if GEM was closed
    EXPECT_EQ(1, this->drmMock->gem_close_cnt.load());

    delete worker;
}
TEST_F(DrmGemCloseWorkerTests, givenAllocationWhenAskedForUnreferenceWithForceFlagSetThenAllocationIsReleasedFromCallingThread) {
    this->drmMock->gem_close_expected = 1;

    auto worker = new DrmGemCloseWorker(*mm);
    auto bo = new BufferObjectWrapper(this->drmMock, 1);

    bo->reference();
    worker->push(bo);

    auto r = mm->unreference(bo, true);
    EXPECT_EQ(1u, r);
    EXPECT_EQ(drmMock->ioctl_caller_thread_id, std::this_thread::get_id());

    delete worker;
}

TEST_F(DrmGemCloseWorkerTests, givenDrmGemCloseWorkerWhenCloseIsCalledWithBlockingFlagThenThreadIsClosed) {
    struct mockDrmGemCloseWorker : DrmGemCloseWorker {
        using DrmGemCloseWorker::DrmGemCloseWorker;
        using DrmGemCloseWorker::thread;
    };

    std::unique_ptr<mockDrmGemCloseWorker> worker(new mockDrmGemCloseWorker(*mm));
    EXPECT_NE(nullptr, worker->thread);
    worker->close(true);
    EXPECT_EQ(nullptr, worker->thread);
}

TEST_F(DrmGemCloseWorkerTests, givenDrmGemCloseWorkerWhenCloseIsCalledMultipleTimeWithBlockingFlagThenThreadIsClosed) {
    struct mockDrmGemCloseWorker : DrmGemCloseWorker {
        using DrmGemCloseWorker::DrmGemCloseWorker;
        using DrmGemCloseWorker::thread;
    };

    std::unique_ptr<mockDrmGemCloseWorker> worker(new mockDrmGemCloseWorker(*mm));
    worker->close(true);
    worker->close(true);
    worker->close(true);
    EXPECT_EQ(nullptr, worker->thread);
}
