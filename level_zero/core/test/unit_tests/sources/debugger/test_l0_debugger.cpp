/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/gen_common/reg_configs_common.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/variable_backup.h"

#include "test.h"

#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/sources/debugger/l0_debugger_fixture.h"

#include <bitset>

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
    auto expectedSipAllocation = neoDevice->getBuiltIns()->getSipKernel(sipType, *neoDevice).getSipAllocation();

    EXPECT_EQ(expectedSipAllocation, systemRoutine);
}

TEST_F(L0DebuggerTest, givenL0DebuggerWhenGettingStateSaveAreaHeaderThenValidSipTypeIsReturned) {
    auto &stateSaveAreaHeader = SipKernel::getSipKernel(*neoDevice).getStateSaveAreaHeader();

    auto sipType = SipKernel::getSipKernelType(*neoDevice);
    auto &expectedStateSaveAreaHeader = neoDevice->getBuiltIns()->getSipKernel(sipType, *neoDevice).getStateSaveAreaHeader();

    EXPECT_EQ(expectedStateSaveAreaHeader, stateSaveAreaHeader);
}

TEST(Debugger, givenL0DebuggerOFFWhenGettingStateSaveAreaHeaderThenValidSipTypeIsReturned) {
    auto executionEnvironment = new NEO::ExecutionEnvironment();
    auto mockBuiltIns = new MockBuiltins();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
    auto hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.ftrLocalMemory = true;
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(&hwInfo);
    executionEnvironment->initializeMemoryManager();

    auto neoDevice = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0u);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    auto driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->enableProgramDebugging = false;

    driverHandle->initialize(std::move(devices));

    auto &stateSaveAreaHeader = SipKernel::getSipKernel(*neoDevice).getStateSaveAreaHeader();

    auto sipType = SipKernel::getSipKernelType(*neoDevice);
    auto &expectedStateSaveAreaHeader = neoDevice->getBuiltIns()->getSipKernel(sipType, *neoDevice).getStateSaveAreaHeader();

    EXPECT_EQ(expectedStateSaveAreaHeader, stateSaveAreaHeader);
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

        EXPECT_EQ(NEO::GraphicsAllocation::AllocationType::DEBUG_SBA_TRACKING_BUFFER, sbaAllocation->getAllocationType());
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
        EXPECT_EQ(systemRoutine->getGpuAddress(), stateSip->getSystemInstructionPointer());
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

using IsSklOrAbove = IsAtLeastProduct<IGFX_SKYLAKE>;
HWTEST2_F(L0DebuggerTest, givenDebuggingEnabledWhenCommandListIsExecutedThenSbaBufferIsPushedToResidencyContainer, IsSklOrAbove) {
    ze_command_queue_desc_t queueDesc = {};

    struct Deleter {
        void operator()(CommandQueueImp *cmdQ) {
            cmdQ->destroy();
        }
    };

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

using L0DebuggerInternalUsageTest = L0DebuggerTest;
HWTEST_F(L0DebuggerInternalUsageTest, givenDebuggingEnabledWhenCommandListIsInititalizedOrResetThenCaptureSbaIsNotCalled) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    size_t usedSpaceBefore = 0;
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, true, NEO::EngineGroupType::RenderCompute, returnValue);

    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, commandList->commandContainer.getCommandStream()->getCpuBase(), usedSpaceAfter));

    auto sbaItor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), sbaItor);

    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->captureStateBaseAddressCount);

    commandList->reset();
    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->captureStateBaseAddressCount);

    commandList->destroy();
}

HWTEST_F(L0DebuggerInternalUsageTest, givenDebuggerLogsDisabledWhenCommandListIsSynchronizedThenSbaAddressesAreNotPrinted) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.DebuggerLogBitmask.set(0);

    EXPECT_NE(nullptr, device->getL0Debugger());
    testing::internal::CaptureStdout();

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, true, NEO::EngineGroupType::RenderCompute, returnValue);

    commandList->executeCommandListImmediate(false);

    std::string output = testing::internal::GetCapturedStdout();
    size_t pos = output.find("Debugger: SBA");
    EXPECT_EQ(std::string::npos, pos);

    commandList->destroy();
}

