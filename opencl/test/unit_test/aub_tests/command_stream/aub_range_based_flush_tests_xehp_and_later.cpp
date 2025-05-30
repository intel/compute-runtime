/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/cache_flush_xehp_and_later.inl"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/common/helpers/cmd_buffer_validator.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/aub_tests/fixtures/aub_fixture.h"
#include "opencl/test/unit_test/aub_tests/fixtures/hello_world_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "test_traits_common.h"

using namespace NEO;

struct RangeBasedFlushTest : public KernelAUBFixture<SimpleKernelFixture>, public ::testing::Test {

    void SetUp() override {
        debugManager.flags.PerformImplicitFlushForNewResource.set(0);
        debugManager.flags.PerformImplicitFlushForIdleGpu.set(0);
        debugManager.flags.CsrDispatchMode.set(static_cast<int32_t>(DispatchMode::batchedDispatch));

        KernelAUBFixture<SimpleKernelFixture>::setUp();
    };

    void TearDown() override {
        KernelAUBFixture<SimpleKernelFixture>::tearDown();
    }

    cl_int retVal = CL_SUCCESS;
    DebugManagerStateRestore debugSettingsRestore;
};

struct L3ControlSupportedMatcher {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        if constexpr (HwMapper<productFamily>::GfxProduct::supportsCmdSet(IGFX_XE_HP_CORE)) {
            return TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::l3ControlSupported;
        }
        return false;
    }
};

HWTEST2_F(RangeBasedFlushTest, givenNoDcFlushInPipeControlWhenL3ControlFlushesCachesThenExpectFlushedCaches, L3ControlSupportedMatcher) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using L3_CONTROL = typename FamilyType::L3_CONTROL;
    using L3_FLUSH_ADDRESS_RANGE = typename FamilyType::L3_FLUSH_ADDRESS_RANGE;

    debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);

    constexpr size_t bufferSize = MemoryConstants::pageSize;
    char bufferAMemory[bufferSize];
    char bufferBMemory[bufferSize];
    for (uint32_t i = 0; i < bufferSize / MemoryConstants::pageSize; ++i) {
        memset(bufferAMemory + i * MemoryConstants::pageSize, 1 + i, MemoryConstants::pageSize);
        memset(bufferBMemory + i * MemoryConstants::pageSize, 129 + i, MemoryConstants::pageSize);
    }

    auto retVal = CL_INVALID_VALUE;
    auto srcBuffer = std::unique_ptr<Buffer>(Buffer::create(context,
                                                            CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                                            bufferSize, bufferAMemory, retVal));

    ASSERT_NE(nullptr, srcBuffer);
    auto dstBuffer = std::unique_ptr<Buffer>(Buffer::create(context,
                                                            CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                                            bufferSize, bufferBMemory, retVal));
    ASSERT_NE(nullptr, dstBuffer);

    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    retVal = pCmdQ->enqueueCopyBuffer(srcBuffer.get(), dstBuffer.get(),
                                      0, 0,
                                      bufferSize, numEventsInWaitList,
                                      eventWaitList, event);

    EXPECT_EQ(CL_SUCCESS, retVal);

    L3RangesVec ranges;
    ranges.push_back(L3Range::fromAddressSizeWithPolicy(ptrOffset(dstBuffer->getGraphicsAllocation(rootDeviceIndex)->getGpuAddress(), dstBuffer->getOffset()), MemoryConstants::pageSize,
                                                        L3_FLUSH_ADDRESS_RANGE::L3_FLUSH_EVICTION_POLICY_FLUSH_L3_WITH_EVICTION));
    size_t requiredSize = getSizeNeededToFlushGpuCache<FamilyType>(ranges, false) + 2 * sizeof(PIPE_CONTROL);
    LinearStream &l3FlushCmdStream = pCmdQ->getCS(requiredSize);
    auto offset = l3FlushCmdStream.getUsed();
    auto pcBeforeFlush = l3FlushCmdStream.getSpaceForCmd<PIPE_CONTROL>();
    *pcBeforeFlush = FamilyType::cmdInitPipeControl;

    flushGpuCache<FamilyType>(&l3FlushCmdStream, ranges, 0U, device->getHardwareInfo());

    auto &csr = pCmdQ->getGpgpuCommandStreamReceiver();
    auto flags = DispatchFlagsHelper::createDefaultDispatchFlags();
    flags.blocking = true;

    debugManager.flags.DisableDcFlushInEpilogue.set(true);
    csr.flushTask(l3FlushCmdStream, offset,
                  &pCmdQ->getIndirectHeap(NEO::IndirectHeap::Type::dynamicState, 0),
                  &pCmdQ->getIndirectHeap(NEO::IndirectHeap::Type::indirectObject, 0),
                  &pCmdQ->getIndirectHeap(NEO::IndirectHeap::Type::surfaceState, 0),
                  pCmdQ->taskLevel,
                  flags,
                  pCmdQ->getDevice());

    std::string err;

    std::vector<MatchCmd *> expectedCommands{
        new MatchAnyCmd(anyNumber),
        new MatchHwCmd<FamilyType, PIPE_CONTROL>(1, Expects{EXPECT_MEMBER(PIPE_CONTROL, getCommandStreamerStallEnable, true), EXPECT_MEMBER(PIPE_CONTROL, getDcFlushEnable, false)}),
        new MatchHwCmd<FamilyType, L3_CONTROL>(1, Expects{EXPECT_MEMBER(L3_CONTROL, getPostSyncOperation, L3_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_NO_WRITE)}),
    };
    if (MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(device->getRootDeviceEnvironment())) {
        expectedCommands.push_back(new MatchHwCmd<FamilyType, PIPE_CONTROL>(1, Expects{EXPECT_MEMBER(PIPE_CONTROL, getDcFlushEnable, false)}));
        if (MemorySynchronizationCommands<FamilyType>::getSizeForAdditionalSynchronization(NEO::FenceType::release, device->getRootDeviceEnvironment()) > 0) {
            expectedCommands.push_back(new MatchHwCmd<FamilyType, MI_SEMAPHORE_WAIT>(1, Expects{EXPECT_MEMBER(MI_SEMAPHORE_WAIT, getSemaphoreDataDword, EncodeSemaphore<FamilyType>::invalidHardwareTag)}));
        }
    }
    expectedCommands.push_back(new MatchHwCmd<FamilyType, PIPE_CONTROL>(1, Expects{EXPECT_MEMBER(PIPE_CONTROL, getDcFlushEnable, false)}));
    expectedCommands.push_back(new MatchAnyCmd(anyNumber));
    expectedCommands.push_back(new MatchHwCmd<FamilyType, PIPE_CONTROL>(0));

    auto cmdBuffOk = expectCmdBuff<FamilyType>(l3FlushCmdStream, 0, std::move(expectedCommands), &err);
    EXPECT_TRUE(cmdBuffOk) << err;

    expectMemory<FamilyType>(addrToPtr(ptrOffset(dstBuffer->getGraphicsAllocation(rootDeviceIndex)->getGpuAddress(), dstBuffer->getOffset())),
                             bufferAMemory, bufferSize);
}

