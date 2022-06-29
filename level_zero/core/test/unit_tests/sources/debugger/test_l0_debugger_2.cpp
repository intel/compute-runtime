/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/preamble.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/image/image_hw.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/sources/debugger/l0_debugger_fixture.h"

namespace L0 {
namespace ult {

struct L0DebuggerWithBlitterTest : public L0DebuggerHwParameterizedFixture {
    void SetUp() override {
        VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
        defaultHwInfo->capabilityTable.blitterOperationsSupported = true;
        L0DebuggerHwParameterizedFixture::SetUp();
    }
};

HWTEST_P(L0DebuggerWithBlitterTest, givenFlushTaskSubmissionEnabledWhenCommandListIsInititalizedOrResetThenCaptureSbaIsNotCalled) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    size_t usedSpaceBefore = 0;
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, true, NEO::EngineGroupType::RenderCompute, returnValue);

    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();
    ASSERT_GE(usedSpaceAfter, usedSpaceBefore);

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

HWTEST_P(L0DebuggerWithBlitterTest, givenFlushTaskSubmissionDisabledWhenCommandListIsInititalizedOrResetThenCaptureSbaIsNotCalled) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(false);

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

HWTEST_P(L0DebuggerWithBlitterTest, givenDebuggerLogsDisabledWhenCommandListIsSynchronizedThenSbaAddressesAreNotPrinted) {
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

using Gen12Plus = IsAtLeastGfxCore<IGFX_GEN12_CORE>;
HWTEST2_P(L0DebuggerWithBlitterTest, givenImmediateCommandListWhenExecutingWithFlushTaskThenSipIsInstalledAndDebuggerAllocationsAreResident, Gen12Plus) {
    using STATE_SIP = typename FamilyType::STATE_SIP;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    Mock<::L0::Kernel> kernel;
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    ze_group_count_t groupCount{1, 1, 1};

    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.storeMakeResidentAllocations = true;

    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::RenderCompute, returnValue);

    EXPECT_TRUE(commandList->isFlushTaskSubmissionEnabled);
    EXPECT_EQ(&csr, commandList->csr);

    csr.lastFlushedCommandStream = nullptr;
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel.toHandle(), &groupCount, nullptr, 0, nullptr, launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_NE(nullptr, csr.lastFlushedCommandStream);

    auto sbaBuffer = device->getL0Debugger()->getSbaTrackingBuffer(commandList->csr->getOsContext().getContextId());
    auto sipIsa = NEO::SipKernel::getSipKernel(*neoDevice).getSipAllocation();
    auto debugSurface = device->getDebugSurface();

    EXPECT_TRUE(csr.isMadeResident(sbaBuffer));
    EXPECT_TRUE(csr.isMadeResident(sipIsa));
    EXPECT_TRUE(csr.isMadeResident(debugSurface));

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, commandList->csr->getCS().getCpuBase(), commandList->csr->getCS().getUsed()));

    const auto &hwHelper = HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily);
    if (hwHelper.isSipWANeeded(hwInfo)) {

        auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());

        auto globalSipFound = 0u;
        for (size_t i = 0; i < miLoadImm.size(); i++) {
            MI_LOAD_REGISTER_IMM *miLoad = genCmdCast<MI_LOAD_REGISTER_IMM *>(*miLoadImm[i]);
            ASSERT_NE(nullptr, miLoad);

            if (miLoad->getRegisterOffset() == NEO::GlobalSipRegister<FamilyType>::registerOffset) {
                globalSipFound++;
            }
        }
        EXPECT_NE(0u, globalSipFound);
    } else {
        auto sipItor = find<STATE_SIP *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), sipItor);
    }
    commandList->destroy();
}

