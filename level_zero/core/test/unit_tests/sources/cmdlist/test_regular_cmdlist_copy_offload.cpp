/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/in_order_cmd_helpers.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_cpu_page_fault_manager.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_cmdlist_execution_context.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_cmdlist_execution_internal_options.h"
#include "level_zero/core/source/cmdqueue/counters_cross_sync_definitions.h"
#include "level_zero/core/source/fence/fence.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/in_order_cmd_list_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/driver_experimental/zex_api.h"

namespace L0 {
namespace ult {

struct RegularCmdListCopyOffloadTests : public CopyOffloadInOrderFixture {
    void SetUp() override {
        NEO::debugManager.flags.ForceCopyOperationOffloadForComputeCmdList.set(2);
        NEO::debugManager.flags.OverrideCopyOffloadMode.set(static_cast<int32_t>(CopyOffloadModes::dualStream));

        CopyOffloadInOrderFixture::SetUp();
    }

    void TearDown() override {
        for (auto ptr : usmAllocations) {
            context->freeMem(ptr);
        }

        CopyOffloadInOrderFixture::TearDown();
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    using RegularCmdList = WhiteBox<L0::CommandListCoreFamily<gfxCoreFamily>>;

    template <GFXCORE_FAMILY gfxCoreFamily>
    static RegularCmdList<gfxCoreFamily> *getCopyOffloadCmdList(RegularCmdList<gfxCoreFamily> &cmdList) {
        return static_cast<RegularCmdList<gfxCoreFamily> *>(cmdList.copyOffloadSubCmdList);
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    DestroyableZeUniquePtr<MockCommandQueueHw<gfxCoreFamily>> createComputeQueue() {
        ze_command_queue_desc_t desc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
        auto cmdQueue = makeZeUniquePtr<MockCommandQueueHw<gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
        cmdQueue->initialize(false, false, false);
        return cmdQueue;
    }

    void *allocHostUsm() {
        auto ptr = allocHostMem(copySize);
        usmAllocations.push_back(ptr);
        return ptr;
    }

    void *allocDeviceUsm() {
        auto ptr = allocDeviceMem(copySize);
        usmAllocations.push_back(ptr);
        return ptr;
    }

    NEO::GraphicsAllocation *getUsmAllocation(void *ptr) {
        return driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    }

    template <typename FamilyType>
    GenCmdList parseStream(NEO::LinearStream &stream, size_t offset) {
        GenCmdList cmdList;
        EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(stream.getCpuBase(), offset), stream.getUsed() - offset));
        return cmdList;
    }

    template <typename FamilyType>
    std::vector<uint64_t> getSemaphoreWaitValues(GenCmdList &cmdList, uint64_t address) {
        using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
        using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

        const bool useSemaphore64bCmd = device->getNEODevice()->getDeviceInfo().semaphore64bCmdSupport;
        const bool lriRequired = NEO::InOrderProgrammingHelpers::isLriFor64bDataProgrammingRequired(FamilyType::isQwordInOrderCounter, useSemaphore64bCmd);
        constexpr uint32_t registerOffsetMask = 0xFFFF;
        auto gmmHelper = device->getNEODevice()->getGmmHelper();

        std::vector<uint64_t> waitValues;
        uint64_t lriLowValue = 0;
        uint64_t lriHighValue = 0;

        for (auto &cmd : cmdList) {
            if (auto lri = genCmdCast<MI_LOAD_REGISTER_IMM *>(cmd)) {
                const auto registerOffset = lri->getRegisterOffset() & registerOffsetMask;
                if (registerOffset == (RegisterOffsets::csGprR0 & registerOffsetMask)) {
                    lriLowValue = lri->getDataDword();
                } else if (registerOffset == ((RegisterOffsets::csGprR0 + 4) & registerOffsetMask)) {
                    lriHighValue = lri->getDataDword();
                }
            } else if (auto semaphore = genCmdCast<MI_SEMAPHORE_WAIT *>(cmd)) {
                if (gmmHelper->decanonize(NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semaphore)) == gmmHelper->decanonize(address)) {
                    waitValues.push_back(lriRequired ? ((lriHighValue << 32) | lriLowValue) : NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(semaphore));
                }
            }
        }

        return waitValues;
    }

    template <typename FamilyType>
    GenCmdList::iterator getNextNonNoopCmd(GenCmdList::iterator itor, GenCmdList::iterator end) {
        using MI_NOOP = typename FamilyType::MI_NOOP;

        while (itor != end && genCmdCast<MI_NOOP *>(*itor) != nullptr) {
            ++itor;
        }
        EXPECT_NE(end, itor);
        return itor;
    }

    template <typename FamilyType>
    std::vector<typename FamilyType::MI_STORE_DATA_IMM *> getStoresToAddress(GenCmdList &cmdList, uint64_t address) {
        using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

        auto gmmHelper = device->getNEODevice()->getGmmHelper();

        std::vector<MI_STORE_DATA_IMM *> stores;
        for (auto &cmd : cmdList) {
            auto store = genCmdCast<MI_STORE_DATA_IMM *>(cmd);
            if (store && gmmHelper->decanonize(store->getAddress()) == gmmHelper->decanonize(address)) {
                stores.push_back(store);
            }
        }
        return stores;
    }

    template <typename FamilyType>
    GenCmdList::iterator findPostSyncWrite(GenCmdList &cmdList, uint64_t address, uint64_t value) {
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
        using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

        auto gmmHelper = device->getNEODevice()->getGmmHelper();

        return std::find_if(cmdList.begin(), cmdList.end(), [&](void *cmd) {
            auto pipeControl = genCmdCast<PIPE_CONTROL *>(cmd);
            return pipeControl != nullptr &&
                   pipeControl->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA &&
                   gmmHelper->decanonize(NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl)) == gmmHelper->decanonize(address) &&
                   pipeControl->getImmediateData() == value;
        });
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    uint64_t getPatchPreambleCounterDeviceAddress(MockCommandQueueHw<gfxCoreFamily> &cmdQueue) {
        NEO::GraphicsAllocation *hostAllocation = nullptr;
        uint64_t hostGpuAddress = 0;
        NEO::GraphicsAllocation *deviceAllocation = nullptr;
        uint64_t deviceGpuAddress = 0;
        cmdQueue.patchPreambleCounter.getPatchPreambleNodeData(hostAllocation, hostGpuAddress, deviceAllocation, deviceGpuAddress);
        EXPECT_NE(nullptr, deviceAllocation);
        return deviceGpuAddress + cmdQueue.patchPreambleCounter.offset;
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    ze_result_t appendKernel(RegularCmdList<gfxCoreFamily> &cmdList) {
        ze_group_count_t groupCount = {1, 1, 1};
        CmdListKernelLaunchParams launchParams = {};
        return cmdList.appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    ze_result_t appendCopy(RegularCmdList<gfxCoreFamily> &cmdList, ze_event_handle_t hSignalEvent) {
        CmdListMemoryCopyParams copyParams = {};
        auto result = cmdList.appendMemoryCopy(allocDeviceUsm(), allocHostUsm(), copySize, hSignalEvent, 0, nullptr, copyParams);
        EXPECT_TRUE(copyParams.copyOffloadAllowed);
        return result;
    }

    static uint64_t getCounterGpuAddress(const NEO::InOrderExecInfo &inOrderExecInfo) {
        return inOrderExecInfo.getBaseDeviceAddress() + inOrderExecInfo.getAllocationOffset();
    }

    std::vector<void *> usmAllocations;
    const size_t copySize = 256;
};

HWTEST2_F(RegularCmdListCopyOffloadTests, givenDualStreamCopyOffloadWhenCreatingRegularCmdListsThenEnableItOnlyForInOrderComputeCmdLists, IsAtLeastXeCore) {
    auto inOrderCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    EXPECT_EQ(CopyOffloadModes::dualStream, inOrderCmdList->copyOffloadMode);
    EXPECT_TRUE(inOrderCmdList->isRegularDualStreamCopyOffloadOperation(true));
    EXPECT_FALSE(inOrderCmdList->isRegularDualStreamCopyOffloadOperation(false));
    EXPECT_EQ(nullptr, inOrderCmdList->cmdQImmediateCopyOffload);
    EXPECT_EQ(nullptr, inOrderCmdList->copyOffloadSubCmdList);
    EXPECT_EQ(nullptr, inOrderCmdList->getCopyOffloadSubCmdList());

    auto outOfOrderCmdList = makeZeUniquePtr<RegularCmdList<FamilyType::gfxCoreFamily>>();
    outOfOrderCmdList->initialize(device, NEO::EngineGroupType::renderCompute, 0);
    EXPECT_EQ(CopyOffloadModes::disabled, outOfOrderCmdList->copyOffloadMode);

    auto copyOnlyCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(true);
    EXPECT_EQ(CopyOffloadModes::disabled, copyOnlyCmdList->copyOffloadMode);
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenPatchPreambleEnabledWhenCreatingRegularCmdListThenDualStreamCopyOffloadIsKept, IsAtLeastXeCore) {
    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    cmdList->setupPatchPreambleEnabled(true);
    EXPECT_TRUE(cmdList->isPatchPreambleEnabled());
    EXPECT_EQ(CopyOffloadModes::dualStream, cmdList->copyOffloadMode);

    NEO::debugManager.flags.ForceEnableRegularCmdListPatchPreamble.set(1);
    auto patchPreambleCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    EXPECT_TRUE(patchPreambleCmdList->isPatchPreambleEnabled());
    EXPECT_EQ(CopyOffloadModes::dualStream, patchPreambleCmdList->copyOffloadMode);
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenRegularCmdListDualStreamSupportWhenCreatingCmdListWithoutCopyOffloadHintThenCopyOffloadIsNotEnabled, IsAtLeastXeCore) {
    NEO::debugManager.flags.ForceCopyOperationOffloadForComputeCmdList.set(-1);
    NEO::debugManager.flags.OverrideDualStreamCopyOffloadForRegularSupport.set(1);

    auto internalCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    EXPECT_EQ(CopyOffloadModes::disabled, internalCmdList->copyOffloadMode);

    ze_command_list_desc_t cmdListDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    cmdListDesc.flags = ZE_COMMAND_LIST_FLAG_IN_ORDER;

    ze_command_list_handle_t hCmdList = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreate(context->toHandle(), device->toHandle(), &cmdListDesc, &hCmdList));
    EXPECT_EQ(CopyOffloadModes::disabled, CommandList::fromHandle(hCmdList)->getCopyOffloadModeForOperation(true));
    zeCommandListDestroy(hCmdList);
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenCopyOffloadHintWhenCreatingRegularCmdListThenDualStreamCopyOffloadIsEnabledOnlyForSupportedInOrderCmdList, IsAtLeastXeCore) {
    NEO::debugManager.flags.ForceCopyOperationOffloadForComputeCmdList.set(-1);

    const auto expectedMode = device->getGfxCoreHelper().crossEngineCacheFlushRequired() ? CopyOffloadModes::disabled : CopyOffloadModes::dualStream;

    zex_intel_queue_copy_operations_offload_hint_exp_desc_t copyOffloadDesc = {ZEX_INTEL_STRUCTURE_TYPE_QUEUE_COPY_OPERATIONS_OFFLOAD_HINT_EXP_PROPERTIES};
    copyOffloadDesc.copyOffloadEnabled = true;

    ze_command_list_desc_t hintFlagDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    hintFlagDesc.flags = ZE_COMMAND_LIST_FLAG_IN_ORDER | ZE_COMMAND_LIST_FLAG_COPY_OFFLOAD_HINT;

    ze_command_list_desc_t hintExtDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    hintExtDesc.flags = ZE_COMMAND_LIST_FLAG_IN_ORDER;
    hintExtDesc.pNext = &copyOffloadDesc;

    ze_command_list_desc_t outOfOrderHintDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    outOfOrderHintDesc.flags = ZE_COMMAND_LIST_FLAG_COPY_OFFLOAD_HINT;

    auto getCopyOffloadMode = [&](const ze_command_list_desc_t &desc) {
        ze_command_list_handle_t hCmdList = nullptr;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreate(context->toHandle(), device->toHandle(), &desc, &hCmdList));
        auto mode = CommandList::fromHandle(hCmdList)->getCopyOffloadModeForOperation(true);
        zeCommandListDestroy(hCmdList);
        return mode;
    };

