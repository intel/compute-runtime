/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/kernel/kernel_descriptor_from_patchtokens.h"
#include "shared/test/unit_test/cmd_parse/gen_cmd_parse.h"
#include "shared/test/unit_test/device_binary_format/patchtokens_tests.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/mocks/mock_device.h"
#include "shared/test/unit_test/mocks/mock_dispatch_kernel_encoder_interface.h"

#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/test/unit_test/gen_common/matchers.h"
#include "test.h"

#include "hw_cmds.h"

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
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint32_t slmTotalSize = 1;
    EXPECT_CALL(*dispatchInterface.get(), getSlmTotalSize()).WillRepeatedly(::testing::Return(slmTotalSize));
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, pDevice, NEO::PreemptionMode::Disabled);

    auto interfaceDescriptorData = static_cast<INTERFACE_DESCRIPTOR_DATA *>(cmdContainer->getIddBlock());

    uint32_t expectedValue = static_cast<typename INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE>(
        HwHelperHw<FamilyType>::get().computeSlmValues(slmTotalSize));

    EXPECT_EQ(expectedValue, interfaceDescriptorData->getSharedLocalMemorySize());
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandEncodeStatesTest, givenSlmTotalSizeEqualZeroWhenDispatchingKernelThenSharedMemorySizeSetCorrectly) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint32_t slmTotalSize = 0;
    EXPECT_CALL(*dispatchInterface.get(), getSlmTotalSize()).WillRepeatedly(::testing::Return(slmTotalSize));
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, pDevice, NEO::PreemptionMode::Disabled);

    auto interfaceDescriptorData = static_cast<INTERFACE_DESCRIPTOR_DATA *>(cmdContainer->getIddBlock());

    uint32_t expectedValue = INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_ENCODES_0K;

    EXPECT_EQ(expectedValue, interfaceDescriptorData->getSharedLocalMemorySize());
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandEncodeStatesTest, givenOneBindingTableEntryWhenDispatchingKernelThenBindingTableOffsetIsCorrect) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    uint32_t numBindingTable = 1;
    BINDING_TABLE_STATE bindingTableState;
    bindingTableState.sInit();

    auto ssh = cmdContainer->getIndirectHeap(HeapType::SURFACE_STATE);
    size_t sizeUsed = 0x20;
    ssh->getSpace(sizeUsed);
    sizeUsed = ssh->getUsed();

    auto expectedOffset = alignUp(sizeUsed, BINDING_TABLE_STATE::SURFACESTATEPOINTER_ALIGN_SIZE);

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.numEntries = numBindingTable;
    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.tableOffset = 0U;
    const uint8_t *sshData = reinterpret_cast<uint8_t *>(&bindingTableState);
    EXPECT_CALL(*dispatchInterface.get(), getSurfaceStateHeapData()).WillRepeatedly(::testing::Return(sshData));
    EXPECT_CALL(*dispatchInterface.get(), getSurfaceStateHeapDataSize()).WillRepeatedly(::testing::Return(static_cast<uint32_t>(sizeof(BINDING_TABLE_STATE))));

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
    size_t sizeUsed = 0x20;
    ssh->getSpace(sizeUsed);
    sizeUsed = ssh->getUsed();

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.numEntries = numBindingTable;
    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.tableOffset = 0U;
    const uint8_t *sshData = reinterpret_cast<uint8_t *>(&bindingTableState);
    EXPECT_CALL(*dispatchInterface.get(), getSurfaceStateHeapData()).WillRepeatedly(::testing::Return(sshData));
    EXPECT_CALL(*dispatchInterface.get(), getSurfaceStateHeapDataSize()).WillRepeatedly(::testing::Return(static_cast<uint32_t>(sizeof(BINDING_TABLE_STATE))));

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

    dispatchInterface->kernelDescriptor.payloadMappings.samplerTable.numSamplers = numSamplers;
    dispatchInterface->kernelDescriptor.payloadMappings.samplerTable.tableOffset = 0U;
    dispatchInterface->kernelDescriptor.payloadMappings.samplerTable.borderColor = 0U;
    const uint8_t *dshData = reinterpret_cast<uint8_t *>(&samplerState);
    EXPECT_CALL(*dispatchInterface.get(), getDynamicStateHeapData()).WillRepeatedly(::testing::Return(dshData));

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

    dispatchInterface->kernelDescriptor.payloadMappings.samplerTable.numSamplers = numSamplers;
    dispatchInterface->kernelDescriptor.payloadMappings.samplerTable.tableOffset = 0U;
    dispatchInterface->kernelDescriptor.payloadMappings.samplerTable.borderColor = 0U;
    const uint8_t *dshData = reinterpret_cast<uint8_t *>(&samplerState);
    EXPECT_CALL(*dispatchInterface.get(), getDynamicStateHeapData()).WillRepeatedly(::testing::Return(dshData));

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, pDevice, NEO::PreemptionMode::Disabled);
    auto interfaceDescriptorData = static_cast<INTERFACE_DESCRIPTOR_DATA *>(cmdContainer->getIddBlock());

    auto borderColorOffsetInDsh = usedBefore;
    samplerState.setIndirectStatePointer(static_cast<uint32_t>(borderColorOffsetInDsh));

    auto samplerStateOffset = interfaceDescriptorData->getSamplerStatePointer();

    auto pSmplr = reinterpret_cast<SAMPLER_STATE *>(ptrOffset(dsh->getCpuBase(), samplerStateOffset));
    EXPECT_NE(memcmp(pSmplr, &samplerState, sizeof(SAMPLER_STATE)), 0);
}

