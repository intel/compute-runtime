/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/bindless_heaps_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/kernel/kernel_descriptor_from_patchtokens.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/device_binary_format/patchtokens_tests.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_l0_debugger.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"
#include "shared/test/unit_test/fixtures/front_window_fixture.h"
#include "shared/test/unit_test/mocks/mock_dispatch_kernel_encoder_interface.h"

using namespace NEO;
#include "shared/test/common/test_macros/header/heapless_matchers.h"

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
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    dispatchArgs.surfaceStateHeap = cmdContainer->getIndirectHeap(HeapType::surfaceState);
    if (EncodeDispatchKernel<FamilyType>::isDshNeeded(pDevice->getDeviceInfo())) {
        dispatchArgs.dynamicStateHeap = cmdContainer->getIndirectHeap(HeapType::dynamicState);
    }

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itorPC = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itorPC, commands.end());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenDebugFlagSetWhenProgrammingWalkerThenSetFlushingBits) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DebugManagerStateRestore restore;
    debugManager.flags.ForceComputeWalkerPostSyncFlush.set(1);

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
    EXPECT_TRUE(walkerCmd->getPostSync().getDataportPipelineFlush());
}

using CommandEncodeStatesUncachedMocsTests = Test<CommandEncodeStatesFixture>;

HWTEST_F(CommandEncodeStatesUncachedMocsTests, whenEncodingDispatchKernelWithUncachedMocsAndDirtyHeapsThenCorrectMocsIsSet) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DebugManagerStateRestore restore;
    debugManager.flags.ForceL1Caching.set(0u);

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    cmdContainer->setDirtyStateForAllHeaps(true);
    bool requiresUncachedMocs = true;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

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
              (gmmHelper->getUncachedMOCS()));
}

HWTEST_F(CommandEncodeStatesUncachedMocsTests, whenEncodingDispatchKernelWithUncachedMocsAndNonDirtyHeapsThenCorrectMocsIsSet) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DebugManagerStateRestore restore;
    debugManager.flags.ForceL1Caching.set(0u);

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    cmdContainer->setDirtyStateForAllHeaps(false);
    bool requiresUncachedMocs = true;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

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
              (gmmHelper->getUncachedMOCS()));
}

HWTEST_F(CommandEncodeStatesUncachedMocsTests, whenEncodingDispatchKernelWithNonUncachedMocsAndDirtyHeapsThenSbaIsNotProgrammed) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DebugManagerStateRestore restore;
    debugManager.flags.ForceL1Caching.set(0u);

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    cmdContainer->setDirtyStateForAllHeaps(true);
    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);
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
              (gmmHelper->getL3EnabledMOCS()));
}

HWTEST_F(CommandEncodeStatesUncachedMocsTests, whenEncodingDispatchKernelWithNonUncachedMocsAndNonDirtyHeapsThenSbaIsNotProgrammed) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DebugManagerStateRestore restore;
    debugManager.flags.ForceL1Caching.set(0u);

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    cmdContainer->setDirtyStateForAllHeaps(false);
    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);
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
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DebugManagerStateRestore restore;
    debugManager.flags.ForceL1Caching.set(0u);

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint32_t slmTotalSize = 1;

    dispatchInterface->getSlmTotalSizeResult = slmTotalSize;

    cmdContainer->setDirtyStateForAllHeaps(false);
    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);
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
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    auto cmdBuffersCountBefore = cmdContainer->getCmdBufferAllocations().size();

    cmdContainer->getCommandStream()->getSpace(cmdContainer->getCommandStream()->getAvailableSpace() - sizeof(typename FamilyType::MI_BATCH_BUFFER_END));

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    auto cmdBuffersCountAfter = cmdContainer->getCmdBufferAllocations().size();

    EXPECT_GT(cmdBuffersCountAfter, cmdBuffersCountBefore);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandEncodeStatesTest, givenSlmTotalSizeGraterThanZeroWhenDispatchingKernelThenSharedMemorySizeSetCorrectly) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA = typename EncodeStates<FamilyType>::INTERFACE_DESCRIPTOR_DATA;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint32_t slmTotalSize = 1;

    dispatchInterface->getSlmTotalSizeResult = slmTotalSize;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    auto interfaceDescriptorData = static_cast<INTERFACE_DESCRIPTOR_DATA *>(cmdContainer->getIddBlock());
    auto &gfxcoreHelper = this->getHelper<GfxCoreHelper>();
    uint32_t expectedValue = static_cast<typename INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE>(
        gfxcoreHelper.computeSlmValues(pDevice->getHardwareInfo(), slmTotalSize, nullptr, false));

    EXPECT_EQ(expectedValue, interfaceDescriptorData->getSharedLocalMemorySize());
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandEncodeStatesTest, whenDispatchingKernelThenSetDenormMode) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA = typename EncodeStates<FamilyType>::INTERFACE_DESCRIPTOR_DATA;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, false);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    auto interfaceDescriptorData = static_cast<INTERFACE_DESCRIPTOR_DATA *>(cmdContainer->getIddBlock());

    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::DENORM_MODE_FTZ, interfaceDescriptorData->getDenormMode());
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandEncodeStatesTest, givenDebuggingEnabledAndAssertInKernelWhenDispatchingKernelThenSwExceptionsAreEnabled) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA = typename EncodeStates<FamilyType>::INTERFACE_DESCRIPTOR_DATA;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    auto debugger = new MockDebuggerL0(pDevice);
    pDevice->getRootDeviceEnvironmentRef().debugger.reset(debugger);

    dispatchInterface->kernelDescriptor.kernelAttributes.flags.usesAssert = true;

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, false);
    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    auto interfaceDescriptorData = static_cast<INTERFACE_DESCRIPTOR_DATA *>(cmdContainer->getIddBlock());
    EXPECT_TRUE(interfaceDescriptorData->getSoftwareExceptionEnable());
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandEncodeStatesTest, givenSlmTotalSizeEqualZeroWhenDispatchingKernelThenSharedMemorySizeSetCorrectly) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA = typename EncodeStates<FamilyType>::INTERFACE_DESCRIPTOR_DATA;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint32_t slmTotalSize = 0;

    dispatchInterface->getSlmTotalSizeResult = slmTotalSize;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    auto interfaceDescriptorData = static_cast<INTERFACE_DESCRIPTOR_DATA *>(cmdContainer->getIddBlock());

    uint32_t expectedValue = INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_0K;

    EXPECT_EQ(expectedValue, interfaceDescriptorData->getSharedLocalMemorySize());
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandEncodeStatesTest, givenOneBindingTableEntryWhenDispatchingKernelThenBindingTableOffsetIsCorrect) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename EncodeStates<FamilyType>::INTERFACE_DESCRIPTOR_DATA;
    uint32_t numBindingTable = 1;
    BINDING_TABLE_STATE bindingTableState = FamilyType::cmdInitBindingTableState;

    auto ssh = cmdContainer->getIndirectHeap(HeapType::surfaceState);
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

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    auto interfaceDescriptorData = static_cast<INTERFACE_DESCRIPTOR_DATA *>(cmdContainer->getIddBlock());

    EXPECT_EQ(interfaceDescriptorData->getBindingTablePointer(), expectedOffset);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandEncodeStatesTest, giveNumBindingTableZeroWhenDispatchingKernelThenBindingTableOffsetIsZero) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename EncodeStates<FamilyType>::INTERFACE_DESCRIPTOR_DATA;
    uint32_t numBindingTable = 0;
    BINDING_TABLE_STATE bindingTableState = FamilyType::cmdInitBindingTableState;

    auto ssh = cmdContainer->getIndirectHeap(HeapType::surfaceState);
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

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    auto interfaceDescriptorData = static_cast<INTERFACE_DESCRIPTOR_DATA *>(cmdContainer->getIddBlock());

    EXPECT_EQ(interfaceDescriptorData->getBindingTablePointer(), 0u);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandEncodeStatesTest, giveNumSamplersOneWhenDispatchingKernelThensamplerStateWasCopied) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename EncodeStates<FamilyType>::INTERFACE_DESCRIPTOR_DATA;
    uint32_t numSamplers = 1;
    SAMPLER_STATE samplerState;
    memset(&samplerState, 2, sizeof(SAMPLER_STATE));

    auto dsh = cmdContainer->getIndirectHeap(HeapType::dynamicState);
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
    dispatchArgs.surfaceStateHeap = cmdContainer->getIndirectHeap(HeapType::surfaceState);
    dispatchArgs.dynamicStateHeap = cmdContainer->getIndirectHeap(HeapType::dynamicState);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    auto interfaceDescriptorData = static_cast<INTERFACE_DESCRIPTOR_DATA *>(cmdContainer->getIddBlock());

    auto borderColorOffsetInDsh = usedBefore;
    samplerState.setIndirectStatePointer(static_cast<uint32_t>(borderColorOffsetInDsh));

    auto samplerStateOffset = interfaceDescriptorData->getSamplerStatePointer();

    auto pSmplr = reinterpret_cast<SAMPLER_STATE *>(ptrOffset(dsh->getCpuBase(), samplerStateOffset));
    EXPECT_EQ(memcmp(pSmplr, &samplerState, sizeof(SAMPLER_STATE)), 0);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandEncodeStatesTest, giveNumSamplersZeroWhenDispatchingKernelThensamplerStateWasNotCopied) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename EncodeStates<FamilyType>::INTERFACE_DESCRIPTOR_DATA;
    uint32_t numSamplers = 0;
    SAMPLER_STATE samplerState;
    memset(&samplerState, 2, sizeof(SAMPLER_STATE));

    auto dsh = cmdContainer->getIndirectHeap(HeapType::dynamicState);
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

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    auto interfaceDescriptorData = static_cast<INTERFACE_DESCRIPTOR_DATA *>(cmdContainer->getIddBlock());

    auto borderColorOffsetInDsh = usedBefore;
    samplerState.setIndirectStatePointer(static_cast<uint32_t>(borderColorOffsetInDsh));

    auto samplerStateOffset = interfaceDescriptorData->getSamplerStatePointer();

    auto pSmplr = reinterpret_cast<SAMPLER_STATE *>(ptrOffset(dsh->getCpuBase(), samplerStateOffset));
    EXPECT_NE(memcmp(pSmplr, &samplerState, sizeof(SAMPLER_STATE)), 0);
}

