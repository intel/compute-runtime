/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/source/cmdlist/cmdlist_memory_copy_params.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/image/image_hw.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"
#include "level_zero/core/test/unit_tests/sources/debugger/l0_debugger_fixture.h"

namespace L0 {
namespace ult {

struct L0DebuggerWithBlitterTest : public L0DebuggerHwParameterizedFixture {
    void SetUp() override {
        VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
        defaultHwInfo->capabilityTable.blitterOperationsSupported = true;
        L0DebuggerHwParameterizedFixture::SetUp();
    }
    CmdListMemoryCopyParams copyParams = {};
};

HWTEST_P(L0DebuggerWithBlitterTest, givenFlushTaskSubmissionEnabledWhenCommandListIsInititalizedOrResetThenCaptureSbaIsNotCalled) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    auto &compilerProductHelper = neoDevice->getCompilerProductHelper();
    auto heaplessEnabled = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo);
    if (compilerProductHelper.isHeaplessStateInitEnabled(heaplessEnabled)) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore restorer;

    debugManager.flags.EnableStateBaseAddressTracking.set(0);

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, true, NEO::EngineGroupType::renderCompute, returnValue);

    Mock<Module> module(device, nullptr, ModuleType::user);
    Mock<::L0::KernelImp> kernel;
    kernel.module = &module;

    auto &csrStream = commandList->getCsr(false)->getCS(0);
    size_t usedSpaceBefore = csrStream.getUsed();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    auto usedSpaceAfter = csrStream.getUsed();

    ASSERT_GE(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, csrStream.getCpuBase(), usedSpaceAfter));

    auto sbaItor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), sbaItor);

    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->captureStateBaseAddressCount);

    commandList->reset();
    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->captureStateBaseAddressCount);

    commandList->destroy();
}

HWTEST_P(L0DebuggerWithBlitterTest, givenDebuggerLogsDisabledWhenCommandListIsSynchronizedThenSbaAddressesAreNotPrinted) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DebuggerLogBitmask.set(0);

    EXPECT_NE(nullptr, device->getL0Debugger());
    StreamCapture capture;
    capture.captureStdout();

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, true, NEO::EngineGroupType::renderCompute, returnValue);

    auto cmdListImm = static_cast<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily> *>(commandList);

    cmdListImm->executeCommandListImmediateWithFlushTask(false, false, false, AppendOperations::kernel, false, false, nullptr, nullptr);

    std::string output = capture.getCapturedStdout();
    size_t pos = output.find("Debugger: SBA");
    EXPECT_EQ(std::string::npos, pos);

    commandList->destroy();
}

using Gen12Plus = IsAtLeastGfxCore<IGFX_GEN12_CORE>;
using singleAddressSpaceModeTest = Test<L0DebuggerSingleAddressSpaceFixture>;

HWTEST2_F(singleAddressSpaceModeTest, givenImmediateCommandListWhenExecutingWithFlushTaskThenGPR15isProgrammed, Gen12Plus) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    Mock<Module> module(device, nullptr, ModuleType::user);
    Mock<::L0::KernelImp> kernel;
    kernel.module = &module;

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    ze_group_count_t groupCount{1, 1, 1};

    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.storeMakeResidentAllocations = true;
    if (csr.heaplessModeEnabled) {
        GTEST_SKIP();
    }

    auto commandList = CommandList::whiteboxCast(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::renderCompute, returnValue));

    EXPECT_EQ(&csr, commandList->getCsr(false));

    csr.lastFlushedCommandStream = nullptr;
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_NE(nullptr, csr.lastFlushedCommandStream);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, commandList->getCsr(false)->getCS().getCpuBase(), commandList->getCsr(false)->getCS().getUsed()));
    bool gpr15Found = false;
    auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    for (size_t i = 0; i < miLoadImm.size(); i++) {
        MI_LOAD_REGISTER_IMM *miLoad = genCmdCast<MI_LOAD_REGISTER_IMM *>(*miLoadImm[i]);
        ASSERT_NE(nullptr, miLoad);

        if (miLoad->getRegisterOffset() == DebuggerRegisterOffsets::csGprR15) {
            gpr15Found = true;
            break;
        }
    }
    EXPECT_TRUE(gpr15Found);
    commandList->destroy();
}

