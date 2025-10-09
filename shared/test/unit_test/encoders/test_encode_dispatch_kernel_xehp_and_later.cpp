/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_container/walker_partition_xehp_and_later.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_l0_debugger.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"
#include "shared/test/unit_test/mocks/mock_dispatch_kernel_encoder_interface.h"

#include "test_traits_common.h"

#include <memory>

using namespace NEO;
#include "shared/test/common/test_macros/header/heapless_matchers.h"

using CommandEncodeStatesTest = Test<CommandEncodeStatesFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenSlmTotalSizeGraterThanZeroWhenDispatchingKernelThenSharedMemorySizeIsSetCorrectly) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::DefaultWalkerType::InterfaceDescriptorType;
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint32_t slmTotalSize = 1;

    dispatchInterface->getSlmTotalSizeResult = slmTotalSize;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmd = genCmdCast<DefaultWalkerType *>(*itor);
    auto &idd = cmd->getInterfaceDescriptor();
    auto &gfxcoreHelper = this->getHelper<GfxCoreHelper>();
    auto releaseHelper = ReleaseHelper::create(pDevice->getHardwareInfo().ipVersion);
    bool isHeapless = pDevice->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo);

    uint32_t expectedValue = static_cast<typename INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE>(
        gfxcoreHelper.computeSlmValues(pDevice->getHardwareInfo(), slmTotalSize, releaseHelper.get(), isHeapless));

    EXPECT_EQ(expectedValue, idd.getSharedLocalMemorySize());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenXeHpAndLaterWhenDispatchingKernelThenSetDenormMode) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA = typename DefaultWalkerType::InterfaceDescriptorType;

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, false);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmd = genCmdCast<DefaultWalkerType *>(*itor);
    auto &idd = cmd->getInterfaceDescriptor();

    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::DENORM_MODE_FTZ, idd.getDenormMode());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenXeHpDebuggingEnabledAndAssertInKernelWhenDispatchingKernelThenSwExceptionsAreEnabled) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    auto debugger = new MockDebuggerL0(pDevice);
    pDevice->getRootDeviceEnvironmentRef().debugger.reset(debugger);

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->kernelDescriptor.kernelAttributes.flags.usesAssert = true;

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, false);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmd = genCmdCast<DefaultWalkerType *>(*itor);
    auto &idd = cmd->getInterfaceDescriptor();

    EXPECT_TRUE(idd.getSoftwareExceptionEnable());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenSimdSizeWhenDispatchingKernelThenSimdMessageIsSet) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint32_t simdSize = 32;
    dispatchInterface->kernelDescriptor.kernelAttributes.simdSize = simdSize;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmd = genCmdCast<DefaultWalkerType *>(*itor);
    EXPECT_EQ(getSimdConfig<DefaultWalkerType>(simdSize), cmd->getMessageSimd());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenSlmTotalSizeEqualZeroWhenDispatchingKernelThenSharedMemorySizeIsSetCorrectly) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::DefaultWalkerType::InterfaceDescriptorType;
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint32_t slmTotalSize = 0;

    dispatchInterface->getSlmTotalSizeResult = slmTotalSize;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmd = genCmdCast<DefaultWalkerType *>(*itor);
    auto &idd = cmd->getInterfaceDescriptor();

    uint32_t expectedValue = INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_0K;

    EXPECT_EQ(expectedValue, idd.getSharedLocalMemorySize());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenOverrideSlmTotalSizeDebugVariableWhenDispatchingKernelThenSharedMemorySizeIsSetCorrectly) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DebugManagerStateRestore restorer;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint32_t slmTotalSize = 0;

    dispatchInterface->getSlmTotalSizeResult = slmTotalSize;

    bool requiresUncachedMocs = false;
    uint32_t maxValueToProgram = 0x8;

    for (uint32_t valueToProgram = 0x0; valueToProgram < maxValueToProgram; valueToProgram++) {
        debugManager.flags.OverrideSlmAllocationSize.set(valueToProgram);
        cmdContainer->reset();

        EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
        EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

        GenCmdList commands;
        CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());
        auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
        ASSERT_NE(itor, commands.end());
        auto cmd = genCmdCast<DefaultWalkerType *>(*itor);
        auto &idd = cmd->getInterfaceDescriptor();

        EXPECT_EQ(valueToProgram, idd.getSharedLocalMemorySize());
    }
}

struct CommandEncodeStatesTestBindingTableStateMatcher {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        if constexpr (HwMapper<productFamily>::GfxProduct::supportsCmdSet(IGFX_XE_HP_CORE)) {
            return TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::bindingTableStateSupported && !TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::heaplessRequired;
        }
        return false;
    }
};

HWTEST2_F(CommandEncodeStatesTest, givenStatelessBufferAndImageWhenDispatchingKernelThenBindingTableOffsetIsCorrect, CommandEncodeStatesTestBindingTableStateMatcher) {

    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    uint32_t numBindingTable = 1;
    BINDING_TABLE_STATE bindingTableState = FamilyType::cmdInitBindingTableState;

    auto ssh = cmdContainer->getIndirectHeap(HeapType::surfaceState);
    ssh->getSpace(0x20);
    uint32_t sizeUsed = static_cast<uint32_t>(ssh->getUsed());
    auto expectedOffset = alignUp(sizeUsed, FamilyType::cacheLineSize);

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.numEntries = numBindingTable;
    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.tableOffset = 0;
    dispatchInterface->kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::Stateless;
    dispatchInterface->kernelDescriptor.kernelAttributes.flags.usesImages = true;

    unsigned char *bindingTableStateRaw = reinterpret_cast<unsigned char *>(&bindingTableState);
    dispatchInterface->getSurfaceStateHeapDataResult = bindingTableStateRaw;
    dispatchInterface->getSurfaceStateHeapDataSizeResult = static_cast<uint32_t>(sizeof(BINDING_TABLE_STATE));

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    EncodeDispatchKernel<FamilyType>::template encode<COMPUTE_WALKER>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<COMPUTE_WALKER *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmd = genCmdCast<COMPUTE_WALKER *>(*itor);
    auto &idd = cmd->getInterfaceDescriptor();

    EXPECT_EQ(idd.getBindingTablePointer(), expectedOffset);
}

HWTEST2_F(CommandEncodeStatesTest, givennumBindingTableOneWhenDispatchingKernelThenBTOffsetIsCorrect, CommandEncodeStatesTestBindingTableStateMatcher) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    uint32_t numBindingTable = 1;
    BINDING_TABLE_STATE bindingTableState = FamilyType::cmdInitBindingTableState;

    auto ssh = cmdContainer->getIndirectHeap(HeapType::surfaceState);
    ssh->getSpace(0x20);
    uint32_t sizeUsed = static_cast<uint32_t>(ssh->getUsed());
    auto expectedOffset = alignUp(sizeUsed, FamilyType::cacheLineSize);

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.numEntries = numBindingTable;
    dispatchInterface->kernelDescriptor.payloadMappings.bindingTable.tableOffset = 0;

    unsigned char *bindingTableStateRaw = reinterpret_cast<unsigned char *>(&bindingTableState);
    dispatchInterface->getSurfaceStateHeapDataResult = bindingTableStateRaw;
    dispatchInterface->getSurfaceStateHeapDataSizeResult = static_cast<uint32_t>(sizeof(BINDING_TABLE_STATE));

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    EncodeDispatchKernel<FamilyType>::template encode<COMPUTE_WALKER>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<COMPUTE_WALKER *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmd = genCmdCast<COMPUTE_WALKER *>(*itor);
    auto &idd = cmd->getInterfaceDescriptor();

    EXPECT_EQ(idd.getBindingTablePointer(), expectedOffset);
}

