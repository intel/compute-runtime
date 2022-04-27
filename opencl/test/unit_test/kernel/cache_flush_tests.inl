/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/l3_range.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/command_queue/gpgpu_walker.h"
#include "opencl/source/command_queue/resource_barrier.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/test/unit_test/fixtures/hello_world_fixture.h"
#include "opencl/test/unit_test/helpers/cmd_buffer_validator.h"
#include "opencl/test/unit_test/helpers/hardware_commands_helper_tests.h"
#include "opencl/test/unit_test/helpers/static_size3.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"

using namespace NEO;

template <typename FamilyType>
struct L3ControlPolicy : CmdValidator {
    L3ControlPolicy(typename FamilyType::L3_FLUSH_ADDRESS_RANGE::L3_FLUSH_EVICTION_POLICY expectedPolicy, bool isA0Stepping)
        : expectedPolicy(expectedPolicy), isA0Stepping(isA0Stepping) {
    }
    bool operator()(GenCmdList::iterator it, size_t numInScetion, const std::string &member, std::string &outReason) override {
        using L3_CONTROL = typename FamilyType::L3_CONTROL;
        auto l3ControlAddress = genCmdCast<L3_CONTROL *>(*it)->getL3FlushAddressRange();
        if (l3ControlAddress.getL3FlushEvictionPolicy(isA0Stepping) != expectedPolicy) {
            outReason = "Invalid L3_FLUSH_EVICTION_POLICY - expected: " + std::to_string(expectedPolicy) + ", got :" + std::to_string(l3ControlAddress.getL3FlushEvictionPolicy(isA0Stepping));
            return false;
        }
        l3RangesParsed.push_back(L3Range::fromAddressMask(l3ControlAddress.getAddress(isA0Stepping), l3ControlAddress.getAddressMask(isA0Stepping)));
        return true;
    }
    L3RangesVec l3RangesParsed;
    typename FamilyType::L3_FLUSH_ADDRESS_RANGE::L3_FLUSH_EVICTION_POLICY expectedPolicy;
    bool isA0Stepping;
};

template <typename FamilyType>
class GivenCacheFlushAfterWalkerEnabledWhenSvmAllocationsSetAsCacheFlushRequiringThenExpectCacheFlushCommand : public HardwareCommandsTest {
  public:
    void TestBodyImpl() {
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
        using L3_CONTROL_WITHOUT_POST_SYNC = typename FamilyType::L3_CONTROL;

        DebugManagerStateRestore dbgRestore;
        DebugManager.flags.EnableCacheFlushAfterWalker.set(1);

        CommandQueueHw<FamilyType> cmdQ(nullptr, pClDevice, 0, false);
        auto &commandStream = cmdQ.getCS(1024);

        void *allocPtr = reinterpret_cast<void *>(static_cast<uintptr_t>(6 * MemoryConstants::pageSize));
        MockGraphicsAllocation svmAllocation{allocPtr, MemoryConstants::pageSize * 2};
        svmAllocation.setFlushL3Required(true);
        this->mockKernelWithInternal->mockKernel->kernelSvmGfxAllocations.push_back(&svmAllocation);
        this->mockKernelWithInternal->mockKernel->svmAllocationsRequireCacheFlush = true;

        size_t expectedSize = sizeof(PIPE_CONTROL) + sizeof(L3_CONTROL_WITHOUT_POST_SYNC);
        size_t actualSize = HardwareCommandsHelper<FamilyType>::getSizeRequiredForCacheFlush(cmdQ, this->mockKernelWithInternal->mockKernel, 0U);
        EXPECT_EQ(expectedSize, actualSize);

        HardwareCommandsHelper<FamilyType>::programCacheFlushAfterWalkerCommand(&commandStream, cmdQ, this->mockKernelWithInternal->mockKernel, 0U);

        std::string err;
        auto cmdBuffOk = expectCmdBuff<FamilyType>(cmdQ.getCS(0), 0,
                                                   std::vector<MatchCmd *>({
                                                       new MatchHwCmd<FamilyType, PIPE_CONTROL>(1, Expects{EXPECT_MEMBER(PIPE_CONTROL, getCommandStreamerStallEnable, true), EXPECT_MEMBER(PIPE_CONTROL, getDcFlushEnable, false)}),
                                                       new MatchHwCmd<FamilyType, L3_CONTROL_WITHOUT_POST_SYNC>(AtLeastOne),
                                                   }),
                                                   &err);
        EXPECT_TRUE(cmdBuffOk) << err;
    }
};

