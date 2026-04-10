/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include <atomic>
#include <memory>
#include <thread>

using namespace NEO;

struct EnqueueSvmTestMt : public ClDeviceFixture,
                          public CommandQueueHwFixture,
                          public ::testing::Test {
    typedef CommandQueueHwFixture CommandQueueFixture;

    void SetUp() override {
        ClDeviceFixture::setUp();
        CommandQueueFixture::setUp(pClDevice, 0);
        ptrSVM = context->getSVMAllocsManager()->createSVMAlloc(256, {}, context->getRootDeviceIndices(), context->getDeviceBitfields());
    }

    void TearDown() override {
        context->getSVMAllocsManager()->freeSVMAlloc(ptrSVM);
        CommandQueueFixture::tearDown();
        ClDeviceFixture::tearDown();
    }

    void *ptrSVM = nullptr;
};

TEST_F(EnqueueSvmTestMt, GivenMultipleThreadsWhenAllocatingSvmThenOnlyOneAllocationIsCreated) {
    std::atomic<int> flag(0);
    std::atomic<int> ready(0);
    void *svmPtrs[15] = {};

    auto allocSvm = [&](uint32_t from, uint32_t to) {
        for (uint32_t i = from; i <= to; i++) {
            svmPtrs[i] = context->getSVMAllocsManager()->createSVMAlloc(1, {}, context->getRootDeviceIndices(), context->getDeviceBitfields());
            auto svmData = context->getSVMAllocsManager()->getSVMAlloc(svmPtrs[i]);
            ASSERT_NE(nullptr, svmData);
            auto ga = svmData->gpuAllocations.getGraphicsAllocation(context->getDevice(0)->getRootDeviceIndex());
            EXPECT_NE(nullptr, ga);
            EXPECT_EQ(ga->getUnderlyingBuffer(), svmPtrs[i]);
        }
    };

    auto freeSvm = [&](uint32_t from, uint32_t to) {
        for (uint32_t i = from; i <= to; i++) {
            context->getSVMAllocsManager()->freeSVMAlloc(svmPtrs[i]);
        }
    };

    auto asyncFcn = [&](bool alloc, uint32_t from, uint32_t to) {
        flag++;
        while (flag < 3) {
            ;
        }
        if (alloc) {
            allocSvm(from, to);
        }
        freeSvm(from, to);
        ready++;
    };

    EXPECT_EQ(1u, context->getSVMAllocsManager()->getNumAllocs());

    allocSvm(10, 14);

    auto t1 = std::unique_ptr<std::thread>(new std::thread(asyncFcn, true, 0, 4));
    auto t2 = std::unique_ptr<std::thread>(new std::thread(asyncFcn, true, 5, 9));
    auto t3 = std::unique_ptr<std::thread>(new std::thread(asyncFcn, false, 10, 14));

    while (ready < 3) {
        std::this_thread::yield();
    }

    EXPECT_EQ(1u, context->getSVMAllocsManager()->getNumAllocs());
    t1->join();
    t2->join();
    t3->join();
}
