/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/device/device_info.h"
#include "runtime/helpers/dispatch_info.h"
#include "unit_tests/fixtures/device_host_queue_fixture.h"
#include "unit_tests/gen_common/matchers.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"

using namespace NEO;
using namespace DeviceHostQueue;

using DeviceQueueSimpleTest = ::testing::Test;

HWCMDTEST_F(IGFX_GEN8_CORE, DeviceQueueSimpleTest, setupExecutionModelDispatchDoesNothing) {
    DeviceQueue devQueue;
    char buffer[20];

    memset(buffer, 1, 20);

    size_t size = 20;
    IndirectHeap ssh(buffer, size);
    IndirectHeap dsh(buffer, size);
    devQueue.setupExecutionModelDispatch(ssh, dsh, nullptr, 0, 0, 0);

    EXPECT_EQ(0u, ssh.getUsed());

    for (uint32_t i = 0; i < 20; i++) {
        EXPECT_EQ(1, buffer[i]);
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, DeviceQueueSimpleTest, nonUsedBaseMethods) {
    DeviceQueue devQueue;
    devQueue.resetDeviceQueue();
    EXPECT_EQ(nullptr, devQueue.getIndirectHeap(IndirectHeap::DYNAMIC_STATE));
}

class DeviceQueueTest : public DeviceHostQueueFixture<DeviceQueue> {
  public:
    using BaseClass = DeviceHostQueueFixture<DeviceQueue>;
    void SetUp() override {
        BaseClass::SetUp();
        device = castToObject<Device>(devices[0]);
        ASSERT_NE(device, nullptr);
    }

    void TearDown() override {
        BaseClass::TearDown();
    }

    void checkQueueBuffer(cl_uint expedtedSize) {
        auto alignedExpectedSize = alignUp(expedtedSize, MemoryConstants::pageSize);
        EXPECT_EQ(deviceQueue->getQueueSize(), expedtedSize);
        ASSERT_NE(deviceQueue->getQueueBuffer(), nullptr);
        EXPECT_EQ(deviceQueue->getQueueBuffer()->getUnderlyingBufferSize(), alignedExpectedSize);
    }

    DeviceQueue *deviceQueue;
    Device *device;
};

HWCMDTEST_F(IGFX_GEN8_CORE, DeviceQueueTest, createDeviceQueueWhenNoDeviceQueueIsSupported) {
    auto maxOnDeviceQueues = device->getDeviceInfo().maxOnDeviceQueues;
    const_cast<DeviceInfo *>(&device->getDeviceInfo())->maxOnDeviceQueues = 0;

    auto deviceQueue = createQueueObject();
    EXPECT_EQ(deviceQueue, nullptr);

    const_cast<DeviceInfo *>(&device->getDeviceInfo())->maxOnDeviceQueues = maxOnDeviceQueues;
}

HWCMDTEST_F(IGFX_GEN8_CORE, DeviceQueueTest, createDeviceQueuesWhenSingleDeviceQueueIsSupported) {
    auto maxOnDeviceQueues = device->getDeviceInfo().maxOnDeviceQueues;
    const_cast<DeviceInfo *>(&device->getDeviceInfo())->maxOnDeviceQueues = 1;

    auto deviceQueue1 = createQueueObject();
    ASSERT_NE(deviceQueue1, nullptr);
    EXPECT_EQ(deviceQueue1->getReference(), 1);

    auto deviceQueue2 = createQueueObject();
    EXPECT_EQ(deviceQueue2, nullptr);

    delete deviceQueue1;

    const_cast<DeviceInfo *>(&device->getDeviceInfo())->maxOnDeviceQueues = maxOnDeviceQueues;
}

HWCMDTEST_F(IGFX_GEN8_CORE, DeviceQueueTest, createDeviceQueuesWhenMultipleDeviceQueuesAreSupported) {
    auto maxOnDeviceQueues = device->getDeviceInfo().maxOnDeviceQueues;
    const_cast<DeviceInfo *>(&device->getDeviceInfo())->maxOnDeviceQueues = 2;

    auto deviceQueue1 = createQueueObject();
    ASSERT_NE(deviceQueue1, nullptr);
    EXPECT_EQ(deviceQueue1->getReference(), 1);

    auto deviceQueue2 = createQueueObject();
    ASSERT_NE(deviceQueue2, nullptr);
    EXPECT_EQ(deviceQueue2->getReference(), 1);

    EXPECT_NE(deviceQueue2, deviceQueue1);

    delete deviceQueue1;
    delete deviceQueue2;

    const_cast<DeviceInfo *>(&device->getDeviceInfo())->maxOnDeviceQueues = maxOnDeviceQueues;
}

typedef DeviceQueueTest DeviceQueueBuffer;

HWCMDTEST_F(IGFX_GEN8_CORE, DeviceQueueBuffer, setPreferredSizeWhenNoPropertyGiven) {
    auto &deviceInfo = device->getDeviceInfo();
    deviceQueue = createQueueObject(); // only minimal properties
    ASSERT_NE(deviceQueue, nullptr);
    checkQueueBuffer(deviceInfo.queueOnDevicePreferredSize);
    deviceQueue->release();
}

HWCMDTEST_F(IGFX_GEN8_CORE, DeviceQueueBuffer, setPreferredSizeWhenInvalidPropertyGiven) {
    cl_queue_properties properties[5] = {CL_QUEUE_PROPERTIES, deviceQueueProperties::minimumProperties[1],
                                         CL_QUEUE_SIZE, 0, 0};
    auto &deviceInfo = device->getDeviceInfo();

    deviceQueue = createQueueObject(properties); // zero size
    ASSERT_NE(deviceQueue, nullptr);

    checkQueueBuffer(deviceInfo.queueOnDevicePreferredSize);
    delete deviceQueue;

    properties[3] = static_cast<cl_queue_properties>(deviceInfo.queueOnDeviceMaxSize + 1);
    deviceQueue = createQueueObject(properties); // greater than max
    EXPECT_EQ(deviceQueue, nullptr);
    delete deviceQueue;
}

HWCMDTEST_F(IGFX_GEN8_CORE, DeviceQueueBuffer, setValidQueueSize) {
    auto &deviceInfo = device->getDeviceInfo();
    cl_uint validSize = deviceInfo.queueOnDevicePreferredSize - 1;
    cl_queue_properties properties[5] = {CL_QUEUE_PROPERTIES, deviceQueueProperties::minimumProperties[1],
                                         CL_QUEUE_SIZE, static_cast<cl_queue_properties>(validSize),
                                         0};

    EXPECT_NE(validSize, alignUp(validSize, MemoryConstants::pageSize)); // create aligned
    deviceQueue = createQueueObject(properties);
    ASSERT_NE(deviceQueue, nullptr);

    checkQueueBuffer(validSize);
    delete deviceQueue;
}

HWCMDTEST_F(IGFX_GEN8_CORE, DeviceQueueBuffer, initValues) {
    auto &deviceInfo = device->getDeviceInfo();

    deviceQueue = createQueueObject();
    ASSERT_NE(deviceQueue, nullptr);

    IGIL_CommandQueue expectedIgilCmdQueue = getExpectedInitIgilCmdQueue(deviceQueue);
    EXPECT_EQ(static_cast<uint32_t>(deviceQueue->isProfilingEnabled()), expectedIgilCmdQueue.m_controls.m_IsProfilingEnabled);

    IGIL_EventPool expectedIgilEventPool = {0, 0, 0};
    expectedIgilEventPool.m_head = 0;
    expectedIgilEventPool.m_size = deviceInfo.maxOnDeviceEvents;

    // initialized header
    EXPECT_EQ(0, memcmp(deviceQueue->getQueueBuffer()->getUnderlyingBuffer(),
                        &expectedIgilCmdQueue, sizeof(IGIL_CommandQueue)));

    EXPECT_EQ(0, memcmp(deviceQueue->getEventPoolBuffer()->getUnderlyingBuffer(),
                        &expectedIgilEventPool, sizeof(IGIL_EventPool)));

    delete deviceQueue;
}

typedef DeviceQueueTest DeviceQueueStackBuffer;

HWCMDTEST_F(IGFX_GEN8_CORE, DeviceQueueStackBuffer, allocateResourcesZeroesStackBufferAndQueueStorage) {
    deviceQueue = createQueueObject();
    ASSERT_NE(deviceQueue, nullptr);

    EXPECT_THAT(deviceQueue->getQueueStorageBuffer()->getUnderlyingBuffer(), MemoryZeroed(deviceQueue->getQueueStorageBuffer()->getUnderlyingBufferSize()));
    EXPECT_THAT(deviceQueue->getStackBuffer()->getUnderlyingBuffer(), MemoryZeroed(deviceQueue->getStackBuffer()->getUnderlyingBufferSize()));
    delete deviceQueue;
}

HWCMDTEST_F(IGFX_GEN8_CORE, DeviceQueueStackBuffer, initAllocation) {
    deviceQueue = createQueueObject();
    ASSERT_NE(deviceQueue, nullptr);

    auto maxEnqueue = deviceQueue->getQueueSize() / sizeof(IGIL_CommandHeader);
    //stack can hold at most 3 full loads of commands
    auto expectedStackSize = maxEnqueue * sizeof(uint32_t) * 3;
    expectedStackSize = alignUp(expectedStackSize, MemoryConstants::pageSize);

    ASSERT_NE(deviceQueue->getStackBuffer(), nullptr);
    EXPECT_EQ(deviceQueue->getStackBuffer()->getUnderlyingBufferSize(), expectedStackSize);
    delete deviceQueue;
}

typedef DeviceQueueTest DeviceQueueStorageBuffer;

HWCMDTEST_F(IGFX_GEN8_CORE, DeviceQueueStorageBuffer, initAllocation) {
    deviceQueue = createQueueObject();
    ASSERT_NE(deviceQueue, nullptr);

    auto expectedStorageSize = deviceQueue->getQueueBuffer()->getUnderlyingBufferSize() * 2;
    expectedStorageSize = alignUp(expectedStorageSize, MemoryConstants::pageSize);

    ASSERT_NE(deviceQueue->getQueueStorageBuffer(), nullptr);
    EXPECT_EQ(deviceQueue->getQueueStorageBuffer()->getUnderlyingBufferSize(), expectedStorageSize);
    delete deviceQueue;
}

typedef DeviceQueueTest DefaultDeviceQueue;

HWCMDTEST_F(IGFX_GEN8_CORE, DefaultDeviceQueue, createOnlyOneDefaultDeviceQueueWhenSingleDeviceQueueIsSupported) {
    cl_queue_properties properties[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_ON_DEVICE_DEFAULT, 0, 0, 0};

    auto maxOnDeviceQueues = device->getDeviceInfo().maxOnDeviceQueues;
    const_cast<DeviceInfo *>(&device->getDeviceInfo())->maxOnDeviceQueues = 1;

    auto deviceQueue1 = createQueueObject(properties);
    ASSERT_NE(deviceQueue1, nullptr);

    EXPECT_EQ(pContext->getDefaultDeviceQueue(), deviceQueue1);
    EXPECT_EQ(deviceQueue1->getReference(), 1);

    auto deviceQueue2 = createQueueObject(properties);
    ASSERT_NE(deviceQueue2, nullptr);

    EXPECT_EQ(deviceQueue2, deviceQueue1);

    EXPECT_EQ(pContext->getDefaultDeviceQueue(), deviceQueue1);
    EXPECT_EQ(deviceQueue1->getReference(), 2);

    deviceQueue1->release();
    deviceQueue2->release();

    const_cast<DeviceInfo *>(&device->getDeviceInfo())->maxOnDeviceQueues = maxOnDeviceQueues;
}

HWCMDTEST_F(IGFX_GEN8_CORE, DefaultDeviceQueue, createOnlyOneDefaultDeviceQueueWhenMultipleDeviceQueuesAreSupported) {
    cl_queue_properties properties[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_ON_DEVICE_DEFAULT, 0, 0, 0};

    auto maxOnDeviceQueues = device->getDeviceInfo().maxOnDeviceQueues;
    const_cast<DeviceInfo *>(&device->getDeviceInfo())->maxOnDeviceQueues = 2;

    auto deviceQueue1 = createQueueObject(properties);
    ASSERT_NE(deviceQueue1, nullptr);

    EXPECT_EQ(pContext->getDefaultDeviceQueue(), deviceQueue1);
    EXPECT_EQ(deviceQueue1->getReference(), 1);

    auto deviceQueue2 = createQueueObject(properties);
    ASSERT_NE(deviceQueue2, nullptr);

    EXPECT_EQ(deviceQueue2, deviceQueue1);

    EXPECT_EQ(pContext->getDefaultDeviceQueue(), deviceQueue1);
    EXPECT_EQ(deviceQueue1->getReference(), 2);

    deviceQueue1->release();
    deviceQueue2->release();

    const_cast<DeviceInfo *>(&device->getDeviceInfo())->maxOnDeviceQueues = maxOnDeviceQueues;
}

typedef DeviceQueueTest DeviceQueueEventPool;

HWCMDTEST_F(IGFX_GEN8_CORE, DeviceQueueEventPool, poolBufferSize) {
    auto &deviceInfo = device->getDeviceInfo();

    // number of events + event pool representation
    auto expectedSize = static_cast<uint32_t>(deviceInfo.maxOnDeviceEvents * sizeof(IGIL_DeviceEvent) +
                                              sizeof(IGIL_EventPool));
    expectedSize = alignUp(expectedSize, MemoryConstants::pageSize);

    auto deviceQueue = createQueueObject();
    ASSERT_NE(deviceQueue, nullptr);

    ASSERT_NE(deviceQueue->getEventPoolBuffer(), nullptr);
    EXPECT_EQ(deviceQueue->getEventPoolBuffer()->getUnderlyingBufferSize(), expectedSize);

    delete deviceQueue;
}

HWCMDTEST_F(IGFX_GEN8_CORE, DeviceQueueTest, sizeOfDshBuffer) {
    deviceQueue = createQueueObject();
    ASSERT_NE(deviceQueue, nullptr);

    ASSERT_NE(deviceQueue->getDshBuffer(), nullptr);
    auto dshBufferSize = deviceQueue->getDshBuffer()->getUnderlyingBufferSize();

    EXPECT_LE(761856u, dshBufferSize);
    delete deviceQueue;
}

HWCMDTEST_F(IGFX_GEN8_CORE, DeviceQueueTest, dispatchScheduler) {
    DeviceQueue devQueue;
    MockContext context;
    MockProgram program(*device->getExecutionEnvironment());
    CommandQueue cmdQ(nullptr, nullptr, 0);
    KernelInfo info;
    MockSchedulerKernel *kernel = new MockSchedulerKernel(&program, info, *device);
    LinearStream cmdStream;

    devQueue.dispatchScheduler(cmdStream, *kernel, device->getPreemptionMode(), nullptr, nullptr);
    delete kernel;
}
