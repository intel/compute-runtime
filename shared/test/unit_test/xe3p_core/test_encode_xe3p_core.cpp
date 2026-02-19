/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/walker_partition_xehp_and_later.h"
#include "shared/source/command_stream/scratch_space_controller.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/gmm_helper/cache_settings_helper.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/definitions/command_encoder_args.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_walk_order.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"
#include "shared/test/unit_test/helpers/state_base_address_tests.h"
#include "shared/test/unit_test/mocks/mock_dispatch_kernel_encoder_interface.h"

#include "hw_cmds_xe3p_core.h"
#include "per_product_test_definitions.h"

using namespace NEO;

using CommandEncodeXe3pCoreTest = ::testing::Test;

XE3P_CORETEST_F(CommandEncodeXe3pCoreTest, whenMiFlushDwIsProgrammedThenSetAndFlushLlcWithoutCcs) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    uint8_t buffer[2 * sizeof(MI_FLUSH_DW)] = {};
    LinearStream linearStream(buffer, sizeof(buffer));
    MockExecutionEnvironment mockExecutionEnvironment{};
    NEO::EncodeDummyBlitWaArgs waArgs{false, mockExecutionEnvironment.rootDeviceEnvironments[0].get()};
    MiFlushArgs args{waArgs};
    args.commandWithPostSync = true;

    EncodeMiFlushDW<FamilyType>::programWithWa(linearStream, 0x1230000, 456, args);
    auto miFlushDwCmd = reinterpret_cast<MI_FLUSH_DW *>(buffer);
    auto expectedFlushCcs = NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *mockExecutionEnvironment.rootDeviceEnvironments[0]) ? 1u : 0u;
    EXPECT_EQ(expectedFlushCcs, miFlushDwCmd->getFlushCcs());
    EXPECT_EQ(1u, miFlushDwCmd->getFlushLlc());
}

XE3P_CORETEST_F(CommandEncodeXe3pCoreTest, givenInterfaceDescriptorDataWhenEncodeComputeWalkerAndAdjustInterfaceDescriptoDataIsCalledThenArgsDoesntChange) {
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using INTERFACE_DESCRIPTOR_DATA = typename COMPUTE_WALKER::InterfaceDescriptorType;
    INTERFACE_DESCRIPTOR_DATA iddArg;
    COMPUTE_WALKER walkerCmd{};
    walkerCmd.setThreadGroupIdXDimension(1);
    walkerCmd.setThreadGroupIdYDimension(1);
    walkerCmd.setThreadGroupIdZDimension(1);
    iddArg = FamilyType::template getInitInterfaceDescriptor<INTERFACE_DESCRIPTOR_DATA>();
    auto samplerCount = static_cast<typename INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT>(1u);
    iddArg.setSamplerCount(samplerCount);
    iddArg.setBindingTableEntryCount(2u);
    MockDevice mockDevice;
    uint32_t threadsPerGroup = 1;
    uint32_t threadGroups[] = {walkerCmd.getThreadGroupIdXDimension(), walkerCmd.getThreadGroupIdYDimension(), walkerCmd.getThreadGroupIdZDimension()};
    EncodeDispatchKernel<FamilyType>::encodeThreadGroupDispatch(iddArg, mockDevice, *defaultHwInfo, threadGroups, 0, 1, 0, threadsPerGroup, walkerCmd);
    EXPECT_EQ(2u, iddArg.getBindingTableEntryCount());
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT_BETWEEN_1_AND_4_SAMPLERS_USED, iddArg.getSamplerCount());
}

