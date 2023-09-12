/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/l3_range.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/test/common/helpers/cmd_buffer_validator.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/static_size3.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/hello_world_fixture.h"
#include "opencl/test/unit_test/helpers/hardware_commands_helper_tests.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"

using namespace NEO;

template <typename FamilyType>
struct L3ControlPolicy : CmdValidator {
    L3ControlPolicy(typename FamilyType::L3_FLUSH_ADDRESS_RANGE::L3_FLUSH_EVICTION_POLICY expectedPolicy)
        : expectedPolicy(expectedPolicy) {
    }
    bool operator()(typename GenCmdList::iterator it, size_t numInScetion, const std::string &member, std::string &outReason) override {
        using L3_FLUSH_ADDRESS_RANGE = typename FamilyType::L3_FLUSH_ADDRESS_RANGE;
        using L3_CONTROL = typename FamilyType::L3_CONTROL;
        auto l3Control = genCmdCast<L3_CONTROL *>(*it);
        auto flushRangesCount = (l3Control->getLength() - 3) / 2;
        l3Control++;
        auto l3Ranges = reinterpret_cast<L3_FLUSH_ADDRESS_RANGE *>(l3Control);
        for (uint32_t i = 0; i < flushRangesCount; i++) {
            if (l3Ranges->getL3FlushEvictionPolicy() != expectedPolicy) {
                outReason = "Invalid L3_FLUSH_EVICTION_POLICY - expected: " + std::to_string(expectedPolicy) + ", got :" + std::to_string(l3Ranges->getL3FlushEvictionPolicy());
                return false;
            }
            l3RangesParsed.push_back(L3Range::fromAddressMask(l3Ranges->getAddress(), l3Ranges->getAddressMask()));
            l3Ranges++;
        }
        return true;
    }
    L3RangesVec l3RangesParsed;
    typename FamilyType::L3_FLUSH_ADDRESS_RANGE::L3_FLUSH_EVICTION_POLICY expectedPolicy;
};

template <typename FamilyType>
class GivenCacheFlushAfterWalkerEnabledWhenSvmAllocationsSetAsCacheFlushRequiringThenExpectCorrectCommandSize : public HardwareCommandsTest {
  public:
    void testBodyImpl() {
        using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
        using L3_CONTROL = typename FamilyType::L3_CONTROL;
        using L3_FLUSH_ADDRESS_RANGE = typename FamilyType::L3_FLUSH_ADDRESS_RANGE;

        DebugManagerStateRestore dbgRestore;
        DebugManager.flags.EnableCacheFlushAfterWalker.set(1);

        CommandQueueHw<FamilyType> cmdQ(nullptr, pClDevice, 0, false);

        void *allocPtr = reinterpret_cast<void *>(static_cast<uintptr_t>(6 * MemoryConstants::pageSize));
        MockGraphicsAllocation svmAllocation{allocPtr, MemoryConstants::pageSize * 2};
        svmAllocation.setFlushL3Required(true);
        this->mockKernelWithInternal->mockKernel->kernelSvmGfxAllocations.push_back(&svmAllocation);
        this->mockKernelWithInternal->mockKernel->svmAllocationsRequireCacheFlush = true;
        StackVec<GraphicsAllocation *, 32> allocationsForCacheFlush;
        this->mockKernelWithInternal->mockKernel->getAllocationsForCacheFlush(allocationsForCacheFlush);
        StackVec<L3Range, 128> subranges;
        for (GraphicsAllocation *alloc : allocationsForCacheFlush) {
            coverRangeExact(alloc->getGpuAddress(), alloc->getUnderlyingBufferSize(), subranges, FamilyType::L3_FLUSH_ADDRESS_RANGE::L3_FLUSH_EVICTION_POLICY_FLUSH_L3_WITH_EVICTION);
        }
        size_t expectedSize = sizeof(COMPUTE_WALKER) + sizeof(PIPE_CONTROL);
        DispatchInfo di;
        size_t actualSize = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, false, false, cmdQ, this->mockKernelWithInternal->mockKernel, di);
        EXPECT_EQ(expectedSize, actualSize);
    }
};

