/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen_common/reg_configs_common.h"
#include "shared/source/helpers/preamble.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/mocks/mock_gmm_helper.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/sources/debugger/l0_debugger_fixture.h"

namespace L0 {
namespace ult {

using L0DebuggerTest = Test<L0DebuggerHwFixture>;

TEST_F(L0DebuggerTest, givenL0DebuggerWhenCallingIsLegacyThenFalseIsReturned) {
    EXPECT_FALSE(neoDevice->getDebugger()->isLegacy());
}

TEST_F(L0DebuggerTest, givenL0DebuggerWhenGettingSourceLevelDebuggerThenNullptrReturned) {
    EXPECT_EQ(nullptr, neoDevice->getSourceLevelDebugger());
}

TEST_F(L0DebuggerTest, givenL0DebuggerWhenGettingL0DebuggerThenValidDebuggerInstanceIsReturned) {
    EXPECT_NE(nullptr, device->getL0Debugger());
}

TEST_F(L0DebuggerTest, givenL0DebuggerWhenGettingSipAllocationThenValidSipTypeIsReturned) {
    neoDevice->setDebuggerActive(true);
    auto systemRoutine = SipKernel::getSipKernel(*neoDevice).getSipAllocation();
    ASSERT_NE(nullptr, systemRoutine);

    auto sipType = SipKernel::getSipKernelType(*neoDevice);
    auto isHexadecimalArrayPreferred = HwHelper::get(hwInfo.platform.eRenderCoreFamily).isSipKernelAsHexadecimalArrayPreferred();

    auto expectedSipAllocation = isHexadecimalArrayPreferred
                                     ? NEO::MockSipData::mockSipKernel->getSipAllocation()
                                     : neoDevice->getBuiltIns()->getSipKernel(sipType, *neoDevice).getSipAllocation();

    EXPECT_EQ(expectedSipAllocation, systemRoutine);
}

TEST_F(L0DebuggerTest, givenL0DebuggerWhenGettingStateSaveAreaHeaderThenValidSipTypeIsReturned) {
    auto &stateSaveAreaHeader = SipKernel::getSipKernel(*neoDevice).getStateSaveAreaHeader();

    auto sipType = SipKernel::getSipKernelType(*neoDevice);
    auto &expectedStateSaveAreaHeader = neoDevice->getBuiltIns()->getSipKernel(sipType, *neoDevice).getStateSaveAreaHeader();

    EXPECT_EQ(expectedStateSaveAreaHeader, stateSaveAreaHeader);
}

TEST_F(L0DebuggerTest, givenProgramDebuggingEnabledWhenDebuggerIsCreatedThenFusedEusAreDisabled) {
    EXPECT_TRUE(driverHandle->enableProgramDebugging);
    EXPECT_FALSE(neoDevice->getHardwareInfo().capabilityTable.fusedEuEnabled);
}

TEST(Debugger, givenL0DebuggerOFFWhenGettingStateSaveAreaHeaderThenValidSipTypeIsReturned) {
    auto executionEnvironment = new NEO::ExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1);

    auto isHexadecimalArrayPreferred = HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).isSipKernelAsHexadecimalArrayPreferred();
    if (!isHexadecimalArrayPreferred) {
        auto mockBuiltIns = new MockBuiltins();
        executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
    }
    auto hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrLocalMemory = true;
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(&hwInfo);
    executionEnvironment->initializeMemoryManager();

    auto neoDevice = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0u);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    auto driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->enableProgramDebugging = false;

    driverHandle->initialize(std::move(devices));
    auto sipType = SipKernel::getSipKernelType(*neoDevice);

    if (isHexadecimalArrayPreferred) {
        SipKernel::initSipKernel(sipType, *neoDevice);
    }
    auto &stateSaveAreaHeader = SipKernel::getSipKernel(*neoDevice).getStateSaveAreaHeader();

    if (isHexadecimalArrayPreferred) {
        auto sipKernel = neoDevice->getRootDeviceEnvironment().sipKernels[static_cast<uint32_t>(sipType)].get();
        ASSERT_NE(sipKernel, nullptr);
        auto &expectedStateSaveAreaHeader = sipKernel->getStateSaveAreaHeader();
        EXPECT_EQ(expectedStateSaveAreaHeader, stateSaveAreaHeader);
    } else {
        auto &expectedStateSaveAreaHeader = neoDevice->getBuiltIns()->getSipKernel(sipType, *neoDevice).getStateSaveAreaHeader();
        EXPECT_EQ(expectedStateSaveAreaHeader, stateSaveAreaHeader);
    }
}

TEST(Debugger, givenDebuggingEnabledInExecEnvWhenAllocatingIsaThenSingleBankIsUsed) {
    auto executionEnvironment = new NEO::ExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->setDebuggingEnabled();

    auto hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrLocalMemory = true;
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(&hwInfo);
    executionEnvironment->initializeMemoryManager();

    std::unique_ptr<NEO::MockDevice> neoDevice(NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0u));

    auto allocation = neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {neoDevice->getRootDeviceIndex(), 4096, NEO::AllocationType::KERNEL_ISA, neoDevice->getDeviceBitfield()});

    if (allocation->getMemoryPool() == MemoryPool::LocalMemory) {
        EXPECT_EQ(1u, allocation->storageInfo.getMemoryBanks());
    } else {
        EXPECT_EQ(0u, allocation->storageInfo.getMemoryBanks());
    }

    neoDevice->getMemoryManager()->freeGraphicsMemory(allocation);
}

