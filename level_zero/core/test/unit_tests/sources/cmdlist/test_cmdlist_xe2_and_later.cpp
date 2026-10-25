/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_cmdlist_execution_internal_options.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/in_order_cmd_list_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"

namespace L0 {
template <GFXCORE_FAMILY gfxCoreFamily>
struct CommandListCoreFamily;

namespace ult {
template <typename Type>
struct WhiteBox;

HWTEST_EXCLUDE_PRODUCT(AppendMemoryCopyTests, givenCopyCommandListWhenTimestampPassedToMemoryCopyRegionBlitThenTimeStampRegistersAreAdded, IGFX_XE2_HPG_CORE);
HWTEST_EXCLUDE_PRODUCT(AppendMemoryCopyTests, givenCopyCommandListWhenTimestampPassedToMemoryCopyThenAppendProfilingCalledOnceBeforeAndAfterCommand, IGFX_XE2_HPG_CORE);

using Platforms = IsAtLeastXe2HpgCore;

struct CommandListXe2AndLaterFixture : public DeviceFixture {
    void setUp() {
        DeviceFixture::setUp();

        constexpr ze_event_pool_desc_t eventPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr, ZE_EVENT_POOL_FLAG_HOST_VISIBLE, 4};
        auto hDevice = device->toHandle();

        ze_result_t result = ZE_RESULT_SUCCESS;
        eventPool.reset(static_cast<EventPool *>(EventPool::create(device->getDriverHandle(), context, 1, &hDevice, &eventPoolDesc, result)));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        ze_event_handle_t hEvent = 0;
        ze_event_desc_t eventDesc = {};

        eventDesc.index = 0;

        result = eventPool->createEvent(&eventDesc, &hEvent);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        eventObj.reset(L0::Event::fromHandle(hEvent));
    }

    void tearDown() {
        eventObj.reset(nullptr);
        eventPool.reset(nullptr);
        DeviceFixture::tearDown();
    }

    template <typename FamilyType>
    std::vector<GenCmdList::iterator> findAllSrmCommands(void *streamStart, size_t size) {
        genSrmCommands.clear();

        EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(genSrmCommands, streamStart, size));

        return findAll<typename FamilyType::MI_STORE_REGISTER_MEM *>(genSrmCommands.begin(), genSrmCommands.end());
    }

    template <typename FamilyType>
    std::vector<GenCmdList::iterator> findAllLrrCommands(void *streamStart, size_t size) {
        genLrrCommands.clear();

        EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(genLrrCommands, streamStart, size));