XE3P_CORETEST_F(CommandEncodeXe3pCoreTest, givenDebugVariableSetwhenProgramingStatePrefetchThenSetCorrectFields) {
    using STATE_PREFETCH = typename FamilyType::STATE_PREFETCH;
    MockExecutionEnvironment mockExecutionEnvironment{};

    uint8_t buffer[sizeof(STATE_PREFETCH) * 4] = {};
    auto statePrefetchCmd = reinterpret_cast<STATE_PREFETCH *>(buffer);

    constexpr uint64_t gpuVa = 0x100000;
    constexpr size_t numCachelines = 3;

    const GraphicsAllocation allocation(0, 1u /*num gmms*/, AllocationType::buffer, nullptr, gpuVa, 0, 4096, MemoryPool::localMemory, MemoryManager::maxOsContextCount);

    auto rootDeviceEnv = mockExecutionEnvironment.rootDeviceEnvironments[0].get();
    auto usage = CacheSettingsHelper::getGmmUsageType(allocation.getAllocationType(), false, rootDeviceEnv->getProductHelper(), rootDeviceEnv->getHardwareInfo());
    uint32_t mocs = rootDeviceEnv->getGmmHelper()->getMOCS(usage);

    static constexpr std::array<uint32_t, 7> expectedSizes = {{
        MemoryConstants::cacheLineSize - 1,
        MemoryConstants::cacheLineSize,
        MemoryConstants::cacheLineSize + 1,
        MemoryConstants::cacheLineSize * numCachelines,
        MemoryConstants::pageSize64k - 1,
        MemoryConstants::pageSize64k,
        (MemoryConstants::pageSize64k * 2) + 1,
    }};

    for (auto expectedSize : expectedSizes) {
        memset(buffer, 0, sizeof(buffer));
        LinearStream linearStream(buffer, sizeof(buffer));

        uint32_t alignedSize = alignUp(expectedSize, MemoryConstants::pageSize64k);
        uint32_t expectedCmdsCount = std::max((alignedSize / static_cast<uint32_t>(MemoryConstants::pageSize64k)), 1u);

        EXPECT_EQ(sizeof(STATE_PREFETCH) * expectedCmdsCount, EncodeMemoryPrefetch<FamilyType>::getSizeForMemoryPrefetch(expectedSize, *mockExecutionEnvironment.rootDeviceEnvironments[0]));

        EncodeMemoryPrefetch<FamilyType>::programMemoryPrefetch(linearStream, allocation, expectedSize, 0, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
        EXPECT_EQ(sizeof(STATE_PREFETCH) * expectedCmdsCount, linearStream.getUsed());

        for (uint32_t i = 0; i < expectedCmdsCount; i++) {
            uint32_t programmedSize = statePrefetchCmd[i].getPrefetchSize() * MemoryConstants::cacheLineSize;

            EXPECT_EQ(statePrefetchCmd[i].getAddress(), gpuVa + (i * MemoryConstants::pageSize64k));
            EXPECT_FALSE(statePrefetchCmd[i].getKernelInstructionPrefetch());
            EXPECT_FALSE(statePrefetchCmd[i].getParserStall());
            EXPECT_EQ(mocs, statePrefetchCmd[i].getMemoryObjectControlState());

            if (programmedSize > expectedSize) {
                // cacheline alignment
                EXPECT_TRUE((programmedSize - expectedSize) < MemoryConstants::cacheLineSize);
                expectedSize = 0;
            } else {
                expectedSize -= programmedSize;
            }
        }
        EXPECT_EQ(0u, expectedSize);
    }
}

XE3P_CORETEST_F(CommandEncodeXe3pCoreTest, givenIsaAllocationWhenProgrammingPrefetchThenSetKernelInstructionPrefetchBit) {
    using STATE_PREFETCH = typename FamilyType::STATE_PREFETCH;
    MockExecutionEnvironment mockExecutionEnvironment{};

    uint8_t buffer[sizeof(STATE_PREFETCH)] = {};
    auto statePrefetchCmd = reinterpret_cast<STATE_PREFETCH *>(buffer);

    EXPECT_EQ(sizeof(STATE_PREFETCH), EncodeMemoryPrefetch<FamilyType>::getSizeForMemoryPrefetch(4096, *mockExecutionEnvironment.rootDeviceEnvironments[0]));

    AllocationType isaTypes[] = {AllocationType::kernelIsa, AllocationType::kernelIsaInternal};

    for (auto &isaType : isaTypes) {
        memset(buffer, 0, sizeof(STATE_PREFETCH));
        LinearStream linearStream(buffer, sizeof(buffer));

        const GraphicsAllocation allocation(0, 1u /*num gmms*/, isaType, nullptr,
                                            1234, 0, 4096, MemoryPool::localMemory, MemoryManager::maxOsContextCount);

        EncodeMemoryPrefetch<FamilyType>::programMemoryPrefetch(linearStream, allocation, 123, 0, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
        EXPECT_EQ(sizeof(STATE_PREFETCH), linearStream.getUsed());

        EXPECT_TRUE(statePrefetchCmd->getKernelInstructionPrefetch());
    }
}

XE3P_CORETEST_F(CommandEncodeXe3pCoreTest, givenDebugFlagSetWhenProgramPrefetchCalledThenDoPrefetchIfSetToOne) {
    using STATE_PREFETCH = typename FamilyType::STATE_PREFETCH;
    DebugManagerStateRestore restore;
    MockExecutionEnvironment mockExecutionEnvironment{};

    uint8_t buffer[sizeof(STATE_PREFETCH)] = {};

    AllocationType isaTypes[] = {AllocationType::kernelIsa, AllocationType::kernelIsaInternal};

    for (auto &isaType : isaTypes) {
        memset(buffer, 0, sizeof(STATE_PREFETCH));
        LinearStream linearStream(buffer, sizeof(buffer));

        const GraphicsAllocation allocation(0, 1u /*num gmms*/, isaType,
                                            nullptr, 1234, 0, 4096, MemoryPool::localMemory, MemoryManager::maxOsContextCount);

        debugManager.flags.EnableMemoryPrefetch.set(0);
        EXPECT_EQ(0u, EncodeMemoryPrefetch<FamilyType>::getSizeForMemoryPrefetch(100, *mockExecutionEnvironment.rootDeviceEnvironments[0]));
        EncodeMemoryPrefetch<FamilyType>::programMemoryPrefetch(linearStream, allocation, 100, 0, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
        EXPECT_EQ(0u, linearStream.getUsed());

        debugManager.flags.EnableMemoryPrefetch.set(1);
        EncodeMemoryPrefetch<FamilyType>::programMemoryPrefetch(linearStream, allocation, 123, 0, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
        EXPECT_EQ(sizeof(STATE_PREFETCH), linearStream.getUsed());
        auto statePrefetchCmd = reinterpret_cast<STATE_PREFETCH *>(buffer);
        EXPECT_TRUE(statePrefetchCmd->getKernelInstructionPrefetch());
    }
}

XE3P_CORETEST_F(CommandEncodeXe3pCoreTest, givenDebugFlagSetWhenProgrammingPrefetchThenSetParserStall) {
    using STATE_PREFETCH = typename FamilyType::STATE_PREFETCH;
    MockExecutionEnvironment mockExecutionEnvironment{};

    DebugManagerStateRestore restore;
    debugManager.flags.ForceCsStallForStatePrefetch.set(1);

    const GraphicsAllocation allocation(0, 1u /*num gmms*/, AllocationType::buffer,
                                        nullptr, 1234, 0, 4096, MemoryPool::localMemory, MemoryManager::maxOsContextCount);
    uint8_t buffer[sizeof(STATE_PREFETCH)] = {};

    LinearStream linearStream(buffer, sizeof(buffer));

    EncodeMemoryPrefetch<FamilyType>::programMemoryPrefetch(linearStream, allocation, 123, 0, *mockExecutionEnvironment.rootDeviceEnvironments[0]);

    auto statePrefetchCmd = reinterpret_cast<STATE_PREFETCH *>(buffer);

    EXPECT_TRUE(statePrefetchCmd->getParserStall());
}

XE3P_CORETEST_F(CommandEncodeXe3pCoreTest, givenOffsetWhenProgrammingStatePrefetchThenSetCorrectGpuVa) {
    using STATE_PREFETCH = typename FamilyType::STATE_PREFETCH;
    MockExecutionEnvironment mockExecutionEnvironment{};

    uint8_t buffer[sizeof(STATE_PREFETCH) * 4] = {};
    auto statePrefetchCmd = reinterpret_cast<STATE_PREFETCH *>(buffer);

    constexpr uint64_t gpuVa = 0x100000;
    constexpr uint32_t gpuVaOffset = 0x10000;

    const GraphicsAllocation allocation(0, 1u /*num gmms*/, AllocationType::buffer, nullptr, gpuVa, 0, 4096, MemoryPool::localMemory, MemoryManager::maxOsContextCount);

    memset(buffer, 0, sizeof(buffer));
    LinearStream linearStream(buffer, sizeof(buffer));

    uint32_t expectedCmdsCount = 3;
    uint32_t alignedSize = MemoryConstants::pageSize64k * expectedCmdsCount;

    EncodeMemoryPrefetch<FamilyType>::programMemoryPrefetch(linearStream, allocation, alignedSize, gpuVaOffset, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
    EXPECT_EQ(sizeof(STATE_PREFETCH) * expectedCmdsCount, linearStream.getUsed());

    for (uint32_t i = 0; i < expectedCmdsCount; i++) {
        uint64_t expectedVa = gpuVa + gpuVaOffset + (i * MemoryConstants::pageSize64k);
        EXPECT_EQ(expectedVa, statePrefetchCmd[i].getAddress());
    }
}

XE3P_CORETEST_F(CommandEncodeXe3pCoreTest, givenLinearStreamWhenSingleBarrierIsProgrammedThenQueueDrainModeIsEnabledByDefaultAndDisabledWithDebugKey) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    uint32_t buffer[2 * sizeof(PIPE_CONTROL)] = {};
    LinearStream linearStream(buffer, sizeof(buffer));

    auto pc = reinterpret_cast<PIPE_CONTROL *>(buffer);

    PipeControlArgs args{};
    NEO::MemorySynchronizationCommands<FamilyType>::addSingleBarrier(linearStream, PostSyncMode::noWrite, 0, 0, args);
    EXPECT_TRUE(pc->getQueueDrainMode());
    linearStream.replaceBuffer(buffer, sizeof(buffer));

    NEO::MemorySynchronizationCommands<FamilyType>::setSingleBarrier(buffer, PostSyncMode::noWrite, 0, 0, args);
    EXPECT_TRUE(pc->getQueueDrainMode());
    linearStream.replaceBuffer(buffer, sizeof(buffer));

    NEO::MemorySynchronizationCommands<FamilyType>::addSingleBarrier(linearStream, args);
    EXPECT_TRUE(pc->getQueueDrainMode());
    linearStream.replaceBuffer(buffer, sizeof(buffer));

    NEO::MemorySynchronizationCommands<FamilyType>::setSingleBarrier(buffer, args);
    EXPECT_TRUE(pc->getQueueDrainMode());
    linearStream.replaceBuffer(buffer, sizeof(buffer));

    DebugManagerStateRestore restore;
    debugManager.flags.PcQueueDrainMode.set(0);

    NEO::MemorySynchronizationCommands<FamilyType>::addSingleBarrier(linearStream, PostSyncMode::noWrite, 0, 0, args);
    EXPECT_FALSE(pc->getQueueDrainMode());
    linearStream.replaceBuffer(buffer, sizeof(buffer));

    NEO::MemorySynchronizationCommands<FamilyType>::setSingleBarrier(buffer, PostSyncMode::noWrite, 0, 0, args);
    EXPECT_FALSE(pc->getQueueDrainMode());
    linearStream.replaceBuffer(buffer, sizeof(buffer));

    NEO::MemorySynchronizationCommands<FamilyType>::addSingleBarrier(linearStream, args);
    EXPECT_FALSE(pc->getQueueDrainMode());
    linearStream.replaceBuffer(buffer, sizeof(buffer));

    NEO::MemorySynchronizationCommands<FamilyType>::setSingleBarrier(buffer, args);
    EXPECT_FALSE(pc->getQueueDrainMode());
    linearStream.replaceBuffer(buffer, sizeof(buffer));
}

using EncodeKernelXe3pCoreTest = Test<CommandEncodeStatesFixture>;

XE3P_CORETEST_F(EncodeKernelXe3pCoreTest, givenScratchRequiredWhenEncodeComputeWalker2ThenInlineDataContainCorrectScratchAddress) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->getCrossThreadDataSizeResult = 256u;
    dispatchInterface->kernelDescriptor.kernelAttributes.perThreadScratchSize[0] = 1024u;
    dispatchInterface->kernelDescriptor.kernelAttributes.flags.passInlineData = true;
    dispatchInterface->kernelDescriptor.payloadMappings.implicitArgs.scratchPointerAddress.offset = 8u;
    dispatchInterface->kernelDescriptor.payloadMappings.implicitArgs.scratchPointerAddress.pointerSize = 8u;

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, false);
    dispatchArgs.isHeaplessModeEnabled = true;
    dispatchArgs.immediateScratchAddressPatching = true;

    auto *csr = dispatchArgs.device->getDefaultEngine().commandStreamReceiver;
    cmdContainer->setImmediateCmdListCsr(csr);

    EncodeDispatchKernel<FamilyType>::template encode<WalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<WalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<WalkerType *>(*itor);
    auto inlineData = reinterpret_cast<uint64_t *>(walkerCmd->getInlineDataPointer());

    auto scratchController = csr->getScratchSpaceController();

    IndirectHeap *ssh = nullptr;
    if (csr->getGlobalStatelessHeapAllocation() != nullptr) {
        ssh = csr->getGlobalStatelessHeap();
    } else {
        ssh = cmdContainer->getIndirectHeap(HeapType::surfaceState);
    }
    auto expectedAddress = scratchController->getScratchPatchAddress() + ssh->getGpuBase();

    auto scratchAddressProgrammed = inlineData[1];

    EXPECT_EQ(expectedAddress, scratchAddressProgrammed);
}

XE3P_CORETEST_F(EncodeKernelXe3pCoreTest, givenUsingCsrHeapAndScratchRequiredWhenEncodeComputeWalker2ThenInlineDataContainCorrectScratchAddress) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->getCrossThreadDataSizeResult = 256u;
    dispatchInterface->kernelDescriptor.kernelAttributes.perThreadScratchSize[0] = 1024u;
    dispatchInterface->kernelDescriptor.kernelAttributes.flags.passInlineData = true;
    dispatchInterface->kernelDescriptor.payloadMappings.implicitArgs.scratchPointerAddress.offset = 8u;
    dispatchInterface->kernelDescriptor.payloadMappings.implicitArgs.scratchPointerAddress.pointerSize = 8u;

    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (ultCsr.globalStatelessHeapAllocation) {
        pDevice->getMemoryManager()->freeGraphicsMemory(ultCsr.globalStatelessHeapAllocation);
        ultCsr.globalStatelessHeapAllocation = nullptr;
    }

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, false);
    dispatchArgs.device->getDefaultEngine().commandStreamReceiver = &ultCsr;
    auto csr = dispatchArgs.device->getDefaultEngine().commandStreamReceiver;
    auto ssh = &csr->getIndirectHeap(NEO::surfaceState, 0);
    dispatchArgs.isHeaplessModeEnabled = true;
    dispatchArgs.immediateScratchAddressPatching = true;
    dispatchArgs.surfaceStateHeap = ssh;

    cmdContainer->setImmediateCmdListCsr(csr);

    EncodeDispatchKernel<FamilyType>::template encode<WalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<WalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<WalkerType *>(*itor);
    auto inlineData = reinterpret_cast<uint64_t *>(walkerCmd->getInlineDataPointer());

    auto scratchController = csr->getScratchSpaceController();
    auto expectedAddress = scratchController->getScratchPatchAddress() + ssh->getGpuBase();

    auto scratchAddressProgrammed = inlineData[1];

    EXPECT_EQ(expectedAddress, scratchAddressProgrammed);
}