template <typename FamilyType>
class GivenCacheFlushAfterWalkerEnabledAndProperSteppingIsSetWhenKernelArgIsSetAsCacheFlushRequiredAndA0SteppingIsDisabledThenExpectCacheFlushCommand : public HardwareCommandsTest {
  public:
    void TestBodyImpl(bool isA0Stepping) {
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
        using L3_FLUSH_ADDRESS_RANGE = typename FamilyType::L3_FLUSH_ADDRESS_RANGE;
        using L3_CONTROL_WITHOUT_POST_SYNC = typename FamilyType::L3_CONTROL;

        DebugManagerStateRestore dbgRestore;
        DebugManager.flags.EnableCacheFlushAfterWalker.set(1);
        const auto &hwInfoConfig = *HwInfoConfig::get(hardwareInfo.platform.eProductFamily);
        auto stepping = (isA0Stepping ? REVISION_A0 : REVISION_A1);
        hardwareInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(stepping, hardwareInfo);
        pDevice->executionEnvironment->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->setHwInfo(&hardwareInfo);

        CommandQueueHw<FamilyType> cmdQ(nullptr, pClDevice, 0, false);
        auto &commandStream = cmdQ.getCS(1024);
        addSpaceForSingleKernelArg();
        this->mockKernelWithInternal->mockKernel->kernelArgRequiresCacheFlush.resize(2);
        void *allocPtr = reinterpret_cast<void *>(static_cast<uintptr_t>(6 * MemoryConstants::pageSize));
        MockGraphicsAllocation cacheRequiringAllocation{allocPtr, MemoryConstants::pageSize * 7};
        this->mockKernelWithInternal->mockKernel->kernelArgRequiresCacheFlush[0] = &cacheRequiringAllocation;
        L3RangesVec rangesExpected;
        coverRangeExact(cacheRequiringAllocation.getGpuAddress(), cacheRequiringAllocation.getUnderlyingBufferSize(), rangesExpected, FamilyType::L3_FLUSH_ADDRESS_RANGE::L3_FLUSH_EVICTION_POLICY_FLUSH_L3_WITH_EVICTION);

        size_t expectedSize = sizeof(PIPE_CONTROL) + rangesExpected.size() * sizeof(L3_CONTROL_WITHOUT_POST_SYNC);
        size_t actualSize = HardwareCommandsHelper<FamilyType>::getSizeRequiredForCacheFlush(cmdQ, this->mockKernelWithInternal->mockKernel, 0U);
        EXPECT_EQ(expectedSize, actualSize);

        HardwareCommandsHelper<FamilyType>::programCacheFlushAfterWalkerCommand(&commandStream, cmdQ, this->mockKernelWithInternal->mockKernel, 0U);

        L3ControlPolicy<FamilyType> validateL3ControlPolicy{L3_FLUSH_ADDRESS_RANGE::L3_FLUSH_EVICTION_POLICY_FLUSH_L3_WITH_EVICTION, isA0Stepping};

        std::string err;
        auto cmdBuffOk = expectCmdBuff<FamilyType>(cmdQ.getCS(0), 0,
                                                   std::vector<MatchCmd *>{
                                                       new MatchHwCmd<FamilyType, PIPE_CONTROL>(1, Expects{EXPECT_MEMBER(PIPE_CONTROL, getCommandStreamerStallEnable, true), EXPECT_MEMBER(PIPE_CONTROL, getDcFlushEnable, false)}),
                                                       new MatchHwCmd<FamilyType, L3_CONTROL_WITHOUT_POST_SYNC>(AtLeastOne, {&validateL3ControlPolicy}),
                                                   },
                                                   &err);
        EXPECT_TRUE(cmdBuffOk) << err;

        EXPECT_EQ(rangesExpected, validateL3ControlPolicy.l3RangesParsed);
    }
};