template <typename FamilyType>
class GivenCacheFlushAfterWalkerEnabledWhenSvmAllocationsSetAsCacheFlushRequiringThenExpectCacheFlushCommand : public HardwareCommandsTest {
  public:
    void testBodyImpl() {
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
        using L3_CONTROL = typename FamilyType::L3_CONTROL;

        DebugManagerStateRestore dbgRestore;
        DebugManager.flags.EnableCacheFlushAfterWalker.set(1);

        CommandQueueHw<FamilyType> cmdQ(nullptr, pClDevice, 0, false);
        auto &commandStream = cmdQ.getCS(1024);

        void *allocPtr = reinterpret_cast<void *>(static_cast<uintptr_t>(6 * MemoryConstants::pageSize));
        MockGraphicsAllocation svmAllocation{allocPtr, MemoryConstants::pageSize * 2};
        svmAllocation.setFlushL3Required(true);
        this->mockKernelWithInternal->mockKernel->kernelSvmGfxAllocations.push_back(&svmAllocation);
        this->mockKernelWithInternal->mockKernel->svmAllocationsRequireCacheFlush = true;

        HardwareCommandsHelper<FamilyType>::programCacheFlushAfterWalkerCommand(&commandStream, cmdQ, this->mockKernelWithInternal->mockKernel, 0U);

        std::string err;
        auto cmdBuffOk = expectCmdBuff<FamilyType>(cmdQ.getCS(0), 0,
                                                   std::vector<MatchCmd *>({
                                                       new MatchHwCmd<FamilyType, PIPE_CONTROL>(1, Expects{EXPECT_MEMBER(PIPE_CONTROL, getCommandStreamerStallEnable, true), EXPECT_MEMBER(PIPE_CONTROL, getDcFlushEnable, false)}),
                                                       new MatchHwCmd<FamilyType, L3_CONTROL>(1, Expects{EXPECT_MEMBER(L3_CONTROL, getPostSyncOperation, L3_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_NO_WRITE)}),
                                                   }),
                                                   &err);
        EXPECT_TRUE(cmdBuffOk) << err;
    }
};

template <typename FamilyType>
class GivenCacheFlushAfterWalkerEnabledWhenNoGlobalSurfaceSvmAllocationKernelArgRequireCacheFlushThenExpectNoCacheFlushCommand : public HardwareCommandsTest {
  public:
    void testBodyImpl() {
        DebugManagerStateRestore dbgRestore;
        DebugManager.flags.EnableCacheFlushAfterWalker.set(1);

        CommandQueueHw<FamilyType> cmdQ(nullptr, pClDevice, 0, false);
        auto &commandStream = cmdQ.getCS(1024);

        addSpaceForSingleKernelArg();

        size_t actualSize = HardwareCommandsHelper<FamilyType>::getSizeRequiredForCacheFlush(cmdQ, this->mockKernelWithInternal->mockKernel, 0U);
        EXPECT_EQ(0U, actualSize);

        HardwareCommandsHelper<FamilyType>::programCacheFlushAfterWalkerCommand(&commandStream, cmdQ, this->mockKernelWithInternal->mockKernel, 0U, 0U);
        EXPECT_EQ(0U, commandStream.getUsed());
    }
};