    // regular cmd list dual stream support disabled
    NEO::debugManager.flags.OverrideDualStreamCopyOffloadForRegularSupport.set(0);
    EXPECT_EQ(CopyOffloadModes::disabled, getCopyOffloadMode(hintFlagDesc));
    EXPECT_EQ(CopyOffloadModes::disabled, getCopyOffloadMode(hintExtDesc));

    NEO::debugManager.flags.OverrideDualStreamCopyOffloadForRegularSupport.set(1);
    EXPECT_EQ(expectedMode, getCopyOffloadMode(hintFlagDesc));
    EXPECT_EQ(expectedMode, getCopyOffloadMode(hintExtDesc));
    EXPECT_EQ(CopyOffloadModes::disabled, getCopyOffloadMode(outOfOrderHintDesc));
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenRegularCmdListWhenAppendingMemoryCopyThenCopyIsRecordedIntoCopyOffloadStream, IsAtLeastXeCore) {
    using XY_COPY_BLT = typename std::remove_const<decltype(FamilyType::cmdInitXyCopyBlt)>::type;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    auto mainStream = cmdList->getCmdContainer().getCommandStream();
    const auto mainStreamOffset = mainStream->getUsed();
    const auto counterAddress = getCounterGpuAddress(*cmdList->inOrderExecInfo);
    const auto incrementValue = cmdList->getInOrderIncrementValue();

    auto dst = allocDeviceUsm();
    auto src = allocHostUsm();
    CmdListMemoryCopyParams copyParams = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdList->appendMemoryCopy(dst, src, copySize, nullptr, 0, nullptr, copyParams));
    EXPECT_TRUE(copyParams.copyOffloadAllowed);

    auto copyCmdList = getCopyOffloadCmdList(*cmdList);
    ASSERT_NE(nullptr, copyCmdList);
    EXPECT_EQ(copyCmdList, cmdList->getCopyOffloadSubCmdList());
    EXPECT_TRUE(copyCmdList->isCopyOnly(false));
    EXPECT_TRUE(copyCmdList->internalUsage);
    EXPECT_FALSE(copyCmdList->isInOrderExecutionEnabled());
    EXPECT_FALSE(cmdList->copyOffloadStreamRecordingActive);
    EXPECT_TRUE(cmdList->latestFlushIsDualCopyOffload);
    EXPECT_EQ(mainStream, cmdList->getCmdContainer().getCommandStream());
    EXPECT_EQ(&cmdList->getCmdContainer(), mainStream->getCmdContainer());

    // compute stream signals counter, so copy offload stream can start
    EXPECT_EQ(2 * incrementValue, cmdList->inOrderExecInfo->getCounterValue());

    auto mainCmds = parseStream<FamilyType>(*mainStream, mainStreamOffset);
    EXPECT_EQ(mainCmds.end(), find<XY_COPY_BLT *>(mainCmds.begin(), mainCmds.end()));
    auto counterSignal = findInOrderCounterSignalPipeControl<FamilyType>(mainCmds, counterAddress);
    ASSERT_NE(nullptr, counterSignal);
    EXPECT_EQ(incrementValue, counterSignal->getImmediateData());

    auto copyStream = copyCmdList->getCmdContainer().getCommandStream();
    EXPECT_EQ(&copyCmdList->getCmdContainer(), copyStream->getCmdContainer());
    auto copyCmds = parseStream<FamilyType>(*copyStream, 0);
    EXPECT_EQ(copyCmds.end(), find<PIPE_CONTROL *>(copyCmds.begin(), copyCmds.end()));

    auto waitValues = getSemaphoreWaitValues<FamilyType>(copyCmds, counterAddress);
    ASSERT_EQ(1u, waitValues.size());
    EXPECT_EQ(incrementValue, waitValues[0]);

    auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(copyCmds.begin(), copyCmds.end());
    ASSERT_NE(copyCmds.end(), semaphoreItor);
    EXPECT_NE(copyCmds.end(), find<XY_COPY_BLT *>(semaphoreItor, copyCmds.end()));

