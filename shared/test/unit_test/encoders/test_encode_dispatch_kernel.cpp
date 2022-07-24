/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/kernel/kernel_descriptor_from_patchtokens.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/fixtures/command_container_fixture.h"
#include "shared/test/common/fixtures/front_window_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_dispatch_kernel_encoder_interface.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/device_binary_format/patchtokens_tests.h"
#include "shared/test/unit_test/helpers/gtest_helpers.h"

using namespace NEO;

using CommandEncodeStatesTest = Test<CommandEncodeStatesFixture>;

TEST_F(CommandEncodeStatesTest, givenDefaultCommandContainerWhenGettingNumIddPerBlockThen64IsReturned) {
    auto numIdds = cmdContainer->getNumIddPerBlock();
    EXPECT_EQ(64u, numIdds);
}

TEST_F(CommandEncodeStatesTest, givenCommandConatinerCreatedWithMaxNumAggregateIddThenVerifyGetNumIddsInBlockIsCorrect) {
    auto cmdContainer = new CommandContainer(1);
    auto numIdds = cmdContainer->getNumIddPerBlock();

    EXPECT_EQ(1u, numIdds);

    delete cmdContainer;
}

HWTEST_F(CommandEncodeStatesTest, givenDispatchInterfaceWhenDispatchKernelThenWalkerCommandProgrammed) {
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    auto itorPC = find<WALKER_TYPE *>(commands.begin(), commands.end());
    ASSERT_NE(itorPC, commands.end());
}

using CommandEncodeStatesUncachedMocsTests = Test<CommandEncodeStatesFixture>;

HWTEST_F(CommandEncodeStatesUncachedMocsTests, whenEncodingDispatchKernelWithUncachedMocsAndDirtyHeapsThenCorrectMocsIsSet) {
    DebugManagerStateRestore restore;
    DebugManager.flags.ForceL1Caching.set(0u);

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    cmdContainer->setDirtyStateForAllHeaps(true);
    bool requiresUncachedMocs = true;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);
    EXPECT_FALSE(dispatchArgs.requiresUncachedMocs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands,
                                             ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0),
                                             cmdContainer->getCommandStream()->getUsed());

    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto itor = find<STATE_BASE_ADDRESS *>(commands.begin(), commands.end());
    ASSERT_NE(commands.end(), itor);

    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*itor);
    auto gmmHelper = cmdContainer->getDevice()->getGmmHelper();
    EXPECT_EQ(cmdSba->getStatelessDataPortAccessMemoryObjectControlState(),
              (gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED)));
}

HWTEST_F(CommandEncodeStatesUncachedMocsTests, whenEncodingDispatchKernelWithUncachedMocsAndNonDirtyHeapsThenCorrectMocsIsSet) {
    DebugManagerStateRestore restore;
    DebugManager.flags.ForceL1Caching.set(0u);

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    cmdContainer->setDirtyStateForAllHeaps(false);
    bool requiresUncachedMocs = true;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);
    EXPECT_FALSE(dispatchArgs.requiresUncachedMocs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands,
                                             ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0),
                                             cmdContainer->getCommandStream()->getUsed());

    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto itor = find<STATE_BASE_ADDRESS *>(commands.begin(), commands.end());
    ASSERT_NE(commands.end(), itor);

    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*itor);
    auto gmmHelper = cmdContainer->getDevice()->getGmmHelper();
    EXPECT_EQ(cmdSba->getStatelessDataPortAccessMemoryObjectControlState(),
              (gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED)));
}

HWTEST_F(CommandEncodeStatesUncachedMocsTests, whenEncodingDispatchKernelWithNonUncachedMocsAndDirtyHeapsThenSbaIsNotProgrammed) {
    DebugManagerStateRestore restore;
    DebugManager.flags.ForceL1Caching.set(0u);

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    cmdContainer->setDirtyStateForAllHeaps(true);
    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);
    EXPECT_FALSE(dispatchArgs.requiresUncachedMocs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands,
                                             ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0),
                                             cmdContainer->getCommandStream()->getUsed());

    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto itor = find<STATE_BASE_ADDRESS *>(commands.begin(), commands.end());
    ASSERT_NE(commands.end(), itor);

    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*itor);
    auto gmmHelper = cmdContainer->getDevice()->getGmmHelper();
    EXPECT_EQ(cmdSba->getStatelessDataPortAccessMemoryObjectControlState(),
              (gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER)));
}