HWTEST2_F(CommandEncodeStatesTest, giveNumBindingTableZeroWhenDispatchingKernelThenBTOffsetIsZero, CommandEncodeStatesTestBindingTableStateMatcher) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    uint32_t numBindingTable = 0;
    BINDING_TABLE_STATE bindingTableState = FamilyType::cmdInitBindingTableState;

    auto ssh = cmdContainer->getIndirectHeap(HeapType::surfaceState);
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
    EncodeDispatchKernel<FamilyType>::template encode<COMPUTE_WALKER>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<COMPUTE_WALKER *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmd = genCmdCast<COMPUTE_WALKER *>(*itor);
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
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using SAMPLER_BORDER_COLOR_STATE = typename FamilyType::SAMPLER_BORDER_COLOR_STATE;

    if (!pDevice->getDeviceInfo().imageSupport) {
        GTEST_SKIP();
    }

    uint32_t numSamplers = 1;
    SAMPLER_STATE samplerState;
    memset(&samplerState, 2, sizeof(SAMPLER_STATE));

    auto dsh = cmdContainer->getIndirectHeap(HeapType::dynamicState);
    auto usedBefore = dsh->getUsed();

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    constexpr auto samplerTableBorderColorOffset = 0u;
    dispatchInterface->kernelDescriptor.payloadMappings.samplerTable.numSamplers = numSamplers;
    dispatchInterface->kernelDescriptor.payloadMappings.samplerTable.borderColor = samplerTableBorderColorOffset;
    dispatchInterface->kernelDescriptor.payloadMappings.samplerTable.tableOffset = 0;

    unsigned char *samplerStateRaw = reinterpret_cast<unsigned char *>(&samplerState);
    dispatchInterface->getDynamicStateHeapDataResult = samplerStateRaw;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.surfaceStateHeap = cmdContainer->getIndirectHeap(HeapType::surfaceState);
    dispatchArgs.dynamicStateHeap = cmdContainer->getIndirectHeap(HeapType::dynamicState);

    EncodeDispatchKernel<FamilyType>::template encode<COMPUTE_WALKER>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<COMPUTE_WALKER *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmd = genCmdCast<COMPUTE_WALKER *>(*itor);
    auto &idd = cmd->getInterfaceDescriptor();

    if (pDevice->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo)) {
        auto borderColor = reinterpret_cast<const SAMPLER_BORDER_COLOR_STATE *>(ptrOffset(dispatchInterface->getDynamicStateHeapData(), samplerTableBorderColorOffset));
        EncodeStates<FamilyType>::adjustSamplerStateBorderColor(samplerState, *borderColor);
    } else {
        auto borderColorOffsetInDsh = usedBefore;
        samplerState.setIndirectStatePointer(static_cast<uint32_t>(borderColorOffsetInDsh));
    }

    auto samplerStateOffset = idd.getSamplerStatePointer();

    auto pSmplr = reinterpret_cast<SAMPLER_STATE *>(ptrOffset(dsh->getCpuBase(), samplerStateOffset));
    EXPECT_EQ(memcmp(pSmplr, &samplerState, sizeof(SAMPLER_STATE)), 0);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenEventAllocationWhenDispatchingKernelThenPostSyncIsAdded) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using POSTSYNC_DATA = decltype(FamilyType::template getPostSyncType<DefaultWalkerType>());
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint64_t eventAddress = MemoryConstants::cacheLineSize * 123;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.postSyncArgs.eventAddress = eventAddress;
    dispatchArgs.postSyncArgs.isTimestampEvent = true;

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());
    auto cmd = genCmdCast<DefaultWalkerType *>(*itor);
    EXPECT_EQ(POSTSYNC_DATA::OPERATION_WRITE_TIMESTAMP, cmd->getPostSync().getOperation());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenEventAddressWhenEncodeThenMocsFromGmmHelperIsSet) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint64_t eventAddress = MemoryConstants::cacheLineSize * 123;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.postSyncArgs.eventAddress = eventAddress;
    dispatchArgs.postSyncArgs.isTimestampEvent = true;
    dispatchArgs.postSyncArgs.dcFlushEnable = MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, pDevice->getRootDeviceEnvironment());

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());
    auto cmd = genCmdCast<DefaultWalkerType *>(*itor);
    if (MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, pDevice->getRootDeviceEnvironment())) {
        EXPECT_EQ(pDevice->getGmmHelper()->getUncachedMOCS(), cmd->getPostSync().getMocs());
    } else {
        EXPECT_EQ(pDevice->getGmmHelper()->getL3EnabledMOCS(), cmd->getPostSync().getMocs());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenCleanHeapsWhenDispatchKernelThenFlushNotAdded) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
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