        return findAll<typename FamilyType::MI_LOAD_REGISTER_REG *>(genLrrCommands.begin(), genLrrCommands.end());
    }

    template <typename FamilyType>
    void validateSrmCommand(const typename FamilyType::MI_STORE_REGISTER_MEM *cmd, uint64_t expectedAddress, uint32_t expectedRegisterOffset) {
        EXPECT_EQ(expectedRegisterOffset, cmd->getRegisterAddress());
        EXPECT_EQ(expectedAddress, cmd->getMemoryAddress());
    }

    template <typename FamilyType>
    void validateLrrCommand(const typename FamilyType::MI_LOAD_REGISTER_REG *cmd, uint32_t expectedRegisterOffset) {
        EXPECT_EQ(expectedRegisterOffset, cmd->getSourceRegisterAddress());
    }

    template <typename FamilyType>
    void validateCommands(const std::vector<GenCmdList::iterator> &srmCommands, bool beforeWalker, bool useMask) {
        using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
        using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;

        auto baseAddr = eventObj->getGpuAddress(device);
        auto contextOffset = beforeWalker ? eventObj->getContextStartOffset() : eventObj->getContextEndOffset();
        auto globalOffset = beforeWalker ? eventObj->getGlobalStartOffset() : eventObj->getGlobalEndOffset();

        uint64_t globalAddress = ptrOffset(baseAddr, globalOffset);
        uint64_t contextAddress = ptrOffset(baseAddr, contextOffset);

        if (useMask) {
            ASSERT_EQ(6u, srmCommands.size());

            validateSrmCommand<FamilyType>(reinterpret_cast<MI_STORE_REGISTER_MEM *>(*srmCommands[0]), globalAddress, RegisterOffsets::csGprR13);
            validateSrmCommand<FamilyType>(reinterpret_cast<MI_STORE_REGISTER_MEM *>(*srmCommands[1]), globalAddress + sizeof(uint32_t), RegisterOffsets::globalTimestampUn);
            validateSrmCommand<FamilyType>(reinterpret_cast<MI_STORE_REGISTER_MEM *>(*srmCommands[2]), contextAddress, RegisterOffsets::csGprR13);
            validateSrmCommand<FamilyType>(reinterpret_cast<MI_STORE_REGISTER_MEM *>(*srmCommands[3]), contextAddress + sizeof(uint32_t), ContextTimestampRegister<FamilyType>::getRegisterOffsetHigh());

            validateLrrCommand<FamilyType>(reinterpret_cast<MI_LOAD_REGISTER_REG *>(*srmCommands[4]), RegisterOffsets::globalTimestampLdw);
            validateLrrCommand<FamilyType>(reinterpret_cast<MI_LOAD_REGISTER_REG *>(*srmCommands[5]), ContextTimestampRegister<FamilyType>::getRegisterOffsetLow());

        } else {
            ASSERT_EQ(4u, srmCommands.size());

            validateSrmCommand<FamilyType>(reinterpret_cast<MI_STORE_REGISTER_MEM *>(*srmCommands[0]), globalAddress, RegisterOffsets::globalTimestampLdw);
            validateSrmCommand<FamilyType>(reinterpret_cast<MI_STORE_REGISTER_MEM *>(*srmCommands[1]), globalAddress + sizeof(uint32_t), RegisterOffsets::globalTimestampUn);
            validateSrmCommand<FamilyType>(reinterpret_cast<MI_STORE_REGISTER_MEM *>(*srmCommands[2]), contextAddress, ContextTimestampRegister<FamilyType>::getRegisterOffsetLow());
            validateSrmCommand<FamilyType>(reinterpret_cast<MI_STORE_REGISTER_MEM *>(*srmCommands[3]), contextAddress + sizeof(uint32_t), ContextTimestampRegister<FamilyType>::getRegisterOffsetHigh());
        }
    }

    DebugManagerStateRestore restore;
    std::unique_ptr<EventPool> eventPool;
    std::unique_ptr<L0::Event> eventObj;
    GenCmdList genSrmCommands;
    GenCmdList genLrrCommands;
};

using CommandListXe2AndLaterTests = Test<CommandListXe2AndLaterFixture>;

HWTEST2_F(CommandListXe2AndLaterTests, given64bEventWhenTimestampIsWrittenThenAddExtraMmioReads, Platforms) {
    auto commandList = std::make_unique<WhiteBox<L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);

    NEO::LinearStream *commandStream = commandList->getCmdContainer().getCommandStream();

    commandList->appendWriteKernelTimestamp(eventObj.get(), nullptr, false, false, false, false);

    size_t streamOffset = commandStream->getUsed();

    {
        auto srmCommands = findAllSrmCommands<FamilyType>(commandStream->getCpuBase(), commandStream->getUsed());

        validateCommands<FamilyType>(srmCommands, false, false);
    }

    commandList->appendWriteKernelTimestamp(eventObj.get(), nullptr, true, false, false, false);

    {
        auto srmCommands = findAllSrmCommands<FamilyType>(ptrOffset(commandStream->getCpuBase(), streamOffset),
                                                          (commandStream->getUsed() - streamOffset));
        validateCommands<FamilyType>(srmCommands, true, false);
    }
}

HWTEST2_F(CommandListXe2AndLaterTests, given64bEventWithLsbMaskingWhenTimestampIsWrittenThenAddExtraMmioReads, Platforms) {
    auto commandList = std::make_unique<WhiteBox<L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);

    NEO::LinearStream *commandStream = commandList->getCmdContainer().getCommandStream();

    commandList->appendWriteKernelTimestamp(eventObj.get(), nullptr, false, true, false, false);

    size_t streamOffset = commandStream->getUsed();

    {
        auto commands = findAllSrmCommands<FamilyType>(commandStream->getCpuBase(), commandStream->getUsed());
        auto lrrCommands = findAllLrrCommands<FamilyType>(commandStream->getCpuBase(), commandStream->getUsed());
        commands.insert(commands.end(), lrrCommands.begin(), lrrCommands.end());

        validateCommands<FamilyType>(commands, false, true);
    }

    commandList->appendWriteKernelTimestamp(eventObj.get(), nullptr, true, true, false, false);

    {
        auto commands = findAllSrmCommands<FamilyType>(ptrOffset(commandStream->getCpuBase(), streamOffset),
                                                       (commandStream->getUsed() - streamOffset));
        auto lrrCommands = findAllLrrCommands<FamilyType>(ptrOffset(commandStream->getCpuBase(), streamOffset),
                                                          (commandStream->getUsed() - streamOffset));
        commands.insert(commands.end(), lrrCommands.begin(), lrrCommands.end());

        validateCommands<FamilyType>(commands, true, true);
    }
}