HWTEST_F(L0DebuggerTest, givenL0DebuggerWhenCreatedThenPerContextSbaTrackingBuffersAreAllocated) {
    auto debugger = device->getL0Debugger();
    ASSERT_NE(nullptr, debugger);

    EXPECT_NE(0u, debugger->getSbaTrackingGpuVa());
    std::vector<NEO::GraphicsAllocation *> allocations;

    auto &allEngines = device->getNEODevice()->getMemoryManager()->getRegisteredEngines();

    for (auto &engine : allEngines) {
        auto sbaAllocation = debugger->getSbaTrackingBuffer(engine.osContext->getContextId());
        ASSERT_NE(nullptr, sbaAllocation);
        allocations.push_back(sbaAllocation);

        EXPECT_EQ(NEO::AllocationType::DEBUG_SBA_TRACKING_BUFFER, sbaAllocation->getAllocationType());
        EXPECT_EQ(MemoryPool::System4KBPages, sbaAllocation->getMemoryPool());
    }

    for (uint32_t i = 0; i < allocations.size() - 1; i++) {
        EXPECT_NE(allocations[i], allocations[i + 1]);
    }

    EXPECT_EQ(allEngines.size(), getMockDebuggerL0Hw<FamilyType>()->perContextSbaAllocations.size());
}

HWTEST_F(L0DebuggerTest, givenCreatedL0DebuggerThenSbaTrackingBuffersContainValidHeader) {
    auto debugger = device->getL0Debugger();
    ASSERT_NE(nullptr, debugger);

    for (auto &sbaBuffer : getMockDebuggerL0Hw<FamilyType>()->perContextSbaAllocations) {
        auto sbaAllocation = sbaBuffer.second;
        ASSERT_NE(nullptr, sbaAllocation);

        auto sbaHeader = reinterpret_cast<SbaTrackedAddresses *>(sbaAllocation->getUnderlyingBuffer());

        EXPECT_STREQ("sbaarea", sbaHeader->magic);
        EXPECT_EQ(0u, sbaHeader->BindlessSamplerStateBaseAddress);
        EXPECT_EQ(0u, sbaHeader->BindlessSurfaceStateBaseAddress);
        EXPECT_EQ(0u, sbaHeader->DynamicStateBaseAddress);
        EXPECT_EQ(0u, sbaHeader->GeneralStateBaseAddress);
        EXPECT_EQ(0u, sbaHeader->IndirectObjectBaseAddress);
        EXPECT_EQ(0u, sbaHeader->InstructionBaseAddress);
        EXPECT_EQ(0u, sbaHeader->SurfaceStateBaseAddress);
        EXPECT_EQ(0u, sbaHeader->Version);
    }
}

HWTEST_F(L0DebuggerTest, givenDebuggingEnabledWhenCommandListIsExecutedThenValidKernelDebugCommandsAreAdded) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using STATE_SIP = typename FamilyType::STATE_SIP;

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue->commandStream);

    auto usedSpaceBefore = commandQueue->commandStream->getUsed();

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle()};
    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));

    auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());

    size_t debugModeRegisterCount = 0;
    size_t tdDebugControlRegisterCount = 0;

    for (size_t i = 0; i < miLoadImm.size(); i++) {
        MI_LOAD_REGISTER_IMM *miLoad = genCmdCast<MI_LOAD_REGISTER_IMM *>(*miLoadImm[i]);
        ASSERT_NE(nullptr, miLoad);

        if (miLoad->getRegisterOffset() == DebugModeRegisterOffset<FamilyType>::registerOffset) {
            EXPECT_EQ(DebugModeRegisterOffset<FamilyType>::debugEnabledValue, miLoad->getDataDword());
            debugModeRegisterCount++;
        }
        if (miLoad->getRegisterOffset() == TdDebugControlRegisterOffset<FamilyType>::registerOffset) {
            EXPECT_EQ(TdDebugControlRegisterOffset<FamilyType>::debugEnabledValue, miLoad->getDataDword());
            tdDebugControlRegisterCount++;
        }
    }
    // those register should not be used
    EXPECT_EQ(0u, debugModeRegisterCount);
    EXPECT_EQ(0u, tdDebugControlRegisterCount);

    if (!HwHelper::get(hwInfo.platform.eRenderCoreFamily).isSipWANeeded(hwInfo)) {
        auto stateSipCmds = findAll<STATE_SIP *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, stateSipCmds.size());

        STATE_SIP *stateSip = genCmdCast<STATE_SIP *>(*stateSipCmds[0]);

        auto systemRoutine = SipKernel::getSipKernel(*neoDevice).getSipAllocation();
        ASSERT_NE(nullptr, systemRoutine);
        EXPECT_EQ(systemRoutine->getGpuAddressToPatch(), stateSip->getSystemInstructionPointer());
    }

    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(commandLists[i]);
        commandList->destroy();
    }

    commandQueue->destroy();
}