HWTEST_F(CommandEncodeStatesTest, givenIndirectOffsetsCountsWhenDispatchingKernelThenCorrestMIStoreOffsetsSet) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
    uint32_t dims[] = {2, 1, 1};
    uint32_t offsets[] = {0x10, 0x20, 0x30};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->kernelDescriptor.payloadMappings.dispatchTraits.numWorkGroups[0] = offsets[0];
    dispatchInterface->kernelDescriptor.payloadMappings.dispatchTraits.numWorkGroups[1] = offsets[1];
    dispatchInterface->kernelDescriptor.payloadMappings.dispatchTraits.numWorkGroups[2] = offsets[2];
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

    EXPECT_CALL(*dispatchInterface.get(), getGroupSize()).WillRepeatedly(::testing::Return(lws));
    dispatchInterface->kernelDescriptor.payloadMappings.dispatchTraits.globalWorkSize[0] = offsets[0];
    dispatchInterface->kernelDescriptor.payloadMappings.dispatchTraits.globalWorkSize[1] = offsets[1];
    dispatchInterface->kernelDescriptor.payloadMappings.dispatchTraits.globalWorkSize[2] = offsets[2];
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

    if (HardwareCommandsHelper<FamilyType>::isPipeControlPriorToPipelineSelectWArequired(pDevice->getHardwareInfo())) {
        auto itorPC = findAll<PIPE_CONTROL *>(commands.begin(), commands.end());
        EXPECT_EQ(2u, itorPC.size());
    } else {
        auto itorPC = find<PIPE_CONTROL *>(commands.begin(), commands.end());
        ASSERT_EQ(itorPC, commands.end());
    }
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

HWCMDTEST_F(IGFX_GEN8_CORE, CommandEncodeStatesTest, givenDirtyHeapsWhenDispatchKernelThenPCIsAddedBeforeSBA) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    cmdContainer->slmSize = 1;
    EXPECT_CALL(*dispatchInterface.get(), getSlmTotalSize()).WillRepeatedly(::testing::Return(cmdContainer->slmSize));
    cmdContainer->setDirtyStateForAllHeaps(true);

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, pDevice, NEO::PreemptionMode::Disabled);

    GenCmdList cmdList;
    CmdParse<FamilyType>::parseCommandBuffer(cmdList, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = reverse_find<STATE_BASE_ADDRESS *>(cmdList.rbegin(), cmdList.rend());
    ASSERT_NE(cmdList.rend(), itor);

    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*itor);
    EXPECT_NE(nullptr, cmdSba);

    auto itorPc = reverse_find<PIPE_CONTROL *>(itor, cmdList.rend());
    ASSERT_NE(cmdList.rend(), itorPc);

    bool foundPcWithDCFlush = false;

    do {
        auto cmdPc = genCmdCast<PIPE_CONTROL *>(*itorPc);
        if (cmdPc && cmdPc->getDcFlushEnable()) {
            foundPcWithDCFlush = true;
            break;
        }
    } while (++itorPc != cmdList.rend());

    EXPECT_TRUE(foundPcWithDCFlush);
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

