/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"
#include "shared/test/unit_test/cmd_parse/gen_cmd_parse.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"
#include "shared/test/unit_test/mocks/mock_dispatch_kernel_encoder_interface.h"

#include "opencl/source/helpers/hardware_commands_helper.h"

using namespace NEO;

using CommandEncodeStatesTest = Test<CommandEncodeStatesFixture>;

TEST_F(CommandEncodeStatesTest, givenDefaultCommandConatinerGetNumIddsInBlock) {
    auto numIdds = cmdContainer->getNumIddPerBlock();
    EXPECT_EQ(64u, numIdds);
}

TEST_F(CommandEncodeStatesTest, givenCommandConatinerCreatedWithMaxNumAggregateIddThenVerifyGetNumIddsInBlockIsCorrect) {
    auto cmdContainer = new CommandContainer(1);
    auto numIdds = cmdContainer->getNumIddPerBlock();

    EXPECT_EQ(1u, numIdds);

    delete cmdContainer;
}

HWTEST_F(CommandEncodeStatesTest, givenenDispatchInterfaceWhenDispatchKernelThenWalkerCommandProgrammed) {
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, pDevice, NEO::PreemptionMode::Disabled);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    auto itorPC = find<WALKER_TYPE *>(commands.begin(), commands.end());
    ASSERT_NE(itorPC, commands.end());
}