HWTEST2_F(CommandEncodeStatesTest, givenForceBtpPrefetchModeDebugFlagWhenDispatchingKernelThenCorrectValuesAreSetUp, CommandEncodeStatesTestBindingTableStateMatcher) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
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
    dispatchInterface->getDynamicStateHeapDataResult = samplerStateRaw;
    unsigned char *bindingTableRaw = reinterpret_cast<unsigned char *>(&bindingTable);
    dispatchInterface->getSurfaceStateHeapDataResult = bindingTableRaw;

    {
        debugManager.flags.ForceBtpPrefetchMode.set(-1);
        cmdContainer.reset(new MyMockCommandContainer());
        cmdContainer->initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);
        cmdContainer->l1CachePolicyDataRef() = &l1CachePolicyData;

        bool requiresUncachedMocs = false;
        EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
        EncodeDispatchKernel<FamilyType>::template encode<COMPUTE_WALKER>(*cmdContainer.get(), dispatchArgs);

        GenCmdList commands;
        CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());
        auto itor = find<COMPUTE_WALKER *>(commands.begin(), commands.end());
        ASSERT_NE(itor, commands.end());
        auto cmd = genCmdCast<COMPUTE_WALKER *>(*itor);
        auto &idd = cmd->getInterfaceDescriptor();

        if (EncodeSurfaceState<FamilyType>::doBindingTablePrefetch()) {
            EXPECT_NE(0u, idd.getBindingTableEntryCount());
        } else {
            EXPECT_EQ(0u, idd.getBindingTableEntryCount());
        }

        if constexpr (FamilyType::supportsSampler) {
            if (pDevice->getDeviceInfo().imageSupport) {
                if (EncodeSurfaceState<FamilyType>::doBindingTablePrefetch()) {
                    EXPECT_NE(INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT_NO_SAMPLERS_USED, idd.getSamplerCount());
                } else {
                    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT_NO_SAMPLERS_USED, idd.getSamplerCount());
                }
            }
        }
    }

    {
        debugManager.flags.ForceBtpPrefetchMode.set(0);
        cmdContainer.reset(new MyMockCommandContainer());
        cmdContainer->initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);
        cmdContainer->l1CachePolicyDataRef() = &l1CachePolicyData;

        bool requiresUncachedMocs = false;
        EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
        EncodeDispatchKernel<FamilyType>::template encode<COMPUTE_WALKER>(*cmdContainer.get(), dispatchArgs);

        GenCmdList commands;
        CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());
        auto itor = find<COMPUTE_WALKER *>(commands.begin(), commands.end());
        ASSERT_NE(itor, commands.end());
        auto cmd = genCmdCast<COMPUTE_WALKER *>(*itor);
        auto &idd = cmd->getInterfaceDescriptor();

        EXPECT_EQ(0u, idd.getBindingTableEntryCount());
        if constexpr (FamilyType::supportsSampler) {
            if (pDevice->getDeviceInfo().imageSupport) {
                EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT_NO_SAMPLERS_USED, idd.getSamplerCount());
            }
        }
    }

    {
        debugManager.flags.ForceBtpPrefetchMode.set(1);
        cmdContainer.reset(new MyMockCommandContainer());
        cmdContainer->initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);
        cmdContainer->l1CachePolicyDataRef() = &l1CachePolicyData;

        bool requiresUncachedMocs = false;
        EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
        EncodeDispatchKernel<FamilyType>::template encode<COMPUTE_WALKER>(*cmdContainer.get(), dispatchArgs);

        GenCmdList commands;
        CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());
        auto itor = find<COMPUTE_WALKER *>(commands.begin(), commands.end());
        ASSERT_NE(itor, commands.end());
        auto cmd = genCmdCast<COMPUTE_WALKER *>(*itor);
        auto &idd = cmd->getInterfaceDescriptor();

        EXPECT_NE(0u, idd.getBindingTableEntryCount());
        if constexpr (FamilyType::supportsSampler) {
            if (pDevice->getDeviceInfo().imageSupport) {
                EXPECT_NE(INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT_NO_SAMPLERS_USED, idd.getSamplerCount());
            }
        }
    }
}

HWTEST2_F(CommandEncodeStatesTest, givenDispatchInterfaceWhenNumRequiredGrfIsNotDefaultThenStateComputeModeCommandAdded, MatchAny) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceGrfNumProgrammingWithScm.set(1);

    StreamProperties streamProperties{};
    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    streamProperties.initSupport(rootDeviceEnvironment);
    streamProperties.stateComputeMode.setPropertiesAll(false, 128, 0u, PreemptionMode::Disabled, false);
    streamProperties.stateComputeMode.setPropertiesAll(false, 128, 0u, PreemptionMode::Disabled, false);
    EXPECT_FALSE(streamProperties.stateComputeMode.isDirty());

    streamProperties.stateComputeMode.setPropertiesAll(false, 256, 0u, PreemptionMode::Disabled, false);
    if constexpr (TestTraits<FamilyType::gfxCoreFamily>::largeGrfModeInStateComputeModeSupported) {
        EXPECT_TRUE(streamProperties.stateComputeMode.isDirty());
    } else {
        EXPECT_FALSE(streamProperties.stateComputeMode.isDirty());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenInlineDataRequiredWhenEncodingWalkerThenEmitInlineParameterIsSet) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    dispatchInterface->kernelDescriptor.kernelAttributes.flags.passInlineData = true;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);
    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmd = genCmdCast<DefaultWalkerType *>(*itor);
    EXPECT_EQ(1u, cmd->getEmitInlineParameter());

    constexpr auto inlineDataSize = DefaultWalkerType::getInlineDataSize();
    size_t expectedSizeIOH = alignUp(dispatchInterface->getCrossThreadDataSize() +
                                         dispatchInterface->getPerThreadDataSizeForWholeThreadGroup() -
                                         inlineDataSize,
                                     NEO::EncodeDispatchKernel<FamilyType>::getDefaultIOHAlignment());
    auto heap = cmdContainer->getIndirectHeap(HeapType::indirectObject);
    EXPECT_EQ(expectedSizeIOH, heap->getUsed());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenInlineDataRequiredIsFalseWhenEncodingWalkerThenEmitInlineParameterIsNotSet) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    dispatchInterface->kernelDescriptor.kernelAttributes.flags.passInlineData = false;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmd = genCmdCast<DefaultWalkerType *>(*itor);
    EXPECT_EQ(0u, cmd->getEmitInlineParameter());

    size_t expectedSizeIOH = alignUp(dispatchInterface->getCrossThreadDataSize() +
                                         dispatchInterface->getPerThreadDataSizeForWholeThreadGroup(),
                                     NEO::EncodeDispatchKernel<FamilyType>::getDefaultIOHAlignment());
    auto heap = cmdContainer->getIndirectHeap(HeapType::indirectObject);
    EXPECT_EQ(expectedSizeIOH, heap->getUsed());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenInlineDataRequiredAndZeroCrossThreadDataSizeWhenEncodingWalkerThenEmitInlineParameterIsNotSet) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    dispatchInterface->kernelDescriptor.kernelAttributes.flags.passInlineData = true;
    dispatchInterface->getCrossThreadDataSizeResult = 0u;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmd = genCmdCast<DefaultWalkerType *>(*itor);
    EXPECT_EQ(0u, cmd->getEmitInlineParameter());

    auto heap = cmdContainer->getIndirectHeap(HeapType::indirectObject);
    EXPECT_EQ(dispatchInterface->getPerThreadDataSizeForWholeThreadGroup(), heap->getUsed());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenInlineDataRequiredAndLocalIdsGenerateByHwWhenEncodingWalkerThenEmitInlineParameterAndGenerateLocalIdsIsSet) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->requiredWalkGroupOrder = 2u;
    dispatchInterface->kernelDescriptor.kernelAttributes.flags.passInlineData = true;
    dispatchInterface->kernelDescriptor.kernelAttributes.numLocalIdChannels = 3u;
    dispatchInterface->kernelDescriptor.kernelAttributes.localId[0] = 1;
    dispatchInterface->kernelDescriptor.kernelAttributes.localId[1] = 1;
    dispatchInterface->kernelDescriptor.kernelAttributes.localId[2] = 1;
    dispatchInterface->kernelDescriptor.kernelAttributes.simdSize = 32u;
    dispatchInterface->getCrossThreadDataSizeResult = 32u;

    dispatchInterface->requiresGenerationOfLocalIdsByRuntimeResult = false;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmd = genCmdCast<DefaultWalkerType *>(*itor);
    EXPECT_EQ(1u, cmd->getEmitInlineParameter());
    EXPECT_EQ(1u, cmd->getGenerateLocalId());
    uint32_t expectedEmitLocalIds = (1 << 0) | (1 << 1) | (1 << 2);
    EXPECT_EQ(expectedEmitLocalIds, cmd->getEmitLocalId());
    EXPECT_EQ(2u, cmd->getWalkOrder());
    EXPECT_EQ(31u, cmd->getLocalXMaximum());
    EXPECT_EQ(0u, cmd->getLocalYMaximum());
    EXPECT_EQ(0u, cmd->getLocalZMaximum());
}

