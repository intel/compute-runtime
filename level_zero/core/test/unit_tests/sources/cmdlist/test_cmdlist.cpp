/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/test/unit_test/cmd_parse/gen_cmd_parse.h"

#include "test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"

namespace L0 {
namespace ult {

using CommandListCreate = Test<DeviceFixture>;

TEST(zeCommandListCreateImmediate, redirectsToObject) {
    Mock<Device> device;
    ze_command_queue_desc_t desc = {};
    ze_command_list_handle_t commandList = {};

    EXPECT_CALL(device, createCommandListImmediate(&desc, &commandList))
        .Times(1)
        .WillRepeatedly(::testing::Return(ZE_RESULT_SUCCESS));

    auto result = zeCommandListCreateImmediate(device.toHandle(), &desc, &commandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(CommandListCreate, whenCommandListIsCreatedThenItIsInitialized) {
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device.get()));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device.get(), commandList->device);
    ASSERT_GT(commandList->commandContainer.getCmdBufferAllocations().size(), 0u);

    auto numAllocations = 0u;
    auto allocation = whitebox_cast(commandList->commandContainer.getCmdBufferAllocations()[0]);
    ASSERT_NE(allocation, nullptr);

    ++numAllocations;

    ASSERT_NE(nullptr, commandList->commandContainer.getCommandStream());
    for (uint32_t i = 0; i < NEO::HeapType::NUM_TYPES; i++) {
        ASSERT_NE(commandList->commandContainer.getIndirectHeap(static_cast<NEO::HeapType>(i)), nullptr);
        ++numAllocations;
        ASSERT_NE(commandList->commandContainer.getIndirectHeapAllocation(static_cast<NEO::HeapType>(i)), nullptr);
    }

    EXPECT_LT(0u, commandList->commandContainer.getCommandStream()->getAvailableSpace());
    ASSERT_EQ(commandList->commandContainer.getResidencyContainer().size(), numAllocations);
    EXPECT_EQ(commandList->commandContainer.getResidencyContainer().front(), allocation);
}

TEST_F(CommandListCreate, givenRegularCommandListThenDefaultNumIddPerBlockIsUsed) {
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device.get()));
    ASSERT_NE(nullptr, commandList);

    const uint32_t defaultNumIdds = CommandList::defaultNumIddsPerBlock;
    EXPECT_EQ(defaultNumIdds, commandList->commandContainer.getNumIddPerBlock());
}

TEST_F(CommandListCreate, givenImmediateCommandListThenCustomNumIddPerBlockUsed) {
    const ze_command_queue_desc_t desc = {
        ZE_COMMAND_QUEUE_DESC_VERSION_CURRENT,
        ZE_COMMAND_QUEUE_FLAG_NONE,
        ZE_COMMAND_QUEUE_MODE_DEFAULT,
        ZE_COMMAND_QUEUE_PRIORITY_NORMAL,
        0};
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device.get(), &desc, false));
    ASSERT_NE(nullptr, commandList);

    const uint32_t cmdListImmediateIdds = CommandList::commandListimmediateIddsPerBlock;
    EXPECT_EQ(cmdListImmediateIdds, commandList->commandContainer.getNumIddPerBlock());
}

TEST_F(CommandListCreate, whenCreatingImmediateCommandListThenItHasImmediateCommandQueueCreated) {
    const ze_command_queue_desc_t desc = {
        ZE_COMMAND_QUEUE_DESC_VERSION_CURRENT,
        ZE_COMMAND_QUEUE_FLAG_NONE,
        ZE_COMMAND_QUEUE_MODE_DEFAULT,
        ZE_COMMAND_QUEUE_PRIORITY_NORMAL,
        0};
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device.get(), &desc, false));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device.get(), commandList->device);
    EXPECT_EQ(1u, commandList->cmdListType);
    EXPECT_NE(nullptr, commandList->cmdQImmediate);
}

TEST_F(CommandListCreate, givenInvalidProductFamilyThenReturnsNullPointer) {
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(IGFX_UNKNOWN, device.get()));
    EXPECT_EQ(nullptr, commandList);
}

HWTEST_F(CommandListCreate, whenCommandListIsCreatedThenStateBaseAddressCmdIsAddedAndCorrectlyProgrammed) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device.get()));
    auto &commandContainer = commandList->commandContainer;
    auto gmmHelper = commandContainer.getDevice()->getGmmHelper();

    ASSERT_NE(nullptr, commandContainer.getCommandStream());
    auto usedSpaceBefore = commandContainer.getCommandStream()->getUsed();

    auto result = commandList->close();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), usedSpaceAfter));
    auto itor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);
    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*itor);

    auto dsh = commandContainer.getIndirectHeap(NEO::HeapType::DYNAMIC_STATE);
    auto ioh = commandContainer.getIndirectHeap(NEO::HeapType::INDIRECT_OBJECT);
    auto ssh = commandContainer.getIndirectHeap(NEO::HeapType::SURFACE_STATE);

    EXPECT_TRUE(cmdSba->getDynamicStateBaseAddressModifyEnable());
    EXPECT_TRUE(cmdSba->getDynamicStateBufferSizeModifyEnable());
    EXPECT_EQ(dsh->getHeapGpuBase(), cmdSba->getDynamicStateBaseAddress());
    EXPECT_EQ(dsh->getHeapSizeInPages(), cmdSba->getDynamicStateBufferSize());

    EXPECT_TRUE(cmdSba->getIndirectObjectBaseAddressModifyEnable());
    EXPECT_TRUE(cmdSba->getIndirectObjectBufferSizeModifyEnable());
    EXPECT_EQ(ioh->getHeapGpuBase(), cmdSba->getIndirectObjectBaseAddress());
    EXPECT_EQ(ioh->getHeapSizeInPages(), cmdSba->getIndirectObjectBufferSize());

    EXPECT_TRUE(cmdSba->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssh->getHeapGpuBase(), cmdSba->getSurfaceStateBaseAddress());

    EXPECT_EQ(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER), cmdSba->getStatelessDataPortAccessMemoryObjectControlState());
}

HWTEST_F(CommandListCreate, givenNotEnoughSpaceInCommandStreamWhenAppendingFunctionThenBbEndIsAddedAndNewCmdBufferAllocated) {
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;
    Mock<Kernel> kernel;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device.get()));

    auto &commandContainer = commandList->commandContainer;
    const auto stream = commandContainer.getCommandStream();
    const auto streamCpu = stream->getCpuBase();

    auto available = stream->getAvailableSpace();
    stream->getSpace(available - sizeof(MI_BATCH_BUFFER_END) - 16);
    auto bbEndPosition = stream->getSpace(0);

    ze_group_count_t dispatchFunctionArguments{1, 1, 1};
    commandList->appendLaunchKernel(kernel.toHandle(), &dispatchFunctionArguments, nullptr, 0, nullptr);

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

} // namespace ult
} // namespace L0