HWTEST2_F(singleAddressSpaceModeTest, givenUseCsrImmediateSubmissionEnabledAndSharedHeapsDisbledForImmediateCommandListWhenExecutingWithFlushTaskThenGPR15isProgrammed, Gen12Plus) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    Mock<Module> module(device, nullptr, ModuleType::user);
    Mock<::L0::KernelImp> kernel;
    kernel.module = &module;
    DebugManagerStateRestore restorer;

    NEO::debugManager.flags.EnableImmediateCmdListHeapSharing.set(0);
    NEO::debugManager.flags.UseImmediateFlushTask.set(0);

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    ze_group_count_t groupCount{1, 1, 1};

    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.storeMakeResidentAllocations = true;

    auto commandList = CommandList::whiteboxCast(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::renderCompute, returnValue));

    EXPECT_EQ(&csr, commandList->getCsr(false));

    csr.lastFlushedCommandStream = nullptr;
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_NE(nullptr, csr.lastFlushedCommandStream);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, commandList->getCsr(false)->getCS().getCpuBase(), commandList->getCsr(false)->getCS().getUsed()));
    bool gpr15Found = false;
    auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    for (size_t i = 0; i < miLoadImm.size(); i++) {
        MI_LOAD_REGISTER_IMM *miLoad = genCmdCast<MI_LOAD_REGISTER_IMM *>(*miLoadImm[i]);
        ASSERT_NE(nullptr, miLoad);

        if (miLoad->getRegisterOffset() == DebuggerRegisterOffsets::csGprR15) {
            gpr15Found = true;
            break;
        }
    }

    if (csr.getHeaplessStateInitEnabled()) {
        EXPECT_FALSE(gpr15Found);
    } else {
        EXPECT_TRUE(gpr15Found);
    }

    commandList->destroy();
}

HWTEST2_P(L0DebuggerWithBlitterTest, givenImmediateCommandListWhenExecutingWithFlushTaskThenSipIsInstalledAndDebuggerAllocationsAreResident, Gen12Plus) {
    using STATE_SIP = typename FamilyType::STATE_SIP;

    Mock<Module> module(device, nullptr, ModuleType::user);
    Mock<::L0::KernelImp> kernel;
    kernel.module = &module;
    DebugManagerStateRestore restorer;

    NEO::debugManager.flags.UseImmediateFlushTask.set(0);

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    ze_group_count_t groupCount{1, 1, 1};

    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.storeMakeResidentAllocations = true;

    auto commandList = CommandList::whiteboxCast(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::renderCompute, returnValue));

    EXPECT_EQ(&csr, commandList->getCsr(false));

    csr.lastFlushedCommandStream = nullptr;
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_NE(nullptr, csr.lastFlushedCommandStream);

    auto sbaBuffer = device->getL0Debugger()->getSbaTrackingBuffer(commandList->getCsr(false)->getOsContext().getContextId());
    auto sipIsa = NEO::SipKernel::getSipKernel(*neoDevice, nullptr).getSipAllocation();
    auto debugSurface = device->getDebugSurface();

    EXPECT_TRUE(csr.isMadeResident(sbaBuffer));
    EXPECT_TRUE(csr.isMadeResident(sipIsa));
    EXPECT_TRUE(csr.isMadeResident(debugSurface));

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, commandList->getCsr(false)->getCS().getCpuBase(), commandList->getCsr(false)->getCS().getUsed()));

    auto sipItor = find<STATE_SIP *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), sipItor);
    commandList->destroy();
}