using EncodeDispatchKernelTest = Test<CommandEncodeStatesFixture>;

HWTEST_F(EncodeDispatchKernelTest, givenBindlessBufferArgWhenDispatchingKernelThenSurfaceStateOffsetInCrossThreadDataIsProgrammed) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    using DataPortBindlessSurfaceExtendedMessageDescriptor = typename FamilyType::DataPortBindlessSurfaceExtendedMessageDescriptor;
    uint32_t numBindingTable = 1;
    BINDING_TABLE_STATE bindingTableState;
    bindingTableState.sInit();

    auto ssh = cmdContainer->getIndirectHeap(HeapType::SURFACE_STATE);
    auto ioh = cmdContainer->getIndirectHeap(HeapType::INDIRECT_OBJECT);

    size_t sizeUsed = 0x20;
    ssh->getSpace(sizeUsed);
    sizeUsed = ssh->getUsed();

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    std::vector<uint8_t> storage;
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens = PatchTokensTestData::ValidEmptyKernel::create(storage);
    kernelTokens.tokens.kernelArgs.resize(1);
    kernelTokens.tokens.kernelArgs[0].objectType = NEO::PatchTokenBinary::ArgObjectType::Buffer;

    const uint32_t iohOffset = dispatchInterface->getCrossThreadDataSize() + 4;
    const uint32_t surfaceStateOffset = 128;
    iOpenCL::SPatchStatelessGlobalMemoryObjectKernelArgument globalMemArg = {};
    globalMemArg.Token = iOpenCL::PATCH_TOKEN_STATELESS_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT;
    globalMemArg.ArgumentNumber = 0;
    globalMemArg.DataParamOffset = iohOffset;
    globalMemArg.DataParamSize = 4;
    globalMemArg.SurfaceStateHeapOffset = surfaceStateOffset;

    auto surfaceStateOffsetOnHeap = static_cast<uint32_t>(alignUp(sizeUsed, BINDING_TABLE_STATE::SURFACESTATEPOINTER_ALIGN_SIZE)) + surfaceStateOffset;
    auto patchLocation = reinterpret_cast<uint32_t *>(ptrOffset(ioh->getCpuBase(), iohOffset));
    *patchLocation = 0xdead;

    kernelTokens.tokens.kernelArgs[0].objectArg = &globalMemArg;

    NEO::populateKernelDescriptor(dispatchInterface->kernelDescriptor, kernelTokens, sizeof(void *));

    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.numEntries = numBindingTable;
    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.tableOffset = 0U;

    auto &arg = dispatchInterface->kernelDescriptor.payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>();
    arg.bindless = iohOffset;
    arg.bindful = surfaceStateOffset;

    const uint8_t *sshData = reinterpret_cast<uint8_t *>(&bindingTableState);
    EXPECT_CALL(*dispatchInterface.get(), getSurfaceStateHeapData()).WillRepeatedly(::testing::Return(sshData));
    EXPECT_CALL(*dispatchInterface.get(), getSurfaceStateHeapDataSize()).WillRepeatedly(::testing::Return(static_cast<uint32_t>(sizeof(BINDING_TABLE_STATE))));

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, pDevice, NEO::PreemptionMode::Disabled);

    DataPortBindlessSurfaceExtendedMessageDescriptor extMessageDesc;
    extMessageDesc.setBindlessSurfaceOffset(surfaceStateOffsetOnHeap);

    auto expectedOffset = extMessageDesc.getBindlessSurfaceOffsetToPatch();
    EXPECT_EQ(expectedOffset, *patchLocation);
}

