/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/gpgpu_walker.h"
#include "runtime/context/context.h"
#include "runtime/utilities/tag_allocator.h"
#include "unit_tests/fixtures/device_host_queue_fixture.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/mocks/mock_device_queue.h"
#include "unit_tests/mocks/mock_kernel.h"

using namespace NEO;
using namespace DeviceHostQueue;

typedef DeviceQueueHwTest Gen12LpDeviceQueueSlb;

GEN12LPTEST_F(Gen12LpDeviceQueueSlb, expectedAllocationSize) {
    deviceQueue = createQueueObject();
    ASSERT_NE(deviceQueue, nullptr);

    auto expectedSize = getMinimumSlbSize<FamilyType>();
    expectedSize *= 128; //num of enqueues
    expectedSize += sizeof(typename FamilyType::MI_BATCH_BUFFER_START);
    expectedSize = alignUp(expectedSize, MemoryConstants::pageSize);
    expectedSize += MockDeviceQueueHw<FamilyType>::getExecutionModelCleanupSectionSize();
    expectedSize += (4 * MemoryConstants::pageSize);
    expectedSize = alignUp(expectedSize, MemoryConstants::pageSize);

    ASSERT_NE(deviceQueue->getSlbBuffer(), nullptr);
    EXPECT_EQ(deviceQueue->getSlbBuffer()->getUnderlyingBufferSize(), expectedSize);

    delete deviceQueue;
}

GEN12LPTEST_F(Gen12LpDeviceQueueSlb, SlbCommandsWa) {
    auto mockDeviceQueueHw = new MockDeviceQueueHw<FamilyType>(pContext, device,
                                                               DeviceHostQueue::deviceQueueProperties::minimumProperties[0]);
    EXPECT_FALSE(mockDeviceQueueHw->arbCheckWa);
    EXPECT_FALSE(mockDeviceQueueHw->pipeControlWa);
    EXPECT_FALSE(mockDeviceQueueHw->miAtomicWa);
    EXPECT_FALSE(mockDeviceQueueHw->lriWa);

    delete mockDeviceQueueHw;
}

GEN12LPTEST_F(Gen12LpDeviceQueueSlb, givenDeviceCommandQueueWithProfilingWhenBatchBufferIsBuiltThenOneMiStoreRegisterMemWithMmioRemapEnableIsPresent) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    auto mockDeviceQueueHw = new MockDeviceQueueHw<FamilyType>(pContext, device, deviceQueueProperties::minimumProperties[0]);
    auto commandsSize = mockDeviceQueueHw->getMinimumSlbSize() + mockDeviceQueueHw->getWaCommandsSize();
    MockParentKernel *mockParentKernel = MockParentKernel::create(*pContext);
    uint32_t taskCount = 7;

    auto hwTimeStamp = pCommandQueue->getGpgpuCommandStreamReceiver().getEventTsAllocator()->getTag();
    mockDeviceQueueHw->buildSlbDummyCommands();
    mockDeviceQueueHw->addExecutionModelCleanUpSection(mockParentKernel, hwTimeStamp, 0x123, taskCount);

    HardwareParse hwParser;
    auto *slbCS = mockDeviceQueueHw->getSlbCS();
    size_t cleanupSectionOffset = alignUp(mockDeviceQueueHw->numberOfDeviceEnqueues * commandsSize + sizeof(MI_BATCH_BUFFER_START), MemoryConstants::pageSize);
    size_t cleanupSectionOffsetToParse = cleanupSectionOffset;

    hwParser.parseCommands<FamilyType>(*slbCS, cleanupSectionOffsetToParse);
    hwParser.findHardwareCommands<FamilyType>();

    auto itorMiStore = find<MI_STORE_REGISTER_MEM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), itorMiStore);
    auto pMiStore = genCmdCast<MI_STORE_REGISTER_MEM *>(*itorMiStore);
    ASSERT_NE(nullptr, pMiStore);
    EXPECT_EQ(GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW, pMiStore->getRegisterAddress());
    EXPECT_TRUE(pMiStore->getMmioRemapEnable());

    ++itorMiStore;
    pMiStore = genCmdCast<MI_STORE_REGISTER_MEM *>(*itorMiStore);
    EXPECT_EQ(nullptr, pMiStore);

    delete mockParentKernel;
    delete mockDeviceQueueHw;
}
