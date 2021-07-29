/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_container/walker_partition_xehp_plus.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/command_container_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_dispatch_kernel_encoder_interface.h"

#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/test/unit_test/mocks/mock_event.h"
#include "test.h"

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
    EXPECT_CALL(*dispatchInterface.get(), getSlmTotalSize()).WillRepeatedly(::testing::Return(slmTotalSize));

    bool requiresUncachedMocs = false;
    uint32_t partitionCount = 0;
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, false, false,
                                             pDevice, NEO::PreemptionMode::Disabled, requiresUncachedMocs, false, partitionCount, false);

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
    uint32_t partitionCount = 0;
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, false, false,
                                             pDevice, NEO::PreemptionMode::Disabled, requiresUncachedMocs, false, partitionCount, false);

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
    EXPECT_CALL(*dispatchInterface.get(), getSlmTotalSize()).WillRepeatedly(::testing::Return(slmTotalSize));

    bool requiresUncachedMocs = false;
    uint32_t partitionCount = 0;
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, false, false,
                                             pDevice, NEO::PreemptionMode::Disabled, requiresUncachedMocs, false, partitionCount, false);

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
    EXPECT_CALL(*dispatchInterface.get(), getSlmTotalSize()).WillRepeatedly(::testing::Return(slmTotalSize));

    bool requiresUncachedMocs = false;
    int32_t maxValueToProgram = 0x8;

    for (int32_t valueToProgram = 0x0; valueToProgram < maxValueToProgram; valueToProgram++) {
        DebugManager.flags.OverrideSlmAllocationSize.set(valueToProgram);
        cmdContainer->reset();

        uint32_t partitionCount = 0;
        EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, false, false,
                                                 pDevice, NEO::PreemptionMode::Disabled, requiresUncachedMocs, false, partitionCount, false);

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
    EXPECT_CALL(*dispatchInterface.get(), getSurfaceStateHeapData()).WillRepeatedly(::testing::Return(bindingTableStateRaw));
    EXPECT_CALL(*dispatchInterface.get(), getSurfaceStateHeapDataSize()).WillRepeatedly(::testing::Return(static_cast<uint32_t>(sizeof(BINDING_TABLE_STATE))));

    bool requiresUncachedMocs = false;
    uint32_t partitionCount = 0;
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, false, false,
                                             pDevice, NEO::PreemptionMode::Disabled, requiresUncachedMocs, false, partitionCount, false);

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
    EXPECT_CALL(*dispatchInterface.get(), getSurfaceStateHeapData()).WillRepeatedly(::testing::Return(bindingTableStateRaw));
    EXPECT_CALL(*dispatchInterface.get(), getSurfaceStateHeapDataSize()).WillRepeatedly(::testing::Return(static_cast<uint32_t>(sizeof(BINDING_TABLE_STATE))));

    bool requiresUncachedMocs = false;
    uint32_t partitionCount = 0;
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, false, false,
                                             pDevice, NEO::PreemptionMode::Disabled, requiresUncachedMocs, false, partitionCount, false);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmd = genCmdCast<WALKER_TYPE *>(*itor);
    auto &idd = cmd->getInterfaceDescriptor();

    EXPECT_EQ(idd.getBindingTablePointer(), 0u);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, giveNumSamplersOneWhenDispatchKernelThensamplerStateWasCopied) {
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
    EXPECT_CALL(*dispatchInterface.get(), getDynamicStateHeapData()).WillRepeatedly(::testing::Return(samplerStateRaw));

    bool requiresUncachedMocs = false;
    uint32_t partitionCount = 0;
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, false, false,
                                             pDevice, NEO::PreemptionMode::Disabled, requiresUncachedMocs, false, partitionCount, false);

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
    uint32_t partitionCount = 0;
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), eventAddress, true, true,
                                             pDevice, NEO::PreemptionMode::Disabled, requiresUncachedMocs, false, partitionCount, false);

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
    uint32_t partitionCount = 0;
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), eventAddress, true, true,
                                             pDevice, NEO::PreemptionMode::Disabled, requiresUncachedMocs, false, partitionCount, false);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());
    auto cmd = genCmdCast<WALKER_TYPE *>(*itor);
    EXPECT_EQ(pDevice->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED), cmd->getPostSync().getMocs());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenCleanHeapsWhenDispatchKernelThenFlushNotAdded) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    cmdContainer->slmSize = 1;
    EXPECT_CALL(*dispatchInterface.get(), getSlmTotalSize()).WillRepeatedly(::testing::Return(cmdContainer->slmSize));
    cmdContainer->setDirtyStateForAllHeaps(false);

    bool requiresUncachedMocs = false;
    uint32_t partitionCount = 0;
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, false, false,
                                             pDevice, NEO::PreemptionMode::Disabled, requiresUncachedMocs, false, partitionCount, false);

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
    EXPECT_CALL(*dispatchInterface.get(), getDynamicStateHeapData()).WillRepeatedly(::testing::Return(samplerStateRaw));
    unsigned char *bindingTableRaw = reinterpret_cast<unsigned char *>(&bindingTable);
    EXPECT_CALL(*dispatchInterface.get(), getSurfaceStateHeapData()).WillRepeatedly(::testing::Return(bindingTableRaw));

    {
        DebugManager.flags.ForceBtpPrefetchMode.set(-1);
        cmdContainer.reset(new MyMockCommandContainer());
        cmdContainer->initialize(pDevice);

        bool requiresUncachedMocs = false;
        uint32_t partitionCount = 0;
        EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, false, false,
                                                 pDevice, NEO::PreemptionMode::Disabled, requiresUncachedMocs, false, partitionCount, false);

        GenCmdList commands;
        CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());
        auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
        ASSERT_NE(itor, commands.end());
        auto cmd = genCmdCast<WALKER_TYPE *>(*itor);
        auto &idd = cmd->getInterfaceDescriptor();

        if (EncodeSurfaceState<FamilyType>::doBindingTablePrefetch()) {
            EXPECT_NE(0u, idd.getBindingTableEntryCount());
            EXPECT_NE(INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT_NO_SAMPLERS_USED, idd.getSamplerCount());
        } else {
            EXPECT_EQ(0u, idd.getBindingTableEntryCount());
            EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT_NO_SAMPLERS_USED, idd.getSamplerCount());
        }
    }

    {
        DebugManager.flags.ForceBtpPrefetchMode.set(0);
        cmdContainer.reset(new MyMockCommandContainer());
        cmdContainer->initialize(pDevice);

        bool requiresUncachedMocs = false;
        uint32_t partitionCount = 0;
        EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, false, false,
                                                 pDevice, NEO::PreemptionMode::Disabled, requiresUncachedMocs, false, partitionCount, false);

        GenCmdList commands;
        CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());
        auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
        ASSERT_NE(itor, commands.end());
        auto cmd = genCmdCast<WALKER_TYPE *>(*itor);
        auto &idd = cmd->getInterfaceDescriptor();

        EXPECT_EQ(0u, idd.getBindingTableEntryCount());
        EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT_NO_SAMPLERS_USED, idd.getSamplerCount());
    }

    {
        DebugManager.flags.ForceBtpPrefetchMode.set(1);
        cmdContainer.reset(new MyMockCommandContainer());
        cmdContainer->initialize(pDevice);

        bool requiresUncachedMocs = false;
        uint32_t partitionCount = 0;
        EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, false, false,
                                                 pDevice, NEO::PreemptionMode::Disabled, requiresUncachedMocs, false, partitionCount, false);

        GenCmdList commands;
        CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());
        auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
        ASSERT_NE(itor, commands.end());
        auto cmd = genCmdCast<WALKER_TYPE *>(*itor);
        auto &idd = cmd->getInterfaceDescriptor();

        EXPECT_NE(0u, idd.getBindingTableEntryCount());
        EXPECT_NE(INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT_NO_SAMPLERS_USED, idd.getSamplerCount());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenDispatchInterfaceWhenNumRequiredGrfIsNotDefaultThenStateComputeModeCommandAdded) {
    StreamProperties streamProperties{};
    streamProperties.stateComputeMode.setProperties(false, 128, 0u);
    streamProperties.stateComputeMode.setProperties(false, 128, 0u);
    EXPECT_FALSE(streamProperties.stateComputeMode.isDirty());

    streamProperties.stateComputeMode.setProperties(false, 256, 0u);
    EXPECT_TRUE(streamProperties.stateComputeMode.isDirty());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenInlineDataRequiredWhenEncodingWalkerThenEmitInlineParameterIsSet) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using InlineData = typename FamilyType::INLINE_DATA;
    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    dispatchInterface->kernelDescriptor.kernelAttributes.flags.passInlineData = true;

    bool requiresUncachedMocs = false;
    uint32_t partitionCount = 0;
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, false, false,
                                             pDevice, NEO::PreemptionMode::Disabled, requiresUncachedMocs, false, partitionCount, false);
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
    uint32_t partitionCount = 0;
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, false, false,
                                             pDevice, NEO::PreemptionMode::Disabled, requiresUncachedMocs, false, partitionCount, false);

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
    EXPECT_CALL(*dispatchInterface.get(), getCrossThreadDataSize()).WillRepeatedly(::testing::Return(0));

    bool requiresUncachedMocs = false;
    uint32_t partitionCount = 0;
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, false, false,
                                             pDevice, NEO::PreemptionMode::Disabled, requiresUncachedMocs, false, partitionCount, false);

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
    dispatchInterface->localIdGenerationByRuntime = false;
    dispatchInterface->requiredWalkGroupOrder = 2u;
    dispatchInterface->kernelDescriptor.kernelAttributes.flags.passInlineData = true;
    dispatchInterface->kernelDescriptor.kernelAttributes.numLocalIdChannels = 3u;
    dispatchInterface->kernelDescriptor.kernelAttributes.simdSize = 32u;

    EXPECT_CALL(*dispatchInterface.get(), getCrossThreadDataSize()).WillRepeatedly(::testing::Return(32u));
    EXPECT_CALL(*dispatchInterface.get(), requiresGenerationOfLocalIdsByRuntime()).WillRepeatedly(::testing::Return(false));

    bool requiresUncachedMocs = false;
    uint32_t partitionCount = 0;
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, false, false,
                                             pDevice, NEO::PreemptionMode::Disabled, requiresUncachedMocs, false, partitionCount, false);

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
    auto &hwHelper = HwHelper::get(renderCoreFamily);

    DebugManagerStateRestore restorer;
    DebugManager.flags.ForceThreadGroupDispatchSize.set(forceThreadGroupDispatchSize);

    uint32_t revisions[] = {REVISION_A0, REVISION_B};
    for (auto revision : revisions) {
        hwInfo.platform.usRevId = hwHelper.getHwRevIdFromStepping(revision, hwInfo);
        EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, hwInfo);

        if (hwHelper.isDisableOverdispatchAvailable(hwInfo)) {
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

using WalkerThreadTestXeHPPlus = WalkerThreadTest;

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerThreadTestXeHPPlus, givenStartWorkGroupWhenIndirectIsFalseThenExpectStartGroupAndThreadDimensionsProgramming) {
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

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerThreadTestXeHPPlus, givenNoStartWorkGroupWhenIndirectIsTrueThenExpectNoStartGroupAndThreadDimensionsProgramming) {
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

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerThreadTestXeHPPlus, givenSimdSizeOneWhenWorkGroupSmallerThanSimdThenExpectSimdSizeAsMaxAndExecutionMaskFull) {
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

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerThreadTestXeHPPlus, givenStartWorkGroupWhenWorkGroupSmallerThanSimdThenExpectStartGroupAndExecutionMaskNotFull) {
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

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerThreadTestXeHPPlus, givenLocalIdGenerationByHwWhenNoLocalIdsPresentThenExpectNoEmitAndGenerateLocalIds) {
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

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerThreadTestXeHPPlus, givenLocalIdGenerationByHwWhenLocalIdsPresentThenExpectEmitAndGenerateLocalIds) {
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

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerThreadTestXeHPPlus, WhenInlineDataIsTrueThenExpectInlineDataProgramming) {
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

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerThreadTestXeHPPlus, WhenExecutionMaskNotZeroThenExpectOverrideExecutionMaskCalculation) {
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

        CommandEncodeStatesFixture::SetUp();
    }

    void TearDown() {
        CommandEncodeStatesFixture::TearDown();
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<VariableBackup<bool>> osLocalMemoryBackup;
    std::unique_ptr<VariableBackup<bool>> mockDeviceBackup;
};

