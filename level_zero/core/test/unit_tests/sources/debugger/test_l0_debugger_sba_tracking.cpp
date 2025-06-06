/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen_common/reg_configs_common.h"
#include "shared/source/helpers/bindless_heaps_helper.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/mocks/mock_gmm_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/cmdlist/cmdlist_launch_params.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"
#include "level_zero/core/test/unit_tests/sources/debugger/l0_debugger_fixture.h"

namespace L0 {
namespace ult {

struct PerContextAddressSpaceFixture : public Test<DeviceFixture> {
    void SetUp() override {
        NEO::debugManager.flags.DebuggerForceSbaTrackingMode.set(0);
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

    auto &allEngines = neoDevice->getMemoryManager()->getRegisteredEngines(neoDevice->getRootDeviceIndex());

    for (auto &engine : allEngines) {
        auto sbaAllocation = debugger->getSbaTrackingBuffer(engine.osContext->getContextId());
        ASSERT_NE(nullptr, sbaAllocation);
        allocations.push_back(sbaAllocation);

        EXPECT_EQ(NEO::AllocationType::debugSbaTrackingBuffer, sbaAllocation->getAllocationType());
        EXPECT_EQ(MemoryPool::system4KBPages, sbaAllocation->getMemoryPool());
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
        EXPECT_EQ(0u, sbaHeader->bindlessSamplerStateBaseAddress);
        EXPECT_EQ(0u, sbaHeader->bindlessSurfaceStateBaseAddress);
        EXPECT_EQ(0u, sbaHeader->dynamicStateBaseAddress);
        EXPECT_EQ(0u, sbaHeader->generalStateBaseAddress);
        EXPECT_EQ(0u, sbaHeader->indirectObjectBaseAddress);
        EXPECT_EQ(0u, sbaHeader->instructionBaseAddress);
        EXPECT_EQ(0u, sbaHeader->surfaceStateBaseAddress);
        EXPECT_EQ(0u, sbaHeader->version);
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

        EXPECT_EQ(NEO::AllocationType::debugSbaTrackingBuffer, sbaAllocation->getAllocationType());
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

using Gen12Plus = IsAtLeastGfxCore<IGFX_GEN12_CORE>;

HWTEST_F(L0DebuggerPerContextAddressSpaceTest, givenDebuggingEnabledAndRequiredGsbaWhenCommandListIsExecutedThenProgramGsbaWritesToSbaTrackingBuffer) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto cmdQ = CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, false, returnValue);
    ASSERT_NE(nullptr, cmdQ);

    auto commandQueue = whiteboxCast(cmdQ);
    auto cmdQHw = static_cast<CommandQueueHw<FamilyType::gfxCoreFamily> *>(cmdQ);

    if (cmdQHw->estimateStateBaseAddressCmdSize() == 0) {
        commandQueue->destroy();
        GTEST_SKIP();
    }

    auto usedSpaceBefore = commandQueue->commandStream.getUsed();

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)->toHandle()};
    CommandList::fromHandle(commandLists[0])->setCommandListPerThreadScratchSize(0u, 4096);
    CommandList::fromHandle(commandLists[0])->close();

    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true, nullptr, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
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

    auto expectedGpuVa = gmmHelper->decanonize(device->getL0Debugger()->getSbaTrackingGpuVa()) + offsetof(NEO::SbaTrackedAddresses, generalStateBaseAddress);
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());

    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(commandLists[i]);
        commandList->destroy();
    }
    commandQueue->destroy();
}

using L0DebuggerPerContextAddressSpaceGlobalBindlessTest = Test<L0DebuggerPerContextAddressSpaceGlobalBindlessFixture>;
using PlatformsSupportingGlobalBindless = IsWithinGfxCore<IGFX_XE_HP_CORE, IGFX_XE2_HPG_CORE>;