HWTEST2_F(L0DebuggerInternalUsageTest, givenDebuggingEnabledWhenInternalCmdQIsUsedThenDebuggerPathsAreNotExecuted, IsSklOrAbove) {
    ze_command_queue_desc_t queueDesc = {};

    struct Deleter {
        void operator()(CommandQueueImp *cmdQ) {
            cmdQ->destroy();
        }
    };

    std::unique_ptr<MockCommandQueueHw<gfxCoreFamily>, Deleter> commandQueue(new MockCommandQueueHw<gfxCoreFamily>(device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc));
    commandQueue->initialize(false, true);
    EXPECT_TRUE(commandQueue->internalUsage);
    ze_result_t returnValue;
    ze_command_list_handle_t commandLists[] = {
        CommandList::createImmediate(productFamily, device, &queueDesc, true, NEO::EngineGroupType::RenderCompute, returnValue)->toHandle()};
    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto sbaBuffer = device->getL0Debugger()->getSbaTrackingBuffer(neoDevice->getDefaultEngine().commandStreamReceiver->getOsContext().getContextId());
    auto sipIsa = NEO::SipKernel::getSipKernel(*neoDevice).getSipAllocation();
    auto debugSurface = device->getDebugSurface();
    bool sbaFound = false;
    bool sipFound = false;
    bool debugSurfaceFound = false;

    for (auto iter : commandQueue->residencyContainerSnapshot) {
        if (iter == sbaBuffer) {
            sbaFound = true;
        }
        if (iter == sipIsa) {
            sipFound = true;
        }
        if (iter == debugSurface) {
            debugSurfaceFound = true;
        }
    }
    EXPECT_FALSE(sbaFound);
    EXPECT_FALSE(sipFound);
    EXPECT_FALSE(debugSurfaceFound);

    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->captureStateBaseAddressCount);
    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->programSbaTrackingCommandsCount);
    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->getSbaTrackingCommandsSizeCount);

    auto commandList = CommandList::fromHandle(commandLists[0]);
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
    EXPECT_EQ(static_cast<uint32_t>(dsba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(dsba >> 32), cmdSdi->getDataDword1());
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
    container.initialize(neoDevice);

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
        container.initialize(neoDevice);

        NEO::Debugger::SbaAddresses sba = {};
        sba.SurfaceStateBaseAddress = 0x123456000;

        debugger->captureStateBaseAddress(container, sba);
        auto sizeUsed = container.getCommandStream()->getUsed();

        EXPECT_NE(0u, sizeUsed);
    }

    {
        NEO::CommandContainer container;
        container.initialize(neoDevice);

        NEO::Debugger::SbaAddresses sba = {};
        sba.GeneralStateBaseAddress = 0x123456000;

        debugger->captureStateBaseAddress(container, sba);
        auto sizeUsed = container.getCommandStream()->getUsed();

        EXPECT_NE(0u, sizeUsed);
    }

    {
        NEO::CommandContainer container;
        container.initialize(neoDevice);

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
        {neoDevice->getRootDeviceIndex(), 4096, NEO::GraphicsAllocation::AllocationType::KERNEL_ISA, neoDevice->getDeviceBitfield()});

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