XE3P_CORETEST_F(EncodeKernelXe3pCoreTest, givenGlobalStatelessHeapAndScratchRequiredWhenEncodeComputeWalker2ThenInlineDataContainCorrectScratchAddress) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->getCrossThreadDataSizeResult = 256u;
    dispatchInterface->kernelDescriptor.kernelAttributes.perThreadScratchSize[0] = 1024u;
    dispatchInterface->kernelDescriptor.kernelAttributes.flags.passInlineData = true;
    dispatchInterface->kernelDescriptor.payloadMappings.implicitArgs.scratchPointerAddress.offset = 8u;
    dispatchInterface->kernelDescriptor.payloadMappings.implicitArgs.scratchPointerAddress.pointerSize = 8u;

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, false);
    dispatchArgs.device->getDefaultEngine().commandStreamReceiver->createGlobalStatelessHeap();
    auto ssh = dispatchArgs.device->getDefaultEngine().commandStreamReceiver->getGlobalStatelessHeap();
    dispatchArgs.isHeaplessModeEnabled = true;
    dispatchArgs.immediateScratchAddressPatching = true;
    cmdContainer->setImmediateCmdListCsr(dispatchArgs.device->getDefaultEngine().commandStreamReceiver);

    EncodeDispatchKernel<FamilyType>::template encode<WalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<WalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<WalkerType *>(*itor);
    auto inlineData = reinterpret_cast<uint64_t *>(walkerCmd->getInlineDataPointer());

    auto scratchController = dispatchArgs.device->getDefaultEngine().commandStreamReceiver->getScratchSpaceController();
    auto expectedAddress = scratchController->getScratchPatchAddress() + ssh->getGpuBase();

    auto scratchAddressProgrammed = inlineData[1];

    EXPECT_EQ(expectedAddress, scratchAddressProgrammed);
}

XE3P_CORETEST_F(EncodeKernelXe3pCoreTest, givenScratchRequiredPatchingDisabledWhenEncodeComputeWalker2ThenInlineDataContainNullScratchAddress) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->getCrossThreadDataSizeResult = 256u;
    dispatchInterface->kernelDescriptor.kernelAttributes.perThreadScratchSize[0] = 1024u;
    dispatchInterface->kernelDescriptor.kernelAttributes.flags.passInlineData = true;
    dispatchInterface->kernelDescriptor.payloadMappings.implicitArgs.scratchPointerAddress.offset = 8u;
    dispatchInterface->kernelDescriptor.payloadMappings.implicitArgs.scratchPointerAddress.pointerSize = 8u;

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, false);
    dispatchArgs.isHeaplessModeEnabled = true;
    dispatchArgs.immediateScratchAddressPatching = false;

    EncodeDispatchKernel<FamilyType>::template encode<WalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<WalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<WalkerType *>(*itor);
    auto inlineData = reinterpret_cast<uint64_t *>(walkerCmd->getInlineDataPointer());

    uint64_t expectedAddress = 0u;

    auto scratchAddressProgrammed = inlineData[1];

    EXPECT_EQ(expectedAddress, scratchAddressProgrammed);
}

XE3P_CORETEST_F(EncodeKernelXe3pCoreTest, givenHeaplessStateInitAndScratchRequiredWhenEncodeComputeWalker2ThenInlineDataContainCorrectScratchAddress) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    DebugManagerStateRestore restore;
    debugManager.flags.Enable64BitAddressing.set(1);
    debugManager.flags.Enable64bAddressingStateInit.set(1);

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->getCrossThreadDataSizeResult = 256u;
    dispatchInterface->kernelDescriptor.kernelAttributes.perThreadScratchSize[0] = 1024u;
    dispatchInterface->kernelDescriptor.kernelAttributes.flags.passInlineData = true;
    dispatchInterface->kernelDescriptor.payloadMappings.implicitArgs.scratchPointerAddress.offset = 8u;
    dispatchInterface->kernelDescriptor.payloadMappings.implicitArgs.scratchPointerAddress.pointerSize = 8u;

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, false);
    dispatchArgs.isHeaplessModeEnabled = true;
    dispatchArgs.isHeaplessStateInitEnabled = true;
    dispatchArgs.immediateScratchAddressPatching = true;

    auto *csr = dispatchArgs.device->getDefaultEngine().commandStreamReceiver;
    cmdContainer->setImmediateCmdListCsr(csr);

    EncodeDispatchKernel<FamilyType>::template encode<WalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<WalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<WalkerType *>(*itor);
    auto inlineData = reinterpret_cast<uint64_t *>(walkerCmd->getInlineDataPointer());

    auto scratchController = dispatchArgs.device->getDefaultEngine().commandStreamReceiver->getScratchSpaceController();

    IndirectHeap *ssh = nullptr;
    if (csr->getGlobalStatelessHeapAllocation() != nullptr) {
        ssh = csr->getGlobalStatelessHeap();
    } else {
        ssh = cmdContainer->getIndirectHeap(HeapType::surfaceState);
    }

    auto expectedAddress = scratchController->getScratchPatchAddress() + ssh->getGpuBase();

    auto scratchAddressProgrammed = inlineData[1];

    EXPECT_EQ(expectedAddress, scratchAddressProgrammed);
}

XE3P_CORETEST_F(EncodeKernelXe3pCoreTest, givenHeaplessAndBindlessHeapsHelperWhenEncodeKernelWithSamplerThenCorrectSamplerAddressIsPassedToPatch) {

    if (!pDevice->getDeviceInfo().imageSupport) {
        GTEST_SKIP();
    }

    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;

    DebugManagerStateRestore restore;
    debugManager.flags.Enable64BitAddressing.set(1);
    debugManager.flags.Enable64bAddressingStateInit.set(1);
    debugManager.flags.UseBindlessMode.set(1);
    debugManager.flags.UseExternalAllocatorForSshAndDsh.set(1);

    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->createBindlessHeapsHelper(pDevice,
                                                                                                                         pDevice->getNumGenericSubDevices() > 1);

    auto bindlessHeapsHelper = pDevice->getBindlessHeapsHelper();
    ASSERT_NE(nullptr, bindlessHeapsHelper);

    uint32_t numSamplers = 1;
    SAMPLER_STATE samplerState;

    auto dsh = cmdContainer->getIndirectHeap(HeapType::dynamicState);

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    dispatchInterface->kernelDescriptor.payloadMappings.samplerTable.numSamplers = numSamplers;
    dispatchInterface->kernelDescriptor.payloadMappings.samplerTable.borderColor = 0;
    dispatchInterface->kernelDescriptor.payloadMappings.samplerTable.tableOffset = 0;

    unsigned char *samplerStateRaw = reinterpret_cast<unsigned char *>(&samplerState);
    dispatchInterface->getDynamicStateHeapDataResult = samplerStateRaw;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.surfaceStateHeap = cmdContainer->getIndirectHeap(HeapType::surfaceState);
    dispatchArgs.dynamicStateHeap = cmdContainer->getIndirectHeap(HeapType::dynamicState);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    uint64_t expectedSamplerStateOffset = 0u;
    expectedSamplerStateOffset += static_cast<uint32_t>(ptrDiff(dsh->getGpuBase(), bindlessHeapsHelper->getGlobalHeapsBase()));
    expectedSamplerStateOffset += bindlessHeapsHelper->getGlobalHeapsBase();

    auto samplerStateOffset = dispatchInterface->samplerStateOffsetPassed;
    EXPECT_EQ(expectedSamplerStateOffset, samplerStateOffset);
}

