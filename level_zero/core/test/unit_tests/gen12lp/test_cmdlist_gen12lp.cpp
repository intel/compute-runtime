/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"

namespace L0 {
namespace ult {

using CommandListCreateGen12Lp = Test<DeviceFixture>;

template <GFXCORE_FAMILY gfxCoreFamily>
struct CommandListAdjustStateComputeMode : public WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> {
    CommandListAdjustStateComputeMode() : WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>() {}
    using ::L0::CommandListCoreFamily<gfxCoreFamily>::applyMemoryRangesBarrier;
    using ::L0::CommandListCoreFamily<gfxCoreFamily>::commandContainer;
};

HWTEST2_F(CommandListCreateGen12Lp, givenAllocationsWhenApplyRangesBarrierThenCheckWhetherL3ControlIsProgrammed, IsGen12LP) {

    using L3_CONTROL = typename FamilyType::L3_CONTROL;
    auto &hardwareInfo = this->neoDevice->getHardwareInfo();
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
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));
    auto itor = find<L3_CONTROL *>(cmdList.begin(), cmdList.end());
    if (hardwareInfo.capabilityTable.supportCacheFlushAfterWalker) {
        EXPECT_NE(cmdList.end(), itor);
    } else {
        EXPECT_EQ(cmdList.end(), itor);
    }
}

HWTEST2_F(CommandListCreateGen12Lp, GivenNullptrWaitEventsArrayAndCountGreaterThanZeroWhenAppendingMemoryBarrierThenInvalidArgumentErrorIsReturned,
          IsDG1) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    uint32_t numRanges = 1;
    const size_t pRangeSizes = 1;
    const char *ranges[pRangeSizes];
    const void **pRanges = reinterpret_cast<const void **>(&ranges[0]);

    auto commandList = new CommandListAdjustStateComputeMode<FamilyType::gfxCoreFamily>();
    bool ret = commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    ASSERT_FALSE(ret);

    uint32_t numWaitEvents{1};
    ze_event_handle_t *phWaitEvents{nullptr};

    result = commandList->appendMemoryRangesBarrier(numRanges, &pRangeSizes,
                                                    pRanges, nullptr, numWaitEvents,
                                                    phWaitEvents);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    commandList->destroy();
}

HWTEST2_F(CommandListCreateGen12Lp, GivenImmediateListAndExecutionSuccessWhenAppendingMemoryBarrierThenExecuteCommandListImmediateCalledAndSuccessReturned,
          IsDG1) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    uint32_t numRanges = 1;
    const size_t rangeSizes = 1;
    const char *rangesBuffer[rangeSizes];
    const void **ranges = reinterpret_cast<const void **>(&rangesBuffer[0]);

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    auto cmdList = new MockCommandListImmediateHw<FamilyType::gfxCoreFamily>;
    cmdList->cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList->cmdQImmediate = queue.get();
    cmdList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    cmdList->executeCommandListImmediateWithFlushTaskReturnValue = ZE_RESULT_SUCCESS;

    result = cmdList->appendMemoryRangesBarrier(numRanges, &rangeSizes,
                                                ranges, nullptr, 0,
                                                nullptr);
    EXPECT_EQ(1u, cmdList->executeCommandListImmediateWithFlushTaskCalledCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    cmdList->destroy();
}

HWTEST2_F(CommandListCreateGen12Lp, GivenImmediateListAndGpuFailureWhenAppendingMemoryBarrierThenExecuteCommandListImmediateCalledAndDeviceLostReturned,
          IsDG1) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    uint32_t numRanges = 1;
    const size_t rangeSizes = 1;
    const char *rangesBuffer[rangeSizes];
    const void **ranges = reinterpret_cast<const void **>(&rangesBuffer[0]);

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    auto cmdList = new MockCommandListImmediateHw<FamilyType::gfxCoreFamily>;
    cmdList->cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList->cmdQImmediate = queue.get();
    cmdList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    cmdList->executeCommandListImmediateWithFlushTaskReturnValue = ZE_RESULT_ERROR_DEVICE_LOST;

    result = cmdList->appendMemoryRangesBarrier(numRanges, &rangeSizes,
                                                ranges, nullptr, 0,
                                                nullptr);
    EXPECT_EQ(1u, cmdList->executeCommandListImmediateWithFlushTaskCalledCount);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, result);

    cmdList->destroy();
}

