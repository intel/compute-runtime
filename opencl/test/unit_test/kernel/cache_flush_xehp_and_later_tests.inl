/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/l3_range.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/hello_world_fixture.h"
#include "opencl/test/unit_test/helpers/cmd_buffer_validator.h"
#include "opencl/test/unit_test/helpers/hardware_commands_helper_tests.h"
#include "opencl/test/unit_test/helpers/static_size3.h"
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
    void TestBodyImpl() {
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
    void TestBodyImpl() {
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
class GivenCacheFlushAfterWalkerEnabledWhenKernelArgIsSetAsCacheFlushRequiredThenExpectCacheFlushCommand : public HardwareCommandsTest {
  public:
    void TestBodyImpl() {
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
        using L3_FLUSH_ADDRESS_RANGE = typename FamilyType::L3_FLUSH_ADDRESS_RANGE;
        using L3_CONTROL = typename FamilyType::L3_CONTROL;

        DebugManagerStateRestore dbgRestore;
        DebugManager.flags.EnableCacheFlushAfterWalker.set(1);

        CommandQueueHw<FamilyType> cmdQ(nullptr, pClDevice, 0, false);
        auto &commandStream = cmdQ.getCS(1024);
        addSpaceForSingleKernelArg();
        this->mockKernelWithInternal->mockKernel->kernelArgRequiresCacheFlush.resize(2);
        void *allocPtr = reinterpret_cast<void *>(static_cast<uintptr_t>(6 * MemoryConstants::pageSize));
        MockGraphicsAllocation cacheRequiringAllocation{allocPtr, MemoryConstants::pageSize * 7};
        this->mockKernelWithInternal->mockKernel->kernelArgRequiresCacheFlush[0] = &cacheRequiringAllocation;

        L3RangesVec rangesExpected;
        coverRangeExact(cacheRequiringAllocation.getGpuAddress(), cacheRequiringAllocation.getUnderlyingBufferSize(), rangesExpected, FamilyType::L3_FLUSH_ADDRESS_RANGE::L3_FLUSH_EVICTION_POLICY_FLUSH_L3_WITH_EVICTION);

        HardwareCommandsHelper<FamilyType>::programCacheFlushAfterWalkerCommand(&commandStream, cmdQ, this->mockKernelWithInternal->mockKernel, 0U);

        L3ControlPolicy<FamilyType> validateL3ControlPolicy{L3_FLUSH_ADDRESS_RANGE::L3_FLUSH_EVICTION_POLICY_FLUSH_L3_WITH_EVICTION};

        std::string err;
        auto cmdBuffOk = expectCmdBuff<FamilyType>(cmdQ.getCS(0), 0,
                                                   std::vector<MatchCmd *>{
                                                       new MatchHwCmd<FamilyType, PIPE_CONTROL>(1, Expects{EXPECT_MEMBER(PIPE_CONTROL, getCommandStreamerStallEnable, true), EXPECT_MEMBER(PIPE_CONTROL, getDcFlushEnable, false)}),
                                                       new MatchHwCmd<FamilyType, L3_CONTROL>(1, Expects{EXPECT_MEMBER(L3_CONTROL, getPostSyncOperation, L3_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_NO_WRITE), &validateL3ControlPolicy}),
                                                   },
                                                   &err);
        EXPECT_TRUE(cmdBuffOk) << err;

        EXPECT_EQ(rangesExpected, validateL3ControlPolicy.l3RangesParsed);
    }
};

template <typename FamilyType>
class GivenCacheFlushAfterWalkerEnabledWhenNoGlobalSurfaceSvmAllocationKernelArgRequireCacheFlushThenExpectNoCacheFlushCommand : public HardwareCommandsTest {
  public:
    void TestBodyImpl() {
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
    void TestBodyImpl() {
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
    void TestBodyImpl() {
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

template <typename FamilyType>
class GivenCacheFlushAfterWalkerEnabledWhenAllocationRequiresCacheFlushThenFlushCommandPresentAfterWalker : public EnqueueKernelTest {
  public:
    void TestBodyImpl() {
        using WALKER = typename FamilyType::WALKER_TYPE;
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
        using L3_FLUSH_ADDRESS_RANGE = typename FamilyType::L3_FLUSH_ADDRESS_RANGE;
        using L3_CONTROL = typename FamilyType::L3_CONTROL;

        DebugManagerStateRestore restore;
        DebugManager.flags.EnableCacheFlushAfterWalker.set(1);
        DebugManager.flags.EnableTimestampPacket.set(0);

        MockKernelWithInternals mockKernel(*pClDevice, context, true);
        mockKernel.mockKernel->svmAllocationsRequireCacheFlush = false;

        auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);

        cmdQ->getUltCommandStreamReceiver().timestampPacketWriteEnabled = false;

        auto memoryManager = pDevice->getUltCommandStreamReceiver<FamilyType>().getMemoryManager();
        SVMAllocsManager svmManager(memoryManager, false);
        void *svm = svmManager.createSVMAlloc(MemoryConstants::pageSize * 5, {}, context->getRootDeviceIndices(), context->getDeviceBitfields());
        ASSERT_NE(nullptr, svm);
        auto svmAllocation = svmManager.getSVMAlloc(svm)->gpuAllocations.getGraphicsAllocation(pDevice->getRootDeviceIndex());
        svmAllocation->setFlushL3Required(true);

        mockKernel.kernelInfo.kernelAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties(pDevice->getRootDeviceIndex(), true, MemoryConstants::pageSize, AllocationType::INTERNAL_HEAP, pDevice->getDeviceBitfield()));
        mockKernel.mockKernel->kernelArgRequiresCacheFlush.resize(1);
        mockKernel.mockKernel->setArgSvmAlloc(0, svm, svmAllocation, 0u);

        cmdQ->enqueueKernel(mockKernel, 1, nullptr, StatickSize3<16, 1, 1>(), StatickSize3<16, 1, 1>(), 0, nullptr, nullptr);

        L3RangesVec rangesExpected;
        coverRangeExact(svmAllocation->getGpuAddress(), svmAllocation->getUnderlyingBufferSize(), rangesExpected, FamilyType::L3_FLUSH_ADDRESS_RANGE::L3_FLUSH_EVICTION_POLICY_FLUSH_L3_WITH_EVICTION);

        L3ControlPolicy<FamilyType> validateL3ControlPolicy{L3_FLUSH_ADDRESS_RANGE::L3_FLUSH_EVICTION_POLICY_FLUSH_L3_WITH_EVICTION};

        std::string err;
        auto cmdBuffOk = expectCmdBuff<FamilyType>(cmdQ->getCS(0), 0,
                                                   std::vector<MatchCmd *>{new MatchAnyCmd(AnyNumber),
                                                                           new MatchHwCmd<FamilyType, WALKER>(1),
                                                                           new MatchAnyCmd(AnyNumber),
                                                                           new MatchHwCmd<FamilyType, PIPE_CONTROL>(1, Expects{EXPECT_MEMBER(PIPE_CONTROL, getCommandStreamerStallEnable, true), EXPECT_MEMBER(PIPE_CONTROL, getDcFlushEnable, false)}),
                                                                           new MatchHwCmd<FamilyType, L3_CONTROL>(1, Expects{EXPECT_MEMBER(L3_CONTROL, getPostSyncOperation, L3_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_NO_WRITE), &validateL3ControlPolicy}),
                                                                           new MatchAnyCmd(AnyNumber)},
                                                   &err);
        EXPECT_TRUE(cmdBuffOk) << err;
        EXPECT_EQ(rangesExpected, validateL3ControlPolicy.l3RangesParsed);

        memoryManager->freeGraphicsMemory(mockKernel.kernelInfo.kernelAllocation);
        svmManager.freeSVMAlloc(svm);
    }
};

template <typename FamilyType>
class GivenCacheFlushAfterWalkerAndTimestampPacketsEnabledWhenAllocationRequiresCacheFlushThenFlushCommandPresentAfterWalker : public EnqueueKernelTest {
  public:
    void TestBodyImpl() {
        using WALKER = typename FamilyType::WALKER_TYPE;
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
        using L3_FLUSH_ADDRESS_RANGE = typename FamilyType::L3_FLUSH_ADDRESS_RANGE;
        using L3_CONTROL = typename FamilyType::L3_CONTROL;

        DebugManagerStateRestore restore;
        DebugManager.flags.EnableCacheFlushAfterWalker.set(1);
        DebugManager.flags.EnableTimestampPacket.set(1);

        MockKernelWithInternals mockKernel(*pClDevice, context, true);
        mockKernel.mockKernel->svmAllocationsRequireCacheFlush = false;

        auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);

        auto memoryManager = pDevice->getUltCommandStreamReceiver<FamilyType>().getMemoryManager();
        SVMAllocsManager svmManager(memoryManager, false);
        void *svm = svmManager.createSVMAlloc(MemoryConstants::pageSize * 5, {}, context->getRootDeviceIndices(), context->getDeviceBitfields());
        ASSERT_NE(nullptr, svm);
        auto svmAllocation = svmManager.getSVMAlloc(svm)->gpuAllocations.getGraphicsAllocation(pDevice->getRootDeviceIndex());
        svmAllocation->setFlushL3Required(true);

        mockKernel.kernelInfo.kernelAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties(pDevice->getRootDeviceIndex(), true, MemoryConstants::pageSize, AllocationType::INTERNAL_HEAP, pDevice->getDeviceBitfield()));
        mockKernel.mockKernel->kernelArgRequiresCacheFlush.resize(1);
        mockKernel.mockKernel->setArgSvmAlloc(0, svm, svmAllocation, 0u);

        cmdQ->enqueueKernel(mockKernel, 1, nullptr, StatickSize3<16, 1, 1>(), StatickSize3<16, 1, 1>(), 0, nullptr, nullptr);

        auto &nodes = cmdQ->timestampPacketContainer->peekNodes();
        EXPECT_FALSE(nodes[nodes.size() - 1]->isProfilingCapable());

        L3RangesVec rangesExpected;
        coverRangeExact(svmAllocation->getGpuAddress(), svmAllocation->getUnderlyingBufferSize(), rangesExpected, FamilyType::L3_FLUSH_ADDRESS_RANGE::L3_FLUSH_EVICTION_POLICY_FLUSH_L3_WITH_EVICTION);

        L3ControlPolicy<FamilyType> validateL3ControlPolicy{L3_FLUSH_ADDRESS_RANGE::L3_FLUSH_EVICTION_POLICY_FLUSH_L3_WITH_EVICTION};

        std::string err;
        auto cmdBuffOk = expectCmdBuff<FamilyType>(cmdQ->getCS(0), 0,
                                                   std::vector<MatchCmd *>{new MatchAnyCmd(AnyNumber),
                                                                           new MatchHwCmd<FamilyType, WALKER>(1),
                                                                           new MatchAnyCmd(AnyNumber),
                                                                           new MatchHwCmd<FamilyType, PIPE_CONTROL>(1, Expects{EXPECT_MEMBER(PIPE_CONTROL, getCommandStreamerStallEnable, true), EXPECT_MEMBER(PIPE_CONTROL, getDcFlushEnable, false)}),
                                                                           new MatchAnyCmd(AnyNumber),
                                                                           new MatchHwCmd<FamilyType, L3_CONTROL>(1, Expects{EXPECT_MEMBER(L3_CONTROL, getPostSyncOperation, L3_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA), &validateL3ControlPolicy}),
                                                                           new MatchAnyCmd(AnyNumber)},
                                                   &err);
        EXPECT_TRUE(cmdBuffOk) << err;
        EXPECT_EQ(rangesExpected, validateL3ControlPolicy.l3RangesParsed);

        memoryManager->freeGraphicsMemory(mockKernel.kernelInfo.kernelAllocation);
        svmManager.freeSVMAlloc(svm);
    }
};

template <typename FamilyType>
class GivenCacheFlushAfterWalkerDisabledWhenAllocationRequiresCacheFlushThenFlushCommandNotPresentAfterWalker : public EnqueueKernelTest {
  public:
    void TestBodyImpl() {
        using WALKER = typename FamilyType::WALKER_TYPE;
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
        using L3_FLUSH_ADDRESS_RANGE = typename FamilyType::L3_FLUSH_ADDRESS_RANGE;
        using L3_CONTROL = typename FamilyType::L3_CONTROL;

        DebugManagerStateRestore restore;
        DebugManager.flags.EnableCacheFlushAfterWalker.set(0);

        MockKernelWithInternals mockKernel(*pClDevice, context, true);
        mockKernel.mockKernel->svmAllocationsRequireCacheFlush = false;

        auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);

        auto memoryManager = pDevice->getUltCommandStreamReceiver<FamilyType>().getMemoryManager();
        SVMAllocsManager svmManager(memoryManager, false);
        void *svm = svmManager.createSVMAlloc(MemoryConstants::pageSize * 5, {}, context->getRootDeviceIndices(), context->getDeviceBitfields());
        ASSERT_NE(nullptr, svm);
        auto svmAllocation = svmManager.getSVMAlloc(svm)->gpuAllocations.getGraphicsAllocation(pDevice->getRootDeviceIndex());
        svmAllocation->setFlushL3Required(true);

        mockKernel.kernelInfo.kernelAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties(pDevice->getRootDeviceIndex(), true, MemoryConstants::pageSize, AllocationType::INTERNAL_HEAP, pDevice->getDeviceBitfield()));
        mockKernel.mockKernel->kernelArgRequiresCacheFlush.resize(1);
        mockKernel.mockKernel->setArgSvmAlloc(0, svm, svmAllocation, 0u);

        cmdQ->enqueueKernel(mockKernel, 1, nullptr, StatickSize3<16, 1, 1>(), StatickSize3<16, 1, 1>(), 0, nullptr, nullptr);

        L3ControlPolicy<FamilyType> validateL3ControlPolicy{L3_FLUSH_ADDRESS_RANGE::L3_FLUSH_EVICTION_POLICY_FLUSH_L3_WITH_EVICTION};

        std::string err;
        auto cmdBuffOk = expectCmdBuff<FamilyType>(cmdQ->getCS(0), 0,
                                                   std::vector<MatchCmd *>{
                                                       new MatchAnyCmd(AnyNumber),
                                                       new MatchHwCmd<FamilyType, WALKER>(1),
                                                       new MatchAnyCmd(AnyNumber),
                                                       new MatchHwCmd<FamilyType, L3_CONTROL>(0),
                                                       new MatchAnyCmd(AnyNumber),
                                                   },
                                                   &err);
        EXPECT_TRUE(cmdBuffOk) << err;

        memoryManager->freeGraphicsMemory(mockKernel.kernelInfo.kernelAllocation);
        svmManager.freeSVMAlloc(svm);
    }
};
template <typename FamilyType>
class GivenCacheFlushAfterWalkerEnabledWhenMoreThan126AllocationRangesRequiresCacheFlushThenAtLeatsTwoFlushCommandPresentAfterWalker : public EnqueueKernelTest {
  public:
    void TestBodyImpl() {
        using WALKER = typename FamilyType::WALKER_TYPE;
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
        using L3_FLUSH_ADDRESS_RANGE = typename FamilyType::L3_FLUSH_ADDRESS_RANGE;
        using L3_CONTROL = typename FamilyType::L3_CONTROL;

        DebugManagerStateRestore restore;
        DebugManager.flags.EnableCacheFlushAfterWalker.set(1);
        DebugManager.flags.EnableTimestampPacket.set(0);

        MockKernelWithInternals mockKernel(*pClDevice, context, true);
        mockKernel.mockKernel->svmAllocationsRequireCacheFlush = false;

        auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);