HWTEST_P(L0DebuggerWithBlitterTest, givenInternalUsageImmediateCommandListWhenExecutingThenDebuggerAllocationsAreNotResident) {
    using STATE_SIP = typename FamilyType::STATE_SIP;

    Mock<::L0::Kernel> kernel;
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    ze_group_count_t groupCount{1, 1, 1};

    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, true, NEO::EngineGroupType::RenderCompute, returnValue);

    // Internal command list must not have flush task enabled
    EXPECT_FALSE(commandList->isFlushTaskSubmissionEnabled);

    auto &csr = reinterpret_cast<NEO::UltCommandStreamReceiver<FamilyType> &>(*commandList->csr);
    csr.storeMakeResidentAllocations = true;

    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel.toHandle(), &groupCount, nullptr, 0, nullptr, launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto sbaBuffer = device->getL0Debugger()->getSbaTrackingBuffer(commandList->csr->getOsContext().getContextId());
    auto sipIsa = NEO::SipKernel::getSipKernel(*neoDevice).getSipAllocation();
    auto debugSurface = device->getDebugSurface();

    EXPECT_FALSE(csr.isMadeResident(sbaBuffer));
    EXPECT_FALSE(csr.isMadeResident(sipIsa));
    EXPECT_FALSE(csr.isMadeResident(debugSurface));

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, commandList->csr->getCS().getCpuBase(), commandList->csr->getCS().getUsed()));

    auto sipItor = find<STATE_SIP *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(cmdList.end(), sipItor);

    commandList->destroy();
}

HWTEST2_P(L0DebuggerWithBlitterTest, givenUseCsrImmediateSubmissionEnabledForImmediateCommandListForAppendLaunchKernelIndirectThenSuccessIsReturned, Gen12Plus) {
    Mock<::L0::Kernel> kernel;
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    ze_command_queue_desc_t queueDesc = {};
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    ze_group_count_t groupCount{1, 1, 1};
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::RenderCompute, returnValue);

    auto result = commandList->appendLaunchKernelIndirect(kernel.toHandle(), &groupCount, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
}

HWTEST2_P(L0DebuggerWithBlitterTest, givenUseCsrImmediateSubmissionDisabledForImmediateCommandListForAppendLaunchKernelIndirectThenSuccessIsReturned, Gen12Plus) {
    Mock<::L0::Kernel> kernel;
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(false);

    ze_command_queue_desc_t queueDesc = {};
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    ze_group_count_t groupCount{1, 1, 1};
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::RenderCompute, returnValue);

    auto result = commandList->appendLaunchKernelIndirect(kernel.toHandle(), &groupCount, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
}

HWTEST2_P(L0DebuggerWithBlitterTest, givenUseCsrImmediateSubmissionEnabledForImmediateCommandListForAppendMemoryCopyThenSuccessIsReturned, Gen12Plus) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::RenderCompute, returnValue);

    auto result = commandList->appendMemoryCopy(dstPtr, srcPtr, 0x100, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
}

HWTEST2_P(L0DebuggerWithBlitterTest, givenUseCsrImmediateSubmissionDisabledForImmediateCommandListForAppendMemoryCopyThenSuccessIsReturned, Gen12Plus) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(false);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::RenderCompute, returnValue);

    auto result = commandList->appendMemoryCopy(dstPtr, srcPtr, 0x100, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
}

HWTEST2_P(L0DebuggerWithBlitterTest, givenUseCsrImmediateSubmissionEnabledForImmediateCommandListForAppendMemoryCopyRegionThenSuccessIsReturned, Gen12Plus) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    uint32_t width = 16;
    uint32_t height = 16;
    ze_copy_region_t sr = {0U, 0U, 0U, width, height, 0U};
    ze_copy_region_t dr = {0U, 0U, 0U, width, height, 0U};

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::RenderCompute, returnValue);

    auto result = commandList->appendMemoryCopyRegion(dstPtr, &dr, 0, 0, srcPtr, &sr, 0, 0, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
}

HWTEST2_P(L0DebuggerWithBlitterTest, givenUseCsrImmediateSubmissionEnabledForRegularCommandListForAppendMemoryCopyRegionThenSuccessIsReturned, Gen12Plus) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    uint32_t width = 16;
    uint32_t height = 16;
    ze_copy_region_t sr = {0U, 0U, 0U, width, height, 0U};
    ze_copy_region_t dr = {0U, 0U, 0U, width, height, 0U};

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue->commandStream);

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle()};
    const uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    auto commandList = CommandList::fromHandle(commandLists[0]);
    auto result = commandList->appendMemoryCopyRegion(dstPtr, &dr, 0, 0, srcPtr, &sr, 0, 0, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    commandQueue->synchronize(0);

    commandList->destroy();
    commandQueue->destroy();
}