HWTEST_F(CommandEncodeStatesUncachedMocsTests, whenEncodingDispatchKernelWithNonUncachedMocsAndNonDirtyHeapsThenSbaIsNotProgrammed) {
    DebugManagerStateRestore restore;
    DebugManager.flags.ForceL1Caching.set(0u);

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    cmdContainer->setDirtyStateForAllHeaps(false);
    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);
    EXPECT_FALSE(dispatchArgs.requiresUncachedMocs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands,
                                             ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0),
                                             cmdContainer->getCommandStream()->getUsed());

    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto itor = find<STATE_BASE_ADDRESS *>(commands.begin(), commands.end());
    ASSERT_EQ(commands.end(), itor);
}

HWTEST_F(CommandEncodeStatesUncachedMocsTests, whenEncodingDispatchKernelWithNonUncachedMocsAndNonDirtyHeapsAndSlmSizeThenSbaIsNotProgrammed) {
    DebugManagerStateRestore restore;
    DebugManager.flags.ForceL1Caching.set(0u);

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint32_t slmTotalSize = 1;

    dispatchInterface->getSlmTotalSizeResult = slmTotalSize;

    cmdContainer->setDirtyStateForAllHeaps(false);
    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);
    EXPECT_FALSE(dispatchArgs.requiresUncachedMocs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands,
                                             ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0),
                                             cmdContainer->getCommandStream()->getUsed());

    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto itor = find<STATE_BASE_ADDRESS *>(commands.begin(), commands.end());
    ASSERT_EQ(commands.end(), itor);
}

HWTEST_F(CommandEncodeStatesTest, givenCommandContainerWithUsedAvailableSizeWhenDispatchKernelThenNextCommandBufferIsAdded) {
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    auto cmdBuffersCountBefore = cmdContainer->getCmdBufferAllocations().size();

    cmdContainer->getCommandStream()->getSpace(cmdContainer->getCommandStream()->getAvailableSpace() - sizeof(typename FamilyType::MI_BATCH_BUFFER_END));

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    auto cmdBuffersCountAfter = cmdContainer->getCmdBufferAllocations().size();

    EXPECT_GT(cmdBuffersCountAfter, cmdBuffersCountBefore);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandEncodeStatesTest, givenSlmTotalSizeGraterThanZeroWhenDispatchingKernelThenSharedMemorySizeSetCorrectly) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint32_t slmTotalSize = 1;

    dispatchInterface->getSlmTotalSizeResult = slmTotalSize;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    auto interfaceDescriptorData = static_cast<INTERFACE_DESCRIPTOR_DATA *>(cmdContainer->getIddBlock());

    uint32_t expectedValue = static_cast<typename INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE>(
        HwHelperHw<FamilyType>::get().computeSlmValues(pDevice->getHardwareInfo(), slmTotalSize));

    EXPECT_EQ(expectedValue, interfaceDescriptorData->getSharedLocalMemorySize());
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandEncodeStatesTest, givenSlmTotalSizeEqualZeroWhenDispatchingKernelThenSharedMemorySizeSetCorrectly) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint32_t slmTotalSize = 0;

    dispatchInterface->getSlmTotalSizeResult = slmTotalSize;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    auto interfaceDescriptorData = static_cast<INTERFACE_DESCRIPTOR_DATA *>(cmdContainer->getIddBlock());

    uint32_t expectedValue = INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_ENCODES_0K;

    EXPECT_EQ(expectedValue, interfaceDescriptorData->getSharedLocalMemorySize());
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandEncodeStatesTest, givenOneBindingTableEntryWhenDispatchingKernelThenBindingTableOffsetIsCorrect) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    uint32_t numBindingTable = 1;
    BINDING_TABLE_STATE bindingTableState = FamilyType::cmdInitBindingTableState;

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
    dispatchInterface->getSurfaceStateHeapDataResult = const_cast<uint8_t *>(sshData);
    dispatchInterface->getSurfaceStateHeapDataSizeResult = static_cast<uint32_t>(sizeof(BINDING_TABLE_STATE));

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    auto interfaceDescriptorData = static_cast<INTERFACE_DESCRIPTOR_DATA *>(cmdContainer->getIddBlock());

    EXPECT_EQ(interfaceDescriptorData->getBindingTablePointer(), expectedOffset);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandEncodeStatesTest, giveNumBindingTableZeroWhenDispatchingKernelThenBindingTableOffsetIsZero) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    uint32_t numBindingTable = 0;
    BINDING_TABLE_STATE bindingTableState = FamilyType::cmdInitBindingTableState;

    auto ssh = cmdContainer->getIndirectHeap(HeapType::SURFACE_STATE);
    size_t sizeUsed = 0x20;
    ssh->getSpace(sizeUsed);
    sizeUsed = ssh->getUsed();

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.numEntries = numBindingTable;
    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.tableOffset = 0U;
    const uint8_t *sshData = reinterpret_cast<uint8_t *>(&bindingTableState);
    dispatchInterface->getSurfaceStateHeapDataResult = const_cast<uint8_t *>(sshData);
    dispatchInterface->getSurfaceStateHeapDataSizeResult = static_cast<uint32_t>(sizeof(BINDING_TABLE_STATE));

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

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
    dispatchInterface->getDynamicStateHeapDataResult = const_cast<uint8_t *>(dshData);

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

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
    dispatchInterface->getDynamicStateHeapDataResult = const_cast<uint8_t *>(dshData);

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

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

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.isIndirect = true;

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = commands.begin();

    for (int i = 0; i < 3; i++) {
        ASSERT_NE(itor, commands.end());
        itor = find<MI_STORE_REGISTER_MEM *>(++itor, commands.end());
    }
}