HWTEST_F(CommandEncodeStatesTest, givenIndirectOffsetsCountsWhenDispatchingKernelThenCorrestMIStoreOffsetsSet) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
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

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = commands.begin();

    for (int i = 0; i < 3; i++) {
        ASSERT_NE(itor, commands.end());
        itor = find<MI_STORE_REGISTER_MEM *>(++itor, commands.end());
    }
}

HWTEST_F(CommandEncodeStatesTest, givenIndirectOffsetsSizeWhenDispatchingKernelThenMiMathEncoded) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
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

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<MI_MATH *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandEncodeStatesTest, givenForceBtpPrefetchModeDebugFlagWhenDispatchingKernelThenValuesAreSetUpCorrectly) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA = typename EncodeStates<FamilyType>::INTERFACE_DESCRIPTOR_DATA;
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
        debugManager.flags.ForceBtpPrefetchMode.set(-1);
        cmdContainer.reset(new MyMockCommandContainer());
        cmdContainer->initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);
        cmdContainer->l1CachePolicyDataRef() = &l1CachePolicyData;

        bool requiresUncachedMocs = false;
        EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

        EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

        auto dsh = cmdContainer->getIndirectHeap(HeapType::dynamicState);

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
        debugManager.flags.ForceBtpPrefetchMode.set(0);
        cmdContainer.reset(new MyMockCommandContainer());
        cmdContainer->initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);
        cmdContainer->l1CachePolicyDataRef() = &l1CachePolicyData;

        bool requiresUncachedMocs = false;
        EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

        EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

        auto dsh = cmdContainer->getIndirectHeap(HeapType::dynamicState);

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
        debugManager.flags.ForceBtpPrefetchMode.set(1);
        cmdContainer.reset(new MyMockCommandContainer());
        cmdContainer->initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);
        cmdContainer->l1CachePolicyDataRef() = &l1CachePolicyData;

        bool requiresUncachedMocs = false;
        EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

        EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

        auto dsh = cmdContainer->getIndirectHeap(HeapType::dynamicState);

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

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandEncodeStatesTest, givenCleanHeapsAndSlmNotChangedWhenDispatchKernelThenFlushNotAdded) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    cmdContainer->slmSizeRef() = 1;
    cmdContainer->setDirtyStateForAllHeaps(false);
    dispatchInterface->getSlmTotalSizeResult = cmdContainer->slmSizeRef();

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itorPC = find<PIPE_CONTROL *>(commands.begin(), commands.end());
    ASSERT_EQ(itorPC, commands.end());
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandEncodeStatesTest, givenCleanHeapsAndSlmNotChangedAndUncachedMocsRequestedThenSBAIsProgrammedAndMocsAreSet) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    cmdContainer->slmSizeRef() = 1;
    cmdContainer->setDirtyStateForAllHeaps(false);
    dispatchInterface->getSlmTotalSizeResult = cmdContainer->slmSizeRef();

    bool requiresUncachedMocs = true;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto itor = find<STATE_BASE_ADDRESS *>(commands.begin(), commands.end());
    ASSERT_NE(commands.end(), itor);

    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*itor);
    auto gmmHelper = cmdContainer->getDevice()->getGmmHelper();
    EXPECT_EQ(cmdSba->getStatelessDataPortAccessMemoryObjectControlState(),
              (gmmHelper->getUncachedMOCS()));
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandEncodeStatesTest, givenDirtyHeapsAndSlmNotChangedWhenDispatchKernelThenHeapsAreCleanAndFlushAdded) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    cmdContainer->slmSizeRef() = 1;
    cmdContainer->setDirtyStateForAllHeaps(true);
    dispatchInterface->getSlmTotalSizeResult = cmdContainer->slmSizeRef();

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itorPC = find<PIPE_CONTROL *>(commands.begin(), commands.end());
    ASSERT_NE(itorPC, commands.end());
    EXPECT_FALSE(cmdContainer->isAnyHeapDirty());
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandEncodeStatesTest, givenDirtyHeapsWhenDispatchKernelThenPCIsAddedBeforeSBA) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    cmdContainer->slmSizeRef() = 1;
    cmdContainer->setDirtyStateForAllHeaps(true);
    dispatchInterface->getSlmTotalSizeResult = cmdContainer->slmSizeRef();

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.postSyncArgs.dcFlushEnable = MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, pDevice->getRootDeviceEnvironment());

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

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

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandEncodeStatesTest, givenCleanHeapsAndSlmChangedWhenDispatchKernelThenFlushAdded) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    cmdContainer->setDirtyStateForAllHeaps(false);
    dispatchInterface->getSlmTotalSizeResult = cmdContainer->slmSizeRef() + 1;

    auto slmSizeBefore = cmdContainer->slmSizeRef();

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itorPC = find<PIPE_CONTROL *>(commands.begin(), commands.end());
    ASSERT_NE(itorPC, commands.end());
    EXPECT_EQ(slmSizeBefore + 1, cmdContainer->slmSizeRef());
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandEncodeStatesTest, giveNextIddInBlockZeroWhenDispatchKernelThenMediaInterfaceDescriptorEncoded) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA = typename EncodeStates<FamilyType>::INTERFACE_DESCRIPTOR_DATA;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using MEDIA_INTERFACE_DESCRIPTOR_LOAD = typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    cmdContainer->getIndirectHeap(HeapType::dynamicState)->align(EncodeStates<FamilyType>::alignInterfaceDescriptorData);
    cmdContainer->setIddBlock(cmdContainer->getHeapSpaceAllowGrow(HeapType::dynamicState, sizeof(INTERFACE_DESCRIPTOR_DATA) * cmdContainer->getNumIddPerBlock()));
    cmdContainer->nextIddInBlockRef() = 0;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itorSBA = find<STATE_BASE_ADDRESS *>(commands.begin(), commands.end());
    auto itorPC = find<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(commands.begin(), commands.end());
    ASSERT_EQ(itorSBA, commands.end()); // no flush needed
    ASSERT_NE(itorPC, commands.end());
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandEncodeStatesTest, giveNextIddInBlockZeroWhenDispatchKernelAndDynamicStateHeapDirtyThenStateBaseAddressEncodedAndMediaInterfaceDescriptorEncoded) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA = typename EncodeStates<FamilyType>::INTERFACE_DESCRIPTOR_DATA;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using MEDIA_INTERFACE_DESCRIPTOR_LOAD = typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    cmdContainer->getIndirectHeap(HeapType::dynamicState)->align(EncodeStates<FamilyType>::alignInterfaceDescriptorData);
    cmdContainer->setIddBlock(cmdContainer->getHeapSpaceAllowGrow(HeapType::dynamicState, sizeof(INTERFACE_DESCRIPTOR_DATA) * cmdContainer->getNumIddPerBlock()));
    cmdContainer->nextIddInBlockRef() = cmdContainer->getNumIddPerBlock();

    // ensure heap has no available space left so that it will be reallocated and set to dirty
    auto heap = cmdContainer->getIndirectHeap(HeapType::dynamicState);
    heap->getSpace(heap->getAvailableSpace());

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itorSBA = find<STATE_BASE_ADDRESS *>(commands.begin(), commands.end());
    auto itorPC = find<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(commands.begin(), commands.end());
    ASSERT_NE(itorSBA, commands.end()); // flush needed
    ASSERT_NE(itorPC, commands.end());
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandEncodeStatesTest, giveNumSamplersOneWhenHeapIsDirtyThenSamplerStateWasCopiedAndStateBaseAddressEncoded) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename EncodeStates<FamilyType>::INTERFACE_DESCRIPTOR_DATA;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using MEDIA_INTERFACE_DESCRIPTOR_LOAD = typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    uint32_t numSamplers = 1;
    SAMPLER_STATE samplerState;
    memset(&samplerState, 2, sizeof(SAMPLER_STATE));

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    dispatchInterface->kernelDescriptor.payloadMappings.samplerTable.numSamplers = numSamplers;
    dispatchInterface->kernelDescriptor.payloadMappings.samplerTable.tableOffset = 0U;
    dispatchInterface->kernelDescriptor.payloadMappings.samplerTable.borderColor = 0U;
    const uint8_t *dshData = reinterpret_cast<uint8_t *>(&samplerState);
    dispatchInterface->getDynamicStateHeapDataResult = const_cast<uint8_t *>(dshData);

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    auto dshBeforeFlush = cmdContainer->getIndirectHeap(HeapType::dynamicState);
    auto &kernelDescriptor = dispatchInterface->getKernelDescriptor();
    dshBeforeFlush->getSpace(dshBeforeFlush->getAvailableSpace() - NEO::EncodeDispatchKernel<FamilyType>::getSizeRequiredDsh(kernelDescriptor, cmdContainer->getNumIddPerBlock()));
    auto cpuBaseBeforeFlush = dshBeforeFlush->getCpuBase();

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());
    auto itorSBA = find<STATE_BASE_ADDRESS *>(commands.begin(), commands.end());
    auto itorPC = find<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(commands.begin(), commands.end());
    EXPECT_NE(itorSBA, commands.end()); // flush needed
    EXPECT_NE(itorPC, commands.end());

    auto dshAfterFlush = cmdContainer->getIndirectHeap(HeapType::dynamicState);
    EXPECT_NE(cpuBaseBeforeFlush, dshAfterFlush->getCpuBase());

    auto interfaceDescriptorData = static_cast<INTERFACE_DESCRIPTOR_DATA *>(cmdContainer->getIddBlock());

    auto borderColorOffsetInDsh = 0;
    samplerState.setIndirectStatePointer(static_cast<uint32_t>(borderColorOffsetInDsh));

    auto samplerStateOffset = interfaceDescriptorData->getSamplerStatePointer();

    auto pSmplr = reinterpret_cast<SAMPLER_STATE *>(ptrOffset(dshAfterFlush->getCpuBase(), samplerStateOffset));
    EXPECT_EQ(memcmp(pSmplr, &samplerState, sizeof(SAMPLER_STATE)), 0);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandEncodeStatesTest, giveNumSamplersOneAndNextIDDInBlockWhenHeapIsDirtyThenSamplerStateWasCopiedAndStateBaseAddressEncoded) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename EncodeStates<FamilyType>::INTERFACE_DESCRIPTOR_DATA;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using MEDIA_INTERFACE_DESCRIPTOR_LOAD = typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    uint32_t numSamplers = 1;
    SAMPLER_STATE samplerState;
    memset(&samplerState, 2, sizeof(SAMPLER_STATE));

    cmdContainer->getIndirectHeap(HeapType::dynamicState)->align(EncodeStates<FamilyType>::alignInterfaceDescriptorData);
    cmdContainer->setIddBlock(cmdContainer->getHeapSpaceAllowGrow(HeapType::dynamicState, sizeof(INTERFACE_DESCRIPTOR_DATA) * cmdContainer->getNumIddPerBlock()));
    cmdContainer->nextIddInBlockRef() = cmdContainer->getNumIddPerBlock();

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    dispatchInterface->kernelDescriptor.payloadMappings.samplerTable.numSamplers = numSamplers;
    dispatchInterface->kernelDescriptor.payloadMappings.samplerTable.tableOffset = 0U;
    dispatchInterface->kernelDescriptor.payloadMappings.samplerTable.borderColor = 0U;
    const uint8_t *dshData = reinterpret_cast<uint8_t *>(&samplerState);
    dispatchInterface->getDynamicStateHeapDataResult = const_cast<uint8_t *>(dshData);

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    auto dshBeforeFlush = cmdContainer->getIndirectHeap(HeapType::dynamicState);
    auto &kernelDescriptor = dispatchInterface->getKernelDescriptor();
    auto sizeRequiredMinusIDD = dshBeforeFlush->getAvailableSpace() - NEO::EncodeDispatchKernel<FamilyType>::getSizeRequiredDsh(kernelDescriptor, cmdContainer->getNumIddPerBlock()) + sizeof(INTERFACE_DESCRIPTOR_DATA);
    dshBeforeFlush->getSpace(sizeRequiredMinusIDD);
    auto cpuBaseBeforeFlush = dshBeforeFlush->getCpuBase();
    auto usedBefore = cmdContainer->getIndirectHeap(HeapType::surfaceState)->getUsed();

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());
    auto itorSBA = find<STATE_BASE_ADDRESS *>(commands.begin(), commands.end());
    auto itorPC = find<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(commands.begin(), commands.end());
    EXPECT_NE(itorSBA, commands.end()); // flush needed
    EXPECT_NE(itorPC, commands.end());

    auto dshAfterFlush = cmdContainer->getIndirectHeap(HeapType::dynamicState);
    EXPECT_NE(cpuBaseBeforeFlush, dshAfterFlush->getCpuBase());

    auto interfaceDescriptorData = static_cast<INTERFACE_DESCRIPTOR_DATA *>(cmdContainer->getIddBlock());

    auto borderColorOffsetInDsh = usedBefore;
    samplerState.setIndirectStatePointer(static_cast<uint32_t>(borderColorOffsetInDsh));

    auto samplerStateOffset = interfaceDescriptorData->getSamplerStatePointer();

    auto pSmplr = reinterpret_cast<SAMPLER_STATE *>(ptrOffset(dshAfterFlush->getCpuBase(), samplerStateOffset));
    EXPECT_EQ(memcmp(pSmplr, &samplerState, sizeof(SAMPLER_STATE)), 0);
}