using NotGen8Or11 = AreNotGfxCores<IGFX_GEN8_CORE, IGFX_GEN11_CORE>;

HWTEST2_F(L0DebuggerTest, givenDebuggingEnabledAndRequiredGsbaWhenCommandListIsExecutedThenProgramGsbaWritesToSbaTrackingBuffer, NotGen8Or11) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto cmdQ = CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue);
    ASSERT_NE(nullptr, cmdQ);

    auto commandQueue = whitebox_cast(cmdQ);
    auto cmdQHw = static_cast<CommandQueueHw<gfxCoreFamily> *>(cmdQ);

    if (cmdQHw->estimateStateBaseAddressCmdSize() == 0) {
        commandQueue->destroy();
        GTEST_SKIP();
    }

    auto usedSpaceBefore = commandQueue->commandStream->getUsed();

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle()};
    CommandList::fromHandle(commandLists[0])->setCommandListPerThreadScratchSize(4096);

    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));

    auto sbaItor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), sbaItor);
    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*sbaItor);

    auto sdiItor = find<MI_STORE_DATA_IMM *>(sbaItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);
    auto cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    uint64_t gsbaGpuVa = cmdSba->getGeneralStateBaseAddress();
    EXPECT_EQ(static_cast<uint32_t>(gsbaGpuVa & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(gsbaGpuVa >> 32), cmdSdi->getDataDword1());

    auto expectedGpuVa = GmmHelper::decanonize(device->getL0Debugger()->getSbaTrackingGpuVa()) + offsetof(SbaTrackedAddresses, GeneralStateBaseAddress);
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());

    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(commandLists[i]);
        commandList->destroy();
    }
    commandQueue->destroy();
}

HWTEST_F(L0DebuggerTest, givenDebuggingEnabledAndDebuggerLogsWhenCommandQueueIsSynchronizedThenSbaAddressesArePrinted) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.DebuggerLogBitmask.set(255);

    testing::internal::CaptureStdout();

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue->commandStream);

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle()};
    const uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    commandQueue->synchronize(0);

    std::string output = testing::internal::GetCapturedStdout();
    size_t pos = output.find("INFO: Debugger: SBA stored ssh");
    EXPECT_NE(std::string::npos, pos);

    pos = output.find("Debugger: SBA ssh");
    EXPECT_NE(std::string::npos, pos);

    auto commandList = CommandList::fromHandle(commandLists[0]);
    commandList->destroy();

    commandQueue->destroy();
}

using L0DebuggerSimpleTest = Test<DeviceFixture>;

HWTEST_F(L0DebuggerSimpleTest, givenNullL0DebuggerAndDebuggerLogsWhenCommandQueueIsSynchronizedThenSbaAddressesAreNotPrinted) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.DebuggerLogBitmask.set(255);

    EXPECT_EQ(nullptr, device->getL0Debugger());
    testing::internal::CaptureStdout();

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue->commandStream);

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle()};
    const uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    commandQueue->synchronize(0);

    std::string output = testing::internal::GetCapturedStdout();
    size_t pos = output.find("Debugger: SBA");
    EXPECT_EQ(std::string::npos, pos);

    auto commandList = CommandList::fromHandle(commandLists[0]);
    commandList->destroy();

    commandQueue->destroy();
}

HWTEST_F(L0DebuggerTest, givenL0DebuggerAndDebuggerLogsDisabledWhenCommandQueueIsSynchronizedThenSbaAddressesAreNotPrinted) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.DebuggerLogBitmask.set(0);

    EXPECT_NE(nullptr, device->getL0Debugger());
    testing::internal::CaptureStdout();

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue->commandStream);

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle()};
    const uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    commandQueue->synchronize(0);

    std::string output = testing::internal::GetCapturedStdout();
    size_t pos = output.find("Debugger: SBA");
    EXPECT_EQ(std::string::npos, pos);

    auto commandList = CommandList::fromHandle(commandLists[0]);
    commandList->destroy();

    commandQueue->destroy();
}

HWTEST2_F(L0DebuggerTest, givenDebuggingEnabledWhenNonCopyCommandListIsInititalizedOrResetThenSSHAddressIsTracked, NotGen8Or11) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    size_t usedSpaceBefore = 0;
    ze_result_t returnValue;
    ze_command_list_handle_t commandListHandle = CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle();
    auto commandList = CommandList::fromHandle(commandListHandle);

    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, commandList->commandContainer.getCommandStream()->getCpuBase(), usedSpaceAfter));

    auto sbaItor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), sbaItor);
    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*sbaItor);

    uint64_t sshGpuVa = cmdSba->getSurfaceStateBaseAddress();
    auto expectedGpuVa = commandList->commandContainer.getIndirectHeap(NEO::HeapType::SURFACE_STATE)->getHeapGpuBase();
    EXPECT_EQ(expectedGpuVa, sshGpuVa);
    EXPECT_EQ(1u, getMockDebuggerL0Hw<FamilyType>()->captureStateBaseAddressCount);

    commandList->reset();
    EXPECT_EQ(2u, getMockDebuggerL0Hw<FamilyType>()->captureStateBaseAddressCount);

    commandList->destroy();
}