        cmdQ->getUltCommandStreamReceiver().timestampPacketWriteEnabled = false;

        auto memoryManager = pDevice->getUltCommandStreamReceiver<FamilyType>().getMemoryManager();
        SVMAllocsManager svmManager(memoryManager, false);
        mockKernel.mockKernel->kernelArgRequiresCacheFlush.resize(maxFlushSubrangeCount + 1);

        std::vector<void *> svmAllocs;
        for (uint32_t i = 0; i < maxFlushSubrangeCount + 1; i++) {
            void *svm = svmManager.createSVMAlloc(MemoryConstants::pageSize, {}, context->getRootDeviceIndices(), context->getDeviceBitfields());
            auto svmAllocation = svmManager.getSVMAlloc(svm)->gpuAllocations.getGraphicsAllocation(pDevice->getRootDeviceIndex());
            svmAllocation->setFlushL3Required(true);
            mockKernel.mockKernel->addAllocationToCacheFlushVector(i, svmAllocation);
            svmAllocs.push_back(svm);
        }

        cmdQ->enqueueKernel(mockKernel, 1, nullptr, StatickSize3<16, 1, 1>(), StatickSize3<16, 1, 1>(), 0, nullptr, nullptr);

        std::string err;
        auto cmdBuffOk = expectCmdBuff<FamilyType>(cmdQ->getCS(0), 0,
                                                   std::vector<MatchCmd *>{new MatchAnyCmd(AnyNumber),
                                                                           new MatchHwCmd<FamilyType, WALKER>(1),
                                                                           new MatchAnyCmd(AnyNumber),
                                                                           new MatchHwCmd<FamilyType, PIPE_CONTROL>(1, Expects{EXPECT_MEMBER(PIPE_CONTROL, getCommandStreamerStallEnable, true), EXPECT_MEMBER(PIPE_CONTROL, getDcFlushEnable, false)}),
                                                                           new MatchHwCmd<FamilyType, L3_CONTROL>(1),
                                                                           new MatchHwCmd<FamilyType, L3_CONTROL>(AtLeastOne),
                                                                           new MatchAnyCmd(AnyNumber)},
                                                   &err);
        EXPECT_TRUE(cmdBuffOk) << err;
        for (void *svm : svmAllocs) {
            svmManager.freeSVMAlloc(svm);
        }
    }
};

