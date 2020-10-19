/*
 * Copyright (C) 2020 Intel Corporation
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
#include "shared/test/unit_test/cmd_parse/gen_cmd_parse.h"
#include "shared/test/unit_test/helpers/variable_backup.h"

#include "test.h"

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

TEST_F(L0DebuggerTest, givenL0DebuggerWhenCallingIsDebuggerActiveThenTrueIsReturned) {
    EXPECT_TRUE(neoDevice->getDebugger()->isDebuggerActive());
}

HWTEST_F(L0DebuggerTest, givenL0DebuggerWhenCreatedThenPerContextSbaTrackingBuffersAreAllocated) {
    auto debugger = device->getL0Debugger();
    ASSERT_NE(nullptr, debugger);

    EXPECT_NE(0u, debugger->getSbaTrackingGpuVa());
    std::vector<NEO::GraphicsAllocation *> allocations;

    for (auto &engine : device->getNEODevice()->getEngines()) {
        auto sbaAllocation = debugger->getSbaTrackingBuffer(engine.osContext->getContextId());
        ASSERT_NE(nullptr, sbaAllocation);
        allocations.push_back(sbaAllocation);

        EXPECT_EQ(NEO::GraphicsAllocation::AllocationType::DEBUG_SBA_TRACKING_BUFFER, sbaAllocation->getAllocationType());
    }

    for (uint32_t i = 0; i < allocations.size() - 1; i++) {
        EXPECT_NE(allocations[i], allocations[i + 1]);
    }

    EXPECT_EQ(device->getNEODevice()->getEngines().size(), getMockDebuggerL0Hw<FamilyType>()->perContextSbaAllocations.size());
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

HWTEST_F(L0DebuggerTest, givenDebuggingEnabledWhenCommandListIsExecutedThenKernelDebugCommandsAreAdded) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using STATE_SIP = typename FamilyType::STATE_SIP;

    ze_command_queue_desc_t queueDesc = {};
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false));
    ASSERT_NE(nullptr, commandQueue->commandStream);

    auto usedSpaceBefore = commandQueue->commandStream->getUsed();

    ze_result_t returnValue;
    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue)->toHandle()};
    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));

    auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_LE(2u, miLoadImm.size());

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
    EXPECT_EQ(1u, debugModeRegisterCount);
    EXPECT_EQ(1u, tdDebugControlRegisterCount);

    auto stateSipCmds = findAll<STATE_SIP *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(1u, stateSipCmds.size());

    STATE_SIP *stateSip = genCmdCast<STATE_SIP *>(*stateSipCmds[0]);

    auto systemRoutine = SipKernel::getSipKernelAllocation(*neoDevice);
    ASSERT_NE(nullptr, systemRoutine);
    EXPECT_EQ(systemRoutine->getGpuAddress(), stateSip->getSystemInstructionPointer());

    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(commandLists[i]);
        commandList->destroy();
    }

    commandQueue->destroy();
}

HWTEST_F(L0DebuggerTest, givenDebuggingEnabledWhenCommandListIsExecutedTwiceThenKernelDebugCommandsAreAddedOnlyOnce) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using STATE_SIP = typename FamilyType::STATE_SIP;

    ze_command_queue_desc_t queueDesc = {};
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false));
    ASSERT_NE(nullptr, commandQueue->commandStream);

    auto usedSpaceBefore = commandQueue->commandStream->getUsed();

    ze_result_t returnValue;
    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue)->toHandle()};
    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    size_t debugModeRegisterCount = 0;
    size_t tdDebugControlRegisterCount = 0;

    {

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
            cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));

        auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());

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
        EXPECT_EQ(1u, debugModeRegisterCount);
        EXPECT_EQ(1u, tdDebugControlRegisterCount);
    }
    result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter2 = commandQueue->commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter2, usedSpaceAfter);

    {
        GenCmdList cmdList2;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
            cmdList2, ptrOffset(commandQueue->commandStream->getCpuBase(), usedSpaceAfter), usedSpaceAfter2 - usedSpaceAfter));

        auto miLoadImm2 = findAll<MI_LOAD_REGISTER_IMM *>(cmdList2.begin(), cmdList2.end());

        for (size_t i = 0; i < miLoadImm2.size(); i++) {
            MI_LOAD_REGISTER_IMM *miLoad = genCmdCast<MI_LOAD_REGISTER_IMM *>(*miLoadImm2[i]);
            ASSERT_NE(nullptr, miLoad);

            if (miLoad->getRegisterOffset() == DebugModeRegisterOffset<FamilyType>::registerOffset) {
                debugModeRegisterCount++;
            }
            if (miLoad->getRegisterOffset() == TdDebugControlRegisterOffset<FamilyType>::registerOffset) {
                tdDebugControlRegisterCount++;
            }
        }

        EXPECT_EQ(1u, debugModeRegisterCount);
        EXPECT_EQ(1u, tdDebugControlRegisterCount);
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
    auto cmdQ = CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false);
    ASSERT_NE(nullptr, cmdQ);

    auto commandQueue = whitebox_cast(cmdQ);
    auto cmdQHw = static_cast<CommandQueueHw<gfxCoreFamily> *>(cmdQ);

    if (cmdQHw->estimateStateBaseAddressCmdSize() == 0) {
        commandQueue->destroy();
        GTEST_SKIP();
    }

    commandQueue->commandQueuePerThreadScratchSize = 4096;

    auto usedSpaceBefore = commandQueue->commandStream->getUsed();

    ze_result_t returnValue;
    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue)->toHandle()};
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

HWTEST_F(L0DebuggerTest, givenDebuggingEnabledAndPrintDebugMessagesWhenCommandQueueIsSynchronizedThenSbaAddressesArePrinted) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.PrintDebugMessages.set(1);

    testing::internal::CaptureStdout();

    ze_command_queue_desc_t queueDesc = {};
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false));
    ASSERT_NE(nullptr, commandQueue->commandStream);

    ze_result_t returnValue;
    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue)->toHandle()};
    const uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    commandQueue->synchronize(0);

    std::string output = testing::internal::GetCapturedStdout();
    size_t pos = output.find("Debugger: SBA stored ssh");
    EXPECT_NE(std::string::npos, pos);

    pos = output.find("Debugger: SBA ssh");
    EXPECT_NE(std::string::npos, pos);

    auto commandList = CommandList::fromHandle(commandLists[0]);
    commandList->destroy();

    commandQueue->destroy();
}

using L0DebuggerSimpleTest = Test<DeviceFixture>;

HWTEST_F(L0DebuggerSimpleTest, givenNullL0DebuggerAndPrintDebugMessagesWhenCommandQueueIsSynchronizedThenSbaAddressesAreNotPrinted) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.PrintDebugMessages.set(1);

    EXPECT_EQ(nullptr, device->getL0Debugger());
    testing::internal::CaptureStdout();

    ze_command_queue_desc_t queueDesc = {};
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false));
    ASSERT_NE(nullptr, commandQueue->commandStream);

    ze_result_t returnValue;
    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue)->toHandle()};
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

HWTEST_F(L0DebuggerTest, givenL0DebuggerAndPrintDebugMessagesSetToFalseWhenCommandQueueIsSynchronizedThenSbaAddressesAreNotPrinted) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.PrintDebugMessages.set(0);

    EXPECT_NE(nullptr, device->getL0Debugger());
    testing::internal::CaptureStdout();

    ze_command_queue_desc_t queueDesc = {};
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false));
    ASSERT_NE(nullptr, commandQueue->commandStream);

    ze_result_t returnValue;
    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue)->toHandle()};
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
    ze_command_list_handle_t commandListHandle = CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue)->toHandle();
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
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue));
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
    commandQueue->initialize(false);

    ze_result_t returnValue;
    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue)->toHandle()};
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

HWTEST_F(L0DebuggerSimpleTest, givenNonZeroGpuVasWhenProgrammingSbaTrackingThenCorrectCmdsAreAddedToStream) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(neoDevice);

    debugger->sbaTrackingGpuVa.address = 0x45670000;
    auto expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(SbaTrackedAddresses, GeneralStateBaseAddress);

    StackVec<char, 4096> buffer(4096);
    NEO::LinearStream cmdStream(buffer.begin(), buffer.size());
    uint64_t gsba = 0x60000;
    uint64_t ssba = 0x1234567000;

    NEO::Debugger::SbaAddresses sbaAddresses = {};
    sbaAddresses.GeneralStateBaseAddress = gsba;
    sbaAddresses.SurfaceStateBaseAddress = ssba;

    debugger->programSbaTrackingCommands(cmdStream, sbaAddresses);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, cmdStream.getCpuBase(), cmdStream.getUsed()));

    EXPECT_EQ(2 * sizeof(MI_STORE_DATA_IMM), cmdStream.getUsed());

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

    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(neoDevice);
    auto debugArea = debugger->getModuleDebugArea();

    auto allocation = neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {neoDevice->getRootDeviceIndex(), 4096, NEO::GraphicsAllocation::AllocationType::KERNEL_ISA, neoDevice->getDeviceBitfield()});

    EXPECT_EQ(allocation->storageInfo.getMemoryBanks(), debugArea->storageInfo.getMemoryBanks());

    DebugAreaHeader *header = reinterpret_cast<DebugAreaHeader *>(debugArea->getUnderlyingBuffer());
    EXPECT_EQ(1u, header->pgsize);
    EXPECT_EQ(0u, header->isShared);

    EXPECT_STREQ("dbgarea", header->magic);
    EXPECT_EQ(sizeof(DebugAreaHeader), header->size);
    EXPECT_EQ(sizeof(DebugAreaHeader), header->scratchBegin);
    EXPECT_EQ(MemoryConstants::pageSize64k - sizeof(DebugAreaHeader), header->scratchEnd);

    neoDevice->getMemoryManager()->freeGraphicsMemory(allocation);
}

} // namespace ult
} // namespace L0