HWTEST_F(CommandEncodeStatesTest, givenPauseOnEnqueueSetToNeverWhenEncodingWalkerThenCommandsToPatchAreNotPresent) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DebugManagerStateRestore restorer;
    debugManager.flags.PauseOnEnqueue.set(-1);

    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    uint32_t dims[] = {1, 1, 1};
    bool requiresUncachedMocs = false;
    std::list<void *> cmdsToPatch;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.additionalCommands = &cmdsToPatch;
    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    EXPECT_EQ(cmdsToPatch.size(), 0u);
}

HWTEST_F(CommandEncodeStatesTest, givenPauseOnEnqueueSetToAlwaysWhenEncodingWalkerThenCommandsToPatchAreFilled) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DebugManagerStateRestore restorer;
    debugManager.flags.PauseOnEnqueue.set(-2);

    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    uint32_t dims[] = {1, 1, 1};
    bool requiresUncachedMocs = false;
    std::list<void *> cmdsToPatch;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.additionalCommands = &cmdsToPatch;
    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    EXPECT_EQ(cmdsToPatch.size(), 4u);
}

using EncodeDispatchKernelTest = Test<CommandEncodeStatesFixture>;

HWTEST2_F(EncodeDispatchKernelTest, givenBindfulKernelWhenDispatchingKernelThenSshFromContainerIsUsed, HeapfulSupportedMatch) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
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

    auto usedBefore = cmdContainer->getIndirectHeap(HeapType::surfaceState)->getUsed();
    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.surfaceStateHeap = cmdContainer->getIndirectHeap(HeapType::surfaceState);
    if (EncodeDispatchKernel<FamilyType>::isDshNeeded(pDevice->getDeviceInfo())) {
        dispatchArgs.dynamicStateHeap = cmdContainer->getIndirectHeap(HeapType::dynamicState);
    }
    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    auto usedAfter = cmdContainer->getIndirectHeap(HeapType::surfaceState)->getUsed();

    EXPECT_NE(usedAfter, usedBefore);
}