HWTEST_F(L0DebuggerTest, givenDebuggerWhenAppendingKernelToCommandListThenBindlessSurfaceStateForDebugSurfaceIsProgrammedAtOffsetZero) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    Mock<::L0::Kernel> kernel;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ze_group_count_t groupCount{1, 1, 1};
    auto result = commandList->appendLaunchKernel(kernel.toHandle(), &groupCount, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->close();

    auto *ssh = commandList->commandContainer.getIndirectHeap(NEO::HeapType::SURFACE_STATE);

    auto debugSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ssh->getCpuBase());
    auto debugSurface = static_cast<L0::DeviceImp *>(device)->getDebugSurface();

    SURFACE_STATE_BUFFER_LENGTH length;
    length.Length = static_cast<uint32_t>(debugSurface->getUnderlyingBufferSize() - 1);

    EXPECT_EQ(length.SurfaceState.Depth + 1u, debugSurfaceState->getDepth());
    EXPECT_EQ(length.SurfaceState.Width + 1u, debugSurfaceState->getWidth());
    EXPECT_EQ(length.SurfaceState.Height + 1u, debugSurfaceState->getHeight());
    EXPECT_EQ(debugSurface->getGpuAddress(), debugSurfaceState->getSurfaceBaseAddress());

    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER, debugSurfaceState->getSurfaceType());
    EXPECT_EQ(RENDER_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT, debugSurfaceState->getCoherencyType());
}

HWTEST_F(L0DebuggerTest, givenDebuggerWhenAppendingKernelToCommandListThenDebugSurfaceiIsProgrammedWithL3DisabledMOCS) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    Mock<::L0::Kernel> kernel;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ze_group_count_t groupCount{1, 1, 1};
    auto result = commandList->appendLaunchKernel(kernel.toHandle(), &groupCount, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->close();

    auto *ssh = commandList->commandContainer.getIndirectHeap(NEO::HeapType::SURFACE_STATE);
    auto debugSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ssh->getCpuBase());

    const auto mocsNoCache = device->getNEODevice()->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED) >> 1;
    const auto actualMocs = debugSurfaceState->getMemoryObjectControlState();

    EXPECT_EQ(actualMocs, mocsNoCache);
}

HWTEST2_F(L0DebuggerTest, givenDebuggingEnabledWhenCommandListIsExecutedThenSbaBufferIsPushedToResidencyContainer, IsAtLeastSkl) {
    ze_command_queue_desc_t queueDesc = {};

    std::unique_ptr<MockCommandQueueHw<gfxCoreFamily>, Deleter> commandQueue(new MockCommandQueueHw<gfxCoreFamily>(device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc));
    commandQueue->initialize(false, false);

    ze_result_t returnValue;
    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle()};
    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto sbaBuffer = device->getL0Debugger()->getSbaTrackingBuffer(neoDevice->getDefaultEngine().commandStreamReceiver->getOsContext().getContextId());
    bool sbaFound = false;

    for (auto iter : commandQueue->residencyContainerSnapshot) {
        if (iter == sbaBuffer) {
            sbaFound = true;
        }
    }
    EXPECT_TRUE(sbaFound);

    auto commandList = CommandList::fromHandle(commandLists[0]);
    commandList->destroy();
}

HWTEST_F(L0DebuggerSimpleTest, givenUseCsrImmediateSubmissionEnabledWithImmediateCommandListToInvokeNonKernelOperationsThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    void *dstPtr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, 16384u, 4096u, &dstPtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->device);
    EXPECT_EQ(1u, commandList->cmdListType);
    EXPECT_NE(nullptr, commandList->cmdQImmediate);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &event);

    std::unique_ptr<L0::Event> event_object(L0::Event::fromHandle(event));
    ASSERT_NE(nullptr, event_object->csr);
    ASSERT_EQ(static_cast<DeviceImp *>(device)->getNEODevice()->getDefaultEngine().commandStreamReceiver, event_object->csr);

    returnValue = commandList->appendWaitOnEvents(1, &event);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);

    returnValue = commandList->appendBarrier(nullptr, 1, &event);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);

    returnValue = commandList->appendSignalEvent(event);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);

    returnValue = event_object->hostSignal();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(event_object->queryStatus(), ZE_RESULT_SUCCESS);

    returnValue = commandList->appendWriteGlobalTimestamp(reinterpret_cast<uint64_t *>(dstPtr), nullptr, 0, nullptr);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);

    returnValue = commandList->appendEventReset(event);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);

    context->freeMem(dstPtr);
}