static void printAttentionBitmask(uint8_t *expected, uint8_t *actual, uint32_t maxSlices, uint32_t maxSubSlicesPerSlice, uint32_t maxEuPerSubslice, uint32_t threadsPerEu, bool printBitmask = false) {
    auto bytesPerThread = threadsPerEu > 8 ? 2u : 1u;

    auto bytesPerSlice = maxSubSlicesPerSlice * maxEuPerSubslice * bytesPerThread;
    auto bytesPerSubSlice = maxEuPerSubslice * bytesPerThread;

    for (uint32_t slice = 0; slice < maxSlices; slice++) {
        for (uint32_t subslice = 0; subslice < maxSubSlicesPerSlice; subslice++) {
            for (uint32_t eu = 0; eu < maxEuPerSubslice; eu++) {
                for (uint32_t byte = 0; byte < bytesPerThread; byte++) {
                    if (printBitmask) {
                        std::bitset<8> bits(actual[slice * bytesPerSlice + subslice * bytesPerSubSlice + eu * bytesPerThread + byte]);
                        std::cout << " slice = " << slice << " subslice = " << subslice << " eu = " << eu << " threads bitmask = " << bits << "\n";
                    }

                    if (expected[slice * bytesPerSlice + subslice * bytesPerSubSlice + eu * bytesPerThread + byte] !=
                        actual[slice * bytesPerSlice + subslice * bytesPerSubSlice + eu * bytesPerThread + byte]) {
                        std::bitset<8> bits(actual[slice * bytesPerSlice + subslice * bytesPerSubSlice + eu * bytesPerThread + byte]);
                        std::bitset<8> bitsExpected(expected[slice * bytesPerSlice + subslice * bytesPerSubSlice + eu * bytesPerThread + byte]);
                        ASSERT_FALSE(true) << " got: slice = " << slice << " subslice = " << subslice << " eu = " << eu << " threads bitmask = " << bits << "\n"
                                           << " expected: slice = " << slice << " subslice = " << subslice << " eu = " << eu << " threads bitmask = " << bitsExpected << "\n";
                        ;
                    }
                }
            }
        }
    }

    if (printBitmask) {
        std::cout << "\n\n";
    }
}
TEST(DebuggerL0, givenSliceSubsliceEuAndThreadIdsWhenGettingBitmaskThenCorrectBitmaskIsReturned) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    std::unique_ptr<uint8_t[]> bitmask;
    size_t size = 0;

    uint32_t subslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    uint32_t subslice = subslicesPerSlice > 1 ? subslicesPerSlice - 1 : 0;

    const auto threadsPerEu = (hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount);
    const auto bytesPerEu = threadsPerEu <= 8 ? 1 : 2;

    const auto threadsSizePerSubSlice = hwInfo.gtSystemInfo.MaxEuPerSubSlice * bytesPerEu;
    const auto threadsSizePerSlice = threadsSizePerSubSlice * subslicesPerSlice;

    DebuggerL0::getAttentionBitmaskForThread(0, 0, 0, 6, hwInfo, bitmask, size);

    auto expectedBitmask = std::make_unique<uint8_t[]>(size);
    uint8_t *data = nullptr;
    memset(expectedBitmask.get(), 0, size);

    auto returnedBitmask = bitmask.get();
    EXPECT_EQ(uint8_t(1u << 6), returnedBitmask[0]);

    DebuggerL0::getAttentionBitmaskForThread(0, 0, 1, 3, hwInfo, bitmask, size);
    returnedBitmask = bitmask.get();
    returnedBitmask += bytesPerEu;
    EXPECT_EQ(uint8_t(1u << 3), returnedBitmask[0]);

    DebuggerL0::getAttentionBitmaskForThread(0, subslice, 3, 6, hwInfo, bitmask, size);

    data = expectedBitmask.get();
    memset(expectedBitmask.get(), 0, size);

    data = ptrOffset(data, subslice * threadsSizePerSubSlice);
    data = ptrOffset(data, 3 * bytesPerEu);
    data[0] = 1 << 6;

    printAttentionBitmask(expectedBitmask.get(), bitmask.get(), hwInfo.gtSystemInfo.MaxSlicesSupported, subslicesPerSlice, hwInfo.gtSystemInfo.MaxEuPerSubSlice, threadsPerEu);
    EXPECT_EQ(0, memcmp(bitmask.get(), expectedBitmask.get(), size));

    DebuggerL0::getAttentionBitmaskForThread(hwInfo.gtSystemInfo.MaxSlicesSupported - 1, subslice, 3, 6, hwInfo, bitmask, size);
    data = expectedBitmask.get();
    memset(expectedBitmask.get(), 0, size);

    data = ptrOffset(data, (hwInfo.gtSystemInfo.MaxSlicesSupported - 1) * threadsSizePerSlice);
    data = ptrOffset(data, subslice * threadsSizePerSubSlice);
    data = ptrOffset(data, 3 * bytesPerEu);
    data[0] = 1 << 6;

    printAttentionBitmask(expectedBitmask.get(), bitmask.get(), hwInfo.gtSystemInfo.MaxSlicesSupported, subslicesPerSlice, hwInfo.gtSystemInfo.MaxEuPerSubSlice, threadsPerEu);
    EXPECT_EQ(0, memcmp(bitmask.get(), expectedBitmask.get(), size));

    DebuggerL0::getAttentionBitmaskForThread(hwInfo.gtSystemInfo.MaxSlicesSupported - 1, subslice, 5, 0, hwInfo, bitmask, size);
    data = expectedBitmask.get();
    memset(expectedBitmask.get(), 0, size);

    data = ptrOffset(data, (hwInfo.gtSystemInfo.MaxSlicesSupported - 1) * threadsSizePerSlice);
    data = ptrOffset(data, subslice * threadsSizePerSubSlice);
    data = ptrOffset(data, 5 * bytesPerEu);
    data[0] = 1;

    printAttentionBitmask(expectedBitmask.get(), bitmask.get(), hwInfo.gtSystemInfo.MaxSlicesSupported, subslicesPerSlice, hwInfo.gtSystemInfo.MaxEuPerSubSlice, threadsPerEu);
    EXPECT_EQ(0, memcmp(bitmask.get(), expectedBitmask.get(), size));
}