HWTEST_F(CommandEncodeStatesTest, givenCommandContainerWithUsedAvailableSizeWhenDispatchKernelThenNextCommandBufferIsAdded) {
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    auto cmdBuffersCountBefore = cmdContainer->getCmdBufferAllocations().size();

    cmdContainer->getCommandStream()->getSpace(cmdContainer->getCommandStream()->getAvailableSpace() - sizeof(typename FamilyType::MI_BATCH_BUFFER_END));

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, pDevice, NEO::PreemptionMode::Disabled);

    auto cmdBuffersCountAfter = cmdContainer->getCmdBufferAllocations().size();

    EXPECT_GT(cmdBuffersCountAfter, cmdBuffersCountBefore);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandEncodeStatesTest, givenSlmTotalSizeGraterThanZeroWhenDispatchingKernelThenSharedMemorySizeSetCorrectly) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint32_t slmTotalSize = 1;
    EXPECT_CALL(*dispatchInterface.get(), getSlmTotalSize()).WillRepeatedly(::testing::Return(slmTotalSize));
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, pDevice, NEO::PreemptionMode::Disabled);

    auto interfaceDescriptorData = static_cast<INTERFACE_DESCRIPTOR_DATA *>(cmdContainer->getIddBlock());

    uint32_t expectedValue = static_cast<typename INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE>(HardwareCommandsHelper<FamilyType>::computeSlmValues(slmTotalSize));

    EXPECT_EQ(expectedValue, interfaceDescriptorData->getSharedLocalMemorySize());
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandEncodeStatesTest, givenSlmTotalSizeEqualZeroWhenDispatchingKernelThenSharedMemorySizeSetCorrectly) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint32_t slmTotalSize = 0;
    EXPECT_CALL(*dispatchInterface.get(), getSlmTotalSize()).WillRepeatedly(::testing::Return(slmTotalSize));
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, pDevice, NEO::PreemptionMode::Disabled);

    auto interfaceDescriptorData = static_cast<INTERFACE_DESCRIPTOR_DATA *>(cmdContainer->getIddBlock());

    uint32_t expectedValue = INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_ENCODES_0K;

    EXPECT_EQ(expectedValue, interfaceDescriptorData->getSharedLocalMemorySize());
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandEncodeStatesTest, givennumBindingTableOneWhenDispatchingKernelThenBindingTableOffsetIsCorrect) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    uint32_t numBindingTable = 1;
    BINDING_TABLE_STATE bindingTableState;
    bindingTableState.sInit();

    auto ssh = cmdContainer->getIndirectHeap(HeapType::SURFACE_STATE);
    uint32_t sizeUsed = 0x20;
    ssh->getSpace(sizeUsed);
    auto expectedOffset = alignUp(sizeUsed, BINDING_TABLE_STATE::SURFACESTATEPOINTER_ALIGN_SIZE);

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    EXPECT_CALL(*dispatchInterface.get(), getNumSurfaceStates()).WillRepeatedly(::testing::Return(numBindingTable));
    EXPECT_CALL(*dispatchInterface.get(), getSurfaceStateHeap()).WillRepeatedly(::testing::Return(&bindingTableState));
    EXPECT_CALL(*dispatchInterface.get(), getSizeSurfaceStateHeapData()).WillRepeatedly(::testing::Return(static_cast<uint32_t>(sizeof(BINDING_TABLE_STATE))));
    EXPECT_CALL(*dispatchInterface.get(), getBindingTableOffset()).WillRepeatedly(::testing::Return(0));

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, pDevice, NEO::PreemptionMode::Disabled);
    auto interfaceDescriptorData = static_cast<INTERFACE_DESCRIPTOR_DATA *>(cmdContainer->getIddBlock());

    EXPECT_EQ(interfaceDescriptorData->getBindingTablePointer(), expectedOffset);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandEncodeStatesTest, giveNumBindingTableZeroWhenDispatchingKernelThenBindingTableOffsetIsZero) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    uint32_t numBindingTable = 0;
    BINDING_TABLE_STATE bindingTableState;
    bindingTableState.sInit();

    auto ssh = cmdContainer->getIndirectHeap(HeapType::SURFACE_STATE);
    uint32_t sizeUsed = 0x20;
    ssh->getSpace(sizeUsed);

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    EXPECT_CALL(*dispatchInterface.get(), getNumSurfaceStates()).WillRepeatedly(::testing::Return(numBindingTable));
    EXPECT_CALL(*dispatchInterface.get(), getSurfaceStateHeap()).WillRepeatedly(::testing::Return(&bindingTableState));
    EXPECT_CALL(*dispatchInterface.get(), getSizeSurfaceStateHeapData()).WillRepeatedly(::testing::Return(static_cast<uint32_t>(sizeof(BINDING_TABLE_STATE))));
    EXPECT_CALL(*dispatchInterface.get(), getBindingTableOffset()).WillRepeatedly(::testing::Return(0));

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, pDevice, NEO::PreemptionMode::Disabled);
    auto interfaceDescriptorData = static_cast<INTERFACE_DESCRIPTOR_DATA *>(cmdContainer->getIddBlock());

    EXPECT_EQ(interfaceDescriptorData->getBindingTablePointer(), 0u);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandEncodeStatesTest, giveNumSamplersOneWhenDispatchingKernelThensamplerStateWasCopied) {
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    uint32_t numSamplers = 1;
    SAMPLER_STATE samplerState;
    memset(&samplerState, 2, sizeof(SAMPLER_STATE));

    auto dsh = cmdContainer->getIndirectHeap(HeapType::DYNAMIC_STATE);
    auto usedBefore = dsh->getUsed();

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    EXPECT_CALL(*dispatchInterface.get(), getNumSamplers()).WillRepeatedly(::testing::Return(numSamplers));
    EXPECT_CALL(*dispatchInterface.get(), getSamplerTableOffset()).WillRepeatedly(::testing::Return(0));
    EXPECT_CALL(*dispatchInterface.get(), getBorderColor()).WillRepeatedly(::testing::Return(0));
    EXPECT_CALL(*dispatchInterface.get(), getDynamicStateHeap()).WillRepeatedly(::testing::Return(&samplerState));

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, pDevice, NEO::PreemptionMode::Disabled);
    auto interfaceDescriptorData = static_cast<INTERFACE_DESCRIPTOR_DATA *>(cmdContainer->getIddBlock());

    auto borderColorOffsetInDsh = usedBefore;
    samplerState.setIndirectStatePointer(static_cast<uint32_t>(borderColorOffsetInDsh));

    auto samplerStateOffset = interfaceDescriptorData->getSamplerStatePointer();

    auto pSmplr = reinterpret_cast<SAMPLER_STATE *>(ptrOffset(dsh->getCpuBase(), samplerStateOffset));
    EXPECT_EQ(memcmp(pSmplr, &samplerState, sizeof(SAMPLER_STATE)), 0);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandEncodeStatesTest, giveNumSamplersZeroWhenDispatchingKernelThensamplerStateWasNotCopied) {
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    uint32_t numSamplers = 0;
    SAMPLER_STATE samplerState;
    memset(&samplerState, 2, sizeof(SAMPLER_STATE));

    auto dsh = cmdContainer->getIndirectHeap(HeapType::DYNAMIC_STATE);
    auto usedBefore = dsh->getUsed();

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    EXPECT_CALL(*dispatchInterface.get(), getNumSamplers()).WillRepeatedly(::testing::Return(numSamplers));
    EXPECT_CALL(*dispatchInterface.get(), getSamplerTableOffset()).WillRepeatedly(::testing::Return(0));
    EXPECT_CALL(*dispatchInterface.get(), getBorderColor()).WillRepeatedly(::testing::Return(0));
    EXPECT_CALL(*dispatchInterface.get(), getDynamicStateHeap()).WillRepeatedly(::testing::Return(&samplerState));

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, pDevice, NEO::PreemptionMode::Disabled);
    auto interfaceDescriptorData = static_cast<INTERFACE_DESCRIPTOR_DATA *>(cmdContainer->getIddBlock());

    auto borderColorOffsetInDsh = usedBefore;
    samplerState.setIndirectStatePointer(static_cast<uint32_t>(borderColorOffsetInDsh));

    auto samplerStateOffset = interfaceDescriptorData->getSamplerStatePointer();

    auto pSmplr = reinterpret_cast<SAMPLER_STATE *>(ptrOffset(dsh->getCpuBase(), samplerStateOffset));
    EXPECT_NE(memcmp(pSmplr, &samplerState, sizeof(SAMPLER_STATE)), 0);
}