HWTEST_F(L0DebuggerSimpleTest, givenUseCsrImmediateSubmissionDisabledWithImmediateCommandListToInvokeNonKernelOperationsThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(false);

    void *dstPtr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, 16384u, 4096u, &dstPtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->device);
    EXPECT_EQ(1u, commandList->cmdListType);
    EXPECT_NE(nullptr, commandList->cmdQImmediate);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &event);

    std::unique_ptr<L0::Event> event_object(L0::Event::fromHandle(event));
    ASSERT_NE(nullptr, event_object->csr);
    ASSERT_EQ(static_cast<DeviceImp *>(device)->getNEODevice()->getDefaultEngine().commandStreamReceiver, event_object->csr);

    returnValue = commandList->appendWaitOnEvents(1, &event);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);

    returnValue = commandList->appendBarrier(nullptr, 1, &event);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);

    returnValue = commandList->appendSignalEvent(event);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);

    returnValue = event_object->hostSignal();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(event_object->queryStatus(), ZE_RESULT_SUCCESS);

    returnValue = commandList->appendWriteGlobalTimestamp(reinterpret_cast<uint64_t *>(dstPtr), nullptr, 0, nullptr);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);

    returnValue = commandList->appendEventReset(event);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);

    context->freeMem(dstPtr);
}

HWTEST_F(L0DebuggerSimpleTest, givenUseCsrImmediateSubmissionEnabledForImmediateCommandListForAppendMemoryFillWithDeviceMemoryThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    void *dstPtr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &dstPtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    int pattern = 1;

    ze_command_queue_desc_t queueDesc = {};
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, true, NEO::EngineGroupType::RenderCompute, returnValue);

    result = commandList->appendMemoryFill(dstPtr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 4096u, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->freeMem(dstPtr);
    commandList->destroy();
}

HWTEST_F(L0DebuggerSimpleTest, givenUseCsrImmediateSubmissionEnabledForImmediateCommandListForAppendMemoryFillThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    void *dstPtr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, 16384u, 4096u, &dstPtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    int pattern = 1;

    ze_command_queue_desc_t queueDesc = {};
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, true, NEO::EngineGroupType::RenderCompute, returnValue);

    result = commandList->appendMemoryFill(dstPtr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 4096u, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->freeMem(dstPtr);
    commandList->destroy();
}

HWTEST_F(L0DebuggerSimpleTest, givenUseCsrImmediateSubmissionDisabledForImmediateCommandListForAppendMemoryFillThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(false);

    void *dstPtr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, 16384u, 4096u, &dstPtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    int pattern = 1;

    ze_command_queue_desc_t queueDesc = {};
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, true, NEO::EngineGroupType::RenderCompute, returnValue);

    result = commandList->appendMemoryFill(dstPtr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 4096u, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->freeMem(dstPtr);
    commandList->destroy();
}

HWTEST_F(L0DebuggerSimpleTest, givenUseCsrImmediateSubmissionEnabledForRegularCommandListForAppendMemoryFillThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue->commandStream);

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle()};
    const uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    void *dstPtr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &dstPtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    int pattern = 1;

    auto commandList = CommandList::fromHandle(commandLists[0]);
    result = commandList->appendMemoryFill(dstPtr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 4096u, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    commandQueue->synchronize(0);

    context->freeMem(dstPtr);
    commandList->destroy();

    commandQueue->destroy();
}

HWTEST_F(L0DebuggerSimpleTest, givenUseCsrImmediateSubmissionDisabledForRegularCommandListForAppendMemoryFillThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(false);

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue->commandStream);

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle()};
    const uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    void *dstPtr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, 16384u, 4096u, &dstPtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    int pattern = 1;

    auto commandList = CommandList::fromHandle(commandLists[0]);
    result = commandList->appendMemoryFill(dstPtr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 4096u, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    commandQueue->synchronize(0);

    context->freeMem(dstPtr);
    commandList->destroy();

    commandQueue->destroy();
}

HWTEST2_F(L0DebuggerSimpleTest, givenUseCsrImmediateSubmissionEnabledCommandListAndAppendPageFaultCopyThenSuccessIsReturned, IsAtLeastSkl) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    size_t size = (sizeof(uint32_t) * 4);
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, true, NEO::EngineGroupType::RenderCompute, returnValue);
    ASSERT_NE(nullptr, commandList);

    NEO::GraphicsAllocation srcPtr(0, NEO::AllocationType::INTERNAL_HOST_MEMORY,
                                   reinterpret_cast<void *>(0x1234), size, 0, sizeof(uint32_t),
                                   MemoryPool::System4KBPages);
    NEO::GraphicsAllocation dstPtr(0, NEO::AllocationType::INTERNAL_HOST_MEMORY,
                                   reinterpret_cast<void *>(0x2345), size, 0, sizeof(uint32_t),
                                   MemoryPool::System4KBPages);

    auto result = commandList->appendPageFaultCopy(&dstPtr, &srcPtr, 0x100, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
}

HWTEST2_F(L0DebuggerSimpleTest, givenUseCsrImmediateSubmissionDisabledCommandListAndAppendPageFaultCopyThenSuccessIsReturned, IsAtLeastSkl) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(false);

    size_t size = (sizeof(uint32_t) * 4);
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, true, NEO::EngineGroupType::RenderCompute, returnValue);
    ASSERT_NE(nullptr, commandList);

    NEO::GraphicsAllocation srcPtr(0, NEO::AllocationType::INTERNAL_HOST_MEMORY,
                                   reinterpret_cast<void *>(0x1234), size, 0, sizeof(uint32_t),
                                   MemoryPool::System4KBPages);
    NEO::GraphicsAllocation dstPtr(0, NEO::AllocationType::INTERNAL_HOST_MEMORY,
                                   reinterpret_cast<void *>(0x2345), size, 0, sizeof(uint32_t),
                                   MemoryPool::System4KBPages);

    auto result = commandList->appendPageFaultCopy(&dstPtr, &srcPtr, 0x100, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
}