    auto dstAllocation = getUsmAllocation(dst);
    auto &copyResidency = copyCmdList->getCmdContainer().getResidencyContainer();
    auto &mainResidency = cmdList->getCmdContainer().getResidencyContainer();
    EXPECT_NE(copyResidency.end(), std::find(copyResidency.begin(), copyResidency.end(), dstAllocation));
    EXPECT_EQ(mainResidency.end(), std::find(mainResidency.begin(), mainResidency.end(), dstAllocation));
    EXPECT_NE(copyResidency.end(), std::find(copyResidency.begin(), copyResidency.end(), cmdList->inOrderExecInfo->getDeviceCounterAllocation()));
    EXPECT_NE(mainResidency.end(), std::find(mainResidency.begin(), mainResidency.end(), cmdList->inOrderExecInfo->getDeviceCounterAllocation()));
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenConsecutiveCopiesWhenAppendingThenComputeStreamIsSignaledOnlyOnce, IsAtLeastXeCore) {
    using XY_COPY_BLT = typename std::remove_const<decltype(FamilyType::cmdInitXyCopyBlt)>::type;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    auto mainStream = cmdList->getCmdContainer().getCommandStream();
    const auto mainStreamOffset = mainStream->getUsed();
    const auto counterAddress = getCounterGpuAddress(*cmdList->inOrderExecInfo);
    const auto incrementValue = cmdList->getInOrderIncrementValue();

    ASSERT_EQ(ZE_RESULT_SUCCESS, appendCopy(*cmdList, nullptr));
    auto copyCmdList = getCopyOffloadCmdList(*cmdList);
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendCopy(*cmdList, nullptr));
    EXPECT_EQ(copyCmdList, getCopyOffloadCmdList(*cmdList));

    EXPECT_EQ(3 * incrementValue, cmdList->inOrderExecInfo->getCounterValue());

    auto mainCmds = parseStream<FamilyType>(*mainStream, mainStreamOffset);
    EXPECT_EQ(mainCmds.end(), find<MI_SEMAPHORE_WAIT *>(mainCmds.begin(), mainCmds.end()));
    EXPECT_NE(nullptr, findInOrderCounterSignalPipeControl<FamilyType>(mainCmds, counterAddress));

    auto copyCmds = parseStream<FamilyType>(*copyCmdList->getCmdContainer().getCommandStream(), 0);
    EXPECT_EQ(2u, findAll<XY_COPY_BLT *>(copyCmds.begin(), copyCmds.end()).size());

    auto waitValues = getSemaphoreWaitValues<FamilyType>(copyCmds, counterAddress);
    ASSERT_EQ(2u, waitValues.size());
    EXPECT_EQ(incrementValue, waitValues[0]);
    EXPECT_EQ(2 * incrementValue, waitValues[1]);
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenCopyAfterKernelWhenAppendingThenCopyOffloadStreamWaitsForKernelCounter, IsAtLeastXeCore) {
    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    const auto counterAddress = getCounterGpuAddress(*cmdList->inOrderExecInfo);

    ASSERT_EQ(ZE_RESULT_SUCCESS, appendKernel(*cmdList));
    const bool counterSignalPending = cmdList->isInOrderCounterSignalPending();
    const auto counterAfterKernel = cmdList->inOrderExecInfo->getCounterValue();

    ASSERT_EQ(ZE_RESULT_SUCCESS, appendCopy(*cmdList, nullptr));

    auto copyCmds = parseStream<FamilyType>(*getCopyOffloadCmdList(*cmdList)->getCmdContainer().getCommandStream(), 0);
    auto waitValues = getSemaphoreWaitValues<FamilyType>(copyCmds, counterAddress);
    ASSERT_EQ(1u, waitValues.size());

    if (counterSignalPending) {
        // kernel did not signal counter, compute stream signals it explicitly before copy offload stream starts
        EXPECT_EQ(counterAfterKernel + cmdList->getInOrderIncrementValue(), waitValues[0]);
    } else {
        EXPECT_EQ(counterAfterKernel, waitValues[0]);
    }
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenCopyOffloadedWhenAppendingKernelThenComputeStreamWaitsForCopyOffloadStreamWithSemaphore, IsAtLeastXeCore) {
    NEO::debugManager.flags.ResolveDependenciesViaPipeControls.set(1);

    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    auto mainStream = cmdList->getCmdContainer().getCommandStream();
    const auto counterAddress = getCounterGpuAddress(*cmdList->inOrderExecInfo);

    ASSERT_EQ(ZE_RESULT_SUCCESS, appendCopy(*cmdList, nullptr));
    EXPECT_TRUE(cmdList->latestFlushIsDualCopyOffload);
    const auto counterAfterCopy = cmdList->inOrderExecInfo->getCounterValue();

    auto offset = mainStream->getUsed();
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendKernel(*cmdList));
    EXPECT_FALSE(cmdList->latestFlushIsDualCopyOffload);

    auto mainCmds = parseStream<FamilyType>(*mainStream, offset);
    auto waitValues = getSemaphoreWaitValues<FamilyType>(mainCmds, counterAddress);
    ASSERT_EQ(1u, waitValues.size());
    EXPECT_EQ(counterAfterCopy, waitValues[0]);

    // dependency between operations on compute stream is resolved without semaphore
    offset = mainStream->getUsed();
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendKernel(*cmdList));

    mainCmds = parseStream<FamilyType>(*mainStream, offset);
    EXPECT_EQ(0u, getSemaphoreWaitValues<FamilyType>(mainCmds, counterAddress).size());
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenCopyAsLastOperationWhenClosingThenComputeStreamWaitsForCopyOffloadStreamCompletion, IsAtLeastXeCore) {
    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    auto mainStream = cmdList->getCmdContainer().getCommandStream();
    const auto counterAddress = getCounterGpuAddress(*cmdList->inOrderExecInfo);

    ASSERT_EQ(ZE_RESULT_SUCCESS, appendCopy(*cmdList, nullptr));
    const auto finalCounterValue = cmdList->inOrderExecInfo->getCounterValue();

    const auto offset = mainStream->getUsed();
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdList->close());

    auto mainCmds = parseStream<FamilyType>(*mainStream, offset);
    auto waitValues = getSemaphoreWaitValues<FamilyType>(mainCmds, counterAddress);
    ASSERT_EQ(1u, waitValues.size());
    EXPECT_EQ(finalCounterValue, waitValues[0]);
    EXPECT_FALSE(cmdList->latestFlushIsDualCopyOffload);

    EXPECT_TRUE(cmdList->isClosed());
    EXPECT_TRUE(getCopyOffloadCmdList(*cmdList)->isClosed());
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenKernelAfterCopyWhenClosingThenNoAdditionalWaitIsProgrammed, IsAtLeastXeCore) {
    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    auto mainStream = cmdList->getCmdContainer().getCommandStream();
    const auto counterAddress = getCounterGpuAddress(*cmdList->inOrderExecInfo);

    ASSERT_EQ(ZE_RESULT_SUCCESS, appendCopy(*cmdList, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendKernel(*cmdList));

    const auto offset = mainStream->getUsed();
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdList->close());

    auto mainCmds = parseStream<FamilyType>(*mainStream, offset);
    EXPECT_EQ(0u, getSemaphoreWaitValues<FamilyType>(mainCmds, counterAddress).size());
    EXPECT_TRUE(getCopyOffloadCmdList(*cmdList)->isClosed());
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenNoCopyOffloadedWhenClosingThenCopyOffloadStreamIsNotUsed, IsAtLeastXeCore) {
    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendKernel(*cmdList));
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdList->close());

    EXPECT_EQ(nullptr, cmdList->copyOffloadSubCmdList);
    EXPECT_EQ(nullptr, cmdList->getCopyOffloadSubCmdList());
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenRegularCmdListWhenAppendingMemoryFillThenFillIsRecordedIntoCopyOffloadStream, IsAtLeastXeCore) {
    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    auto mainStream = cmdList->getCmdContainer().getCommandStream();
    const auto mainStreamOffset = mainStream->getUsed();

    uint8_t pattern = 0xAB;
    CmdListMemoryCopyParams copyParams = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdList->appendMemoryFill(allocDeviceUsm(), &pattern, sizeof(pattern), copySize, nullptr, 0, nullptr, copyParams));
    EXPECT_TRUE(copyParams.copyOffloadAllowed);

    auto copyCmdList = getCopyOffloadCmdList(*cmdList);
    ASSERT_NE(nullptr, copyCmdList);

    auto mainCmds = parseStream<FamilyType>(*mainStream, mainStreamOffset);
    EXPECT_EQ(mainCmds.end(), findBltFillCmd<FamilyType>(mainCmds.begin(), mainCmds.end()));

    auto copyCmds = parseStream<FamilyType>(*copyCmdList->getCmdContainer().getCommandStream(), 0);
    EXPECT_NE(copyCmds.end(), findBltFillCmd<FamilyType>(copyCmds.begin(), copyCmds.end()));
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenRegularCmdListWhenAppendingMemoryCopyRegionThenCopyIsRecordedIntoCopyOffloadStream, IsAtLeastXeCore) {
    using XY_COPY_BLT = typename std::remove_const<decltype(FamilyType::cmdInitXyCopyBlt)>::type;

    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    auto mainStream = cmdList->getCmdContainer().getCommandStream();
    const auto mainStreamOffset = mainStream->getUsed();

    ze_copy_region_t region = {0, 0, 0, 16, 2, 1};
    CmdListMemoryCopyParams copyParams = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdList->appendMemoryCopyRegion(allocDeviceUsm(), &region, 16, 32, allocHostUsm(), &region, 16, 32, nullptr, 0, nullptr, copyParams));
    EXPECT_TRUE(copyParams.copyOffloadAllowed);

    auto copyCmdList = getCopyOffloadCmdList(*cmdList);
    ASSERT_NE(nullptr, copyCmdList);

    auto mainCmds = parseStream<FamilyType>(*mainStream, mainStreamOffset);
    EXPECT_EQ(mainCmds.end(), find<XY_COPY_BLT *>(mainCmds.begin(), mainCmds.end()));

    auto copyCmds = parseStream<FamilyType>(*copyCmdList->getCmdContainer().getCommandStream(), 0);
    EXPECT_NE(copyCmds.end(), find<XY_COPY_BLT *>(copyCmds.begin(), copyCmds.end()));
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenCopyOffloadStreamOutOfSpaceWhenRecordingCopyThenNextCommandBufferIsOwnedByCopyOffloadCmdList, IsAtLeastXeCore) {
    using XY_COPY_BLT = typename std::remove_const<decltype(FamilyType::cmdInitXyCopyBlt)>::type;

    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendCopy(*cmdList, nullptr));

    auto copyCmdList = getCopyOffloadCmdList(*cmdList);
    auto &mainContainer = cmdList->getCmdContainer();
    auto &copyContainer = copyCmdList->getCmdContainer();
    ASSERT_EQ(1u, mainContainer.getCmdBufferAllocations().size());
    ASSERT_EQ(1u, copyContainer.getCmdBufferAllocations().size());

    auto mainStream = mainContainer.getCommandStream();
    auto copyStream = copyContainer.getCommandStream();
    auto firstCopyCmdBuffer = copyContainer.getCmdBufferAllocations()[0];

    // leave space only for batch buffer chaining
    copyStream->getSpace(copyStream->getAvailableSpace() - 2 * NEO::EncodeBatchBufferStartOrEnd<FamilyType>::getBatchBufferStartSize());

    ASSERT_EQ(ZE_RESULT_SUCCESS, appendCopy(*cmdList, nullptr));

    EXPECT_EQ(1u, mainContainer.getCmdBufferAllocations().size());
    ASSERT_EQ(2u, copyContainer.getCmdBufferAllocations().size());
    EXPECT_EQ(firstCopyCmdBuffer, copyContainer.getCmdBufferAllocations()[0]);

    EXPECT_EQ(mainStream, mainContainer.getCommandStream());
    EXPECT_EQ(copyStream, copyContainer.getCommandStream());
    EXPECT_EQ(&mainContainer, mainStream->getCmdContainer());
    EXPECT_EQ(&copyContainer, copyStream->getCmdContainer());
    EXPECT_EQ(copyContainer.getCmdBufferAllocations()[1], copyStream->getGraphicsAllocation());

    auto copyCmds = parseStream<FamilyType>(*copyStream, 0);
    EXPECT_NE(copyCmds.end(), find<XY_COPY_BLT *>(copyCmds.begin(), copyCmds.end()));
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenRecordedCopyOffloadStreamWhenResettingCmdListThenCopyOffloadStreamIsReset, IsAtLeastXeCore) {
    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendCopy(*cmdList, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdList->close());

    auto copyCmdList = getCopyOffloadCmdList(*cmdList);
    ASSERT_NE(nullptr, copyCmdList);
    EXPECT_TRUE(copyCmdList->isClosed());

    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdList->reset());

    EXPECT_EQ(copyCmdList, cmdList->copyOffloadSubCmdList);
    EXPECT_EQ(nullptr, cmdList->getCopyOffloadSubCmdList());
    EXPECT_FALSE(cmdList->copyOffloadStreamUsed);
    EXPECT_FALSE(cmdList->latestFlushIsDualCopyOffload);
    EXPECT_FALSE(copyCmdList->isClosed());
    auto &copyResidency = copyCmdList->getCmdContainer().getResidencyContainer();
    EXPECT_EQ(copyResidency.end(), std::find(copyResidency.begin(), copyResidency.end(), cmdList->inOrderExecInfo->getDeviceCounterAllocation()));

    ASSERT_EQ(ZE_RESULT_SUCCESS, appendCopy(*cmdList, nullptr));
    EXPECT_EQ(copyCmdList, cmdList->getCopyOffloadSubCmdList());
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenEventWithKmdWaitSignaledFromCopyOffloadStreamWhenRecordingThenEventIsAssignedToCopyOffloadCmdList, IsAtLeastXeCore) {
    auto eventPool = createEvents<FamilyType>(2, false);
    events[0]->enableKmdWaitMode();
    events[1]->enableKmdWaitMode();

    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendCopy(*cmdList, events[0]->toHandle()));

    ze_group_count_t groupCount = {1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams));
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdList->close());

    auto copyCmdList = getCopyOffloadCmdList(*cmdList);
    ASSERT_EQ(1u, copyCmdList->interruptEvents.size());
    EXPECT_EQ(events[0].get(), copyCmdList->interruptEvents[0]);
    ASSERT_EQ(1u, cmdList->interruptEvents.size());
    EXPECT_EQ(events[1].get(), cmdList->interruptEvents[0]);

    auto cmdQueue = createComputeQueue<FamilyType::gfxCoreFamily>();
    auto cmdListHandle = cmdList->toHandle();
    CommandListExecutionInternalOptions internalOptions = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdQueue->executeCommandLists(1, &cmdListHandle, nullptr, internalOptions));
    ASSERT_NE(nullptr, cmdQueue->copyOffloadQueue);

    ASSERT_EQ(1u, events[0]->csrs.size());
    EXPECT_EQ(cmdQueue->copyOffloadQueue->getCsr(), events[0]->csrs[0]);
    ASSERT_EQ(1u, events[1]->csrs.size());
    EXPECT_EQ(cmdQueue->getCsr(), events[1]->csrs[0]);
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenCopyOffloadStreamWhenExecutingCmdListThenCopyOffloadStreamIsSubmittedToCopyQueue, IsAtLeastXeCore) {
    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendCopy(*cmdList, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdList->close());

    auto cmdQueue = createComputeQueue<FamilyType::gfxCoreFamily>();
    EXPECT_EQ(nullptr, cmdQueue->copyOffloadQueue);

    auto cmdListHandle = cmdList->toHandle();
    CommandListExecutionInternalOptions internalOptions = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdQueue->executeCommandLists(1, &cmdListHandle, nullptr, internalOptions));

    auto copyQueue = cmdQueue->copyOffloadQueue;
    ASSERT_NE(nullptr, copyQueue);
    EXPECT_TRUE(copyQueue->peekIsCopyOnlyCommandQueue());

    auto copyCsr = copyQueue->getCsr();
    EXPECT_NE(cmdQueue->getCsr(), copyCsr);
    EXPECT_TRUE(NEO::EngineHelpers::isBcs(copyCsr->getOsContext().getEngineType()));
    EXPECT_FALSE(getCopyOffloadCmdList(*cmdList)->isPatchPreambleEnabled());

    const auto copyTaskCount = copyCsr->peekTaskCount();
    EXPECT_LT(0u, copyTaskCount);

    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdQueue->executeCommandLists(1, &cmdListHandle, nullptr, internalOptions));
    EXPECT_EQ(copyQueue, cmdQueue->copyOffloadQueue);
    EXPECT_EQ(copyTaskCount + 1, copyCsr->peekTaskCount());

    // cmd list without copy offload stream does not need copy queue
    auto computeOnlyCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendKernel(*computeOnlyCmdList));
    ASSERT_EQ(ZE_RESULT_SUCCESS, computeOnlyCmdList->close());

    auto computeOnlyQueue = createComputeQueue<FamilyType::gfxCoreFamily>();
    auto computeOnlyCmdListHandle = computeOnlyCmdList->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, computeOnlyQueue->executeCommandLists(1, &computeOnlyCmdListHandle, nullptr, internalOptions));
    EXPECT_EQ(nullptr, computeOnlyQueue->copyOffloadQueue);
    EXPECT_EQ(copyTaskCount + 1, copyCsr->peekTaskCount());
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenSubmittedCopyOffloadStreamWhenDestroyingComputeQueueThenWaitForCopyQueueTaskCountOnCopyCsr, IsAtLeastXeCore) {
    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendCopy(*cmdList, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdList->close());

    auto cmdQueue = createComputeQueue<FamilyType::gfxCoreFamily>();
    auto cmdListHandle = cmdList->toHandle();
    CommandListExecutionInternalOptions internalOptions = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdQueue->executeCommandLists(1, &cmdListHandle, nullptr, internalOptions));

    auto copyQueue = cmdQueue->copyOffloadQueue;
    ASSERT_NE(nullptr, copyQueue);
    const auto copyQueueTaskCount = copyQueue->getTaskCount();
    EXPECT_LT(0u, copyQueueTaskCount);

    auto copyCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(copyQueue->getCsr());
    const auto waitCalledBefore = copyCsr->waitForCompletionWithTimeoutTaskCountCalled.load();

    cmdQueue.reset();

    EXPECT_EQ(waitCalledBefore + 1, copyCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_EQ(copyQueueTaskCount, copyCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
    EXPECT_FALSE(copyCsr->latestWaitForCompletionWithTimeoutWaitParams.enableTimeout);
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenCopyOffloadQueueWithoutSubmissionsWhenDestroyingComputeQueueThenDoNotWaitOnCopyCsr, IsAtLeastXeCore) {
    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendCopy(*cmdList, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdList->close());

    // copy queue is created ahead of submission, without dispatching copy offload stream
    auto cmdQueue = createComputeQueue<FamilyType::gfxCoreFamily>();
    auto cmdListHandle = cmdList->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdQueue->ensureCopyOffloadQueue(1, &cmdListHandle));

    auto copyQueue = cmdQueue->copyOffloadQueue;
    ASSERT_NE(nullptr, copyQueue);
    EXPECT_EQ(0u, copyQueue->getTaskCount());

    auto copyCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(copyQueue->getCsr());
    const auto waitCalledBefore = copyCsr->waitForCompletionWithTimeoutTaskCountCalled.load();

    cmdQueue.reset();

    EXPECT_EQ(waitCalledBefore, copyCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenSharedAllocationUsedByCopyOffloadStreamWhenAppendingCmdListToImmediateCmdListThenAllocationIsMigrated, IsAtLeastXeCore) {
    struct RecordingPageFaultManager : public MockPageFaultManager {
        void moveAllocationToGpuDomain(void *ptr) override {
            migratedPtrs.push_back(ptr);
            MockPageFaultManager::moveAllocationToGpuDomain(ptr);
        }
        std::vector<void *> migratedPtrs;
    };
    auto pageFaultManager = new RecordingPageFaultManager();
    static_cast<NEO::MockMemoryManager *>(neoDevice->getExecutionEnvironment()->memoryManager.get())->pageFaultManager.reset(pageFaultManager);

    void *sharedPtr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, copySize, 1, &sharedPtr));
    usmAllocations.push_back(sharedPtr);
    const auto sharedGpuVa = reinterpret_cast<void *>(getUsmAllocation(sharedPtr)->getGpuAddress());

    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    CmdListMemoryCopyParams copyParams = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, regularCmdList->appendMemoryCopy(allocDeviceUsm(), sharedPtr, copySize, nullptr, 0, nullptr, copyParams));
    ASSERT_TRUE(copyParams.copyOffloadAllowed);
    ASSERT_EQ(ZE_RESULT_SUCCESS, regularCmdList->close());

    // shared allocation is tracked only in copy offload stream residency
    auto &computeResidency = regularCmdList->getCmdContainer().getResidencyContainer();
    EXPECT_EQ(computeResidency.end(), std::find(computeResidency.begin(), computeResidency.end(), getUsmAllocation(sharedPtr)));

    auto regularCmdListHandle = regularCmdList->toHandle();

    // regular submission does not request migration
    {
        auto cmdQueue = createComputeQueue<FamilyType::gfxCoreFamily>();
        CommandListExecutionInternalOptions internalOptions = {};
        ASSERT_EQ(ZE_RESULT_SUCCESS, cmdQueue->executeCommandLists(1, &regularCmdListHandle, nullptr, internalOptions));
        EXPECT_TRUE(pageFaultManager->migratedPtrs.empty());
    }

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    ze_command_queue_desc_t desc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    auto immCmdQueue = new MockCommandQueueHw<FamilyType::gfxCoreFamily>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    immCmdQueue->initialize(false, false, true);
    immCmdList->cmdQImmediate = immCmdQueue; // owned and destroyed by immediate cmd list

    CommandListExecutionInternalOptions internalOptions = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendCommandLists(1, &regularCmdListHandle, nullptr, 0, nullptr, internalOptions));
    ASSERT_NE(nullptr, immCmdQueue->copyOffloadQueue);

    EXPECT_EQ(1, std::count(pageFaultManager->migratedPtrs.begin(), pageFaultManager->migratedPtrs.end(), sharedGpuVa));
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenTbxModeWhenSynchronizingQueueThenAllocationsAreDownloadedOnComputeAndCopyCsr, IsAtLeastXeCore) {
    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendCopy(*cmdList, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdList->close());

    auto cmdQueue = createComputeQueue<FamilyType::gfxCoreFamily>();
    auto cmdListHandle = cmdList->toHandle();
    CommandListExecutionInternalOptions internalOptions = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdQueue->executeCommandLists(1, &cmdListHandle, nullptr, internalOptions));

    auto copyQueue = cmdQueue->copyOffloadQueue;
    ASSERT_NE(nullptr, copyQueue);

    auto computeCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(cmdQueue->getCsr());
    auto copyCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(copyQueue->getCsr());
    computeCsr->waitForTaskCountWithKmdNotifyFallbackReturnValue = NEO::WaitStatus::ready;

    // hardware mode does not download allocations
    EXPECT_EQ(ZE_RESULT_SUCCESS, cmdQueue->L0::CommandQueue::synchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(0u, computeCsr->downloadAllocationsCalledCount.load());
    EXPECT_EQ(0u, copyCsr->downloadAllocationsCalledCount.load());

    computeCsr->setType(NEO::CommandStreamReceiverType::tbx);
    copyCsr->setType(NEO::CommandStreamReceiverType::tbx);

    for (bool useKmdWait : {false, true}) {
        cmdQueue->useKmdWaitFunction = useKmdWait;
        const auto computeDownloadsBefore = computeCsr->downloadAllocationsCalledCount.load();
        const auto copyDownloadsBefore = copyCsr->downloadAllocationsCalledCount.load();
        copyCsr->latestDownloadAllocationsBlocking = false;

        EXPECT_EQ(ZE_RESULT_SUCCESS, cmdQueue->L0::CommandQueue::synchronize(std::numeric_limits<uint64_t>::max()));
        EXPECT_EQ(computeDownloadsBefore + 1, computeCsr->downloadAllocationsCalledCount.load());
        EXPECT_EQ(copyDownloadsBefore + 1, copyCsr->downloadAllocationsCalledCount.load());
        EXPECT_TRUE(copyCsr->latestDownloadAllocationsBlocking.load());
        EXPECT_EQ(copyCsr->peekLatestFlushedTaskCount(), copyCsr->latestDownloadAllocationsTaskCount.load());
    }

    // nothing is downloaded when compute queue wait fails
    cmdQueue->useKmdWaitFunction = false;
    computeCsr->callBaseWaitForCompletionWithTimeout = false;
    computeCsr->returnWaitForCompletionWithTimeout = NEO::WaitStatus::gpuHang;
    const auto computeDownloadsBefore = computeCsr->downloadAllocationsCalledCount.load();
    const auto copyDownloadsBefore = copyCsr->downloadAllocationsCalledCount.load();
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, cmdQueue->L0::CommandQueue::synchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(computeDownloadsBefore, computeCsr->downloadAllocationsCalledCount.load());
    EXPECT_EQ(copyDownloadsBefore, copyCsr->downloadAllocationsCalledCount.load());

    computeCsr->callBaseWaitForCompletionWithTimeout = true;
    computeCsr->setType(NEO::CommandStreamReceiverType::hardware);
    copyCsr->setType(NEO::CommandStreamReceiverType::hardware);
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenNoDualStreamCopyOffloadQueueWhenSynchronizingTbxQueueThenNothingIsDownloadedForCopyOffload, IsAtLeastXeCore) {
    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendKernel(*cmdList));
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdList->close());

    auto cmdQueue = createComputeQueue<FamilyType::gfxCoreFamily>();
    auto cmdListHandle = cmdList->toHandle();
    CommandListExecutionInternalOptions internalOptions = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdQueue->executeCommandLists(1, &cmdListHandle, nullptr, internalOptions));

    EXPECT_EQ(nullptr, cmdQueue->copyOffloadQueue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, cmdQueue->L0::CommandQueue::synchronize(std::numeric_limits<uint64_t>::max()));
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenDualStreamCopyOffloadWhenQueryingFenceThenAllocationsAreDownloadedOnComputeAndCopyCsr, IsAtLeastXeCore) {
    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendCopy(*cmdList, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdList->close());

    auto cmdQueue = createComputeQueue<FamilyType::gfxCoreFamily>();
    ze_fence_desc_t fenceDesc = {ZE_STRUCTURE_TYPE_FENCE_DESC};
    auto fence = Fence::create(cmdQueue.get(), &fenceDesc);
    ASSERT_NE(nullptr, fence);

    auto cmdListHandle = cmdList->toHandle();
    CommandListExecutionInternalOptions internalOptions = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdQueue->executeCommandLists(1, &cmdListHandle, fence->toHandle(), internalOptions));

    auto copyQueue = cmdQueue->copyOffloadQueue;
    ASSERT_NE(nullptr, copyQueue);

    auto computeCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(cmdQueue->getCsr());
    auto copyCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(copyQueue->getCsr());
    computeCsr->testTaskCountReadyReturnValue = true;

    EXPECT_EQ(ZE_RESULT_SUCCESS, fence->queryStatus());
    EXPECT_EQ(1u, computeCsr->downloadAllocationsCalledCount.load());
    EXPECT_EQ(1u, copyCsr->downloadAllocationsCalledCount.load());
    EXPECT_TRUE(copyCsr->latestDownloadAllocationsBlocking.load());
    EXPECT_EQ(copyCsr->peekLatestFlushedTaskCount(), copyCsr->latestDownloadAllocationsTaskCount.load());

    computeCsr->testTaskCountReadyReturnValue.reset();
    fence->destroy();
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenTbxModeWhenSynchronizingImmediateCmdListAfterAppendingRegularCmdListThenDualStreamCopyOffloadAllocationsAreDownloadedOnCopyCsr, IsAtLeastXeCore) {
    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendCopy(*regularCmdList, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, regularCmdList->close());
    auto regularCmdListHandle = regularCmdList->toHandle();

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    ze_command_queue_desc_t desc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    auto immCmdQueue = new MockCommandQueueHw<FamilyType::gfxCoreFamily>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    immCmdQueue->initialize(false, false, true);
    immCmdList->cmdQImmediate = immCmdQueue; // owned and destroyed by immediate cmd list

    CommandListExecutionInternalOptions internalOptions = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendCommandLists(1, &regularCmdListHandle, nullptr, 0, nullptr, internalOptions));

    auto copyQueue = immCmdQueue->copyOffloadQueue;
    ASSERT_NE(nullptr, copyQueue);
    auto computeCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(immCmdQueue->getCsr());
    auto copyCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(copyQueue->getCsr());
    std::vector<TaskCountType> copyCsrDownloadTaskCounts;
    copyCsr->onDownloadAllocations = [&]() { copyCsrDownloadTaskCounts.push_back(copyCsr->latestDownloadAllocationsTaskCount.load()); };
    immCmdList->isTbxMode = true;

    // immediate cmd list own copy offload queue may use the same copy csr
    const bool copyCsrSharedWithImmediateCopyOffload = immCmdList->cmdQImmediateCopyOffload && (immCmdList->cmdQImmediateCopyOffload->getCsr() == copyCsr);
    const size_t expectedCopyCsrDownloads = copyCsrSharedWithImmediateCopyOffload ? 2u : 1u;

    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(1u, computeCsr->downloadAllocationsCalledCount.load());
    ASSERT_EQ(expectedCopyCsrDownloads, copyCsrDownloadTaskCounts.size());
    EXPECT_EQ(copyCsr->peekLatestFlushedTaskCount(), copyCsrDownloadTaskCounts[0]);
    EXPECT_TRUE(copyCsr->latestDownloadAllocationsBlocking.load());

    immCmdList->isTbxMode = false;
    copyCsr->onDownloadAllocations = nullptr;
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenPatchPreambleWhenExecutingCmdListThenQueueAcquiresPatchPreambleCounterAndCopyOffloadStreamWaitsForIt, IsAtLeastXeCore) {
    NEO::debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(0);

    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendCopy(*cmdList, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdList->close());

    auto cmdQueue = createComputeQueue<FamilyType::gfxCoreFamily>();
    cmdQueue->setPatchingPreamble(true);

    auto cmdListHandle = cmdList->toHandle();
    const auto counterNodeAddress = cmdList->getInOrderExecDeviceGpuAddress();

    for (uint64_t execution = 1; execution <= 2; execution++) {
        const auto computeQueueOffset = cmdQueue->commandStream.getUsed();
        const auto copyQueueOffset = cmdQueue->copyOffloadQueue ? static_cast<WhiteBox<L0::CommandQueue> *>(cmdQueue->copyOffloadQueue)->commandStream.getUsed() : 0u;

        CommandListExecutionInternalOptions internalOptions = {};
        ASSERT_EQ(ZE_RESULT_SUCCESS, cmdQueue->executeCommandLists(1, &cmdListHandle, nullptr, internalOptions));

        EXPECT_EQ(execution, cmdQueue->patchPreambleCounter.counter);
        const auto patchPreambleCounterAddress = getPatchPreambleCounterDeviceAddress(*cmdQueue);

        auto copyCmdList = getCopyOffloadCmdList(*cmdList);
        EXPECT_TRUE(copyCmdList->isPatchPreambleEnabled());

        // compute stream patch preamble resets in-order counter and then signals patch preamble counter with barrier post sync
        auto computeCmds = parseStream<FamilyType>(cmdQueue->commandStream, computeQueueOffset);
        auto counterResets = getStoresToAddress<FamilyType>(computeCmds, counterNodeAddress);
        ASSERT_EQ(1u, counterResets.size());
        EXPECT_EQ(0u, counterResets[0]->getDataDword0());
        auto counterResetItor = std::find(computeCmds.begin(), computeCmds.end(), counterResets[0]);
        ASSERT_NE(computeCmds.end(), counterResetItor);

        auto postSyncItor = findPostSyncWrite<FamilyType>(computeCmds, patchPreambleCounterAddress, execution);
        ASSERT_NE(computeCmds.end(), postSyncItor);
        EXPECT_NE(computeCmds.end(), std::find(std::next(counterResetItor), computeCmds.end(), *postSyncItor));
        EXPECT_EQ(0u, getStoresToAddress<FamilyType>(computeCmds, patchPreambleCounterAddress).size());

        ASSERT_NE(nullptr, cmdQueue->copyOffloadQueue);
        auto copyQueue = static_cast<WhiteBox<L0::CommandQueue> *>(cmdQueue->copyOffloadQueue);
        auto copyQueueCmds = parseStream<FamilyType>(copyQueue->commandStream, copyQueueOffset);
        auto patchPreambleCounterWaits = getSemaphoreWaitValues<FamilyType>(copyQueueCmds, patchPreambleCounterAddress);
        ASSERT_EQ(1u, patchPreambleCounterWaits.size());
        EXPECT_EQ(execution, patchPreambleCounterWaits[0]);
    }
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenPatchPreambleRequiredCounterFromUpperLayerWhenExecutingCmdListThenQueueDoesNotAcquireCounterAndCopyOffloadStreamWaitsForProvidedCounter, IsAtLeastXeCore) {
    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendCopy(*cmdList, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdList->close());

    auto cmdQueue = createComputeQueue<FamilyType::gfxCoreFamily>();
    cmdQueue->setPatchingPreamble(true);

    // graph acquires patch preamble counter of the executing queue and uses it for cross sync between segments
    uint64_t requiredCounter = 0;
    uint64_t *hostAddress = nullptr;
    uint64_t hostGpuAddress = 0;
    NEO::GraphicsAllocation *hostAllocation = nullptr;
    uint64_t requiredDeviceGpuAddress = 0;
    NEO::GraphicsAllocation *deviceAllocation = nullptr;
    cmdQueue->getPatchPreambleFullData(requiredCounter, hostAddress, hostGpuAddress, hostAllocation, requiredDeviceGpuAddress, deviceAllocation);

    auto crossSyncAllocation = getUsmAllocation(allocDeviceUsm());
    CountersCrossSyncContainer crossSyncContainer;
    crossSyncContainer.patchPreambleCrossSyncList.push_back({.counter = 5,
                                                             .deviceGpuAddress = crossSyncAllocation->getGpuAddress(),
                                                             .deviceGraphicsAllocation = crossSyncAllocation,
                                                             .appendedCommandListToSyncBefore = cmdList.get()});

    CommandListExecutionInternalOptions internalOptions = {};
    internalOptions.countersCrossSyncContainer = &crossSyncContainer;
    internalOptions.patchPreambleRequiredCounter = requiredCounter;
    internalOptions.patchPreambleRequiredDevicePostSyncGpuAddress = requiredDeviceGpuAddress;

    auto cmdListHandle = cmdList->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdQueue->executeCommandLists(1, &cmdListHandle, nullptr, internalOptions));

    EXPECT_EQ(requiredCounter, cmdQueue->patchPreambleCounter.counter);

    auto computeCmds = parseStream<FamilyType>(cmdQueue->commandStream, 0);
    EXPECT_NE(computeCmds.end(), findPostSyncWrite<FamilyType>(computeCmds, requiredDeviceGpuAddress, requiredCounter));

    // cross sync is resolved by compute stream, copy offload stream waits only for patch preamble counter
    ASSERT_NE(nullptr, cmdQueue->copyOffloadQueue);
    auto copyQueue = static_cast<WhiteBox<L0::CommandQueue> *>(cmdQueue->copyOffloadQueue);
    auto copyQueueCmds = parseStream<FamilyType>(copyQueue->commandStream, 0);
    auto patchPreambleCounterWaits = getSemaphoreWaitValues<FamilyType>(copyQueueCmds, requiredDeviceGpuAddress);
    ASSERT_EQ(1u, patchPreambleCounterWaits.size());
    EXPECT_EQ(requiredCounter, patchPreambleCounterWaits[0]);
    EXPECT_EQ(0u, getSemaphoreWaitValues<FamilyType>(copyQueueCmds, crossSyncAllocation->getGpuAddress()).size());
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenPatchPreambleAndMultipleCmdListsWithCopyOffloadWhenExecutingThenSinglePatchPreambleCounterIsAcquiredAndAllCopyOffloadStreamsWaitForIt, IsAtLeastXeCore) {
    auto cmdList0 = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendCopy(*cmdList0, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdList0->close());
    auto cmdList1 = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendCopy(*cmdList1, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdList1->close());

    auto cmdQueue = createComputeQueue<FamilyType::gfxCoreFamily>();
    cmdQueue->setPatchingPreamble(true);

    ze_command_list_handle_t cmdListHandles[] = {cmdList0->toHandle(), cmdList1->toHandle()};
    CommandListExecutionInternalOptions internalOptions = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdQueue->executeCommandLists(2, cmdListHandles, nullptr, internalOptions));

    EXPECT_EQ(1u, cmdQueue->patchPreambleCounter.counter);
    const auto patchPreambleCounterAddress = getPatchPreambleCounterDeviceAddress(*cmdQueue);

    ASSERT_NE(nullptr, cmdQueue->copyOffloadQueue);
    auto copyQueue = static_cast<WhiteBox<L0::CommandQueue> *>(cmdQueue->copyOffloadQueue);
    auto copyQueueCmds = parseStream<FamilyType>(copyQueue->commandStream, 0);
    auto patchPreambleCounterWaits = getSemaphoreWaitValues<FamilyType>(copyQueueCmds, patchPreambleCounterAddress);
    ASSERT_EQ(2u, patchPreambleCounterWaits.size());
    EXPECT_EQ(1u, patchPreambleCounterWaits[0]);
    EXPECT_EQ(1u, patchPreambleCounterWaits[1]);
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenPatchPreambleAndCmdListWithoutCopyOffloadWhenExecutingThenPatchPreambleCounterIsNotAcquired, IsAtLeastXeCore) {
    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendKernel(*cmdList));
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdList->close());
    ASSERT_EQ(nullptr, cmdList->getCopyOffloadSubCmdList());

    auto cmdQueue = createComputeQueue<FamilyType::gfxCoreFamily>();
    cmdQueue->setPatchingPreamble(true);

    auto cmdListHandle = cmdList->toHandle();
    CommandListExecutionInternalOptions internalOptions = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdQueue->executeCommandLists(1, &cmdListHandle, nullptr, internalOptions));

    EXPECT_EQ(0u, cmdQueue->patchPreambleCounter.counter);
    EXPECT_EQ(nullptr, cmdQueue->patchPreambleCounter.hostCounterNode);
    EXPECT_EQ(nullptr, cmdQueue->copyOffloadQueue);
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenNoPatchPreambleWhenExecutingCmdListThenPatchPreambleCounterIsNotAcquired, IsAtLeastXeCore) {
    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendCopy(*cmdList, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdList->close());

    auto cmdQueue = createComputeQueue<FamilyType::gfxCoreFamily>();
    auto cmdListHandle = cmdList->toHandle();
    CommandListExecutionInternalOptions internalOptions = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdQueue->executeCommandLists(1, &cmdListHandle, nullptr, internalOptions));

    EXPECT_EQ(0u, cmdQueue->patchPreambleCounter.counter);
    EXPECT_EQ(nullptr, cmdQueue->patchPreambleCounter.hostCounterNode);
    EXPECT_FALSE(getCopyOffloadCmdList(*cmdList)->isPatchPreambleEnabled());
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenCbEventWithProfilingAsLatestOperationWhenAppendingCopyThenComputeStreamSignalsCounterBeforeCopyOffloadStreamStarts, IsAtLeastXeCore) {
    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    auto mainStream = cmdList->getCmdContainer().getCommandStream();
    const auto counterAddress = getCounterGpuAddress(*cmdList->inOrderExecInfo);
    const auto incrementValue = cmdList->getInOrderIncrementValue();

    ASSERT_EQ(ZE_RESULT_SUCCESS, appendKernel(*cmdList));
    ASSERT_TRUE(cmdList->hasInOrderDependencies());
    cmdList->latestOperationHasCbEventWithProfiling = true;
    const auto counterAfterKernel = cmdList->inOrderExecInfo->getCounterValue();

    const auto offset = mainStream->getUsed();
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendCopy(*cmdList, nullptr));
    EXPECT_FALSE(cmdList->latestOperationHasCbEventWithProfiling);

    auto mainCmds = parseStream<FamilyType>(*mainStream, offset);
    auto counterSignal = findInOrderCounterSignalPipeControl<FamilyType>(mainCmds, counterAddress);
    ASSERT_NE(nullptr, counterSignal);
    EXPECT_EQ(counterAfterKernel + incrementValue, counterSignal->getImmediateData());

    auto copyCmds = parseStream<FamilyType>(*getCopyOffloadCmdList(*cmdList)->getCmdContainer().getCommandStream(), 0);
    auto waitValues = getSemaphoreWaitValues<FamilyType>(copyCmds, counterAddress);
    ASSERT_EQ(1u, waitValues.size());
    EXPECT_EQ(counterAfterKernel + incrementValue, waitValues[0]);
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenInOrderCounterSignalPendingAsLatestOperationWhenAppendingCopyThenComputeStreamSignalsCounterBeforeCopyOffloadStreamStarts, IsAtLeastXeCore) {
    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    auto mainStream = cmdList->getCmdContainer().getCommandStream();
    const auto counterAddress = getCounterGpuAddress(*cmdList->inOrderExecInfo);
    const auto incrementValue = cmdList->getInOrderIncrementValue();

    ASSERT_EQ(ZE_RESULT_SUCCESS, appendKernel(*cmdList));
    const auto counterAfterKernel = cmdList->inOrderExecInfo->getCounterValue();

    cmdList->inOrderExecInfo->setProgrammedCounterValue(counterAfterKernel - incrementValue);
    ASSERT_TRUE(cmdList->hasInOrderDependencies());
    ASSERT_FALSE(cmdList->latestOperationHasCbEventWithProfiling);
    ASSERT_TRUE(cmdList->isInOrderCounterSignalPending());

    const auto offset = mainStream->getUsed();
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendCopy(*cmdList, nullptr));

    auto mainCmds = parseStream<FamilyType>(*mainStream, offset);
    auto counterSignal = findInOrderCounterSignalPipeControl<FamilyType>(mainCmds, counterAddress);
    ASSERT_NE(nullptr, counterSignal);
    EXPECT_EQ(counterAfterKernel + incrementValue, counterSignal->getImmediateData());

    auto copyCmds = parseStream<FamilyType>(*getCopyOffloadCmdList(*cmdList)->getCmdContainer().getCommandStream(), 0);
    auto waitValues = getSemaphoreWaitValues<FamilyType>(copyCmds, counterAddress);
    ASSERT_EQ(1u, waitValues.size());
    EXPECT_EQ(counterAfterKernel + incrementValue, waitValues[0]);
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenCounterSignaledByLatestOperationWhenAppendingCopyThenComputeStreamDoesNotSignalCounterAndCopyOffloadStreamWaitsForLatestOperation, IsAtLeastXeCore) {
    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    auto mainStream = cmdList->getCmdContainer().getCommandStream();
    const auto counterAddress = getCounterGpuAddress(*cmdList->inOrderExecInfo);
    const auto incrementValue = cmdList->getInOrderIncrementValue();

    ASSERT_EQ(ZE_RESULT_SUCCESS, appendKernel(*cmdList));
    const auto counterAfterKernel = cmdList->inOrderExecInfo->getCounterValue();

    cmdList->inOrderExecInfo->setProgrammedCounterValue(counterAfterKernel);
    ASSERT_TRUE(cmdList->hasInOrderDependencies());
    ASSERT_FALSE(cmdList->latestOperationHasCbEventWithProfiling);
    ASSERT_FALSE(cmdList->isInOrderCounterSignalPending());

    const auto offset = mainStream->getUsed();
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendCopy(*cmdList, nullptr));

    EXPECT_EQ(counterAfterKernel + incrementValue, cmdList->inOrderExecInfo->getCounterValue());

    auto mainCmds = parseStream<FamilyType>(*mainStream, offset);
    EXPECT_EQ(nullptr, findInOrderCounterSignalPipeControl<FamilyType>(mainCmds, counterAddress));
    EXPECT_EQ(0u, getStoresToAddress<FamilyType>(mainCmds, counterAddress).size());

    auto copyCmds = parseStream<FamilyType>(*getCopyOffloadCmdList(*cmdList)->getCmdContainer().getCommandStream(), 0);
    auto waitValues = getSemaphoreWaitValues<FamilyType>(copyCmds, counterAddress);
    ASSERT_EQ(1u, waitValues.size());
    EXPECT_EQ(counterAfterKernel, waitValues[0]);
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenNotDualStreamCopyOffloadOperationWhenBeginningCopyOffloadStreamRecordingThenRecordingIsNotStarted, IsAtLeastXeCore) {
    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    auto mainStream = cmdList->getCmdContainer().getCommandStream();
    const auto mainStreamUsed = mainStream->getUsed();
    const auto counterValue = cmdList->inOrderExecInfo->getCounterValue();

    auto expectRecordingNotStarted = [&](bool copyOffloadOperation) {
        EXPECT_FALSE(cmdList->beginCopyOffloadStreamRecording(copyOffloadOperation));
        EXPECT_FALSE(cmdList->copyOffloadStreamRecordingActive);
        EXPECT_FALSE(cmdList->copyOffloadStreamUsed);
        EXPECT_EQ(nullptr, cmdList->copyOffloadSubCmdList);
        EXPECT_EQ(mainStream, cmdList->getCmdContainer().getCommandStream());
        EXPECT_EQ(mainStreamUsed, mainStream->getUsed());
        EXPECT_EQ(counterValue, cmdList->inOrderExecInfo->getCounterValue());
    };

    expectRecordingNotStarted(false);

    cmdList->copyOffloadMode = CopyOffloadModes::disabled;
    expectRecordingNotStarted(true);
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenCopyOffloadStreamRecordingActiveWhenBeginningRecordingAgainThenNestedRecordingIsNotStarted, IsAtLeastXeCore) {
    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    auto &mainContainer = cmdList->getCmdContainer();
    auto mainStream = mainContainer.getCommandStream();

    ASSERT_TRUE(cmdList->beginCopyOffloadStreamRecording(true));
    EXPECT_TRUE(cmdList->copyOffloadStreamRecordingActive);
    auto copyCmdList = getCopyOffloadCmdList(*cmdList);
    ASSERT_NE(nullptr, copyCmdList);

    auto copyStream = mainContainer.getCommandStream();
    EXPECT_NE(mainStream, copyStream);
    EXPECT_EQ(mainStream, copyCmdList->getCmdContainer().getCommandStream());

    const auto copyStreamUsed = copyStream->getUsed();
    const auto mainStreamUsed = mainStream->getUsed();
    const auto counterValue = cmdList->inOrderExecInfo->getCounterValue();

    EXPECT_FALSE(cmdList->beginCopyOffloadStreamRecording(true));
    EXPECT_TRUE(cmdList->copyOffloadStreamRecordingActive);
    EXPECT_EQ(copyCmdList, getCopyOffloadCmdList(*cmdList));
    EXPECT_EQ(copyStream, mainContainer.getCommandStream());
    EXPECT_EQ(mainStream, copyCmdList->getCmdContainer().getCommandStream());
    EXPECT_EQ(copyStreamUsed, copyStream->getUsed());
    EXPECT_EQ(mainStreamUsed, mainStream->getUsed());
    EXPECT_EQ(counterValue, cmdList->inOrderExecInfo->getCounterValue());

    cmdList->endCopyOffloadStreamRecording();
    EXPECT_FALSE(cmdList->copyOffloadStreamRecordingActive);
    EXPECT_EQ(mainStream, mainContainer.getCommandStream());
    EXPECT_EQ(copyStream, copyCmdList->getCmdContainer().getCommandStream());
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenImplicitInOrderDependenciesSkippedWhenClosingAfterCopyThenComputeStreamDoesNotWaitForCopyOffloadStream, IsAtLeastXeCore) {
    NEO::debugManager.flags.SkipImplicitInOrderDependencies.set(1);

    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    auto mainStream = cmdList->getCmdContainer().getCommandStream();
    const auto counterAddress = getCounterGpuAddress(*cmdList->inOrderExecInfo);

    ASSERT_EQ(ZE_RESULT_SUCCESS, appendCopy(*cmdList, nullptr));
    EXPECT_TRUE(cmdList->latestFlushIsDualCopyOffload);
    EXPECT_FALSE(cmdList->hasInOrderDependencies());

    const auto offset = mainStream->getUsed();
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdList->close());

    auto mainCmds = parseStream<FamilyType>(*mainStream, offset);
    EXPECT_EQ(0u, getSemaphoreWaitValues<FamilyType>(mainCmds, counterAddress).size());
    EXPECT_TRUE(cmdList->latestFlushIsDualCopyOffload);
    EXPECT_TRUE(getCopyOffloadCmdList(*cmdList)->isClosed());
}