HWTEST2_F(L0DebuggerPerContextAddressSpaceGlobalBindlessTest, givenDebuggingEnabledAndRequiredSshWhenCommandListIsExecutedThenProgramSsbaWritesToSbaTrackingBuffer, PlatformsSupportingGlobalBindless) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto cmdQ = CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, false, returnValue);
    ASSERT_NE(nullptr, cmdQ);

    auto commandQueue = whiteboxCast(cmdQ);
    auto usedSpaceBefore = commandQueue->commandStream.getUsed();

    auto commandList = CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false);
    ze_command_list_handle_t commandLists[] = {commandList->toHandle()};

    Mock<Module> module(device, nullptr, ModuleType::user);
    Mock<KernelImp> kernel;
    kernel.module = &module;
    ze_group_count_t groupCount{1, 1, 1};

    kernel.descriptor.kernelAttributes.perThreadScratchSize[0] = 0x40;

    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    CommandList::fromHandle(commandLists[0])->close();

    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true, nullptr, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    auto sbaItors = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, sbaItors.size());

    auto sbaItor = sbaItors[sbaItors.size() - 1];

    ASSERT_NE(cmdList.end(), sbaItor);
    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*sbaItor);

    auto sdiItors = findAll<MI_STORE_DATA_IMM *>(sbaItor, cmdList.end());
    ASSERT_NE(0u, sdiItors.size());

    auto cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItors[0]);

    auto gmmHelper = neoDevice->getGmmHelper();
    auto expectedSshGpuVa = commandList->getCmdContainer().getIndirectHeap(HeapType::surfaceState)->getGpuBase();

    for (size_t i = 0; i < sdiItors.size(); i++) {
        cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItors[i]);
        uint64_t address = cmdSdi->getDataDword1();
        address <<= 32;
        address = address | cmdSdi->getDataDword0();
        if (expectedSshGpuVa == address) {
            break;
        }
        cmdSdi = nullptr;
    }

    ASSERT_NE(nullptr, cmdSdi);
    uint64_t ssbaGpuVa = gmmHelper->canonize(cmdSba->getSurfaceStateBaseAddress());
    EXPECT_EQ(static_cast<uint32_t>(ssbaGpuVa & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(ssbaGpuVa >> 32), cmdSdi->getDataDword1());

    auto expectedGpuVa = gmmHelper->decanonize(device->getL0Debugger()->getSbaTrackingGpuVa()) + offsetof(NEO::SbaTrackedAddresses, surfaceStateBaseAddress);
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());

    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(commandLists[i]);
        commandList->destroy();
    }
    commandQueue->destroy();
}

HWTEST2_F(L0DebuggerTest, givenDebuggingEnabledAndDebuggerLogsWhenCommandQueueIsSynchronizedThenSbaAddressesArePrinted, Gen12Plus) {

    auto &compilerProductHelper = neoDevice->getCompilerProductHelper();
    auto isHeaplessEnabled = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo);
    if (isHeaplessEnabled) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DebuggerLogBitmask.set(255);

    StreamCapture capture;
    capture.captureStdout();

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue);

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)->toHandle()};
    const uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);
    auto commandList = CommandList::fromHandle(commandLists[0]);
    commandList->close();

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    commandQueue->synchronize(0);

    std::string output = capture.getCapturedStdout();
    size_t pos = output.find("INFO: Debugger: SBA stored ssh");
    EXPECT_NE(std::string::npos, pos);

    pos = output.find("Debugger: SBA ssh");
    EXPECT_NE(std::string::npos, pos);

    commandList->destroy();

    commandQueue->destroy();
}

using L0DebuggerSimpleTest = Test<DeviceFixture>;

HWTEST2_F(L0DebuggerSimpleTest, givenNullL0DebuggerAndDebuggerLogsWhenCommandQueueIsSynchronizedThenSbaAddressesAreNotPrinted, Gen12Plus) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DebuggerLogBitmask.set(255);

    EXPECT_EQ(nullptr, device->getL0Debugger());
    StreamCapture capture;
    capture.captureStdout();

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue);

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)->toHandle()};
    const uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);
    auto commandList = CommandList::fromHandle(commandLists[0]);
    commandList->close();

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    commandQueue->synchronize(0);

    std::string output = capture.getCapturedStdout();
    size_t pos = output.find("Debugger: SBA");
    EXPECT_EQ(std::string::npos, pos);

    commandList->destroy();

    commandQueue->destroy();
}