HWTEST2_F(EncodeDispatchKernelTest, givenBindlessKernelWhenDispatchingKernelThenSshFromContainerIsUsed, MatchAny) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    alignas(64) uint8_t data[2 * sizeof(RENDER_SURFACE_STATE)];
    RENDER_SURFACE_STATE state = FamilyType::cmdInitRenderSurfaceState;
    memset(data, 0, sizeof(data));
    memcpy(data, &state, sizeof(state));

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.numEntries = 0;
    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.tableOffset = undefined<SurfaceStateHeapOffset>;
    dispatchInterface->kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::BindlessAndStateless;

    const uint8_t *sshData = data;
    dispatchInterface->getSurfaceStateHeapDataResult = const_cast<uint8_t *>(sshData);
    dispatchInterface->getSurfaceStateHeapDataSizeResult = static_cast<uint32_t>(sizeof(data));

    bool requiresUncachedMocs = false;
    cmdContainer->getIndirectHeap(HeapType::surfaceState)->getSpace(sizeof(RENDER_SURFACE_STATE));
    auto usedBefore = cmdContainer->getIndirectHeap(HeapType::surfaceState)->getUsed();
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    auto usedAfter = cmdContainer->getIndirectHeap(HeapType::surfaceState)->getUsed();

    EXPECT_NE(usedAfter, usedBefore);
}

HWTEST2_F(EncodeDispatchKernelTest, givenBindlessKernelWithRequiringSshForMisalignedBufferAddressWhenDispatchingKernelThenSshFromContainerIsUsed, MatchAny) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    uint32_t numBindingTable = 0;
    alignas(64) uint8_t data[2 * sizeof(RENDER_SURFACE_STATE)];
    RENDER_SURFACE_STATE state = FamilyType::cmdInitRenderSurfaceState;
    memset(data, 0, sizeof(data));
    memcpy(data, &state, sizeof(state));

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.numEntries = numBindingTable;
    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.tableOffset = 0;
    dispatchInterface->kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::BindlessAndStateless;

    const uint8_t *sshData = data;
    dispatchInterface->getSurfaceStateHeapDataResult = const_cast<uint8_t *>(sshData);
    dispatchInterface->getSurfaceStateHeapDataSizeResult = static_cast<uint32_t>(sizeof(RENDER_SURFACE_STATE));

    bool requiresUncachedMocs = false;
    cmdContainer->getIndirectHeap(HeapType::surfaceState)->getSpace(sizeof(RENDER_SURFACE_STATE));
    cmdContainer->getIndirectHeap(HeapType::surfaceState)->align(FamilyType::cacheLineSize);
    auto usedBefore = cmdContainer->getIndirectHeap(HeapType::surfaceState)->getUsed();
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    auto usedAfter = cmdContainer->getIndirectHeap(HeapType::surfaceState)->getUsed();

    EXPECT_NE(usedAfter, usedBefore);

    auto ssh = ptrOffset(cmdContainer->getIndirectHeap(HeapType::surfaceState)->getCpuBase(), usedBefore);
    EXPECT_EQ(0, memcmp(ssh, &state, sizeof(state)));
}

