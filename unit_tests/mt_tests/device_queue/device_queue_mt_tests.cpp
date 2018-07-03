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

#include "unit_tests/mocks/mock_device_queue.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_context.h"
#include "test.h"

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