template <GFXCORE_FAMILY gfxCoreFamily>
struct FailingInitializeCommandQueueHw : public L0::CommandQueueHw<gfxCoreFamily> {
    using L0::CommandQueueHw<gfxCoreFamily>::CommandQueueHw;

    ze_result_t initialize(bool copyOnly, bool isInternal, bool immediateCmdListQueue) override {
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    static L0::CommandQueue *allocate(L0::Device *device, NEO::CommandStreamReceiver *csr, const ze_command_queue_desc_t *desc) {
        allocatedCsr = csr;
        owningQueueCountAtAllocation = csr->getOwningQueueCount();
        return new FailingInitializeCommandQueueHw<gfxCoreFamily>(device, csr, desc);
    }

    static inline NEO::CommandStreamReceiver *allocatedCsr = nullptr;
    static inline uint32_t owningQueueCountAtAllocation = 0;
};

HWTEST2_F(RegularCmdListCopyOffloadTests, givenCopyQueueCreationFailureWhenExecutingCmdListThenErrorIsReturned, IsAtLeastXeCore) {
    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendCopy(*cmdList, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdList->close());

    auto cmdQueue = createComputeQueue<FamilyType::gfxCoreFamily>();
    auto cmdListHandle = cmdList->toHandle();
    CommandListExecutionInternalOptions internalOptions = {};

    {
        using FailingQueue = FailingInitializeCommandQueueHw<FamilyType::gfxCoreFamily>;
        VariableBackup<NEO::CommandStreamReceiver *> allocatedCsrBackup(&FailingQueue::allocatedCsr, nullptr);
        VariableBackup<uint32_t> owningQueueCountBackup(&FailingQueue::owningQueueCountAtAllocation, 0u);
        VariableBackup<CommandQueueAllocatorFn> commandQueueFactoryBackup(&commandQueueFactory[FamilyType::gfxCoreFamily], FailingQueue::allocate);

        EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, cmdQueue->executeCommandLists(1, &cmdListHandle, nullptr, internalOptions));
        EXPECT_EQ(nullptr, cmdQueue->copyOffloadQueue);
        EXPECT_NE(nullptr, FailingQueue::allocatedCsr);
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, cmdQueue->executeCommandLists(1, &cmdListHandle, nullptr, internalOptions));
    EXPECT_NE(nullptr, cmdQueue->copyOffloadQueue);
}

