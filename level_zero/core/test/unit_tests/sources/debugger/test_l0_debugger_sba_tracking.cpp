/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen_common/reg_configs_common.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/mocks/mock_gmm_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/sources/debugger/l0_debugger_fixture.h"

namespace L0 {
namespace ult {

struct PerContextAddressSpaceFixture : public Test<DeviceFixture> {
    void SetUp() override {
        NEO::DebugManager.flags.DebuggerForceSbaTrackingMode.set(0);
        Test<DeviceFixture>::SetUp();
    }

    void TearDown() override {
        Test<DeviceFixture>::TearDown();
    }

    DebugManagerStateRestore restorer;
};

using L0DebuggerPerContextAddressSpaceTest = Test<L0DebuggerPerContextAddressSpaceFixture>;

using L0DebuggerTest = Test<L0DebuggerHwFixture>;

using L0DebuggerParameterizedTests = L0DebuggerHwParameterizedFixture;

HWTEST_P(L0DebuggerParameterizedTests, givenL0DebuggerWhenCreatedThenPerContextSbaTrackingBuffersAreAllocated) {
    auto debugger = device->getL0Debugger();
    ASSERT_NE(nullptr, debugger);

    if (!debugger->getSingleAddressSpaceSbaTracking()) {
        EXPECT_NE(0u, debugger->getSbaTrackingGpuVa());
    }
    std::vector<NEO::GraphicsAllocation *> allocations;

    auto &allEngines = neoDevice->getMemoryManager()->getRegisteredEngines();

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

        auto sbaHeader = reinterpret_cast<NEO::SbaTrackedAddresses *>(sbaAllocation->getUnderlyingBuffer());

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

HWTEST_P(L0DebuggerParameterizedTests, givenL0DebuggerWhenCreatedThenPerContextSbaTrackingBuffersAreAllocatedWithProperStorageInfo) {
    auto debugger = device->getL0Debugger();
    ASSERT_NE(nullptr, debugger);

    if (!debugger->getSingleAddressSpaceSbaTracking()) {
        EXPECT_NE(0u, debugger->getSbaTrackingGpuVa());
    }
    std::vector<NEO::GraphicsAllocation *> allocations;

    for (auto &engine : device->getNEODevice()->getAllEngines()) {
        auto sbaAllocation = debugger->getSbaTrackingBuffer(engine.osContext->getContextId());
        ASSERT_NE(nullptr, sbaAllocation);
        allocations.push_back(sbaAllocation);

        EXPECT_EQ(NEO::AllocationType::DEBUG_SBA_TRACKING_BUFFER, sbaAllocation->getAllocationType());
        if (sbaAllocation->isAllocatedInLocalMemoryPool()) {
            EXPECT_EQ(neoDevice->getDeviceBitfield(), sbaAllocation->storageInfo.pageTablesVisibility);
        }
        EXPECT_FALSE(sbaAllocation->storageInfo.cloningOfPageTables);
        EXPECT_FALSE(sbaAllocation->storageInfo.multiStorage);
        EXPECT_FALSE(sbaAllocation->storageInfo.tileInstanced);
        EXPECT_GE(neoDevice->getDeviceBitfield().to_ullong(), sbaAllocation->storageInfo.getMemoryBanks());
        EXPECT_EQ(1u, sbaAllocation->storageInfo.getNumBanks());
    }
}

using NotGen8Or11 = AreNotGfxCores<IGFX_GEN8_CORE, IGFX_GEN11_CORE>;
using Gen12Plus = IsAtLeastGfxCore<IGFX_GEN12_CORE>;

HWTEST2_F(L0DebuggerPerContextAddressSpaceTest, givenDebuggingEnabledAndRequiredGsbaWhenCommandListIsExecutedThenProgramGsbaWritesToSbaTrackingBuffer, NotGen8Or11) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto cmdQ = CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue);
    ASSERT_NE(nullptr, cmdQ);

    auto commandQueue = whiteboxCast(cmdQ);
    auto cmdQHw = static_cast<CommandQueueHw<gfxCoreFamily> *>(cmdQ);

    if (cmdQHw->estimateStateBaseAddressCmdSize() == 0) {
        commandQueue->destroy();
        GTEST_SKIP();
    }

    auto usedSpaceBefore = commandQueue->commandStream.getUsed();

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle()};
    CommandList::fromHandle(commandLists[0])->setCommandListPerThreadScratchSize(4096);

    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream.getCpuBase(), 0), usedSpaceAfter));

    auto sbaItor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), sbaItor);
    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*sbaItor);

    auto sdiItor = find<MI_STORE_DATA_IMM *>(sbaItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);
    auto cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    auto gmmHelper = neoDevice->getGmmHelper();

    uint64_t gsbaGpuVa = gmmHelper->canonize(cmdSba->getGeneralStateBaseAddress());
    EXPECT_EQ(static_cast<uint32_t>(gsbaGpuVa & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(gsbaGpuVa >> 32), cmdSdi->getDataDword1());

    auto expectedGpuVa = gmmHelper->decanonize(device->getL0Debugger()->getSbaTrackingGpuVa()) + offsetof(NEO::SbaTrackedAddresses, GeneralStateBaseAddress);
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());

    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(commandLists[i]);
        commandList->destroy();
    }
    commandQueue->destroy();
}

HWTEST2_F(L0DebuggerTest, givenDebuggingEnabledAndDebuggerLogsWhenCommandQueueIsSynchronizedThenSbaAddressesArePrinted, Gen12Plus) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.DebuggerLogBitmask.set(255);

    testing::internal::CaptureStdout();

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue);

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