using CommandListAppendRangesBarrierXe2AndLater = Test<DeviceFixture>;

HWTEST2_F(CommandListAppendRangesBarrierXe2AndLater, givenCallToAppendRangesBarrierThenPipeControlProgrammed, Platforms) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    uint64_t gpuAddress = 0x1200;
    void *buffer = reinterpret_cast<void *>(gpuAddress);
    size_t size = 0x1100;

    NEO::MockGraphicsAllocation mockAllocation(buffer, gpuAddress, size);
    NEO::SvmAllocationData allocData(0);
    allocData.size = size;
    allocData.gpuAllocations.addAllocation(&mockAllocation);
    device->getDriverHandle()->getSvmAllocsManager()->insertSVMAlloc(allocData);
    const void *ranges[] = {buffer};
    const size_t sizes[] = {size};
    commandList->applyMemoryRangesBarrier(1, sizes, ranges);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));
    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto pipeControlCmd = reinterpret_cast<PIPE_CONTROL *>(*itor);
    EXPECT_TRUE(pipeControlCmd->getDataportFlush());
    EXPECT_TRUE(pipeControlCmd->getUnTypedDataPortCacheFlush());
    auto expectedDcFlushEnable = NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getNEODevice()->getRootDeviceEnvironment());
    EXPECT_EQ(expectedDcFlushEnable, pipeControlCmd->getDcFlushEnable());
}

using CommandListXe2AndLaterPreemptionTest = Test<ModuleMutableCommandListFixture>;
HWTEST2_F(CommandListXe2AndLaterPreemptionTest, givenAppendLaunchKernelWhenKernelFlagRequiresDisablePreemptionThenExpectInterfaceDescriptorDataDisablePreemption, Platforms) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    auto &container = commandList->getCmdContainer();
    auto &cmdListStream = *container.getCommandStream();

    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.requiresDisabledMidThreadPreemption = true;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    size_t sizeBefore = cmdListStream.getUsed();
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t sizeAfter = cmdListStream.getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdListStream.getCpuBase(), sizeBefore),
        sizeAfter - sizeBefore));

    auto walkerCmds = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerCmds);
    auto walker = genCmdCast<WalkerType *>(*walkerCmds);
    auto &idd = walker->getInterfaceDescriptor();
    EXPECT_FALSE(idd.getThreadPreemption());
}

HWTEST2_F(CommandListXe2AndLaterPreemptionTest,
          givenObtainKernelPreemptionModeWhenInDeviceFrocePreemptionModeDifferentThanMidThreadThenThreadGroupPreemptionModeIsReturned,
          Platforms) {

    neoDevice->preemptionMode = NEO::PreemptionMode::Disabled;
    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.requiresDisabledMidThreadPreemption = false;
    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.hasRTCalls = false;

    auto commandListCore = std::make_unique<WhiteBox<L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandListCore->initialize(device, NEO::EngineGroupType::compute, 0u);

    auto result = commandListCore->obtainKernelPreemptionMode(kernel.get());
    EXPECT_EQ(NEO::PreemptionMode::ThreadGroup, result);
}

HWTEST2_F(CommandListXe2AndLaterPreemptionTest,
          givenObtainKernelPreemptionModeWhenInDeviceFrocePreemptionModeMidThreadAndOtherKernelFlagsNotSetThenMidThreadPreemptionModeIsReturned,
          Platforms) {

    neoDevice->preemptionMode = NEO::PreemptionMode::MidThread;
    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.requiresDisabledMidThreadPreemption = false;
    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.hasRTCalls = false;

    auto commandListCore = std::make_unique<WhiteBox<L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandListCore->initialize(device, NEO::EngineGroupType::compute, 0u);

    auto result = commandListCore->obtainKernelPreemptionMode(kernel.get());
    EXPECT_EQ(NEO::PreemptionMode::MidThread, result);
}