HWTEST2_F(EncodeDispatchKernelTest, givenKernelsSharingISAParentAllocationsWhenProgrammingWalkerThenKernelStartPointerHasProperOffset, IsGen12LP) {
    using INTERFACE_DESCRIPTOR_DATA = typename EncodeStates<FamilyType>::INTERFACE_DESCRIPTOR_DATA;
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    auto dispatchInterface = std::make_unique<MockDispatchKernelEncoder>();
    dispatchInterface->getIsaOffsetInParentAllocationResult = 8 << INTERFACE_DESCRIPTOR_DATA::KERNELSTARTPOINTER_BIT_SHIFT;
    uint32_t dims[] = {2, 1, 1};
    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    auto idd = static_cast<INTERFACE_DESCRIPTOR_DATA *>(cmdContainer->getIddBlock());
    EXPECT_EQ(idd->getKernelStartPointer(), dispatchInterface->getIsaAllocation()->getGpuAddressToPatch() + dispatchInterface->getIsaOffsetInParentAllocation());
}

HWTEST_F(EncodeDispatchKernelTest, givenKernelStartPointerAlignmentInInterfaceDescriptorWhenHelperGetterUsedThenCorrectValueReturned) {
    using INTERFACE_DESCRIPTOR_DATA = typename EncodeStates<FamilyType>::INTERFACE_DESCRIPTOR_DATA;
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::KERNELSTARTPOINTER_ALIGN_SIZE, pDevice->getGfxCoreHelper().getKernelIsaPointerAlignment());
}

HWTEST2_F(EncodeDispatchKernelTest, givenKernelsSharingISAParentAllocationsWhenProgrammingWalkerThenKernelStartPointerHasProperOffset, IsAtLeastXeCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA = typename EncodeStates<FamilyType>::INTERFACE_DESCRIPTOR_DATA;

    auto dispatchInterface = std::make_unique<MockDispatchKernelEncoder>();
    dispatchInterface->getIsaOffsetInParentAllocationResult = 8 << INTERFACE_DESCRIPTOR_DATA::KERNELSTARTPOINTER_BIT_SHIFT;
    uint32_t dims[] = {2, 1, 1};
    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());
    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
    EXPECT_EQ(walkerCmd->getInterfaceDescriptor().getKernelStartPointer(), dispatchInterface->getIsaAllocation()->getGpuAddressToPatch() + dispatchInterface->getIsaOffsetInParentAllocation());
}

HWTEST2_F(EncodeDispatchKernelTest, givenPrintKernelDispatchParametersWhenEncodingKernelThenPrintKernelDispatchParams, IsAtLeastXeCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    auto dispatchInterface = std::make_unique<MockDispatchKernelEncoder>();

    uint32_t dims[] = {2, 1, 1};
    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    DebugManagerStateRestore restore;
    debugManager.flags.PrintKernelDispatchParameters.set(true);
    StreamCapture capture;
    capture.captureStdout(); // start capturing
    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);
    std::string outputString = capture.getCapturedStdout(); // stop capturing

    EXPECT_NE(std::string::npos, outputString.find("kernel"));
    EXPECT_NE(std::string::npos, outputString.find("grfCount"));
    EXPECT_NE(std::string::npos, outputString.find("simdSize"));
    EXPECT_NE(std::string::npos, outputString.find("tilesCount"));
    EXPECT_NE(std::string::npos, outputString.find("implicitScaling"));
    EXPECT_NE(std::string::npos, outputString.find("threadGroupCount"));
    EXPECT_NE(std::string::npos, outputString.find("numberOfThreadsInGpgpuThreadGroup"));
    EXPECT_NE(std::string::npos, outputString.find("threadGroupDimensions"));
    EXPECT_NE(std::string::npos, outputString.find("threadGroupDispatchSize enum"));
}