TEST(DebuggerL0, givenAllSliceSubsliceEuAndThreadIdsWhenGettingBitmaskThenCorrectBitmaskIsReturned) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    std::unique_ptr<uint8_t[]> bitmask;
    size_t size = 0;

    uint32_t subslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    uint32_t subsliceID = subslicesPerSlice > 2 ? subslicesPerSlice - 2 : 0;

    const auto threadsPerEu = (hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount);
    const auto bytesPerEu = threadsPerEu <= 8 ? 1u : 2u;

    const auto threadsSizePerSubSlice = hwInfo.gtSystemInfo.MaxEuPerSubSlice * bytesPerEu;
    const auto threadsSizePerSlice = threadsSizePerSubSlice * subslicesPerSlice;

    const auto threadID = threadsPerEu - 1;

    // ALL slices
    DebuggerL0::getAttentionBitmaskForThread(UINT32_MAX, subsliceID, 0, threadID, hwInfo, bitmask, size);
    auto expectedBitmask = std::make_unique<uint8_t[]>(size);
    uint8_t *data = nullptr;

    memset(expectedBitmask.get(), 0, size);

    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.MaxSlicesSupported; i++) {
        data = ptrOffset(expectedBitmask.get(), i * threadsSizePerSlice);
        data = ptrOffset(data, subsliceID * threadsSizePerSubSlice);
        data[0] = 1 << threadID;
    }
    printAttentionBitmask(expectedBitmask.get(), bitmask.get(), hwInfo.gtSystemInfo.MaxSlicesSupported, subslicesPerSlice, hwInfo.gtSystemInfo.MaxEuPerSubSlice, threadsPerEu);
    EXPECT_EQ(0, memcmp(bitmask.get(), expectedBitmask.get(), size));

    // ALL Subslices
    DebuggerL0::getAttentionBitmaskForThread(hwInfo.gtSystemInfo.MaxSlicesSupported - 1, UINT32_MAX, 0, threadID, hwInfo, bitmask, size);
    memset(expectedBitmask.get(), 0, size);

    data = ptrOffset(expectedBitmask.get(), (hwInfo.gtSystemInfo.MaxSlicesSupported - 1) * threadsSizePerSlice);

    for (uint32_t i = 0; i < subslicesPerSlice; i++) {
        data[i * threadsSizePerSubSlice] = 1 << threadID;
    }
    printAttentionBitmask(expectedBitmask.get(), bitmask.get(), hwInfo.gtSystemInfo.MaxSlicesSupported, subslicesPerSlice, hwInfo.gtSystemInfo.MaxEuPerSubSlice, threadsPerEu);
    EXPECT_EQ(0, memcmp(bitmask.get(), expectedBitmask.get(), size));

    // ALL EUs
    DebuggerL0::getAttentionBitmaskForThread(hwInfo.gtSystemInfo.MaxSlicesSupported - 1, subsliceID, UINT32_MAX, threadID, hwInfo, bitmask, size);
    memset(expectedBitmask.get(), 0, size);

    data = ptrOffset(expectedBitmask.get(), (hwInfo.gtSystemInfo.MaxSlicesSupported - 1) * threadsSizePerSlice);
    data = ptrOffset(data, subsliceID * threadsSizePerSubSlice);

    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.MaxEuPerSubSlice; i++) {
        data[i * bytesPerEu] = 1 << threadID;
    }
    printAttentionBitmask(expectedBitmask.get(), bitmask.get(), hwInfo.gtSystemInfo.MaxSlicesSupported, subslicesPerSlice, hwInfo.gtSystemInfo.MaxEuPerSubSlice, threadsPerEu);
    EXPECT_EQ(0, memcmp(bitmask.get(), expectedBitmask.get(), size));

    // ALL threads
    DebuggerL0::getAttentionBitmaskForThread(hwInfo.gtSystemInfo.MaxSlicesSupported - 1, subsliceID, 1, UINT32_MAX, hwInfo, bitmask, size);
    memset(expectedBitmask.get(), 0, size);

    data = ptrOffset(expectedBitmask.get(), (hwInfo.gtSystemInfo.MaxSlicesSupported - 1) * threadsSizePerSlice);
    data = ptrOffset(data, subsliceID * threadsSizePerSubSlice);
    data = ptrOffset(data, 1 * bytesPerEu);

    for (uint32_t byte = 0; byte < bytesPerEu; byte++) {
        for (uint32_t i = 0; i < std::min(threadsPerEu, 8u); i++) {
            data[byte] |= 1 << i;
        }
    }
    printAttentionBitmask(expectedBitmask.get(), bitmask.get(), hwInfo.gtSystemInfo.MaxSlicesSupported, subslicesPerSlice, hwInfo.gtSystemInfo.MaxEuPerSubSlice, threadsPerEu);
    EXPECT_EQ(0, memcmp(bitmask.get(), expectedBitmask.get(), size));

    // ALL slices, All subslices, ALL EUs, ALL threads
    DebuggerL0::getAttentionBitmaskForThread(UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX, hwInfo, bitmask, size);

    auto expectedThreads = threadsPerEu == 7 ? 0x7f : 0xff;
    memset(expectedBitmask.get(), expectedThreads, size);
    EXPECT_EQ(0, memcmp(bitmask.get(), expectedBitmask.get(), size));

    // ALL slices, All subslices
    DebuggerL0::getAttentionBitmaskForThread(UINT32_MAX, UINT32_MAX, 0, 0, hwInfo, bitmask, size);
    memset(expectedBitmask.get(), 0, size);
    data = expectedBitmask.get();

    for (uint32_t slice = 0; slice < hwInfo.gtSystemInfo.MaxSlicesSupported; slice++) {
        for (uint32_t subslice = 0; subslice < subslicesPerSlice; subslice++) {
            data[slice * threadsSizePerSlice + subslice * threadsSizePerSubSlice] = 1;
        }
    }

    printAttentionBitmask(expectedBitmask.get(), bitmask.get(), hwInfo.gtSystemInfo.MaxSlicesSupported, subslicesPerSlice, hwInfo.gtSystemInfo.MaxEuPerSubSlice, threadsPerEu);
    EXPECT_EQ(0, memcmp(bitmask.get(), expectedBitmask.get(), size));

    // ALL slices, All subslices, ALL EUs
    DebuggerL0::getAttentionBitmaskForThread(UINT32_MAX, UINT32_MAX, UINT32_MAX, 0, hwInfo, bitmask, size);
    memset(expectedBitmask.get(), 0, size);
    data = expectedBitmask.get();

    for (uint32_t slice = 0; slice < hwInfo.gtSystemInfo.MaxSlicesSupported; slice++) {
        for (uint32_t subslice = 0; subslice < subslicesPerSlice; subslice++) {
            for (uint32_t eu = 0; eu < hwInfo.gtSystemInfo.MaxEuPerSubSlice; eu++) {
                data[slice * threadsSizePerSlice + subslice * threadsSizePerSubSlice + eu * bytesPerEu] = 1;
            }
        }
    }

    printAttentionBitmask(expectedBitmask.get(), bitmask.get(), hwInfo.gtSystemInfo.MaxSlicesSupported, subslicesPerSlice, hwInfo.gtSystemInfo.MaxEuPerSubSlice, threadsPerEu);
    EXPECT_EQ(0, memcmp(bitmask.get(), expectedBitmask.get(), size));
}

