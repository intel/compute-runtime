/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/gen_common/reg_configs/reg_configs_common.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/unit_test/cmd_parse/gen_cmd_parse.h"

#include "test.h"

#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/sources/debugger/l0_debugger_fixture.h"

namespace L0 {
namespace ult {

using L0DebuggerTest = Test<MockL0DebuggerFixture>;

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

TEST_F(L0DebuggerTest, givenL0DebuggerWhenCreatedThenPerContextSbaTrackingBuffersAreAllocated) {
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

    EXPECT_EQ(device->getNEODevice()->getEngines().size(), mockDebugger->perContextSbaAllocations.size());
}

TEST_F(L0DebuggerTest, givenCreatedL0DebuggerThenSbaTrackingBuffersContainValidHeader) {
    auto debugger = device->getL0Debugger();
    ASSERT_NE(nullptr, debugger);

    for (auto &sbaBuffer : mockDebugger->perContextSbaAllocations) {
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

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, false)->toHandle()};
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
        if (miLoad->getRegisterOffset() == TdDebugControlRegisterOffset::registerOffset) {
            EXPECT_EQ(TdDebugControlRegisterOffset::debugEnabledValue, miLoad->getDataDword());
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

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, false)->toHandle()};
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
            if (miLoad->getRegisterOffset() == TdDebugControlRegisterOffset::registerOffset) {
                EXPECT_EQ(TdDebugControlRegisterOffset::debugEnabledValue, miLoad->getDataDword());
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
            if (miLoad->getRegisterOffset() == TdDebugControlRegisterOffset::registerOffset) {
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

HWTEST_F(L0DebuggerTest, givenDebuggerWhenAppendingKernelToCommandListThenBindlessSurfaceStateForDebugSurfaceIsProgrammedAtOffsetZero) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    Mock<::L0::Kernel> kernel;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, false));
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
    EXPECT_EQ(RENDER_SURFACE_STATE::COHERENCY_TYPE_IA_COHERENT, debugSurfaceState->getCoherencyType());
}

} // namespace ult
} // namespace L0
