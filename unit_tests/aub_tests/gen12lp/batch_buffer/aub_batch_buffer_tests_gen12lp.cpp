/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "aub_batch_buffer_tests_gen12lp.h"

#include "core/memory_manager/graphics_allocation.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/event/event.h"
#include "runtime/helpers/hardware_commands_helper.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/utilities/tag_allocator.h"
#include "test.h"
#include "unit_tests/aub_tests/command_stream/aub_command_stream_fixture.h"
#include "unit_tests/aub_tests/fixtures/hello_world_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_device.h"

using Gen12LPAubBatchBufferTests = Test<NEO::DeviceFixture>;
using Gen12LPTimestampTests = Test<HelloWorldFixture<AUBHelloWorldFixtureFactory>>;

static constexpr auto gpuBatchBufferAddr = 0x400400001000; // 47-bit GPU address

GEN12LPTEST_F(Gen12LPAubBatchBufferTests, givenSimpleRCSWithBatchBufferWhenItHasMSBSetInGpuAddressThenAUBShouldBeSetupSuccessfully) {
    setupAUBWithBatchBuffer<FamilyType>(pDevice, aub_stream::ENGINE_RCS, gpuBatchBufferAddr);
}

GEN12LPTEST_F(Gen12LPAubBatchBufferTests, givenSimpleCCSWithBatchBufferWhenItHasMSBSetInGpuAddressThenAUBShouldBeSetupSuccessfully) {
    setupAUBWithBatchBuffer<FamilyType>(pDevice, aub_stream::ENGINE_CCS, gpuBatchBufferAddr);
}

GEN12LPTEST_F(Gen12LPTimestampTests, DISABLED_GivenCommandQueueWithProfilingEnabledWhenKernelIsEnqueuedThenProfilingTimestampsAreNotZero) {
    cl_queue_properties properties[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    CommandQueueHw<FamilyType> cmdQ(pContext, pDevice, &properties[0]);
    EXPECT_EQ(aub_stream::ENGINE_CCS, pDevice->getDefaultEngine().osContext->getEngineType());

    const uint32_t bufferSize = 4;
    std::unique_ptr<Buffer> buffer(Buffer::create(pContext, CL_MEM_READ_WRITE, bufferSize, nullptr, retVal));
    memset(buffer->getGraphicsAllocation()->getUnderlyingBuffer(), 0, buffer->getGraphicsAllocation()->getUnderlyingBufferSize());
    buffer->forceDisallowCPUCopy = true;

    uint8_t writeData[bufferSize] = {0x11, 0x22, 0x33, 0x44};
    cl_event event;
    cmdQ.enqueueWriteBuffer(buffer.get(), CL_TRUE, 0, bufferSize, writeData, nullptr, 0, nullptr, &event);
    ASSERT_NE(event, nullptr);
    auto eventObject = castToObject<Event>(event);
    ASSERT_NE(eventObject, nullptr);
    expectMemory<FamilyType>(buffer->getGraphicsAllocation()->getUnderlyingBuffer(), writeData, bufferSize);

    uint64_t expectedTimestampValues[2] = {0, 0};
    TagNode<HwTimeStamps> &hwTimeStamps = *(eventObject->getHwTimeStampNode());
    uint64_t timeStampStartAddress = hwTimeStamps.getGpuAddress() + offsetof(HwTimeStamps, ContextStartTS);
    uint64_t timeStampEndAddress = hwTimeStamps.getGpuAddress() + offsetof(HwTimeStamps, ContextEndTS);
    expectMemoryNotEqual<FamilyType>(reinterpret_cast<void *>(timeStampStartAddress), &expectedTimestampValues[0], sizeof(uint64_t));
    expectMemoryNotEqual<FamilyType>(reinterpret_cast<void *>(timeStampEndAddress), &expectedTimestampValues[1], sizeof(uint64_t));

    eventObject->release();
}