HWTEST2_F(EncodeDispatchKernelTest, givenNonBindlessOrStatelessArgWhenDispatchingKernelThenSurfaceStateOffsetInCrossThreadDataIsNotPatched, IsHeapfulRequired) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    uint32_t numBindingTable = 1;
    BINDING_TABLE_STATE bindingTableState = FamilyType::cmdInitBindingTableState;

    auto ssh = cmdContainer->getIndirectHeap(HeapType::surfaceState);
    auto ioh = cmdContainer->getIndirectHeap(HeapType::indirectObject);

    size_t sizeUsed = 0x20;
    ssh->getSpace(sizeUsed);
    sizeUsed = ssh->getUsed();

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    std::vector<uint8_t> storage;
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens = PatchTokensTestData::ValidEmptyKernel::create(storage);
    kernelTokens.tokens.kernelArgs.resize(1);
    kernelTokens.tokens.kernelArgs[0].objectType = NEO::PatchTokenBinary::ArgObjectType::buffer;

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
    dispatchInterface->kernelDescriptor.kernelAttributes.numGrfRequired = 128U;

    auto &arg = dispatchInterface->kernelDescriptor.payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>();
    arg.bindless = NEO::undefined<CrossThreadDataOffset>;
    arg.bindful = surfaceStateOffset;

    const uint8_t *sshData = reinterpret_cast<uint8_t *>(&bindingTableState);
    dispatchInterface->getSurfaceStateHeapDataResult = const_cast<uint8_t *>(sshData);
    dispatchInterface->getSurfaceStateHeapDataSizeResult = static_cast<uint32_t>(sizeof(BINDING_TABLE_STATE));

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    EXPECT_EQ(pattern, *patchLocation);

    iOpenCL::SPatchSamplerKernelArgument samplerArg = {};
    samplerArg.Token = iOpenCL::PATCH_TOKEN_SAMPLER_KERNEL_ARGUMENT;
    samplerArg.ArgumentNumber = 1;
    samplerArg.Offset = surfaceStateOffset;
    samplerArg.Type = iOpenCL::SAMPLER_OBJECT_TEXTURE;
    kernelTokens.tokens.kernelArgs[0].objectArg = &samplerArg;
    kernelTokens.tokens.kernelArgs[0].objectType = NEO::PatchTokenBinary::ArgObjectType::sampler;

    dispatchInterface.reset(new MockDispatchKernelEncoder());

    NEO::populateKernelDescriptor(dispatchInterface->kernelDescriptor, kernelTokens, sizeof(void *));

    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.numEntries = numBindingTable;
    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.tableOffset = 0U;
    dispatchInterface->kernelDescriptor.kernelAttributes.numGrfRequired = 128U;

    sshData = reinterpret_cast<uint8_t *>(&bindingTableState);
    dispatchInterface->getSurfaceStateHeapDataResult = const_cast<uint8_t *>(sshData);
    dispatchInterface->getSurfaceStateHeapDataSizeResult = static_cast<uint32_t>(sizeof(BINDING_TABLE_STATE));

    ioh->replaceBuffer(ioh->getCpuBase(), ioh->getMaxAvailableSpace());
    memset(ioh->getCpuBase(), 0, ioh->getMaxAvailableSpace());

    dispatchArgs.dispatchInterface = dispatchInterface.get();
    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    EXPECT_TRUE(memoryZeroed(ptrOffset(ioh->getCpuBase(), iohOffset), ioh->getMaxAvailableSpace() - iohOffset));
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, WalkerThreadTest, givenStartWorkGroupWhenIndirectIsFalseThenExpectStartGroupAndThreadDimensionsProgramming) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    DefaultWalkerType walkerCmd = FamilyType::cmdInitGpgpuWalker;
    startWorkGroup[1] = 2u;
    startWorkGroup[2] = 3u;

    EncodeDispatchKernel<FamilyType>::encodeThreadData(walkerCmd, startWorkGroup, numWorkGroups, workGroupSizes, simd, localIdDimensions,
                                                       0, 0, true, false, false, requiredWorkGroupOrder, rootDeviceEnvironment);
    EXPECT_FALSE(walkerCmd.getIndirectParameterEnable());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdXDimension());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdYDimension());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdZDimension());

    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingX());
    EXPECT_EQ(2u, walkerCmd.getThreadGroupIdStartingY());
    EXPECT_EQ(3u, walkerCmd.getThreadGroupIdStartingResumeZ());

    auto expectedSimd = getSimdConfig<DefaultWalkerType>(simd);
    EXPECT_EQ(expectedSimd, walkerCmd.getSimdSize());
    EXPECT_EQ(1u, walkerCmd.getThreadWidthCounterMaximum());

    EXPECT_EQ(0xffffffffu, walkerCmd.getRightExecutionMask());
    EXPECT_EQ(0xffffffffu, walkerCmd.getBottomExecutionMask());
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, WalkerThreadTest, givenNoStartWorkGroupWhenIndirectIsTrueThenExpectNoStartGroupAndThreadDimensionsProgramming) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    DefaultWalkerType walkerCmd = FamilyType::cmdInitGpgpuWalker;
    startWorkGroup[1] = 2u;
    startWorkGroup[2] = 3u;
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    EncodeDispatchKernel<FamilyType>::encodeThreadData(walkerCmd, nullptr, numWorkGroups, workGroupSizes, simd, localIdDimensions,
                                                       0, 0, true, false, true, requiredWorkGroupOrder, rootDeviceEnvironment);
    EXPECT_TRUE(walkerCmd.getIndirectParameterEnable());
    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdXDimension());
    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdYDimension());
    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdZDimension());

    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingX());
    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingY());
    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingResumeZ());

    auto expectedSimd = getSimdConfig<DefaultWalkerType>(simd);
    EXPECT_EQ(expectedSimd, walkerCmd.getSimdSize());
    EXPECT_EQ(1u, walkerCmd.getThreadWidthCounterMaximum());

    EXPECT_EQ(0xffffffffu, walkerCmd.getRightExecutionMask());
    EXPECT_EQ(0xffffffffu, walkerCmd.getBottomExecutionMask());
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, WalkerThreadTest, givenStartWorkGroupWhenWorkGroupSmallerThanSimdThenExpectStartGroupAndRightExecutionMaskNotFull) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    DefaultWalkerType walkerCmd = FamilyType::cmdInitGpgpuWalker;
    startWorkGroup[1] = 2u;
    startWorkGroup[2] = 3u;
    workGroupSizes[0] = 30u;
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    EncodeDispatchKernel<FamilyType>::encodeThreadData(walkerCmd, startWorkGroup, numWorkGroups, workGroupSizes, simd, localIdDimensions,
                                                       0, 0, true, false, false, requiredWorkGroupOrder, rootDeviceEnvironment);
    EXPECT_FALSE(walkerCmd.getIndirectParameterEnable());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdXDimension());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdYDimension());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdZDimension());

    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingX());
    EXPECT_EQ(2u, walkerCmd.getThreadGroupIdStartingY());
    EXPECT_EQ(3u, walkerCmd.getThreadGroupIdStartingResumeZ());

    auto expectedSimd = getSimdConfig<DefaultWalkerType>(simd);
    EXPECT_EQ(expectedSimd, walkerCmd.getSimdSize());
    EXPECT_EQ(1u, walkerCmd.getThreadWidthCounterMaximum());

    EXPECT_EQ(0x3fffffffu, walkerCmd.getRightExecutionMask());
    EXPECT_EQ(0xffffffffu, walkerCmd.getBottomExecutionMask());
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, WalkerThreadTest, WhenThreadPerThreadGroupNotZeroThenExpectOverrideThreadGroupCalculation) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    DefaultWalkerType walkerCmd = FamilyType::cmdInitGpgpuWalker;
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    uint32_t expectedThreadPerThreadGroup = 5u;
    EncodeDispatchKernel<FamilyType>::encodeThreadData(walkerCmd, startWorkGroup, numWorkGroups, workGroupSizes, simd, localIdDimensions,
                                                       expectedThreadPerThreadGroup, 0, true, false, false, requiredWorkGroupOrder, rootDeviceEnvironment);
    EXPECT_FALSE(walkerCmd.getIndirectParameterEnable());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdXDimension());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdYDimension());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdZDimension());

    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingX());
    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingY());
    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingResumeZ());

    auto expectedSimd = getSimdConfig<DefaultWalkerType>(simd);
    EXPECT_EQ(expectedSimd, walkerCmd.getSimdSize());
    EXPECT_EQ(expectedThreadPerThreadGroup, walkerCmd.getThreadWidthCounterMaximum());

    EXPECT_EQ(0xffffffffu, walkerCmd.getRightExecutionMask());
    EXPECT_EQ(0xffffffffu, walkerCmd.getBottomExecutionMask());
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, WalkerThreadTest, WhenExecutionMaskNotZeroThenExpectOverrideExecutionMaskCalculation) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    DefaultWalkerType walkerCmd = FamilyType::cmdInitGpgpuWalker;
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    uint32_t expectedExecutionMask = 0xFFFFu;
    EncodeDispatchKernel<FamilyType>::encodeThreadData(walkerCmd, startWorkGroup, numWorkGroups, workGroupSizes, simd, localIdDimensions,
                                                       0, expectedExecutionMask, true, false, false, requiredWorkGroupOrder, rootDeviceEnvironment);
    EXPECT_FALSE(walkerCmd.getIndirectParameterEnable());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdXDimension());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdYDimension());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdZDimension());

    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingX());
    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingY());
    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingResumeZ());

    auto expectedSimd = getSimdConfig<DefaultWalkerType>(simd);
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
    debugManager.flags.EnablePassInlineData.set(0);

    NEO::KernelDescriptor kernelDesc;
    kernelDesc.kernelAttributes.flags.passInlineData = true;

    EXPECT_FALSE(EncodeDispatchKernel<FamilyType>::inlineDataProgrammingRequired(kernelDesc));
}