XE3P_CORETEST_F(EncodeKernelXe3pCoreTest, givenNoFenceAsPostSyncOperationInComputeWalkerWhenEnqueueKernelIsCalledThenDoNotGenerateFenceCommands) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DebugManagerStateRestore restore;
    debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(0);

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->getCrossThreadDataSizeResult = 0;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
    auto &postSyncData = walkerCmd->getPostSync();
    EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());
}

XE3P_CORETEST_F(EncodeKernelXe3pCoreTest, givenDispatchAllEnabledWhenSubmittingPartitionedWalkerThenConfigureDispAllMode) {
    DebugManagerStateRestore restore;

    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using DISPATCH_ALL_MOD_VALUE = typename DefaultWalkerType::DISPATCH_ALL_MOD_VALUE;

    DefaultWalkerType walkerCmd = FamilyType::cmdInitGpgpuWalker2;

    walkerCmd.setPartitionType(DefaultWalkerType::PARTITION_TYPE::PARTITION_TYPE_X);

    // even size
    walkerCmd.setThreadGroupIdXDimension(100);
    for (bool dispAllEnabed : {true, false}) {
        for (uint32_t tileCount : {1, 2, 4}) {
            walkerCmd.setComputeDispatchAllWalkerEnable(dispAllEnabed);

            void *walkerPtr = &walkerCmd;
            uint32_t bytesProgrammed = 0;
            WalkerPartition::WalkerPartitionArgs args = {};
            args.partitionCount = 2;
            args.tileCount = tileCount;
            args.threadGroupCount = 100;
            WalkerPartition::programPartitionedWalker<FamilyType, DefaultWalkerType>(walkerPtr, bytesProgrammed, &walkerCmd, args, *this->pDevice);

            EXPECT_EQ(DISPATCH_ALL_MOD_VALUE::DISPATCH_ALL_MOD_VALUE_MOD1, walkerCmd.getDispatchAllModValue());
        }
    }

    // uneven size
    walkerCmd.setThreadGroupIdXDimension(819);
    for (bool dispAllEnabed : {true, false}) {
        for (uint32_t tileCount : {1, 2, 4}) {
            walkerCmd.setComputeDispatchAllWalkerEnable(dispAllEnabed);

            void *walkerPtr = &walkerCmd;
            uint32_t bytesProgrammed = 0;
            WalkerPartition::WalkerPartitionArgs args = {};
            args.partitionCount = 2;
            args.tileCount = tileCount;
            args.threadGroupCount = 819;
            WalkerPartition::programPartitionedWalker<FamilyType, DefaultWalkerType>(walkerPtr, bytesProgrammed, &walkerCmd, args, *this->pDevice);

            if (!dispAllEnabed || tileCount == 1) {
                EXPECT_EQ(DISPATCH_ALL_MOD_VALUE::DISPATCH_ALL_MOD_VALUE_MOD1, walkerCmd.getDispatchAllModValue());
            } else if (tileCount == 2) {
                EXPECT_EQ(DISPATCH_ALL_MOD_VALUE::DISPATCH_ALL_MOD_VALUE_MOD2, walkerCmd.getDispatchAllModValue());
            } else {
                EXPECT_EQ(DISPATCH_ALL_MOD_VALUE::DISPATCH_ALL_MOD_VALUE_MOD4, walkerCmd.getDispatchAllModValue());
            }
        }
    }

    auto newValue = DISPATCH_ALL_MOD_VALUE::DISPATCH_ALL_MOD_VALUE_MOD4;
    debugManager.flags.OverrideDispatchAllModValue.set(static_cast<int32_t>(newValue));

    walkerCmd.setComputeDispatchAllWalkerEnable(false);
    void *walkerPtr = &walkerCmd;
    uint32_t bytesProgrammed = 0;
    WalkerPartition::WalkerPartitionArgs args = {};
    args.partitionCount = 2;
    args.tileCount = 1;
    args.threadGroupCount = 819;
    WalkerPartition::programPartitionedWalker<FamilyType, DefaultWalkerType>(walkerPtr, bytesProgrammed, &walkerCmd, args, *this->pDevice);

    EXPECT_EQ(newValue, walkerCmd.getDispatchAllModValue());
}

XE3P_CORETEST_F(EncodeKernelXe3pCoreTest, givenFenceAsPostSyncOperationInComputeWalkerWhenEnqueueKernelIsCalledThenGenerateFenceCommands) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DebugManagerStateRestore restore;
    debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(1);

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->getCrossThreadDataSizeResult = 0;

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, false);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
    auto &postSyncData = walkerCmd->getPostSync();
    EXPECT_TRUE(postSyncData.getSystemMemoryFenceRequest());
}

XE3P_CORETEST_F(EncodeKernelXe3pCoreTest, givenDefaultSettingForFenceAsPostSyncOperationInComputeWalkerWhenEnqueueKernelIsCalledThenGenerateFenceCommands) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DebugManagerStateRestore restore;
    debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(-1);

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->getCrossThreadDataSizeResult = 0;

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, false);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
    auto &postSyncData = walkerCmd->getPostSync();
    EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());
}

XE3P_CORETEST_F(EncodeKernelXe3pCoreTest, givenDefaultSettingForFenceWhenKernelUsesSystemMemoryFlagTrueAndNoHostSignalEventThenNotUseSystemFence) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    DebugManagerStateRestore restore;
    debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(-1);

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->getCrossThreadDataSizeResult = 0;

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, false);
    dispatchArgs.postSyncArgs.isUsingSystemAllocation = true;

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
    auto &postSyncData = walkerCmd->getPostSync();
    EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());
}

XE3P_CORETEST_F(EncodeKernelXe3pCoreTest, givenDefaultSettingForFenceWhenEventHostSignalScopeFlagTrueAndNoSystemMemoryThenNotUseSystemFence) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    DebugManagerStateRestore restore;
    debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(-1);

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->getCrossThreadDataSizeResult = 0;

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, false);
    dispatchArgs.postSyncArgs.isHostScopeSignalEvent = true;

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
    auto &postSyncData = walkerCmd->getPostSync();
    EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());
}

XE3P_CORETEST_F(EncodeKernelXe3pCoreTest, givenDefaultSettingForFenceWhenKernelUsesSystemMemoryAndHostSignalEventFlagTrueThenUseSystemFence) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    DebugManagerStateRestore restore;
    debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(-1);

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->getCrossThreadDataSizeResult = 0;

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, false);
    dispatchArgs.postSyncArgs.isUsingSystemAllocation = true;
    dispatchArgs.postSyncArgs.isHostScopeSignalEvent = true;

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
    auto &postSyncData = walkerCmd->getPostSync();
    EXPECT_EQ(postSyncData.getSystemMemoryFenceRequest(), !pDevice->getHardwareInfo().capabilityTable.isIntegratedDevice);
}

XE3P_CORETEST_F(EncodeKernelXe3pCoreTest, givenDebugFlagSetWhenSetPropertiesAllCalledThenDisablePipelinedThreadArbitrationPolicy) {
    DebugManagerStateRestore restore;

    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];

    {
        StreamProperties streamProperties{};
        streamProperties.initSupport(rootDeviceEnvironment);

        streamProperties.stateComputeMode.setPropertiesAll(false, 0, 0, PreemptionMode::Disabled, false);
        EXPECT_TRUE(streamProperties.stateComputeMode.isPipelinedEuThreadArbitrationEnabled());
        EXPECT_TRUE(streamProperties.stateComputeMode.pipelinedEuThreadArbitration.isDirty);

        streamProperties.stateComputeMode.setPropertiesAll(false, 1, 1, PreemptionMode::Disabled, false);
        EXPECT_FALSE(streamProperties.stateComputeMode.pipelinedEuThreadArbitration.isDirty);
    }

    {
        debugManager.flags.PipelinedEuThreadArbitration.set(0);

        StreamProperties streamProperties{};
        streamProperties.initSupport(rootDeviceEnvironment);

        streamProperties.stateComputeMode.setPropertiesAll(false, 0, 0, PreemptionMode::Disabled, false);
        EXPECT_FALSE(streamProperties.stateComputeMode.isPipelinedEuThreadArbitrationEnabled());
        EXPECT_FALSE(streamProperties.stateComputeMode.pipelinedEuThreadArbitration.isDirty);
    }
}