HWTEST_F(EncodeDispatchKernelTest, givenBindlessImageArgWhenDispatchingKernelThenSurfaceStateOffsetInCrossThreadDataIsProgrammed) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    using DataPortBindlessSurfaceExtendedMessageDescriptor = typename FamilyType::DataPortBindlessSurfaceExtendedMessageDescriptor;
    uint32_t numBindingTable = 1;
    BINDING_TABLE_STATE bindingTableState;
    bindingTableState.sInit();

    auto ssh = cmdContainer->getIndirectHeap(HeapType::SURFACE_STATE);
    auto ioh = cmdContainer->getIndirectHeap(HeapType::INDIRECT_OBJECT);

    size_t sizeUsed = 0x20;
    ssh->getSpace(sizeUsed);
    sizeUsed = ssh->getUsed();

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    std::vector<uint8_t> storage;
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens = PatchTokensTestData::ValidEmptyKernel::create(storage);
    kernelTokens.tokens.kernelArgs.resize(1);
    kernelTokens.tokens.kernelArgs[0].objectType = NEO::PatchTokenBinary::ArgObjectType::Image;

    const uint32_t iohOffset = dispatchInterface->getCrossThreadDataSize() + 4;
    const uint32_t surfaceStateOffset = 128;

    iOpenCL::SPatchImageMemoryObjectKernelArgument imageArg = {};
    imageArg.Token = iOpenCL::PATCH_TOKEN_IMAGE_MEMORY_OBJECT_KERNEL_ARGUMENT;
    imageArg.ArgumentNumber = 0;
    imageArg.Offset = surfaceStateOffset;

    auto surfaceStateOffsetOnHeap = static_cast<uint32_t>(alignUp(sizeUsed, BINDING_TABLE_STATE::SURFACESTATEPOINTER_ALIGN_SIZE)) + surfaceStateOffset;
    auto patchLocation = reinterpret_cast<uint32_t *>(ptrOffset(ioh->getCpuBase(), iohOffset));
    *patchLocation = 0xdead;

    kernelTokens.tokens.kernelArgs[0].objectArg = &imageArg;

    NEO::populateKernelDescriptor(dispatchInterface->kernelDescriptor, kernelTokens, sizeof(void *));

    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.numEntries = numBindingTable;
    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.tableOffset = 0U;

    auto &arg = dispatchInterface->kernelDescriptor.payloadMappings.explicitArgs[0].as<NEO::ArgDescImage>();
    arg.bindless = iohOffset;
    arg.bindful = surfaceStateOffset;

    const uint8_t *sshData = reinterpret_cast<uint8_t *>(&bindingTableState);
    EXPECT_CALL(*dispatchInterface.get(), getSurfaceStateHeapData()).WillRepeatedly(::testing::Return(sshData));
    EXPECT_CALL(*dispatchInterface.get(), getSurfaceStateHeapDataSize()).WillRepeatedly(::testing::Return(static_cast<uint32_t>(sizeof(BINDING_TABLE_STATE))));

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, pDevice, NEO::PreemptionMode::Disabled);

    DataPortBindlessSurfaceExtendedMessageDescriptor extMessageDesc;
    extMessageDesc.setBindlessSurfaceOffset(surfaceStateOffsetOnHeap);

    auto expectedOffset = extMessageDesc.getBindlessSurfaceOffsetToPatch();
    EXPECT_EQ(expectedOffset, *patchLocation);
}