HWTEST2_F(L0DebuggerTest, givenL0DebuggerAndDebuggerLogsDisabledWhenCommandQueueIsSynchronizedThenSbaAddressesAreNotPrinted, Gen12Plus) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DebuggerLogBitmask.set(0);

    EXPECT_NE(nullptr, device->getL0Debugger());
    StreamCapture capture;
    capture.captureStdout();

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue);

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)->toHandle()};
    const uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);
    auto commandList = CommandList::fromHandle(commandLists[0]);
    commandList->close();

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    commandQueue->synchronize(0);

    std::string output = capture.getCapturedStdout();
    size_t pos = output.find("Debugger: SBA");
    EXPECT_EQ(std::string::npos, pos);

    commandList->destroy();

    commandQueue->destroy();
}

HWTEST2_F(L0DebuggerTest, givenDebuggingEnabledWhenNonCopyCommandListIsInititalizedOrResetThenSSHAddressIsTracked, Gen12Plus) {

    auto &compilerProductHelper = neoDevice->getCompilerProductHelper();
    auto isHeaplessEnabled = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo);
    if (isHeaplessEnabled) {
        GTEST_SKIP();
    }

    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableStateBaseAddressTracking.set(0);
    debugManager.flags.DispatchCmdlistCmdBufferPrimary.set(0);
    debugManager.flags.SelectCmdListHeapAddressModel.set(0);
    debugManager.flags.UseExternalAllocatorForSshAndDsh.set(0);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset();

    size_t usedSpaceBefore = 0;
    ze_result_t returnValue;
    ze_command_list_handle_t commandListHandle = CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)->toHandle();
    auto commandList = CommandList::fromHandle(commandListHandle);

    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, commandList->getCmdContainer().getCommandStream()->getCpuBase(), usedSpaceAfter));

    auto sbaItor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), sbaItor);
    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*sbaItor);

    uint64_t sshGpuVa = cmdSba->getSurfaceStateBaseAddress();
    auto expectedGpuVa = commandList->getCmdContainer().getIndirectHeap(NEO::HeapType::surfaceState)->getHeapGpuBase();
    EXPECT_EQ(expectedGpuVa, sshGpuVa);
    EXPECT_EQ(1u, getMockDebuggerL0Hw<FamilyType>()->captureStateBaseAddressCount);

    auto bbStartList = findAll<MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());
    for (const auto &bbStartIt : bbStartList) {
        auto bbStartCmd = reinterpret_cast<MI_BATCH_BUFFER_START *>(*bbStartIt);
        EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, bbStartCmd->getSecondLevelBatchBuffer());
    }

    commandList->reset();
    EXPECT_EQ(2u, getMockDebuggerL0Hw<FamilyType>()->captureStateBaseAddressCount);

    commandList->destroy();

    debugManager.flags.DispatchCmdlistCmdBufferPrimary.set(1);
    commandListHandle = CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)->toHandle();
    commandList = CommandList::fromHandle(commandListHandle);

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        commandList->getCmdContainer().getCommandStream()->getCpuBase(),
        commandList->getCmdContainer().getCommandStream()->getUsed()));

    bbStartList = findAll<MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());
    for (const auto &bbStartIt : bbStartList) {
        auto bbStartCmd = reinterpret_cast<MI_BATCH_BUFFER_START *>(*bbStartIt);
        EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER_FIRST_LEVEL_BATCH, bbStartCmd->getSecondLevelBatchBuffer());
    }

    commandList->destroy();
}

HWTEST2_F(L0DebuggerTest, givenDebuggingEnabledWhenCommandListIsExecutedThenSbaBufferIsPushedToResidencyContainer, Gen12Plus) {
    ze_command_queue_desc_t queueDesc = {};

    std::unique_ptr<MockCommandQueueHw<FamilyType::gfxCoreFamily>, Deleter> commandQueue(new MockCommandQueueHw<FamilyType::gfxCoreFamily>(device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc));
    commandQueue->initialize(false, false, false);

    ze_result_t returnValue;
    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)->toHandle()};
    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);
    auto commandList = CommandList::fromHandle(commandLists[0]);
    commandList->close();

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto sbaBuffer = device->getL0Debugger()->getSbaTrackingBuffer(neoDevice->getDefaultEngine().commandStreamReceiver->getOsContext().getContextId());
    bool sbaFound = false;

    for (auto iter : commandQueue->residencyContainerSnapshot) {
        if (iter == sbaBuffer) {
            sbaFound = true;
        }
    }
    EXPECT_TRUE(sbaFound);

    commandList->destroy();
}