XE3P_CORETEST_F(EncodeKernelXe3pCoreTest, givenDebugFlagWhenProgrammingStateComputeModeThenEnableVrtFieldIsCorrectlySet) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    DebugManagerStateRestore restore;

    uint8_t buffer[sizeof(STATE_COMPUTE_MODE)]{};
    MockExecutionEnvironment executionEnvironment{};
    const auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];

    {
        // default
        LinearStream linearStream(buffer, sizeof(buffer));

        StreamProperties streamProperties{};
        streamProperties.initSupport(rootDeviceEnvironment);
        streamProperties.stateComputeMode.setPropertiesAll(false, 0, 0, PreemptionMode::Disabled, false);
        EncodeComputeMode<FamilyType>::programComputeModeCommand(linearStream, streamProperties.stateComputeMode, rootDeviceEnvironment);

        auto &stateComputeModeCmd = *reinterpret_cast<STATE_COMPUTE_MODE *>(linearStream.getCpuBase());
        EXPECT_TRUE(stateComputeModeCmd.getEnableVariableRegisterSizeAllocationVrt());
    }

    {
        // enabled
        debugManager.flags.EnableXe3VariableRegisterSizeAllocation.set(1);

        LinearStream linearStream(buffer, sizeof(buffer));

        StreamProperties streamProperties{};
        streamProperties.initSupport(rootDeviceEnvironment);
        streamProperties.stateComputeMode.setPropertiesAll(false, 0, 0, PreemptionMode::Disabled, false);
        EncodeComputeMode<FamilyType>::programComputeModeCommand(linearStream, streamProperties.stateComputeMode, rootDeviceEnvironment);

        auto &stateComputeModeCmd = *reinterpret_cast<STATE_COMPUTE_MODE *>(linearStream.getCpuBase());
        EXPECT_TRUE(stateComputeModeCmd.getEnableVariableRegisterSizeAllocationVrt());
    }

    {
        // disabled
        debugManager.flags.EnableXe3VariableRegisterSizeAllocation.set(0);

        LinearStream linearStream(buffer, sizeof(buffer));

        StreamProperties streamProperties{};
        streamProperties.initSupport(rootDeviceEnvironment);
        streamProperties.stateComputeMode.setPropertiesAll(false, 0, 0, PreemptionMode::Disabled, false);
        EncodeComputeMode<FamilyType>::programComputeModeCommand(linearStream, streamProperties.stateComputeMode, rootDeviceEnvironment);

        auto &stateComputeModeCmd = *reinterpret_cast<STATE_COMPUTE_MODE *>(linearStream.getCpuBase());
        EXPECT_FALSE(stateComputeModeCmd.getEnableVariableRegisterSizeAllocationVrt());
    }
}

XE3P_CORETEST_F(EncodeKernelXe3pCoreTest, givenDebugFlagWhenProgrammingStateComputeModeThenEnableOOBTranslationExceptionFieldIsCorrectlySet) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    DebugManagerStateRestore dbgStateRestore;

    uint8_t buffer[sizeof(STATE_COMPUTE_MODE)]{};
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    const auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();

    // without debugger
    {
        LinearStream linearStream(buffer, sizeof(buffer));
        StreamProperties streamProperties{};
        streamProperties.initSupport(rootDeviceEnvironment);
        streamProperties.stateComputeMode.setPropertiesAll(false, 0, 0, PreemptionMode::Disabled, false);
        EncodeComputeMode<FamilyType>::programComputeModeCommand(linearStream, streamProperties.stateComputeMode, rootDeviceEnvironment);

        const auto &stateComputeModeCmd = *reinterpret_cast<STATE_COMPUTE_MODE *>(linearStream.getCpuBase());
        EXPECT_FALSE(stateComputeModeCmd.getOutOfBoundariesInTranslationExceptionEnable());
    }

    // with debugger
    rootDeviceEnvironment.debugger = DebuggerL0::create(this->pDevice);

    for (int32_t debugFlagValue : {0, 1}) {
        debugManager.flags.TemporaryEnableOutOfBoundariesInTranslationException.set(debugFlagValue);

        LinearStream linearStream(buffer, sizeof(buffer));
        StreamProperties streamProperties{};
        streamProperties.initSupport(rootDeviceEnvironment);
        streamProperties.stateComputeMode.setPropertiesAll(false, 0, 0, PreemptionMode::Disabled, false);
        EncodeComputeMode<FamilyType>::programComputeModeCommand(linearStream, streamProperties.stateComputeMode, rootDeviceEnvironment);

        const auto &stateComputeModeCmd = *reinterpret_cast<STATE_COMPUTE_MODE *>(linearStream.getCpuBase());
        auto expectedValue = (productHelper.isTranslationExceptionSupported() && debugManager.flags.TemporaryEnableOutOfBoundariesInTranslationException.get());
        EXPECT_EQ(expectedValue, stateComputeModeCmd.getOutOfBoundariesInTranslationExceptionEnable());

        if (debugFlagValue && productHelper.isTranslationExceptionSupported()) {
            EXPECT_EQ(FamilyType::stateComputeModeEnableOutOfBoundariesInTranslationExceptionMask, stateComputeModeCmd.getMask2() & FamilyType::stateComputeModeEnableOutOfBoundariesInTranslationExceptionMask);
        } else {
            EXPECT_EQ(0u, stateComputeModeCmd.getMask2() & FamilyType::stateComputeModeEnableOutOfBoundariesInTranslationExceptionMask);
        }
    }
}

XE3P_CORETEST_F(EncodeKernelXe3pCoreTest, givenNoDebugFlagWhenProgrammingStateComputeModeThenEnableOOBTranslationExceptionFieldIsNotSet) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    uint8_t buffer[sizeof(STATE_COMPUTE_MODE)]{};
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];

    LinearStream linearStream(buffer, sizeof(buffer));
    StreamProperties streamProperties{};
    streamProperties.initSupport(rootDeviceEnvironment);
    streamProperties.stateComputeMode.setPropertiesAll(false, 0, 0, PreemptionMode::Disabled, false);
    EncodeComputeMode<FamilyType>::programComputeModeCommand(linearStream, streamProperties.stateComputeMode, rootDeviceEnvironment);

    const auto &stateComputeModeCmd = *reinterpret_cast<STATE_COMPUTE_MODE *>(linearStream.getCpuBase());
    EXPECT_FALSE(stateComputeModeCmd.getOutOfBoundariesInTranslationExceptionEnable());
}

XE3P_CORETEST_F(EncodeKernelXe3pCoreTest, givenDebugFlagWhenProgrammingStateComputeModeThenEnableSystemMemoryReadFenceFieldIsCorrectlySet) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    DebugManagerStateRestore restore;

    uint8_t buffer[sizeof(STATE_COMPUTE_MODE)]{};
    MockExecutionEnvironment executionEnvironment{};
    const auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];

    {
        // default
        LinearStream linearStream(buffer, sizeof(buffer));

        StreamProperties streamProperties{};
        streamProperties.initSupport(rootDeviceEnvironment);
        streamProperties.stateComputeMode.setPropertiesAll(false, 0, 0, PreemptionMode::Disabled, false);
        EncodeComputeMode<FamilyType>::programComputeModeCommand(linearStream, streamProperties.stateComputeMode, rootDeviceEnvironment);

        auto &stateComputeModeCmd = *reinterpret_cast<STATE_COMPUTE_MODE *>(linearStream.getCpuBase());
        EXPECT_FALSE(stateComputeModeCmd.getSystemMemoryReadFenceEnable());
    }

    {
        // enabled
        debugManager.flags.EnableSystemMemoryReadFence.set(1);

        LinearStream linearStream(buffer, sizeof(buffer));

        StreamProperties streamProperties{};
        streamProperties.initSupport(rootDeviceEnvironment);
        streamProperties.stateComputeMode.setPropertiesAll(false, 0, 0, PreemptionMode::Disabled, false);
        EncodeComputeMode<FamilyType>::programComputeModeCommand(linearStream, streamProperties.stateComputeMode, rootDeviceEnvironment);

        auto &stateComputeModeCmd = *reinterpret_cast<STATE_COMPUTE_MODE *>(linearStream.getCpuBase());
        EXPECT_TRUE(stateComputeModeCmd.getSystemMemoryReadFenceEnable());
    }

    {
        // disabled
        debugManager.flags.EnableSystemMemoryReadFence.set(0);

        LinearStream linearStream(buffer, sizeof(buffer));

        StreamProperties streamProperties{};
        streamProperties.initSupport(rootDeviceEnvironment);
        streamProperties.stateComputeMode.setPropertiesAll(false, 0, 0, PreemptionMode::Disabled, true);
        EncodeComputeMode<FamilyType>::programComputeModeCommand(linearStream, streamProperties.stateComputeMode, rootDeviceEnvironment);

        auto &stateComputeModeCmd = *reinterpret_cast<STATE_COMPUTE_MODE *>(linearStream.getCpuBase());
        EXPECT_FALSE(stateComputeModeCmd.getSystemMemoryReadFenceEnable());
    }
}