template <typename FamilyType>
class GivenCacheFlushAfterWalkerEnabledWhenProgramGlobalSurfacePresentThenExpectCacheFlushCommand : public HardwareCommandsTest {
  public:
    void TestBodyImpl() {
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
        using L3_CONTROL_WITHOUT_POST_SYNC = typename FamilyType::L3_CONTROL;
        DebugManagerStateRestore dbgRestore;
        DebugManager.flags.EnableCacheFlushAfterWalker.set(1);

        CommandQueueHw<FamilyType> cmdQ(nullptr, pClDevice, 0, false);
        auto &commandStream = cmdQ.getCS(1024);

        void *allocPtr = reinterpret_cast<void *>(static_cast<uintptr_t>(6 * MemoryConstants::pageSize));
        MockGraphicsAllocation globalAllocation{allocPtr, MemoryConstants::pageSize * 2};
        this->mockKernelWithInternal->mockProgram->setGlobalSurface(&globalAllocation);

        size_t expectedSize = sizeof(PIPE_CONTROL) + sizeof(L3_CONTROL_WITHOUT_POST_SYNC);
        size_t actualSize = HardwareCommandsHelper<FamilyType>::getSizeRequiredForCacheFlush(cmdQ, this->mockKernelWithInternal->mockKernel, 0U);
        EXPECT_EQ(expectedSize, actualSize);

        HardwareCommandsHelper<FamilyType>::programCacheFlushAfterWalkerCommand(&commandStream, cmdQ, this->mockKernelWithInternal->mockKernel, 0U);

        std::string err;
        auto cmdBuffOk = expectCmdBuff<FamilyType>(cmdQ.getCS(0), 0,
                                                   std::vector<MatchCmd *>{
                                                       new MatchHwCmd<FamilyType, PIPE_CONTROL>(1, Expects{EXPECT_MEMBER(PIPE_CONTROL, getCommandStreamerStallEnable, true), EXPECT_MEMBER(PIPE_CONTROL, getDcFlushEnable, false)}),
                                                       new MatchHwCmd<FamilyType, L3_CONTROL_WITHOUT_POST_SYNC>(AtLeastOne)},
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
        using L3_CONTROL_WITH_POST_SYNC = typename FamilyType::L3_CONTROL;

        DebugManagerStateRestore dbgRestore;
        DebugManager.flags.EnableCacheFlushAfterWalker.set(1);

        CommandQueueHw<FamilyType> cmdQ(nullptr, pClDevice, 0, false);
        auto &commandStream = cmdQ.getCS(1024);

        void *allocPtr = reinterpret_cast<void *>(static_cast<uintptr_t>(6 * MemoryConstants::pageSize));
        MockGraphicsAllocation globalAllocation{allocPtr, MemoryConstants::pageSize * 2};
        this->mockKernelWithInternal->mockProgram->setGlobalSurface(&globalAllocation);

        constexpr uint64_t postSyncAddress = 1024;
        size_t expectedSize = sizeof(PIPE_CONTROL) + sizeof(L3_CONTROL_WITH_POST_SYNC);
        size_t actualSize = HardwareCommandsHelper<FamilyType>::getSizeRequiredForCacheFlush(cmdQ, this->mockKernelWithInternal->mockKernel, postSyncAddress);
        EXPECT_EQ(expectedSize, actualSize);

        HardwareCommandsHelper<FamilyType>::programCacheFlushAfterWalkerCommand(&commandStream, cmdQ, this->mockKernelWithInternal->mockKernel, postSyncAddress);

        std::string err;
        auto cmdBuffOk = expectCmdBuff<FamilyType>(cmdQ.getCS(0), 0,
                                                   std::vector<MatchCmd *>{
                                                       new MatchHwCmd<FamilyType, PIPE_CONTROL>(1, Expects{EXPECT_MEMBER(PIPE_CONTROL, getCommandStreamerStallEnable, true), EXPECT_MEMBER(PIPE_CONTROL, getDcFlushEnable, false)}),
                                                       new MatchHwCmd<FamilyType, L3_CONTROL_WITH_POST_SYNC>(1, Expects{EXPECT_MEMBER(L3_CONTROL_WITH_POST_SYNC, getPostSyncAddress, postSyncAddress),
                                                                                                                        EXPECT_MEMBER(L3_CONTROL_WITH_POST_SYNC, getPostSyncImmediateData, 0)})},
                                                   &err);
        EXPECT_TRUE(cmdBuffOk) << err;

        this->mockKernelWithInternal->mockProgram->setGlobalSurface(nullptr);
    }
};

using EnqueueKernelFixture = HelloWorldFixture<HelloWorldFixtureFactory>;
using EnqueueKernelTest = Test<EnqueueKernelFixture>;

template <typename FamilyType>
class GivenCacheFlushAfterWalkerEnabledAndProperSteppingIsSetWhenAllocationRequiresCacheFlushThenFlushCommandPresentAfterWalker : public EnqueueKernelTest {
  public:
    void TestBodyImpl(bool isA0Stepping) {
        using WALKER = typename FamilyType::WALKER_TYPE;
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
        using L3_FLUSH_ADDRESS_RANGE = typename FamilyType::L3_FLUSH_ADDRESS_RANGE;
        using L3_CONTROL_WITHOUT_POST_SYNC = typename FamilyType::L3_CONTROL;

        DebugManagerStateRestore restore;
        DebugManager.flags.EnableCacheFlushAfterWalker.set(1);
        DebugManager.flags.EnableTimestampPacket.set(0);
        const auto &hwInfoConfig = *HwInfoConfig::get(hardwareInfo.platform.eProductFamily);
        auto stepping = (isA0Stepping ? REVISION_A0 : REVISION_A1);
        hardwareInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(stepping, hardwareInfo);
        pDevice->executionEnvironment->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->setHwInfo(&hardwareInfo);

        MockKernelWithInternals mockKernel(*pClDevice, context, true);
        mockKernel.mockKernel->svmAllocationsRequireCacheFlush = false;

        auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);