HWTEST2_P(L0DebuggerWithBlitterTest, givenUseCsrImmediateSubmissionDisabledForImmediateCommandListForAppendMemoryCopyRegionThenSuccessIsReturned, Gen12Plus) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(false);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {};
    ze_copy_region_t srcRegion = {};

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::RenderCompute, returnValue);

    auto result = commandList->appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
}

HWTEST2_P(L0DebuggerWithBlitterTest, givenUseCsrImmediateSubmissionEnabledCommandListAndAppendMemoryCopyCalledInLoopThenMultipleCommandBufferAreUsedAndSuccessIsReturned, Gen12Plus) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::RenderCompute, returnValue);
    ASSERT_NE(nullptr, commandList);

    for (uint32_t count = 0; count < 2048; count++) {
        auto result = commandList->appendMemoryCopy(dstPtr, srcPtr, 0x100, nullptr, 0, nullptr);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    }
    commandList->destroy();
}

HWTEST2_P(L0DebuggerWithBlitterTest, givenUseCsrImmediateSubmissionDisabledCommandListAndAppendMemoryCopyCalledInLoopThenMultipleCommandBufferAreUsedAndSuccessIsReturned, Gen12Plus) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(false);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::RenderCompute, returnValue);
    ASSERT_NE(nullptr, commandList);

    for (uint32_t count = 0; count < 2048; count++) {
        auto result = commandList->appendMemoryCopy(dstPtr, srcPtr, 0x100, nullptr, 0, nullptr);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    }
    commandList->destroy();
}

HWTEST2_P(L0DebuggerWithBlitterTest, givenDebuggingEnabledWhenInternalCmdQIsUsedThenDebuggerPathsAreNotExecuted, IsAtLeastSkl) {
    ze_command_queue_desc_t queueDesc = {};

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

HWTEST_P(L0DebuggerWithBlitterTest, givenDebuggingEnabledWhenCommandListIsExecutedOnCopyOnlyCmdQThenKernelDebugCommandsAreNotAdded) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using STATE_SIP = typename FamilyType::STATE_SIP;

    auto &selectorCopyEngine = neoDevice->getSelectorCopyEngine();
    auto deviceBitfield = neoDevice->getDeviceBitfield();
    auto bcsEngine = neoDevice->tryGetEngine(EngineHelpers::getBcsEngineType(hwInfo, deviceBitfield, selectorCopyEngine, false), EngineUsage::Regular);

    if (!bcsEngine) {
        GTEST_SKIP();
    }

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;

    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, bcsEngine->commandStreamReceiver, &queueDesc, true, false, returnValue));
    ASSERT_NE(nullptr, commandQueue->commandStream);

    auto usedSpaceBefore = commandQueue->commandStream->getUsed();

    auto commandList = CommandList::create(productFamily, device, EngineGroupType::Copy, 0u, returnValue);
    ze_command_list_handle_t commandLists[] = {commandList->toHandle()};
    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    char src[8];
    char dest[8];
    auto result = commandList->appendMemoryCopy(dest, src, 8, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));

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

    if (!HwHelper::get(hwInfo.platform.eRenderCoreFamily).isSipWANeeded(hwInfo)) {
        auto stateSipCmds = findAll<STATE_SIP *>(cmdList.begin(), cmdList.end());

        if (device->getDevicePreemptionMode() != PreemptionMode::MidThread) {
            ASSERT_EQ(0u, stateSipCmds.size());
        }
    }

    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(commandLists[i]);
        commandList->destroy();
    }

    commandQueue->destroy();
}
INSTANTIATE_TEST_CASE_P(SBAModesForDebugger, L0DebuggerWithBlitterTest, ::testing::Values(0, 1));

} // namespace ult
} // namespace L0
