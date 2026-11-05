/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/cache_flush_xehp_and_later.inl"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/test/common/helpers/cmd_buffer_validator.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/helpers/test_traits.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist_memory_copy_params.h"
#include "level_zero/core/test/aub_tests/command_stream/command_stream_aub_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

#include "gtest/gtest.h"

#include <cstring>
#include <vector>

using namespace NEO;

namespace L0 {
namespace ult {

struct RangeBasedFlushTest : public CommandStreamAubFixture, public ::testing::Test {
    void SetUp() override {
        debugManager.flags.PerformImplicitFlushForNewResource.set(0);
        debugManager.flags.PerformImplicitFlushForIdleGpu.set(0);
        debugManager.flags.EnableDirectSubmission.set(0);
        debugManager.flags.CsrDispatchMode.set(static_cast<int32_t>(DispatchMode::batchedDispatch));
        debugManager.flags.AllowDcFlush.set(0);

        CommandStreamAubFixture::setUp();
    }

    void TearDown() override {
        CommandStreamAubFixture::tearDown();
    }

    void copyBuffer(const void *srcHostMemory, size_t size) {
        auto srcMemory = this->allocateHostMemory(size, MemoryConstants::pageSize);
        ASSERT_NE(nullptr, srcMemory);
        memcpy(srcMemory, srcHostMemory, size);

        this->dstMemory = this->allocateDeviceMemory(size, MemoryConstants::pageSize);
        ASSERT_NE(nullptr, this->dstMemory);

        CmdListMemoryCopyParams copyParams = {};
        ASSERT_EQ(ZE_RESULT_SUCCESS, this->commandList->appendMemoryCopy(this->dstMemory, srcMemory, size, nullptr, 0, nullptr, copyParams));
        this->executeAndSynchronize();
    }

    void flushStreamBlocking(size_t streamStartOffset) {
        auto dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
        dispatchFlags.blocking = true;

        debugManager.flags.DisableDcFlushInEpilogue.set(true);
        this->flushStream(streamStartOffset, dispatchFlags);
    }

    void *dstMemory = nullptr;
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
    memset(bufferAMemory, 1, bufferSize);

    this->copyBuffer(bufferAMemory, bufferSize);

    L3RangesVec ranges;
    ranges.push_back(L3Range::fromAddressSizeWithPolicy(castToUint64(this->dstMemory), MemoryConstants::pageSize,
                                                        L3_FLUSH_ADDRESS_RANGE::L3_FLUSH_EVICTION_POLICY_FLUSH_L3_WITH_EVICTION));
    size_t requiredSize = getSizeNeededToFlushGpuCache<FamilyType>(ranges, false) + 2 * sizeof(PIPE_CONTROL);
    ASSERT_LE(requiredSize, this->taskStream->getAvailableSpace());
    auto offset = this->taskStream->getUsed();
    auto pcBeforeFlush = this->taskStream->getSpaceForCmd<PIPE_CONTROL>();
    *pcBeforeFlush = FamilyType::cmdInitPipeControl;

    flushGpuCache<FamilyType>(this->taskStream.get(), ranges, 0U, this->neoDevice->getHardwareInfo());

    this->flushStreamBlocking(offset);

    std::string err;

    std::vector<MatchCmd *> expectedCommands{
        new MatchAnyCmd(anyNumber),
        new MatchHwCmd<FamilyType, PIPE_CONTROL>(1, Expects{EXPECT_MEMBER(PIPE_CONTROL, getCommandStreamerStallEnable, true), EXPECT_MEMBER(PIPE_CONTROL, getDcFlushEnable, false)}),
        new MatchHwCmd<FamilyType, L3_CONTROL>(1, Expects{EXPECT_MEMBER(L3_CONTROL, getPostSyncOperation, L3_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_NO_WRITE)}),
    };
    if (MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(this->neoDevice->getRootDeviceEnvironment())) {
        expectedCommands.push_back(new MatchHwCmd<FamilyType, PIPE_CONTROL>(1, Expects{EXPECT_MEMBER(PIPE_CONTROL, getDcFlushEnable, false)}));
        if (MemorySynchronizationCommands<FamilyType>::getSizeForAdditionalSynchronization(NEO::FenceType::release, this->neoDevice->getRootDeviceEnvironment()) > 0) {
            expectedCommands.push_back(new MatchHwCmd<FamilyType, MI_SEMAPHORE_WAIT>(1, Expects{EXPECT_MEMBER(MI_SEMAPHORE_WAIT, getSemaphoreDataDword, EncodeSemaphore<FamilyType>::invalidHardwareTag)}));
        }
    }
    expectedCommands.push_back(new MatchHwCmd<FamilyType, PIPE_CONTROL>(1, Expects{EXPECT_MEMBER(PIPE_CONTROL, getDcFlushEnable, false)}));
    expectedCommands.push_back(new MatchAnyCmd(anyNumber));
    expectedCommands.push_back(new MatchHwCmd<FamilyType, PIPE_CONTROL>(0));

