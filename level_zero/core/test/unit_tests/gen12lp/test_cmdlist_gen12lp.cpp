/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/gen12lp/cmdlist_gen12lp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"

namespace L0 {
namespace ult {

using CommandListCreate = Test<DeviceFixture>;

template <PRODUCT_FAMILY productFamily>
struct CommandListAdjustStateComputeMode : public WhiteBox<::L0::CommandListProductFamily<productFamily>> {
    CommandListAdjustStateComputeMode() : WhiteBox<::L0::CommandListProductFamily<productFamily>>(1) {}
    using ::L0::CommandListProductFamily<productFamily>::applyMemoryRangesBarrier;
};

HWTEST2_F(CommandListCreate, givenAllocationsWhenApplyRangesBarrierThenCheckWhetherL3ControlIsProgrammed, IsGen12LP) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using L3_CONTROL = typename GfxFamily::L3_CONTROL;
    auto &hardwareInfo = this->neoDevice->getHardwareInfo();
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy, 0u);
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
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));
    auto itor = find<L3_CONTROL *>(cmdList.begin(), cmdList.end());
    if (hardwareInfo.capabilityTable.supportCacheFlushAfterWalker) {
        EXPECT_NE(cmdList.end(), itor);
    } else {
        EXPECT_EQ(cmdList.end(), itor);
    }
}

HWTEST2_F(CommandListCreate, GivenNullptrWaitEventsArrayAndCountGreaterThanZeroWhenAppendingMemoryBarrierThenInvalidArgumentErrorIsReturned,
          IsDG1) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    uint32_t numRanges = 1;
    const size_t pRangeSizes = 1;
    const char *ranges[pRangeSizes];
    const void **pRanges = reinterpret_cast<const void **>(&ranges[0]);

    auto commandList = new CommandListAdjustStateComputeMode<productFamily>();
    bool ret = commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    ASSERT_FALSE(ret);

    uint32_t numWaitEvents{1};
    ze_event_handle_t *phWaitEvents{nullptr};

    result = commandList->appendMemoryRangesBarrier(numRanges, &pRangeSizes,
                                                    pRanges, nullptr, numWaitEvents,
                                                    phWaitEvents);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    commandList->destroy();
}

HWTEST2_F(CommandListCreate, GivenImmediateListAndExecutionSuccessWhenAppendingMemoryBarrierThenExecuteCommandListImmediateCalledAndSuccessReturned,
          IsDG1) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    uint32_t numRanges = 1;
    const size_t rangeSizes = 1;
    const char *rangesBuffer[rangeSizes];
    const void **ranges = reinterpret_cast<const void **>(&rangesBuffer[0]);

    auto cmdList = new MockCommandListImmediateHw<gfxCoreFamily>;
    cmdList->cmdListType = CommandList::CommandListType::TYPE_IMMEDIATE;
    cmdList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    cmdList->executeCommandListImmediateReturnValue = ZE_RESULT_SUCCESS;

    result = cmdList->appendMemoryRangesBarrier(numRanges, &rangeSizes,
                                                ranges, nullptr, 0,
                                                nullptr);
    EXPECT_EQ(1u, cmdList->executeCommandListImmediateCalledCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    cmdList->destroy();
}

HWTEST2_F(CommandListCreate, GivenImmediateListAndGpuFailureWhenAppendingMemoryBarrierThenExecuteCommandListImmediateCalledAndDeviceLostReturned,
          IsDG1) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    uint32_t numRanges = 1;
    const size_t rangeSizes = 1;
    const char *rangesBuffer[rangeSizes];
    const void **ranges = reinterpret_cast<const void **>(&rangesBuffer[0]);

    auto cmdList = new MockCommandListImmediateHw<gfxCoreFamily>;
    cmdList->cmdListType = CommandList::CommandListType::TYPE_IMMEDIATE;
    cmdList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    cmdList->executeCommandListImmediateReturnValue = ZE_RESULT_ERROR_DEVICE_LOST;

    result = cmdList->appendMemoryRangesBarrier(numRanges, &rangeSizes,
                                                ranges, nullptr, 0,
                                                nullptr);
    EXPECT_EQ(1u, cmdList->executeCommandListImmediateCalledCount);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, result);

    cmdList->destroy();
}

HWTEST2_F(CommandListCreate, GivenHostMemoryNotInSvmManagerWhenAppendingMemoryBarrierThenAdditionalCommandsNotAdded,
          IsDG1) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    uint32_t numRanges = 1;
    const size_t pRangeSizes = 1;
    const char *ranges[pRangeSizes];
    const void **pRanges = reinterpret_cast<const void **>(&ranges[0]);

    auto commandList = new CommandListAdjustStateComputeMode<productFamily>();
    bool ret = commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
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

HWTEST2_F(CommandListCreate, GivenHostMemoryInSvmManagerWhenAppendingMemoryBarrierThenL3CommandsAdded,
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

    auto commandList = new CommandListAdjustStateComputeMode<productFamily>();
    bool ret = commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
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
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
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
        const auto &hwInfoConfig = *NEO::HwInfoConfig::get(device->getHwInfo().platform.eProductFamily);
        auto isA0Stepping = (hwInfoConfig.getSteppingFromHwRevId(device->getHwInfo()) == REVISION_A0);
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

HWTEST2_F(CommandListCreate, GivenHostMemoryWhenAppendingMemoryBarrierThenAddressMisalignmentCorrected,
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

    auto commandList = new CommandListAdjustStateComputeMode<productFamily>();
    bool ret = commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
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
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
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
        const auto &hwInfoConfig = *NEO::HwInfoConfig::get(device->getHwInfo().platform.eProductFamily);
        auto isA0Stepping = (hwInfoConfig.getSteppingFromHwRevId(device->getHwInfo()) == REVISION_A0);
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

HWTEST2_F(CommandListCreate, givenAllocationsWhenApplyRangesBarrierWithInvalidAddressSizeThenL3ControlIsNotProgrammed, IsDG1) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using L3_CONTROL = typename GfxFamily::L3_CONTROL;

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

    auto commandList = new CommandListAdjustStateComputeMode<productFamily>();
    ASSERT_NE(nullptr, commandList); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
    bool ret = commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    ASSERT_FALSE(ret);

    const void *pRanges[] = {ranges};
    const size_t sizes[] = {2 * pRangeSizes};
    commandList->applyMemoryRangesBarrier(1, sizes, pRanges);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));
    auto itor = find<L3_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);

    commandList->destroy();
    context->freeMem(ranges);
}

HWTEST2_F(CommandListCreate, givenAllocationsWhenApplyRangesBarrierWithInvalidAddressThenL3ControlIsNotProgrammed, IsDG1) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using L3_CONTROL = typename GfxFamily::L3_CONTROL;

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

    auto commandList = new CommandListAdjustStateComputeMode<productFamily>();
    ASSERT_NE(nullptr, commandList); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
    bool ret = commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    ASSERT_FALSE(ret);

    const void *pRanges[] = {nullptr};
    const size_t sizes[] = {pRangeSizes};
    commandList->applyMemoryRangesBarrier(1, sizes, pRanges);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));
    auto itor = find<L3_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);

    commandList->destroy();
    context->freeMem(ranges);
}

} // namespace ult
} // namespace L0
