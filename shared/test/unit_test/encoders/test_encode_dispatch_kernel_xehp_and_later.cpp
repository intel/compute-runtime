/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_container/walker_partition_xehp_and_later.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/command_container_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_dispatch_kernel_encoder_interface.h"
#include "shared/test/common/test_macros/test.h"

#include "hw_cmds.h"

#include <memory>

using namespace NEO;

using CommandEncodeStatesTest = Test<CommandEncodeStatesFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenSlmTotalSizeGraterThanZeroWhenDispatchingKernelThenSharedMemorySizeIsSetCorrectly) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint32_t slmTotalSize = 1;

    dispatchInterface->getSlmTotalSizeResult = slmTotalSize;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmd = genCmdCast<WALKER_TYPE *>(*itor);
    auto &idd = cmd->getInterfaceDescriptor();

    uint32_t expectedValue = static_cast<typename INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE>(
        HwHelperHw<FamilyType>::get().computeSlmValues(pDevice->getHardwareInfo(), slmTotalSize));

    EXPECT_EQ(expectedValue, idd.getSharedLocalMemorySize());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenSimdSizeWhenDispatchingKernelThenSimdMessageIsSet) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint32_t simdSize = 32;
    dispatchInterface->kernelDescriptor.kernelAttributes.simdSize = simdSize;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmd = genCmdCast<WALKER_TYPE *>(*itor);
    EXPECT_EQ(getSimdConfig<WALKER_TYPE>(simdSize), cmd->getMessageSimd());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenSlmTotalSizeEqualZeroWhenDispatchingKernelThenSharedMemorySizeIsSetCorrectly) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint32_t slmTotalSize = 0;

    dispatchInterface->getSlmTotalSizeResult = slmTotalSize;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmd = genCmdCast<WALKER_TYPE *>(*itor);
    auto &idd = cmd->getInterfaceDescriptor();

    uint32_t expectedValue = INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_ENCODES_0K;

    EXPECT_EQ(expectedValue, idd.getSharedLocalMemorySize());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenOverrideSlmTotalSizeDebugVariableWhenDispatchingKernelThenSharedMemorySizeIsSetCorrectly) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    DebugManagerStateRestore restorer;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint32_t slmTotalSize = 0;

    dispatchInterface->getSlmTotalSizeResult = slmTotalSize;

    bool requiresUncachedMocs = false;
    int32_t maxValueToProgram = 0x8;

    for (int32_t valueToProgram = 0x0; valueToProgram < maxValueToProgram; valueToProgram++) {
        DebugManager.flags.OverrideSlmAllocationSize.set(valueToProgram);
        cmdContainer->reset();

        EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
        EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

        GenCmdList commands;
        CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());
        auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
        ASSERT_NE(itor, commands.end());
        auto cmd = genCmdCast<WALKER_TYPE *>(*itor);
        auto &idd = cmd->getInterfaceDescriptor();

        EXPECT_EQ(valueToProgram, idd.getSharedLocalMemorySize());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givennumBindingTableOneWhenDispatchingKernelThenBTOffsetIsCorrect) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    uint32_t numBindingTable = 1;
    BINDING_TABLE_STATE bindingTableState = FamilyType::cmdInitBindingTableState;

    auto ssh = cmdContainer->getIndirectHeap(HeapType::SURFACE_STATE);
    ssh->getSpace(0x20);
    uint32_t sizeUsed = static_cast<uint32_t>(ssh->getUsed());
    auto expectedOffset = alignUp(sizeUsed, BINDING_TABLE_STATE::SURFACESTATEPOINTER_ALIGN_SIZE);

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.numEntries = numBindingTable;
    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.tableOffset = 0;

    unsigned char *bindingTableStateRaw = reinterpret_cast<unsigned char *>(&bindingTableState);
    dispatchInterface->getSurfaceStateHeapDataResult = bindingTableStateRaw;
    dispatchInterface->getSurfaceStateHeapDataSizeResult = static_cast<uint32_t>(sizeof(BINDING_TABLE_STATE));

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmd = genCmdCast<WALKER_TYPE *>(*itor);
    auto &idd = cmd->getInterfaceDescriptor();

    EXPECT_EQ(idd.getBindingTablePointer(), expectedOffset);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, giveNumBindingTableZeroWhenDispatchingKernelThenBTOffsetIsZero) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    uint32_t numBindingTable = 0;
    BINDING_TABLE_STATE bindingTableState = FamilyType::cmdInitBindingTableState;

    auto ssh = cmdContainer->getIndirectHeap(HeapType::SURFACE_STATE);
    uint32_t sizeUsed = 0x20;
    ssh->getSpace(sizeUsed);

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.numEntries = numBindingTable;
    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.tableOffset = 0;

    unsigned char *bindingTableStateRaw = reinterpret_cast<unsigned char *>(&bindingTableState);
    dispatchInterface->getSurfaceStateHeapDataResult = bindingTableStateRaw;
    dispatchInterface->getSurfaceStateHeapDataSizeResult = static_cast<uint32_t>(sizeof(BINDING_TABLE_STATE));

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmd = genCmdCast<WALKER_TYPE *>(*itor);
    auto &idd = cmd->getInterfaceDescriptor();

    EXPECT_EQ(idd.getBindingTablePointer(), 0u);
}
struct SamplerSupportedMatcher {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        if constexpr (HwMapper<productFamily>::GfxProduct::supportsCmdSet(IGFX_XE_HP_CORE)) {
            return HwMapper<productFamily>::GfxProduct::supportsSampler;
        }
        return false;
    }
};