XE3P_CORETEST_F(EncodeKernelXe3pCoreTest, givenDebugFlagWhenProgrammingStateComputeModeThenEnableExceptionFieldIsCorrectlySet) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    DebugManagerStateRestore dbgStateRestore;

    uint8_t buffer[sizeof(STATE_COMPUTE_MODE)]{};
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];

    // without debugger
    {
        LinearStream linearStream(buffer, sizeof(buffer));
        StreamProperties streamProperties{};
        streamProperties.initSupport(rootDeviceEnvironment);
        streamProperties.stateComputeMode.setPropertiesAll(false, 0, 0, PreemptionMode::Disabled, false);
        EncodeComputeMode<FamilyType>::programComputeModeCommand(linearStream, streamProperties.stateComputeMode, rootDeviceEnvironment);

        const auto &stateComputeModeCmd = *reinterpret_cast<STATE_COMPUTE_MODE *>(linearStream.getCpuBase());
        EXPECT_FALSE(stateComputeModeCmd.getPageFaultExceptionEnable());
        EXPECT_FALSE(stateComputeModeCmd.getMemoryExceptionEnable());
        EXPECT_FALSE(stateComputeModeCmd.getEnableBreakpoints());
        EXPECT_FALSE(stateComputeModeCmd.getEnableForceExternalHaltAndForceException());
    }

    // with debugger
    rootDeviceEnvironment.debugger = DebuggerL0::create(this->pDevice);

    for (int32_t debugFlagValue : ::testing::Bool()) {
        debugManager.flags.TemporaryEnablePageFaultException.set(debugFlagValue);

        LinearStream linearStream(buffer, sizeof(buffer));
        StreamProperties streamProperties{};
        streamProperties.initSupport(rootDeviceEnvironment);
        streamProperties.stateComputeMode.setPropertiesAll(false, 0, 0, PreemptionMode::Disabled, false);
        EncodeComputeMode<FamilyType>::programComputeModeCommand(linearStream, streamProperties.stateComputeMode, rootDeviceEnvironment);

        const auto &stateComputeModeCmd = *reinterpret_cast<STATE_COMPUTE_MODE *>(linearStream.getCpuBase());
        EXPECT_EQ(debugFlagValue, stateComputeModeCmd.getPageFaultExceptionEnable());
        EXPECT_TRUE(stateComputeModeCmd.getMemoryExceptionEnable());
        EXPECT_TRUE(stateComputeModeCmd.getEnableBreakpoints());
        EXPECT_TRUE(stateComputeModeCmd.getEnableForceExternalHaltAndForceException());
    }
}

XE3P_CORETEST_F(EncodeKernelXe3pCoreTest, givenNoDebugFlagWhenProgrammingStateComputeModeThenEnablePageFaultExceptionFieldIsNotSet) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    uint8_t buffer[sizeof(STATE_COMPUTE_MODE)]{};
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];

    LinearStream linearStream(buffer, sizeof(buffer));
    StreamProperties streamProperties{};
    streamProperties.initSupport(rootDeviceEnvironment);
    streamProperties.stateComputeMode.setPropertiesAll(false, 0, 0, PreemptionMode::Disabled, false);
    EncodeComputeMode<FamilyType>::programComputeModeCommand(linearStream, streamProperties.stateComputeMode, rootDeviceEnvironment);

    const auto &stateComputeModeCmd = *reinterpret_cast<STATE_COMPUTE_MODE *>(linearStream.getCpuBase());
    EXPECT_FALSE(stateComputeModeCmd.getPageFaultExceptionEnable());
}

XE3P_CORETEST_F(EncodeKernelXe3pCoreTest, givenSystolicModeRequiredWhenEncodeIsCalledThenDontReprogramPipelineSelect) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    bool requiresUncachedMocs = false;
    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->kernelDescriptor.kernelAttributes.flags.usesSystolicPipelineSelectMode = true;
    cmdContainer->lastPipelineSelectModeRequiredRef() = false;

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.preemptionMode = PreemptionMode::Initial;

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);
    EXPECT_FALSE(cmdContainer->lastPipelineSelectModeRequiredRef());
}

XE3P_CORETEST_F(EncodeKernelXe3pCoreTest, givenXe3pThenPipelineSelectIsNotProgrammed) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    EncodeComputeMode<FamilyType>::adjustPipelineSelect(*cmdContainer.get(), dispatchInterface->kernelDescriptor);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<PIPELINE_SELECT *>(commands.begin(), commands.end());
    EXPECT_EQ(itor, commands.end());
}

XE3P_CORETEST_F(EncodeKernelXe3pCoreTest, givenRequiredWorkGroupOrderWhenCallAdjustWalkOrderThenDispatchWalkOrderIsProgrammedCorrectly) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];

    DefaultWalkerType walkerCmd{};
    uint32_t yOrder = 2u;
    EXPECT_EQ(HwWalkOrderHelper::compatibleDimensionOrders[yOrder], HwWalkOrderHelper::yOrderWalk);

    EncodeDispatchKernel<FamilyType>::adjustWalkOrder(walkerCmd, yOrder, rootDeviceEnvironment);
    EXPECT_EQ(DefaultWalkerType::DISPATCH_WALK_ORDER::DISPATCH_WALK_ORDER_Y_ORDER_WALK, walkerCmd.getDispatchWalkOrder());

    uint32_t linearOrder = 0u;
    EXPECT_EQ(HwWalkOrderHelper::compatibleDimensionOrders[linearOrder], HwWalkOrderHelper::linearWalk);

    EncodeDispatchKernel<FamilyType>::adjustWalkOrder(walkerCmd, linearOrder, rootDeviceEnvironment);
    EXPECT_EQ(DefaultWalkerType::DISPATCH_WALK_ORDER::DISPATCH_WALK_ORDER_LINEAR_WALK, walkerCmd.getDispatchWalkOrder());

    auto currentDispatchWalkOrder = walkerCmd.getDispatchWalkOrder();
    uint32_t fakeOrder = 5u;
    EncodeDispatchKernel<FamilyType>::adjustWalkOrder(walkerCmd, fakeOrder, rootDeviceEnvironment);
    EXPECT_EQ(currentDispatchWalkOrder, walkerCmd.getDispatchWalkOrder()); // no change
}

XE3P_CORETEST_F(EncodeKernelXe3pCoreTest, givenRequiredWorkGroupOrderWhenCallEncodeThreadDataThenDispatchWalkOrderIsProgrammedCorrectly) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DefaultWalkerType walkerCmd = FamilyType::template getInitGpuWalker<DefaultWalkerType>();

    uint32_t startWorkGroup[3] = {1, 1, 1};
    uint32_t numWorkGroups[3] = {1, 1, 1};
    uint32_t workGroupSizes[3] = {1, 1, 1};

    uint32_t yOrder = 2u;
    EXPECT_EQ(HwWalkOrderHelper::compatibleDimensionOrders[yOrder], HwWalkOrderHelper::yOrderWalk);
    auto &rootDeviceEnvironment = this->pDevice->getRootDeviceEnvironment();
    EncodeDispatchKernel<FamilyType>::encodeThreadData(walkerCmd, startWorkGroup, numWorkGroups, workGroupSizes, 0, 3,
                                                       0, 1, false, false, true, yOrder, rootDeviceEnvironment);
    EXPECT_EQ(DefaultWalkerType::DISPATCH_WALK_ORDER::DISPATCH_WALK_ORDER_Y_ORDER_WALK, walkerCmd.getDispatchWalkOrder());

    EncodeDispatchKernel<FamilyType>::encodeThreadData(walkerCmd, startWorkGroup, numWorkGroups, workGroupSizes, 0, 3,
                                                       0, 1, true, false, true, yOrder, rootDeviceEnvironment);
    EXPECT_EQ(DefaultWalkerType::DISPATCH_WALK_ORDER::DISPATCH_WALK_ORDER_Y_ORDER_WALK, walkerCmd.getDispatchWalkOrder());

    uint32_t linearOrder = 0u;
    EXPECT_EQ(HwWalkOrderHelper::compatibleDimensionOrders[linearOrder], HwWalkOrderHelper::linearWalk);
    EncodeDispatchKernel<FamilyType>::encodeThreadData(walkerCmd, startWorkGroup, numWorkGroups, workGroupSizes, 0, 3,
                                                       0, 1, false, false, true, linearOrder, rootDeviceEnvironment);
    EXPECT_EQ(DefaultWalkerType::DISPATCH_WALK_ORDER::DISPATCH_WALK_ORDER_LINEAR_WALK, walkerCmd.getDispatchWalkOrder());

    EncodeDispatchKernel<FamilyType>::encodeThreadData(walkerCmd, startWorkGroup, numWorkGroups, workGroupSizes, 0, 3,
                                                       0, 1, true, false, true, linearOrder, rootDeviceEnvironment);
    EXPECT_EQ(DefaultWalkerType::DISPATCH_WALK_ORDER::DISPATCH_WALK_ORDER_LINEAR_WALK, walkerCmd.getDispatchWalkOrder());
}

XE3P_CORETEST_F(EncodeKernelXe3pCoreTest, givenCommandContainerWhenNumGrfRequiredIsGreaterThanDefaultThenLargeGrfModeEnabled) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    DebugManagerStateRestore restore;
    debugManager.flags.EnableXe3VariableRegisterSizeAllocation.set(0);

    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    StreamProperties streamProperties{};
    streamProperties.initSupport(rootDeviceEnvironment);
    streamProperties.stateComputeMode.setPropertiesAll(false, GrfConfig::largeGrfNumber, 0u, PreemptionMode::Disabled, false);
    EncodeComputeMode<FamilyType>::programComputeModeCommand(*cmdContainer->getCommandStream(), streamProperties.stateComputeMode, rootDeviceEnvironment);
    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itorCmd = find<STATE_COMPUTE_MODE *>(commands.begin(), commands.end());
    ASSERT_NE(itorCmd, commands.end());

    auto cmd = genCmdCast<STATE_COMPUTE_MODE *>(*itorCmd);
    EXPECT_EQ(productHelper.isGrfNumReportedWithScm(), cmd->getLargeGrfMode());
}