        cmdQ->getUltCommandStreamReceiver().timestampPacketWriteEnabled = false;

        auto memoryManager = pDevice->getUltCommandStreamReceiver<FamilyType>().getMemoryManager();
        SVMAllocsManager svmManager(memoryManager, false);
        void *svm = svmManager.createSVMAlloc(MemoryConstants::pageSize * 5, {}, context->getRootDeviceIndices(), context->getDeviceBitfields());
        ASSERT_NE(nullptr, svm);
        auto svmData = svmManager.getSVMAlloc(svm);
        ASSERT_NE(nullptr, svmData);
        auto svmAllocation = svmData->gpuAllocations.getGraphicsAllocation(pDevice->getRootDeviceIndex());
        ASSERT_NE(nullptr, svmAllocation);
        svmAllocation->setFlushL3Required(true);

        mockKernel.kernelInfo.kernelAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties(pDevice->getRootDeviceIndex(), true, MemoryConstants::pageSize, AllocationType::INTERNAL_HEAP));
        mockKernel.mockKernel->kernelArgRequiresCacheFlush.resize(1);
        mockKernel.mockKernel->setArgSvmAlloc(0, svm, svmAllocation, 0u);

        cmdQ->getUltCommandStreamReceiver().timestampPacketWriteEnabled = false;

        cmdQ->enqueueKernel(mockKernel, 1, nullptr, StatickSize3<16, 1, 1>(), StatickSize3<16, 1, 1>(), 0, nullptr, nullptr);

        L3RangesVec rangesExpected;
        coverRangeExact(svmAllocation->getGpuAddress(), svmAllocation->getUnderlyingBufferSize(), rangesExpected, FamilyType::L3_FLUSH_ADDRESS_RANGE::L3_FLUSH_EVICTION_POLICY_FLUSH_L3_WITH_EVICTION);

        L3ControlPolicy<FamilyType> validateL3ControlPolicy{L3_FLUSH_ADDRESS_RANGE::L3_FLUSH_EVICTION_POLICY_FLUSH_L3_WITH_EVICTION, isA0Stepping};

