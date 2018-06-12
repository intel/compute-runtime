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

#include "unit_tests/fixtures/device_host_queue_fixture.h"

using namespace OCLRT;

namespace DeviceHostQueue {
cl_queue_properties deviceQueueProperties::minimumProperties[5] = {
    CL_QUEUE_PROPERTIES,
    CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
    0, 0, 0};

cl_queue_properties deviceQueueProperties::minimumPropertiesWithProfiling[5] = {
    CL_QUEUE_PROPERTIES,
    CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_PROFILING_ENABLE,
    0, 0, 0};

cl_queue_properties deviceQueueProperties::noProperties[5] = {0};

cl_queue_properties deviceQueueProperties::allProperties[5] = {
    CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_ON_DEVICE,
    CL_QUEUE_SIZE, 128 * 1024,
    0};

template <>
cl_command_queue DeviceHostQueueFixture<DeviceQueue>::create(cl_context ctx, cl_device_id device, cl_int &retVal,
                                                             cl_queue_properties properties[5]) {
    cl_queue_properties qProps[5];
    memcpy(qProps, properties, 5 * sizeof(cl_queue_properties));
    qProps[0] = CL_QUEUE_PROPERTIES;
    qProps[1] = qProps[1] | deviceQueueProperties::minimumProperties[1];
    return clCreateCommandQueueWithProperties(ctx, device, qProps, &retVal);
}

template <>
cl_command_queue DeviceHostQueueFixture<CommandQueue>::create(cl_context ctx, cl_device_id device, cl_int &retVal,
                                                              cl_queue_properties properties[5]) {
    return clCreateCommandQueueWithProperties(ctx, device, properties, &retVal);
}

IGIL_CommandQueue getExpectedInitIgilCmdQueue(DeviceQueue *deviceQueue) {
    IGIL_CommandQueue igilCmdQueueInit;
    auto queueBuffer = deviceQueue->getQueueBuffer();

    memset(&igilCmdQueueInit, 0, sizeof(IGIL_CommandQueue));
    igilCmdQueueInit.m_head = IGIL_DEVICE_QUEUE_HEAD_INIT;
    igilCmdQueueInit.m_size = static_cast<uint32_t>(queueBuffer->getUnderlyingBufferSize() - sizeof(IGIL_CommandQueue));
    igilCmdQueueInit.m_magic = IGIL_MAGIC_NUMBER;

    igilCmdQueueInit.m_controls.m_SLBENDoffsetInBytes = -1;
    return igilCmdQueueInit;
}

IGIL_CommandQueue getExpectedgilCmdQueueAfterReset(DeviceQueue *deviceQueue) {
    auto queueBuffer = deviceQueue->getQueueBuffer();
    auto stackBuffer = deviceQueue->getStackBuffer();
    auto queueStorage = deviceQueue->getQueueStorageBuffer();

    auto deviceQueueIgilCmdQueue = reinterpret_cast<IGIL_CommandQueue *>(queueBuffer->getUnderlyingBuffer());
    IGIL_CommandQueue expectedIgilCmdQueue;
    memcpy(&expectedIgilCmdQueue, deviceQueueIgilCmdQueue, sizeof(IGIL_CommandQueue));

    expectedIgilCmdQueue.m_head = IGIL_DEVICE_QUEUE_HEAD_INIT;
    expectedIgilCmdQueue.m_size = static_cast<uint32_t>(queueBuffer->getUnderlyingBufferSize() - sizeof(IGIL_CommandQueue));
    expectedIgilCmdQueue.m_magic = IGIL_MAGIC_NUMBER;

    expectedIgilCmdQueue.m_controls.m_SLBENDoffsetInBytes = -1;
    expectedIgilCmdQueue.m_controls.m_StackSize =
        static_cast<uint32_t>((stackBuffer->getUnderlyingBufferSize() / sizeof(cl_uint)) - 1);
    expectedIgilCmdQueue.m_controls.m_StackTop =
        static_cast<uint32_t>((stackBuffer->getUnderlyingBufferSize() / sizeof(cl_uint)) - 1);
    expectedIgilCmdQueue.m_controls.m_PreviousHead = IGIL_DEVICE_QUEUE_HEAD_INIT;
    expectedIgilCmdQueue.m_controls.m_IDTAfterFirstPhase = 1;
    expectedIgilCmdQueue.m_controls.m_CurrentIDToffset = 1;
    expectedIgilCmdQueue.m_controls.m_PreviousStorageTop = static_cast<uint32_t>(queueStorage->getUnderlyingBufferSize());
    expectedIgilCmdQueue.m_controls.m_PreviousStackTop =
        static_cast<uint32_t>((stackBuffer->getUnderlyingBufferSize() / sizeof(cl_uint)) - 1);
    expectedIgilCmdQueue.m_controls.m_DebugNextBlockID = 0xFFFFFFFF;
    expectedIgilCmdQueue.m_controls.m_QstorageSize = static_cast<uint32_t>(queueStorage->getUnderlyingBufferSize());
    expectedIgilCmdQueue.m_controls.m_QstorageTop = static_cast<uint32_t>(queueStorage->getUnderlyingBufferSize());
    expectedIgilCmdQueue.m_controls.m_IsProfilingEnabled = static_cast<uint32_t>(deviceQueue->isProfilingEnabled());
    expectedIgilCmdQueue.m_controls.m_SLBENDoffsetInBytes = -1;
    expectedIgilCmdQueue.m_controls.m_IsSimulation = static_cast<uint32_t>(deviceQueue->getDevice().isSimulation());

    expectedIgilCmdQueue.m_controls.m_LastScheduleEventNumber = 0;
    expectedIgilCmdQueue.m_controls.m_PreviousNumberOfQueues = 0;
    expectedIgilCmdQueue.m_controls.m_EnqueueMarkerScheduled = 0;
    expectedIgilCmdQueue.m_controls.m_SecondLevelBatchOffset = 0;
    expectedIgilCmdQueue.m_controls.m_TotalNumberOfQueues = 0;
    expectedIgilCmdQueue.m_controls.m_EventTimestampAddress = 0;
    expectedIgilCmdQueue.m_controls.m_ErrorCode = 0;
    expectedIgilCmdQueue.m_controls.m_CurrentScheduleEventNumber = 0;
    expectedIgilCmdQueue.m_controls.m_DummyAtomicOperationPlaceholder = 0x00;
    expectedIgilCmdQueue.m_controls.m_DebugNextBlockGWS = 0;

    return expectedIgilCmdQueue;
}
} // namespace DeviceHostQueue