HWTEST2_F(RegularCmdListCopyOffloadTests, givenNoCopyQueueAndCopyCsrNotAvailableWhenDispatchingCopyOffloadCmdListThenErrorIsReturned, IsAtLeastXeCore) {
    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendCopy(*cmdList, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdList->close());

    auto cmdQueue = createComputeQueue<FamilyType::gfxCoreFamily>();
    ASSERT_EQ(nullptr, cmdQueue->copyOffloadQueue);

    CommandListExecutionContext ctx{};

    {
        DebugManagerStateRestore restorer;
        NEO::debugManager.flags.ForceBcsEngineIndex.set(std::numeric_limits<int32_t>::max());

        using FailingQueue = FailingInitializeCommandQueueHw<FamilyType::gfxCoreFamily>;
        VariableBackup<NEO::CommandStreamReceiver *> allocatedCsrBackup(&FailingQueue::allocatedCsr, nullptr);
        VariableBackup<CommandQueueAllocatorFn> commandQueueFactoryBackup(&commandQueueFactory[FamilyType::gfxCoreFamily], FailingQueue::allocate);

        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, cmdQueue->dispatchCopyOffloadCmdList(cmdList.get(), ctx));
        EXPECT_EQ(nullptr, cmdQueue->copyOffloadQueue);

        EXPECT_EQ(nullptr, FailingQueue::allocatedCsr);
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, cmdQueue->dispatchCopyOffloadCmdList(cmdList.get(), ctx));
    EXPECT_NE(nullptr, cmdQueue->copyOffloadQueue);
}