TEST(DebuggerL0, givenBitmaskWithAttentionBitsForSingleThreadWhenGettingThreadsThenSingleCorrectThreadReturned) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    std::unique_ptr<uint8_t[]> bitmask;
    size_t size = 0;

    uint32_t subslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    uint32_t subsliceID = subslicesPerSlice > 2 ? subslicesPerSlice - 2 : 0;

    uint32_t threadID = 3;
    DebuggerL0::getAttentionBitmaskForThread(0, subsliceID, 0, threadID, hwInfo, bitmask, size);

    auto threads = DebuggerL0::getThreadsFromAttentionBitmask(hwInfo, bitmask.get(), size);

    ASSERT_EQ(1u, threads.size());

    EXPECT_EQ(0u, threads[0].slice);
    EXPECT_EQ(subsliceID, threads[0].subslice);
    EXPECT_EQ(0u, threads[0].eu);
    EXPECT_EQ(threadID, threads[0].thread);
}

TEST(DebuggerL0, givenBitmaskWithAttentionBitsForAllSubslicesWhenGettingThreadsThenCorrectThreadsAreReturned) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    std::unique_ptr<uint8_t[]> bitmask;
    size_t size = 0;

    uint32_t subslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    uint32_t threadID = 3;
    DebuggerL0::getAttentionBitmaskForThread(0, UINT32_MAX, 0, threadID, hwInfo, bitmask, size);

    auto threads = DebuggerL0::getThreadsFromAttentionBitmask(hwInfo, bitmask.get(), size);

    ASSERT_EQ(subslicesPerSlice, threads.size());

    for (uint32_t i = 0; i < subslicesPerSlice; i++) {
        EXPECT_EQ(0u, threads[i].slice);
        EXPECT_EQ(i, threads[i].subslice);
        EXPECT_EQ(0u, threads[i].eu);
        EXPECT_EQ(threadID, threads[i].thread);
    }
}