HWTEST2_F(CommandEncodeStatesTest, givenInterfaceDescriptorDataWhenForceThreadGroupDispatchSizeVariableIsDefaultThenThreadGroupDispatchSizeIsNotChanged, IsXeLpg) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    INTERFACE_DESCRIPTOR_DATA iddArg;
    DefaultWalkerType walkerCmd{};
    walkerCmd.setThreadGroupIdXDimension(1);
    walkerCmd.setThreadGroupIdYDimension(1);
    walkerCmd.setThreadGroupIdZDimension(1);
    iddArg = FamilyType::cmdInitInterfaceDescriptorData;
    const uint32_t forceThreadGroupDispatchSize = -1;
    auto hwInfo = pDevice->getHardwareInfo();
    const auto &productHelper = pDevice->getProductHelper();

    DebugManagerStateRestore restorer;
    debugManager.flags.ForceThreadGroupDispatchSize.set(forceThreadGroupDispatchSize);
    uint32_t threadGroups[] = {walkerCmd.getThreadGroupIdXDimension(), walkerCmd.getThreadGroupIdYDimension(), walkerCmd.getThreadGroupIdZDimension()};
    const uint32_t threadGroupCount = walkerCmd.getThreadGroupIdXDimension() * walkerCmd.getThreadGroupIdYDimension() * walkerCmd.getThreadGroupIdZDimension();
    uint32_t revisions[] = {REVISION_A0, REVISION_B};
    uint32_t threadsPerThreadGroup = 4;
    for (auto revision : revisions) {
        hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(revision, hwInfo);
        EncodeDispatchKernel<FamilyType>::encodeThreadGroupDispatch(iddArg, *pDevice, hwInfo, threadGroups, threadGroupCount, 0, 0, threadsPerThreadGroup, walkerCmd);

        if (productHelper.isDisableOverdispatchAvailable(hwInfo)) {
            EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1, iddArg.getThreadGroupDispatchSize());
        } else {
            EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_8, iddArg.getThreadGroupDispatchSize());
        }
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenInterfaceDescriptorDataWhenForceThreadGroupDispatchSizeVariableIsSetThenThreadGroupDispatchSizeIsChanged) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA = typename DefaultWalkerType::InterfaceDescriptorType;
    INTERFACE_DESCRIPTOR_DATA iddArg;
    DefaultWalkerType walkerCmd{};
    walkerCmd.setThreadGroupIdXDimension(1);
    walkerCmd.setThreadGroupIdYDimension(1);
    walkerCmd.setThreadGroupIdZDimension(1);
    iddArg = FamilyType::template getInitInterfaceDescriptor<INTERFACE_DESCRIPTOR_DATA>();
    iddArg.setNumberOfThreadsInGpgpuThreadGroup(1u);

    const uint32_t forceThreadGroupDispatchSize = 1;
    const uint32_t defaultThreadGroupDispatchSize = iddArg.getThreadGroupDispatchSize();
    const uint32_t threadGroupCount = 1u;

    DebugManagerStateRestore restorer;
    debugManager.flags.ForceThreadGroupDispatchSize.set(forceThreadGroupDispatchSize);
    uint32_t threadGroups[] = {walkerCmd.getThreadGroupIdXDimension(), walkerCmd.getThreadGroupIdYDimension(), walkerCmd.getThreadGroupIdZDimension()};
    EncodeDispatchKernel<FamilyType>::encodeThreadGroupDispatch(iddArg, *pDevice, pDevice->getHardwareInfo(), threadGroups, threadGroupCount, 0, 1, 1, walkerCmd);

    EXPECT_NE(defaultThreadGroupDispatchSize, iddArg.getThreadGroupDispatchSize());
    EXPECT_EQ(forceThreadGroupDispatchSize, iddArg.getThreadGroupDispatchSize());
}