HWTEST2_P(L0DebuggerWithBlitterTest, givenImmediateFlushTaskWhenExecutingKernelThenSipIsInstalledAndDebuggerAllocationsAreResident, IsAtLeastXeCore) {
    using STATE_SIP = typename FamilyType::STATE_SIP;

    auto &compilerProductHelper = neoDevice->getCompilerProductHelper();
    auto heaplessEnabled = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo);
    auto heaplessStateInit = compilerProductHelper.isHeaplessStateInitEnabled(heaplessEnabled);
    if (heaplessStateInit) {
        GTEST_SKIP();
    }

    Mock<Module> module(device, nullptr, ModuleType::user);
    Mock<::L0::KernelImp> kernel;
    kernel.module = &module;
    DebugManagerStateRestore restorer;

    NEO::debugManager.flags.UseImmediateFlushTask.set(1);

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    ze_group_count_t groupCount{1, 1, 1};

    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.storeMakeResidentAllocations = true;

    auto commandList = CommandList::whiteboxCast(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::renderCompute, returnValue));

    EXPECT_EQ(&csr, commandList->getCsr(false));

    csr.lastFlushedCommandStream = nullptr;
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_NE(nullptr, csr.lastFlushedCommandStream);

    auto sbaBuffer = device->getL0Debugger()->getSbaTrackingBuffer(commandList->getCsr(false)->getOsContext().getContextId());
    auto sipIsa = NEO::SipKernel::getSipKernel(*neoDevice, nullptr).getSipAllocation();
    auto debugSurface = device->getDebugSurface();

    EXPECT_TRUE(csr.isMadeResident(sbaBuffer));
    EXPECT_TRUE(csr.isMadeResident(sipIsa));
    EXPECT_TRUE(csr.isMadeResident(debugSurface));

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, commandList->getCsr(false)->getCS().getCpuBase(), commandList->getCsr(false)->getCS().getUsed()));

    auto sipItor = find<STATE_SIP *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), sipItor);
    commandList->destroy();
}

HWTEST2_P(L0DebuggerWithBlitterTest, givenUseCsrImmediateSubmissionEnabledForImmediateCommandListForAppendLaunchKernelIndirectThenSuccessIsReturned, Gen12Plus) {
    Mock<Module> module(device, nullptr, ModuleType::user);
    Mock<::L0::KernelImp> kernel;
    kernel.module = &module;

    ze_command_queue_desc_t queueDesc = {};
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    ze_group_count_t groupCount{1, 1, 1};
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::renderCompute, returnValue);

    auto result = commandList->appendLaunchKernelIndirect(kernel.toHandle(), groupCount, nullptr, 0, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
}

HWTEST2_P(L0DebuggerWithBlitterTest, givenUseCsrImmediateSubmissionEnabledForImmediateCommandListForAppendMemoryCopyThenSuccessIsReturned, Gen12Plus) {
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::renderCompute, returnValue);

    auto result = commandList->appendMemoryCopy(dstPtr, srcPtr, 0x100, nullptr, 0, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
}

HWTEST2_P(L0DebuggerWithBlitterTest, givenUseCsrImmediateSubmissionEnabledForImmediateCommandListForAppendMemoryCopyRegionThenSuccessIsReturned, Gen12Plus) {
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    uint32_t width = 16;
    uint32_t height = 16;
    ze_copy_region_t sr = {0U, 0U, 0U, width, height, 0U};
    ze_copy_region_t dr = {0U, 0U, 0U, width, height, 0U};

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::renderCompute, returnValue);

    auto result = commandList->appendMemoryCopyRegion(dstPtr, &dr, 0, 0, srcPtr, &sr, 0, 0, nullptr, 0, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
}

HWTEST2_P(L0DebuggerWithBlitterTest, givenUseCsrImmediateSubmissionEnabledForRegularCommandListForAppendMemoryCopyRegionThenSuccessIsReturned, Gen12Plus) {
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    uint32_t width = 16;
    uint32_t height = 16;
    ze_copy_region_t sr = {0U, 0U, 0U, width, height, 0U};
    ze_copy_region_t dr = {0U, 0U, 0U, width, height, 0U};

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue);

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)->toHandle()};
    const uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    auto commandList = CommandList::fromHandle(commandLists[0]);
    auto result = commandList->appendMemoryCopyRegion(dstPtr, &dr, 0, 0, srcPtr, &sr, 0, 0, nullptr, 0, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    commandList->close();

    result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    commandQueue->synchronize(0);

    commandList->destroy();
    commandQueue->destroy();
}

HWTEST2_P(L0DebuggerWithBlitterTest, givenUseCsrImmediateSubmissionEnabledCommandListAndAppendMemoryCopyCalledInLoopThenMultipleCommandBufferAreUsedAndSuccessIsReturned, Gen12Plus) {
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::renderCompute, returnValue);
    ASSERT_NE(nullptr, commandList);

    for (uint32_t count = 0; count < 2048; count++) {
        auto result = commandList->appendMemoryCopy(dstPtr, srcPtr, 0x100, nullptr, 0, nullptr, copyParams);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    }
    commandList->destroy();
}

