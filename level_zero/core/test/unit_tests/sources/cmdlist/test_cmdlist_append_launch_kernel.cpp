/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/preamble.h"
#include "shared/test/unit_test/cmd_parse/gen_cmd_parse.h"

#include "opencl/source/helpers/hardware_commands_helper.h"
#include "test.h"

#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
namespace L0 {
namespace ult {

using CommandListAppendLaunchKernel = Test<ModuleFixture>;

HWTEST_F(CommandListAppendLaunchKernel, givenNotEnoughSpaceInCommandStreamWhenAppendingKernelThenBbEndIsAddedAndNewCmdBufferAllocated) {
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;
    createKernel();

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device));

    auto &commandContainer = commandList->commandContainer;
    const auto stream = commandContainer.getCommandStream();
    const auto streamCpu = stream->getCpuBase();

    auto available = stream->getAvailableSpace();
    stream->getSpace(available - sizeof(MI_BATCH_BUFFER_END) - 16);
    auto bbEndPosition = stream->getSpace(0);

    ze_group_count_t groupCount{1, 1, 1};
    commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr);

    auto usedSpaceAfter = commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, 0u);

    const auto streamCpu2 = stream->getCpuBase();

    EXPECT_NE(nullptr, streamCpu2);
    EXPECT_NE(streamCpu, streamCpu2);

    EXPECT_EQ(2u, commandContainer.getCmdBufferAllocations().size());

    GenCmdList cmdList;
    FamilyType::PARSE::parseCommandBuffer(cmdList, bbEndPosition, 2 * sizeof(MI_BATCH_BUFFER_END));
    auto itor = find<MI_BATCH_BUFFER_END *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandListAppendLaunchKernel, givenFunctionWhenBindingTablePrefetchAllowedThenProgramBindingTableEntryCount) {
    using MEDIA_INTERFACE_DESCRIPTOR_LOAD = typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    createKernel();

    ze_group_count_t groupCount{1, 1, 1};
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device));
    commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr);

    auto commandStream = commandList->commandContainer.getCommandStream();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, commandStream->getCpuBase(), commandStream->getUsed()));

    auto itorMIDL = find<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(itorMIDL, cmdList.end());

    auto cmd = genCmdCast<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(*itorMIDL);
    ASSERT_NE(cmd, nullptr);

    auto dsh = commandList->commandContainer.getIndirectHeap(NEO::HeapType::DYNAMIC_STATE);
    auto idd = static_cast<INTERFACE_DESCRIPTOR_DATA *>(ptrOffset(dsh->getCpuBase(), cmd->getInterfaceDescriptorDataStartAddress()));

    if (NEO::HardwareCommandsHelper<FamilyType>::doBindingTablePrefetch()) {
        uint32_t numArgs = kernel->kernelImmData->getDescriptor().payloadMappings.bindingTable.numEntries;
        EXPECT_EQ(numArgs, idd->getBindingTableEntryCount());
    } else {
        EXPECT_EQ(0u, idd->getBindingTableEntryCount());
    }
}

HWTEST_F(CommandListAppendLaunchKernel, givenKernelWithPrintfUsedWhenAppendedToCommandListThenKernelIsStored) {
    createKernel();
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device));
    ze_group_count_t groupCount{1, 1, 1};

    EXPECT_TRUE(kernel->kernelImmData->getDescriptor().kernelAttributes.flags.usesPrintf);

    auto result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(1u, commandList->getPrintfFunctionContainer().size());
    EXPECT_EQ(kernel.get(), commandList->getPrintfFunctionContainer()[0]);
}

HWTEST_F(CommandListAppendLaunchKernel, givenKernelWithPrintfUsedWhenAppendedToCommandListMultipleTimesThenKernelIsStoredOnce) {
    createKernel();
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device));
    ze_group_count_t groupCount{1, 1, 1};

    EXPECT_TRUE(kernel->kernelImmData->getDescriptor().kernelAttributes.flags.usesPrintf);

    auto result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(1u, commandList->getPrintfFunctionContainer().size());
    EXPECT_EQ(kernel.get(), commandList->getPrintfFunctionContainer()[0]);

    result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, commandList->getPrintfFunctionContainer().size());
}

HWTEST_F(CommandListAppendLaunchKernel, WhenAppendingMultipleTimesThenSshIsNotDepletedButReallocated) {
    createKernel();
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device));
    ze_group_count_t groupCount{1, 1, 1};

    auto kernelSshSize = kernel->getSurfaceStateHeapDataSize();
    auto ssh = commandList->commandContainer.getIndirectHeap(NEO::HeapType::SURFACE_STATE);
    auto sshHeapSize = ssh->getMaxAvailableSpace();
    auto initialAllocation = ssh->getGraphicsAllocation();
    EXPECT_NE(nullptr, initialAllocation);

    for (size_t i = 0; i < sshHeapSize / kernelSshSize + 1; i++) {
        auto result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    }

    auto reallocatedAllocation = ssh->getGraphicsAllocation();
    EXPECT_NE(nullptr, reallocatedAllocation);
    EXPECT_NE(initialAllocation, reallocatedAllocation);
}

using SklPlusMatcher = IsAtLeastProduct<IGFX_SKYLAKE>;
HWTEST2_F(CommandListAppendLaunchKernel, WhenAppendingFunctionThenUsedCmdBufferSizeDoesNotExceedEstimate, SklPlusMatcher) {
    createKernel();
    ze_group_count_t groupCount{1, 1, 1};

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    bool ret = commandList->initialize(device);
    ASSERT_TRUE(ret);

    auto sizeBefore = commandList->commandContainer.getCommandStream()->getUsed();

    auto result = commandList->appendLaunchKernelWithParams(kernel->toHandle(), &groupCount, nullptr, 0, nullptr, false, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto sizeAfter = commandList->commandContainer.getCommandStream()->getUsed();
    auto estimate = NEO::EncodeDispatchKernel<FamilyType>::estimateEncodeDispatchKernelCmdsSize(device->getNEODevice());

    EXPECT_LE(sizeAfter - sizeBefore, estimate);

    sizeBefore = commandList->commandContainer.getCommandStream()->getUsed();

    result = commandList->appendLaunchKernelWithParams(kernel->toHandle(), &groupCount, nullptr, 0, nullptr, true, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    sizeAfter = commandList->commandContainer.getCommandStream()->getUsed();
    estimate = NEO::EncodeDispatchKernel<FamilyType>::estimateEncodeDispatchKernelCmdsSize(device->getNEODevice());

    EXPECT_LE(sizeAfter - sizeBefore, estimate);
    EXPECT_LE(sizeAfter - sizeBefore, estimate);
}

} // namespace ult
} // namespace L0