template <typename FamilyType>
class GivenCacheFlushAfterWalkerAndTimestampPacketsEnabledWhenMoreThan126AllocationRangesRequiresCacheFlushThenExpectFlushWithOutPostSyncAndThenWithPostSync : public EnqueueKernelTest {
  public:
    void TestBodyImpl() {
        using WALKER = typename FamilyType::WALKER_TYPE;
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
        using L3_FLUSH_ADDRESS_RANGE = typename FamilyType::L3_FLUSH_ADDRESS_RANGE;
        using L3_CONTROL = typename FamilyType::L3_CONTROL;

        DebugManagerStateRestore restore;
        DebugManager.flags.EnableCacheFlushAfterWalker.set(1);
        DebugManager.flags.EnableTimestampPacket.set(1);

        MockKernelWithInternals mockKernel(*pClDevice, context, true);
        mockKernel.mockKernel->svmAllocationsRequireCacheFlush = false;

        auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);

        cmdQ->getUltCommandStreamReceiver().timestampPacketWriteEnabled = true;

        auto memoryManager = pDevice->getUltCommandStreamReceiver<FamilyType>().getMemoryManager();
        SVMAllocsManager svmManager(memoryManager, false);
        mockKernel.mockKernel->kernelArgRequiresCacheFlush.resize(maxFlushSubrangeCount + 1);