HWTEST_F(CommandEncodeStatesTest, givenIndarectOffsetsCountsWhenDispatchingKernelThenCorrestMIStoreOffsetsSet) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
    uint32_t dims[] = {2, 1, 1};
    uint32_t offsets[] = {0x10, 0x20, 0x30};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    EXPECT_CALL(*dispatchInterface.get(), hasGroupCounts()).WillRepeatedly(::testing::Return(true));
    EXPECT_CALL(*dispatchInterface.get(), getCountOffsets()).WillRepeatedly(::testing::Return(offsets));
    EXPECT_CALL(*dispatchInterface.get(), hasGroupSize()).WillRepeatedly(::testing::Return(false));

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, true, false, dispatchInterface.get(), 0, pDevice, NEO::PreemptionMode::Disabled);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = commands.begin();

    for (int i = 0; i < 3; i++) {
        ASSERT_NE(itor, commands.end());
        itor = find<MI_STORE_REGISTER_MEM *>(++itor, commands.end());
    }
}

HWTEST_F(CommandEncodeStatesTest, givenIndarectOffsetsSizeWhenDispatchingKernelThenMiMathEncoded) {
    using MI_MATH = typename FamilyType::MI_MATH;
    uint32_t dims[] = {2, 1, 1};
    uint32_t offsets[] = {0x10, 0x20, 0x30};
    uint32_t lws[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    EXPECT_CALL(*dispatchInterface.get(), hasGroupCounts()).WillRepeatedly(::testing::Return(false));
    EXPECT_CALL(*dispatchInterface.get(), getSizeOffsets()).WillRepeatedly(::testing::Return(offsets));
    EXPECT_CALL(*dispatchInterface.get(), hasGroupSize()).WillRepeatedly(::testing::Return(true));
    EXPECT_CALL(*dispatchInterface.get(), getLocalWorkSize()).WillRepeatedly(::testing::Return(lws));

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, true, false, dispatchInterface.get(), 0, pDevice, NEO::PreemptionMode::Disabled);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<MI_MATH *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandEncodeStatesTest, givenCleanHeapsAndSlmNotChangedWhenDispatchKernelThenFlushNotAdded) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    cmdContainer->slmSize = 1;
    EXPECT_CALL(*dispatchInterface.get(), getSlmTotalSize()).WillRepeatedly(::testing::Return(cmdContainer->slmSize));
    cmdContainer->setDirtyStateForAllHeaps(false);

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, pDevice, NEO::PreemptionMode::Disabled);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itorPC = find<PIPE_CONTROL *>(commands.begin(), commands.end());
    ASSERT_EQ(itorPC, commands.end());
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandEncodeStatesTest, givenDirtyHeapsAndSlmNotChangedWhenDispatchKernelThenHeapsAreCleanAndFlushAdded) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    cmdContainer->slmSize = 1;
    EXPECT_CALL(*dispatchInterface.get(), getSlmTotalSize()).WillRepeatedly(::testing::Return(cmdContainer->slmSize));
    cmdContainer->setDirtyStateForAllHeaps(true);

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, pDevice, NEO::PreemptionMode::Disabled);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itorPC = find<PIPE_CONTROL *>(commands.begin(), commands.end());
    ASSERT_NE(itorPC, commands.end());
    EXPECT_FALSE(cmdContainer->isAnyHeapDirty());
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandEncodeStatesTest, givenCleanHeapsAndSlmChangedWhenDispatchKernelThenFlushAdded) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    EXPECT_CALL(*dispatchInterface.get(), getSlmTotalSize()).WillRepeatedly(::testing::Return(cmdContainer->slmSize + 1));
    cmdContainer->setDirtyStateForAllHeaps(false);

    auto slmSizeBefore = cmdContainer->slmSize;

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, pDevice, NEO::PreemptionMode::Disabled);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itorPC = find<PIPE_CONTROL *>(commands.begin(), commands.end());
    ASSERT_NE(itorPC, commands.end());
    EXPECT_EQ(slmSizeBefore + 1, cmdContainer->slmSize);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandEncodeStatesTest, giveNextIddInBlockZeorWhenDispatchKernelThenMediaInterfaceDescriptorEncoded) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using MEDIA_INTERFACE_DESCRIPTOR_LOAD = typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    cmdContainer->getIndirectHeap(HeapType::DYNAMIC_STATE)->align(HardwareCommandsHelper<FamilyType>::alignInterfaceDescriptorData);
    cmdContainer->setIddBlock(cmdContainer->getHeapSpaceAllowGrow(HeapType::DYNAMIC_STATE, sizeof(INTERFACE_DESCRIPTOR_DATA) * cmdContainer->getNumIddPerBlock()));
    cmdContainer->nextIddInBlock = 0;

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, pDevice, NEO::PreemptionMode::Disabled);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itorPC = find<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(commands.begin(), commands.end());
    ASSERT_NE(itorPC, commands.end());
}