HWTEST_F(CommandEncodeStatesTest, givenIndirectOffsetsSizeWhenDispatchingKernelThenMiMathEncoded) {
    using MI_MATH = typename FamilyType::MI_MATH;
    uint32_t dims[] = {2, 1, 1};
    uint32_t offsets[] = {0x10, 0x20, 0x30};
    uint32_t lws[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->getGroupSizeResult = lws;

    dispatchInterface->kernelDescriptor.payloadMappings.dispatchTraits.globalWorkSize[0] = offsets[0];
    dispatchInterface->kernelDescriptor.payloadMappings.dispatchTraits.globalWorkSize[1] = offsets[1];
    dispatchInterface->kernelDescriptor.payloadMappings.dispatchTraits.globalWorkSize[2] = offsets[2];

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.isIndirect = true;

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<MI_MATH *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandEncodeStatesTest, givenForceBtpPrefetchModeDebugFlagWhenDispatchingKernelThenValuesAreSetUpCorrectly) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using MEDIA_INTERFACE_DESCRIPTOR_LOAD = typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;

    DebugManagerStateRestore restorer;
    uint32_t dims[] = {2, 1, 1};
    uint32_t numBindingTable = 1;
    uint32_t numSamplers = 1;
    SAMPLER_STATE samplerState{};
    BINDING_TABLE_STATE bindingTable{};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.numEntries = numBindingTable;
    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.tableOffset = 0;
    dispatchInterface->kernelDescriptor.payloadMappings.samplerTable.numSamplers = numSamplers;
    dispatchInterface->kernelDescriptor.payloadMappings.samplerTable.tableOffset = 0;
    dispatchInterface->kernelDescriptor.payloadMappings.samplerTable.borderColor = 0;
    unsigned char *samplerStateRaw = reinterpret_cast<unsigned char *>(&samplerState);
    dispatchInterface->getDynamicStateHeapDataResult = const_cast<uint8_t *>(samplerStateRaw);
    unsigned char *bindingTableRaw = reinterpret_cast<unsigned char *>(&bindingTable);
    dispatchInterface->getSurfaceStateHeapDataResult = const_cast<uint8_t *>(bindingTableRaw);

    {
        DebugManager.flags.ForceBtpPrefetchMode.set(-1);
        cmdContainer.reset(new MyMockCommandContainer());
        cmdContainer->initialize(pDevice, nullptr, true);
        bool requiresUncachedMocs = false;
        EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

        EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

        auto dsh = cmdContainer->getIndirectHeap(HeapType::DYNAMIC_STATE);

        GenCmdList commands;
        CmdParse<FamilyType>::parseCommandBuffer(commands, cmdContainer->getCommandStream()->getCpuBase(), cmdContainer->getCommandStream()->getUsed());

        auto itorMIDL = find<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(commands.begin(), commands.end());
        EXPECT_NE(itorMIDL, commands.end());

        auto cmd = genCmdCast<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(*itorMIDL);
        EXPECT_NE(cmd, nullptr);

        auto idd = static_cast<INTERFACE_DESCRIPTOR_DATA *>(ptrOffset(dsh->getCpuBase(), cmd->getInterfaceDescriptorDataStartAddress()));

        if (EncodeSurfaceState<FamilyType>::doBindingTablePrefetch()) {
            EXPECT_EQ(numBindingTable, idd->getBindingTableEntryCount());
            EXPECT_EQ(static_cast<typename INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT>((numSamplers + 3) / 4), idd->getSamplerCount());
        } else {
            EXPECT_EQ(0u, idd->getBindingTableEntryCount());
            EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT_NO_SAMPLERS_USED, idd->getSamplerCount());
        }
    }

    {
        DebugManager.flags.ForceBtpPrefetchMode.set(0);
        cmdContainer.reset(new MyMockCommandContainer());
        cmdContainer->initialize(pDevice, nullptr, true);

        bool requiresUncachedMocs = false;
        EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

        EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

        auto dsh = cmdContainer->getIndirectHeap(HeapType::DYNAMIC_STATE);

        GenCmdList commands;
        CmdParse<FamilyType>::parseCommandBuffer(commands, cmdContainer->getCommandStream()->getCpuBase(), cmdContainer->getCommandStream()->getUsed());

        auto itorMIDL = find<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(commands.begin(), commands.end());
        EXPECT_NE(itorMIDL, commands.end());

        auto cmd = genCmdCast<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(*itorMIDL);
        EXPECT_NE(cmd, nullptr);

        auto idd = static_cast<INTERFACE_DESCRIPTOR_DATA *>(ptrOffset(dsh->getCpuBase(), cmd->getInterfaceDescriptorDataStartAddress()));

        EXPECT_EQ(0u, idd->getBindingTableEntryCount());
        EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT_NO_SAMPLERS_USED, idd->getSamplerCount());
    }

    {
        DebugManager.flags.ForceBtpPrefetchMode.set(1);
        cmdContainer.reset(new MyMockCommandContainer());
        cmdContainer->initialize(pDevice, nullptr, true);

        bool requiresUncachedMocs = false;
        EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

        EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

        auto dsh = cmdContainer->getIndirectHeap(HeapType::DYNAMIC_STATE);

        GenCmdList commands;
        CmdParse<FamilyType>::parseCommandBuffer(commands, cmdContainer->getCommandStream()->getCpuBase(), cmdContainer->getCommandStream()->getUsed());

        auto itorMIDL = find<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(commands.begin(), commands.end());
        EXPECT_NE(itorMIDL, commands.end());

        auto cmd = genCmdCast<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(*itorMIDL);
        EXPECT_NE(cmd, nullptr);

        auto idd = static_cast<INTERFACE_DESCRIPTOR_DATA *>(ptrOffset(dsh->getCpuBase(), cmd->getInterfaceDescriptorDataStartAddress()));

        EXPECT_EQ(numBindingTable, idd->getBindingTableEntryCount());
        EXPECT_EQ(static_cast<typename INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT>((numSamplers + 3) / 4), idd->getSamplerCount());
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandEncodeStatesTest, givenCleanHeapsAndSlmNotChangedWhenDispatchKernelThenFlushNotAdded) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    cmdContainer->slmSize = 1;
    cmdContainer->setDirtyStateForAllHeaps(false);
    dispatchInterface->getSlmTotalSizeResult = cmdContainer->slmSize;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itorPC = find<PIPE_CONTROL *>(commands.begin(), commands.end());
    ASSERT_EQ(itorPC, commands.end());
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandEncodeStatesTest, givenCleanHeapsAndSlmNotChangedAndUncachedMocsRequestedThenSBAIsProgrammedAndMocsAreSet) {
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    cmdContainer->slmSize = 1;
    cmdContainer->setDirtyStateForAllHeaps(false);
    dispatchInterface->getSlmTotalSizeResult = cmdContainer->slmSize;

    bool requiresUncachedMocs = true;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto itor = find<STATE_BASE_ADDRESS *>(commands.begin(), commands.end());
    ASSERT_NE(commands.end(), itor);

    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*itor);
    auto gmmHelper = cmdContainer->getDevice()->getGmmHelper();
    EXPECT_EQ(cmdSba->getStatelessDataPortAccessMemoryObjectControlState(),
              (gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED)));
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandEncodeStatesTest, givenDirtyHeapsAndSlmNotChangedWhenDispatchKernelThenHeapsAreCleanAndFlushAdded) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    cmdContainer->slmSize = 1;
    cmdContainer->setDirtyStateForAllHeaps(true);
    dispatchInterface->getSlmTotalSizeResult = cmdContainer->slmSize;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

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
    cmdContainer->setDirtyStateForAllHeaps(true);
    dispatchInterface->getSlmTotalSizeResult = cmdContainer->slmSize;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    GenCmdList cmdList;
    CmdParse<FamilyType>::parseCommandBuffer(cmdList, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = reverseFind<STATE_BASE_ADDRESS *>(cmdList.rbegin(), cmdList.rend());
    ASSERT_NE(cmdList.rend(), itor);

    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*itor);
    EXPECT_NE(nullptr, cmdSba);

    auto itorPc = reverseFind<PIPE_CONTROL *>(itor, cmdList.rend());
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

    cmdContainer->setDirtyStateForAllHeaps(false);
    dispatchInterface->getSlmTotalSizeResult = cmdContainer->slmSize + 1;

    auto slmSizeBefore = cmdContainer->slmSize;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

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

    cmdContainer->getIndirectHeap(HeapType::DYNAMIC_STATE)->align(EncodeStates<FamilyType>::alignInterfaceDescriptorData);
    cmdContainer->setIddBlock(cmdContainer->getHeapSpaceAllowGrow(HeapType::DYNAMIC_STATE, sizeof(INTERFACE_DESCRIPTOR_DATA) * cmdContainer->getNumIddPerBlock()));
    cmdContainer->nextIddInBlock = 0;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itorPC = find<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(commands.begin(), commands.end());
    ASSERT_NE(itorPC, commands.end());
}