        std::vector<void *> svmAllocs;
        for (uint32_t i = 0; i < maxFlushSubrangeCount + 1; i++) {
            void *svm = svmManager.createSVMAlloc(MemoryConstants::pageSize, {}, context->getRootDeviceIndices(), context->getDeviceBitfields());
            auto svmAllocation = svmManager.getSVMAlloc(svm)->gpuAllocations.getGraphicsAllocation(pDevice->getRootDeviceIndex());
            svmAllocation->setFlushL3Required(true);
            mockKernel.mockKernel->addAllocationToCacheFlushVector(i, svmAllocation);
            svmAllocs.push_back(svm);
        }

        cmdQ->enqueueKernel(mockKernel, 1, nullptr, StatickSize3<16, 1, 1>(), StatickSize3<16, 1, 1>(), 0, nullptr, nullptr);

        auto &nodes = cmdQ->timestampPacketContainer->peekNodes();
        EXPECT_FALSE(nodes[1]->isProfilingCapable());

        auto timestampPacketNode = nodes[1];
        auto timestampPacketGpuAddress = TimestampPacketHelper::getContextEndGpuAddress(*timestampPacketNode);

        std::string err;
        auto cmdBuffOk = expectCmdBuff<FamilyType>(cmdQ->getCS(0), 0,
                                                   std::vector<MatchCmd *>{new MatchAnyCmd(AnyNumber),
                                                                           new MatchHwCmd<FamilyType, WALKER>(1),
                                                                           new MatchAnyCmd(AnyNumber),
                                                                           new MatchHwCmd<FamilyType, PIPE_CONTROL>(1, Expects{EXPECT_MEMBER(PIPE_CONTROL, getCommandStreamerStallEnable, true), EXPECT_MEMBER(PIPE_CONTROL, getDcFlushEnable, false)}),
                                                                           new MatchHwCmd<FamilyType, L3_CONTROL>(1, Expects{EXPECT_MEMBER(L3_CONTROL, getPostSyncOperation, L3_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_NO_WRITE)}),
                                                                           new MatchHwCmd<FamilyType, L3_CONTROL>(1, Expects{EXPECT_MEMBER(L3_CONTROL, getPostSyncOperation, L3_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA)}),
                                                                           new MatchAnyCmd(AnyNumber)},
                                                   &err);

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(cmdQ->getCS(0), 0);