HWTEST_F(WalkerThreadTest, givenDebugFlagEnabledWhenKernelDescriptorInlineDataEnabledThenReturnInlineRequired) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnablePassInlineData.set(1);

    NEO::KernelDescriptor kernelDesc;
    kernelDesc.kernelAttributes.flags.passInlineData = true;

    EXPECT_TRUE(EncodeDispatchKernel<FamilyType>::inlineDataProgrammingRequired(kernelDesc));
}

HWTEST_F(WalkerThreadTest, givenDebugFlagEnabledWhenKernelDescriptorInlineDataDisabledThenReturnInlineNotRequired) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnablePassInlineData.set(1);

    NEO::KernelDescriptor kernelDesc;
    kernelDesc.kernelAttributes.flags.passInlineData = false;

    EXPECT_FALSE(EncodeDispatchKernel<FamilyType>::inlineDataProgrammingRequired(kernelDesc));
}

using namespace NEO;

using InterfaceDescriptorDataTests = ::testing::Test;

HWCMDTEST_F(IGFX_GEN12LP_CORE, InterfaceDescriptorDataTests, givenVariousValuesWhenCallingSetBarrierEnableThenCorrectValueIsSet) {
    using INTERFACE_DESCRIPTOR_DATA = typename EncodeStates<FamilyType>::INTERFACE_DESCRIPTOR_DATA;
    INTERFACE_DESCRIPTOR_DATA idd = FamilyType::cmdInitInterfaceDescriptorData;
    MockDevice device;
    auto hwInfo = device.getHardwareInfo();
    KernelDescriptor kd = {};
    kd.kernelAttributes.barrierCount = 0;
    EncodeDispatchKernel<FamilyType>::programBarrierEnable(idd, kd, hwInfo);
    EXPECT_FALSE(idd.getBarrierEnable());

    kd.kernelAttributes.barrierCount = 1;
    EncodeDispatchKernel<FamilyType>::programBarrierEnable(idd, kd, hwInfo);
    EXPECT_TRUE(idd.getBarrierEnable());

    kd.kernelAttributes.barrierCount = 2;
    EncodeDispatchKernel<FamilyType>::programBarrierEnable(idd, kd, hwInfo);
    EXPECT_TRUE(idd.getBarrierEnable());
}

using BindlessCommandEncodeStatesTest = Test<BindlessCommandEncodeStatesFixture>;
using BindlessCommandEncodeStatesContainerTest = Test<CommandEncodeStatesFixture>;

HWTEST_F(BindlessCommandEncodeStatesContainerTest, givenBindlessKernelAndBindlessModeEnabledWhenEncodingKernelThenCmdContainerHasNullptrSSH) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.UseBindlessMode.set(1);
    debugManager.flags.UseExternalAllocatorForSshAndDsh.set(1);

    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->createBindlessHeapsHelper(pDevice,
                                                                                                                         pDevice->getNumGenericSubDevices() > 1);

    auto commandContainer = std::make_unique<CommandContainer>();
    commandContainer->initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);
    commandContainer->setDirtyStateForAllHeaps(false);
    commandContainer->l1CachePolicyDataRef() = &l1CachePolicyData;

    uint32_t numBindingTable = 1;
    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.numEntries = numBindingTable;
    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.tableOffset = 0U;
    dispatchInterface->kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::BindlessAndStateless;

    bool requiresUncachedMocs = false;
    EXPECT_EQ(commandContainer->getIndirectHeap(HeapType::surfaceState), nullptr);
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*commandContainer.get(), dispatchArgs);

    EXPECT_EQ(commandContainer->getIndirectHeap(HeapType::surfaceState), nullptr);
}

HWTEST2_F(BindlessCommandEncodeStatesContainerTest, givenBindfulKernelWhenBindlessModeEnabledThenCmdContainerHasSsh, IsHeapfulRequired) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.UseBindlessMode.set(1);
    auto commandContainer = std::make_unique<CommandContainer>();
    commandContainer->initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);
    commandContainer->setDirtyStateForAllHeaps(false);
    commandContainer->l1CachePolicyDataRef() = &l1CachePolicyData;
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->createBindlessHeapsHelper(pDevice,
                                                                                                                         pDevice->getNumGenericSubDevices() > 1);
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
    EXPECT_NE(commandContainer->getIndirectHeap(HeapType::surfaceState), nullptr);
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*commandContainer.get(), dispatchArgs);

    EXPECT_NE(commandContainer->getIndirectHeap(HeapType::surfaceState), nullptr);
}

using NgenGeneratorDispatchKernelEncodeTest = Test<CommandEncodeStatesFixture>;

HWTEST2_F(NgenGeneratorDispatchKernelEncodeTest, givenBindfulKernelAndIsNotGeneratedByIgcWhenEncodeDispatchKernelThenCmdContainerDoesNotHaveSsh, IsPVC) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    for (auto isGeneratedByIgc : {false, true}) {
        auto commandContainer = std::make_unique<MyMockCommandContainer>();
        commandContainer->initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);
        commandContainer->setDirtyStateForAllHeaps(false);
        commandContainer->l1CachePolicyDataRef() = &l1CachePolicyData;

        if (isGeneratedByIgc == false) {
            commandContainer->indirectHeaps[HeapType::surfaceState].reset(nullptr);
        }

        uint32_t numBindingTable = 1;
        BINDING_TABLE_STATE bindingTableState = FamilyType::cmdInitBindingTableState;

        uint32_t dims[] = {1, 1, 1};
        std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

        dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.numEntries = numBindingTable;
        dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.tableOffset = 0U;
        dispatchInterface->kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::BindfulAndStateless;
        dispatchInterface->kernelDescriptor.kernelMetadata.isGeneratedByIgc = isGeneratedByIgc;

        const uint8_t *sshData = reinterpret_cast<uint8_t *>(&bindingTableState);
        dispatchInterface->getSurfaceStateHeapDataResult = const_cast<uint8_t *>(sshData);
        dispatchInterface->getSurfaceStateHeapDataSizeResult = static_cast<uint32_t>(sizeof(BINDING_TABLE_STATE));

        bool requiresUncachedMocs = false;

        EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

        EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*commandContainer.get(), dispatchArgs);

        if (isGeneratedByIgc) {
            EXPECT_NE(commandContainer->getIndirectHeap(HeapType::surfaceState), nullptr);
        } else {
            EXPECT_EQ(commandContainer->getIndirectHeap(HeapType::surfaceState), nullptr);
        }
    }
}