using EncodeDispatchKernelTest = Test<CommandEncodeStatesFixture>;

HWTEST2_F(EncodeDispatchKernelTest, givenBindfulKernelWhenDispatchingKernelThenSshFromContainerIsUsed, IsAtLeastSkl) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using WALKER = typename FamilyType::WALKER_TYPE;
    uint32_t numBindingTable = 1;
    BINDING_TABLE_STATE bindingTableState = FamilyType::cmdInitBindingTableState;

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.numEntries = numBindingTable;
    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.tableOffset = 0U;
    dispatchInterface->kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::BindfulAndStateless;

    const uint8_t *sshData = reinterpret_cast<uint8_t *>(&bindingTableState);
    dispatchInterface->getSurfaceStateHeapDataResult = const_cast<uint8_t *>(sshData);
    dispatchInterface->getSurfaceStateHeapDataSizeResult = static_cast<uint32_t>(sizeof(BINDING_TABLE_STATE));

    auto usedBefore = cmdContainer->getIndirectHeap(HeapType::SURFACE_STATE)->getUsed();
    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    auto usedAfter = cmdContainer->getIndirectHeap(HeapType::SURFACE_STATE)->getUsed();

    EXPECT_NE(usedAfter, usedBefore);
}

HWTEST2_F(EncodeDispatchKernelTest, givenBindlessKernelWhenDispatchingKernelThenThenSshFromContainerIsNotUsed, IsAtLeastSkl) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using WALKER = typename FamilyType::WALKER_TYPE;
    uint32_t numBindingTable = 1;
    BINDING_TABLE_STATE bindingTableState = FamilyType::cmdInitBindingTableState;

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.numEntries = numBindingTable;
    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.tableOffset = 0U;
    dispatchInterface->kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::BindlessAndStateless;

    const uint8_t *sshData = reinterpret_cast<uint8_t *>(&bindingTableState);
    dispatchInterface->getSurfaceStateHeapDataResult = const_cast<uint8_t *>(sshData);
    dispatchInterface->getSurfaceStateHeapDataSizeResult = static_cast<uint32_t>(sizeof(BINDING_TABLE_STATE));

    bool requiresUncachedMocs = false;
    auto usedBefore = cmdContainer->getIndirectHeap(HeapType::SURFACE_STATE)->getUsed();
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    auto usedAfter = cmdContainer->getIndirectHeap(HeapType::SURFACE_STATE)->getUsed();

    EXPECT_EQ(usedAfter, usedBefore);
}

