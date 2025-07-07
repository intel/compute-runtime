/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/device_command_stream.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_command_stream.h"
#include "shared/source/os_interface/linux/drm_gem_close_worker.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/linux/device_command_stream_fixture.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

#include <iostream>
#include <memory>
#include <mutex>
#include <sched.h>
#include <thread>

using namespace NEO;

class DrmMockForWorker : public Drm {
  public:
    using Drm::setupIoctlHelper;
    std::mutex mutex;
    std::atomic<int> gemCloseCnt;
    std::atomic<int> gemCloseExpected;
    std::atomic<std::thread::id> ioctlCallerThreadId;
    DrmMockForWorker(RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<HwDeviceIdDrm>(mockFd, mockPciPath), rootDeviceEnvironment) {
    }
    int ioctl(DrmIoctl request, void *arg) override {
        if (request == DrmIoctl::gemClose)
            gemCloseCnt++;

        ioctlCallerThreadId = std::this_thread::get_id();

        return 0;
    };
};

class DrmGemCloseWorkerFixture {
  public:
    DrmGemCloseWorkerFixture() : executionEnvironment(defaultHwInfo.get()){};
    // max loop count for while
    static const uint32_t deadCntInit = 10 * 1000 * 1000;

    DrmMemoryManager *mm;
    DrmMockForWorker *drmMock;
    uint32_t deadCnt = deadCntInit;
    const uint32_t rootDeviceIndex = 0u;

    void setUp() {
        this->drmMock = new DrmMockForWorker(*executionEnvironment.rootDeviceEnvironments[0]);

        auto hwInfo = executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo();
        DebugManagerStateRestore restore;
        debugManager.flags.IgnoreProductSpecificIoctlHelper.set(true);
        drmMock->setupIoctlHelper(hwInfo->platform.eProductFamily);

        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drmMock));
        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drmMock, 0u, false);

        this->mm = new DrmMemoryManager(GemCloseWorkerMode::gemCloseWorkerInactive,
                                        false,
                                        false,
                                        executionEnvironment);

        this->drmMock->gemCloseCnt = 0;
        this->drmMock->gemCloseExpected = 0;
    }

    void tearDown() {
        if (this->drmMock->gemCloseExpected >= 0) {
            EXPECT_EQ(this->drmMock->gemCloseExpected, this->drmMock->gemCloseCnt);
        }

        delete this->mm;
    }

  protected:
    class DrmAllocationWrapper : public DrmAllocation {
      public:
        DrmAllocationWrapper(BufferObject *bo)
            : DrmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, bo, nullptr, 0, static_cast<osHandle>(0u), MemoryPool::memoryNull) {
        }
    };
    MockExecutionEnvironment executionEnvironment;
};

typedef Test<DrmGemCloseWorkerFixture> DrmGemCloseWorkerTests;

TEST_F(DrmGemCloseWorkerTests, WhenClosingGemThenSucceeds) {
    this->drmMock->gemCloseExpected = 1;

    auto worker = new DrmGemCloseWorker(*mm);
    auto bo = new BufferObject(rootDeviceIndex, this->drmMock, 3, 1, 0, 1);

    worker->push(bo);

    delete worker;
}

TEST_F(DrmGemCloseWorkerTests, GivenMultipleThreadsWhenClosingGemThenSucceeds) {
    this->drmMock->gemCloseExpected = -1;

    auto worker = new DrmGemCloseWorker(*mm);
    auto bo = new BufferObject(rootDeviceIndex, this->drmMock, 3, 1, 0, 1);

    worker->push(bo);

    // wait for worker to complete or deadCnt drops
    while (!worker->isEmpty() && (deadCnt-- > 0))
        sched_yield(); // yield to another threads

    worker->close(false);

    // and check if GEM was closed
    EXPECT_EQ(1, this->drmMock->gemCloseCnt.load());

    delete worker;
}

TEST_F(DrmGemCloseWorkerTests, GivenMultipleThreadsAndCloseFalseWhenClosingGemThenSucceeds) {
    this->drmMock->gemCloseExpected = -1;

    auto worker = new DrmGemCloseWorker(*mm);
    auto bo = new BufferObject(rootDeviceIndex, this->drmMock, 3, 1, 0, 1);

    worker->push(bo);
    worker->close(false);

    // wait for worker to complete or deadCnt drops
    while (!worker->isEmpty() && (deadCnt-- > 0))
        sched_yield(); // yield to another threads

    // and check if GEM was closed
    EXPECT_EQ(1, this->drmMock->gemCloseCnt.load());

    delete worker;
}
TEST_F(DrmGemCloseWorkerTests, givenAllocationWhenAskedForUnreferenceWithForceFlagSetThenAllocationIsReleasedFromCallingThread) {
    this->drmMock->gemCloseExpected = 1;

    auto worker = new DrmGemCloseWorker(*mm);
    auto bo = new BufferObject(rootDeviceIndex, this->drmMock, 3, 1, 0, 1);

    bo->reference();
    worker->push(bo);

    auto r = mm->unreference(bo, true);
    EXPECT_EQ(1u, r);
    EXPECT_EQ(drmMock->ioctlCallerThreadId, std::this_thread::get_id());

    delete worker;
}

TEST_F(DrmGemCloseWorkerTests, givenDrmGemCloseWorkerWhenCloseIsCalledWithBlockingFlagThenThreadIsClosed) {
    struct MockDrmGemCloseWorker : DrmGemCloseWorker {
        using DrmGemCloseWorker::DrmGemCloseWorker;
        using DrmGemCloseWorker::thread;
    };

    std::unique_ptr<MockDrmGemCloseWorker> worker(new MockDrmGemCloseWorker(*mm));
    EXPECT_NE(nullptr, worker->thread);
    worker->close(true);
    EXPECT_EQ(nullptr, worker->thread);
}

TEST_F(DrmGemCloseWorkerTests, givenDrmGemCloseWorkerWhenCloseIsCalledMultipleTimeWithBlockingFlagThenThreadIsClosed) {
    struct MockDrmGemCloseWorker : DrmGemCloseWorker {
        using DrmGemCloseWorker::DrmGemCloseWorker;
        using DrmGemCloseWorker::thread;
    };

    std::unique_ptr<MockDrmGemCloseWorker> worker(new MockDrmGemCloseWorker(*mm));
    worker->close(true);
    worker->close(true);
    worker->close(true);
    EXPECT_EQ(nullptr, worker->thread);
}