struct CommandEncodeStatesDynamicImplicitScaling : ::testing::Test, CommandEncodeStatesImplicitScalingFixture {
    void SetUp() override {
        DebugManager.flags.EnableStaticPartitioning.set(0);
        CommandEncodeStatesImplicitScalingFixture::SetUp();
    }

    void TearDown() override {
        CommandEncodeStatesImplicitScalingFixture::TearDown();
    }

    DebugManagerStateRestore restore{};
};

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesDynamicImplicitScaling, givenImplicitScalingWhenEncodingDispatchKernelThenExpectPartitionCommandBuffer) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    uint32_t dims[] = {16, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    DebugManager.flags.EnableWalkerPartition.set(0);

    bool requiresUncachedMocs = false;
    bool isInternal = false;
    size_t regularEstimateSize = EncodeDispatchKernel<FamilyType>::estimateEncodeDispatchKernelCmdsSize(pDevice, Vec3<size_t>(0, 0, 0), Vec3<size_t>(16, 1, 1), isInternal);
    uint32_t partitionCount = 0;
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, false, false, pDevice,
                                             NEO::PreemptionMode::Disabled, requiresUncachedMocs, false, partitionCount, isInternal);

    size_t containerUsedAfterBase = cmdContainer->getCommandStream()->getUsed();

    GenCmdList baseWalkerList;
    CmdParse<FamilyType>::parseCommandBuffer(baseWalkerList, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), containerUsedAfterBase);
    auto itor = find<WALKER_TYPE *>(baseWalkerList.begin(), baseWalkerList.end());
    ASSERT_NE(itor, baseWalkerList.end());
    auto baseWalkerCmd = genCmdCast<WALKER_TYPE *>(*itor);
    EXPECT_EQ(WALKER_TYPE::PARTITION_TYPE::PARTITION_TYPE_DISABLED, baseWalkerCmd->getPartitionType());
    EXPECT_EQ(16u, baseWalkerCmd->getThreadGroupIdXDimension());

    DebugManager.flags.EnableWalkerPartition.set(-1);

    size_t partitionEstimateSize = EncodeDispatchKernel<FamilyType>::estimateEncodeDispatchKernelCmdsSize(pDevice, Vec3<size_t>(0, 0, 0), Vec3<size_t>(16, 1, 1), isInternal);
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, false, false, pDevice,
                                             NEO::PreemptionMode::Disabled, requiresUncachedMocs, false, partitionCount, isInternal);

    size_t total = cmdContainer->getCommandStream()->getUsed();
    size_t partitionedWalkerSize = total - containerUsedAfterBase;

    size_t expectedPartitionedWalkerSize = ImplicitScalingDispatch<FamilyType>::getSize(true, false, pDevice->getDeviceBitfield(), Vec3<size_t>(0, 0, 0), Vec3<size_t>(16, 1, 1));
    EXPECT_EQ(expectedPartitionedWalkerSize, partitionedWalkerSize);
    EXPECT_EQ(partitionEstimateSize, regularEstimateSize + expectedPartitionedWalkerSize);

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
    uint32_t expectedPartitionSize = (dims[0] + partitionCount - 1u) / partitionCount;
    EXPECT_EQ(expectedPartitionSize, partitionWalkerCmd->getPartitionSize());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesDynamicImplicitScaling, givenImplicitScalingWhenEncodingDispatchKernelThenExpectNativeCrossTileCleanupSection) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    uint32_t dims[] = {16, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    DebugManager.flags.EnableWalkerPartition.set(0);
    bool isInternal = false;
    size_t baseEstimateSize = EncodeDispatchKernel<FamilyType>::estimateEncodeDispatchKernelCmdsSize(pDevice, Vec3<size_t>(0, 0, 0), Vec3<size_t>(16, 1, 1), isInternal);

    DebugManager.flags.EnableWalkerPartition.set(1);

    uint32_t partitionCount = 0;
    bool requiresUncachedMocs = false;

    size_t partitionEstimateSize = EncodeDispatchKernel<FamilyType>::estimateEncodeDispatchKernelCmdsSize(pDevice, Vec3<size_t>(0, 0, 0), Vec3<size_t>(16, 1, 1), isInternal);
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, false, false, pDevice,
                                             NEO::PreemptionMode::Disabled, requiresUncachedMocs, false, partitionCount, isInternal);

    EXPECT_EQ(2u, partitionCount);
    size_t partitionedWalkerSize = cmdContainer->getCommandStream()->getUsed();

    size_t expectedPartitionedWalkerSize = ImplicitScalingDispatch<FamilyType>::getSize(true, false, pDevice->getDeviceBitfield(), Vec3<size_t>(0, 0, 0), Vec3<size_t>(16, 1, 1));
    EXPECT_EQ(partitionEstimateSize, baseEstimateSize + expectedPartitionedWalkerSize);
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
    uint32_t expectedPartitionSize = (dims[0] + partitionCount - 1u) / partitionCount;
    EXPECT_EQ(expectedPartitionSize, partitionWalkerCmd->getPartitionSize());

    auto cleanupSectionOffset = WalkerPartition::computeControlSectionOffset<FamilyType>(partitionCount, false, true, false);
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
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesDynamicImplicitScaling, givenImplicitScalingWhenEncodingDispatchKernelOnInternalEngineThenExpectNoWalkerPartitioning) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    uint32_t dims[] = {16, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    DebugManager.flags.EnableWalkerPartition.set(0);
    bool isInternal = false;
    size_t baseEstimateSize = EncodeDispatchKernel<FamilyType>::estimateEncodeDispatchKernelCmdsSize(pDevice, Vec3<size_t>(0, 0, 0), Vec3<size_t>(16, 1, 1), isInternal);

    DebugManager.flags.EnableWalkerPartition.set(1);
    isInternal = true;
    uint32_t partitionCount = 0;
    bool requiresUncachedMocs = false;
    size_t internalEstimateSize = EncodeDispatchKernel<FamilyType>::estimateEncodeDispatchKernelCmdsSize(pDevice, Vec3<size_t>(0, 0, 0), Vec3<size_t>(16, 1, 1), isInternal);
    EXPECT_EQ(baseEstimateSize, internalEstimateSize);

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, false, false, pDevice,
                                             NEO::PreemptionMode::Disabled, requiresUncachedMocs, false, partitionCount, isInternal);
    EXPECT_EQ(1u, partitionCount);
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