HWTEST_F(EncodeDispatchKernelTest, givenNonBindlessOrStatelessArgWhenDispatchingKernelThenSurfaceStateOffsetInCrossThreadDataIsNotPatched) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    using DataPortBindlessSurfaceExtendedMessageDescriptor = typename FamilyType::DataPortBindlessSurfaceExtendedMessageDescriptor;
    uint32_t numBindingTable = 1;
    BINDING_TABLE_STATE bindingTableState = FamilyType::cmdInitBindingTableState;

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
    dispatchInterface->getSurfaceStateHeapDataResult = const_cast<uint8_t *>(sshData);
    dispatchInterface->getSurfaceStateHeapDataSizeResult = static_cast<uint32_t>(sizeof(BINDING_TABLE_STATE));

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

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
    dispatchInterface->getSurfaceStateHeapDataResult = const_cast<uint8_t *>(sshData);
    dispatchInterface->getSurfaceStateHeapDataSizeResult = static_cast<uint32_t>(sizeof(BINDING_TABLE_STATE));

    ioh->replaceBuffer(ioh->getCpuBase(), ioh->getMaxAvailableSpace());
    memset(ioh->getCpuBase(), 0, ioh->getMaxAvailableSpace());

    dispatchArgs.dispatchInterface = dispatchInterface.get();
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    EXPECT_TRUE(memoryZeroed(ptrOffset(ioh->getCpuBase(), iohOffset), ioh->getMaxAvailableSpace() - iohOffset));
}

