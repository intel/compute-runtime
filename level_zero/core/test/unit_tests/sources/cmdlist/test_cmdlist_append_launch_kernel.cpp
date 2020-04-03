/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/cmd_parse/gen_cmd_parse.h"

#include "opencl/source/helpers/hardware_commands_helper.h"
#include "test.h"

#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"

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

} // namespace ult
} // namespace L0