HWTEST2_F(L0DebuggerSimpleTest, givenNullL0DebuggerAndDebuggerLogsWhenCommandQueueIsSynchronizedThenSbaAddressesAreNotPrinted, Gen12Plus) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.DebuggerLogBitmask.set(255);

    EXPECT_EQ(nullptr, device->getL0Debugger());
    testing::internal::CaptureStdout();

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue);

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

HWTEST2_F(L0DebuggerTest, givenL0DebuggerAndDebuggerLogsDisabledWhenCommandQueueIsSynchronizedThenSbaAddressesAreNotPrinted, Gen12Plus) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.DebuggerLogBitmask.set(0);

    EXPECT_NE(nullptr, device->getL0Debugger());
    testing::internal::CaptureStdout();

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue);

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

HWTEST2_F(L0DebuggerTest, givenDebuggingEnabledWhenNonCopyCommandListIsInititalizedOrResetThenSSHAddressIsTracked, Gen12Plus) {
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

HWTEST2_F(L0DebuggerTest, givenDebuggingEnabledWhenCommandListIsExecutedThenSbaBufferIsPushedToResidencyContainer, Gen12Plus) {
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

HWTEST2_F(L0DebuggerTest, givenDebugerEnabledWhenPrepareAndSubmitBatchBufferThenLeftoverIsZeroed, Gen12Plus) {
    ze_command_queue_desc_t queueDesc = {};
    std::unique_ptr<MockCommandQueueHw<gfxCoreFamily>, Deleter> commandQueue(new MockCommandQueueHw<gfxCoreFamily>(device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc));
    commandQueue->initialize(false, false);

    auto &commandStream = commandQueue->commandStream;
    auto estimatedSize = 4096u;
    NEO::LinearStream linearStream(commandStream.getSpace(estimatedSize), estimatedSize);
    // fill with random data
    memset(commandStream.getCpuBase(), 0xD, estimatedSize);

    typename MockCommandQueueHw<gfxCoreFamily>::CommandListExecutionContext ctx{};
    ctx.isDebugEnabled = true;

    commandQueue->prepareAndSubmitBatchBuffer(ctx, linearStream);

    // MI_BATCH_BUFFER END is added during prepareAndSubmitBatchBuffer
    auto offsetInBytes = sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    auto isLeftoverZeroed = true;
    for (auto i = offsetInBytes; i < estimatedSize; i++) {
        uint8_t *data = reinterpret_cast<uint8_t *>(commandStream.getCpuBase());
        if (data[i] != 0) {
            isLeftoverZeroed = false;
            break;
        }
    }
    EXPECT_TRUE(isLeftoverZeroed);
}

INSTANTIATE_TEST_CASE_P(SBAModesForDebugger, L0DebuggerParameterizedTests, ::testing::Values(0, 1));

struct L0DebuggerSingleAddressSpace : public Test<L0DebuggerHwFixture> {
    void SetUp() override {
        NEO::DebugManager.flags.DebuggerForceSbaTrackingMode.set(1);
        Test<L0DebuggerHwFixture>::SetUp();
    }

    void TearDown() override {
        Test<L0DebuggerHwFixture>::TearDown();
    }

    DebugManagerStateRestore restorer;
};

HWTEST2_F(L0DebuggerSingleAddressSpace, givenDebuggingEnabledWhenCommandListIsExecutedThenValidKernelDebugCommandsAreAdded, IsAtLeastGen12lp) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using STATE_SIP = typename FamilyType::STATE_SIP;

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue);

    auto usedSpaceBefore = commandQueue->commandStream.getUsed();

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle()};
    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream.getCpuBase(), 0), usedSpaceAfter));

    auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());

    size_t gpr15RegisterCount = 0;

    size_t gprMiLoadindex = std::numeric_limits<size_t>::max();

    for (size_t i = 0; i < miLoadImm.size(); i++) {
        MI_LOAD_REGISTER_IMM *miLoad = genCmdCast<MI_LOAD_REGISTER_IMM *>(*miLoadImm[i]);
        ASSERT_NE(nullptr, miLoad);

        if (miLoad->getRegisterOffset() == CS_GPR_R15) {
            gpr15RegisterCount++;
            gprMiLoadindex = i;
        }
        if (miLoad->getRegisterOffset() == CS_GPR_R15 + 4) {
            gpr15RegisterCount++;
        }
    }

    // 2 LRI commands to store SBA buffer address
    EXPECT_EQ(2u, gpr15RegisterCount);

    auto sbaGpuVa = getMockDebuggerL0Hw<FamilyType>()->getSbaTrackingBuffer(commandQueue->getCsr()->getOsContext().getContextId())->getGpuAddress();
    uint32_t low = sbaGpuVa & 0xffffffff;
    uint32_t high = (sbaGpuVa >> 32) & 0xffffffff;

    MI_LOAD_REGISTER_IMM *miLoad = genCmdCast<MI_LOAD_REGISTER_IMM *>(*miLoadImm[gprMiLoadindex]);
    EXPECT_EQ(CS_GPR_R15, miLoad->getRegisterOffset());
    EXPECT_EQ(low, miLoad->getDataDword());

    miLoad = genCmdCast<MI_LOAD_REGISTER_IMM *>(*miLoadImm[gprMiLoadindex + 1]);
    EXPECT_EQ(CS_GPR_R15 + 4, miLoad->getRegisterOffset());
    EXPECT_EQ(high, miLoad->getDataDword());

    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(commandLists[i]);
        commandList->destroy();
    }

    commandQueue->destroy();
}

} // namespace ult
} // namespace L0