HWCMDTEST_F(IGFX_GEN8_CORE, WalkerThreadTest, givenStartWorkGroupWhenIndirectIsFalseThenExpectStartGroupAndThreadDimensionsProgramming) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

    WALKER_TYPE walkerCmd = FamilyType::cmdInitGpgpuWalker;
    startWorkGroup[1] = 2u;
    startWorkGroup[2] = 3u;

    EncodeDispatchKernel<FamilyType>::encodeThreadData(walkerCmd, startWorkGroup, numWorkGroups, workGroupSizes, simd, localIdDimensions,
                                                       0, 0, true, false, false, requiredWorkGroupOrder, *defaultHwInfo);
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
                                                       0, 0, true, false, true, requiredWorkGroupOrder, *defaultHwInfo);
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
                                                       0, 0, true, false, false, requiredWorkGroupOrder, *defaultHwInfo);
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
                                                       expectedThreadPerThreadGroup, 0, true, false, false, requiredWorkGroupOrder, *defaultHwInfo);
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
                                                       0, expectedExecutionMask, true, false, false, requiredWorkGroupOrder, *defaultHwInfo);
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

using BindlessCommandEncodeStatesTest = Test<MemManagerFixture>;
using BindlessCommandEncodeStatesContainerTest = Test<CommandEncodeStatesFixture>;

HWTEST_F(BindlessCommandEncodeStatesContainerTest, givenBindlessKernelAndBindlessModeEnabledWhenEncodingKernelThenCmdContainerHasNullptrSSH) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using WALKER = typename FamilyType::WALKER_TYPE;
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseBindlessMode.set(1);
    auto commandContainer = std::make_unique<CommandContainer>();
    commandContainer->initialize(pDevice, nullptr, true);
    commandContainer->setDirtyStateForAllHeaps(false);
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->createBindlessHeapsHelper(pDevice->getMemoryManager(),
                                                                                                                         pDevice->getNumGenericSubDevices() > 1,
                                                                                                                         pDevice->getRootDeviceIndex(),
                                                                                                                         pDevice->getDeviceBitfield());
    uint32_t numBindingTable = 1;
    BINDING_TABLE_STATE bindingTableState = FamilyType::cmdInitBindingTableState;

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.numEntries = numBindingTable;
    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.tableOffset = 0U;
    dispatchInterface->kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::BindlessAndStateless;

    const uint8_t *sshData = reinterpret_cast<uint8_t *>(&bindingTableState);
    dispatchInterface->getSurfaceStateHeapDataResult = const_cast<uint8_t *>(sshData);
    dispatchInterface->getSurfaceStateHeapDataSizeResult = static_cast<uint32_t>(sizeof(BINDING_TABLE_STATE));

    bool requiresUncachedMocs = false;
    EXPECT_EQ(commandContainer->getIndirectHeap(HeapType::SURFACE_STATE), nullptr);
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::encode(*commandContainer.get(), dispatchArgs);

    EXPECT_EQ(commandContainer->getIndirectHeap(HeapType::SURFACE_STATE), nullptr);
}