        bool postSyncWriteFound = false;
        for (auto &cmd : hwParser.cmdList) {
            if (auto l3ControlCmd = genCmdCast<L3_CONTROL *>(cmd)) {
                if (l3ControlCmd->getPostSyncOperation() == L3_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
                    EXPECT_EQ(timestampPacketGpuAddress, l3ControlCmd->getPostSyncAddress());
                    postSyncWriteFound = true;
                }
            }
        }
        EXPECT_TRUE(postSyncWriteFound);

        EXPECT_TRUE(cmdBuffOk) << err;
        for (void *svm : svmAllocs) {
            svmManager.freeSVMAlloc(svm);
        }
    }
};

template <typename FamilyType>
class GivenCacheFlushAfterWalkerEnabledWhen126AllocationRangesRequiresCacheFlushThenExpectOneFlush : public EnqueueKernelTest {
  public:
    void TestBodyImpl() {
        using WALKER = typename FamilyType::WALKER_TYPE;
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
        using L3_FLUSH_ADDRESS_RANGE = typename FamilyType::L3_FLUSH_ADDRESS_RANGE;
        using L3_CONTROL = typename FamilyType::L3_CONTROL;

        DebugManagerStateRestore restore;
        DebugManager.flags.EnableCacheFlushAfterWalker.set(1);
        DebugManager.flags.EnableTimestampPacket.set(0);

        MockKernelWithInternals mockKernel(*pClDevice, context, true);
        mockKernel.mockKernel->svmAllocationsRequireCacheFlush = false;

        auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);