    auto cmdBuffOk = expectCmdBuff<FamilyType>(*this->taskStream, 0, std::move(expectedCommands), &err);
    EXPECT_TRUE(cmdBuffOk) << err;

    this->expectMemory<FamilyType>(this->dstMemory, bufferAMemory, bufferSize);
}

HWTEST2_F(RangeBasedFlushTest, givenL3ControlWhenPostSyncIsSetThenExpectPostSyncWrite, L3ControlSupportedMatcher) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using L3_CONTROL = typename FamilyType::L3_CONTROL;
    using L3_FLUSH_ADDRESS_RANGE = typename FamilyType::L3_FLUSH_ADDRESS_RANGE;

    if (MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(this->neoDevice->getRootDeviceEnvironment())) {
        GTEST_SKIP();
    }

    constexpr size_t bufferSize = MemoryConstants::pageSize;
    char bufferAMemory[bufferSize];
    memset(bufferAMemory, 1, bufferSize);

    auto postSyncBuffer = this->createBufferWithData(bufferAMemory, sizeof(uint64_t));
    ASSERT_NE(nullptr, postSyncBuffer);

    uint64_t expectedPostSyncData = 0;

    this->copyBuffer(bufferAMemory, bufferSize);

    L3RangesVec ranges;
    ranges.push_back(L3Range::fromAddressSizeWithPolicy(castToUint64(this->dstMemory),
                                                        MemoryConstants::pageSize, L3_FLUSH_ADDRESS_RANGE::L3_FLUSH_EVICTION_POLICY_FLUSH_L3_WITH_EVICTION));
    size_t requiredSize = getSizeNeededToFlushGpuCache<FamilyType>(ranges, true) + 2 * sizeof(PIPE_CONTROL);
    ASSERT_LE(requiredSize, this->taskStream->getAvailableSpace());
    auto offset = this->taskStream->getUsed();
    auto pcBeforeFlush = this->taskStream->getSpaceForCmd<PIPE_CONTROL>();
    *pcBeforeFlush = FamilyType::cmdInitPipeControl;

    flushGpuCache<FamilyType>(this->taskStream.get(), ranges, postSyncBuffer->getGpuAddress(), this->neoDevice->getHardwareInfo());

    this->flushStreamBlocking(offset);

    std::string err;
    auto cmdBuffOk = expectCmdBuff<FamilyType>(*this->taskStream, 0,
                                               std::vector<MatchCmd *>{
                                                   new MatchAnyCmd(anyNumber),
                                                   new MatchHwCmd<FamilyType, PIPE_CONTROL>(1, Expects{EXPECT_MEMBER(PIPE_CONTROL, getCommandStreamerStallEnable, true), EXPECT_MEMBER(PIPE_CONTROL, getDcFlushEnable, false)}),
                                                   new MatchHwCmd<FamilyType, L3_CONTROL>(1, Expects{EXPECT_MEMBER(L3_CONTROL, getPostSyncOperation, L3_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA)}),
                                                   new MatchHwCmd<FamilyType, PIPE_CONTROL>(1, Expects{EXPECT_MEMBER(PIPE_CONTROL, getDcFlushEnable, false)}),
                                                   new MatchAnyCmd(anyNumber),
                                                   new MatchHwCmd<FamilyType, PIPE_CONTROL>(0),
                                               },
                                               &err);
    EXPECT_TRUE(cmdBuffOk) << err;

    this->expectMemory<FamilyType>(this->dstMemory, bufferAMemory, bufferSize);

    this->expectMemory<FamilyType>(addrToPtr(postSyncBuffer->getGpuAddress()), &expectedPostSyncData, sizeof(expectedPostSyncData));
}

} // namespace ult
} // namespace L0