using WalkerThreadTestXeHPAndLater = WalkerThreadTest;

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerThreadTestXeHPAndLater, givenStartWorkGroupWhenIndirectIsFalseThenExpectStartGroupAndThreadDimensionsProgramming) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    DefaultWalkerType walkerCmd = FamilyType::template getInitGpuWalker<DefaultWalkerType>();

    startWorkGroup[1] = 2u;
    startWorkGroup[2] = 3u;
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
    EXPECT_EQ(3u, walkerCmd.getThreadGroupIdStartingZ());

    auto expectedSimd = getSimdConfig<DefaultWalkerType>(simd);
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
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    DefaultWalkerType walkerCmd = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
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
    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingZ());

    auto expectedSimd = getSimdConfig<DefaultWalkerType>(simd);
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
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    DefaultWalkerType walkerCmd = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
    startWorkGroup[1] = 2u;
    startWorkGroup[2] = 3u;
    workGroupSizes[0] = 30u;
    simd = 1;
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
    EXPECT_EQ(3u, walkerCmd.getThreadGroupIdStartingZ());

    auto expectedSimd = getSimdConfig<DefaultWalkerType>(simd);
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
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    DefaultWalkerType walkerCmd = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
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
    EXPECT_EQ(3u, walkerCmd.getThreadGroupIdStartingZ());

    auto expectedSimd = getSimdConfig<DefaultWalkerType>(simd);
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
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    DefaultWalkerType walkerCmd = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
    localIdDimensions = 0u;
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    EncodeDispatchKernel<FamilyType>::encodeThreadData(walkerCmd, nullptr, numWorkGroups, workGroupSizes, simd, localIdDimensions,
                                                       0, 0, false, false, false, requiredWorkGroupOrder, rootDeviceEnvironment);
    EXPECT_FALSE(walkerCmd.getIndirectParameterEnable());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdXDimension());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdYDimension());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdZDimension());

    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingX());
    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingY());
    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingZ());

    auto expectedSimd = getSimdConfig<DefaultWalkerType>(simd);
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
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    DefaultWalkerType walkerCmd = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
    requiredWorkGroupOrder = 2u;
    workGroupSizes[1] = workGroupSizes[2] = 2u;
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];

    uint8_t localIdDims[3] = {2,
                              1,
                              3};

    uint32_t expectedEmitLocalIds[3] = {(1 << 0) | (1 << 1),
                                        (1 << 0),
                                        (1 << 0) | (1 << 1) | (1 << 2)};

    for (int i = 0; i < 3; i++) {
        EncodeDispatchKernel<FamilyType>::encodeThreadData(walkerCmd, nullptr, numWorkGroups, workGroupSizes, simd, localIdDims[i],
                                                           0, 0, false, false, false, requiredWorkGroupOrder, rootDeviceEnvironment);
        EXPECT_FALSE(walkerCmd.getIndirectParameterEnable());
        EXPECT_EQ(1u, walkerCmd.getThreadGroupIdXDimension());
        EXPECT_EQ(1u, walkerCmd.getThreadGroupIdYDimension());
        EXPECT_EQ(1u, walkerCmd.getThreadGroupIdZDimension());

        EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingX());
        EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingY());
        EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingZ());

        auto expectedSimd = getSimdConfig<DefaultWalkerType>(simd);
        EXPECT_EQ(expectedSimd, walkerCmd.getSimdSize());
        EXPECT_EQ(expectedSimd, walkerCmd.getMessageSimd());

        EXPECT_EQ(0xffffffffu, walkerCmd.getExecutionMask());

        EXPECT_EQ(expectedEmitLocalIds[i], walkerCmd.getEmitLocalId());
        EXPECT_EQ(31u, walkerCmd.getLocalXMaximum());
        EXPECT_EQ(1u, walkerCmd.getLocalYMaximum());
        EXPECT_EQ(1u, walkerCmd.getLocalZMaximum());
        EXPECT_EQ(2u, walkerCmd.getWalkOrder());

        EXPECT_TRUE(walkerCmd.getGenerateLocalId());
        EXPECT_FALSE(walkerCmd.getEmitInlineParameter());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerThreadTestXeHPAndLater, givenLocalIdGenerationByHwWhenLocalIdsNotPresentThenEmitLocalIdsIsNotSet) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    DefaultWalkerType walkerCmd = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
    requiredWorkGroupOrder = 2u;
    workGroupSizes[1] = workGroupSizes[2] = 2u;
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];

    uint8_t localIdDims = 0;
    uint32_t expectedEmitLocalIds = 0;

    EncodeDispatchKernel<FamilyType>::encodeThreadData(walkerCmd, nullptr, numWorkGroups, workGroupSizes, simd, localIdDims,
                                                       0, 0, false, false, false, requiredWorkGroupOrder, rootDeviceEnvironment);
    EXPECT_FALSE(walkerCmd.getIndirectParameterEnable());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdXDimension());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdYDimension());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdZDimension());

    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingX());
    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingY());
    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingZ());

    auto expectedSimd = getSimdConfig<DefaultWalkerType>(simd);
    EXPECT_EQ(expectedSimd, walkerCmd.getSimdSize());
    EXPECT_EQ(expectedSimd, walkerCmd.getMessageSimd());

    EXPECT_EQ(0xffffffffu, walkerCmd.getExecutionMask());

    EXPECT_EQ(expectedEmitLocalIds, walkerCmd.getEmitLocalId());
    EXPECT_EQ(0u, walkerCmd.getLocalXMaximum());
    EXPECT_EQ(0u, walkerCmd.getLocalYMaximum());
    EXPECT_EQ(0u, walkerCmd.getLocalZMaximum());
    EXPECT_EQ(0u, walkerCmd.getWalkOrder());
    EXPECT_FALSE(walkerCmd.getGenerateLocalId());
    EXPECT_FALSE(walkerCmd.getEmitInlineParameter());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerThreadTestXeHPAndLater, givenDebugVariableToOverrideSimdMessageSizeWhenWalkerIsProgrammedItIsOverwritten) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceSimdMessageSizeInWalker.set(1);

    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    DefaultWalkerType walkerCmd = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
    requiredWorkGroupOrder = 2u;
    workGroupSizes[1] = workGroupSizes[2] = 2u;

    EncodeDispatchKernel<FamilyType>::encodeThreadData(walkerCmd, nullptr, numWorkGroups, workGroupSizes, simd, localIdDimensions,
                                                       0, 0, false, false, false, requiredWorkGroupOrder, rootDeviceEnvironment);
    EXPECT_EQ(1u, walkerCmd.getMessageSimd());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, WalkerThreadTestXeHPAndLater, WhenInlineDataIsTrueThenExpectInlineDataProgramming) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    DefaultWalkerType walkerCmd = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
    startWorkGroup[1] = 2u;
    startWorkGroup[2] = 3u;
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    EncodeDispatchKernel<FamilyType>::encodeThreadData(walkerCmd, startWorkGroup, numWorkGroups, workGroupSizes, simd, localIdDimensions,
                                                       0, 0, true, true, false, requiredWorkGroupOrder, rootDeviceEnvironment);
    EXPECT_FALSE(walkerCmd.getIndirectParameterEnable());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdXDimension());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdYDimension());
    EXPECT_EQ(1u, walkerCmd.getThreadGroupIdZDimension());

    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingX());
    EXPECT_EQ(2u, walkerCmd.getThreadGroupIdStartingY());
    EXPECT_EQ(3u, walkerCmd.getThreadGroupIdStartingZ());

    auto expectedSimd = getSimdConfig<DefaultWalkerType>(simd);
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
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    DefaultWalkerType walkerCmd = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
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
    EXPECT_EQ(0u, walkerCmd.getThreadGroupIdStartingZ());

    auto expectedSimd = getSimdConfig<DefaultWalkerType>(simd);
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

template <bool flushTaskUsedForImmediate, bool usePrimaryBuffer>
struct CommandEncodeStatesImplicitScalingFixtureT : public CommandEncodeStatesFixture {
    void setUp() {
        debugManager.flags.CreateMultipleSubDevices.set(2);
        mockDeviceBackup = std::make_unique<VariableBackup<bool>>(&MockDevice::createSingleDevice, false);
        apiSupportBackup = std::make_unique<VariableBackup<bool>>(&ImplicitScaling::apiSupport, true);

        CommandEncodeStatesFixture::setUp();

        cmdContainer->setUsingPrimaryBuffer(usePrimaryBuffer);
        cmdContainer->setFlushTaskUsedForImmediate(flushTaskUsedForImmediate);
    }

    void tearDown() {
        CommandEncodeStatesFixture::tearDown();
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<VariableBackup<bool>> mockDeviceBackup;
    std::unique_ptr<VariableBackup<bool>> apiSupportBackup;
};

using CommandEncodeStatesImplicitScalingFixture = CommandEncodeStatesImplicitScalingFixtureT<false, false>;
using CommandEncodeStatesImplicitScaling = Test<CommandEncodeStatesImplicitScalingFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesImplicitScaling, givenCooperativeKernelWhenEncodingDispatchKernelThenExpectPartitionSizeEqualWorkgroupSize) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    uint32_t dims[] = {16, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    bool requiresUncachedMocs = false;
    bool isInternal = false;
    bool isCooperative = true;

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.isInternal = isInternal;
    dispatchArgs.isCooperative = isCooperative;
    dispatchArgs.partitionCount = 2;
    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    size_t containerUsedAfterBase = cmdContainer->getCommandStream()->getUsed();

    GenCmdList partitionedWalkerList;
    CmdParse<FamilyType>::parseCommandBuffer(partitionedWalkerList, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), containerUsedAfterBase);
    auto itor = find<DefaultWalkerType *>(partitionedWalkerList.begin(), partitionedWalkerList.end());
    ASSERT_NE(itor, partitionedWalkerList.end());

    auto partitionWalkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
    EXPECT_EQ(DefaultWalkerType::PARTITION_TYPE::PARTITION_TYPE_X, partitionWalkerCmd->getPartitionType());

    const auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    uint32_t expectedPartitionSize = dims[0];

    if (!gfxCoreHelper.singleTileExecImplicitScalingRequired(isCooperative)) {
        expectedPartitionSize /= dispatchArgs.partitionCount;
    }

    EXPECT_EQ(expectedPartitionSize, partitionWalkerCmd->getPartitionSize());
}