using EncodeKernelScratchProgrammingXe3pCoreTest = Test<ScratchProgrammingFixture>;

XE3P_CORETEST_F(EncodeKernelScratchProgrammingXe3pCoreTest, givenHeaplessModeDisabledWhenSetScratchAddressIsCalledThenDoNothing) {

    static constexpr bool heaplessModeEnabled = false;
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    uint64_t scratchAddress = 0;
    uint32_t requiredScratchSlot0Size = 64;
    uint32_t requiredScratchSlot1Size = 0;

    EncodeDispatchKernel<FamilyType>::template setScratchAddress<heaplessModeEnabled>(scratchAddress, requiredScratchSlot0Size, requiredScratchSlot1Size, ssh, ultCsr);

    uint64_t expectedScratchAddress = 0;
    EXPECT_EQ(expectedScratchAddress, scratchAddress);
}

XE3P_CORETEST_F(EncodeKernelScratchProgrammingXe3pCoreTest, givenScratchSizeZeroWhenSetScratchAddressIsCalledThenDoNothing) {

    static constexpr bool heaplessModeEnabled = true;
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    uint64_t scratchAddress = 0;
    uint32_t requiredScratchSlot0Size = 0;
    uint32_t requiredScratchSlot1Size = 0;

    EncodeDispatchKernel<FamilyType>::template setScratchAddress<heaplessModeEnabled>(scratchAddress, requiredScratchSlot0Size, requiredScratchSlot1Size, ssh, ultCsr);

    uint64_t expectedScratchAddress = 0;
    EXPECT_EQ(expectedScratchAddress, scratchAddress);
}

XE3P_CORETEST_F(EncodeKernelScratchProgrammingXe3pCoreTest, givenScratchSlot0SizeWhenSetScratchAddressIsCalledThenScratchAddressIsCorrect) {

    static constexpr bool heaplessModeEnabled = true;
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    uint64_t scratchAddress = 0u;
    uint32_t requiredScratchSlot0Size = 64u;
    uint32_t requiredScratchSlot1Size = 0u;
    auto scratchController = ultCsr.getScratchSpaceController();

    EncodeDispatchKernel<FamilyType>::template setScratchAddress<heaplessModeEnabled>(scratchAddress, requiredScratchSlot0Size, requiredScratchSlot1Size, ssh, ultCsr);

    uint64_t expectedScratchAddress = ssh->getGpuBase() + scratchController->getScratchPatchAddress();

    EXPECT_EQ(expectedScratchAddress, scratchAddress);
}

XE3P_CORETEST_F(EncodeKernelScratchProgrammingXe3pCoreTest, givenScratchSlot1SizeWhenSetScratchAddressIsCalledThenScratchAddressIsCorrect) {

    static constexpr bool heaplessModeEnabled = true;
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    uint64_t scratchAddress = 0u;
    uint32_t requiredScratchSlot0Size = 0u;
    uint32_t requiredScratchSlot1Size = 64u;
    auto scratchController = ultCsr.getScratchSpaceController();

    EncodeDispatchKernel<FamilyType>::template setScratchAddress<heaplessModeEnabled>(scratchAddress, requiredScratchSlot0Size, requiredScratchSlot1Size, ssh, ultCsr);

    uint64_t expectedScratchAddress = ssh->getGpuBase() + scratchController->getScratchPatchAddress();

    EXPECT_EQ(expectedScratchAddress, scratchAddress);
}

XE3P_CORETEST_F(EncodeKernelScratchProgrammingXe3pCoreTest, givenTheSameScratchSizeWhenSetScratchAddressIsCalledTwiceThenScratchAllocationIsReused) {
    static constexpr bool heaplessModeEnabled = true;
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    uint64_t scratchAddress = 0u;
    uint32_t requiredScratchSlot0Size = 64u;
    uint32_t requiredScratchSlot1Size = 0u;
    auto scratchController = ultCsr.getScratchSpaceController();

    EncodeDispatchKernel<FamilyType>::template setScratchAddress<heaplessModeEnabled>(scratchAddress, requiredScratchSlot0Size, requiredScratchSlot1Size, ssh, ultCsr);

    uint64_t expectedScratchAddress = ssh->getGpuBase() + scratchController->getScratchPatchAddress();
    EXPECT_EQ(expectedScratchAddress, scratchAddress);

    uint32_t requiredScratchSlot0Size2nd = requiredScratchSlot0Size;
    uint64_t scratchAddress2nd = 0u;
    EncodeDispatchKernel<FamilyType>::template setScratchAddress<heaplessModeEnabled>(scratchAddress2nd, requiredScratchSlot0Size2nd, requiredScratchSlot1Size, ssh, ultCsr);
    EXPECT_EQ(scratchAddress, scratchAddress2nd);
}

XE3P_CORETEST_F(EncodeKernelScratchProgrammingXe3pCoreTest, givenBiggerScratchSizeWhenSetScratchAddressIsCalledTwiceThenScratchAllocationIsNotReused) {
    static constexpr bool heaplessModeEnabled = true;
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    uint64_t scratchAddress = 0u;
    uint32_t requiredScratchSlot0Size = 64u;
    uint32_t requiredScratchSlot1Size = 0u;
    auto scratchController = ultCsr.getScratchSpaceController();

    EncodeDispatchKernel<FamilyType>::template setScratchAddress<heaplessModeEnabled>(scratchAddress, requiredScratchSlot0Size, requiredScratchSlot1Size, ssh, ultCsr);

    uint64_t expectedScratchAddress = ssh->getGpuBase() + scratchController->getScratchPatchAddress();
    EXPECT_EQ(expectedScratchAddress, scratchAddress);

    uint32_t requiredScratchSlot0Size2nd = 128u;
    ASSERT_GT(requiredScratchSlot0Size2nd, requiredScratchSlot0Size);
    uint64_t scratchAddress2nd = 0u;
    EncodeDispatchKernel<FamilyType>::template setScratchAddress<heaplessModeEnabled>(scratchAddress2nd, requiredScratchSlot0Size2nd, requiredScratchSlot1Size, ssh, ultCsr);

    EXPECT_NE(scratchAddress, scratchAddress2nd);

    expectedScratchAddress = ssh->getGpuBase() + scratchController->getScratchPatchAddress();
    EXPECT_EQ(expectedScratchAddress, scratchAddress2nd);
}

XE3P_CORETEST_F(EncodeKernelScratchProgrammingXe3pCoreTest, givenSmallerScratchSizeWhenSetScratchAddressIsCalledTwiceThenScratchAllocationIsReused) {
    static constexpr bool heaplessModeEnabled = true;
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    uint64_t scratchAddress = 0u;
    uint32_t requiredScratchSlot0Size = 64u;
    uint32_t requiredScratchSlot1Size = 0u;
    auto scratchController = ultCsr.getScratchSpaceController();

    EncodeDispatchKernel<FamilyType>::template setScratchAddress<heaplessModeEnabled>(scratchAddress, requiredScratchSlot0Size, requiredScratchSlot1Size, ssh, ultCsr);

    uint64_t expectedScratchAddress = ssh->getGpuBase() + scratchController->getScratchPatchAddress();
    EXPECT_EQ(expectedScratchAddress, scratchAddress);

    uint32_t requiredScratchSlot0Size2nd = 32u;
    ASSERT_LT(requiredScratchSlot0Size2nd, requiredScratchSlot0Size);

    uint64_t scratchAddress2nd = 0u;
    EncodeDispatchKernel<FamilyType>::template setScratchAddress<heaplessModeEnabled>(scratchAddress2nd, requiredScratchSlot0Size2nd, requiredScratchSlot1Size, ssh, ultCsr);
    EXPECT_EQ(scratchAddress, scratchAddress2nd);
}