HWTEST2_F(CommandEncodeStatesTest, giveNumSamplersOneWhenDispatchKernelThensamplerStateWasCopied, SamplerSupportedMatcher) {
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    uint32_t numSamplers = 1;
    SAMPLER_STATE samplerState;
    memset(&samplerState, 2, sizeof(SAMPLER_STATE));

    auto dsh = cmdContainer->getIndirectHeap(HeapType::DYNAMIC_STATE);
    auto usedBefore = dsh->getUsed();

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    dispatchInterface->kernelDescriptor.payloadMappings.samplerTable.numSamplers = numSamplers;
    dispatchInterface->kernelDescriptor.payloadMappings.samplerTable.borderColor = 0;
    dispatchInterface->kernelDescriptor.payloadMappings.samplerTable.tableOffset = 0;

    unsigned char *samplerStateRaw = reinterpret_cast<unsigned char *>(&samplerState);
    dispatchInterface->getDynamicStateHeapDataResult = samplerStateRaw;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmd = genCmdCast<WALKER_TYPE *>(*itor);
    auto &idd = cmd->getInterfaceDescriptor();

    auto borderColorOffsetInDsh = usedBefore;
    samplerState.setIndirectStatePointer(static_cast<uint32_t>(borderColorOffsetInDsh));

    auto samplerStateOffset = idd.getSamplerStatePointer();

    auto pSmplr = reinterpret_cast<SAMPLER_STATE *>(ptrOffset(dsh->getCpuBase(), samplerStateOffset));
    EXPECT_EQ(memcmp(pSmplr, &samplerState, sizeof(SAMPLER_STATE)), 0);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenEventAllocationWhenDispatchingKernelThenPostSyncIsAdded) {
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint64_t eventAddress = MemoryConstants::cacheLineSize * 123;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.eventAddress = eventAddress;
    dispatchArgs.isTimestampEvent = true;
    dispatchArgs.L3FlushEnable = true;

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());
    auto cmd = genCmdCast<WALKER_TYPE *>(*itor);
    EXPECT_EQ(POSTSYNC_DATA::OPERATION_WRITE_TIMESTAMP, cmd->getPostSync().getOperation());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenEventAddressWhenEncodeThenMocsFromGmmHelperIsSet) {
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint64_t eventAddress = MemoryConstants::cacheLineSize * 123;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.eventAddress = eventAddress;
    dispatchArgs.isTimestampEvent = true;
    dispatchArgs.L3FlushEnable = true;
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());
    auto cmd = genCmdCast<WALKER_TYPE *>(*itor);
    if (MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo)) {
        EXPECT_EQ(pDevice->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED), cmd->getPostSync().getMocs());
    } else {
        EXPECT_EQ(pDevice->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER), cmd->getPostSync().getMocs());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenCleanHeapsWhenDispatchKernelThenFlushNotAdded) {
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

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenForceBtpPrefetchModeDebugFlagWhenDispatchingKernelThenCorrectValuesAreSetUp) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;

    DebugManagerStateRestore restorer;
    uint32_t dims[] = {2, 1, 1};
    uint32_t numBindingTable = 1;
    uint32_t numSamplers = 1;
    SAMPLER_STATE samplerState;
    BINDING_TABLE_STATE bindingTable;
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.numEntries = numBindingTable;
    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.tableOffset = 0;
    dispatchInterface->kernelDescriptor.payloadMappings.samplerTable.numSamplers = numSamplers;
    dispatchInterface->kernelDescriptor.payloadMappings.samplerTable.tableOffset = 0;
    dispatchInterface->kernelDescriptor.payloadMappings.samplerTable.borderColor = 0;
    unsigned char *samplerStateRaw = reinterpret_cast<unsigned char *>(&samplerState);
    dispatchInterface->getDynamicStateHeapDataResult = samplerStateRaw;
    unsigned char *bindingTableRaw = reinterpret_cast<unsigned char *>(&bindingTable);
    dispatchInterface->getSurfaceStateHeapDataResult = bindingTableRaw;

    {
        DebugManager.flags.ForceBtpPrefetchMode.set(-1);
        cmdContainer.reset(new MyMockCommandContainer());
        cmdContainer->initialize(pDevice, nullptr, true);

        bool requiresUncachedMocs = false;
        EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
        EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

        GenCmdList commands;
        CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());
        auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
        ASSERT_NE(itor, commands.end());
        auto cmd = genCmdCast<WALKER_TYPE *>(*itor);
        auto &idd = cmd->getInterfaceDescriptor();

        if (EncodeSurfaceState<FamilyType>::doBindingTablePrefetch()) {
            EXPECT_NE(0u, idd.getBindingTableEntryCount());
        } else {
            EXPECT_EQ(0u, idd.getBindingTableEntryCount());
        }

        if constexpr (FamilyType::supportsSampler) {
            if (EncodeSurfaceState<FamilyType>::doBindingTablePrefetch()) {
                EXPECT_NE(INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT_NO_SAMPLERS_USED, idd.getSamplerCount());
            } else {
                EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT_NO_SAMPLERS_USED, idd.getSamplerCount());
            }
        }
    }

    {
        DebugManager.flags.ForceBtpPrefetchMode.set(0);
        cmdContainer.reset(new MyMockCommandContainer());
        cmdContainer->initialize(pDevice, nullptr, true);

        bool requiresUncachedMocs = false;
        EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
        EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

        GenCmdList commands;
        CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());
        auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
        ASSERT_NE(itor, commands.end());
        auto cmd = genCmdCast<WALKER_TYPE *>(*itor);
        auto &idd = cmd->getInterfaceDescriptor();

        EXPECT_EQ(0u, idd.getBindingTableEntryCount());
        if constexpr (FamilyType::supportsSampler) {
            EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT_NO_SAMPLERS_USED, idd.getSamplerCount());
        }
    }

    {
        DebugManager.flags.ForceBtpPrefetchMode.set(1);
        cmdContainer.reset(new MyMockCommandContainer());
        cmdContainer->initialize(pDevice, nullptr, true);

        bool requiresUncachedMocs = false;
        EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
        EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

        GenCmdList commands;
        CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());
        auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
        ASSERT_NE(itor, commands.end());
        auto cmd = genCmdCast<WALKER_TYPE *>(*itor);
        auto &idd = cmd->getInterfaceDescriptor();

        EXPECT_NE(0u, idd.getBindingTableEntryCount());
        if constexpr (FamilyType::supportsSampler) {
            EXPECT_NE(INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT_NO_SAMPLERS_USED, idd.getSamplerCount());
        }
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenDispatchInterfaceWhenNumRequiredGrfIsNotDefaultThenStateComputeModeCommandAdded) {
    StreamProperties streamProperties{};
    streamProperties.stateComputeMode.setProperties(false, 128, 0u, *defaultHwInfo);
    streamProperties.stateComputeMode.setProperties(false, 128, 0u, *defaultHwInfo);
    EXPECT_FALSE(streamProperties.stateComputeMode.isDirty());

    streamProperties.stateComputeMode.setProperties(false, 256, 0u, *defaultHwInfo);
    EXPECT_TRUE(streamProperties.stateComputeMode.isDirty());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenInlineDataRequiredWhenEncodingWalkerThenEmitInlineParameterIsSet) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using InlineData = typename FamilyType::INLINE_DATA;
    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    dispatchInterface->kernelDescriptor.kernelAttributes.flags.passInlineData = true;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);
    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmd = genCmdCast<WALKER_TYPE *>(*itor);
    EXPECT_EQ(1u, cmd->getEmitInlineParameter());

    const uint32_t inlineDataSize = sizeof(InlineData);
    size_t expectedSizeIOH = dispatchInterface->getCrossThreadDataSize() +
                             dispatchInterface->getPerThreadDataSizeForWholeThreadGroup() -
                             inlineDataSize;
    auto heap = cmdContainer->getIndirectHeap(HeapType::INDIRECT_OBJECT);
    EXPECT_EQ(expectedSizeIOH, heap->getUsed());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenInlineDataRequiredIsFalseWhenEncodingWalkerThenEmitInlineParameterIsNotSet) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    dispatchInterface->kernelDescriptor.kernelAttributes.flags.passInlineData = false;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmd = genCmdCast<WALKER_TYPE *>(*itor);
    EXPECT_EQ(0u, cmd->getEmitInlineParameter());

    size_t expectedSizeIOH = dispatchInterface->getCrossThreadDataSize() +
                             dispatchInterface->getPerThreadDataSizeForWholeThreadGroup();
    auto heap = cmdContainer->getIndirectHeap(HeapType::INDIRECT_OBJECT);
    EXPECT_EQ(expectedSizeIOH, heap->getUsed());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenInlineDataRequiredAndZeroCrossThreadDataSizeWhenEncodingWalkerThenEmitInlineParameterIsNotSet) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    dispatchInterface->kernelDescriptor.kernelAttributes.flags.passInlineData = true;
    dispatchInterface->getCrossThreadDataSizeResult = 0u;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmd = genCmdCast<WALKER_TYPE *>(*itor);
    EXPECT_EQ(0u, cmd->getEmitInlineParameter());

    auto heap = cmdContainer->getIndirectHeap(HeapType::INDIRECT_OBJECT);
    EXPECT_EQ(dispatchInterface->getPerThreadDataSizeForWholeThreadGroup(), heap->getUsed());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenInlineDataRequiredAndLocalIdsGenerateByHwWhenEncodingWalkerThenEmitInlineParameterAndGenerateLocalIdsIsSet) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->requiredWalkGroupOrder = 2u;
    dispatchInterface->kernelDescriptor.kernelAttributes.flags.passInlineData = true;
    dispatchInterface->kernelDescriptor.kernelAttributes.numLocalIdChannels = 3u;
    dispatchInterface->kernelDescriptor.kernelAttributes.simdSize = 32u;
    dispatchInterface->getCrossThreadDataSizeResult = 32u;

    dispatchInterface->requiresGenerationOfLocalIdsByRuntimeResult = false;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmd = genCmdCast<WALKER_TYPE *>(*itor);
    EXPECT_EQ(1u, cmd->getEmitInlineParameter());
    EXPECT_EQ(1u, cmd->getGenerateLocalId());
    uint32_t expectedEmitLocalIds = (1 << 0) | (1 << 1) | (1 << 2);
    EXPECT_EQ(expectedEmitLocalIds, cmd->getEmitLocalId());
    EXPECT_EQ(2u, cmd->getWalkOrder());
    EXPECT_EQ(31u, cmd->getLocalXMaximum());
    EXPECT_EQ(0u, cmd->getLocalYMaximum());
    EXPECT_EQ(0u, cmd->getLocalZMaximum());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenInterfaceDescriptorDataWhenForceThreadGroupDispatchSizeVariableIsDefaultThenThreadGroupDispatchSizeIsNotChanged) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    INTERFACE_DESCRIPTOR_DATA iddArg;
    iddArg = FamilyType::cmdInitInterfaceDescriptorData;
    const uint32_t forceThreadGroupDispatchSize = -1;
    auto hwInfo = pDevice->getHardwareInfo();
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);

    DebugManagerStateRestore restorer;
    DebugManager.flags.ForceThreadGroupDispatchSize.set(forceThreadGroupDispatchSize);

    uint32_t revisions[] = {REVISION_A0, REVISION_B};
    for (auto revision : revisions) {
        hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(revision, hwInfo);
        EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, hwInfo);

        if (hwInfoConfig.isDisableOverdispatchAvailable(hwInfo)) {
            EXPECT_EQ(3u, iddArg.getThreadGroupDispatchSize());
        } else {
            EXPECT_EQ(0u, iddArg.getThreadGroupDispatchSize());
        }
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenInterfaceDescriptorDataWhenForceThreadGroupDispatchSizeVariableIsSetThenThreadGroupDispatchSizeIsChanged) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    INTERFACE_DESCRIPTOR_DATA iddArg;
    iddArg = FamilyType::cmdInitInterfaceDescriptorData;
    const uint32_t forceThreadGroupDispatchSize = 1;
    const uint32_t defaultThreadGroupDispatchSize = iddArg.getThreadGroupDispatchSize();

    DebugManagerStateRestore restorer;
    DebugManager.flags.ForceThreadGroupDispatchSize.set(forceThreadGroupDispatchSize);

    EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, pDevice->getHardwareInfo());

    EXPECT_NE(defaultThreadGroupDispatchSize, iddArg.getThreadGroupDispatchSize());
    EXPECT_EQ(forceThreadGroupDispatchSize, iddArg.getThreadGroupDispatchSize());
}