        std::string err;
        auto cmdBuffOk = expectCmdBuff<FamilyType>(cmdQ->getCS(0), 0,
                                                   std::vector<MatchCmd *>{new MatchAnyCmd(AnyNumber),
                                                                           new MatchHwCmd<FamilyType, WALKER>(1),
                                                                           new MatchAnyCmd(AnyNumber),
                                                                           new MatchHwCmd<FamilyType, PIPE_CONTROL>(1, Expects{EXPECT_MEMBER(PIPE_CONTROL, getCommandStreamerStallEnable, true), EXPECT_MEMBER(PIPE_CONTROL, getDcFlushEnable, false)}),
                                                                           new MatchHwCmd<FamilyType, L3_CONTROL_WITHOUT_POST_SYNC>(AtLeastOne, Expects{&validateL3ControlPolicy}),
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
        using L3_CONTROL_WITH_POST_SYNC = typename FamilyType::L3_CONTROL;

        DebugManagerStateRestore restore;
        DebugManager.flags.EnableCacheFlushAfterWalker.set(1);
        DebugManager.flags.EnableTimestampPacket.set(1);

        MockKernelWithInternals mockKernel(*pDevice, context, true);
        mockKernel.mockKernel->svmAllocationsRequireCacheFlush = false;

        auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);

        auto memoryManager = pDevice->getUltCommandStreamReceiver<FamilyType>().getMemoryManager();
        SVMAllocsManager svmManager(memoryManager, false);
        void *svm = svmManager.createSVMAlloc(MemoryConstants::pageSize * 5, {}, context->getRootDeviceIndices(), context->getDeviceBitfields());
        ASSERT_NE(nullptr, svm);
        auto svmAllocation = svmManager.getSVMAlloc(svm)->gpuAllocations.getGraphicsAllocation(pDevice->getRootDeviceIndex());
        svmAllocation->setFlushL3Required(true);

        mockKernel.kernelInfo.kernelAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties(pDevice->getRootDeviceIndex(), true, MemoryConstants::pageSize, AllocationType::INTERNAL_HEAP));
        mockKernel.mockKernel->kernelArgRequiresCacheFlush.resize(1);
        mockKernel.mockKernel->setArgSvmAlloc(0, svm, svmAllocation, 0);

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
                                                                           new MatchAnyCmd(AnyNumber),
                                                                           new MatchHwCmd<FamilyType, L3_CONTROL_WITH_POST_SYNC>(AtLeastOne, Expects{&validateL3ControlPolicy}),
                                                                           new MatchAnyCmd(AnyNumber)},
                                                   &err);
        EXPECT_TRUE(cmdBuffOk) << err;
        auto expectedRangeWithPostSync = rangesExpected[rangesExpected.size() - 1];
        auto l3ParsedRangeWithPostSync = validateL3ControlPolicy.l3RangesParsed[validateL3ControlPolicy.l3RangesParsed.size() - 1];
        EXPECT_EQ(expectedRangeWithPostSync, l3ParsedRangeWithPostSync);

        memoryManager->freeGraphicsMemory(mockKernel.kernelInfo.kernelAllocation);
        svmManager.freeSVMAlloc(svm);
    }
};

template <typename FamilyType>
class GivenCacheFlushAfterWalkerDisabledAndProperSteppingIsSetWhenAllocationRequiresCacheFlushThenFlushCommandNotPresentAfterWalker : public EnqueueKernelTest {
  public:
    void TestBodyImpl(bool isA0Stepping) {
        using WALKER = typename FamilyType::WALKER_TYPE;
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
        using L3_FLUSH_ADDRESS_RANGE = typename FamilyType::L3_FLUSH_ADDRESS_RANGE;
        using L3_CONTROL_BASE = typename FamilyType::L3_CONTROL_BASE;

        DebugManagerStateRestore restore;
        DebugManager.flags.EnableCacheFlushAfterWalker.set(0);

        MockKernelWithInternals mockKernel(*pClDevice, context, true);
        mockKernel.mockKernel->svmAllocationsRequireCacheFlush = false;

        auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);