XE3P_CORETEST_F(EncodeKernelScratchProgrammingXe3pCoreTest, givenPrimaryCsrWhenSetScratchAddressThenLockCalledOnPrimaryCsr) {
    static constexpr bool heaplessModeEnabled = true;
    auto &primaryCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    MockCommandStreamReceiver submissionCsr(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    uint64_t scratchAddress = 0u;
    uint32_t requiredScratchSlot0Size = 64u;
    uint32_t requiredScratchSlot1Size = 0u;
    auto scratchController = primaryCsr.getScratchSpaceController();

    submissionCsr.primaryCsr = &primaryCsr;
    auto lockCounterPrimaryCsr = primaryCsr.recursiveLockCounter.load();
    auto nSubmissionCsrMakeResidentCalled = submissionCsr.makeResidentCalledTimes;
    auto nPrimaryCsrMakeResidentCalled = primaryCsr.makeResidentCalledTimes;
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    submissionCsr.osContext = &osContext;

    EncodeDispatchKernel<FamilyType>::template setScratchAddress<heaplessModeEnabled>(scratchAddress, requiredScratchSlot0Size, requiredScratchSlot1Size, ssh, submissionCsr);

    EXPECT_EQ(nSubmissionCsrMakeResidentCalled + 1, submissionCsr.makeResidentCalledTimes);
    EXPECT_EQ(nPrimaryCsrMakeResidentCalled, primaryCsr.makeResidentCalledTimes);
    EXPECT_EQ(lockCounterPrimaryCsr + 1, primaryCsr.recursiveLockCounter);

    uint64_t expectedScratchAddress = ssh->getGpuBase() + scratchController->getScratchPatchAddress();
    EXPECT_EQ(expectedScratchAddress, scratchAddress);

    uint32_t requiredScratchSlot0Size2nd = 32u;
    ASSERT_LT(requiredScratchSlot0Size2nd, requiredScratchSlot0Size);
}

XE3P_CORETEST_F(EncodeKernelScratchProgrammingXe3pCoreTest, givenNoPrimaryCsrWhenSetScratchAddressThenLockCsrNotCalled) {
    static constexpr bool heaplessModeEnabled = true;
    auto &submissionCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    uint64_t scratchAddress = 0u;
    uint32_t requiredScratchSlot0Size = 64u;
    uint32_t requiredScratchSlot1Size = 0u;
    auto scratchController = submissionCsr.getScratchSpaceController();

    auto lockCounterPrimaryCsr = submissionCsr.recursiveLockCounter.load();
    auto nSubmissionCsrMakeResidentCalled = submissionCsr.makeResidentCalledTimes;

    EncodeDispatchKernel<FamilyType>::template setScratchAddress<heaplessModeEnabled>(scratchAddress, requiredScratchSlot0Size, requiredScratchSlot1Size, ssh, submissionCsr);
    EXPECT_EQ(nSubmissionCsrMakeResidentCalled + 1, submissionCsr.makeResidentCalledTimes);
    EXPECT_EQ(lockCounterPrimaryCsr, submissionCsr.recursiveLockCounter);

    uint64_t expectedScratchAddress = ssh->getGpuBase() + scratchController->getScratchPatchAddress();
    EXPECT_EQ(expectedScratchAddress, scratchAddress);

    uint32_t requiredScratchSlot0Size2nd = 32u;
    ASSERT_LT(requiredScratchSlot0Size2nd, requiredScratchSlot0Size);
}

XE3P_CORETEST_F(EncodeKernelScratchProgrammingXe3pCoreTest, givenPrimaryCsrAsSubmissionCsrWhenSetScratchAddressThenLockCsrNotCalled) {
    static constexpr bool heaplessModeEnabled = true;
    auto &submissionCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    uint64_t scratchAddress = 0u;
    uint32_t requiredScratchSlot0Size = 64u;
    uint32_t requiredScratchSlot1Size = 0u;
    auto scratchController = submissionCsr.getScratchSpaceController();

    submissionCsr.primaryCsr = &submissionCsr;
    auto lockCounterPrimaryCsr = submissionCsr.recursiveLockCounter.load();
    auto nSubmissionCsrMakeResidentCalled = submissionCsr.makeResidentCalledTimes;

    EncodeDispatchKernel<FamilyType>::template setScratchAddress<heaplessModeEnabled>(scratchAddress, requiredScratchSlot0Size, requiredScratchSlot1Size, ssh, submissionCsr);
    EXPECT_EQ(nSubmissionCsrMakeResidentCalled + 1, submissionCsr.makeResidentCalledTimes);
    EXPECT_EQ(lockCounterPrimaryCsr, submissionCsr.recursiveLockCounter);

    uint64_t expectedScratchAddress = ssh->getGpuBase() + scratchController->getScratchPatchAddress();
    EXPECT_EQ(expectedScratchAddress, scratchAddress);

    uint32_t requiredScratchSlot0Size2nd = 32u;
    ASSERT_LT(requiredScratchSlot0Size2nd, requiredScratchSlot0Size);
}

using Xe3pSbaTest = SbaTest;

XE3P_CORETEST_F(Xe3pSbaTest, givenSpecificProductFamilyWhenAppendingSbaThenProgramWtL1CachePolicy) {
    auto sbaCmd = FamilyType::cmdInitStateBaseAddress;

    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(&sbaCmd, pDevice->getRootDeviceEnvironment().getGmmHelper(), &ssh, nullptr, nullptr);
    args.setGeneralStateBaseAddress = true;

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args);

    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WBP, sbaCmd.getL1CacheControlCachePolicy());
}

XE3P_CORETEST_F(Xe3pSbaTest, givenL1CachingOverrideWhenStateBaseAddressIsProgrammedThenItMatchesTheOverrideValue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceStatelessL1CachingPolicy.set(0u);

    auto sbaCmd = FamilyType::cmdInitStateBaseAddress;

    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(&sbaCmd, pDevice->getRootDeviceEnvironment().getGmmHelper(), &ssh, nullptr, nullptr);
    args.setGeneralStateBaseAddress = true;

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args);

    EXPECT_EQ(0u, sbaCmd.getL1CacheControlCachePolicy());

    debugManager.flags.ForceStatelessL1CachingPolicy.set(1u);

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args);

    EXPECT_EQ(1u, sbaCmd.getL1CacheControlCachePolicy());
}

XE3P_CORETEST_F(Xe3pSbaTest, givenOverrideSamplerArbitrationControlWhenStateBaseAddressIsProgrammedThenItMatchesTheOverrideValue) {
    DebugManagerStateRestore restorer;
    auto sbaCmd = FamilyType::cmdInitStateBaseAddress;
    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(&sbaCmd, pDevice->getRootDeviceEnvironment().getGmmHelper(), &ssh, nullptr, nullptr);

    debugManager.flags.OverrideSamplerArbitrationControl.set(0u);
    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args);
    EXPECT_EQ(static_cast<FamilyType::STATE_BASE_ADDRESS::SAMPLER_ARBITRATION_CONTROL>(0), sbaCmd.getSamplerArbitrationControl());

    debugManager.flags.OverrideSamplerArbitrationControl.set(1u);
    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args);
    EXPECT_EQ(static_cast<FamilyType::STATE_BASE_ADDRESS::SAMPLER_ARBITRATION_CONTROL>(1), sbaCmd.getSamplerArbitrationControl());

    debugManager.flags.OverrideSamplerArbitrationControl.set(2u);
    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args);
    EXPECT_EQ(static_cast<FamilyType::STATE_BASE_ADDRESS::SAMPLER_ARBITRATION_CONTROL>(2), sbaCmd.getSamplerArbitrationControl());
}

using CommandContainerXe3pTest = Test<CommandEncodeStatesFixture>;
XE3P_CORETEST_F(CommandContainerXe3pTest, GivenCmdContainerWhenContainerIsInitializedThenSurfaceStateIndirectHeapSizeIsCorrect) {
    MyMockCommandContainer mockCmdContainer;
    mockCmdContainer.initialize(pDevice, nullptr, HeapSize::getDefaultHeapSize(IndirectHeapType::surfaceState), true, false);
    auto size = mockCmdContainer.allocationIndirectHeaps[IndirectHeap::Type::surfaceState]->getUnderlyingBufferSize();
    auto &productHelper = pDevice->getProductHelper();
    if (productHelper.isAvailableExtendedScratch()) {
        size_t expectedExtendedHeapSize = alignUp(2 * HeapSize::getDefaultHeapSize(IndirectHeapType::surfaceState), MemoryConstants::pageSize64k);
        EXPECT_EQ(expectedExtendedHeapSize, size);
    } else {
        constexpr size_t expectedHeapSize = MemoryConstants::pageSize64k;
        EXPECT_EQ(expectedHeapSize, size);
    }
}

using EncodePostSyncXe3pCoreTest = Test<CommandEncodeStatesFixture>;
XE3P_CORETEST_F(CommandContainerXe3pTest, GivenComputeWalker2AndArgsWhencallingSetCommandLevelInterruptThenInterruptValueLeftUnchanged) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    uint32_t dims[] = {1, 1, 1};
    bool requiresUncachedMocs = false;
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.postSyncArgs.dcFlushEnable = true;

    EncodeDispatchKernel<FamilyType>::template encode<WalkerType>(*cmdContainer.get(), dispatchArgs);
    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<WalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<WalkerType *>(*itor);

    walkerCmd->getPostSync().setInterruptSignalEnable(true);
    walkerCmd->getPostSyncOpn1().setInterruptSignalEnable(true);

    EncodePostSync<FamilyType>::template setCommandLevelInterrupt<WalkerType>(*walkerCmd, false);

    EXPECT_TRUE(walkerCmd->getPostSync().getInterruptSignalEnable());
    EXPECT_TRUE(walkerCmd->getPostSyncOpn1().getInterruptSignalEnable());
}