HWTEST2_F(L0DebuggerTest, givenDebugerEnabledWhenPrepareAndSubmitBatchBufferThenLeftoverIsZeroed, Gen12Plus) {
    ze_command_queue_desc_t queueDesc = {};
    std::unique_ptr<MockCommandQueueHw<FamilyType::gfxCoreFamily>, Deleter> commandQueue(new MockCommandQueueHw<FamilyType::gfxCoreFamily>(device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc));
    commandQueue->initialize(false, false, false);

    auto &commandStream = commandQueue->commandStream;
    auto estimatedSize = 4096u;
    NEO::LinearStream linearStream(commandStream.getSpace(estimatedSize), estimatedSize);
    // fill with random data
    memset(commandStream.getCpuBase(), 0xD, estimatedSize);

    typename MockCommandQueueHw<FamilyType::gfxCoreFamily>::CommandListExecutionContext ctx{};
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

INSTANTIATE_TEST_SUITE_P(SBAModesForDebugger, L0DebuggerParameterizedTests, ::testing::Values(0, 1));

struct L0DebuggerSingleAddressSpace : public Test<L0DebuggerHwFixture> {
    void SetUp() override {
        NEO::debugManager.flags.DebuggerForceSbaTrackingMode.set(1);
        Test<L0DebuggerHwFixture>::SetUp();
    }

    void TearDown() override {
        Test<L0DebuggerHwFixture>::TearDown();
    }

    DebugManagerStateRestore restorer;
};

HWTEST_F(L0DebuggerSingleAddressSpace, givenDebuggingEnabledWhenCommandListIsExecutedThenValidKernelDebugCommandsAreAdded) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue);
    if (commandQueue->heaplessModeEnabled) {
        GTEST_SKIP();
    }
    auto usedSpaceBefore = commandQueue->commandStream.getUsed();

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)->toHandle()};
    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);
    auto commandList = CommandList::fromHandle(commandLists[0]);
    commandList->close();

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true, nullptr, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream.getCpuBase(), 0), usedSpaceAfter));

    auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());

    size_t gpr15RegisterCount = 0;

    size_t gprMiLoadindex = std::numeric_limits<size_t>::max();

    for (size_t i = 0; i < miLoadImm.size(); i++) {
        MI_LOAD_REGISTER_IMM *miLoad = genCmdCast<MI_LOAD_REGISTER_IMM *>(*miLoadImm[i]);
        ASSERT_NE(nullptr, miLoad);

        if (miLoad->getRegisterOffset() == DebuggerRegisterOffsets::csGprR15) {
            gpr15RegisterCount++;
            gprMiLoadindex = i;
        }
        if (miLoad->getRegisterOffset() == DebuggerRegisterOffsets::csGprR15 + 4) {
            gpr15RegisterCount++;
        }
    }

    // 2 LRI commands to store SBA buffer address
    EXPECT_EQ(2u, gpr15RegisterCount);

    auto sbaGpuVa = getMockDebuggerL0Hw<FamilyType>()->getSbaTrackingBuffer(commandQueue->getCsr()->getOsContext().getContextId())->getGpuAddress();
    uint32_t low = sbaGpuVa & 0xffffffff;
    uint32_t high = (sbaGpuVa >> 32) & 0xffffffff;

    MI_LOAD_REGISTER_IMM *miLoad = genCmdCast<MI_LOAD_REGISTER_IMM *>(*miLoadImm[gprMiLoadindex]);
    EXPECT_EQ(DebuggerRegisterOffsets::csGprR15, miLoad->getRegisterOffset());
    EXPECT_EQ(low, miLoad->getDataDword());

    miLoad = genCmdCast<MI_LOAD_REGISTER_IMM *>(*miLoadImm[gprMiLoadindex + 1]);
    EXPECT_EQ(DebuggerRegisterOffsets::csGprR15 + 4, miLoad->getRegisterOffset());
    EXPECT_EQ(high, miLoad->getDataDword());

    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(commandLists[i]);
        commandList->destroy();
    }

    commandQueue->destroy();
}

} // namespace ult
} // namespace L0