HWTEST_F(EncodeDispatchKernelTest, givenNonBindlessOrStatelessArgWhenDispatchingKernelThenSurfaceStateOffsetInCrossThreadDataIsNotPatched) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    using DataPortBindlessSurfaceExtendedMessageDescriptor = typename FamilyType::DataPortBindlessSurfaceExtendedMessageDescriptor;
    uint32_t numBindingTable = 1;
    BINDING_TABLE_STATE bindingTableState;
    bindingTableState.sInit();

    auto ssh = cmdContainer->getIndirectHeap(HeapType::SURFACE_STATE);
    auto ioh = cmdContainer->getIndirectHeap(HeapType::INDIRECT_OBJECT);

    size_t sizeUsed = 0x20;
    ssh->getSpace(sizeUsed);
    sizeUsed = ssh->getUsed();

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    std::vector<uint8_t> storage;
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens = PatchTokensTestData::ValidEmptyKernel::create(storage);
    kernelTokens.tokens.kernelArgs.resize(1);
    kernelTokens.tokens.kernelArgs[0].objectType = NEO::PatchTokenBinary::ArgObjectType::Buffer;

    const uint32_t iohOffset = dispatchInterface->getCrossThreadDataSize() + 4;
    const uint32_t surfaceStateOffset = 128;
    iOpenCL::SPatchStatelessGlobalMemoryObjectKernelArgument globalMemArg = {};
    globalMemArg.Token = iOpenCL::PATCH_TOKEN_STATELESS_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT;
    globalMemArg.ArgumentNumber = 0;
    globalMemArg.DataParamOffset = iohOffset;
    globalMemArg.DataParamSize = 4;
    globalMemArg.SurfaceStateHeapOffset = surfaceStateOffset;

    auto patchLocation = reinterpret_cast<uint32_t *>(ptrOffset(ioh->getCpuBase(), iohOffset));
    const uint32_t pattern = 0xdeadu;
    *patchLocation = pattern;

    kernelTokens.tokens.kernelArgs[0].objectArg = &globalMemArg;

    NEO::populateKernelDescriptor(dispatchInterface->kernelDescriptor, kernelTokens, sizeof(void *));

    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.numEntries = numBindingTable;
    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.tableOffset = 0U;

    auto &arg = dispatchInterface->kernelDescriptor.payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>();
    arg.bindless = NEO::undefined<CrossThreadDataOffset>;
    arg.bindful = surfaceStateOffset;

    const uint8_t *sshData = reinterpret_cast<uint8_t *>(&bindingTableState);
    EXPECT_CALL(*dispatchInterface.get(), getSurfaceStateHeapData()).WillRepeatedly(::testing::Return(sshData));
    EXPECT_CALL(*dispatchInterface.get(), getSurfaceStateHeapDataSize()).WillRepeatedly(::testing::Return(static_cast<uint32_t>(sizeof(BINDING_TABLE_STATE))));

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, pDevice, NEO::PreemptionMode::Disabled);
    EXPECT_EQ(pattern, *patchLocation);

    iOpenCL::SPatchSamplerKernelArgument samplerArg = {};
    samplerArg.Token = iOpenCL::PATCH_TOKEN_SAMPLER_KERNEL_ARGUMENT;
    samplerArg.ArgumentNumber = 1;
    samplerArg.Offset = surfaceStateOffset;
    samplerArg.Type = iOpenCL::SAMPLER_OBJECT_TEXTURE;
    kernelTokens.tokens.kernelArgs[0].objectArg = &samplerArg;
    kernelTokens.tokens.kernelArgs[0].objectType = NEO::PatchTokenBinary::ArgObjectType::Sampler;

    dispatchInterface.reset(new MockDispatchKernelEncoder());

    NEO::populateKernelDescriptor(dispatchInterface->kernelDescriptor, kernelTokens, sizeof(void *));

    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.numEntries = numBindingTable;
    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.tableOffset = 0U;

    sshData = reinterpret_cast<uint8_t *>(&bindingTableState);
    EXPECT_CALL(*dispatchInterface.get(), getSurfaceStateHeapData()).WillRepeatedly(::testing::Return(sshData));
    EXPECT_CALL(*dispatchInterface.get(), getSurfaceStateHeapDataSize()).WillRepeatedly(::testing::Return(static_cast<uint32_t>(sizeof(BINDING_TABLE_STATE))));

    ioh->replaceBuffer(ioh->getCpuBase(), ioh->getMaxAvailableSpace());
    memset(ioh->getCpuBase(), 0, ioh->getMaxAvailableSpace());
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, pDevice, NEO::PreemptionMode::Disabled);
    EXPECT_THAT(ptrOffset(ioh->getCpuBase(), iohOffset), MemoryZeroed(ioh->getMaxAvailableSpace() - iohOffset));
}

HWCMDTEST_F(IGFX_GEN8_CORE, WalkerThreadTest, givenStartWorkGroupWhenIndirectIsFalseThenExpectStartGroupAndThreadDimensionsProgramming) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

    WALKER_TYPE walkerCmd = FamilyType::cmdInitGpgpuWalker;
    startWorkGroup[1] = 2u;
    startWorkGroup[2] = 3u;

    EncodeDispatchKernel<FamilyType>::encodeThreadData(walkerCmd, startWorkGroup, numWorkGroups, workGroupSizes, simd, localIdDimensions,
                                                       0, 0, true, false, false, requiredWorkGroupOrder);
    EXPECT_FALSE(walkerCmd.getIndirectParameterEnable());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdXDimension());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdYDimension());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdZDimension());

    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingX());
    EXPECT_EQ(2u, walkerCmd.getThreadGroupIdStartingY());
    EXPECT_EQ(3u, walkerCmd.getThreadGroupIdStartingResumeZ());

    auto expectedSimd = getSimdConfig<WALKER_TYPE>(simd);
    EXPECT_EQ(expectedSimd, walkerCmd.getSimdSize());
    EXPECT_EQ(1u, walkerCmd.getThreadWidthCounterMaximum());

    EXPECT_EQ(0xffffffffu, walkerCmd.getRightExecutionMask());
    EXPECT_EQ(0xffffffffu, walkerCmd.getBottomExecutionMask());
}