template <GFXCORE_FAMILY gfxCoreFamily>
struct FailingExecuteCommandQueueHw : public L0::CommandQueueHw<gfxCoreFamily> {
    using L0::CommandQueueHw<gfxCoreFamily>::CommandQueueHw;

    ze_result_t executeCommandLists(uint32_t numCommandLists, ze_command_list_handle_t *phCommandLists, ze_fence_handle_t hFence,
                                    CommandListExecutionInternalOptions &internalOptions) override {
        executeCommandListsCalled++;
        return ZE_RESULT_ERROR_DEVICE_LOST;
    }

    uint32_t executeCommandListsCalled = 0;
};

HWTEST2_F(RegularCmdListCopyOffloadTests, givenCopyOffloadCmdListDispatchFailureWhenExecutingCmdListThenErrorIsPropagatedAndComputeStreamIsNotSubmitted, IsAtLeastXeCore) {
    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendCopy(*cmdList, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdList->close());

    auto cmdQueue = createComputeQueue<FamilyType::gfxCoreFamily>();

    NEO::CommandStreamReceiver *copyCsr = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, device->getCsrForOrdinalAndIndex(&copyCsr, device->getCopyEngineOrdinal(), 0, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, std::nullopt));

    ze_command_queue_desc_t copyQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    copyQueueDesc.ordinal = device->getCopyEngineOrdinal();
    auto failingCopyQueue = new FailingExecuteCommandQueueHw<FamilyType::gfxCoreFamily>(device, copyCsr, &copyQueueDesc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, failingCopyQueue->initialize(true, true, false));

    cmdQueue->copyOffloadQueue = failingCopyQueue;

    auto cmdListHandle = cmdList->toHandle();
    CommandListExecutionInternalOptions internalOptions = {};
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, cmdQueue->executeCommandLists(1, &cmdListHandle, nullptr, internalOptions));

    EXPECT_EQ(1u, failingCopyQueue->executeCommandListsCalled);
    EXPECT_EQ(failingCopyQueue, cmdQueue->copyOffloadQueue);
    EXPECT_EQ(0u, cmdQueue->getTaskCount());
}