HWTEST2_F(CommandListCreateGen12Lp, GivenHostMemoryNotInSvmManagerWhenAppendingMemoryBarrierThenAdditionalCommandsNotAdded,
          IsDG1) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    uint32_t numRanges = 1;
    const size_t pRangeSizes = 1;
    const char *ranges[pRangeSizes];
    const void **pRanges = reinterpret_cast<const void **>(&ranges[0]);

    auto commandList = new CommandListAdjustStateComputeMode<FamilyType::gfxCoreFamily>();
    bool ret = commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    ASSERT_FALSE(ret);

    auto usedSpaceBefore =
        commandList->commandContainer.getCommandStream()->getUsed();
    result = commandList->appendMemoryRangesBarrier(numRanges, &pRangeSizes,
                                                    pRanges, nullptr, 0,
                                                    nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter =
        commandList->commandContainer.getCommandStream()->getUsed();

    ASSERT_EQ(usedSpaceAfter, usedSpaceBefore);

    commandList->destroy();
}

HWTEST2_F(CommandListCreateGen12Lp, GivenHostMemoryInSvmManagerWhenAppendingMemoryBarrierThenL3CommandsAdded,
          IsDG1) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    uint32_t numRanges = 1;
    const size_t pRangeSizes = 1;
    void *ranges;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    result = context->allocDeviceMem(device->toHandle(),
                                     &deviceDesc,
                                     pRangeSizes,
                                     4096u,
                                     &ranges);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    const void **pRanges = const_cast<const void **>(&ranges);

    auto commandList = new CommandListAdjustStateComputeMode<FamilyType::gfxCoreFamily>();
    bool ret = commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    ASSERT_FALSE(ret);

    auto usedSpaceBefore =
        commandList->commandContainer.getCommandStream()->getUsed();
    result = commandList->appendMemoryRangesBarrier(numRanges, &pRangeSizes,
                                                    pRanges, nullptr, 0,
                                                    nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto usedSpaceAfter =
        commandList->commandContainer.getCommandStream()->getUsed();
    ASSERT_NE(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(
            commandList->commandContainer.getCommandStream()->getCpuBase(), 0),
        usedSpaceAfter));

    using L3_CONTROL = typename FamilyType::L3_CONTROL;
    auto itorPC = find<L3_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPC);
    {
        using L3_FLUSH_EVICTION_POLICY = typename FamilyType::L3_FLUSH_ADDRESS_RANGE::L3_FLUSH_EVICTION_POLICY;
        auto cmd = genCmdCast<L3_CONTROL *>(*itorPC);
        const auto &productHelper = device->getProductHelper();
        auto isA0Stepping = (productHelper.getSteppingFromHwRevId(device->getHwInfo()) == REVISION_A0);
        auto maskedAddress = cmd->getL3FlushAddressRange().getAddress(isA0Stepping);
        EXPECT_NE(maskedAddress, 0u);

        EXPECT_EQ(reinterpret_cast<uint64_t>(*pRanges),
                  static_cast<uint64_t>(maskedAddress));
        EXPECT_EQ(
            cmd->getL3FlushAddressRange().getL3FlushEvictionPolicy(isA0Stepping),
            L3_FLUSH_EVICTION_POLICY::L3_FLUSH_EVICTION_POLICY_FLUSH_L3_WITH_EVICTION);
    }

    commandList->destroy();
    context->freeMem(ranges);
}

