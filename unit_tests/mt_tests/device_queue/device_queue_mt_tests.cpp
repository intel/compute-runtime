/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_device_queue.h"

using namespace OCLRT;

typedef ::testing::Test DeviceQueueHwMtTest;

HWCMDTEST_F(IGFX_GEN8_CORE, DeviceQueueHwMtTest, givenTakenIgilCriticalSectionWhenSecondThreadIsWaitingThenDontHang) {
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto context = std::unique_ptr<MockContext>(new MockContext());

    cl_queue_properties properties[3] = {0};
    MockDeviceQueueHw<FamilyType> mockDevQueue(context.get(), device.get(), properties[0]);

    auto igilCmdQueue = mockDevQueue.getIgilQueue();
    auto igilCriticalSection = const_cast<volatile uint *>(&igilCmdQueue->m_controls.m_CriticalSection);
    *igilCriticalSection = DeviceQueue::ExecutionModelCriticalSection::Taken;
    EXPECT_FALSE(mockDevQueue.isEMCriticalSectionFree());

    std::mutex mtx;

    auto thread = std::thread([&] {
        std::unique_lock<std::mutex> inThreadLock(mtx);
        while (!mockDevQueue.isEMCriticalSectionFree()) {
            inThreadLock.unlock();
            inThreadLock.lock();
        }
    });

    std::unique_lock<std::mutex> lock(mtx);
    *igilCriticalSection = DeviceQueue::ExecutionModelCriticalSection::Free;
    lock.unlock();

    thread.join();
    EXPECT_TRUE(mockDevQueue.isEMCriticalSectionFree());
}