HWTEST2_F(BindlessCommandEncodeStatesContainerTest, givenBindlessKernelAndBindlessModeEnabledWhenEncodingKernelThenCmdContainerResidencyContainsGlobalDSH, IsAtMostGen12lp) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using WALKER = typename FamilyType::WALKER_TYPE;
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseBindlessMode.set(1);
    auto commandContainer = std::make_unique<CommandContainer>();
    commandContainer->initialize(pDevice, nullptr, true);
    commandContainer->setDirtyStateForAllHeaps(false);
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->createBindlessHeapsHelper(pDevice->getMemoryManager(),
                                                                                                                         pDevice->getNumGenericSubDevices() > 1,
                                                                                                                         pDevice->getRootDeviceIndex(),
                                                                                                                         pDevice->getDeviceBitfield());
    uint32_t numBindingTable = 1;
    BINDING_TABLE_STATE bindingTableState = FamilyType::cmdInitBindingTableState;

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.numEntries = numBindingTable;
    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.tableOffset = 0U;
    dispatchInterface->kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::BindlessAndStateless;

    const uint8_t *sshData = reinterpret_cast<uint8_t *>(&bindingTableState);
    dispatchInterface->getSurfaceStateHeapDataResult = const_cast<uint8_t *>(sshData);
    dispatchInterface->getSurfaceStateHeapDataSizeResult = static_cast<uint32_t>(sizeof(BINDING_TABLE_STATE));

    bool requiresUncachedMocs = false;
    EXPECT_EQ(commandContainer->getIndirectHeap(HeapType::SURFACE_STATE), nullptr);
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::encode(*commandContainer.get(), dispatchArgs);

    auto globalDSHIterator = std::find(commandContainer->getResidencyContainer().begin(), commandContainer->getResidencyContainer().end(),
                                       pDevice->getBindlessHeapsHelper()->getHeap(BindlessHeapsHelper::GLOBAL_DSH)->getGraphicsAllocation());

    EXPECT_NE(commandContainer->getResidencyContainer().end(), globalDSHIterator);
}

HWTEST_F(BindlessCommandEncodeStatesContainerTest, givenBindfulKernelWhenBindlessModeEnabledThenCmdContainerHaveSsh) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using WALKER = typename FamilyType::WALKER_TYPE;
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseBindlessMode.set(1);
    auto commandContainer = std::make_unique<CommandContainer>();
    commandContainer->initialize(pDevice, nullptr, true);
    commandContainer->setDirtyStateForAllHeaps(false);
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->createBindlessHeapsHelper(pDevice->getMemoryManager(),
                                                                                                                         pDevice->getNumGenericSubDevices() > 1,
                                                                                                                         pDevice->getRootDeviceIndex(),
                                                                                                                         pDevice->getDeviceBitfield());
    uint32_t numBindingTable = 1;
    BINDING_TABLE_STATE bindingTableState = FamilyType::cmdInitBindingTableState;

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.numEntries = numBindingTable;
    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.tableOffset = 0U;
    dispatchInterface->kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::BindfulAndStateless;

    const uint8_t *sshData = reinterpret_cast<uint8_t *>(&bindingTableState);
    dispatchInterface->getSurfaceStateHeapDataResult = const_cast<uint8_t *>(sshData);
    dispatchInterface->getSurfaceStateHeapDataSizeResult = static_cast<uint32_t>(sizeof(BINDING_TABLE_STATE));

    bool requiresUncachedMocs = false;
    EXPECT_EQ(commandContainer->getIndirectHeap(HeapType::SURFACE_STATE), nullptr);
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::encode(*commandContainer.get(), dispatchArgs);

    EXPECT_NE(commandContainer->getIndirectHeap(HeapType::SURFACE_STATE), nullptr);
}

HWTEST_F(BindlessCommandEncodeStatesContainerTest, givenBindlessModeEnabledWhenDispatchingTwoBindfulKernelsThenItuseTheSameSsh) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using WALKER = typename FamilyType::WALKER_TYPE;
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseBindlessMode.set(1);
    auto commandContainer = std::make_unique<CommandContainer>();
    commandContainer->initialize(pDevice, nullptr, true);
    commandContainer->setDirtyStateForAllHeaps(false);
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->createBindlessHeapsHelper(pDevice->getMemoryManager(),
                                                                                                                         pDevice->getNumGenericSubDevices() > 1,
                                                                                                                         pDevice->getRootDeviceIndex(),
                                                                                                                         pDevice->getDeviceBitfield());
    uint32_t numBindingTable = 1;
    BINDING_TABLE_STATE bindingTableState = FamilyType::cmdInitBindingTableState;

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.numEntries = numBindingTable;
    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.tableOffset = 0U;
    dispatchInterface->kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::BindfulAndStateless;

    const uint8_t *sshData = reinterpret_cast<uint8_t *>(&bindingTableState);
    dispatchInterface->getSurfaceStateHeapDataResult = const_cast<uint8_t *>(sshData);
    dispatchInterface->getSurfaceStateHeapDataSizeResult = static_cast<uint32_t>(sizeof(BINDING_TABLE_STATE));

    bool requiresUncachedMocs = false;
    EXPECT_EQ(commandContainer->getIndirectHeap(HeapType::SURFACE_STATE), nullptr);
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::encode(*commandContainer.get(), dispatchArgs);

    auto sshBefore = commandContainer->getIndirectHeap(HeapType::SURFACE_STATE);
    EncodeDispatchKernel<FamilyType>::encode(*commandContainer.get(), dispatchArgs);
    EncodeDispatchKernel<FamilyType>::encode(*commandContainer.get(), dispatchArgs);
    auto sshAfter = commandContainer->getIndirectHeap(HeapType::SURFACE_STATE);
    EncodeDispatchKernel<FamilyType>::encode(*commandContainer.get(), dispatchArgs);
    EXPECT_EQ(sshBefore, sshAfter);
}