HWTEST_F(L0DebuggerSimpleTest, givenNonZeroGpuVasWhenProgrammingSbaTrackingThenCorrectCmdsAreAddedToStream) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(neoDevice);

    debugger->sbaTrackingGpuVa.address = 0x45670000;
    auto expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(SbaTrackedAddresses, GeneralStateBaseAddress);

    StackVec<char, 4096> buffer(4096);
    NEO::LinearStream cmdStream(buffer.begin(), buffer.size());
    uint64_t gsba = 0x60000;
    uint64_t ssba = 0x1234567000;
    uint64_t iba = 0xfff80000;
    uint64_t ioba = 0x8100000;
    uint64_t dsba = 0xffff0000aaaa0000;

    NEO::Debugger::SbaAddresses sbaAddresses = {};
    sbaAddresses.GeneralStateBaseAddress = gsba;
    sbaAddresses.SurfaceStateBaseAddress = ssba;
    sbaAddresses.InstructionBaseAddress = iba;
    sbaAddresses.IndirectObjectBaseAddress = ioba;
    sbaAddresses.DynamicStateBaseAddress = dsba;
    sbaAddresses.BindlessSurfaceStateBaseAddress = ssba;

    debugger->programSbaTrackingCommands(cmdStream, sbaAddresses);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, cmdStream.getCpuBase(), cmdStream.getUsed()));

    EXPECT_EQ(6 * sizeof(MI_STORE_DATA_IMM), cmdStream.getUsed());

    auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);
    auto cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    EXPECT_EQ(static_cast<uint32_t>(gsba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(gsba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(SbaTrackedAddresses, SurfaceStateBaseAddress);
    EXPECT_EQ(static_cast<uint32_t>(ssba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(ssba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(SbaTrackedAddresses, DynamicStateBaseAddress);

    auto decanonizedAddress = GmmHelper::decanonize(dsba);
    EXPECT_EQ(static_cast<uint32_t>(decanonizedAddress & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(decanonizedAddress >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(SbaTrackedAddresses, IndirectObjectBaseAddress);
    EXPECT_EQ(static_cast<uint32_t>(ioba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(ioba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(SbaTrackedAddresses, InstructionBaseAddress);
    EXPECT_EQ(static_cast<uint32_t>(iba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(iba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(SbaTrackedAddresses, BindlessSurfaceStateBaseAddress);
    EXPECT_EQ(static_cast<uint32_t>(ssba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(ssba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());
}

HWTEST_F(L0DebuggerSimpleTest, givenCanonizedGpuVasWhenProgrammingSbaTrackingThenNonCanonicalAddressesAreStored) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(neoDevice);

    debugger->sbaTrackingGpuVa.address = 0x45670000;
    auto expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(SbaTrackedAddresses, GeneralStateBaseAddress);

    StackVec<char, 4096> buffer(4096);
    NEO::LinearStream cmdStream(buffer.begin(), buffer.size());
    uint64_t gsba = 0xffff800000060000;
    uint64_t ssba = 0xffff801234567000;
    uint64_t iba = 0xffff8000fff80000;
    uint64_t ioba = 0xffff800008100000;
    uint64_t dsba = 0xffff8000aaaa0000;

    NEO::Debugger::SbaAddresses sbaAddresses = {};
    sbaAddresses.GeneralStateBaseAddress = gsba;
    sbaAddresses.SurfaceStateBaseAddress = ssba;
    sbaAddresses.InstructionBaseAddress = iba;
    sbaAddresses.IndirectObjectBaseAddress = ioba;
    sbaAddresses.DynamicStateBaseAddress = dsba;
    sbaAddresses.BindlessSurfaceStateBaseAddress = ssba;

    debugger->programSbaTrackingCommands(cmdStream, sbaAddresses);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, cmdStream.getCpuBase(), cmdStream.getUsed()));

    EXPECT_EQ(6 * sizeof(MI_STORE_DATA_IMM), cmdStream.getUsed());

    auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);
    auto cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    auto decanonizedAddress = GmmHelper::decanonize(gsba);
    EXPECT_EQ(static_cast<uint32_t>(decanonizedAddress & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(decanonizedAddress >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(SbaTrackedAddresses, SurfaceStateBaseAddress);
    decanonizedAddress = GmmHelper::decanonize(ssba);
    EXPECT_EQ(static_cast<uint32_t>(decanonizedAddress & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(decanonizedAddress >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(SbaTrackedAddresses, DynamicStateBaseAddress);
    decanonizedAddress = GmmHelper::decanonize(dsba);
    EXPECT_EQ(static_cast<uint32_t>(decanonizedAddress & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(decanonizedAddress >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(SbaTrackedAddresses, IndirectObjectBaseAddress);
    decanonizedAddress = GmmHelper::decanonize(ioba);
    EXPECT_EQ(static_cast<uint32_t>(decanonizedAddress & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(decanonizedAddress >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(SbaTrackedAddresses, InstructionBaseAddress);
    decanonizedAddress = GmmHelper::decanonize(iba);
    EXPECT_EQ(static_cast<uint32_t>(decanonizedAddress & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(decanonizedAddress >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(SbaTrackedAddresses, BindlessSurfaceStateBaseAddress);
    decanonizedAddress = GmmHelper::decanonize(ssba);
    EXPECT_EQ(static_cast<uint32_t>(decanonizedAddress & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(decanonizedAddress >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());
}

HWTEST_F(L0DebuggerSimpleTest, givenZeroGpuVasWhenProgrammingSbaTrackingThenStreamIsNotUsed) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(neoDevice);

    debugger->sbaTrackingGpuVa.address = 0x45670000;

    StackVec<char, 4096> buffer(4096);
    NEO::LinearStream cmdStream(buffer.begin(), buffer.size());
    uint64_t gsba = 0;
    uint64_t ssba = 0;

    NEO::Debugger::SbaAddresses sbaAddresses = {};
    sbaAddresses.GeneralStateBaseAddress = gsba;
    sbaAddresses.SurfaceStateBaseAddress = ssba;

    debugger->programSbaTrackingCommands(cmdStream, sbaAddresses);

    EXPECT_EQ(0u, cmdStream.getUsed());
}

HWTEST_F(L0DebuggerSimpleTest, whenAllocateCalledThenDebuggerIsCreated) {
    auto debugger = DebuggerL0Hw<FamilyType>::allocate(neoDevice);
    EXPECT_NE(nullptr, debugger);
    delete debugger;
}

HWTEST_F(L0DebuggerSimpleTest, givenNotChangedSurfaceStateWhenCapturingSBAThenNoTrackingCmdsAreAdded) {
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(neoDevice);

    debugger->sbaTrackingGpuVa.address = 0x45670000;

    NEO::CommandContainer container;
    container.initialize(neoDevice, nullptr, true);

    NEO::Debugger::SbaAddresses sba = {};
    sba.SurfaceStateBaseAddress = 0x123456000;

    debugger->captureStateBaseAddress(container, sba);
    auto sizeUsed = container.getCommandStream()->getUsed();

    EXPECT_NE(0u, sizeUsed);
    sba.SurfaceStateBaseAddress = 0;

    debugger->captureStateBaseAddress(container, sba);
    auto sizeUsed2 = container.getCommandStream()->getUsed();

    EXPECT_EQ(sizeUsed, sizeUsed2);
}

HWTEST_F(L0DebuggerSimpleTest, givenChangedBaseAddressesWhenCapturingSBAThenNoTrackingCmdsAreAdded) {
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(neoDevice);

    debugger->sbaTrackingGpuVa.address = 0x45670000;
    {
        NEO::CommandContainer container;
        container.initialize(neoDevice, nullptr, true);

        NEO::Debugger::SbaAddresses sba = {};
        sba.SurfaceStateBaseAddress = 0x123456000;

        debugger->captureStateBaseAddress(container, sba);
        auto sizeUsed = container.getCommandStream()->getUsed();

        EXPECT_NE(0u, sizeUsed);
    }

    {
        NEO::CommandContainer container;
        container.initialize(neoDevice, nullptr, true);

        NEO::Debugger::SbaAddresses sba = {};
        sba.GeneralStateBaseAddress = 0x123456000;

        debugger->captureStateBaseAddress(container, sba);
        auto sizeUsed = container.getCommandStream()->getUsed();

        EXPECT_NE(0u, sizeUsed);
    }

    {
        NEO::CommandContainer container;
        container.initialize(neoDevice, nullptr, true);

        NEO::Debugger::SbaAddresses sba = {};
        sba.BindlessSurfaceStateBaseAddress = 0x123456000;

        debugger->captureStateBaseAddress(container, sba);
        auto sizeUsed = container.getCommandStream()->getUsed();

        EXPECT_NE(0u, sizeUsed);
    }
}

HWTEST_F(L0DebuggerTest, givenDebuggerWhenCreatedThenModuleHeapDebugAreaIsCreated) {
    auto mockBlitMemoryToAllocation = [](const NEO::Device &device, NEO::GraphicsAllocation *memory, size_t offset, const void *hostPtr,
                                         Vec3<size_t> size) -> NEO::BlitOperationResult {
        memcpy(memory->getUnderlyingBuffer(), hostPtr, size.x);
        return BlitOperationResult::Success;
    };
    VariableBackup<NEO::BlitHelperFunctions::BlitMemoryToAllocationFunc> blitMemoryToAllocationFuncBackup(
        &NEO::BlitHelperFunctions::blitMemoryToAllocation, mockBlitMemoryToAllocation);

    memoryOperationsHandler->makeResidentCalledCount = 0;
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(neoDevice);
    auto debugArea = debugger->getModuleDebugArea();

    EXPECT_EQ(1, memoryOperationsHandler->makeResidentCalledCount);

    auto allocation = neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {neoDevice->getRootDeviceIndex(), 4096, NEO::AllocationType::KERNEL_ISA, neoDevice->getDeviceBitfield()});

    EXPECT_EQ(allocation->storageInfo.getMemoryBanks(), debugArea->storageInfo.getMemoryBanks());

    DebugAreaHeader *header = reinterpret_cast<DebugAreaHeader *>(debugArea->getUnderlyingBuffer());
    EXPECT_EQ(1u, header->pgsize);
    uint64_t isShared = debugArea->storageInfo.getNumBanks() == 1 ? 1 : 0;
    EXPECT_EQ(isShared, header->isShared);

    EXPECT_STREQ("dbgarea", header->magic);
    EXPECT_EQ(sizeof(DebugAreaHeader), header->size);
    EXPECT_EQ(sizeof(DebugAreaHeader), header->scratchBegin);
    EXPECT_EQ(MemoryConstants::pageSize64k - sizeof(DebugAreaHeader), header->scratchEnd);

    neoDevice->getMemoryManager()->freeGraphicsMemory(allocation);
}

HWTEST_F(L0DebuggerTest, givenBindlessSipWhenModuleHeapDebugAreaIsCreatedThenReservedFieldIsSet) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.UseBindlessDebugSip.set(1);

    auto mockBlitMemoryToAllocation = [](const NEO::Device &device, NEO::GraphicsAllocation *memory, size_t offset, const void *hostPtr,
                                         Vec3<size_t> size) -> NEO::BlitOperationResult {
        memcpy(memory->getUnderlyingBuffer(), hostPtr, size.x);
        return BlitOperationResult::Success;
    };
    VariableBackup<NEO::BlitHelperFunctions::BlitMemoryToAllocationFunc> blitMemoryToAllocationFuncBackup(
        &NEO::BlitHelperFunctions::blitMemoryToAllocation, mockBlitMemoryToAllocation);

    memoryOperationsHandler->makeResidentCalledCount = 0;
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(neoDevice);
    auto debugArea = debugger->getModuleDebugArea();

    DebugAreaHeader *header = reinterpret_cast<DebugAreaHeader *>(debugArea->getUnderlyingBuffer());
    EXPECT_EQ(1u, header->reserved1);
}

HWTEST_F(L0DebuggerTest, givenBindfulSipWhenModuleHeapDebugAreaIsCreatedThenReservedFieldIsNotSet) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.UseBindlessDebugSip.set(0);

    auto mockBlitMemoryToAllocation = [](const NEO::Device &device, NEO::GraphicsAllocation *memory, size_t offset, const void *hostPtr,
                                         Vec3<size_t> size) -> NEO::BlitOperationResult {
        memcpy(memory->getUnderlyingBuffer(), hostPtr, size.x);
        return BlitOperationResult::Success;
    };
    VariableBackup<NEO::BlitHelperFunctions::BlitMemoryToAllocationFunc> blitMemoryToAllocationFuncBackup(
        &NEO::BlitHelperFunctions::blitMemoryToAllocation, mockBlitMemoryToAllocation);

    memoryOperationsHandler->makeResidentCalledCount = 0;
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(neoDevice);
    auto debugArea = debugger->getModuleDebugArea();

    DebugAreaHeader *header = reinterpret_cast<DebugAreaHeader *>(debugArea->getUnderlyingBuffer());
    EXPECT_EQ(0u, header->reserved1);
}

TEST(Debugger, givenNonLegacyDebuggerWhenInitializingDeviceCapsThenUnrecoverableIsCalled) {
    class MockDebugger : public NEO::Debugger {
      public:
        MockDebugger() {
            isLegacyMode = false;
        }

        void captureStateBaseAddress(CommandContainer &container, SbaAddresses sba) override{};
    };
    auto executionEnvironment = new NEO::ExecutionEnvironment();
    auto mockBuiltIns = new MockBuiltins();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());

    auto debugger = new MockDebugger;
    executionEnvironment->rootDeviceEnvironments[0]->debugger.reset(debugger);
    executionEnvironment->initializeMemoryManager();

    EXPECT_THROW(NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0u), std::exception);
}

using NotXeHPOrDG2 = AreNotGfxCores<IGFX_XE_HP_CORE, IGFX_XE_HPG_CORE>;
HWTEST2_F(L0DebuggerTest, givenNotAtsOrDg2AndDebugIsActiveThenDisableL3CacheInGmmHelperIsNotSet, NotXeHPOrDG2) {
    EXPECT_FALSE(static_cast<MockGmmHelper *>(neoDevice->getGmmHelper())->allResourcesUncached);
}

using ATSOrDG2 = IsWithinGfxCore<IGFX_XE_HP_CORE, IGFX_XE_HPG_CORE>;
HWTEST2_F(L0DebuggerTest, givenAtsOrDg2AndDebugIsActiveThenDisableL3CacheInGmmHelperIsSet, ATSOrDG2) {
    EXPECT_TRUE(static_cast<MockGmmHelper *>(neoDevice->getGmmHelper())->allResourcesUncached);
}
} // namespace ult
} // namespace L0