using WalkerThreadTestXeHPAndLater = WalkerThreadTest;

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerThreadTestXeHPAndLater, givenStartWorkGroupWhenIndirectIsFalseThenExpectStartGroupAndThreadDimensionsProgramming) {
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
    EXPECT_EQ(3u, walkerCmd.getThreadGroupIdStartingZ());

    auto expectedSimd = getSimdConfig<WALKER_TYPE>(simd);
    EXPECT_EQ(expectedSimd, walkerCmd.getSimdSize());
    EXPECT_EQ(expectedSimd, walkerCmd.getMessageSimd());

    EXPECT_EQ(0xffffffffu, walkerCmd.getExecutionMask());

    EXPECT_EQ(0u, walkerCmd.getEmitLocalId());
    EXPECT_EQ(0u, walkerCmd.getLocalXMaximum());
    EXPECT_EQ(0u, walkerCmd.getLocalYMaximum());
    EXPECT_EQ(0u, walkerCmd.getLocalZMaximum());
    EXPECT_EQ(0u, walkerCmd.getWalkOrder());

    EXPECT_FALSE(walkerCmd.getGenerateLocalId());
    EXPECT_FALSE(walkerCmd.getEmitInlineParameter());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerThreadTestXeHPAndLater, givenNoStartWorkGroupWhenIndirectIsTrueThenExpectNoStartGroupAndThreadDimensionsProgramming) {
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
    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingZ());

    auto expectedSimd = getSimdConfig<WALKER_TYPE>(simd);
    EXPECT_EQ(expectedSimd, walkerCmd.getSimdSize());
    EXPECT_EQ(expectedSimd, walkerCmd.getMessageSimd());

    EXPECT_EQ(0xffffffffu, walkerCmd.getExecutionMask());

    EXPECT_EQ(0u, walkerCmd.getEmitLocalId());
    EXPECT_EQ(0u, walkerCmd.getLocalXMaximum());
    EXPECT_EQ(0u, walkerCmd.getLocalYMaximum());
    EXPECT_EQ(0u, walkerCmd.getLocalZMaximum());
    EXPECT_EQ(0u, walkerCmd.getWalkOrder());

    EXPECT_FALSE(walkerCmd.getGenerateLocalId());
    EXPECT_FALSE(walkerCmd.getEmitInlineParameter());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerThreadTestXeHPAndLater, givenSimdSizeOneWhenWorkGroupSmallerThanSimdThenExpectSimdSizeAsMaxAndExecutionMaskFull) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

    WALKER_TYPE walkerCmd = FamilyType::cmdInitGpgpuWalker;
    startWorkGroup[1] = 2u;
    startWorkGroup[2] = 3u;
    workGroupSizes[0] = 30u;
    simd = 1;

    EncodeDispatchKernel<FamilyType>::encodeThreadData(walkerCmd, startWorkGroup, numWorkGroups, workGroupSizes, simd, localIdDimensions,
                                                       0, 0, true, false, false, requiredWorkGroupOrder);
    EXPECT_FALSE(walkerCmd.getIndirectParameterEnable());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdXDimension());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdYDimension());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdZDimension());

    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingX());
    EXPECT_EQ(2u, walkerCmd.getThreadGroupIdStartingY());
    EXPECT_EQ(3u, walkerCmd.getThreadGroupIdStartingZ());

    auto expectedSimd = getSimdConfig<WALKER_TYPE>(simd);
    EXPECT_EQ(expectedSimd, walkerCmd.getSimdSize());
    EXPECT_EQ(expectedSimd, walkerCmd.getMessageSimd());

    EXPECT_EQ(0xffffffffu, walkerCmd.getExecutionMask());

    EXPECT_EQ(0u, walkerCmd.getEmitLocalId());
    EXPECT_EQ(0u, walkerCmd.getLocalXMaximum());
    EXPECT_EQ(0u, walkerCmd.getLocalYMaximum());
    EXPECT_EQ(0u, walkerCmd.getLocalZMaximum());
    EXPECT_EQ(0u, walkerCmd.getWalkOrder());

    EXPECT_FALSE(walkerCmd.getGenerateLocalId());
    EXPECT_FALSE(walkerCmd.getEmitInlineParameter());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerThreadTestXeHPAndLater, givenStartWorkGroupWhenWorkGroupSmallerThanSimdThenExpectStartGroupAndExecutionMaskNotFull) {
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
    EXPECT_EQ(3u, walkerCmd.getThreadGroupIdStartingZ());

    auto expectedSimd = getSimdConfig<WALKER_TYPE>(simd);
    EXPECT_EQ(expectedSimd, walkerCmd.getSimdSize());
    EXPECT_EQ(expectedSimd, walkerCmd.getMessageSimd());

    EXPECT_EQ(0x3fffffffu, walkerCmd.getExecutionMask());

    EXPECT_EQ(0u, walkerCmd.getEmitLocalId());
    EXPECT_EQ(0u, walkerCmd.getLocalXMaximum());
    EXPECT_EQ(0u, walkerCmd.getLocalYMaximum());
    EXPECT_EQ(0u, walkerCmd.getLocalZMaximum());
    EXPECT_EQ(0u, walkerCmd.getWalkOrder());

    EXPECT_FALSE(walkerCmd.getGenerateLocalId());
    EXPECT_FALSE(walkerCmd.getEmitInlineParameter());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerThreadTestXeHPAndLater, givenLocalIdGenerationByHwWhenNoLocalIdsPresentThenExpectNoEmitAndGenerateLocalIds) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

    WALKER_TYPE walkerCmd = FamilyType::cmdInitGpgpuWalker;
    localIdDimensions = 0u;

    EncodeDispatchKernel<FamilyType>::encodeThreadData(walkerCmd, nullptr, numWorkGroups, workGroupSizes, simd, localIdDimensions,
                                                       0, 0, false, false, false, requiredWorkGroupOrder);
    EXPECT_FALSE(walkerCmd.getIndirectParameterEnable());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdXDimension());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdYDimension());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdZDimension());

    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingX());
    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingY());
    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingZ());

    auto expectedSimd = getSimdConfig<WALKER_TYPE>(simd);
    EXPECT_EQ(expectedSimd, walkerCmd.getSimdSize());
    EXPECT_EQ(expectedSimd, walkerCmd.getMessageSimd());

    EXPECT_EQ(0xffffffffu, walkerCmd.getExecutionMask());

    EXPECT_EQ(0u, walkerCmd.getEmitLocalId());
    EXPECT_EQ(0u, walkerCmd.getLocalXMaximum());
    EXPECT_EQ(0u, walkerCmd.getLocalYMaximum());
    EXPECT_EQ(0u, walkerCmd.getLocalZMaximum());
    EXPECT_EQ(0u, walkerCmd.getWalkOrder());

    EXPECT_FALSE(walkerCmd.getGenerateLocalId());
    EXPECT_FALSE(walkerCmd.getEmitInlineParameter());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerThreadTestXeHPAndLater, givenLocalIdGenerationByHwWhenLocalIdsPresentThenExpectEmitAndGenerateLocalIds) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

    WALKER_TYPE walkerCmd = FamilyType::cmdInitGpgpuWalker;
    requiredWorkGroupOrder = 2u;
    workGroupSizes[1] = workGroupSizes[2] = 2u;

    EncodeDispatchKernel<FamilyType>::encodeThreadData(walkerCmd, nullptr, numWorkGroups, workGroupSizes, simd, localIdDimensions,
                                                       0, 0, false, false, false, requiredWorkGroupOrder);
    EXPECT_FALSE(walkerCmd.getIndirectParameterEnable());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdXDimension());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdYDimension());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdZDimension());

    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingX());
    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingY());
    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingZ());

    auto expectedSimd = getSimdConfig<WALKER_TYPE>(simd);
    EXPECT_EQ(expectedSimd, walkerCmd.getSimdSize());
    EXPECT_EQ(expectedSimd, walkerCmd.getMessageSimd());

    EXPECT_EQ(0xffffffffu, walkerCmd.getExecutionMask());

    uint32_t expectedEmitLocalIds = (1 << 0) | (1 << 1) | (1 << 2);
    EXPECT_EQ(expectedEmitLocalIds, walkerCmd.getEmitLocalId());
    EXPECT_EQ(31u, walkerCmd.getLocalXMaximum());
    EXPECT_EQ(1u, walkerCmd.getLocalYMaximum());
    EXPECT_EQ(1u, walkerCmd.getLocalZMaximum());
    EXPECT_EQ(2u, walkerCmd.getWalkOrder());

    EXPECT_TRUE(walkerCmd.getGenerateLocalId());
    EXPECT_FALSE(walkerCmd.getEmitInlineParameter());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerThreadTestXeHPAndLater, givenDebugVariableToOverrideSimdMessageSizeWhenWalkerIsProgrammedItIsOverwritten) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ForceSimdMessageSizeInWalker.set(1);

    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

    WALKER_TYPE walkerCmd = FamilyType::cmdInitGpgpuWalker;
    requiredWorkGroupOrder = 2u;
    workGroupSizes[1] = workGroupSizes[2] = 2u;

    EncodeDispatchKernel<FamilyType>::encodeThreadData(walkerCmd, nullptr, numWorkGroups, workGroupSizes, simd, localIdDimensions,
                                                       0, 0, false, false, false, requiredWorkGroupOrder);
    EXPECT_EQ(1u, walkerCmd.getMessageSimd());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerThreadTestXeHPAndLater, WhenInlineDataIsTrueThenExpectInlineDataProgramming) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

    WALKER_TYPE walkerCmd = FamilyType::cmdInitGpgpuWalker;
    startWorkGroup[1] = 2u;
    startWorkGroup[2] = 3u;

    EncodeDispatchKernel<FamilyType>::encodeThreadData(walkerCmd, startWorkGroup, numWorkGroups, workGroupSizes, simd, localIdDimensions,
                                                       0, 0, true, true, false, requiredWorkGroupOrder);
    EXPECT_FALSE(walkerCmd.getIndirectParameterEnable());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdXDimension());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdYDimension());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdZDimension());

    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingX());
    EXPECT_EQ(2u, walkerCmd.getThreadGroupIdStartingY());
    EXPECT_EQ(3u, walkerCmd.getThreadGroupIdStartingZ());

    auto expectedSimd = getSimdConfig<WALKER_TYPE>(simd);
    EXPECT_EQ(expectedSimd, walkerCmd.getSimdSize());
    EXPECT_EQ(expectedSimd, walkerCmd.getMessageSimd());

    EXPECT_EQ(0xffffffffu, walkerCmd.getExecutionMask());

    EXPECT_EQ(0u, walkerCmd.getEmitLocalId());
    EXPECT_EQ(0u, walkerCmd.getLocalXMaximum());
    EXPECT_EQ(0u, walkerCmd.getLocalYMaximum());
    EXPECT_EQ(0u, walkerCmd.getLocalZMaximum());
    EXPECT_EQ(0u, walkerCmd.getWalkOrder());

    EXPECT_FALSE(walkerCmd.getGenerateLocalId());
    EXPECT_TRUE(walkerCmd.getEmitInlineParameter());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerThreadTestXeHPAndLater, WhenExecutionMaskNotZeroThenExpectOverrideExecutionMaskCalculation) {
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
    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingZ());

    auto expectedSimd = getSimdConfig<WALKER_TYPE>(simd);
    EXPECT_EQ(expectedSimd, walkerCmd.getSimdSize());
    EXPECT_EQ(expectedSimd, walkerCmd.getMessageSimd());

    EXPECT_EQ(expectedExecutionMask, walkerCmd.getExecutionMask());

    EXPECT_EQ(0u, walkerCmd.getEmitLocalId());
    EXPECT_EQ(0u, walkerCmd.getLocalXMaximum());
    EXPECT_EQ(0u, walkerCmd.getLocalYMaximum());
    EXPECT_EQ(0u, walkerCmd.getLocalZMaximum());
    EXPECT_EQ(0u, walkerCmd.getWalkOrder());

    EXPECT_FALSE(walkerCmd.getGenerateLocalId());
    EXPECT_FALSE(walkerCmd.getEmitInlineParameter());
}