struct CommandEncodeStatesDynamicImplicitScalingFixture : CommandEncodeStatesImplicitScalingFixture {
    void setUp() {
        debugManager.flags.EnableStaticPartitioning.set(0);
        CommandEncodeStatesImplicitScalingFixture::setUp();
    }

    void tearDown() {
        CommandEncodeStatesImplicitScalingFixture::tearDown();
    }

    DebugManagerStateRestore restore{};
};

using CommandEncodeStatesDynamicImplicitScaling = Test<CommandEncodeStatesDynamicImplicitScalingFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesDynamicImplicitScaling, givenImplicitScalingWhenEncodingDispatchKernelThenExpectPartitionCommandBuffer) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    uint32_t dims[] = {16, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    bool requiresUncachedMocs = false;
    bool isInternal = false;

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.isInternal = isInternal;
    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    size_t containerUsedAfterBase = cmdContainer->getCommandStream()->getUsed();

    GenCmdList baseWalkerList;
    CmdParse<FamilyType>::parseCommandBuffer(baseWalkerList, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), containerUsedAfterBase);
    auto itor = find<DefaultWalkerType *>(baseWalkerList.begin(), baseWalkerList.end());
    ASSERT_NE(itor, baseWalkerList.end());
    auto baseWalkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
    EXPECT_EQ(DefaultWalkerType::PARTITION_TYPE::PARTITION_TYPE_DISABLED, baseWalkerCmd->getPartitionType());
    EXPECT_EQ(16u, baseWalkerCmd->getThreadGroupIdXDimension());

    dispatchArgs.partitionCount = 2;
    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    size_t total = cmdContainer->getCommandStream()->getUsed();
    size_t partitionedWalkerSize = total - containerUsedAfterBase;

    size_t expectedPartitionedWalkerSize = ImplicitScalingDispatch<FamilyType>::template getSize<DefaultWalkerType>(true, false, pDevice->getDeviceBitfield(), Vec3<size_t>(0, 0, 0), Vec3<size_t>(16, 1, 1));
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

    itor = find<DefaultWalkerType *>(partitionedWalkerList.begin(), partitionedWalkerList.end());
    ASSERT_NE(itor, partitionedWalkerList.end());
    auto partitionWalkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
    EXPECT_EQ(DefaultWalkerType::PARTITION_TYPE::PARTITION_TYPE_X, partitionWalkerCmd->getPartitionType());
    uint32_t expectedPartitionSize = (dims[0] + dispatchArgs.partitionCount - 1u) / dispatchArgs.partitionCount;
    EXPECT_EQ(expectedPartitionSize, partitionWalkerCmd->getPartitionSize());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesDynamicImplicitScaling, givenImplicitScalingRequiresPipeControlStallWhenEncodingDispatchKernelThenExpectCrossTileSyncAndSelfCleanupSection) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
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
    dispatchArgs.postSyncArgs.dcFlushEnable = MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, pDevice->getRootDeviceEnvironment());

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    EXPECT_EQ(2u, dispatchArgs.partitionCount);
    size_t partitionedWalkerSize = cmdContainer->getCommandStream()->getUsed();

    size_t expectedPartitionedWalkerSize = ImplicitScalingDispatch<FamilyType>::template getSize<DefaultWalkerType>(true, false, pDevice->getDeviceBitfield(), Vec3<size_t>(0, 0, 0), Vec3<size_t>(16, 1, 1));
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

    auto itor = find<DefaultWalkerType *>(partitionedWalkerList.begin(), partitionedWalkerList.end());
    ASSERT_NE(itor, partitionedWalkerList.end());
    auto partitionWalkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
    EXPECT_EQ(DefaultWalkerType::PARTITION_TYPE::PARTITION_TYPE_X, partitionWalkerCmd->getPartitionType());
    uint32_t expectedPartitionSize = (dims[0] + dispatchArgs.partitionCount - 1u) / dispatchArgs.partitionCount;
    EXPECT_EQ(expectedPartitionSize, partitionWalkerCmd->getPartitionSize());

    WalkerPartition::WalkerPartitionArgs args = {};
    args.initializeWparidRegister = true;
    args.emitPipeControlStall = true;
    args.partitionCount = dispatchArgs.partitionCount;
    args.emitSelfCleanup = true;
    args.dcFlushEnable = dispatchArgs.postSyncArgs.dcFlushEnable;

    auto cleanupSectionOffset = WalkerPartition::computeControlSectionOffset<FamilyType, DefaultWalkerType>(args);
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
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
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

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    EXPECT_EQ(2u, dispatchArgs.partitionCount);
    size_t partitionedWalkerSize = cmdContainer->getCommandStream()->getUsed();

    size_t expectedPartitionedWalkerSize = ImplicitScalingDispatch<FamilyType>::template getSize<DefaultWalkerType>(true, false, pDevice->getDeviceBitfield(), Vec3<size_t>(0, 0, 0), Vec3<size_t>(16, 1, 1));
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

    auto itor = find<DefaultWalkerType *>(partitionedWalkerList.begin(), partitionedWalkerList.end());
    ASSERT_NE(itor, partitionedWalkerList.end());
    auto partitionWalkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
    EXPECT_EQ(DefaultWalkerType::PARTITION_TYPE::PARTITION_TYPE_X, partitionWalkerCmd->getPartitionType());
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
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    uint32_t dims[] = {16, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    bool isInternal = true;
    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.isInternal = isInternal;
    dispatchArgs.partitionCount = 2;

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    size_t internalWalkerSize = cmdContainer->getCommandStream()->getUsed();

    GenCmdList internalWalkerList;
    CmdParse<FamilyType>::parseCommandBuffer(internalWalkerList,
                                             cmdContainer->getCommandStream()->getCpuBase(),
                                             internalWalkerSize);
    auto itor = find<DefaultWalkerType *>(internalWalkerList.begin(), internalWalkerList.end());
    ASSERT_NE(itor, internalWalkerList.end());
    auto internalWalkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
    EXPECT_EQ(DefaultWalkerType::PARTITION_TYPE::PARTITION_TYPE_DISABLED, internalWalkerCmd->getPartitionType());
    EXPECT_EQ(16u, internalWalkerCmd->getThreadGroupIdXDimension());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenNonTimestampEventWhenTimestampPostSyncRequiredThenTimestampPostSyncIsAdded) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using POSTSYNC_DATA = decltype(FamilyType::template getPostSyncType<DefaultWalkerType>());
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint64_t eventAddress = MemoryConstants::cacheLineSize * 123;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.postSyncArgs.eventAddress = eventAddress;
    dispatchArgs.postSyncArgs.isTimestampEvent = true;

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());
    auto cmd = genCmdCast<DefaultWalkerType *>(*itor);
    EXPECT_EQ(POSTSYNC_DATA::OPERATION_WRITE_TIMESTAMP, cmd->getPostSync().getOperation());
}

