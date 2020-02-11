/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/execution_environment/execution_environment.h"
#include "core/helpers/aligned_memory.h"
#include "core/os_interface/linux/drm_buffer_object.h"
#include "core/os_interface/linux/drm_gem_close_worker.h"
#include "core/os_interface/linux/drm_memory_manager.h"
#include "core/os_interface/linux/drm_memory_operations_handler.h"
#include "core/os_interface/linux/os_interface.h"
#include "runtime/command_stream/device_command_stream.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/os_interface/linux/drm_command_stream.h"
#include "test.h"
#include "unit_tests/mocks/mock_execution_environment.h"
#include "unit_tests/os_interface/linux/device_command_stream_fixture.h"

#include "drm/i915_drm.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <iostream>
#include <memory>
#include <mutex>
#include <thread>

using namespace NEO;

class DrmMockForWorker : public Drm {
  public:
    std::mutex mutex;
    std::atomic<int> gem_close_cnt;
    std::atomic<int> gem_close_expected;
    std::atomic<std::thread::id> ioctl_caller_thread_id;
    DrmMockForWorker() : Drm(std::make_unique<HwDeviceId>(33), *platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]) {
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
    DrmGemCloseWorkerFixture() : executionEnvironment(*platformDevices){};
    //max loop count for while
    static const uint32_t deadCntInit = 10 * 1000 * 1000;

    DrmMemoryManager *mm;
    DrmMockForWorker *drmMock;
    uint32_t deadCnt = deadCntInit;

    void SetUp() {
        this->drmMock = new DrmMockForWorker;

        this->drmMock->gem_close_cnt = 0;
        this->drmMock->gem_close_expected = 0;

        executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment.rootDeviceEnvironments[0]->osInterface->get()->setDrm(drmMock);
        executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<DrmMemoryOperationsHandler>();

        this->mm = new DrmMemoryManager(gemCloseWorkerMode::gemCloseWorkerInactive,
                                        false,
                                        false,
                                        executionEnvironment);
    }

    void TearDown() {
        if (this->drmMock->gem_close_expected >= 0) {
            EXPECT_EQ(this->drmMock->gem_close_expected, this->drmMock->gem_close_cnt);
        }

        delete this->mm;
    }

  protected:
    class DrmAllocationWrapper : public DrmAllocation {
      public:
        DrmAllocationWrapper(BufferObject *bo)
            : DrmAllocation(0, GraphicsAllocation::AllocationType::UNKNOWN, bo, nullptr, 0, (osHandle)0u, MemoryPool::MemoryNull) {
        }
    };
    MockExecutionEnvironment executionEnvironment;
};

typedef Test<DrmGemCloseWorkerFixture> DrmGemCloseWorkerTests;

TEST_F(DrmGemCloseWorkerTests, gemClose) {
    this->drmMock->gem_close_expected = 1;

    auto worker = new DrmGemCloseWorker(*mm);
    auto bo = new BufferObject(this->drmMock, 1, 0);

    worker->push(bo);

    delete worker;
}

TEST_F(DrmGemCloseWorkerTests, gemCloseExit) {
    this->drmMock->gem_close_expected = -1;

    auto worker = new DrmGemCloseWorker(*mm);
    auto bo = new BufferObject(this->drmMock, 1, 0);

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
    auto bo = new BufferObject(this->drmMock, 1, 0);

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
    auto bo = new BufferObject(this->drmMock, 1, 0);

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