HWTEST_F(CommandEncodeStatesTest, givenKernelInfoWhenGettingRequiredDshSpaceThenReturnCorrectValues) {
    using INTERFACE_DESCRIPTOR_DATA = typename EncodeStates<FamilyType>::INTERFACE_DESCRIPTOR_DATA;
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;

    size_t additionalSize = UnitTestHelper<FamilyType>::getAdditionalDshSize(cmdContainer->getNumIddPerBlock());
    size_t expectedSize = alignUp(additionalSize, EncodeStates<FamilyType>::alignInterfaceDescriptorData);

    // no samplers
    kernelInfo.kernelDescriptor.payloadMappings.samplerTable.numSamplers = 0;
    size_t size = EncodeDispatchKernel<FamilyType>::getSizeRequiredDsh(kernelInfo.kernelDescriptor, cmdContainer->getNumIddPerBlock());
    EXPECT_EQ(expectedSize, size);

    // two samplers, no border color state
    kernelInfo.kernelDescriptor.payloadMappings.samplerTable.numSamplers = 2;
    kernelInfo.kernelDescriptor.payloadMappings.samplerTable.tableOffset = 0;
    kernelInfo.kernelDescriptor.payloadMappings.samplerTable.borderColor = 0;

    // align samplers
    size_t alignedSamplers = alignUp(2 * sizeof(SAMPLER_STATE), InterfaceDescriptorTraits<INTERFACE_DESCRIPTOR_DATA>::samplerStatePointerAlignSize);

    // additional IDD for requiring platforms
    if (additionalSize > 0) {
        expectedSize = alignUp(alignedSamplers + additionalSize, EncodeStates<FamilyType>::alignInterfaceDescriptorData);
    } else {
        expectedSize = alignedSamplers;
    }

    size = EncodeDispatchKernel<FamilyType>::getSizeRequiredDsh(kernelInfo.kernelDescriptor, cmdContainer->getNumIddPerBlock());
    EXPECT_EQ(expectedSize, size);

    // three samplers, border color state
    kernelInfo.kernelDescriptor.payloadMappings.samplerTable.numSamplers = 3;
    kernelInfo.kernelDescriptor.payloadMappings.samplerTable.tableOffset = 32;

    // align border color state and samplers
    alignedSamplers = alignUp(alignUp(32, FamilyType::cacheLineSize) + 3 * sizeof(SAMPLER_STATE), InterfaceDescriptorTraits<INTERFACE_DESCRIPTOR_DATA>::samplerStatePointerAlignSize);

    // additional IDD for requiring platforms
    if (additionalSize > 0) {
        expectedSize = alignUp(alignedSamplers + additionalSize, EncodeStates<FamilyType>::alignInterfaceDescriptorData);
    } else {
        expectedSize = alignedSamplers;
    }
    size = EncodeDispatchKernel<FamilyType>::getSizeRequiredDsh(kernelInfo.kernelDescriptor, cmdContainer->getNumIddPerBlock());
    EXPECT_EQ(expectedSize, size);
}

HWTEST_F(CommandEncodeStatesTest, givenKernelInfoWhenGettingRequiredSshSpaceThenReturnCorrectValues) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    // no surface states
    kernelInfo.heapInfo.surfaceStateHeapSize = 0;
    size_t size = EncodeDispatchKernel<FamilyType>::getSizeRequiredSsh(kernelInfo);
    EXPECT_EQ(0u, size);

    // two surface states and BTI indices
    kernelInfo.heapInfo.surfaceStateHeapSize = 2 * sizeof(RENDER_SURFACE_STATE) + 2 * sizeof(uint32_t);
    size_t expectedSize = alignUp(kernelInfo.heapInfo.surfaceStateHeapSize, FamilyType::cacheLineSize);

    size = EncodeDispatchKernel<FamilyType>::getSizeRequiredSsh(kernelInfo);
    EXPECT_EQ(expectedSize, size);
}

HWTEST_F(CommandEncodeStatesTest, givenKernelInfoOfBindlessKernelWhenGettingRequiredSshSpaceThenReturnCorrectValues) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    kernelInfo.kernelDescriptor.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::BindlessAndStateless;
    kernelInfo.kernelDescriptor.kernelAttributes.imageAddressingMode = NEO::KernelDescriptor::Bindless;
    // no surface states
    kernelInfo.heapInfo.surfaceStateHeapSize = 0;
    kernelInfo.kernelDescriptor.kernelAttributes.numArgsStateful = 0;
    size_t size = EncodeDispatchKernel<FamilyType>::getSizeRequiredSsh(kernelInfo);
    EXPECT_EQ(0u, size);

    // two surface states
    kernelInfo.kernelDescriptor.kernelAttributes.numArgsStateful = 2;
    size_t expectedSize = alignUp(2 * sizeof(RENDER_SURFACE_STATE), FamilyType::cacheLineSize);

    size = EncodeDispatchKernel<FamilyType>::getSizeRequiredSsh(kernelInfo);
    EXPECT_EQ(expectedSize, size);
}

HWTEST_F(CommandEncodeStatesTest, givenCommandContainerWhenIsKernelDispatchedFromImmediateCmdListTrueThenGetHeapWithRequiredSizeAndAlignmentCalled) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    uint32_t dims[] = {1, 1, 1};
    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.isKernelDispatchedFromImmediateCmdList = true;
    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);
    EXPECT_NE(0u, cmdContainer->getHeapWithRequiredSizeAndAlignmentCalled);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandEncodeStatesTest, givenEncodeDispatchKernelWhenGettingInlineDataOffsetThenReturnZero) {
    EncodeDispatchKernelArgs dispatchArgs = {};

    EXPECT_EQ(0u, EncodeDispatchKernel<FamilyType>::getInlineDataOffset(dispatchArgs));
}
HWTEST2_F(CommandEncodeStatesTest, givenEncodeDispatchKernelWhenGetIohAlignmentThenOneReturned, IsAtMostXeCore) {
    EXPECT_EQ(NEO::EncodeDispatchKernel<FamilyType>::getDefaultIOHAlignment(), 1u);
}

HWTEST2_F(CommandEncodeStatesTest, givenEncodeDispatchKernelWhenGetIohAlignmentThenCacheLineSizeReturned, IsAtLeastXe2HpgCore) {
    EXPECT_EQ(NEO::EncodeDispatchKernel<FamilyType>::getDefaultIOHAlignment(), FamilyType::cacheLineSize);
}

HWTEST2_F(CommandEncodeStatesTest, givenEncodeDispatchKernelWhenForcingDifferentIohAlignmentThenExpectedAlignmentReturned, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore restorer;
    auto expectedAlignment = 1024u;
    debugManager.flags.ForceIOHAlignment.set(expectedAlignment);
    EXPECT_EQ(NEO::EncodeDispatchKernel<FamilyType>::getDefaultIOHAlignment(), expectedAlignment);
}

HWTEST2_F(CommandEncodeStatesTest, givenEncodeDispatchKernelWhenGettingThreadCountPerSubsliceThenUseDualSubSliceAsDenominator, IsAtMostXeCore) {
    auto &hwInfo = pDevice->getHardwareInfo();
    auto expectedValue = hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.DualSubSliceCount;
    EXPECT_EQ(expectedValue, NEO::EncodeDispatchKernel<FamilyType>::getThreadCountPerSubslice(hwInfo));
}

HWTEST2_F(CommandEncodeStatesTest, givenEncodeDispatchKernelWhenGettingThreadCountPerSubsliceThenUseSubSliceAsDenominator, IsAtLeastXe2HpgCore) {
    auto &hwInfo = pDevice->getHardwareInfo();
    auto expectedValue = hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.SubSliceCount;
    EXPECT_EQ(expectedValue, NEO::EncodeDispatchKernel<FamilyType>::getThreadCountPerSubslice(hwInfo));
}