HWTEST2_F(RangeBasedFlushTest, givenL3ControlWhenPostSyncIsSetThenExpectPostSyncWrite, L3ControlSupportedMatcher) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using L3_CONTROL = typename FamilyType::L3_CONTROL;
    using L3_FLUSH_ADDRESS_RANGE = typename FamilyType::L3_FLUSH_ADDRESS_RANGE;

    if (MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(device->getRootDeviceEnvironment())) {
        GTEST_SKIP();
    }

    constexpr size_t bufferSize = MemoryConstants::pageSize;
    char bufferAMemory[bufferSize];
    char bufferBMemory[bufferSize];
    for (uint32_t i = 0; i < bufferSize / MemoryConstants::pageSize; ++i) {
        memset(bufferAMemory + i * MemoryConstants::pageSize, 1 + i, MemoryConstants::pageSize);
        memset(bufferBMemory + i * MemoryConstants::pageSize, 129 + i, MemoryConstants::pageSize);
    }

    auto retVal = CL_INVALID_VALUE;
    auto srcBuffer = std::unique_ptr<Buffer>(Buffer::create(context,
                                                            CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                                            bufferSize, bufferAMemory, retVal));

    ASSERT_NE(nullptr, srcBuffer);
    auto dstBuffer = std::unique_ptr<Buffer>(Buffer::create(context,
                                                            CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                                            bufferSize, bufferBMemory, retVal));
    ASSERT_NE(nullptr, dstBuffer);

    auto postSyncBuffer = std::unique_ptr<Buffer>(Buffer::create(context,
                                                                 CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                                                 sizeof(uint64_t), bufferAMemory, retVal));
    ASSERT_NE(nullptr, dstBuffer);

    uint64_t expectedPostSyncData = 0;

    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    retVal = pCmdQ->enqueueCopyBuffer(srcBuffer.get(), dstBuffer.get(),
                                      0, 0,
                                      bufferSize, numEventsInWaitList,
                                      eventWaitList, event);

    EXPECT_EQ(CL_SUCCESS, retVal);

    L3RangesVec ranges;
    ranges.push_back(L3Range::fromAddressSizeWithPolicy(ptrOffset(dstBuffer->getGraphicsAllocation(rootDeviceIndex)->getGpuAddress(), dstBuffer->getOffset()),
                                                        MemoryConstants::pageSize, L3_FLUSH_ADDRESS_RANGE::L3_FLUSH_EVICTION_POLICY_FLUSH_L3_WITH_EVICTION));
    size_t requiredSize = getSizeNeededToFlushGpuCache<FamilyType>(ranges, true) + 2 * sizeof(PIPE_CONTROL);
    LinearStream &l3FlushCmdStream = pCmdQ->getCS(requiredSize);
    auto offset = l3FlushCmdStream.getUsed();
    auto pcBeforeFlush = l3FlushCmdStream.getSpaceForCmd<PIPE_CONTROL>();
    *pcBeforeFlush = FamilyType::cmdInitPipeControl;

    flushGpuCache<FamilyType>(&l3FlushCmdStream, ranges, ptrOffset(postSyncBuffer->getGraphicsAllocation(rootDeviceIndex)->getGpuAddress(), postSyncBuffer->getOffset()), device->getHardwareInfo());

    auto &csr = pCmdQ->getGpgpuCommandStreamReceiver();
    auto flags = DispatchFlagsHelper::createDefaultDispatchFlags();
    flags.blocking = true;

    debugManager.flags.DisableDcFlushInEpilogue.set(true);
    csr.makeResident(*postSyncBuffer->getGraphicsAllocation(rootDeviceIndex));
    csr.flushTask(l3FlushCmdStream, offset,
                  &pCmdQ->getIndirectHeap(NEO::IndirectHeap::Type::dynamicState, 0),
                  &pCmdQ->getIndirectHeap(NEO::IndirectHeap::Type::indirectObject, 0),
                  &pCmdQ->getIndirectHeap(NEO::IndirectHeap::Type::surfaceState, 0),
                  pCmdQ->taskLevel,
                  flags,
                  pCmdQ->getDevice());

    std::string err;
    auto cmdBuffOk = expectCmdBuff<FamilyType>(l3FlushCmdStream, 0,
                                               std::vector<MatchCmd *>{
                                                   new MatchAnyCmd(anyNumber),
                                                   new MatchHwCmd<FamilyType, PIPE_CONTROL>(1, Expects{EXPECT_MEMBER(PIPE_CONTROL, getCommandStreamerStallEnable, true), EXPECT_MEMBER(PIPE_CONTROL, getDcFlushEnable, false)}),
                                                   new MatchHwCmd<FamilyType, L3_CONTROL>(1, Expects{EXPECT_MEMBER(L3_CONTROL, getPostSyncOperation, L3_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA)}),
                                                   new MatchHwCmd<FamilyType, PIPE_CONTROL>(1, Expects{EXPECT_MEMBER(PIPE_CONTROL, getDcFlushEnable, false)}), // epilogue
                                                   new MatchAnyCmd(anyNumber),
                                                   new MatchHwCmd<FamilyType, PIPE_CONTROL>(0),
                                               },
                                               &err);
    EXPECT_TRUE(cmdBuffOk) << err;

    expectMemory<FamilyType>(addrToPtr(ptrOffset(dstBuffer->getGraphicsAllocation(rootDeviceIndex)->getGpuAddress(), dstBuffer->getOffset())),
                             bufferAMemory, bufferSize);

    expectMemory<FamilyType>(addrToPtr(ptrOffset(postSyncBuffer->getGraphicsAllocation(rootDeviceIndex)->getGpuAddress(), postSyncBuffer->getOffset())),
                             &expectedPostSyncData, sizeof(expectedPostSyncData));
}