HWTEST2_F(CommandListCreateGen12Lp, GivenHostMemoryWhenAppendingMemoryBarrierThenAddressMisalignmentCorrected,
          IsDG1) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    uint32_t numRanges = 1;
    const size_t misalignmentFactor = 761;
    const size_t pRangeSizes = 4096;
    void *ranges;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    result = context->allocDeviceMem(device->toHandle(),
                                     &deviceDesc,
                                     pRangeSizes,
                                     4096u,
                                     &ranges);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    unsigned char *cPRanges = reinterpret_cast<unsigned char *>(ranges);
    cPRanges += misalignmentFactor;
    ranges = static_cast<void *>(cPRanges);
    const void **pRanges = const_cast<const void **>(&ranges);

    auto commandList = new CommandListAdjustStateComputeMode<FamilyType::gfxCoreFamily>();
    bool ret = commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    ASSERT_FALSE(ret);

    auto usedSpaceBefore =
        commandList->commandContainer.getCommandStream()->getUsed();
    result = commandList->appendMemoryRangesBarrier(numRanges, &pRangeSizes,
                                                    pRanges, nullptr, 0,
                                                    nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto usedSpaceAfter =
        commandList->commandContainer.getCommandStream()->getUsed();
    ASSERT_NE(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(
            commandList->commandContainer.getCommandStream()->getCpuBase(), 0),
        usedSpaceAfter));

    using L3_CONTROL = typename FamilyType::L3_CONTROL;
    auto itorPC = find<L3_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPC);
    {
        using L3_FLUSH_EVICTION_POLICY = typename FamilyType::L3_FLUSH_ADDRESS_RANGE::L3_FLUSH_EVICTION_POLICY;
        auto cmd = genCmdCast<L3_CONTROL *>(*itorPC);
        const auto &productHelper = device->getProductHelper();
        auto isA0Stepping = (productHelper.getSteppingFromHwRevId(device->getHwInfo()) == REVISION_A0);
        auto maskedAddress = cmd->getL3FlushAddressRange().getAddress(isA0Stepping);
        EXPECT_NE(maskedAddress, 0u);

        EXPECT_EQ(reinterpret_cast<uint64_t>(*pRanges) - misalignmentFactor,
                  static_cast<uint64_t>(maskedAddress));

        EXPECT_EQ(
            cmd->getL3FlushAddressRange().getL3FlushEvictionPolicy(isA0Stepping),
            L3_FLUSH_EVICTION_POLICY::L3_FLUSH_EVICTION_POLICY_FLUSH_L3_WITH_EVICTION);
    }

    commandList->destroy();
    context->freeMem(ranges);
}

HWTEST2_F(CommandListCreateGen12Lp, givenAllocationsWhenApplyRangesBarrierWithInvalidAddressSizeThenL3ControlIsNotProgrammed, IsDG1) {

    using L3_CONTROL = typename FamilyType::L3_CONTROL;

    ze_result_t result = ZE_RESULT_SUCCESS;
    const size_t pRangeSizes = 4096;
    void *ranges;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    result = context->allocDeviceMem(device->toHandle(),
                                     &deviceDesc,
                                     pRangeSizes,
                                     4096u,
                                     &ranges);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto commandList = new CommandListAdjustStateComputeMode<FamilyType::gfxCoreFamily>();
    ASSERT_NE(nullptr, commandList); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
    bool ret = commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    ASSERT_FALSE(ret);

    const void *pRanges[] = {ranges};
    const size_t sizes[] = {2 * pRangeSizes};
    commandList->applyMemoryRangesBarrier(1, sizes, pRanges);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));
    auto itor = find<L3_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);

    commandList->destroy();
    context->freeMem(ranges);
}

HWTEST2_F(CommandListCreateGen12Lp, givenAllocationsWhenApplyRangesBarrierWithInvalidAddressThenL3ControlIsNotProgrammed, IsDG1) {

    using L3_CONTROL = typename FamilyType::L3_CONTROL;

    ze_result_t result = ZE_RESULT_SUCCESS;
    const size_t pRangeSizes = 4096;
    void *ranges;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    result = context->allocDeviceMem(device->toHandle(),
                                     &deviceDesc,
                                     pRangeSizes,
                                     4096u,
                                     &ranges);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto commandList = new CommandListAdjustStateComputeMode<FamilyType::gfxCoreFamily>();
    ASSERT_NE(nullptr, commandList); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
    bool ret = commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    ASSERT_FALSE(ret);

    const void *pRanges[] = {nullptr};
    const size_t sizes[] = {pRangeSizes};
    commandList->applyMemoryRangesBarrier(1, sizes, pRanges);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));
    auto itor = find<L3_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);

    commandList->destroy();
    context->freeMem(ranges);
}

using CommandListGen12LpStateComputeModeTrackingTest = Test<ModuleMutableCommandListFixture>;