        auto memoryManager = pDevice->getUltCommandStreamReceiver<FamilyType>().getMemoryManager();
        SVMAllocsManager svmManager(memoryManager, false);
        void *svm = svmManager.createSVMAlloc(MemoryConstants::pageSize * 5, {}, context->getRootDeviceIndices(), context->getDeviceBitfields());
        ASSERT_NE(nullptr, svm);
        auto svmData = svmManager.getSVMAlloc(svm);
        ASSERT_NE(nullptr, svmData);
        auto svmAllocation = svmData->gpuAllocations.getGraphicsAllocation(pDevice->getRootDeviceIndex());
        ASSERT_NE(nullptr, svmAllocation);
        svmAllocation->setFlushL3Required(true);

        mockKernel.kernelInfo.kernelAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties(pDevice->getRootDeviceIndex(), true, MemoryConstants::pageSize, AllocationType::INTERNAL_HEAP));
        mockKernel.mockKernel->kernelArgRequiresCacheFlush.resize(1);
        mockKernel.mockKernel->setArgSvmAlloc(0, svm, svmAllocation, 0u);

        cmdQ->enqueueKernel(mockKernel, 1, nullptr, StatickSize3<16, 1, 1>(), StatickSize3<16, 1, 1>(), 0, nullptr, nullptr);

        L3ControlPolicy<FamilyType> validateL3ControlPolicy{L3_FLUSH_ADDRESS_RANGE::L3_FLUSH_EVICTION_POLICY_FLUSH_L3_WITH_EVICTION, isA0Stepping};

        std::string err;
        auto cmdBuffOk = expectCmdBuff<FamilyType>(cmdQ->getCS(0), 0,
                                                   std::vector<MatchCmd *>{
                                                       new MatchAnyCmd(AnyNumber),
                                                       new MatchHwCmd<FamilyType, WALKER>(1),
                                                       new MatchAnyCmd(AnyNumber),
                                                       new MatchHwCmd<FamilyType, L3_CONTROL_BASE>(0),
                                                       new MatchAnyCmd(AnyNumber),
                                                   },
                                                   &err);
        EXPECT_TRUE(cmdBuffOk) << err;

        memoryManager->freeGraphicsMemory(mockKernel.kernelInfo.kernelAllocation);
        svmManager.freeSVMAlloc(svm);
    }
};

template <typename FamilyType>
class GivenCacheResourceSurfacesWhenprocessingCacheFlushThenExpectProperCacheFlushCommand : public EnqueueKernelTest {
  public:
    void TestBodyImpl() {

        using L3_CONTROL_WITHOUT_POST_SYNC = typename FamilyType::L3_CONTROL;

        MockCommandQueueHw<FamilyType> cmdQ(context, pClDevice, 0);
        auto &commandStream = cmdQ.getCS(1024);

        cl_resource_barrier_descriptor_intel descriptor{};
        cl_resource_barrier_descriptor_intel descriptor2{};

        SVMAllocsManager *svmManager = cmdQ.getContext().getSVMAllocsManager();
        void *svm = svmManager->createSVMAlloc(MemoryConstants::pageSize, {}, context->getRootDeviceIndices(), context->getDeviceBitfields());

        auto retVal = CL_INVALID_VALUE;
        size_t bufferSize = MemoryConstants::pageSize;
        std::unique_ptr<Buffer> buffer(Buffer::create(
            context,
            CL_MEM_READ_WRITE,
            bufferSize,
            nullptr,
            retVal));

        descriptor.svm_allocation_pointer = svm;

        descriptor2.mem_object = buffer.get();

        const cl_resource_barrier_descriptor_intel descriptors[] = {descriptor, descriptor2};
        BarrierCommand bCmd(&cmdQ, descriptors, 2);
        CsrDependencies csrDeps;

        cmdQ.processDispatchForCacheFlush(bCmd.surfacePtrs.begin(), bCmd.numSurfaces, &commandStream, csrDeps);

        std::string err;
        auto cmdBuffOk = expectCmdBuff<FamilyType>(cmdQ.getCS(0), 0,
                                                   std::vector<MatchCmd *>{
                                                       new MatchHwCmd<FamilyType, L3_CONTROL_WITHOUT_POST_SYNC>(AtLeastOne)},
                                                   &err);
        EXPECT_TRUE(cmdBuffOk) << err;
        svmManager->freeSVMAlloc(svm);
    }
};