struct RegularCmdListCopyOffloadContextGroupTests : public RegularCmdListCopyOffloadTests {
    void SetUp() override {
        NEO::debugManager.flags.ContextGroupSize.set(5);
        RegularCmdListCopyOffloadTests::SetUp();
    }

    bool isCopyEngineQueueOwnershipTaken() {
        NEO::CommandStreamReceiver *copyCsr = nullptr;
        bool queueOwnershipTaken = false;
        auto result = device->getCsrForOrdinalAndIndex(&copyCsr, device->getCopyEngineOrdinal(), 0, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, std::nullopt, 0u, &queueOwnershipTaken);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        if (queueOwnershipTaken) {
            copyCsr->releaseQueueOwnership();
        }
        return queueOwnershipTaken;
    }
};

HWTEST2_F(RegularCmdListCopyOffloadContextGroupTests, givenCopyEngineSecondaryContextWhenExecutingCmdListThenCopyQueueTakesQueueOwnershipUntilDestroyed, IsAtLeastXeCore) {
    if (!isCopyEngineQueueOwnershipTaken()) {
        GTEST_SKIP();
    }

    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendCopy(*cmdList, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdList->close());

    auto cmdQueue = createComputeQueue<FamilyType::gfxCoreFamily>();
    auto cmdListHandle = cmdList->toHandle();
    CommandListExecutionInternalOptions internalOptions = {};

    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdQueue->executeCommandLists(1, &cmdListHandle, nullptr, internalOptions));
    ASSERT_NE(nullptr, cmdQueue->copyOffloadQueue);
    auto copyCsr = cmdQueue->copyOffloadQueue->getCsr();
    EXPECT_EQ(1u, copyCsr->getOwningQueueCount());

    // copy queue is reused, queue ownership is not taken again
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdQueue->executeCommandLists(1, &cmdListHandle, nullptr, internalOptions));
    EXPECT_EQ(1u, copyCsr->getOwningQueueCount());

    cmdQueue.reset();
    EXPECT_EQ(0u, copyCsr->getOwningQueueCount());
}