HWTEST_P(L0DebuggerWithBlitterTest, givenDebuggingEnabledWhenInternalCmdQIsUsedThenDebuggerPathsAreNotExecuted) {
    ze_command_queue_desc_t queueDesc = {};

    std::unique_ptr<MockCommandQueueHw<FamilyType::gfxCoreFamily>, Deleter> commandQueue(new MockCommandQueueHw<FamilyType::gfxCoreFamily>(device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc));
    commandQueue->initialize(false, true, true);
    EXPECT_TRUE(commandQueue->internalUsage);
    ze_result_t returnValue;
    ze_command_list_handle_t commandLists[] = {
        CommandList::createImmediate(productFamily, device, &queueDesc, true, NEO::EngineGroupType::renderCompute, returnValue)->toHandle()};
    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);
    auto commandList = whiteboxCast(static_cast<L0::CommandListImp *>(CommandList::fromHandle(commandLists[0])));
    commandList->closedCmdList = true;

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto sbaBuffer = device->getL0Debugger()->getSbaTrackingBuffer(neoDevice->getDefaultEngine().commandStreamReceiver->getOsContext().getContextId());
    auto sipIsa = NEO::SipKernel::getSipKernel(*neoDevice, nullptr).getSipAllocation();
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
    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->getSbaTrackingCommandsSizeCount);

    commandList->destroy();
}

HWTEST_P(L0DebuggerWithBlitterTest, givenDebuggingEnabledWhenCommandListIsExecutedOnCopyOnlyCmdQThenKernelDebugCommandsAreNotAdded) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using STATE_SIP = typename FamilyType::STATE_SIP;

    auto &selectorCopyEngine = neoDevice->getSelectorCopyEngine();
    auto deviceBitfield = neoDevice->getDeviceBitfield();
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironment();
    auto bcsEngine = neoDevice->tryGetEngine(EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false), EngineUsage::regular);

    if (!bcsEngine) {
        GTEST_SKIP();
    }

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;

    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, bcsEngine->commandStreamReceiver, &queueDesc, true, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue);

    auto usedSpaceBefore = commandQueue->commandStream.getUsed();

    auto commandList = CommandList::create(productFamily, device, EngineGroupType::copy, 0u, returnValue, false);
    ze_command_list_handle_t commandLists[] = {commandList->toHandle()};
    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    char src[8];
    char dest[8];
    auto result = commandList->appendMemoryCopy(dest, src, 8, nullptr, 0, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    commandList->close();

    result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true, nullptr, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream.getCpuBase(), 0), usedSpaceAfter));

    auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());

    size_t debugModeRegisterCount = 0;
    size_t tdDebugControlRegisterCount = 0;
    uint32_t globalSipFound = 0;

    for (size_t i = 0; i < miLoadImm.size(); i++) {
        MI_LOAD_REGISTER_IMM *miLoad = genCmdCast<MI_LOAD_REGISTER_IMM *>(*miLoadImm[i]);
        ASSERT_NE(nullptr, miLoad);

        if (miLoad->getRegisterOffset() == DebugModeRegisterOffset<FamilyType>::registerOffset) {
            debugModeRegisterCount++;
        }
        if (miLoad->getRegisterOffset() == TdDebugControlRegisterOffset<FamilyType>::registerOffset) {
            tdDebugControlRegisterCount++;
        }

        if (miLoad->getRegisterOffset() == GlobalSipRegister<FamilyType>::registerOffset) {
            globalSipFound++;
        }
    }
    // those register should not be used
    EXPECT_EQ(0u, debugModeRegisterCount);
    EXPECT_EQ(0u, tdDebugControlRegisterCount);
    EXPECT_EQ(0u, globalSipFound);

    auto stateSipCmds = findAll<STATE_SIP *>(cmdList.begin(), cmdList.end());

    if (device->getDevicePreemptionMode() != PreemptionMode::MidThread) {
        ASSERT_EQ(0u, stateSipCmds.size());
    }

    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(commandLists[i]);
        commandList->destroy();
    }

    commandQueue->destroy();
}
INSTANTIATE_TEST_SUITE_P(SBAModesForDebugger, L0DebuggerWithBlitterTest, ::testing::Values(0, 1));

} // namespace ult
} // namespace L0