HWTEST2_F(CommandEncodeStatesTest,
          givenDispatchInterfaceWhenDpasRequiredIsNotDefaultThenPipelineSelectCommandAdded, IsXeCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    bool dpasModeRequired = true;
    cmdContainer->lastPipelineSelectModeRequiredRef() = false;

    dispatchInterface->kernelDescriptor.kernelAttributes.flags.usesSystolicPipelineSelectMode = dpasModeRequired;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands,
                                             ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0),
                                             cmdContainer->getCommandStream()->getUsed());

    auto itorCmd = find<PIPELINE_SELECT *>(commands.begin(), commands.end());

    if (pDevice->getUltCommandStreamReceiver<FamilyType>().pipelineSupportFlags.systolicMode) {
        ASSERT_NE(itorCmd, commands.end());

        auto cmd = genCmdCast<PIPELINE_SELECT *>(*itorCmd);
        EXPECT_EQ(cmd->getSystolicModeEnable(), dpasModeRequired);
    } else {
        EXPECT_EQ(itorCmd, commands.end());
    }
}

HWTEST2_F(CommandEncodeStatesTest,
          givenDebugVariableWhenEncodeStateIsCalledThenSystolicValueIsOverwritten, IsXeCore) {
    DebugManagerStateRestore restorer;
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    bool dpasModeRequired = true;
    debugManager.flags.OverrideSystolicPipelineSelect.set(!dpasModeRequired);
    cmdContainer->lastPipelineSelectModeRequiredRef() = false;
    dispatchInterface->kernelDescriptor.kernelAttributes.flags.usesSystolicPipelineSelectMode = dpasModeRequired;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands,
                                             ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0),
                                             cmdContainer->getCommandStream()->getUsed());

    auto itorCmd = find<PIPELINE_SELECT *>(commands.begin(), commands.end());

    if (pDevice->getUltCommandStreamReceiver<FamilyType>().pipelineSupportFlags.systolicMode) {
        ASSERT_NE(itorCmd, commands.end());

        auto cmd = genCmdCast<PIPELINE_SELECT *>(*itorCmd);
        EXPECT_EQ(cmd->getSystolicModeEnable(), !dpasModeRequired);
    } else {
        EXPECT_EQ(itorCmd, commands.end());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest,
            givenDispatchInterfaceWhenDpasRequiredIsSameAsDefaultThenPipelineSelectCommandNotAdded) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    bool dpasModeRequired = true;
    cmdContainer->lastPipelineSelectModeRequiredRef() = dpasModeRequired;
    dispatchInterface->kernelDescriptor.kernelAttributes.flags.usesSystolicPipelineSelectMode = dpasModeRequired;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands,
                                             ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0),
                                             cmdContainer->getCommandStream()->getUsed());

    auto itorCmd = find<PIPELINE_SELECT *>(commands.begin(), commands.end());
    EXPECT_EQ(itorCmd, commands.end());
}

template <bool flushTaskUsedForImmediate, bool usePrimaryBuffer>
struct CommandEncodeStatesImplicitScalingPrimaryBufferFixture : public CommandEncodeStatesImplicitScalingFixtureT<flushTaskUsedForImmediate, usePrimaryBuffer> {
    using BaseClass = CommandEncodeStatesImplicitScalingFixtureT<flushTaskUsedForImmediate, usePrimaryBuffer>;
    void setUp() {
        debugManager.flags.UsePipeControlAfterPartitionedWalker.set(1);
        BaseClass::setUp();
    }

    void tearDown() {
        BaseClass::tearDown();
    }

    template <typename FamilyType>
    void testBodyFindPrimaryBatchBuffer() {
        using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
        using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
        constexpr bool expectPrimary = flushTaskUsedForImmediate || usePrimaryBuffer;

        uint32_t dims[] = {16, 1, 1};
        std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

        bool requiresUncachedMocs = false;
        uint64_t eventAddress = 0xFF112233000;
        EncodeDispatchKernelArgs dispatchArgs = BaseClass::createDefaultDispatchKernelArgs(BaseClass::pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
        dispatchArgs.postSyncArgs.eventAddress = eventAddress;
        dispatchArgs.partitionCount = 2;

        EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*BaseClass::cmdContainer.get(), dispatchArgs);
        size_t usedBuffer = BaseClass::cmdContainer->getCommandStream()->getUsed();
        EXPECT_EQ(2u, dispatchArgs.partitionCount);

        GenCmdList cmdList;
        CmdParse<FamilyType>::parseCommandBuffer(
            cmdList,
            BaseClass::cmdContainer->getCommandStream()->getCpuBase(),
            usedBuffer);

        auto itBbStart = find<MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), itBbStart);

        auto bbStartCmd = reinterpret_cast<MI_BATCH_BUFFER_START *>(*itBbStart);
        EXPECT_EQ(expectPrimary, !static_cast<bool>(bbStartCmd->getSecondLevelBatchBuffer()));
    }
};

using CommandEncodeStatesImplicitScalingFlushTaskTest = Test<CommandEncodeStatesImplicitScalingPrimaryBufferFixture<true, false>>;
using CommandEncodeStatesImplicitScalingPrimaryBufferTest = Test<CommandEncodeStatesImplicitScalingPrimaryBufferFixture<false, true>>;
using CommandEncodeStatesImplicitScalingSecondaryBufferTest = Test<CommandEncodeStatesImplicitScalingPrimaryBufferFixture<false, false>>;

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesImplicitScalingFlushTaskTest,
            givenDispatchImplicitScalingWithBbStartOverControlSectionWhenDispatchingFromFlushTaskContainerThenExpectPrimaryBatchBuffer) {
    testBodyFindPrimaryBatchBuffer<FamilyType>();
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesImplicitScalingPrimaryBufferTest,
            givenDispatchImplicitScalingWithBbStartOverControlSectionWhenDispatchingAsPrimaryBufferContainerThenExpectPrimaryBatchBuffer) {
    testBodyFindPrimaryBatchBuffer<FamilyType>();
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesImplicitScalingSecondaryBufferTest,
            givenDispatchImplicitScalingWithBbStartOverControlSectionWhenDispatchingAsSecondaryBufferContainerThenExpectSecondaryBatchBuffer) {
    testBodyFindPrimaryBatchBuffer<FamilyType>();
}

using EncodeKernelScratchProgrammingTest = Test<ScratchProgrammingFixture>;

HWTEST2_F(EncodeKernelScratchProgrammingTest, givenHeaplessModeDisabledWhenSetScratchAddressIsCalledThenDoNothing, IsAtLeastXeCore) {

    static constexpr bool heaplessModeEnabled = false;
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    uint64_t scratchAddress = 0;
    uint32_t requiredScratchSlot0Size = 64;
    uint32_t requiredScratchSlot1Size = 0;

    EncodeDispatchKernel<FamilyType>::template setScratchAddress<heaplessModeEnabled>(scratchAddress, requiredScratchSlot0Size, requiredScratchSlot1Size, ssh, ultCsr);

    uint64_t expectedScratchAddress = 0;
    EXPECT_EQ(expectedScratchAddress, scratchAddress);
}