HWCMDTEST_F(IGFX_GEN8_CORE, WalkerThreadTest, givenNoStartWorkGroupWhenIndirectIsTrueThenExpectNoStartGroupAndThreadDimensionsProgramming) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

    WALKER_TYPE walkerCmd = FamilyType::cmdInitGpgpuWalker;
    startWorkGroup[1] = 2u;
    startWorkGroup[2] = 3u;

    EncodeDispatchKernel<FamilyType>::encodeThreadData(walkerCmd, nullptr, numWorkGroups, workGroupSizes, simd, localIdDimensions,
                                                       0, 0, true, false, true, requiredWorkGroupOrder);
    EXPECT_TRUE(walkerCmd.getIndirectParameterEnable());
    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdXDimension());
    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdYDimension());
    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdZDimension());

    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingX());
    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingY());
    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingResumeZ());

    auto expectedSimd = getSimdConfig<WALKER_TYPE>(simd);
    EXPECT_EQ(expectedSimd, walkerCmd.getSimdSize());
    EXPECT_EQ(1u, walkerCmd.getThreadWidthCounterMaximum());

    EXPECT_EQ(0xffffffffu, walkerCmd.getRightExecutionMask());
    EXPECT_EQ(0xffffffffu, walkerCmd.getBottomExecutionMask());
}

HWCMDTEST_F(IGFX_GEN8_CORE, WalkerThreadTest, givenStartWorkGroupWhenWorkGroupSmallerThanSimdThenExpectStartGroupAndRightExecutionMaskNotFull) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

    WALKER_TYPE walkerCmd = FamilyType::cmdInitGpgpuWalker;
    startWorkGroup[1] = 2u;
    startWorkGroup[2] = 3u;
    workGroupSizes[0] = 30u;

    EncodeDispatchKernel<FamilyType>::encodeThreadData(walkerCmd, startWorkGroup, numWorkGroups, workGroupSizes, simd, localIdDimensions,
                                                       0, 0, true, false, false, requiredWorkGroupOrder);
    EXPECT_FALSE(walkerCmd.getIndirectParameterEnable());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdXDimension());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdYDimension());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdZDimension());

    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingX());
    EXPECT_EQ(2u, walkerCmd.getThreadGroupIdStartingY());
    EXPECT_EQ(3u, walkerCmd.getThreadGroupIdStartingResumeZ());

    auto expectedSimd = getSimdConfig<WALKER_TYPE>(simd);
    EXPECT_EQ(expectedSimd, walkerCmd.getSimdSize());
    EXPECT_EQ(1u, walkerCmd.getThreadWidthCounterMaximum());

    EXPECT_EQ(0x3fffffffu, walkerCmd.getRightExecutionMask());
    EXPECT_EQ(0xffffffffu, walkerCmd.getBottomExecutionMask());
}

HWCMDTEST_F(IGFX_GEN8_CORE, WalkerThreadTest, WhenThreadPerThreadGroupNotZeroThenExpectOverrideThreadGroupCalculation) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

    WALKER_TYPE walkerCmd = FamilyType::cmdInitGpgpuWalker;

    uint32_t expectedThreadPerThreadGroup = 5u;
    EncodeDispatchKernel<FamilyType>::encodeThreadData(walkerCmd, startWorkGroup, numWorkGroups, workGroupSizes, simd, localIdDimensions,
                                                       expectedThreadPerThreadGroup, 0, true, false, false, requiredWorkGroupOrder);
    EXPECT_FALSE(walkerCmd.getIndirectParameterEnable());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdXDimension());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdYDimension());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdZDimension());

    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingX());
    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingY());
    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingResumeZ());

    auto expectedSimd = getSimdConfig<WALKER_TYPE>(simd);
    EXPECT_EQ(expectedSimd, walkerCmd.getSimdSize());
    EXPECT_EQ(expectedThreadPerThreadGroup, walkerCmd.getThreadWidthCounterMaximum());

    EXPECT_EQ(0xffffffffu, walkerCmd.getRightExecutionMask());
    EXPECT_EQ(0xffffffffu, walkerCmd.getBottomExecutionMask());
}