TEST(DebuggerL0, givenBitmaskWithAttentionBitsForAllEUsWhenGettingThreadsThenCorrectThreadsAreReturned) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    std::unique_ptr<uint8_t[]> bitmask;
    size_t size = 0;

    uint32_t maxEUs = hwInfo.gtSystemInfo.MaxEuPerSubSlice;
    uint32_t threadID = 3;
    DebuggerL0::getAttentionBitmaskForThread(0, 0, UINT32_MAX, threadID, hwInfo, bitmask, size);

    auto threads = DebuggerL0::getThreadsFromAttentionBitmask(hwInfo, bitmask.get(), size);

    ASSERT_EQ(maxEUs, threads.size());

    for (uint32_t i = 0; i < maxEUs; i++) {
        EXPECT_EQ(0u, threads[i].slice);
        EXPECT_EQ(0u, threads[i].subslice);
        EXPECT_EQ(i, threads[i].eu);
        EXPECT_EQ(threadID, threads[i].thread);
    }
}

TEST(DebuggerL0, givenEu0To1Threads0To3BitmaskWhenGettingThreadsThenCorrectThreadsAreReturned) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    uint8_t data[2] = {0x0f, 0x0f};
    auto threads = DebuggerL0::getThreadsFromAttentionBitmask(hwInfo, data, sizeof(data));

    ASSERT_EQ(8u, threads.size());

    ze_device_thread_t expectedThreads[] = {
        {0, 0, 0, 0},
        {0, 0, 0, 1},
        {0, 0, 0, 2},
        {0, 0, 0, 3},
        {0, 0, 1, 0},
        {0, 0, 1, 1},
        {0, 0, 1, 2},
        {0, 0, 1, 3}};

    for (uint32_t i = 0; i < 8u; i++) {
        EXPECT_EQ(expectedThreads[i].slice, threads[i].slice);
        EXPECT_EQ(expectedThreads[i].subslice, threads[i].subslice);
        EXPECT_EQ(expectedThreads[i].eu, threads[i].eu);
        EXPECT_EQ(expectedThreads[i].thread, threads[i].thread);
    }
}

TEST(DebuggerL0, givenBitmaskWithAttentionBitsForHalfOfThreadsWhenGettingThreadsThenCorrectThreadsAreReturned) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    std::unique_ptr<uint8_t[]> bitmask;
    size_t size = 0;

    uint32_t subslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    uint32_t threadID = 3;
    DebuggerL0::getAttentionBitmaskForThread(0, UINT32_MAX, 0, threadID, hwInfo, bitmask, size);

    auto bitmaskSizePerSingleSubslice = size / hwInfo.gtSystemInfo.MaxSlicesSupported / subslicesPerSlice;
    auto numOfActiveSubslices = ((subslicesPerSlice + 1) / 2);

    auto threads = DebuggerL0::getThreadsFromAttentionBitmask(hwInfo, bitmask.get(), bitmaskSizePerSingleSubslice * numOfActiveSubslices);

    ASSERT_EQ(numOfActiveSubslices, threads.size());

    for (uint32_t i = 0; i < numOfActiveSubslices; i++) {
        EXPECT_EQ(0u, threads[i].slice);
        EXPECT_EQ(i, threads[i].subslice);
        EXPECT_EQ(0u, threads[i].eu);
        EXPECT_EQ(threadID, threads[i].thread);
    }
}

} // namespace ult
} // namespace L0