GEN12LPTEST_F(CommandListGen12LpStateComputeModeTrackingTest,
              givenPlatformDisabledStateComputeModeTrackingWhenCommandListCreatedAndKernelAppendedThenStreamPropertiesCorrectlyTransitionAndStateComputeModeCommandDispatched) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    auto &productHelper = getHelper<ProductHelper>();
    StateComputeModePropertiesSupport scmPropertiesSupport;
    productHelper.fillScmPropertiesSupportStructure(scmPropertiesSupport);

    ASSERT_FALSE(commandList->stateComputeModeTracking);

    auto &cmdlistRequiredState = commandList->getRequiredStreamState();
    auto &cmdListFinalState = commandList->getFinalStreamState();
    auto &commandListStream = *commandList->commandContainer.getCommandStream();

    EXPECT_EQ(-1, cmdlistRequiredState.stateComputeMode.devicePreemptionMode.value);
    EXPECT_EQ(-1, cmdlistRequiredState.stateComputeMode.isCoherencyRequired.value);
    EXPECT_EQ(-1, cmdlistRequiredState.stateComputeMode.largeGrfMode.value);
    EXPECT_EQ(-1, cmdlistRequiredState.stateComputeMode.threadArbitrationPolicy.value);

    EXPECT_EQ(-1, cmdListFinalState.stateComputeMode.devicePreemptionMode.value);
    EXPECT_EQ(-1, cmdListFinalState.stateComputeMode.isCoherencyRequired.value);
    EXPECT_EQ(-1, cmdListFinalState.stateComputeMode.largeGrfMode.value);
    EXPECT_EQ(-1, cmdListFinalState.stateComputeMode.threadArbitrationPolicy.value);

    const ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    GenCmdList cmdList;
    std::vector<GenCmdList::iterator> stateComputeModeList;
    size_t sizeBefore = 0;
    size_t sizeAfter = 0;
    auto result = ZE_RESULT_SUCCESS;

    mockKernelImmData->kernelDescriptor->kernelAttributes.numGrfRequired = GrfConfig::defaultGrfNumber;
    mockKernelImmData->kernelDescriptor->kernelAttributes.threadArbitrationPolicy = NEO::ThreadArbitrationPolicy::RoundRobin;

    sizeBefore = commandListStream.getUsed();
    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    sizeAfter = commandListStream.getUsed();

    if (scmPropertiesSupport.devicePreemptionMode) {
        int32_t expectedPreemption = static_cast<int32_t>(device->getDevicePreemptionMode());
        EXPECT_EQ(expectedPreemption, cmdlistRequiredState.stateComputeMode.devicePreemptionMode.value);
        EXPECT_EQ(expectedPreemption, cmdListFinalState.stateComputeMode.devicePreemptionMode.value);
    } else {
        EXPECT_EQ(-1, cmdlistRequiredState.stateComputeMode.devicePreemptionMode.value);
        EXPECT_EQ(-1, cmdListFinalState.stateComputeMode.devicePreemptionMode.value);
    }

    if (scmPropertiesSupport.coherencyRequired) {
        EXPECT_EQ(0, cmdlistRequiredState.stateComputeMode.isCoherencyRequired.value);
        EXPECT_EQ(0, cmdListFinalState.stateComputeMode.isCoherencyRequired.value);
    } else {
        EXPECT_EQ(-1, cmdlistRequiredState.stateComputeMode.isCoherencyRequired.value);
        EXPECT_EQ(-1, cmdListFinalState.stateComputeMode.isCoherencyRequired.value);
    }

    if (scmPropertiesSupport.largeGrfMode) {
        EXPECT_EQ(0, cmdlistRequiredState.stateComputeMode.largeGrfMode.value);
        EXPECT_EQ(0, cmdListFinalState.stateComputeMode.largeGrfMode.value);
    } else {
        EXPECT_EQ(-1, cmdlistRequiredState.stateComputeMode.largeGrfMode.value);
        EXPECT_EQ(-1, cmdListFinalState.stateComputeMode.largeGrfMode.value);
    }

    if (scmPropertiesSupport.threadArbitrationPolicy) {
        EXPECT_EQ(1, cmdlistRequiredState.stateComputeMode.threadArbitrationPolicy.value);
        EXPECT_EQ(1, cmdListFinalState.stateComputeMode.threadArbitrationPolicy.value);
    } else {
        EXPECT_EQ(-1, cmdlistRequiredState.stateComputeMode.threadArbitrationPolicy.value);
        EXPECT_EQ(-1, cmdListFinalState.stateComputeMode.threadArbitrationPolicy.value);
    }

    auto currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      currentBuffer,
                                                      (sizeAfter - sizeBefore)));
    stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, stateComputeModeList.size());
}

} // namespace ult
} // namespace L0