HWTEST_F(BindlessCommandEncodeStatesTest, givenGlobalBindlessHeapsWhenDispatchingKernelWithSamplerThenGlobalDshInResidencyContainer) {
    bool deviceUsesDsh = pDevice->getHardwareInfo().capabilityTable.supportsImages;
    if (!deviceUsesDsh) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseBindlessMode.set(1);
    auto cmdContainer = std::make_unique<CommandContainer>();
    cmdContainer->initialize(pDevice, nullptr, true);
    cmdContainer->setDirtyStateForAllHeaps(false);
    using SAMPLER_BORDER_COLOR_STATE = typename FamilyType::SAMPLER_BORDER_COLOR_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    uint32_t numSamplers = 1;
    SAMPLER_BORDER_COLOR_STATE samplerState;
    samplerState.init();
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->createBindlessHeapsHelper(pDevice->getMemoryManager(),
                                                                                                                         pDevice->getNumGenericSubDevices() > 1,
                                                                                                                         pDevice->getRootDeviceIndex(),
                                                                                                                         pDevice->getDeviceBitfield());

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    dispatchInterface->kernelDescriptor.payloadMappings.samplerTable.numSamplers = numSamplers;
    dispatchInterface->kernelDescriptor.payloadMappings.samplerTable.tableOffset = 0U;
    dispatchInterface->kernelDescriptor.payloadMappings.samplerTable.borderColor = 0U;
    const uint8_t *dshData = reinterpret_cast<uint8_t *>(&samplerState);
    dispatchInterface->getDynamicStateHeapDataResult = const_cast<uint8_t *>(dshData);

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs{
        0,
        pDevice,
        dispatchInterface.get(),
        dims,
        NEO::PreemptionMode::Disabled,
        0,
        false,
        false,
        false,
        requiresUncachedMocs,
        false,
        false,
        false,
        false};

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    EXPECT_NE(std::find(cmdContainer->getResidencyContainer().begin(), cmdContainer->getResidencyContainer().end(), pDevice->getBindlessHeapsHelper()->getHeap(BindlessHeapsHelper::GLOBAL_DSH)->getGraphicsAllocation()), cmdContainer->getResidencyContainer().end());
}

HWTEST_F(BindlessCommandEncodeStatesTest, givenBindlessModeDisabledelWithSamplerThenGlobalDshIsNotResidnecyContainer) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseBindlessMode.set(0);
    auto cmdContainer = std::make_unique<CommandContainer>();
    cmdContainer->initialize(pDevice, nullptr, true);
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    uint32_t numSamplers = 1;
    SAMPLER_STATE samplerState;
    memset(&samplerState, 2, sizeof(SAMPLER_STATE));
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->createBindlessHeapsHelper(pDevice->getMemoryManager(),
                                                                                                                         pDevice->getNumGenericSubDevices() > 1,
                                                                                                                         pDevice->getRootDeviceIndex(),
                                                                                                                         pDevice->getDeviceBitfield());

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    dispatchInterface->kernelDescriptor.payloadMappings.samplerTable.numSamplers = numSamplers;
    dispatchInterface->kernelDescriptor.payloadMappings.samplerTable.tableOffset = 0U;
    dispatchInterface->kernelDescriptor.payloadMappings.samplerTable.borderColor = 0U;
    const uint8_t *dshData = reinterpret_cast<uint8_t *>(&samplerState);
    dispatchInterface->getDynamicStateHeapDataResult = const_cast<uint8_t *>(dshData);

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs{
        0,
        pDevice,
        dispatchInterface.get(),
        dims,
        NEO::PreemptionMode::Disabled,
        0,
        false,
        false,
        false,
        requiresUncachedMocs,
        false,
        false,
        false,
        false};

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    EXPECT_EQ(std::find(cmdContainer->getResidencyContainer().begin(), cmdContainer->getResidencyContainer().end(), pDevice->getBindlessHeapsHelper()->getHeap(BindlessHeapsHelper::GLOBAL_DSH)->getGraphicsAllocation()), cmdContainer->getResidencyContainer().end());
}