HWTEST2_F(CommandListXe2AndLaterPreemptionTest, givenMidThreadPreemptionDeviceWhenAppendingKernelsThenMidThreadPreemptionKernelIsTrackedUntilReset, Platforms) {
    neoDevice->preemptionMode = NEO::PreemptionMode::MidThread;
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.requiresDisabledMidThreadPreemption = true;
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams));
    EXPECT_FALSE(commandList->hasMidThreadPreemptionKernel());

    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.requiresDisabledMidThreadPreemption = false;
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams));
    EXPECT_TRUE(commandList->hasMidThreadPreemptionKernel());

    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.requiresDisabledMidThreadPreemption = true;
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams));
    EXPECT_TRUE(commandList->hasMidThreadPreemptionKernel());

    commandList->reset();
    EXPECT_FALSE(commandList->hasMidThreadPreemptionKernel());
}

HWTEST2_F(CommandListXe2AndLaterPreemptionTest, givenMidThreadPreemptionAllocationOnDemandWhenExecutingCommandListsThenPreemptionStateIsProgrammedOnceOnFirstMidThreadPreemptionKernel, IsXe2HpgCore) {
    using STATE_CONTEXT_DATA_BASE_ADDRESS = typename FamilyType::STATE_CONTEXT_DATA_BASE_ADDRESS;
    using STATE_SIP = typename FamilyType::STATE_SIP;

    neoDevice->preemptionMode = NEO::PreemptionMode::MidThread;
    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    ASSERT_EQ(&csr, commandQueue->getCsr());
    VariableBackup<NEO::GraphicsAllocation *> preemptionAllocationBackup(&csr.preemptionAllocation, nullptr);
    VariableBackup<bool> onDemandBackup(&csr.midThreadPreemptionAllocationOnDemand, true);
    VariableBackup<bool> stateSipSentBackup(&csr.isStateSipSent, false);
    VariableBackup<NEO::PreemptionMode> preemptionModeBackup(&csr.lastPreemptionMode, NEO::PreemptionMode::Initial);

    auto &queueStream = commandQueue->commandStream;
    auto commandListHandle = commandList->toHandle();
    CommandListExecutionInternalOptions internalOptions = {};

    auto countPreemptionStateCommands = [&](size_t streamOffset, uint32_t &contextDataBaseAddressCount, uint32_t &stateSipCount) {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(queueStream.getCpuBase(), streamOffset), queueStream.getUsed() - streamOffset));
        contextDataBaseAddressCount = static_cast<uint32_t>(findAll<STATE_CONTEXT_DATA_BASE_ADDRESS *>(cmdList.begin(), cmdList.end()).size());
        stateSipCount = static_cast<uint32_t>(findAll<STATE_SIP *>(cmdList.begin(), cmdList.end()).size());
    };
    auto fillUnusedQueueStreamWithGarbage = [&]() {
        memset(ptrOffset(queueStream.getCpuBase(), queueStream.getUsed()), 0xFF, queueStream.getAvailableSpace());
    };
    uint32_t contextDataBaseAddressCount = 0;
    uint32_t stateSipCount = 0;

    commandList->close();
    auto usedSize = queueStream.getUsed();
    fillUnusedQueueStreamWithGarbage();
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandQueue->executeCommandLists(1, &commandListHandle, nullptr, internalOptions));
    EXPECT_EQ(nullptr, csr.getPreemptionAllocation());
    countPreemptionStateCommands(usedSize, contextDataBaseAddressCount, stateSipCount);
    EXPECT_EQ(0u, contextDataBaseAddressCount);
    EXPECT_EQ(0u, stateSipCount);

    commandList->reset();
    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.requiresDisabledMidThreadPreemption = false;
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams));
    commandList->close();

    usedSize = queueStream.getUsed();
    fillUnusedQueueStreamWithGarbage();
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandQueue->executeCommandLists(1, &commandListHandle, nullptr, internalOptions));
    auto preemptionAllocation = csr.getPreemptionAllocation();
    ASSERT_NE(nullptr, preemptionAllocation);
    EXPECT_TRUE(csr.getSipSentFlag());
    countPreemptionStateCommands(usedSize, contextDataBaseAddressCount, stateSipCount);
    EXPECT_EQ(1u, contextDataBaseAddressCount);
    EXPECT_EQ(1u, stateSipCount);

    usedSize = queueStream.getUsed();
    fillUnusedQueueStreamWithGarbage();
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandQueue->executeCommandLists(1, &commandListHandle, nullptr, internalOptions));
    EXPECT_EQ(preemptionAllocation, csr.getPreemptionAllocation());
    countPreemptionStateCommands(usedSize, contextDataBaseAddressCount, stateSipCount);
    EXPECT_EQ(0u, contextDataBaseAddressCount);
    EXPECT_EQ(0u, stateSipCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandQueue->synchronize(0));
    csr.getMemoryManager()->freeGraphicsMemory(preemptionAllocation);
}