template <typename FamilyType>
class GivenCacheFlushAfterWalkerEnabledWhenProgramGlobalSurfacePresentThenExpectCacheFlushCommand : public HardwareCommandsTest {
  public:
    void testBodyImpl() {
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
        using L3_CONTROL = typename FamilyType::L3_CONTROL;
        DebugManagerStateRestore dbgRestore;
        DebugManager.flags.EnableCacheFlushAfterWalker.set(1);

        CommandQueueHw<FamilyType> cmdQ(nullptr, pClDevice, 0, false);
        auto &commandStream = cmdQ.getCS(1024);

        void *allocPtr = reinterpret_cast<void *>(static_cast<uintptr_t>(6 * MemoryConstants::pageSize));
        MockGraphicsAllocation globalAllocation{allocPtr, MemoryConstants::pageSize * 2};
        this->mockKernelWithInternal->mockProgram->setGlobalSurface(&globalAllocation);

        HardwareCommandsHelper<FamilyType>::programCacheFlushAfterWalkerCommand(&commandStream, cmdQ, this->mockKernelWithInternal->mockKernel, 0U);

        std::string err;
        auto cmdBuffOk = expectCmdBuff<FamilyType>(cmdQ.getCS(0), 0,
                                                   std::vector<MatchCmd *>{
                                                       new MatchHwCmd<FamilyType, PIPE_CONTROL>(1, Expects{EXPECT_MEMBER(PIPE_CONTROL, getCommandStreamerStallEnable, true), EXPECT_MEMBER(PIPE_CONTROL, getDcFlushEnable, false)}),
                                                       new MatchHwCmd<FamilyType, L3_CONTROL>(1, Expects{EXPECT_MEMBER(L3_CONTROL, getPostSyncOperation, L3_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_NO_WRITE)})},
                                                   &err);
        EXPECT_TRUE(cmdBuffOk) << err;

        this->mockKernelWithInternal->mockProgram->setGlobalSurface(nullptr);
    }
};

template <typename FamilyType>
class GivenCacheFlushAfterWalkerEnabledWhenProgramGlobalSurfacePresentAndPostSyncRequiredThenExpectProperCacheFlushCommand : public HardwareCommandsTest {
  public:
    void testBodyImpl() {
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
        using L3_CONTROL = typename FamilyType::L3_CONTROL;

        DebugManagerStateRestore dbgRestore;
        DebugManager.flags.EnableCacheFlushAfterWalker.set(1);

        CommandQueueHw<FamilyType> cmdQ(nullptr, pClDevice, 0, false);
        auto &commandStream = cmdQ.getCS(1024);

        void *allocPtr = reinterpret_cast<void *>(static_cast<uintptr_t>(6 * MemoryConstants::pageSize));
        MockGraphicsAllocation globalAllocation{allocPtr, MemoryConstants::pageSize * 2};
        this->mockKernelWithInternal->mockProgram->setGlobalSurface(&globalAllocation);

        constexpr uint64_t postSyncAddress = 1024;

        HardwareCommandsHelper<FamilyType>::programCacheFlushAfterWalkerCommand(&commandStream, cmdQ, this->mockKernelWithInternal->mockKernel, postSyncAddress);

        std::string err;
        auto cmdBuffOk = expectCmdBuff<FamilyType>(cmdQ.getCS(0), 0,
                                                   std::vector<MatchCmd *>{
                                                       new MatchHwCmd<FamilyType, PIPE_CONTROL>(1, Expects{EXPECT_MEMBER(PIPE_CONTROL, getCommandStreamerStallEnable, true), EXPECT_MEMBER(PIPE_CONTROL, getDcFlushEnable, false)}),
                                                       new MatchHwCmd<FamilyType, L3_CONTROL>(1, Expects{EXPECT_MEMBER(L3_CONTROL, getPostSyncAddress, postSyncAddress),
                                                                                                         EXPECT_MEMBER(L3_CONTROL, getPostSyncImmediateData, 0),
                                                                                                         EXPECT_MEMBER(L3_CONTROL, getPostSyncOperation, L3_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA)})},
                                                   &err);
        EXPECT_TRUE(cmdBuffOk) << err;

        this->mockKernelWithInternal->mockProgram->setGlobalSurface(nullptr);
    }
};

using EnqueueKernelFixture = HelloWorldFixture<HelloWorldFixtureFactory>;
using EnqueueKernelTest = Test<EnqueueKernelFixture>;