HWCMDTEST_F(IGFX_GEN8_CORE, WalkerThreadTest, WhenExecutionMaskNotZeroThenExpectOverrideExecutionMaskCalculation) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

    WALKER_TYPE walkerCmd = FamilyType::cmdInitGpgpuWalker;

    uint32_t expectedExecutionMask = 0xFFFFu;
    EncodeDispatchKernel<FamilyType>::encodeThreadData(walkerCmd, startWorkGroup, numWorkGroups, workGroupSizes, simd, localIdDimensions,
                                                       0, expectedExecutionMask, true, false, false, requiredWorkGroupOrder);
    EXPECT_FALSE(walkerCmd.getIndirectParameterEnable());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdXDimension());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdYDimension());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdZDimension());

    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingX());
    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingY());
    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingResumeZ());

    auto expectedSimd = getSimdConfig<WALKER_TYPE>(simd);
    EXPECT_EQ(expectedSimd, walkerCmd.getSimdSize());
    EXPECT_EQ(1u, walkerCmd.getThreadWidthCounterMaximum());

    EXPECT_EQ(expectedExecutionMask, walkerCmd.getRightExecutionMask());
    EXPECT_EQ(0xffffffffu, walkerCmd.getBottomExecutionMask());
}

HWTEST_F(WalkerThreadTest, givenDefaultDebugFlagWhenKernelDescriptorInlineDataDisabledThenReturnInlineNotRequired) {
    NEO::KernelDescriptor kernelDesc;
    kernelDesc.kernelAttributes.flags.passInlineData = false;

    EXPECT_FALSE(EncodeDispatchKernel<FamilyType>::inlineDataProgrammingRequired(kernelDesc));
}

HWTEST_F(WalkerThreadTest, givenDefaultDebugFlagWhenKernelDescriptorInlineDataEnabledThenReturnInlineRequired) {
    NEO::KernelDescriptor kernelDesc;
    kernelDesc.kernelAttributes.flags.passInlineData = true;

    EXPECT_TRUE(EncodeDispatchKernel<FamilyType>::inlineDataProgrammingRequired(kernelDesc));
}

HWTEST_F(WalkerThreadTest, givenDebugFlagDisabledWhenKernelDescriptorInlineDataEnabledThenReturnInlineNotRequired) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnablePassInlineData.set(0);

    NEO::KernelDescriptor kernelDesc;
    kernelDesc.kernelAttributes.flags.passInlineData = true;

    EXPECT_FALSE(EncodeDispatchKernel<FamilyType>::inlineDataProgrammingRequired(kernelDesc));
}

HWTEST_F(WalkerThreadTest, givenDebugFlagEnabledWhenKernelDescriptorInlineDataEnabledThenReturnInlineRequired) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnablePassInlineData.set(1);

    NEO::KernelDescriptor kernelDesc;
    kernelDesc.kernelAttributes.flags.passInlineData = true;

    EXPECT_TRUE(EncodeDispatchKernel<FamilyType>::inlineDataProgrammingRequired(kernelDesc));
}

HWTEST_F(WalkerThreadTest, givenDebugFlagEnabledWhenKernelDescriptorInlineDataDisabledThenReturnInlineNotRequired) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnablePassInlineData.set(1);

    NEO::KernelDescriptor kernelDesc;
    kernelDesc.kernelAttributes.flags.passInlineData = false;

    EXPECT_FALSE(EncodeDispatchKernel<FamilyType>::inlineDataProgrammingRequired(kernelDesc));
}

using namespace NEO;

using InterfaceDescriptorDataTests = ::testing::Test;

HWCMDTEST_F(IGFX_GEN8_CORE, InterfaceDescriptorDataTests, givenVariousValuesWhenCallingSetBarrierEnableThenCorrectValueIsSet) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    INTERFACE_DESCRIPTOR_DATA idd = FamilyType::cmdInitInterfaceDescriptorData;
    MockDevice device;
    auto hwInfo = device.getHardwareInfo();

    EncodeDispatchKernel<FamilyType>::programBarrierEnable(idd, 0, hwInfo);
    EXPECT_FALSE(idd.getBarrierEnable());

    EncodeDispatchKernel<FamilyType>::programBarrierEnable(idd, 1, hwInfo);
    EXPECT_TRUE(idd.getBarrierEnable());

    EncodeDispatchKernel<FamilyType>::programBarrierEnable(idd, 2, hwInfo);
    EXPECT_TRUE(idd.getBarrierEnable());
}