template <typename FamilyType>
void verifyImmediateCommandListCreatesPreemptionAllocationOnFirstMidThreadPreemptionKernel(L0::Device *device, NEO::MockDevice *neoDevice, ModuleImmutableDataFixture::MockImmutableData &mockKernelImmData, L0::Kernel &kernel, int32_t useImmediateFlushTask) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.UseImmediateFlushTask.set(useImmediateFlushTask);
    neoDevice->setPreemptionMode(NEO::PreemptionMode::MidThread);

    ze_command_queue_desc_t queueDesc{ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandListImmediate(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
    ASSERT_NE(nullptr, commandListImmediate);
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandListImmediate->ensureImmediateResourcesInitialized());

    auto &csr = *static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(commandListImmediate->getCsr(false));
    VariableBackup<NEO::GraphicsAllocation *> preemptionAllocationBackup(&csr.preemptionAllocation, nullptr);
    VariableBackup<bool> onDemandBackup(&csr.midThreadPreemptionAllocationOnDemand, true);
    VariableBackup<bool> stateSipSentBackup(&csr.isStateSipSent, false);

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    mockKernelImmData.kernelDescriptor->kernelAttributes.flags.requiresDisabledMidThreadPreemption = true;
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandListImmediate->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams));
    EXPECT_EQ(nullptr, csr.getPreemptionAllocation());
    EXPECT_FALSE(csr.getSipSentFlag());

    mockKernelImmData.kernelDescriptor->kernelAttributes.flags.requiresDisabledMidThreadPreemption = false;
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandListImmediate->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams));
    auto preemptionAllocation = csr.getPreemptionAllocation();
    EXPECT_NE(nullptr, preemptionAllocation);
    EXPECT_TRUE(csr.getSipSentFlag());

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandListImmediate->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    csr.getMemoryManager()->freeGraphicsMemory(preemptionAllocation);
}

HWTEST2_F(CommandListXe2AndLaterPreemptionTest, givenMidThreadPreemptionAllocationOnDemandAndImmediateFlushTaskWhenAppendingKernelsToImmediateCommandListThenPreemptionAllocationIsCreatedOnFirstMidThreadPreemptionKernel, IsXe2HpgCore) {
    verifyImmediateCommandListCreatesPreemptionAllocationOnFirstMidThreadPreemptionKernel<FamilyType>(device, neoDevice, *mockKernelImmData, *kernel, 1);
}

HWTEST2_F(CommandListXe2AndLaterPreemptionTest, givenMidThreadPreemptionAllocationOnDemandAndRegularFlushTaskWhenAppendingKernelsToImmediateCommandListThenPreemptionAllocationIsCreatedOnFirstMidThreadPreemptionKernel, IsXe2HpgCore) {
    verifyImmediateCommandListCreatesPreemptionAllocationOnFirstMidThreadPreemptionKernel<FamilyType>(device, neoDevice, *mockKernelImmData, *kernel, 0);
}

using InOrderCmdListTests = InOrderCmdListFixture;

HWTEST2_F(InOrderCmdListTests, givenDebugFlagWhenPostSyncWithInOrderExecInfoIsCreateThenL1IsNotFlushed, Platforms) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ForcePostSyncL1Flush.set(0);
    using WalkerType = typename FamilyType::DefaultWalkerType;
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto walkerItor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);

    auto walker = genCmdCast<WalkerType *>(*walkerItor);
    auto &postSync = walker->getPostSync();

    EXPECT_FALSE(postSync.getDataportPipelineFlush());
    EXPECT_FALSE(postSync.getDataportSubsliceCacheFlush());
}

} // namespace ult
} // namespace L0