HWTEST2_F(RegularCmdListCopyOffloadContextGroupTests, givenCopyEngineSecondaryContextAndCopyQueueCreationFailureWhenExecutingCmdListThenQueueOwnershipIsReleasedAndErrorIsReturned, IsAtLeastXeCore) {
    if (!isCopyEngineQueueOwnershipTaken()) {
        GTEST_SKIP();
    }

    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendCopy(*cmdList, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, cmdList->close());

    auto cmdQueue = createComputeQueue<FamilyType::gfxCoreFamily>();
    auto cmdListHandle = cmdList->toHandle();
    CommandListExecutionInternalOptions internalOptions = {};

    using FailingQueue = FailingInitializeCommandQueueHw<FamilyType::gfxCoreFamily>;
    VariableBackup<NEO::CommandStreamReceiver *> allocatedCsrBackup(&FailingQueue::allocatedCsr, nullptr);
    VariableBackup<uint32_t> owningQueueCountBackup(&FailingQueue::owningQueueCountAtAllocation, 0u);
    VariableBackup<CommandQueueAllocatorFn> commandQueueFactoryBackup(&commandQueueFactory[FamilyType::gfxCoreFamily], FailingQueue::allocate);

    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, cmdQueue->executeCommandLists(1, &cmdListHandle, nullptr, internalOptions));
    EXPECT_EQ(nullptr, cmdQueue->copyOffloadQueue);

    // queue ownership taken for copy csr is released after copy queue creation failure
    ASSERT_NE(nullptr, FailingQueue::allocatedCsr);
    EXPECT_EQ(1u, FailingQueue::owningQueueCountAtAllocation);
    EXPECT_EQ(0u, FailingQueue::allocatedCsr->getOwningQueueCount());
}

} // namespace ult
} // namespace L0