struct CommandEncodeStatesImplicitScalingFixture : public CommandEncodeStatesFixture {
    void SetUp() {
        DebugManager.flags.CreateMultipleSubDevices.set(2);
        osLocalMemoryBackup = std::make_unique<VariableBackup<bool>>(&OSInterface::osEnableLocalMemory, true);
        mockDeviceBackup = std::make_unique<VariableBackup<bool>>(&MockDevice::createSingleDevice, false);
        apiSupportBackup = std::make_unique<VariableBackup<bool>>(&ImplicitScaling::apiSupport, true);

        CommandEncodeStatesFixture::SetUp();
    }

    void TearDown() {
        CommandEncodeStatesFixture::TearDown();
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<VariableBackup<bool>> osLocalMemoryBackup;
    std::unique_ptr<VariableBackup<bool>> mockDeviceBackup;
    std::unique_ptr<VariableBackup<bool>> apiSupportBackup;
};

using CommandEncodeStatesImplicitScaling = Test<CommandEncodeStatesImplicitScalingFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesImplicitScaling,
            givenStaticPartitioningWhenNonTimestampEventProvidedThenExpectTimestampComputeWalkerPostSync) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;

    uint32_t dims[] = {16, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    bool requiresUncachedMocs = false;
    uint64_t eventAddress = 0xFF112233000;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.eventAddress = eventAddress;
    dispatchArgs.partitionCount = 2;

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);
    size_t usedBuffer = cmdContainer->getCommandStream()->getUsed();
    EXPECT_EQ(2u, dispatchArgs.partitionCount);

    GenCmdList partitionedWalkerList;
    CmdParse<FamilyType>::parseCommandBuffer(
        partitionedWalkerList,
        cmdContainer->getCommandStream()->getCpuBase(),
        usedBuffer);

    auto itor = find<WALKER_TYPE *>(partitionedWalkerList.begin(), partitionedWalkerList.end());
    ASSERT_NE(itor, partitionedWalkerList.end());
    auto partitionWalkerCmd = genCmdCast<WALKER_TYPE *>(*itor);
    auto &postSync = partitionWalkerCmd->getPostSync();
    EXPECT_EQ(POSTSYNC_DATA::OPERATION_WRITE_TIMESTAMP, postSync.getOperation());
    EXPECT_EQ(eventAddress, postSync.getDestinationAddress());
}