HWTEST2_F(CommandEncodeStatesTest, givenEncodeDispatchKernelWhenGettingInlineDataOffsetThenReturnWalkerInlineOffset, IsHeapfulRequiredAndAtLeastXeCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    EncodeDispatchKernelArgs dispatchArgs = {};

    size_t expectedOffset = offsetof(DefaultWalkerType, TheStructure.Common.InlineData);

    EXPECT_EQ(expectedOffset, EncodeDispatchKernel<FamilyType>::getInlineDataOffset(dispatchArgs));
}

HWTEST2_F(CommandEncodeStatesTest, givenEncodeDispatchKernelWhenCpuWalkerPointerIsSetThenProvideWalkerContentInCpuBuffer, IsAtLeastXeCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    auto walkerPtr = std::make_unique<DefaultWalkerType>();
    DefaultWalkerType *cpuWalkerPointer = walkerPtr.get();

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, false);
    dispatchArgs.cpuWalkerBuffer = cpuWalkerPointer;

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands,
                                             cmdContainer->getCommandStream()->getCpuBase(),
                                             cmdContainer->getCommandStream()->getUsed());

    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmdWalkerGfxMemory = genCmdCast<DefaultWalkerType *>(*itor);

    EXPECT_EQ(0, memcmp(cmdWalkerGfxMemory, cpuWalkerPointer, sizeof(DefaultWalkerType)));
}

HWTEST2_F(CommandEncodeStatesTest, givenEncodeDispatchKernelWhenRequestingExtraPayloadSpaceThenConsumeExtraIndirectHeapSpace, IsAtLeastXeCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    dispatchInterface->kernelDescriptor.kernelAttributes.flags.passInlineData = false;
    dispatchInterface->getCrossThreadDataSizeResult = 64;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.reserveExtraPayloadSpace = 1024;

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    auto heap = cmdContainer->getIndirectHeap(HeapType::indirectObject);

    size_t expectedConsumedSize = 64 + 1024;
    expectedConsumedSize = alignUp(expectedConsumedSize, NEO::EncodeDispatchKernel<FamilyType>::getDefaultIOHAlignment());
    EXPECT_EQ(expectedConsumedSize, heap->getUsed());
}

HWTEST2_F(CommandEncodeStatesTest, givenForceComputeWalkerPostSyncFlushWithWriteWhenEncodeIsCalledThenPostSyncIsProgrammedCorrectly, IsAtLeastXeCore) {

    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using PostSyncType = decltype(FamilyType::template getPostSyncType<DefaultWalkerType>());
    using OPERATION = typename PostSyncType::OPERATION;

    DebugManagerStateRestore restore;
    debugManager.flags.ForceComputeWalkerPostSyncFlushWithWrite.set(0);

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, false);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());
    auto it = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(it, commands.end());

    auto walker = genCmdCast<DefaultWalkerType *>(*it);
    auto &postSync = walker->getPostSync();
    EXPECT_TRUE(postSync.getDataportPipelineFlush());
    EXPECT_TRUE(postSync.getDataportSubsliceCacheFlush());

    uint64_t expectedAddress = 0u;
    EXPECT_EQ(expectedAddress, postSync.getDestinationAddress());
    EXPECT_EQ(OPERATION::OPERATION_WRITE_IMMEDIATE_DATA, postSync.getOperation());
    uint64_t expectedData = 0u;
    EXPECT_EQ(expectedData, postSync.getImmediateData());
}

HWTEST2_F(CommandEncodeStatesTest, givenEncodeDispatchKernelWhenRequestingCommandViewThenDoNotConsumeCmdBufferAndHeapSpace, IsAtLeastXeCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    auto payloadHeap = cmdContainer->getIndirectHeap(HeapType::indirectObject);
    auto payloadHeapUsed = payloadHeap->getUsed();

    auto cmdBuffer = cmdContainer->getCommandStream();
    auto cmdBufferUsed = cmdBuffer->getUsed();

    uint8_t payloadView[256] = {};
    dispatchInterface->getCrossThreadDataSizeResult = 64;

    auto walkerPtr = std::make_unique<DefaultWalkerType>();
    DefaultWalkerType *cpuWalkerPointer = walkerPtr.get();

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.makeCommandView = true;
    dispatchArgs.cpuPayloadBuffer = payloadView;
    dispatchArgs.cpuWalkerBuffer = cpuWalkerPointer;

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    EXPECT_EQ(payloadHeapUsed, payloadHeap->getUsed());
    EXPECT_EQ(cmdBufferUsed, cmdBuffer->getUsed());
}

HWTEST2_F(CommandEncodeStatesTest, givenEncodeDispatchKernelWhenRequestingCommandViewWithoutCpuPointersThenExpectUnrecoverable, IsAtLeastXeCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    uint8_t payloadView[256] = {};
    dispatchInterface->getCrossThreadDataSizeResult = 64;

    auto walkerPtr = std::make_unique<DefaultWalkerType>();
    DefaultWalkerType *cpuWalkerPointer = walkerPtr.get();

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.makeCommandView = true;
    dispatchArgs.cpuPayloadBuffer = nullptr;
    dispatchArgs.cpuWalkerBuffer = cpuWalkerPointer;

    EXPECT_ANY_THROW(EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs));

    dispatchArgs.cpuPayloadBuffer = payloadView;
    dispatchArgs.cpuWalkerBuffer = nullptr;

    EXPECT_ANY_THROW(EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs));
}

struct MultiTileCommandEncodeStatesFixture : public CommandEncodeStatesFixture {
    void setUp() {
        debugManager.flags.CreateMultipleSubDevices.set(2);
        CommandEncodeStatesFixture::setUp();
    }

    DebugManagerStateRestore restorer;
};

using MultiTileCommandEncodeStatesTest = Test<MultiTileCommandEncodeStatesFixture>;
HWTEST2_F(MultiTileCommandEncodeStatesTest, givenEncodeDispatchKernelInImplicitScalingWhenRequestingCommandViewThenDoNotConsumeCmdBufferAndHeapSpace, IsAtLeastXeCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(pDevice->getDefaultEngine().commandStreamReceiver);
    ultCsr->staticWorkPartitioningEnabled = true;
    ultCsr->createWorkPartitionAllocation(*pDevice);

    auto payloadHeap = cmdContainer->getIndirectHeap(HeapType::indirectObject);
    auto payloadHeapUsed = payloadHeap->getUsed();

    auto cmdBuffer = cmdContainer->getCommandStream();
    auto cmdBufferUsed = cmdBuffer->getUsed();

    uint8_t payloadView[256] = {};
    dispatchInterface->getCrossThreadDataSizeResult = 64;

    auto walkerPtr = std::make_unique<DefaultWalkerType>();
    DefaultWalkerType *cpuWalkerPointer = walkerPtr.get();

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.makeCommandView = true;
    dispatchArgs.partitionCount = 2;
    dispatchArgs.cpuPayloadBuffer = payloadView;
    dispatchArgs.cpuWalkerBuffer = cpuWalkerPointer;

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    EXPECT_EQ(payloadHeapUsed, payloadHeap->getUsed());
    EXPECT_EQ(cmdBufferUsed, cmdBuffer->getUsed());
}