        cmdQ->getUltCommandStreamReceiver().timestampPacketWriteEnabled = false;

        auto memoryManager = pDevice->getUltCommandStreamReceiver<FamilyType>().getMemoryManager();
        SVMAllocsManager svmManager(memoryManager, false);
        mockKernel.mockKernel->kernelArgRequiresCacheFlush.resize(maxFlushSubrangeCount);

        std::vector<void *> svmAllocs;
        for (uint32_t i = 0; i < maxFlushSubrangeCount; i++) {
            void *svm = svmManager.createSVMAlloc(MemoryConstants::pageSize, {}, context->getRootDeviceIndices(), context->getDeviceBitfields());
            auto svmAllocation = svmManager.getSVMAlloc(svm)->gpuAllocations.getGraphicsAllocation(pDevice->getRootDeviceIndex());
            svmAllocation->setFlushL3Required(true);
            mockKernel.mockKernel->addAllocationToCacheFlushVector(i, svmAllocation);
            svmAllocs.push_back(svm);
        }

        cmdQ->enqueueKernel(mockKernel, 1, nullptr, StatickSize3<16, 1, 1>(), StatickSize3<16, 1, 1>(), 0, nullptr, nullptr);

        std::string err;
        auto cmdBuffOk = expectCmdBuff<FamilyType>(cmdQ->getCS(0), 0,
                                                   std::vector<MatchCmd *>{new MatchAnyCmd(AnyNumber),
                                                                           new MatchHwCmd<FamilyType, WALKER>(1),
                                                                           new MatchAnyCmd(AnyNumber),
                                                                           new MatchHwCmd<FamilyType, PIPE_CONTROL>(1, Expects{EXPECT_MEMBER(PIPE_CONTROL, getCommandStreamerStallEnable, true), EXPECT_MEMBER(PIPE_CONTROL, getDcFlushEnable, false)}),
                                                                           new MatchHwCmd<FamilyType, L3_CONTROL>(1),
                                                                           new MatchHwCmd<FamilyType, L3_CONTROL>(0),
                                                                           new MatchAnyCmd(AnyNumber)},
                                                   &err);
        EXPECT_TRUE(cmdBuffOk) << err;
        for (void *svm : svmAllocs) {
            svmManager.freeSVMAlloc(svm);
        }
    }
};