struct CommandEncodeStatesDynamicImplicitScalingFixture : CommandEncodeStatesImplicitScalingFixture {
    void SetUp() {
        DebugManager.flags.EnableStaticPartitioning.set(0);
        CommandEncodeStatesImplicitScalingFixture::SetUp();
    }

    void TearDown() {
        CommandEncodeStatesImplicitScalingFixture::TearDown();
    }

    DebugManagerStateRestore restore{};
};

using CommandEncodeStatesDynamicImplicitScaling = Test<CommandEncodeStatesDynamicImplicitScalingFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesDynamicImplicitScaling, givenImplicitScalingWhenEncodingDispatchKernelThenExpectPartitionCommandBuffer) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    uint32_t dims[] = {16, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    bool requiresUncachedMocs = false;
    bool isInternal = false;

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.isInternal = isInternal;
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    size_t containerUsedAfterBase = cmdContainer->getCommandStream()->getUsed();

    GenCmdList baseWalkerList;
    CmdParse<FamilyType>::parseCommandBuffer(baseWalkerList, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), containerUsedAfterBase);
    auto itor = find<WALKER_TYPE *>(baseWalkerList.begin(), baseWalkerList.end());
    ASSERT_NE(itor, baseWalkerList.end());
    auto baseWalkerCmd = genCmdCast<WALKER_TYPE *>(*itor);
    EXPECT_EQ(WALKER_TYPE::PARTITION_TYPE::PARTITION_TYPE_DISABLED, baseWalkerCmd->getPartitionType());
    EXPECT_EQ(16u, baseWalkerCmd->getThreadGroupIdXDimension());

    dispatchArgs.partitionCount = 2;
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    size_t total = cmdContainer->getCommandStream()->getUsed();
    size_t partitionedWalkerSize = total - containerUsedAfterBase;

    size_t expectedPartitionedWalkerSize = ImplicitScalingDispatch<FamilyType>::getSize(true, false, pDevice->getDeviceBitfield(), Vec3<size_t>(0, 0, 0), Vec3<size_t>(16, 1, 1));
    EXPECT_EQ(expectedPartitionedWalkerSize, partitionedWalkerSize);

    GenCmdList partitionedWalkerList;
    CmdParse<FamilyType>::parseCommandBuffer(
        partitionedWalkerList,
        ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), containerUsedAfterBase),
        partitionedWalkerSize);
    auto startCmdList = findAll<BATCH_BUFFER_START *>(partitionedWalkerList.begin(), partitionedWalkerList.end());
    EXPECT_EQ(3u, startCmdList.size());
    bool secondary = true;
    for (auto &ptr : startCmdList) {
        BATCH_BUFFER_START *startCmd = reinterpret_cast<BATCH_BUFFER_START *>(*ptr);
        secondary &= static_cast<bool>(startCmd->getSecondLevelBatchBuffer());
    }
    EXPECT_TRUE(secondary);

    itor = find<WALKER_TYPE *>(partitionedWalkerList.begin(), partitionedWalkerList.end());
    ASSERT_NE(itor, partitionedWalkerList.end());
    auto partitionWalkerCmd = genCmdCast<WALKER_TYPE *>(*itor);
    EXPECT_EQ(WALKER_TYPE::PARTITION_TYPE::PARTITION_TYPE_X, partitionWalkerCmd->getPartitionType());
    uint32_t expectedPartitionSize = (dims[0] + dispatchArgs.partitionCount - 1u) / dispatchArgs.partitionCount;
    EXPECT_EQ(expectedPartitionSize, partitionWalkerCmd->getPartitionSize());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesDynamicImplicitScaling, givenImplicitScalingRequiresPipeControlStallWhenEncodingDispatchKernelThenExpectCrossTileSyncAndSelfCleanupSection) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    VariableBackup<bool> pipeControlConfigBackup(&ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired(), true);

    uint32_t dims[] = {16, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    bool isInternal = false;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.isInternal = isInternal;
    dispatchArgs.partitionCount = 2;

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    EXPECT_EQ(2u, dispatchArgs.partitionCount);
    size_t partitionedWalkerSize = cmdContainer->getCommandStream()->getUsed();

    size_t expectedPartitionedWalkerSize = ImplicitScalingDispatch<FamilyType>::getSize(true, false, pDevice->getDeviceBitfield(), Vec3<size_t>(0, 0, 0), Vec3<size_t>(16, 1, 1));
    EXPECT_EQ(expectedPartitionedWalkerSize, partitionedWalkerSize);

    GenCmdList partitionedWalkerList;
    CmdParse<FamilyType>::parseCommandBuffer(
        partitionedWalkerList,
        cmdContainer->getCommandStream()->getCpuBase(),
        partitionedWalkerSize);
    auto startCmdList = findAll<BATCH_BUFFER_START *>(partitionedWalkerList.begin(), partitionedWalkerList.end());
    EXPECT_EQ(3u, startCmdList.size());
    bool secondary = true;
    for (auto &ptr : startCmdList) {
        BATCH_BUFFER_START *startCmd = reinterpret_cast<BATCH_BUFFER_START *>(*ptr);
        secondary &= static_cast<bool>(startCmd->getSecondLevelBatchBuffer());
    }
    EXPECT_TRUE(secondary);

    auto itor = find<WALKER_TYPE *>(partitionedWalkerList.begin(), partitionedWalkerList.end());
    ASSERT_NE(itor, partitionedWalkerList.end());
    auto partitionWalkerCmd = genCmdCast<WALKER_TYPE *>(*itor);
    EXPECT_EQ(WALKER_TYPE::PARTITION_TYPE::PARTITION_TYPE_X, partitionWalkerCmd->getPartitionType());
    uint32_t expectedPartitionSize = (dims[0] + dispatchArgs.partitionCount - 1u) / dispatchArgs.partitionCount;
    EXPECT_EQ(expectedPartitionSize, partitionWalkerCmd->getPartitionSize());

    WalkerPartition::WalkerPartitionArgs args = {};
    args.initializeWparidRegister = true;
    args.emitPipeControlStall = true;
    args.partitionCount = dispatchArgs.partitionCount;
    args.emitSelfCleanup = true;

    auto cleanupSectionOffset = WalkerPartition::computeControlSectionOffset<FamilyType>(args);
    uint64_t expectedCleanupGpuVa = cmdContainer->getCommandStream()->getGraphicsAllocation()->getGpuAddress() +
                                    cleanupSectionOffset;
    constexpr uint32_t expectedData = 0ull;

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdContainer->getCommandStream());
    GenCmdList storeDataImmList = hwParser.getCommandsList<MI_STORE_DATA_IMM>();
    ASSERT_EQ(4u, storeDataImmList.size());
    GenCmdList::iterator storeCmd = storeDataImmList.begin();

    auto finalSyncTileCountFieldImm = static_cast<MI_STORE_DATA_IMM *>(*storeCmd);
    EXPECT_EQ(expectedCleanupGpuVa + offsetof(WalkerPartition::BatchBufferControlData, finalSyncTileCount),
              finalSyncTileCountFieldImm->getAddress());
    EXPECT_EQ(expectedData, finalSyncTileCountFieldImm->getDataDword0());

    ++storeCmd;
    auto partitionCountFieldImm = static_cast<MI_STORE_DATA_IMM *>(*storeCmd);
    EXPECT_EQ(expectedCleanupGpuVa, partitionCountFieldImm->getAddress());
    EXPECT_EQ(expectedData, partitionCountFieldImm->getDataDword0());

    ++storeCmd;
    expectedCleanupGpuVa += sizeof(WalkerPartition::BatchBufferControlData::tileCount);
    auto tileCountFieldImm = static_cast<MI_STORE_DATA_IMM *>(*storeCmd);
    EXPECT_EQ(expectedCleanupGpuVa, tileCountFieldImm->getAddress());
    EXPECT_EQ(expectedData, tileCountFieldImm->getDataDword0());

    ++storeCmd;
    expectedCleanupGpuVa += sizeof(WalkerPartition::BatchBufferControlData::inTileCount);
    auto inTileCountFieldImm = static_cast<MI_STORE_DATA_IMM *>(*storeCmd);
    EXPECT_EQ(expectedCleanupGpuVa, inTileCountFieldImm->getAddress());
    EXPECT_EQ(expectedData, inTileCountFieldImm->getDataDword0());

    GenCmdList pipeControlList = hwParser.getCommandsList<PIPE_CONTROL>();
    EXPECT_EQ(1u, pipeControlList.size());

    GenCmdList miAtomicList = hwParser.getCommandsList<MI_ATOMIC>();
    EXPECT_EQ(4u, miAtomicList.size());

    GenCmdList miSemaphoreWaitList = hwParser.getCommandsList<MI_SEMAPHORE_WAIT>();
    EXPECT_EQ(3u, miSemaphoreWaitList.size());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesDynamicImplicitScaling,
            givenImplicitScalingRequiresNoPipeControlStallWhenEncodingDispatchKernelThenExpectCrossTileSyncAndSelfCleanupSection) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    VariableBackup<bool> pipeControlConfigBackup(&ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired(), false);

    uint32_t dims[] = {16, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    bool isInternal = false;
    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.isInternal = isInternal;
    dispatchArgs.partitionCount = 2;

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    EXPECT_EQ(2u, dispatchArgs.partitionCount);
    size_t partitionedWalkerSize = cmdContainer->getCommandStream()->getUsed();

    size_t expectedPartitionedWalkerSize = ImplicitScalingDispatch<FamilyType>::getSize(true, false, pDevice->getDeviceBitfield(), Vec3<size_t>(0, 0, 0), Vec3<size_t>(16, 1, 1));
    EXPECT_EQ(expectedPartitionedWalkerSize, partitionedWalkerSize);

    GenCmdList partitionedWalkerList;
    CmdParse<FamilyType>::parseCommandBuffer(
        partitionedWalkerList,
        cmdContainer->getCommandStream()->getCpuBase(),
        partitionedWalkerSize);
    auto startCmdList = findAll<BATCH_BUFFER_START *>(partitionedWalkerList.begin(), partitionedWalkerList.end());
    EXPECT_EQ(3u, startCmdList.size());
    bool secondary = true;
    for (auto &ptr : startCmdList) {
        BATCH_BUFFER_START *startCmd = reinterpret_cast<BATCH_BUFFER_START *>(*ptr);
        secondary &= static_cast<bool>(startCmd->getSecondLevelBatchBuffer());
    }
    EXPECT_TRUE(secondary);

    auto itor = find<WALKER_TYPE *>(partitionedWalkerList.begin(), partitionedWalkerList.end());
    ASSERT_NE(itor, partitionedWalkerList.end());
    auto partitionWalkerCmd = genCmdCast<WALKER_TYPE *>(*itor);
    EXPECT_EQ(WALKER_TYPE::PARTITION_TYPE::PARTITION_TYPE_X, partitionWalkerCmd->getPartitionType());
    uint32_t expectedPartitionSize = (dims[0] + dispatchArgs.partitionCount - 1u) / dispatchArgs.partitionCount;
    EXPECT_EQ(expectedPartitionSize, partitionWalkerCmd->getPartitionSize());

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdContainer->getCommandStream());
    GenCmdList storeDataImmList = hwParser.getCommandsList<MI_STORE_DATA_IMM>();
    EXPECT_EQ(4u, storeDataImmList.size());

    GenCmdList pipeControlList = hwParser.getCommandsList<PIPE_CONTROL>();
    EXPECT_EQ(0u, pipeControlList.size());

    GenCmdList miAtomicList = hwParser.getCommandsList<MI_ATOMIC>();
    EXPECT_EQ(4u, miAtomicList.size());

    GenCmdList miSemaphoreWaitList = hwParser.getCommandsList<MI_SEMAPHORE_WAIT>();
    EXPECT_EQ(3u, miSemaphoreWaitList.size());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesDynamicImplicitScaling, givenImplicitScalingWhenEncodingDispatchKernelOnInternalEngineThenExpectNoWalkerPartitioning) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    uint32_t dims[] = {16, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    bool isInternal = true;
    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.isInternal = isInternal;
    dispatchArgs.partitionCount = 2;

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    size_t internalWalkerSize = cmdContainer->getCommandStream()->getUsed();

    GenCmdList internalWalkerList;
    CmdParse<FamilyType>::parseCommandBuffer(internalWalkerList,
                                             cmdContainer->getCommandStream()->getCpuBase(),
                                             internalWalkerSize);
    auto itor = find<WALKER_TYPE *>(internalWalkerList.begin(), internalWalkerList.end());
    ASSERT_NE(itor, internalWalkerList.end());
    auto internalWalkerCmd = genCmdCast<WALKER_TYPE *>(*itor);
    EXPECT_EQ(WALKER_TYPE::PARTITION_TYPE::PARTITION_TYPE_DISABLED, internalWalkerCmd->getPartitionType());
    EXPECT_EQ(16u, internalWalkerCmd->getThreadGroupIdXDimension());
}
