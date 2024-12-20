/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/test/common/helpers/relaxed_ordering_commands_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/in_order_cmd_list_fixture.h"
#include "level_zero/driver_experimental/zex_api.h"

namespace L0 {
namespace ult {
struct CopyOffloadInOrderTests : public InOrderCmdListFixture {
    void SetUp() override {
        debugManager.flags.EnableLocalMemory.set(1);
        backupHwInfo = std::make_unique<VariableBackup<NEO::HardwareInfo>>(defaultHwInfo.get());
        defaultHwInfo->capabilityTable.blitterOperationsSupported = true;
        defaultHwInfo->featureTable.ftrBcsInfo = 0b111;
        InOrderCmdListFixture::SetUp();
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    DestroyableZeUniquePtr<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>> createImmCmdListWithOffload() {
        return createImmCmdListImpl<gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>>(true);
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    DestroyableZeUniquePtr<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>> createMultiTileImmCmdListWithOffload(uint32_t partitionCount) {
        auto cmdList = createImmCmdListWithOffload<gfxCoreFamily>();
        cmdList->partitionCount = partitionCount;
        return cmdList;
    }

    uint32_t copyData1 = 0;
    uint32_t copyData2 = 0;
    std::unique_ptr<VariableBackup<NEO::HardwareInfo>> backupHwInfo;
};

HWTEST2_F(CopyOffloadInOrderTests, givenDebugFlagSetWhenCreatingCmdListThenEnableCopyOffload, IsAtLeastXeHpCore) {
    NEO::debugManager.flags.ForceCopyOperationOffloadForComputeCmdList.set(1);

    ze_command_list_handle_t cmdListHandle;

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
    cmdQueueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdListHandle));
        auto cmdList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>> *>(CommandList::fromHandle(cmdListHandle));
        EXPECT_TRUE(cmdList->copyOperationOffloadEnabled);
        EXPECT_NE(nullptr, cmdList->cmdQImmediateCopyOffload);

        auto queue = static_cast<WhiteBox<L0::CommandQueue> *>(cmdList->cmdQImmediateCopyOffload);
        EXPECT_EQ(cmdQueueDesc.priority, queue->desc.priority);
        EXPECT_EQ(cmdQueueDesc.mode, queue->desc.mode);
        EXPECT_TRUE(queue->peekIsCopyOnlyCommandQueue());
        EXPECT_TRUE(NEO::EngineHelpers::isBcs(queue->getCsr()->getOsContext().getEngineType()));

        zeCommandListDestroy(cmdListHandle);
    }

    {
        cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_HIGH;
        cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdListHandle));
        auto cmdList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>> *>(CommandList::fromHandle(cmdListHandle));
        EXPECT_TRUE(cmdList->copyOperationOffloadEnabled);
        EXPECT_NE(nullptr, cmdList->cmdQImmediateCopyOffload);

        auto queue = static_cast<WhiteBox<L0::CommandQueue> *>(cmdList->cmdQImmediateCopyOffload);
        EXPECT_EQ(cmdQueueDesc.priority, queue->desc.priority);
        EXPECT_EQ(cmdQueueDesc.mode, queue->desc.mode);
        EXPECT_TRUE(queue->peekIsCopyOnlyCommandQueue());
        EXPECT_TRUE(NEO::EngineHelpers::isBcs(queue->getCsr()->getOsContext().getEngineType()));

        zeCommandListDestroy(cmdListHandle);

        cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
        cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    }

    {
        cmdQueueDesc.flags = 0;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdListHandle));
        auto cmdList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>> *>(CommandList::fromHandle(cmdListHandle));
        EXPECT_FALSE(cmdList->copyOperationOffloadEnabled);
        EXPECT_EQ(nullptr, cmdList->cmdQImmediateCopyOffload);

        zeCommandListDestroy(cmdListHandle);

        cmdQueueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    }

    {
        cmdQueueDesc.ordinal = static_cast<DeviceImp *>(device)->getCopyEngineOrdinal();

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdListHandle));
        auto cmdList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>> *>(CommandList::fromHandle(cmdListHandle));
        EXPECT_FALSE(cmdList->copyOperationOffloadEnabled);
        EXPECT_EQ(nullptr, cmdList->cmdQImmediateCopyOffload);

        zeCommandListDestroy(cmdListHandle);

        cmdQueueDesc.ordinal = 0;
    }

    {
        NEO::debugManager.flags.ForceCopyOperationOffloadForComputeCmdList.set(-1);

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdListHandle));
        auto cmdList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>> *>(CommandList::fromHandle(cmdListHandle));
        EXPECT_FALSE(cmdList->copyOperationOffloadEnabled);
        EXPECT_EQ(nullptr, cmdList->cmdQImmediateCopyOffload);

        zeCommandListDestroy(cmdListHandle);
    }
}

HWTEST2_F(CopyOffloadInOrderTests, givenQueueDescriptorWhenCreatingCmdListThenEnableCopyOffload, IsAtLeastXeHpCore) {
    NEO::debugManager.flags.ForceCopyOperationOffloadForComputeCmdList.set(-1);

    ze_command_list_handle_t cmdListHandle;

    zex_intel_queue_copy_operations_offload_hint_exp_desc_t copyOffloadDesc = {ZEX_INTEL_STRUCTURE_TYPE_QUEUE_COPY_OPERATIONS_OFFLOAD_HINT_EXP_PROPERTIES}; // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), NEO-12901
    copyOffloadDesc.copyOffloadEnabled = true;

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
    cmdQueueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    cmdQueueDesc.pNext = &copyOffloadDesc;

    {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdListHandle));
        auto cmdList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>> *>(CommandList::fromHandle(cmdListHandle));
        EXPECT_TRUE(cmdList->copyOperationOffloadEnabled);
        EXPECT_NE(nullptr, cmdList->cmdQImmediateCopyOffload);

        zeCommandListDestroy(cmdListHandle);
    }

    {
        copyOffloadDesc.copyOffloadEnabled = false;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdListHandle));
        auto cmdList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>> *>(CommandList::fromHandle(cmdListHandle));
        EXPECT_FALSE(cmdList->copyOperationOffloadEnabled);
        EXPECT_EQ(nullptr, cmdList->cmdQImmediateCopyOffload);

        zeCommandListDestroy(cmdListHandle);
    }
}

HWTEST2_F(CopyOffloadInOrderTests, givenCopyOffloadEnabledWhenProgrammingHwCmdsThenUseCopyCommands, IsAtLeastXeHpCore) {
    using XY_COPY_BLT = typename std::remove_const<decltype(FamilyType::cmdInitXyCopyBlt)>::type;

    auto immCmdList = createImmCmdListWithOffload<gfxCoreFamily>();
    EXPECT_FALSE(immCmdList->isCopyOnly(false));
    EXPECT_TRUE(immCmdList->isCopyOnly(true));

    auto mainQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto copyQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto initialMainTaskCount = mainQueueCsr->taskCount.load();
    auto initialCopyTaskCount = copyQueueCsr->taskCount.load();

    {
        auto offset = cmdStream->getUsed();

        immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, nullptr, 0, nullptr, copyParams);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto copyItor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
        EXPECT_NE(cmdList.end(), copyItor);

        EXPECT_EQ(initialMainTaskCount, mainQueueCsr->taskCount);
        EXPECT_EQ(initialCopyTaskCount + 1, copyQueueCsr->taskCount);
    }

    {
        auto offset = cmdStream->getUsed();

        ze_copy_region_t region = {0, 0, 0, 1, 1, 1};
        immCmdList->appendMemoryCopyRegion(&copyData1, &region, 1, 1, &copyData2, &region, 1, 1, nullptr, 0, nullptr, copyParams);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto copyItor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), copyItor);

        EXPECT_EQ(initialMainTaskCount, mainQueueCsr->taskCount);
        EXPECT_EQ(initialCopyTaskCount + 2, copyQueueCsr->taskCount);
    }
}

HWTEST2_F(CopyOffloadInOrderTests, givenCopyOffloadEnabledAndD2DAllocWhenProgrammingHwCmdsThenDontUseCopyCommands, IsAtLeastXeHpCore) {
    using XY_COPY_BLT = typename std::remove_const<decltype(FamilyType::cmdInitXyCopyBlt)>::type;

    auto immCmdList = createImmCmdListWithOffload<gfxCoreFamily>();
    EXPECT_FALSE(immCmdList->isCopyOnly(false));
    EXPECT_TRUE(immCmdList->isCopyOnly(true));

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    ze_device_mem_alloc_desc_t deviceDesc = {};
    void *deviceMem = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->allocDeviceMem(device->toHandle(), &deviceDesc, 1, 1, &deviceMem));

    auto mainQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto copyQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));

    auto initialMainTaskCount = mainQueueCsr->taskCount.load();
    auto initialCopyTaskCount = copyQueueCsr->taskCount.load();

    {
        auto offset = cmdStream->getUsed();

        immCmdList->appendMemoryCopy(deviceMem, deviceMem, 1, nullptr, 0, nullptr, copyParams);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto copyItor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(cmdList.end(), copyItor);

        EXPECT_EQ(initialMainTaskCount + 1, mainQueueCsr->taskCount);
        EXPECT_EQ(initialCopyTaskCount, copyQueueCsr->taskCount);
    }

    {
        auto offset = cmdStream->getUsed();

        ze_copy_region_t region = {0, 0, 0, 1, 1, 1};
        immCmdList->appendMemoryCopyRegion(deviceMem, &region, 1, 1, deviceMem, &region, 1, 1, nullptr, 0, nullptr, copyParams);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto copyItor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(cmdList.end(), copyItor);

        EXPECT_EQ(initialMainTaskCount + 2, mainQueueCsr->taskCount);
        EXPECT_EQ(initialCopyTaskCount, copyQueueCsr->taskCount);
    }

    context->freeMem(deviceMem);
}

HWTEST2_F(CopyOffloadInOrderTests, givenProfilingEventWhenAppendingThenUseBcsCommands, IsAtLeastXeHpCore) {
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    auto immCmdList = createImmCmdListWithOffload<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, true);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    auto offset = cmdStream->getUsed();

    auto eventHandle = events[0]->toHandle();

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, eventHandle, 0, nullptr, copyParams);

    ze_copy_region_t region = {0, 0, 0, 1, 1, 1};
    immCmdList->appendMemoryCopyRegion(&copyData1, &region, 1, 1, &copyData2, &region, 1, 1, eventHandle, 0, nullptr, copyParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

    auto pipeControls = findAll<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(0u, pipeControls.size());

    auto miFlushCmds = findAll<typename FamilyType::MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(0u, miFlushCmds.size());

    auto lrrCmds = findAll<MI_LOAD_REGISTER_REG *>(cmdList.begin(), cmdList.end());
    auto lriCmds = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    auto lrmCmds = findAll<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());

    for (auto &lrr : lrrCmds) {
        auto lrrCmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*lrr);
        EXPECT_TRUE(lrrCmd->getSourceRegisterAddress() > RegisterOffsets::bcs0Base);
        EXPECT_TRUE(lrrCmd->getDestinationRegisterAddress() > RegisterOffsets::bcs0Base);
    }

    for (auto &lri : lriCmds) {
        auto lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*lri);
        EXPECT_TRUE(lriCmd->getRegisterOffset() > RegisterOffsets::bcs0Base);
    }

    for (auto &lrm : lrmCmds) {
        auto lrmCmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*lrm);
        EXPECT_TRUE(lrmCmd->getRegisterAddress() > RegisterOffsets::bcs0Base);
    }
}

HWTEST2_F(CopyOffloadInOrderTests, givenProfilingEventWithRelaxedOrderingWhenAppendingThenUseBcsCommands, IsAtLeastXeHpCore) {
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);

    auto immCmdList = createImmCmdListWithOffload<gfxCoreFamily>();

    auto mainQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto copyQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));

    auto mainQueueDirectSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*mainQueueCsr);
    auto offloadDirectSubmission = new MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>>(*copyQueueCsr);

    mainQueueCsr->directSubmission.reset(mainQueueDirectSubmission);
    copyQueueCsr->blitterDirectSubmission.reset(offloadDirectSubmission);

    int client1, client2;

    mainQueueCsr->registerClient(&client1);
    mainQueueCsr->registerClient(&client2);
    copyQueueCsr->registerClient(&client1);
    copyQueueCsr->registerClient(&client2);

    auto eventPool = createEvents<FamilyType>(1, true);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    auto offset = cmdStream->getUsed();

    auto eventHandle = events[0]->toHandle();

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, eventHandle, 0, nullptr, copyParams);

    ze_copy_region_t region = {0, 0, 0, 1, 1, 1};
    immCmdList->appendMemoryCopyRegion(&copyData1, &region, 1, 1, &copyData2, &region, 1, 1, eventHandle, 0, nullptr, copyParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

    auto lrrCmds = findAll<MI_LOAD_REGISTER_REG *>(cmdList.begin(), cmdList.end());
    auto lriCmds = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    auto lrmCmds = findAll<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());

    for (auto &lrr : lrrCmds) {
        auto lrrCmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*lrr);
        EXPECT_TRUE(lrrCmd->getSourceRegisterAddress() > RegisterOffsets::bcs0Base);
        EXPECT_TRUE(lrrCmd->getDestinationRegisterAddress() > RegisterOffsets::bcs0Base);
    }

    for (auto &lri : lriCmds) {
        auto lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*lri);
        EXPECT_TRUE(lriCmd->getRegisterOffset() > RegisterOffsets::bcs0Base);
    }

    for (auto &lrm : lrmCmds) {
        auto lrmCmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*lrm);
        EXPECT_TRUE(lrmCmd->getRegisterAddress() > RegisterOffsets::bcs0Base);
    }
}

HWTEST2_F(CopyOffloadInOrderTests, givenAtomicSignalingModeWhenUpdatingCounterThenUseCorrectHwCommands, IsAtLeastXeHpCore) {
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using ATOMIC_OPCODES = typename FamilyType::MI_ATOMIC::ATOMIC_OPCODES;
    using DATA_SIZE = typename FamilyType::MI_ATOMIC::DATA_SIZE;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    constexpr uint32_t partitionCount = 4;

    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(0);

    auto gmmHelper = device->getNEODevice()->getGmmHelper();

    {
        debugManager.flags.InOrderAtomicSignallingEnabled.set(1);

        auto immCmdList = createMultiTileImmCmdListWithOffload<gfxCoreFamily>(partitionCount);

        auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

        size_t offset = cmdStream->getUsed();

        immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, nullptr, 0, nullptr, copyParams);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        auto miAtomics = findAll<MI_ATOMIC *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(1u, miAtomics.size());

        auto atomicCmd = genCmdCast<MI_ATOMIC *>(*miAtomics[0]);
        ASSERT_NE(nullptr, atomicCmd);

        auto gpuAddress = immCmdList->inOrderExecInfo->getBaseDeviceAddress();

        EXPECT_EQ(gpuAddress, gmmHelper->canonize(NEO::UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*atomicCmd)));
        EXPECT_EQ(ATOMIC_OPCODES::ATOMIC_8B_ADD, atomicCmd->getAtomicOpcode());
        EXPECT_EQ(DATA_SIZE::DATA_SIZE_QWORD, atomicCmd->getDataSize());
        EXPECT_EQ(getLowPart(partitionCount), atomicCmd->getOperand1DataDword0());
        EXPECT_EQ(getHighPart(partitionCount), atomicCmd->getOperand1DataDword1());
    }

    {
        debugManager.flags.InOrderAtomicSignallingEnabled.set(0);

        auto immCmdList = createMultiTileImmCmdListWithOffload<gfxCoreFamily>(partitionCount);

        auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

        size_t offset = cmdStream->getUsed();

        immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, nullptr, 0, nullptr, copyParams);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        auto miStoreDws = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(partitionCount, miStoreDws.size());

        for (uint32_t i = 0; i < partitionCount; i++) {

            auto storeDw = genCmdCast<MI_STORE_DATA_IMM *>(*miStoreDws[i]);
            ASSERT_NE(nullptr, storeDw);

            auto gpuAddress = immCmdList->inOrderExecInfo->getBaseDeviceAddress() + (i * device->getL0GfxCoreHelper().getImmediateWritePostSyncOffset());
            EXPECT_EQ(gpuAddress, gmmHelper->canonize(storeDw->getAddress()));
            EXPECT_EQ(1u, storeDw->getDataDword0());
        }
    }

    {
        debugManager.flags.InOrderAtomicSignallingEnabled.set(0);
        debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(1);

        auto immCmdList = createMultiTileImmCmdListWithOffload<gfxCoreFamily>(partitionCount);

        auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

        size_t offset = cmdStream->getUsed();

        immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, nullptr, 0, nullptr, copyParams);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        auto miStoreDws = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(partitionCount * 2, miStoreDws.size());

        for (uint32_t i = 0; i < partitionCount; i++) {

            auto storeDw = genCmdCast<MI_STORE_DATA_IMM *>(*miStoreDws[i + partitionCount]);
            ASSERT_NE(nullptr, storeDw);

            auto gpuAddress = immCmdList->inOrderExecInfo->getBaseHostGpuAddress() + (i * device->getL0GfxCoreHelper().getImmediateWritePostSyncOffset());
            EXPECT_EQ(gpuAddress, storeDw->getAddress());
            EXPECT_EQ(1u, storeDw->getDataDword0());
        }
    }
}
HWTEST2_F(CopyOffloadInOrderTests, givenDeviceToHostCopyWhenProgrammingThenAddFence, IsAtLeastXeHpcCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;
    using MI_MEM_FENCE = typename GfxFamily::MI_MEM_FENCE;

    ze_result_t result = ZE_RESULT_SUCCESS;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDescHostVisible = {};
    eventDescHostVisible.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));

    auto hostVisibleEvent = DestroyableZeUniquePtr<L0::Event>(Event::create<typename GfxFamily::TimestampPacketType>(eventPool.get(), &eventDescHostVisible, device));

    auto immCmdList = createImmCmdListWithOffload<gfxCoreFamily>();

    constexpr size_t allocSize = 1;
    void *hostBuffer = nullptr;
    void *deviceBuffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_device_mem_alloc_desc_t deviceDesc = {};
    result = context->allocHostMem(&hostDesc, allocSize, allocSize, &hostBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->allocDeviceMem(device->toHandle(), &deviceDesc, allocSize, allocSize, &deviceBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ze_copy_region_t dstRegion = {0, 0, 0, 1, 1, 1};
    ze_copy_region_t srcRegion = {0, 0, 0, 1, 1, 1};

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    size_t offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopyRegion(hostBuffer, &dstRegion, 1, 1, deviceBuffer, &srcRegion, 1, 1, hostVisibleEvent->toHandle(), 0, nullptr, copyParams);

    bool expected = device->getProductHelper().isDeviceToHostCopySignalingFenceRequired();

    GenCmdList genCmdList;
    EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(genCmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

    auto itor = find<XY_COPY_BLT *>(genCmdList.begin(), genCmdList.end());
    itor = find<MI_MEM_FENCE *>(itor, genCmdList.end());
    EXPECT_EQ(expected, genCmdList.end() != itor);

    context->freeMem(hostBuffer);
    context->freeMem(deviceBuffer);
}

HWTEST2_F(CopyOffloadInOrderTests, whenDispatchingSelectCorrectQueueAndCsr, IsAtLeastXeHpcCore) {
    auto regularEventsPool = createEvents<FamilyType>(1, false);

    auto immCmdList = createImmCmdListWithOffload<gfxCoreFamily>();
    auto heaplessStateInit = immCmdList->isHeaplessStateInitEnabled();
    auto regularCsr = static_cast<CommandQueueImp *>(immCmdList->cmdQImmediate)->getCsr();
    auto copyCsr = static_cast<CommandQueueImp *>(immCmdList->cmdQImmediateCopyOffload)->getCsr();

    auto expectedRegularCsrTaskCount = heaplessStateInit ? 1u : 0u;
    auto expectedCopyCsrTaskCount = heaplessStateInit ? 1u : 0u;
    auto expectedImmediateCmdTaskCount = 0u;
    auto expectedCopyOffloadTaskCount = 0u;

    EXPECT_EQ(expectedRegularCsrTaskCount, regularCsr->peekTaskCount());
    EXPECT_EQ(expectedImmediateCmdTaskCount, immCmdList->cmdQImmediate->getTaskCount());
    EXPECT_EQ(expectedCopyCsrTaskCount, copyCsr->peekTaskCount());
    EXPECT_EQ(expectedCopyOffloadTaskCount, immCmdList->cmdQImmediateCopyOffload->getTaskCount());

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0].get(), 0, nullptr, launchParams, false);
    expectedRegularCsrTaskCount++;
    expectedImmediateCmdTaskCount = expectedRegularCsrTaskCount;

    EXPECT_EQ(expectedRegularCsrTaskCount, regularCsr->peekTaskCount());
    EXPECT_EQ(expectedImmediateCmdTaskCount, immCmdList->cmdQImmediate->getTaskCount());
    EXPECT_EQ(expectedCopyCsrTaskCount, copyCsr->peekTaskCount());
    EXPECT_EQ(expectedCopyOffloadTaskCount, immCmdList->cmdQImmediateCopyOffload->getTaskCount());

    EXPECT_EQ(regularCsr, events[0]->csrs[0]);
    EXPECT_EQ(immCmdList->cmdQImmediate, events[0]->latestUsedCmdQueue);

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, events[0].get(), 0, nullptr, copyParams);
    expectedCopyCsrTaskCount++;
    expectedCopyOffloadTaskCount = expectedCopyCsrTaskCount;

    EXPECT_EQ(expectedRegularCsrTaskCount, regularCsr->peekTaskCount());
    EXPECT_EQ(expectedImmediateCmdTaskCount, immCmdList->cmdQImmediate->getTaskCount());
    EXPECT_EQ(expectedCopyCsrTaskCount, copyCsr->peekTaskCount());
    EXPECT_EQ(expectedCopyOffloadTaskCount, immCmdList->cmdQImmediateCopyOffload->getTaskCount());

    EXPECT_EQ(copyCsr, events[0]->csrs[0]);
    EXPECT_EQ(immCmdList->cmdQImmediateCopyOffload, events[0]->latestUsedCmdQueue);
}

HWTEST2_F(CopyOffloadInOrderTests, givenCopyOperationWithHostVisibleEventThenMarkAsNotHostVisibleSubmission, IsAtLeastXeHpcCore) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDescHostVisible = {};
    eventDescHostVisible.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));

    auto hostVisibleEvent = DestroyableZeUniquePtr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDescHostVisible, device));

    auto immCmdList = createImmCmdListWithOffload<gfxCoreFamily>();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, hostVisibleEvent.get(), 0, nullptr, launchParams, false);

    EXPECT_TRUE(immCmdList->latestFlushIsHostVisible);

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, hostVisibleEvent.get(), 0, nullptr, copyParams);

    EXPECT_EQ(!immCmdList->dcFlushSupport, immCmdList->latestFlushIsHostVisible);
}

HWTEST2_F(CopyOffloadInOrderTests, givenRelaxedOrderingEnabledWhenDispatchingThenUseCorrectCsr, IsAtLeastXeHpcCore) {
    class MyMockCmdList : public WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>> {
      public:
        ze_result_t flushImmediate(ze_result_t inputRet, bool performMigration, bool hasStallingCmds, bool hasRelaxedOrderingDependencies, bool kernelOperation, bool copyOffloadSubmission, ze_event_handle_t hSignalEvent, bool requireTaskCountUpdate) override {
            latestRelaxedOrderingMode = hasRelaxedOrderingDependencies;

            return ZE_RESULT_SUCCESS;
        }

        bool latestRelaxedOrderingMode = false;
    };

    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);
    debugManager.flags.DirectSubmissionRelaxedOrderingCounterHeuristic.set(0);

    auto immCmdList = createImmCmdListImpl<gfxCoreFamily, MyMockCmdList>(true);

    auto mainQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto copyQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));

    auto mainQueueDirectSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*mainQueueCsr);
    auto offloadDirectSubmission = new MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>>(*copyQueueCsr);

    mainQueueCsr->directSubmission.reset(mainQueueDirectSubmission);
    copyQueueCsr->blitterDirectSubmission.reset(offloadDirectSubmission);

    int client1, client2;

    // first dependency
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    // compute CSR
    mainQueueCsr->registerClient(&client1);
    mainQueueCsr->registerClient(&client2);

    EXPECT_TRUE(immCmdList->isRelaxedOrderingDispatchAllowed(0, false));
    EXPECT_FALSE(immCmdList->isRelaxedOrderingDispatchAllowed(0, true));

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_TRUE(immCmdList->latestRelaxedOrderingMode);

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, nullptr, 0, nullptr, copyParams);
    EXPECT_FALSE(immCmdList->latestRelaxedOrderingMode);

    // offload CSR
    mainQueueCsr->unregisterClient(&client1);
    mainQueueCsr->unregisterClient(&client2);
    copyQueueCsr->registerClient(&client1);
    copyQueueCsr->registerClient(&client2);

    EXPECT_FALSE(immCmdList->isRelaxedOrderingDispatchAllowed(0, false));
    EXPECT_TRUE(immCmdList->isRelaxedOrderingDispatchAllowed(0, true));

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_FALSE(immCmdList->latestRelaxedOrderingMode);

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, nullptr, 0, nullptr, copyParams);
    EXPECT_TRUE(immCmdList->latestRelaxedOrderingMode);
}

HWTEST2_F(CopyOffloadInOrderTests, givenInOrderModeWhenCallingSyncThenHandleCompletionOnCorrectCsr, IsAtLeastXeHpCore) {
    auto immCmdList = createImmCmdListWithOffload<gfxCoreFamily>();

    auto mainQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto offloadCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));

    EXPECT_NE(mainQueueCsr, offloadCsr);

    auto eventPool = createEvents<FamilyType>(1, false);

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    GraphicsAllocation *expectedAlloc = nullptr;
    uint64_t *hostAddress = nullptr;

    if (inOrderExecInfo->isHostStorageDuplicated()) {
        hostAddress = inOrderExecInfo->getBaseHostAddress();
        expectedAlloc = inOrderExecInfo->getHostCounterAllocation();
    } else {

        expectedAlloc = inOrderExecInfo->getDeviceCounterAllocation();
        hostAddress = static_cast<uint64_t *>(expectedAlloc->getUnderlyingBuffer());
    }
    *hostAddress = 0;

    GraphicsAllocation *mainCsrDownloadedAlloc = nullptr;
    uint32_t mainCsrCallCounter = 0;

    GraphicsAllocation *offloadCsrDownloadedAlloc = nullptr;
    uint32_t offloadCsrCallCounter = 0;

    mainQueueCsr->downloadAllocationImpl = [&](GraphicsAllocation &graphicsAllocation) {
        mainCsrCallCounter++;

        mainCsrDownloadedAlloc = &graphicsAllocation;
    };

    offloadCsr->downloadAllocationImpl = [&](GraphicsAllocation &graphicsAllocation) {
        offloadCsrCallCounter++;

        offloadCsrDownloadedAlloc = &graphicsAllocation;
    };

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    immCmdList->hostSynchronize(0, false);
    EXPECT_EQ(mainCsrDownloadedAlloc, expectedAlloc);
    EXPECT_EQ(offloadCsrDownloadedAlloc, nullptr);
    EXPECT_EQ(1u, mainCsrCallCounter);
    EXPECT_EQ(0u, offloadCsrCallCounter);
    EXPECT_EQ(1u, mainQueueCsr->checkGpuHangDetectedCalled);
    EXPECT_EQ(0u, offloadCsr->checkGpuHangDetectedCalled);

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, events[0].get(), 0, nullptr, copyParams);

    EXPECT_EQ(0u, mainQueueCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_EQ(0u, offloadCsr->waitForCompletionWithTimeoutTaskCountCalled.load());

    immCmdList->hostSynchronize(0, false);

    if (immCmdList->dcFlushSupport) {
        EXPECT_EQ(0u, mainQueueCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
        EXPECT_EQ(1u, offloadCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    } else {
        EXPECT_EQ(mainCsrDownloadedAlloc, expectedAlloc);
        EXPECT_EQ(offloadCsrDownloadedAlloc, expectedAlloc);
        EXPECT_EQ(1u, mainCsrCallCounter);
        EXPECT_EQ(1u, offloadCsrCallCounter);
        EXPECT_EQ(1u, mainQueueCsr->checkGpuHangDetectedCalled);
        EXPECT_EQ(1u, offloadCsr->checkGpuHangDetectedCalled);

        EXPECT_EQ(0u, mainQueueCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
        EXPECT_EQ(0u, offloadCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    }
}

HWTEST2_F(CopyOffloadInOrderTests, givenTbxModeWhenSyncCalledAlwaysDownloadAllocationsFromBothCsrs, IsAtLeastXeHpCore) {
    auto immCmdList = createImmCmdListWithOffload<gfxCoreFamily>();
    immCmdList->isTbxMode = true;

    auto mainQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto offloadCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));

    EXPECT_NE(mainQueueCsr, offloadCsr);

    auto eventPool = createEvents<FamilyType>(1, false);

    if (immCmdList->inOrderExecInfo->isHostStorageDuplicated()) {
        uint64_t *hostAddress = immCmdList->inOrderExecInfo->getBaseHostAddress();
        *hostAddress = 3;
    } else {
        auto deviceAlloc = immCmdList->inOrderExecInfo->getDeviceCounterAllocation();
        auto hostAddress = static_cast<uint64_t *>(deviceAlloc->getUnderlyingBuffer());
        *hostAddress = 3;
    }

    *mainQueueCsr->getTagAddress() = 3;
    *offloadCsr->getTagAddress() = 3;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    EXPECT_EQ(0u, mainQueueCsr->downloadAllocationsCalledCount);
    EXPECT_EQ(0u, offloadCsr->downloadAllocationsCalledCount);

    immCmdList->hostSynchronize(0, false);

    EXPECT_EQ(1u, mainQueueCsr->downloadAllocationsCalledCount);
    EXPECT_EQ(1u, offloadCsr->downloadAllocationsCalledCount);

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, events[0].get(), 0, nullptr, copyParams);

    immCmdList->hostSynchronize(0, false);

    EXPECT_EQ(2u, mainQueueCsr->downloadAllocationsCalledCount);
    EXPECT_EQ(2u, offloadCsr->downloadAllocationsCalledCount);
}

HWTEST2_F(CopyOffloadInOrderTests, givenNonInOrderModeWaitWhenCallingSyncThenHandleCompletionOnCorrectCsr, IsAtLeastXeHpCore) {
    auto immCmdList = createImmCmdListWithOffload<gfxCoreFamily>();

    auto mainQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto offloadCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));

    *mainQueueCsr->getTagAddress() = 3;
    *offloadCsr->getTagAddress() = 3;

    auto mockAlloc = new MockGraphicsAllocation();

    auto internalAllocStorage = mainQueueCsr->getInternalAllocationStorage();
    internalAllocStorage->storeAllocationWithTaskCount(std::move(std::unique_ptr<MockGraphicsAllocation>(mockAlloc)), NEO::AllocationUsage::TEMPORARY_ALLOCATION, 123);

    EXPECT_NE(mainQueueCsr, offloadCsr);

    auto eventPool = createEvents<FamilyType>(1, false);

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    uint64_t *hostAddress = nullptr;
    if (inOrderExecInfo->isHostStorageDuplicated()) {
        hostAddress = inOrderExecInfo->getBaseHostAddress();
    } else {
        hostAddress = static_cast<uint64_t *>(inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer());
    }
    *hostAddress = 0;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    immCmdList->hostSynchronize(0, true);
    EXPECT_EQ(1u, mainQueueCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_EQ(0u, offloadCsr->waitForCompletionWithTimeoutTaskCountCalled.load());

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, events[0].get(), 0, nullptr, copyParams);

    immCmdList->hostSynchronize(0, true);
    EXPECT_EQ(1u, mainQueueCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_EQ(1u, offloadCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
}

HWTEST2_F(CopyOffloadInOrderTests, givenNonInOrderModeWaitWhenCallingSyncThenHandleCompletionAndTempAllocations, IsAtLeastXeHpCore) {
    auto immCmdList = createImmCmdListWithOffload<gfxCoreFamily>();

    auto mainQueueCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    auto offloadCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(true));

    *mainQueueCsr->getTagAddress() = 4;
    *offloadCsr->getTagAddress() = 4;

    auto mainInternalStorage = mainQueueCsr->getInternalAllocationStorage();

    auto offloadInternalStorage = offloadCsr->getInternalAllocationStorage();

    EXPECT_NE(mainQueueCsr, offloadCsr);

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    uint64_t *hostAddress = nullptr;
    if (inOrderExecInfo->isHostStorageDuplicated()) {
        hostAddress = inOrderExecInfo->getBaseHostAddress();
    } else {
        hostAddress = static_cast<uint64_t *>(inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer());
    }
    *hostAddress = 0;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    EXPECT_TRUE(mainInternalStorage->getTemporaryAllocations().peekIsEmpty());
    EXPECT_TRUE(offloadInternalStorage->getTemporaryAllocations().peekIsEmpty());

    mainInternalStorage->storeAllocationWithTaskCount(std::move(std::make_unique<MockGraphicsAllocation>()), NEO::AllocationUsage::TEMPORARY_ALLOCATION, 1);
    offloadInternalStorage->storeAllocationWithTaskCount(std::move(std::make_unique<MockGraphicsAllocation>()), NEO::AllocationUsage::TEMPORARY_ALLOCATION, 1);

    // only main is completed
    immCmdList->hostSynchronize(0, true);
    EXPECT_TRUE(mainInternalStorage->getTemporaryAllocations().peekIsEmpty());
    EXPECT_FALSE(offloadInternalStorage->getTemporaryAllocations().peekIsEmpty());

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, nullptr, 0, nullptr, copyParams);

    EXPECT_TRUE(mainInternalStorage->getTemporaryAllocations().peekIsEmpty());
    EXPECT_FALSE(offloadInternalStorage->getTemporaryAllocations().peekIsEmpty()); // temp allocation created on offload csr

    mainInternalStorage->storeAllocationWithTaskCount(std::move(std::make_unique<MockGraphicsAllocation>()), NEO::AllocationUsage::TEMPORARY_ALLOCATION, 1);

    // both completed
    immCmdList->hostSynchronize(0, true);
    EXPECT_TRUE(mainInternalStorage->getTemporaryAllocations().peekIsEmpty());
    EXPECT_TRUE(offloadInternalStorage->getTemporaryAllocations().peekIsEmpty());

    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, nullptr, 0, nullptr, copyParams);
    EXPECT_TRUE(mainInternalStorage->getTemporaryAllocations().peekIsEmpty());
    EXPECT_FALSE(offloadInternalStorage->getTemporaryAllocations().peekIsEmpty());

    auto mockAlloc = new MockGraphicsAllocation();
    mainInternalStorage->storeAllocationWithTaskCount(std::move(std::unique_ptr<MockGraphicsAllocation>(mockAlloc)), NEO::AllocationUsage::TEMPORARY_ALLOCATION, 123);

    // only copy completed
    immCmdList->hostSynchronize(0, true);
    EXPECT_FALSE(mainInternalStorage->getTemporaryAllocations().peekIsEmpty());
    EXPECT_TRUE(offloadInternalStorage->getTemporaryAllocations().peekIsEmpty());

    mockAlloc->updateTaskCount(1, mainQueueCsr->getOsContext().getContextId());

    immCmdList->hostSynchronize(0, true);
    EXPECT_TRUE(mainInternalStorage->getTemporaryAllocations().peekIsEmpty());
    EXPECT_TRUE(offloadInternalStorage->getTemporaryAllocations().peekIsEmpty());

    // stored only in copy storage
    offloadInternalStorage->storeAllocationWithTaskCount(std::move(std::make_unique<MockGraphicsAllocation>()), NEO::AllocationUsage::TEMPORARY_ALLOCATION, 1);
    immCmdList->hostSynchronize(0, true);
    EXPECT_TRUE(mainInternalStorage->getTemporaryAllocations().peekIsEmpty());
    EXPECT_TRUE(offloadInternalStorage->getTemporaryAllocations().peekIsEmpty());
}

HWTEST2_F(CopyOffloadInOrderTests, givenInterruptEventWhenDispatchingTheProgramUserInterrupt, IsAtLeastXeHpcCore) {
    using MI_USER_INTERRUPT = typename FamilyType::MI_USER_INTERRUPT;

    auto immCmdList = createImmCmdListWithOffload<gfxCoreFamily>();
    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->enableInterruptMode();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    auto offset = cmdStream->getUsed();
    immCmdList->appendMemoryCopy(&copyData1, &copyData2, 1, events[0]->toHandle(), 0, nullptr, copyParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

    auto itor = find<MI_USER_INTERRUPT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
}

using InOrderRegularCmdListTests = InOrderCmdListFixture;

HWTEST2_F(InOrderRegularCmdListTests, givenInOrderFlagWhenCreatingCmdListThenEnableInOrderMode, MatchAny) {
    ze_command_list_desc_t cmdListDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    cmdListDesc.flags = ZE_COMMAND_LIST_FLAG_IN_ORDER;

    ze_command_list_handle_t cmdList;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreate(context, device, &cmdListDesc, &cmdList));

    EXPECT_TRUE(static_cast<CommandListCoreFamily<gfxCoreFamily> *>(cmdList)->isInOrderExecutionEnabled());

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListDestroy(cmdList));
}

HWTEST2_F(InOrderRegularCmdListTests, whenUsingRegularCmdListThenAddCmdsToPatch, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    ze_command_queue_desc_t desc = {};

    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(true, false, false);
    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(true);

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();

    size_t offset = cmdStream->getUsed();

    uint32_t copyData = 0;

    regularCmdList->appendMemoryCopy(&copyData, &copyData, 1, nullptr, 0, nullptr, copyParams);

    EXPECT_EQ(1u, regularCmdList->inOrderPatchCmds.size()); // SDI

    auto sdiFromContainer1 = genCmdCast<MI_STORE_DATA_IMM *>(regularCmdList->inOrderPatchCmds[0].cmd1);
    ASSERT_NE(nullptr, sdiFromContainer1);
    MI_STORE_DATA_IMM *sdiFromParser1 = nullptr;

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto itor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), itor);

        sdiFromParser1 = genCmdCast<MI_STORE_DATA_IMM *>(*itor);
    }

    offset = cmdStream->getUsed();
    regularCmdList->appendMemoryCopy(&copyData, &copyData, 1, nullptr, 0, nullptr, copyParams);
    ASSERT_EQ(3u, regularCmdList->inOrderPatchCmds.size()); // SDI + Semaphore/2xLRI + SDI

    MI_SEMAPHORE_WAIT *semaphoreFromParser2 = nullptr;
    MI_SEMAPHORE_WAIT *semaphoreFromContainer2 = nullptr;

    MI_LOAD_REGISTER_IMM *firstLriFromContainer2 = nullptr;
    MI_LOAD_REGISTER_IMM *secondLriFromContainer2 = nullptr;

    MI_LOAD_REGISTER_IMM *firstLriFromParser2 = nullptr;
    MI_LOAD_REGISTER_IMM *secondLriFromParser2 = nullptr;

    if (regularCmdList->isQwordInOrderCounter()) {
        firstLriFromContainer2 = genCmdCast<MI_LOAD_REGISTER_IMM *>(regularCmdList->inOrderPatchCmds[1].cmd1);
        ASSERT_NE(nullptr, firstLriFromContainer2);
        secondLriFromContainer2 = genCmdCast<MI_LOAD_REGISTER_IMM *>(regularCmdList->inOrderPatchCmds[1].cmd2);
        ASSERT_NE(nullptr, secondLriFromContainer2);
    } else {
        semaphoreFromContainer2 = genCmdCast<MI_SEMAPHORE_WAIT *>(regularCmdList->inOrderPatchCmds[1].cmd1);
        EXPECT_EQ(nullptr, regularCmdList->inOrderPatchCmds[1].cmd2);
        ASSERT_NE(nullptr, semaphoreFromContainer2);
    }

    auto sdiFromContainer2 = genCmdCast<MI_STORE_DATA_IMM *>(regularCmdList->inOrderPatchCmds[2].cmd1);
    ASSERT_NE(nullptr, sdiFromContainer2);
    MI_STORE_DATA_IMM *sdiFromParser2 = nullptr;

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto itor = cmdList.begin();

        if (regularCmdList->isQwordInOrderCounter()) {
            itor = find<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
            ASSERT_NE(cmdList.end(), itor);

            firstLriFromParser2 = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itor);
            ASSERT_NE(nullptr, firstLriFromParser2);
            secondLriFromParser2 = genCmdCast<MI_LOAD_REGISTER_IMM *>(*(++itor));
            ASSERT_NE(nullptr, secondLriFromParser2);
        } else {
            auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
            ASSERT_NE(cmdList.end(), itor);

            semaphoreFromParser2 = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);
            ASSERT_NE(nullptr, semaphoreFromParser2);
        }

        auto sdiItor = find<MI_STORE_DATA_IMM *>(itor, cmdList.end());
        ASSERT_NE(cmdList.end(), sdiItor);

        sdiFromParser2 = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);
    }

    EXPECT_EQ(2u, regularCmdList->inOrderExecInfo->getCounterValue());

    auto verifyPatching = [&](uint64_t executionCounter) {
        auto appendValue = regularCmdList->inOrderExecInfo->getCounterValue() * executionCounter;

        EXPECT_EQ(getLowPart(1u + appendValue), sdiFromContainer1->getDataDword0());
        EXPECT_EQ(getLowPart(1u + appendValue), sdiFromParser1->getDataDword0());

        if (regularCmdList->isQwordInOrderCounter()) {
            EXPECT_EQ(getHighPart(1u + appendValue), sdiFromContainer1->getDataDword1());
            EXPECT_EQ(getHighPart(1u + appendValue), sdiFromParser1->getDataDword1());

            EXPECT_TRUE(sdiFromContainer1->getStoreQword());
            EXPECT_TRUE(sdiFromParser1->getStoreQword());

            EXPECT_EQ(getLowPart(1u + appendValue), firstLriFromContainer2->getDataDword());
            EXPECT_EQ(getLowPart(1u + appendValue), firstLriFromParser2->getDataDword());

            EXPECT_EQ(getHighPart(1u + appendValue), secondLriFromContainer2->getDataDword());
            EXPECT_EQ(getHighPart(1u + appendValue), secondLriFromParser2->getDataDword());
        } else {
            EXPECT_FALSE(sdiFromContainer1->getStoreQword());
            EXPECT_FALSE(sdiFromParser1->getStoreQword());

            EXPECT_EQ(1u + appendValue, semaphoreFromContainer2->getSemaphoreDataDword());
            EXPECT_EQ(1u + appendValue, semaphoreFromParser2->getSemaphoreDataDword());
        }

        EXPECT_EQ(getLowPart(2u + appendValue), sdiFromContainer2->getDataDword0());
        EXPECT_EQ(getLowPart(2u + appendValue), sdiFromParser2->getDataDword0());

        if (regularCmdList->isQwordInOrderCounter()) {
            EXPECT_EQ(getHighPart(2u + appendValue), sdiFromContainer2->getDataDword1());
            EXPECT_EQ(getHighPart(2u + appendValue), sdiFromParser2->getDataDword1());

            EXPECT_TRUE(sdiFromContainer2->getStoreQword());
            EXPECT_TRUE(sdiFromParser2->getStoreQword());
        } else {
            EXPECT_FALSE(sdiFromContainer2->getStoreQword());
            EXPECT_FALSE(sdiFromParser2->getStoreQword());
        }
    };

    regularCmdList->close();

    auto handle = regularCmdList->toHandle();

    mockCmdQHw->executeCommandLists(1, &handle, nullptr, false, nullptr);
    verifyPatching(0);

    mockCmdQHw->executeCommandLists(1, &handle, nullptr, false, nullptr);
    verifyPatching(1);

    mockCmdQHw->executeCommandLists(1, &handle, nullptr, false, nullptr);
    verifyPatching(2);

    if (regularCmdList->isQwordInOrderCounter()) {
        regularCmdList->inOrderExecInfo->addRegularCmdListSubmissionCounter(static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 3);
        mockCmdQHw->executeCommandLists(1, &handle, nullptr, false, nullptr);

        verifyPatching(regularCmdList->inOrderExecInfo->getRegularCmdListSubmissionCounter() - 1);
    }
}

HWTEST2_F(InOrderRegularCmdListTests, givenCrossRegularCmdListDependenciesWhenExecutingThenDontPatchWhenExecutedOnlyOnce, MatchAny) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    ze_command_queue_desc_t desc = {};

    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(true, false, false);

    auto regularCmdList1 = createRegularCmdList<gfxCoreFamily>(false);
    auto regularCmdList2 = createRegularCmdList<gfxCoreFamily>(false);

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    regularCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    regularCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    regularCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    regularCmdList1->close();

    uint64_t baseEventWaitValue = 3;

    auto implicitCounterGpuVa = regularCmdList2->inOrderExecInfo->getBaseDeviceAddress();
    auto externalCounterGpuVa = regularCmdList1->inOrderExecInfo->getBaseDeviceAddress();

    auto cmdStream2 = regularCmdList2->getCmdContainer().getCommandStream();

    size_t offset2 = cmdStream2->getUsed();

    regularCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    regularCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams, false);
    regularCmdList2->close();

    size_t sizeToParse2 = cmdStream2->getUsed();

    auto verifyPatching = [&](uint64_t expectedImplicitDependencyValue, uint64_t expectedExplicitDependencyValue) {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream2->getCpuBase(), offset2), (sizeToParse2 - offset2)));

        auto semaphoreCmds = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(2u, semaphoreCmds.size());

        if (regularCmdList1->isQwordInOrderCounter()) {
            // verify 2x LRI before semaphore
            std::advance(semaphoreCmds[0], -2);
            std::advance(semaphoreCmds[1], -2);
        }

        ASSERT_TRUE(verifyInOrderDependency<FamilyType>(semaphoreCmds[0], expectedImplicitDependencyValue, implicitCounterGpuVa, regularCmdList1->isQwordInOrderCounter(), false));
        ASSERT_TRUE(verifyInOrderDependency<FamilyType>(semaphoreCmds[1], expectedExplicitDependencyValue, externalCounterGpuVa, regularCmdList1->isQwordInOrderCounter(), false));
    };

    auto cmdListHandle1 = regularCmdList1->toHandle();
    auto cmdListHandle2 = regularCmdList2->toHandle();

    mockCmdQHw->executeCommandLists(1, &cmdListHandle2, nullptr, false, nullptr);
    mockCmdQHw->executeCommandLists(1, &cmdListHandle2, nullptr, false, nullptr);
    mockCmdQHw->executeCommandLists(1, &cmdListHandle2, nullptr, false, nullptr);

    verifyPatching(5, baseEventWaitValue);

    mockCmdQHw->executeCommandLists(1, &cmdListHandle1, nullptr, false, nullptr);
    mockCmdQHw->executeCommandLists(1, &cmdListHandle2, nullptr, false, nullptr);

    verifyPatching(7, baseEventWaitValue);
}

HWTEST2_F(InOrderRegularCmdListTests, givenCrossRegularCmdListDependenciesWhenExecutingThenPatchWhenExecutedMultipleTimes, MatchAny) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    ze_command_queue_desc_t desc = {};

    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(true, false, false);

    auto regularCmdList1 = createRegularCmdList<gfxCoreFamily>(false);
    auto regularCmdList2 = createRegularCmdList<gfxCoreFamily>(false);

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    regularCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    regularCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    regularCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    regularCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    regularCmdList1->close();

    uint64_t baseEventWaitValue = 3;

    auto implicitCounterGpuVa = regularCmdList2->inOrderExecInfo->getBaseDeviceAddress();
    auto externalCounterGpuVa = regularCmdList1->inOrderExecInfo->getBaseDeviceAddress();

    auto cmdListHandle1 = regularCmdList1->toHandle();
    auto cmdListHandle2 = regularCmdList2->toHandle();

    mockCmdQHw->executeCommandLists(1, &cmdListHandle1, nullptr, false, nullptr);
    mockCmdQHw->executeCommandLists(1, &cmdListHandle1, nullptr, false, nullptr);
    mockCmdQHw->executeCommandLists(1, &cmdListHandle1, nullptr, false, nullptr);

    auto cmdStream2 = regularCmdList2->getCmdContainer().getCommandStream();

    size_t offset2 = cmdStream2->getUsed();
    size_t sizeToParse2 = 0;

    auto verifyPatching = [&](uint64_t expectedImplicitDependencyValue, uint64_t expectedExplicitDependencyValue) {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream2->getCpuBase(), offset2), (sizeToParse2 - offset2)));

        auto semaphoreCmds = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(2u, semaphoreCmds.size());

        if (regularCmdList1->isQwordInOrderCounter()) {
            // verify 2x LRI before semaphore
            std::advance(semaphoreCmds[0], -2);
            std::advance(semaphoreCmds[1], -2);
        }

        ASSERT_TRUE(verifyInOrderDependency<FamilyType>(semaphoreCmds[0], expectedImplicitDependencyValue, implicitCounterGpuVa, regularCmdList1->isQwordInOrderCounter(), false));
        ASSERT_TRUE(verifyInOrderDependency<FamilyType>(semaphoreCmds[1], expectedExplicitDependencyValue, externalCounterGpuVa, regularCmdList1->isQwordInOrderCounter(), false));
    };

    regularCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    regularCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams, false);
    regularCmdList2->close();

    sizeToParse2 = cmdStream2->getUsed();

    verifyPatching(1, baseEventWaitValue);

    mockCmdQHw->executeCommandLists(1, &cmdListHandle2, nullptr, false, nullptr);

    verifyPatching(1, baseEventWaitValue + (2 * regularCmdList1->inOrderExecInfo->getCounterValue()));

    mockCmdQHw->executeCommandLists(1, &cmdListHandle2, nullptr, false, nullptr);
    mockCmdQHw->executeCommandLists(1, &cmdListHandle2, nullptr, false, nullptr);

    verifyPatching(5, baseEventWaitValue + (2 * regularCmdList1->inOrderExecInfo->getCounterValue()));

    mockCmdQHw->executeCommandLists(1, &cmdListHandle1, nullptr, false, nullptr);
    mockCmdQHw->executeCommandLists(1, &cmdListHandle2, nullptr, false, nullptr);

    verifyPatching(7, baseEventWaitValue + (3 * regularCmdList1->inOrderExecInfo->getCounterValue()));
}

HWTEST2_F(InOrderRegularCmdListTests, givenDebugFlagSetWhenUsingRegularCmdListThenDontAddCmdsToPatch, IsAtLeastXeHpCore) {
    debugManager.flags.EnableInOrderRegularCmdListPatching.set(0);

    ze_command_queue_desc_t desc = {};

    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(true, false, false);
    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(true);

    uint32_t copyData = 0;

    regularCmdList->appendMemoryCopy(&copyData, &copyData, 1, nullptr, 0, nullptr, copyParams);

    EXPECT_EQ(0u, regularCmdList->inOrderPatchCmds.size());
}

HWTEST2_F(InOrderRegularCmdListTests, whenUsingRegularCmdListThenAddWalkerToPatch, IsAtLeastXeHpCore) {
    using WalkerVariant = typename FamilyType::WalkerVariant;

    ze_command_queue_desc_t desc = {};

    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(true, false, false);

    if (mockCmdQHw->heaplessModeEnabled) {
        GTEST_SKIP();
    }

    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(false);
    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();

    size_t offset = cmdStream->getUsed();

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    ASSERT_EQ(3u, regularCmdList->inOrderPatchCmds.size()); // Walker + Semaphore + Walker

    WalkerVariant walkerVariantFromContainer1 = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(regularCmdList->inOrderPatchCmds[0].cmd1);
    WalkerVariant walkerVariantFromContainer2 = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(regularCmdList->inOrderPatchCmds[2].cmd1);
    std::visit([](auto &&walker1, auto &&walker2) {
        ASSERT_NE(nullptr, walker1);
        ASSERT_NE(nullptr, walker2);
    },
               walkerVariantFromContainer1, walkerVariantFromContainer2);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto itor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);
    WalkerVariant walkerVariantFromParser1 = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(*itor);

    itor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(++itor, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);
    WalkerVariant walkerVariantFromParser2 = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(*itor);

    std::visit([&](auto &&walkerFromParser1, auto &&walkerFromParser2, auto &&walkerFromContainer1, auto &&walkerFromContainer2) {
        auto verifyPatching = [&](uint64_t executionCounter) {
            auto appendValue = regularCmdList->inOrderExecInfo->getCounterValue() * executionCounter;

            EXPECT_EQ(1u + appendValue, walkerFromContainer1->getPostSync().getImmediateData());
            EXPECT_EQ(1u + appendValue, walkerFromParser1->getPostSync().getImmediateData());

            EXPECT_EQ(2u + appendValue, walkerFromContainer2->getPostSync().getImmediateData());
            EXPECT_EQ(2u + appendValue, walkerFromParser2->getPostSync().getImmediateData());
        };

        regularCmdList->close();

        auto handle = regularCmdList->toHandle();

        mockCmdQHw->executeCommandLists(1, &handle, nullptr, false, nullptr);
        verifyPatching(0);

        mockCmdQHw->executeCommandLists(1, &handle, nullptr, false, nullptr);
        verifyPatching(1);

        mockCmdQHw->executeCommandLists(1, &handle, nullptr, false, nullptr);
        verifyPatching(2);
    },
               walkerVariantFromParser1, walkerVariantFromParser2, walkerVariantFromContainer1, walkerVariantFromContainer2);

    EXPECT_EQ(2u, regularCmdList->inOrderExecInfo->getCounterValue());
}

HWTEST2_F(InOrderRegularCmdListTests, givenInOrderModeWhenDispatchingRegularCmdListThenProgramPipeControlsToHandleDependencies, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using WalkerVariant = typename FamilyType::WalkerVariant;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(false);

    if (regularCmdList->isHeaplessModeEnabled()) {
        GTEST_SKIP();
    }

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();

    size_t offset = cmdStream->getUsed();

    EXPECT_EQ(0u, regularCmdList->inOrderExecInfo->getCounterValue());
    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(1u, regularCmdList->inOrderExecInfo->getCounterValue());

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto walkerItor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), walkerItor);

        WalkerVariant walkerVariant = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(*walkerItor);
        std::visit([&regularCmdList](auto &&walker) {
            auto &postSync = walker->getPostSync();
            using PostSyncType = std::decay_t<decltype(postSync)>;

            EXPECT_EQ(PostSyncType::OPERATION::OPERATION_WRITE_IMMEDIATE_DATA, postSync.getOperation());
            EXPECT_EQ(1u, postSync.getImmediateData());
            EXPECT_EQ(regularCmdList->inOrderExecInfo->getBaseDeviceAddress(), postSync.getDestinationAddress());
        },
                   walkerVariant);

        auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(cmdList.end(), sdiItor);
    }

    offset = cmdStream->getUsed();

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(2u, regularCmdList->inOrderExecInfo->getCounterValue());

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));
        auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
        EXPECT_NE(cmdList.end(), semaphoreItor);

        auto walkerItor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(semaphoreItor, cmdList.end());
        ASSERT_NE(cmdList.end(), walkerItor);

        WalkerVariant walkerVariant = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(*walkerItor);
        std::visit([&regularCmdList](auto &&walker) {
            auto &postSync = walker->getPostSync();
            using PostSyncType = std::decay_t<decltype(postSync)>;

            EXPECT_EQ(PostSyncType::OPERATION::OPERATION_WRITE_IMMEDIATE_DATA, postSync.getOperation());
            EXPECT_EQ(2u, postSync.getImmediateData());
            EXPECT_EQ(regularCmdList->inOrderExecInfo->getBaseDeviceAddress(), postSync.getDestinationAddress());
        },
                   walkerVariant);

        auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(cmdList.end(), sdiItor);
    }

    regularCmdList->inOrderExecInfo->setAllocationOffset(123);
    auto hostAddr = static_cast<uint64_t *>(regularCmdList->inOrderExecInfo->getBaseHostAddress());
    *hostAddr = 0x1234;
    regularCmdList->latestOperationRequiredNonWalkerInOrderCmdsChaining = true;

    auto originalInOrderExecInfo = regularCmdList->inOrderExecInfo;

    regularCmdList->reset();
    EXPECT_NE(originalInOrderExecInfo.get(), regularCmdList->inOrderExecInfo.get());
    EXPECT_EQ(0u, regularCmdList->inOrderExecInfo->getCounterValue());
    EXPECT_EQ(0u, regularCmdList->inOrderExecInfo->getAllocationOffset());
    hostAddr = static_cast<uint64_t *>(regularCmdList->inOrderExecInfo->getBaseHostAddress());
    EXPECT_EQ(0u, *hostAddr);
    EXPECT_FALSE(regularCmdList->latestOperationRequiredNonWalkerInOrderCmdsChaining);
}

HWTEST2_F(InOrderRegularCmdListTests, givenInOrderModeWhenDispatchingRegularCmdListThenUpdateCounterAllocation, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto eventPool = createEvents<FamilyType>(1, true);
    auto eventHandle = events[0]->toHandle();
    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());

    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(false);
    auto regularCopyOnlyCmdList = createRegularCmdList<gfxCoreFamily>(true);

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();
    auto copyOnlyCmdStream = regularCopyOnlyCmdList->getCmdContainer().getCommandStream();

    size_t offset = cmdStream->getUsed();

    EXPECT_EQ(0u, regularCmdList->inOrderExecInfo->getCounterValue());
    EXPECT_NE(nullptr, regularCmdList->inOrderExecInfo.get());

    constexpr size_t size = 128 * sizeof(uint32_t);
    auto data = allocHostMem(size);

    ze_copy_region_t region = {0, 0, 0, 1, 1, 1};

    regularCmdList->appendMemoryCopyRegion(data, &region, 1, 1, data, &region, 1, 1, nullptr, 0, nullptr, copyParams);

    regularCmdList->appendMemoryFill(data, data, 1, size, nullptr, 0, nullptr, false);

    regularCmdList->appendSignalEvent(eventHandle, false);

    regularCmdList->appendBarrier(nullptr, 1, &eventHandle, false);

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto sdiCmds = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(2u, sdiCmds.size());
    }

    offset = copyOnlyCmdStream->getUsed();
    regularCopyOnlyCmdList->appendMemoryFill(data, data, 1, size, nullptr, 0, nullptr, false);

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(copyOnlyCmdStream->getCpuBase(), offset),
                                                          (copyOnlyCmdStream->getUsed() - offset)));

        auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
        EXPECT_NE(cmdList.end(), sdiItor);
    }

    context->freeMem(data);
}

using InOrderRegularCopyOnlyCmdListTests = InOrderCmdListFixture;

HWTEST2_F(InOrderRegularCopyOnlyCmdListTests, givenInOrderModeWhenDispatchingRegularCmdListThenDontProgramBarriers, IsAtLeastXeHpCore) {
    using XY_COPY_BLT = typename std::remove_const<decltype(FamilyType::cmdInitXyCopyBlt)>::type;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(true);

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();

    size_t offset = cmdStream->getUsed();

    auto alignedPtr = alignedMalloc(MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize);

    regularCmdList->appendMemoryCopy(alignedPtr, alignedPtr, MemoryConstants::cacheLineSize, nullptr, 0, nullptr, copyParams);

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
        EXPECT_NE(cmdList.end(), sdiItor);

        auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

        ASSERT_NE(nullptr, sdiCmd);

        auto gpuAddress = regularCmdList->inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(regularCmdList->inOrderExecInfo->getBaseHostAddress()) : regularCmdList->inOrderExecInfo->getBaseDeviceAddress();

        EXPECT_EQ(gpuAddress, sdiCmd->getAddress());
        EXPECT_EQ(regularCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
        EXPECT_EQ(1u, sdiCmd->getDataDword0());
        EXPECT_EQ(0u, sdiCmd->getDataDword1());
    }

    offset = cmdStream->getUsed();

    regularCmdList->appendMemoryCopy(alignedPtr, alignedPtr, MemoryConstants::cacheLineSize, nullptr, 0, nullptr, copyParams);

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto itor = cmdList.begin();
        if (regularCmdList->isQwordInOrderCounter()) {
            std::advance(itor, 2); // 2x LRI before semaphore
        }
        EXPECT_NE(nullptr, genCmdCast<MI_SEMAPHORE_WAIT *>(*itor));

        itor++;
        auto copyCmd = genCmdCast<XY_COPY_BLT *>(*itor);

        EXPECT_NE(nullptr, copyCmd);

        auto sdiItor = find<MI_STORE_DATA_IMM *>(itor, cmdList.end());
        EXPECT_NE(cmdList.end(), sdiItor);

        auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

        ASSERT_NE(nullptr, sdiCmd);

        auto gpuAddress = regularCmdList->inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(regularCmdList->inOrderExecInfo->getBaseHostAddress()) : regularCmdList->inOrderExecInfo->getBaseDeviceAddress();

        EXPECT_EQ(gpuAddress, sdiCmd->getAddress());
        EXPECT_EQ(regularCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
        EXPECT_EQ(2u, sdiCmd->getDataDword0());
        EXPECT_EQ(0u, sdiCmd->getDataDword1());
    }

    alignedFree(alignedPtr);
}

HWTEST2_F(InOrderRegularCmdListTests, givenNonInOrderRegularCmdListWhenPassingCounterBasedEventToWaitThenPatchOnExecute, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    ze_command_queue_desc_t desc = {};

    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(false, false, false);
    auto inOrderRegularCmdList = createRegularCmdList<gfxCoreFamily>(false);
    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(false);
    regularCmdList->inOrderExecInfo.reset();

    inOrderRegularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();

    size_t offset = cmdStream->getUsed();
    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams, false);
    ASSERT_EQ(1u, regularCmdList->inOrderPatchCmds.size());

    MI_SEMAPHORE_WAIT *semaphoreFromParser2 = nullptr;
    MI_SEMAPHORE_WAIT *semaphoreFromContainer2 = nullptr;

    MI_LOAD_REGISTER_IMM *firstLriFromContainer2 = nullptr;
    MI_LOAD_REGISTER_IMM *secondLriFromContainer2 = nullptr;

    MI_LOAD_REGISTER_IMM *firstLriFromParser2 = nullptr;
    MI_LOAD_REGISTER_IMM *secondLriFromParser2 = nullptr;

    if (regularCmdList->isQwordInOrderCounter()) {
        firstLriFromContainer2 = genCmdCast<MI_LOAD_REGISTER_IMM *>(regularCmdList->inOrderPatchCmds[0].cmd1);
        ASSERT_NE(nullptr, firstLriFromContainer2);
        secondLriFromContainer2 = genCmdCast<MI_LOAD_REGISTER_IMM *>(regularCmdList->inOrderPatchCmds[0].cmd2);
        ASSERT_NE(nullptr, secondLriFromContainer2);
    } else {
        semaphoreFromContainer2 = genCmdCast<MI_SEMAPHORE_WAIT *>(regularCmdList->inOrderPatchCmds[0].cmd1);
        EXPECT_EQ(nullptr, regularCmdList->inOrderPatchCmds[0].cmd2);
        ASSERT_NE(nullptr, semaphoreFromContainer2);
    }

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto itor = cmdList.begin();

        if (regularCmdList->isQwordInOrderCounter()) {
            itor = find<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
            ASSERT_NE(cmdList.end(), itor);

            firstLriFromParser2 = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itor);
            ASSERT_NE(nullptr, firstLriFromParser2);
            secondLriFromParser2 = genCmdCast<MI_LOAD_REGISTER_IMM *>(*(++itor));
            ASSERT_NE(nullptr, secondLriFromParser2);
        } else {
            auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
            ASSERT_NE(cmdList.end(), itor);

            semaphoreFromParser2 = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);
            ASSERT_NE(nullptr, semaphoreFromParser2);
        }
    }

    auto verifyPatching = [&](uint64_t executionCounter) {
        auto appendValue = inOrderRegularCmdList->inOrderExecInfo->getCounterValue() * executionCounter;

        if (regularCmdList->isQwordInOrderCounter()) {
            EXPECT_EQ(getLowPart(1u + appendValue), firstLriFromContainer2->getDataDword());
            EXPECT_EQ(getLowPart(1u + appendValue), firstLriFromParser2->getDataDword());

            EXPECT_EQ(getHighPart(1u + appendValue), secondLriFromContainer2->getDataDword());
            EXPECT_EQ(getHighPart(1u + appendValue), secondLriFromParser2->getDataDword());
        } else {
            EXPECT_EQ(1u + appendValue, semaphoreFromContainer2->getSemaphoreDataDword());
            EXPECT_EQ(1u + appendValue, semaphoreFromParser2->getSemaphoreDataDword());
        }
    };

    regularCmdList->close();
    inOrderRegularCmdList->close();

    auto inOrderRegularCmdListHandle = inOrderRegularCmdList->toHandle();
    auto regularHandle = regularCmdList->toHandle();

    mockCmdQHw->executeCommandLists(1, &inOrderRegularCmdListHandle, nullptr, false, nullptr);
    mockCmdQHw->executeCommandLists(1, &regularHandle, nullptr, false, nullptr);
    verifyPatching(0);

    mockCmdQHw->executeCommandLists(1, &inOrderRegularCmdListHandle, nullptr, false, nullptr);
    mockCmdQHw->executeCommandLists(1, &regularHandle, nullptr, false, nullptr);
    verifyPatching(1);

    mockCmdQHw->executeCommandLists(1, &inOrderRegularCmdListHandle, nullptr, false, nullptr);
    mockCmdQHw->executeCommandLists(1, &regularHandle, nullptr, false, nullptr);
    verifyPatching(2);

    mockCmdQHw->executeCommandLists(1, &regularHandle, nullptr, false, nullptr);
    verifyPatching(2);
}

HWTEST2_F(InOrderRegularCmdListTests, givenAddedCmdForPatchWhenUpdateNewInOrderInfoThenNewInfoIsSet, IsAtLeastXeHpCore) {
    auto semaphoreCmd = FamilyType::cmdInitMiSemaphoreWait;

    auto inOrderRegularCmdList = createRegularCmdList<gfxCoreFamily>(false);
    auto &inOrderExecInfo = inOrderRegularCmdList->inOrderExecInfo;
    inOrderExecInfo->addRegularCmdListSubmissionCounter(4);
    inOrderExecInfo->addCounterValue(1);

    auto inOrderRegularCmdList2 = createRegularCmdList<gfxCoreFamily>(false);
    auto &inOrderExecInfo2 = inOrderRegularCmdList2->inOrderExecInfo;
    inOrderExecInfo2->addRegularCmdListSubmissionCounter(6);
    inOrderExecInfo2->addCounterValue(1);

    inOrderRegularCmdList->addCmdForPatching(&inOrderExecInfo, &semaphoreCmd, nullptr, 1, NEO::InOrderPatchCommandHelpers::PatchCmdType::semaphore);

    ASSERT_EQ(1u, inOrderRegularCmdList->inOrderPatchCmds.size());

    inOrderRegularCmdList->disablePatching(0);
    inOrderRegularCmdList->inOrderPatchCmds[0].patch(3);
    EXPECT_EQ(0u, semaphoreCmd.getSemaphoreDataDword());

    inOrderRegularCmdList->enablePatching(0);
    inOrderRegularCmdList->inOrderPatchCmds[0].patch(3);
    EXPECT_EQ(4u, semaphoreCmd.getSemaphoreDataDword());

    inOrderRegularCmdList->updateInOrderExecInfo(0, &inOrderExecInfo2, false);
    inOrderRegularCmdList->inOrderPatchCmds[0].patch(3);
    EXPECT_EQ(6u, semaphoreCmd.getSemaphoreDataDword());

    inOrderExecInfo->addRegularCmdListSubmissionCounter(1);
    inOrderRegularCmdList->updateInOrderExecInfo(0, &inOrderExecInfo, true);
    inOrderRegularCmdList->inOrderPatchCmds[0].patch(3);
    EXPECT_EQ(6u, semaphoreCmd.getSemaphoreDataDword());

    inOrderRegularCmdList->enablePatching(0);
    inOrderRegularCmdList->inOrderPatchCmds[0].patch(3);
    EXPECT_EQ(5u, semaphoreCmd.getSemaphoreDataDword());
}

struct StandaloneInOrderTimestampAllocationTests : public InOrderCmdListFixture {
    void SetUp() override {
        InOrderCmdListFixture::SetUp();
    }
};

HWTEST2_F(StandaloneInOrderTimestampAllocationTests, givenDebugFlagSetWhenCreatingEventThenDontCreateTimestampNode, MatchAny) {
    NEO::debugManager.flags.StandaloneInOrderTimestampAllocationEnabled.set(0);
    auto eventPool = createEvents<FamilyType>(1, true);
    auto cmdList = createImmCmdList<gfxCoreFamily>();
    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    EXPECT_EQ(nullptr, events[0]->inOrderTimestampNode);
    EXPECT_NE(nullptr, events[0]->eventPoolAllocation);
}

HWTEST2_F(StandaloneInOrderTimestampAllocationTests, givenSignalScopeEventWhenSignalEventIsCalledThenProgramPipeControl, MatchAny) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    auto eventPool = createEvents<FamilyType>(2, false);

    auto cmdList = createImmCmdList<gfxCoreFamily>();

    events[0]->signalScope = true;
    events[1]->signalScope = false;

    auto cmdStream = cmdList->getCmdContainer().getCommandStream();
    size_t offset = cmdStream->getUsed();

    {
        cmdList->appendSignalEvent(events[1]->toHandle(), false);

        GenCmdList hwCmdList;
        EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(hwCmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        auto itor = find<PIPE_CONTROL *>(hwCmdList.begin(), hwCmdList.end());
        EXPECT_EQ(hwCmdList.end(), itor);
    }

    offset = cmdStream->getUsed();

    {
        cmdList->appendSignalEvent(events[0]->toHandle(), false);

        GenCmdList hwCmdList;
        EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(hwCmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        auto itor = find<PIPE_CONTROL *>(hwCmdList.begin(), hwCmdList.end());

        if (cmdList->getDcFlushRequired(true)) {
            ASSERT_NE(hwCmdList.end(), itor);
            auto pipeControl = genCmdCast<PIPE_CONTROL *>(*itor);
            ASSERT_NE(nullptr, pipeControl);
            EXPECT_TRUE(pipeControl->getDcFlushEnable());
            EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_NO_WRITE, pipeControl->getPostSyncOperation());
        } else {
            EXPECT_EQ(hwCmdList.end(), itor);
        }
    }
}

HWTEST2_F(StandaloneInOrderTimestampAllocationTests, givenTimestampEventWhenAskingForAllocationOrGpuAddressThenReturnNodeAllocation, MatchAny) {
    auto eventPool = createEvents<FamilyType>(2, true);

    auto cmdList = createImmCmdList<gfxCoreFamily>();

    EXPECT_FALSE(events[0]->hasInOrderTimestampNode());
    EXPECT_FALSE(events[1]->hasInOrderTimestampNode());

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams, false);

    EXPECT_TRUE(events[0]->hasInOrderTimestampNode());
    EXPECT_TRUE(events[1]->hasInOrderTimestampNode());

    EXPECT_NE(events[0]->inOrderTimestampNode->getBaseGraphicsAllocation(), events[0]->eventPoolAllocation);
    EXPECT_NE(nullptr, events[0]->inOrderTimestampNode->getBaseGraphicsAllocation());
    EXPECT_EQ(nullptr, events[0]->eventPoolAllocation);

    EXPECT_EQ(events[0]->inOrderTimestampNode->getBaseGraphicsAllocation()->getGraphicsAllocation(0), events[0]->getAllocation(device));
    EXPECT_EQ(events[0]->inOrderTimestampNode->getBaseGraphicsAllocation()->getGraphicsAllocation(0)->getGpuAddress(), events[0]->getGpuAddress(device));
    EXPECT_EQ(events[0]->getGpuAddress(device) + events[0]->getCompletionFieldOffset(), events[0]->getCompletionFieldGpuAddress(device));

    EXPECT_EQ(events[0]->getGpuAddress(device), events[0]->inOrderTimestampNode->getGpuAddress());
    EXPECT_EQ(events[1]->getGpuAddress(device), events[1]->inOrderTimestampNode->getGpuAddress());
    EXPECT_NE(events[0]->getGpuAddress(device), events[1]->getGpuAddress(device));
}

HWTEST2_F(StandaloneInOrderTimestampAllocationTests, givenDebugFlagSetWhenAssigningTimestampNodeThenClear, MatchAny) {
    auto eventPool = createEvents<FamilyType>(2, true);

    auto cmdList = createImmCmdList<gfxCoreFamily>();

    auto tag0 = device->getInOrderTimestampAllocator()->getTag();
    auto tag1 = device->getInOrderTimestampAllocator()->getTag();

    auto tag0Data = const_cast<uint32_t *>(reinterpret_cast<const uint32_t *>(tag0->getContextEndAddress(0)));
    auto tag1Data = const_cast<uint32_t *>(reinterpret_cast<const uint32_t *>(tag1->getContextEndAddress(0)));

    *tag0Data = 123;
    *tag1Data = 456;

    tag1->returnTag();
    tag0->returnTag();

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    debugManager.flags.ClearStandaloneInOrderTimestampAllocation.set(0);
    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams, false);

    EXPECT_EQ(456u, *tag1Data);
    EXPECT_EQ(static_cast<uint32_t>(Event::STATE_INITIAL), *tag0Data);
}

HWTEST2_F(StandaloneInOrderTimestampAllocationTests, givenNonWalkerCounterSignalingWhenPassedNonProfilingEventThenNotAssignAllocation, IsAtLeastXeHpCore) {
    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    auto cmdList = createImmCmdList<gfxCoreFamily>();

    EXPECT_EQ(nullptr, events[0]->inOrderTimestampNode);

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);

    bool isCompactEvent = cmdList->compactL3FlushEvent(cmdList->getDcFlushRequired(events[0]->isSignalScope()));

    if (cmdList->getDcFlushRequired(events[0]->isSignalScope())) {
        EXPECT_EQ(isCompactEvent, events[0]->getAllocation(device) == nullptr);
    } else {
        EXPECT_EQ(isCompactEvent, events[0]->getAllocation(device) != nullptr);
    }
    EXPECT_EQ(isCompactEvent, cmdList->isInOrderNonWalkerSignalingRequired(events[0].get()));
}

HWTEST2_F(StandaloneInOrderTimestampAllocationTests, givenTempNodeWhenCallingSyncPointsThenReleaseNotUsedNodes, MatchAny) {
    auto eventPool = createEvents<FamilyType>(1, true);
    auto eventHandle = events[0]->toHandle();

    auto cmdList = createImmCmdList<gfxCoreFamily>();

    auto inOrderExecInfo = static_cast<WhiteboxInOrderExecInfo *>(cmdList->inOrderExecInfo.get());
    auto hostAddress = inOrderExecInfo->getBaseHostAddress();
    *hostAddress = 3;

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);

    EXPECT_EQ(1u, inOrderExecInfo->tempTimestampNodes.size());

    EXPECT_EQ(ZE_RESULT_SUCCESS, cmdList->hostSynchronize(1, false));
    EXPECT_EQ(1u, inOrderExecInfo->tempTimestampNodes.size());

    EXPECT_EQ(ZE_RESULT_SUCCESS, cmdList->hostSynchronize(1, true));
    EXPECT_EQ(0u, inOrderExecInfo->tempTimestampNodes.size());

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    EXPECT_EQ(1u, inOrderExecInfo->tempTimestampNodes.size());

    EXPECT_EQ(ZE_RESULT_SUCCESS, cmdList->hostSynchronize(1, false));
    EXPECT_EQ(1u, inOrderExecInfo->tempTimestampNodes.size());

    events[0].reset();
    EXPECT_EQ(0u, inOrderExecInfo->tempTimestampNodes.size());
}

HWTEST2_F(StandaloneInOrderTimestampAllocationTests, givenTimestampEventWhenDispatchingThenAssignNewNode, MatchAny) {
    auto eventPool = createEvents<FamilyType>(1, true);
    auto eventHandle = events[0]->toHandle();

    auto cmdList = createImmCmdList<gfxCoreFamily>();

    EXPECT_EQ(nullptr, events[0]->inOrderTimestampNode);

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);

    EXPECT_NE(nullptr, events[0]->inOrderTimestampNode);

    // keep node0 ownership for testing
    auto node0 = events[0]->inOrderTimestampNode;
    events[0]->inOrderTimestampNode = nullptr;

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    EXPECT_NE(nullptr, events[0]->inOrderTimestampNode);
    EXPECT_NE(node0, events[0]->inOrderTimestampNode);

    auto node1 = events[0]->inOrderTimestampNode;

    // node1 moved to reusable list
    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    EXPECT_NE(nullptr, events[0]->inOrderTimestampNode);
    EXPECT_NE(node1->getGpuAddress(), events[0]->inOrderTimestampNode->getGpuAddress());

    auto node2 = events[0]->inOrderTimestampNode;

    auto hostAddress = cmdList->inOrderExecInfo->getBaseHostAddress();
    *hostAddress = 3;

    // return node1 to pool
    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(1));

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);
    // node1 reused
    EXPECT_EQ(node1->getGpuAddress(), events[0]->inOrderTimestampNode->getGpuAddress());

    // reuse node2 - counter already waited
    *hostAddress = 2;

    cmdList->inOrderExecInfo->releaseNotUsedTempTimestampNodes(false);
    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);

    EXPECT_EQ(node2->getGpuAddress(), events[0]->inOrderTimestampNode->getGpuAddress());

    events[0]->unsetInOrderExecInfo();
    EXPECT_EQ(nullptr, events[0]->inOrderTimestampNode);

    cmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);

    // mark as not ready, to make sure that destructor will release everything anyway
    *hostAddress = 0;
}

using SynchronizedDispatchTests = InOrderCmdListFixture;

using MultiTileSynchronizedDispatchTests = MultiTileSynchronizedDispatchFixture;

HWTEST2_F(MultiTileSynchronizedDispatchTests, givenSyncDispatchExtensionWhenCreatingRegularCmdListThenEnableSyncDispatchMode, MatchAny) {
    NEO::debugManager.flags.ForceSynchronizedDispatchMode.set(-1);

    ze_base_desc_t unknownDesc = {ZE_STRUCTURE_TYPE_FORCE_UINT32};

    ze_synchronized_dispatch_exp_desc_t syncDispatchDesc = {};
    syncDispatchDesc.stype = ZE_STRUCTURE_TYPE_SYNCHRONIZED_DISPATCH_EXP_DESC; // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), NEO-12901

    ze_command_list_desc_t cmdListDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    zex_command_list_handle_t hCmdList;

    // pNext == nullptr
    cmdListDesc.flags = ZE_COMMAND_LIST_FLAG_IN_ORDER;
    auto result = zeCommandListCreate(context->toHandle(), device->toHandle(), &cmdListDesc, &hCmdList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(NEO::SynchronizedDispatchMode::disabled, static_cast<CommandListImp *>(CommandList::fromHandle(hCmdList))->getSynchronizedDispatchMode());
    zeCommandListDestroy(hCmdList);

    // pNext == unknown type
    cmdListDesc.pNext = &unknownDesc;
    cmdListDesc.flags = ZE_COMMAND_LIST_FLAG_IN_ORDER;

    result = zeCommandListCreate(context->toHandle(), device->toHandle(), &cmdListDesc, &hCmdList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(NEO::SynchronizedDispatchMode::disabled, static_cast<CommandListImp *>(CommandList::fromHandle(hCmdList))->getSynchronizedDispatchMode());
    zeCommandListDestroy(hCmdList);

    // limited dispatch mode
    syncDispatchDesc.flags = ZE_SYNCHRONIZED_DISPATCH_DISABLED_EXP_FLAG;
    cmdListDesc.pNext = &syncDispatchDesc;
    cmdListDesc.flags = ZE_COMMAND_LIST_FLAG_IN_ORDER;
    result = zeCommandListCreate(context->toHandle(), device->toHandle(), &cmdListDesc, &hCmdList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(NEO::SynchronizedDispatchMode::limited, static_cast<CommandListImp *>(CommandList::fromHandle(hCmdList))->getSynchronizedDispatchMode());
    zeCommandListDestroy(hCmdList);

    // full dispatch mode
    syncDispatchDesc.flags = ZE_SYNCHRONIZED_DISPATCH_ENABLED_EXP_FLAG;
    cmdListDesc.pNext = &syncDispatchDesc;
    cmdListDesc.flags = ZE_COMMAND_LIST_FLAG_IN_ORDER;
    result = zeCommandListCreate(context->toHandle(), device->toHandle(), &cmdListDesc, &hCmdList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(NEO::SynchronizedDispatchMode::full, static_cast<CommandListImp *>(CommandList::fromHandle(hCmdList))->getSynchronizedDispatchMode());
    zeCommandListDestroy(hCmdList);

    // No ZE_COMMAND_LIST_FLAG_IN_ORDER flag
    syncDispatchDesc.flags = ZE_SYNCHRONIZED_DISPATCH_DISABLED_EXP_FLAG;
    cmdListDesc.pNext = &syncDispatchDesc;
    cmdListDesc.flags = 0;
    result = zeCommandListCreate(context->toHandle(), device->toHandle(), &cmdListDesc, &hCmdList);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    EXPECT_EQ(nullptr, hCmdList);

    // No ZE_COMMAND_LIST_FLAG_IN_ORDER flag
    syncDispatchDesc.flags = ZE_SYNCHRONIZED_DISPATCH_ENABLED_EXP_FLAG;
    cmdListDesc.pNext = &syncDispatchDesc;
    cmdListDesc.flags = 0;
    result = zeCommandListCreate(context->toHandle(), device->toHandle(), &cmdListDesc, &hCmdList);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    EXPECT_EQ(nullptr, hCmdList);
}

HWTEST2_F(MultiTileSynchronizedDispatchTests, givenSyncDispatchExtensionWhenCreatingImmediateCmdListThenEnableSyncDispatchMode, MatchAny) {
    NEO::debugManager.flags.ForceSynchronizedDispatchMode.set(-1);

    ze_base_desc_t unknownDesc = {ZE_STRUCTURE_TYPE_FORCE_UINT32};

    ze_synchronized_dispatch_exp_desc_t syncDispatchDesc = {};
    syncDispatchDesc.stype = ZE_STRUCTURE_TYPE_SYNCHRONIZED_DISPATCH_EXP_DESC; // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), NEO-12901

    ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    zex_command_list_handle_t hCmdList;

    // pNext == nullptr
    queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    auto result = zeCommandListCreateImmediate(context->toHandle(), device->toHandle(), &queueDesc, &hCmdList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(NEO::SynchronizedDispatchMode::disabled, static_cast<CommandListImp *>(CommandList::fromHandle(hCmdList))->getSynchronizedDispatchMode());
    zeCommandListDestroy(hCmdList);

    // pNext == unknown type
    queueDesc.pNext = &unknownDesc;
    queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    result = zeCommandListCreateImmediate(context->toHandle(), device->toHandle(), &queueDesc, &hCmdList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(NEO::SynchronizedDispatchMode::disabled, static_cast<CommandListImp *>(CommandList::fromHandle(hCmdList))->getSynchronizedDispatchMode());
    zeCommandListDestroy(hCmdList);

    // limited dispatch mode
    syncDispatchDesc.flags = ZE_SYNCHRONIZED_DISPATCH_DISABLED_EXP_FLAG;
    queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    queueDesc.pNext = &syncDispatchDesc;
    result = zeCommandListCreateImmediate(context->toHandle(), device->toHandle(), &queueDesc, &hCmdList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(NEO::SynchronizedDispatchMode::limited, static_cast<CommandListImp *>(CommandList::fromHandle(hCmdList))->getSynchronizedDispatchMode());
    zeCommandListDestroy(hCmdList);

    // full dispatch mode
    syncDispatchDesc.flags = ZE_SYNCHRONIZED_DISPATCH_ENABLED_EXP_FLAG;
    queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    queueDesc.pNext = &syncDispatchDesc;
    result = zeCommandListCreateImmediate(context->toHandle(), device->toHandle(), &queueDesc, &hCmdList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(NEO::SynchronizedDispatchMode::full, static_cast<CommandListImp *>(CommandList::fromHandle(hCmdList))->getSynchronizedDispatchMode());
    zeCommandListDestroy(hCmdList);

    // No ZE_SYNCHRONIZED_DISPATCH_ENABLED_EXP_FLAG flag
    syncDispatchDesc.flags = ZE_SYNCHRONIZED_DISPATCH_DISABLED_EXP_FLAG;
    queueDesc.flags = 0;
    queueDesc.pNext = &syncDispatchDesc;
    result = zeCommandListCreateImmediate(context->toHandle(), device->toHandle(), &queueDesc, &hCmdList);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    EXPECT_EQ(nullptr, hCmdList);

    // No ZE_SYNCHRONIZED_DISPATCH_ENABLED_EXP_FLAG flag
    syncDispatchDesc.flags = ZE_SYNCHRONIZED_DISPATCH_ENABLED_EXP_FLAG;
    queueDesc.flags = 0;
    queueDesc.pNext = &syncDispatchDesc;
    result = zeCommandListCreateImmediate(context->toHandle(), device->toHandle(), &queueDesc, &hCmdList);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    EXPECT_EQ(nullptr, hCmdList);
}

HWTEST2_F(SynchronizedDispatchTests, givenSingleTileSyncDispatchQueueWhenCreatingThenDontAssignQueueId, MatchAny) {
    NEO::debugManager.flags.ForceSynchronizedDispatchMode.set(1);

    auto regularCmdList0 = createRegularCmdList<gfxCoreFamily>(false);
    auto regularCmdList1 = createRegularCmdList<gfxCoreFamily>(false);
    auto immCmdList0 = createImmCmdList<gfxCoreFamily>();
    auto immCmdList1 = createImmCmdList<gfxCoreFamily>();

    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), regularCmdList0->syncDispatchQueueId);
    EXPECT_EQ(NEO::SynchronizedDispatchMode::disabled, regularCmdList0->synchronizedDispatchMode);

    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), regularCmdList1->syncDispatchQueueId);
    EXPECT_EQ(NEO::SynchronizedDispatchMode::disabled, regularCmdList1->synchronizedDispatchMode);

    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), immCmdList0->syncDispatchQueueId);
    EXPECT_EQ(NEO::SynchronizedDispatchMode::disabled, immCmdList0->synchronizedDispatchMode);

    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), immCmdList1->syncDispatchQueueId);
    EXPECT_EQ(NEO::SynchronizedDispatchMode::disabled, immCmdList1->synchronizedDispatchMode);
}

HWTEST2_F(MultiTileSynchronizedDispatchTests, givenDebugFlagSetWhenCreatingCmdListThenEnableSynchronizedDispatch, MatchAny) {
    NEO::debugManager.flags.ForceSynchronizedDispatchMode.set(-1);

    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();
    auto regularCmdList = createMultiTileRegularCmdList<gfxCoreFamily>(false);

    EXPECT_EQ(NEO::SynchronizedDispatchMode::disabled, immCmdList->synchronizedDispatchMode);
    EXPECT_EQ(NEO::SynchronizedDispatchMode::disabled, regularCmdList->synchronizedDispatchMode);

    NEO::debugManager.flags.ForceSynchronizedDispatchMode.set(0);

    immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();
    regularCmdList = createMultiTileRegularCmdList<gfxCoreFamily>(false);

    EXPECT_EQ(NEO::SynchronizedDispatchMode::disabled, immCmdList->synchronizedDispatchMode);
    EXPECT_EQ(NEO::SynchronizedDispatchMode::disabled, regularCmdList->synchronizedDispatchMode);

    NEO::debugManager.flags.ForceSynchronizedDispatchMode.set(1);

    immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();
    regularCmdList = createMultiTileRegularCmdList<gfxCoreFamily>(false);

    EXPECT_EQ(NEO::SynchronizedDispatchMode::full, immCmdList->synchronizedDispatchMode);
    EXPECT_EQ(NEO::SynchronizedDispatchMode::full, regularCmdList->synchronizedDispatchMode);
}

HWTEST2_F(MultiTileSynchronizedDispatchTests, givenMultiTileSyncDispatchQueueWhenCreatingThenAssignQueueId, MatchAny) {
    auto regularCmdList0 = createMultiTileRegularCmdList<gfxCoreFamily>(false);
    auto regularCmdList1 = createMultiTileRegularCmdList<gfxCoreFamily>(false);
    auto immCmdList0 = createMultiTileImmCmdList<gfxCoreFamily>();
    auto immCmdList1 = createMultiTileImmCmdList<gfxCoreFamily>();

    auto limitedRegularCmdList = createMultiTileRegularCmdList<gfxCoreFamily>(false);
    limitedRegularCmdList->synchronizedDispatchMode = NEO::SynchronizedDispatchMode::limited;

    auto limitedImmCmdList = createMultiTileImmCmdList<gfxCoreFamily>();
    limitedImmCmdList->synchronizedDispatchMode = NEO::SynchronizedDispatchMode::limited;

    EXPECT_EQ(0u, regularCmdList0->syncDispatchQueueId);
    EXPECT_EQ(NEO::SynchronizedDispatchMode::full, regularCmdList0->synchronizedDispatchMode);

    EXPECT_EQ(1u, regularCmdList1->syncDispatchQueueId);
    EXPECT_EQ(NEO::SynchronizedDispatchMode::full, regularCmdList1->synchronizedDispatchMode);

    EXPECT_EQ(2u, immCmdList0->syncDispatchQueueId);
    EXPECT_EQ(NEO::SynchronizedDispatchMode::full, immCmdList0->synchronizedDispatchMode);

    EXPECT_EQ(3u, immCmdList1->syncDispatchQueueId);
    EXPECT_EQ(NEO::SynchronizedDispatchMode::full, immCmdList1->synchronizedDispatchMode);
}

HWTEST2_F(MultiTileSynchronizedDispatchTests, givenMultiTileSyncDispatchQueueWhenCreatingThenDontAssignQueueIdForLimitedMode, MatchAny) {
    NEO::debugManager.flags.ForceSynchronizedDispatchMode.set(0);

    auto mockDevice = static_cast<MockDeviceImp *>(device);

    constexpr uint32_t limitedQueueId = std::numeric_limits<uint32_t>::max();

    EXPECT_EQ(nullptr, mockDevice->syncDispatchTokenAllocation);

    auto limitedRegularCmdList = createMultiTileRegularCmdList<gfxCoreFamily>(false);
    limitedRegularCmdList->enableSynchronizedDispatch(NEO::SynchronizedDispatchMode::limited);

    EXPECT_NE(nullptr, mockDevice->syncDispatchTokenAllocation);

    auto limitedImmCmdList = createMultiTileImmCmdList<gfxCoreFamily>();
    limitedImmCmdList->enableSynchronizedDispatch(NEO::SynchronizedDispatchMode::limited);

    EXPECT_EQ(limitedQueueId, limitedRegularCmdList->syncDispatchQueueId);
    EXPECT_EQ(NEO::SynchronizedDispatchMode::limited, limitedRegularCmdList->synchronizedDispatchMode);

    EXPECT_EQ(limitedQueueId, limitedImmCmdList->syncDispatchQueueId);
    EXPECT_EQ(NEO::SynchronizedDispatchMode::limited, limitedImmCmdList->synchronizedDispatchMode);

    auto regularCmdList = createMultiTileRegularCmdList<gfxCoreFamily>(false);
    regularCmdList->enableSynchronizedDispatch(NEO::SynchronizedDispatchMode::full);
    EXPECT_EQ(0u, regularCmdList->syncDispatchQueueId);

    regularCmdList = createMultiTileRegularCmdList<gfxCoreFamily>(false);
    regularCmdList->enableSynchronizedDispatch(NEO::SynchronizedDispatchMode::full);
    EXPECT_EQ(1u, regularCmdList->syncDispatchQueueId);

    limitedImmCmdList = createMultiTileImmCmdList<gfxCoreFamily>();
    limitedImmCmdList->enableSynchronizedDispatch(NEO::SynchronizedDispatchMode::limited);

    EXPECT_EQ(limitedQueueId, limitedRegularCmdList->syncDispatchQueueId);

    regularCmdList = createMultiTileRegularCmdList<gfxCoreFamily>(false);
    regularCmdList->enableSynchronizedDispatch(NEO::SynchronizedDispatchMode::full);
    EXPECT_EQ(2u, regularCmdList->syncDispatchQueueId);
}

HWTEST2_F(MultiTileSynchronizedDispatchTests, givenSyncDispatchEnabledWhenAllocatingQueueIdThenEnsureTokenAllocation, MatchAny) {
    auto mockDevice = static_cast<MockDeviceImp *>(device);

    EXPECT_EQ(nullptr, mockDevice->syncDispatchTokenAllocation);

    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();
    EXPECT_EQ(NEO::SynchronizedDispatchMode::full, immCmdList->synchronizedDispatchMode);

    auto syncAllocation = mockDevice->syncDispatchTokenAllocation;
    EXPECT_NE(nullptr, syncAllocation);

    EXPECT_EQ(syncAllocation->getAllocationType(), NEO::AllocationType::syncDispatchToken);

    immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();
    EXPECT_EQ(NEO::SynchronizedDispatchMode::full, immCmdList->synchronizedDispatchMode);

    EXPECT_EQ(mockDevice->syncDispatchTokenAllocation, syncAllocation);
}

HWTEST2_F(MultiTileSynchronizedDispatchTests, givenSyncDispatchWhenAppendingThenHandleResidency, MatchAny) {
    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    ultCsr->storeMakeResidentAllocations = true;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(1u, ultCsr->makeResidentAllocations[device->getSyncDispatchTokenAllocation()]);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(2u, ultCsr->makeResidentAllocations[device->getSyncDispatchTokenAllocation()]);
}

HWTEST2_F(MultiTileSynchronizedDispatchTests, givenDefaultCmdListWhenCooperativeDispatchEnableThenEnableSyncDispatchMode, MatchAny) {
    debugManager.flags.ForceSynchronizedDispatchMode.set(-1);
    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(immCmdList->synchronizedDispatchMode, NEO::SynchronizedDispatchMode::disabled);

    CmdListKernelLaunchParams cooperativeParams = {};
    cooperativeParams.isCooperative = true;

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, cooperativeParams, false);

    if (device->getL0GfxCoreHelper().implicitSynchronizedDispatchForCooperativeKernelsAllowed()) {
        EXPECT_EQ(immCmdList->synchronizedDispatchMode, NEO::SynchronizedDispatchMode::full);
    } else {
        EXPECT_EQ(immCmdList->synchronizedDispatchMode, NEO::SynchronizedDispatchMode::disabled);
    }

    immCmdList->synchronizedDispatchMode = NEO::SynchronizedDispatchMode::limited;
    device->ensureSyncDispatchTokenAllocation();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, cooperativeParams, false);
    EXPECT_EQ(immCmdList->synchronizedDispatchMode, NEO::SynchronizedDispatchMode::limited);
}

HWTEST2_F(MultiTileSynchronizedDispatchTests, givenLimitedSyncDispatchWhenAppendingThenProgramTokenCheck, MatchAny) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using COMPARE_OPERATION = typename MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    using BaseClass = WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>;
    class MyCmdList : public BaseClass {
      public:
        void appendSynchronizedDispatchInitializationSection() override {
            initCalled++;
            BaseClass::appendSynchronizedDispatchInitializationSection();
        }

        void appendSynchronizedDispatchCleanupSection() override {
            cleanupCalled++;
            BaseClass::appendSynchronizedDispatchCleanupSection();
        }

        uint32_t initCalled = 0;
        uint32_t cleanupCalled = 0;
    };

    void *alloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &alloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    auto immCmdList = createImmCmdListImpl<gfxCoreFamily, MyCmdList>(false);
    immCmdList->partitionCount = partitionCount;
    immCmdList->synchronizedDispatchMode = NEO::SynchronizedDispatchMode::limited;

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    size_t offset = cmdStream->getUsed();

    uint32_t expectedInitCalls = 1;
    uint32_t expectedCleanupCalls = 1;

    auto verifyTokenCheck = [&](uint32_t numDependencies) {
        GenCmdList cmdList;
        EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));
        if (::testing::Test::HasFailure()) {
            return false;
        }

        auto semaphore = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
        EXPECT_NE(cmdList.end(), semaphore);
        if (::testing::Test::HasFailure()) {
            return false;
        }

        for (uint32_t i = 0; i < numDependencies; i++) {
            for (uint32_t j = 1; j < partitionCount; j++) {
                semaphore++;
                semaphore = find<MI_SEMAPHORE_WAIT *>(semaphore, cmdList.end());
                EXPECT_NE(cmdList.end(), semaphore);
            }
            semaphore++;
        }

        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphore);
        EXPECT_NE(nullptr, semaphoreCmd);
        if (::testing::Test::HasFailure()) {
            return false;
        }

        EXPECT_EQ(0u, semaphoreCmd->getSemaphoreDataDword());
        EXPECT_EQ(device->getSyncDispatchTokenAllocation()->getGpuAddress() + sizeof(uint32_t), semaphoreCmd->getSemaphoreGraphicsAddress());
        EXPECT_EQ(COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, semaphoreCmd->getCompareOperation());

        EXPECT_EQ(expectedInitCalls++, immCmdList->initCalled);
        EXPECT_EQ(expectedCleanupCalls++, immCmdList->cleanupCalled);

        return !::testing::Test::HasFailure();
    };

    // first run without dependency
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_TRUE(verifyTokenCheck(0));

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_TRUE(verifyTokenCheck(1));

    CmdListKernelLaunchParams cooperativeParams = {};
    cooperativeParams.isCooperative = true;

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, cooperativeParams, false);
    EXPECT_TRUE(verifyTokenCheck(1));

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernelIndirect(kernel->toHandle(), *static_cast<ze_group_count_t *>(alloc), nullptr, 0, nullptr, false);
    EXPECT_TRUE(verifyTokenCheck(1));

    offset = cmdStream->getUsed();
    const ze_kernel_handle_t launchKernels = kernel->toHandle();
    immCmdList->appendLaunchMultipleKernelsIndirect(1, &launchKernels, reinterpret_cast<const uint32_t *>(alloc), &groupCount, nullptr, 0, nullptr, false);
    EXPECT_TRUE(verifyTokenCheck(1));

    offset = cmdStream->getUsed();
    immCmdList->appendEventReset(events[0]->toHandle());
    EXPECT_TRUE(verifyTokenCheck(1));

    offset = cmdStream->getUsed();
    size_t rangeSizes = 1;
    const void **ranges = const_cast<const void **>(&alloc);
    immCmdList->appendMemoryRangesBarrier(1, &rangeSizes, ranges, nullptr, 0, nullptr);
    EXPECT_TRUE(verifyTokenCheck(1));

    offset = cmdStream->getUsed();
    CmdListMemoryCopyParams copyParams = {};

    immCmdList->appendMemoryCopy(alloc, alloc, 1, nullptr, 0, nullptr, copyParams);
    EXPECT_TRUE(verifyTokenCheck(1));

    offset = cmdStream->getUsed();
    ze_copy_region_t region = {0, 0, 0, 1, 1, 1};
    immCmdList->appendMemoryCopyRegion(alloc, &region, 1, 1, alloc, &region, 1, 1, nullptr, 0, nullptr, copyParams);
    EXPECT_TRUE(verifyTokenCheck(1));

    offset = cmdStream->getUsed();
    immCmdList->appendMemoryFill(alloc, alloc, 2, 2, nullptr, 0, nullptr, false);
    EXPECT_TRUE(verifyTokenCheck(1));

    offset = cmdStream->getUsed();
    immCmdList->appendWriteGlobalTimestamp(reinterpret_cast<uint64_t *>(alloc), nullptr, 0, nullptr);
    EXPECT_TRUE(verifyTokenCheck(1));

    offset = cmdStream->getUsed();
    auto handle = events[0]->toHandle();
    events[0]->unsetCmdQueue();
    immCmdList->appendBarrier(nullptr, 1, &handle, false);
    EXPECT_TRUE(verifyTokenCheck(2));

    context->freeMem(alloc);
}

HWTEST2_F(MultiTileSynchronizedDispatchTests, givenFullSyncDispatchWhenAppendingThenProgramTokenAcquire, IsAtLeastXeHpcCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_SET_PREDICATE = typename FamilyType::MI_SET_PREDICATE;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();
    immCmdList->synchronizedDispatchMode = NEO::SynchronizedDispatchMode::full;
    immCmdList->syncDispatchQueueId = 0x1234;

    const uint32_t queueId = immCmdList->syncDispatchQueueId + 1;
    const uint64_t queueIdToken = static_cast<uint64_t>(queueId) << 32;
    const uint64_t tokenInitialValue = queueIdToken + partitionCount;

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    size_t offset = cmdStream->getUsed();

    uint64_t syncAllocGpuVa = device->getSyncDispatchTokenAllocation()->getGpuAddress();

    auto verifyTokenAcquisition = [&](bool hasDependencySemaphore) {
        GenCmdList cmdList;
        EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));
        if (::testing::Test::HasFailure()) {
            return false;
        }

        auto itor = cmdList.begin();
        if (hasDependencySemaphore) {
            auto nPartition = std::min(immCmdList->inOrderExecInfo->getNumDevicePartitionsToWait(), partitionCount);
            for (uint32_t i = 0; i < nPartition; i++) {
                itor = find<MI_SEMAPHORE_WAIT *>(itor, cmdList.end());
                EXPECT_NE(cmdList.end(), itor);
                itor++;
            }
        }

        // Primary-secondaty path selection
        void *primaryTileSectionSkipVa = *itor;

        // Primary Tile section
        auto miPredicate = reinterpret_cast<MI_SET_PREDICATE *>(
            ptrOffset(primaryTileSectionSkipVa, NEO::EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataMemBatchBufferStart(false)));
        void *loopBackToAcquireVa = miPredicate;

        if (!RelaxedOrderingCommandsHelper::verifyMiPredicate<FamilyType>(miPredicate, MiPredicateType::disable)) {
            return false;
        }

        auto itAtomic = find<MI_ATOMIC *>(itor, cmdList.end());

        auto miAtomic = reinterpret_cast<MI_ATOMIC *>(*itAtomic);
        EXPECT_EQ(MI_ATOMIC::DWORD_LENGTH::DWORD_LENGTH_INLINE_DATA_1, miAtomic->getDwordLength());
        EXPECT_EQ(1u, miAtomic->getInlineData());

        EXPECT_EQ(0u, miAtomic->getOperand1DataDword0());
        EXPECT_EQ(0u, miAtomic->getOperand1DataDword1());

        EXPECT_EQ(getLowPart(tokenInitialValue), miAtomic->getOperand2DataDword0());
        EXPECT_EQ(getHighPart(tokenInitialValue), miAtomic->getOperand2DataDword1());

        EXPECT_EQ(MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_8B_CMP_WR, miAtomic->getAtomicOpcode());
        EXPECT_EQ(MI_ATOMIC::DATA_SIZE::DATA_SIZE_QWORD, miAtomic->getDataSize());

        EXPECT_EQ(syncAllocGpuVa, NEO::UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic));

        if (::testing::Test::HasFailure()) {
            return false;
        }

        void *jumpToEndSectionFromPrimaryTile = ++miAtomic;

        auto semaphore = reinterpret_cast<MI_SEMAPHORE_WAIT *>(
            ptrOffset(jumpToEndSectionFromPrimaryTile, NEO::EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataMemBatchBufferStart(false)));

        EXPECT_EQ(0u, semaphore->getSemaphoreDataDword());
        EXPECT_EQ(syncAllocGpuVa + sizeof(uint32_t), semaphore->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, semaphore->getCompareOperation());

        if (::testing::Test::HasFailure()) {
            return false;
        }

        auto bbStart = reinterpret_cast<MI_BATCH_BUFFER_START *>(++semaphore);
        EXPECT_EQ(castToUint64(loopBackToAcquireVa), bbStart->getBatchBufferStartAddress());

        if (::testing::Test::HasFailure()) {
            return false;
        }

        uint64_t workPartitionGpuVa = device->getNEODevice()->getDefaultEngine().commandStreamReceiver->getWorkPartitionAllocation()->getGpuAddress();

        // Secondary Tile section
        miPredicate = reinterpret_cast<MI_SET_PREDICATE *>(++bbStart);
        if (!RelaxedOrderingCommandsHelper::verifyMiPredicate<FamilyType>(miPredicate, MiPredicateType::disable)) {
            return false;
        }

        // Primary Tile section skip - patching
        if (!RelaxedOrderingCommandsHelper::verifyConditionalDataMemBbStart<FamilyType>(primaryTileSectionSkipVa, castToUint64(miPredicate), workPartitionGpuVa, 0, NEO::CompareOperation::notEqual, false, false, false)) {
            return false;
        }

        semaphore = reinterpret_cast<MI_SEMAPHORE_WAIT *>(++miPredicate);
        EXPECT_EQ(queueId, semaphore->getSemaphoreDataDword());
        EXPECT_EQ(syncAllocGpuVa + sizeof(uint32_t), semaphore->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, semaphore->getCompareOperation());

        // End section
        miPredicate = reinterpret_cast<MI_SET_PREDICATE *>(++semaphore);
        if (!RelaxedOrderingCommandsHelper::verifyMiPredicate<FamilyType>(miPredicate, MiPredicateType::disable)) {
            return false;
        }

        // Jump to end from Primary Tile section - patching
        if (!RelaxedOrderingCommandsHelper::verifyConditionalDataMemBbStart<FamilyType>(jumpToEndSectionFromPrimaryTile, castToUint64(miPredicate), syncAllocGpuVa + sizeof(uint32_t), queueId, NEO::CompareOperation::equal, false, false, false)) {
            return false;
        }

        return true;
    };

    // first run without dependency
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_TRUE(verifyTokenAcquisition(false));

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_TRUE(verifyTokenAcquisition(true));
}

HWTEST2_F(MultiTileSynchronizedDispatchTests, givenFullSyncDispatchWhenAppendingThenProgramTokenCleanup, MatchAny) {
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;

    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();
    immCmdList->synchronizedDispatchMode = NEO::SynchronizedDispatchMode::full;
    immCmdList->syncDispatchQueueId = 0x1234;

    const uint32_t queueId = immCmdList->syncDispatchQueueId + 1;
    const uint64_t queueIdToken = static_cast<uint64_t>(queueId) << 32;

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    size_t offset = cmdStream->getUsed();

    uint64_t syncAllocGpuVa = device->getSyncDispatchTokenAllocation()->getGpuAddress();

    auto verifyTokenCleanup = [&]() {
        GenCmdList cmdList;
        EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));
        if (::testing::Test::HasFailure()) {
            return false;
        }

        auto itor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());

        EXPECT_NE(cmdList.end(), itor);
        if (::testing::Test::HasFailure()) {
            return false;
        }

        MI_ATOMIC *miAtomic = nullptr;
        bool atomicFound = false;

        while (itor != cmdList.end()) {
            itor = find<MI_ATOMIC *>(itor, cmdList.end());
            EXPECT_NE(cmdList.end(), itor);
            if (::testing::Test::HasFailure()) {
                return false;
            }

            miAtomic = genCmdCast<MI_ATOMIC *>(*itor);

            if (syncAllocGpuVa == NEO::UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic)) {
                atomicFound = true;
                break;
            }
            itor++;
        }

        EXPECT_TRUE(atomicFound);
        if (::testing::Test::HasFailure()) {
            return false;
        }

        EXPECT_EQ(MI_ATOMIC::DWORD_LENGTH::DWORD_LENGTH_INLINE_DATA_0, miAtomic->getDwordLength());
        EXPECT_EQ(0u, miAtomic->getInlineData());

        EXPECT_EQ(0u, miAtomic->getOperand1DataDword0());
        EXPECT_EQ(0u, miAtomic->getOperand1DataDword1());
        EXPECT_EQ(0u, miAtomic->getOperand2DataDword0());
        EXPECT_EQ(0u, miAtomic->getOperand2DataDword1());

        EXPECT_EQ(MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_8B_DECREMENT, miAtomic->getAtomicOpcode());
        EXPECT_EQ(MI_ATOMIC::DATA_SIZE::DATA_SIZE_QWORD, miAtomic->getDataSize());

        if (::testing::Test::HasFailure()) {
            return false;
        }

        miAtomic++;

        EXPECT_EQ(MI_ATOMIC::DWORD_LENGTH::DWORD_LENGTH_INLINE_DATA_1, miAtomic->getDwordLength());
        EXPECT_EQ(1u, miAtomic->getInlineData());

        EXPECT_EQ(getLowPart(queueIdToken), miAtomic->getOperand1DataDword0());
        EXPECT_EQ(getHighPart(queueIdToken), miAtomic->getOperand1DataDword1());
        EXPECT_EQ(0u, miAtomic->getOperand2DataDword0());
        EXPECT_EQ(0u, miAtomic->getOperand2DataDword1());

        EXPECT_EQ(MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_8B_CMP_WR, miAtomic->getAtomicOpcode());
        EXPECT_EQ(MI_ATOMIC::DATA_SIZE::DATA_SIZE_QWORD, miAtomic->getDataSize());

        EXPECT_EQ(syncAllocGpuVa, NEO::UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic));

        return !::testing::Test::HasFailure();
    };

    // first run without dependency
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_TRUE(verifyTokenCleanup());

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_TRUE(verifyTokenCleanup());
}

HWTEST2_F(MultiTileSynchronizedDispatchTests, givenLimitedSyncDispatchWhenAppendingThenDontProgramTokenCleanup, MatchAny) {
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;

    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();
    immCmdList->synchronizedDispatchMode = NEO::SynchronizedDispatchMode::limited;

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    size_t offset = cmdStream->getUsed();

    uint64_t syncAllocGpuVa = device->getSyncDispatchTokenAllocation()->getGpuAddress();

    auto verifyTokenCleanup = [&]() {
        GenCmdList cmdList;
        EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));
        if (::testing::Test::HasFailure()) {
            return false;
        }

        auto itor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());

        EXPECT_NE(cmdList.end(), itor);
        if (::testing::Test::HasFailure()) {
            return false;
        }

        auto atomics = findAll<MI_ATOMIC *>(itor, cmdList.end());
        for (auto &atomic : atomics) {
            auto miAtomic = genCmdCast<MI_ATOMIC *>(*atomic);
            EXPECT_NE(nullptr, miAtomic);
            EXPECT_NE(syncAllocGpuVa, NEO::UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic));
        }

        return !::testing::Test::HasFailure();
    };

    // first run without dependency
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_TRUE(verifyTokenCleanup());

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_TRUE(verifyTokenCleanup());
}

using MultiTileInOrderCmdListTests = MultiTileInOrderCmdListFixture;

HWTEST2_F(MultiTileInOrderCmdListTests, givenStandaloneEventWhenCallingAppendThenSuccess, IsAtLeastXeHpCore) {
    uint64_t counterValue = 2;
    auto hostAddress = reinterpret_cast<uint64_t *>(allocHostMem(sizeof(uint64_t)));

    *hostAddress = counterValue;
    uint64_t *gpuAddress = ptrOffset(&counterValue, 64);

    ze_event_desc_t eventDesc = {};
    ze_event_handle_t eHandle1 = nullptr;
    ze_event_handle_t eHandle2 = nullptr;
    ze_event_handle_t eHandle3 = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate(context, device, gpuAddress, hostAddress, counterValue + 1, &eventDesc, &eHandle1));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate(context, device, gpuAddress, hostAddress, counterValue + 1, &eventDesc, &eHandle2));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate(context, device, gpuAddress, hostAddress, counterValue + 1, &eventDesc, &eHandle3));

    constexpr size_t size = 128 * sizeof(uint32_t);
    auto data = allocHostMem(size);

    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();

    immCmdList->appendMemoryFill(data, data, 1, size, eHandle1, 0, nullptr, false);
    immCmdList->appendMemoryFill(data, data, 1, size, nullptr, 1, &eHandle2, false);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eHandle3, 0, nullptr, launchParams, false);

    context->freeMem(data);
    zeEventDestroy(eHandle1);
    zeEventDestroy(eHandle2);
    zeEventDestroy(eHandle3);
    context->freeMem(hostAddress);
}

HWTEST2_F(MultiTileInOrderCmdListTests, givenStandaloneEventAndKernelSplitWhenCallingAppendThenSuccess, IsAtLeastXeHpCore) {
    uint64_t counterValue = 2;
    auto hostAddress = reinterpret_cast<uint64_t *>(allocHostMem(sizeof(uint64_t)));

    *hostAddress = counterValue;
    uint64_t *gpuAddress = ptrOffset(&counterValue, 64);

    ze_event_desc_t eventDesc = {};
    ze_event_handle_t eHandle1 = nullptr;
    ze_event_handle_t eHandle2 = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate(context, device, gpuAddress, hostAddress, counterValue + 1, &eventDesc, &eHandle1));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate(context, device, gpuAddress, hostAddress, counterValue + 1, &eventDesc, &eHandle2));

    const size_t ptrBaseSize = 128;
    const size_t offset = 1;
    auto alignedPtr = alignedMalloc(ptrBaseSize, MemoryConstants::cacheLineSize);
    auto unalignedPtr = ptrOffset(alignedPtr, offset);

    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();

    immCmdList->appendMemoryCopy(unalignedPtr, unalignedPtr, ptrBaseSize - offset, eHandle1, 0, nullptr, copyParams);
    immCmdList->appendMemoryCopy(unalignedPtr, unalignedPtr, ptrBaseSize - offset, nullptr, 1, &eHandle2, copyParams);

    alignedFree(alignedPtr);
    zeEventDestroy(eHandle1);
    zeEventDestroy(eHandle2);
    context->freeMem(hostAddress);
}

HWTEST2_F(MultiTileInOrderCmdListTests, givenImplicitScalingEnabledWhenAskingForExtensionsThenReturnSyncDispatchExtension, IsAtLeastXeHpCore) {
    uint32_t count = 0;
    ze_result_t res = driverHandle->getExtensionProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    std::vector<ze_driver_extension_properties_t> extensionProperties(count);

    res = driverHandle->getExtensionProperties(&count, extensionProperties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto it = std::find_if(extensionProperties.begin(), extensionProperties.end(), [](const auto &param) {
        return (strcmp(param.name, ZE_SYNCHRONIZED_DISPATCH_EXP_NAME) == 0);
    });

    if (device->getL0GfxCoreHelper().synchronizedDispatchSupported()) {
        EXPECT_NE(extensionProperties.end(), it);
    } else {
        EXPECT_EQ(extensionProperties.end(), it);
    }
}

HWTEST2_F(MultiTileInOrderCmdListTests, givenStandaloneEventAndCopyOnlyCmdListWhenCallingAppendThenSuccess, IsAtLeastXeHpCore) {
    uint64_t counterValue = 2;
    auto hostAddress = reinterpret_cast<uint64_t *>(allocHostMem(sizeof(uint64_t)));

    *hostAddress = counterValue;
    uint64_t *gpuAddress = ptrOffset(&counterValue, 64);

    ze_event_desc_t eventDesc = {};
    ze_event_handle_t eHandle1 = nullptr;
    ze_event_handle_t eHandle2 = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate(context, device, gpuAddress, hostAddress, counterValue + 1, &eventDesc, &eHandle1));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate(context, device, gpuAddress, hostAddress, counterValue + 1, &eventDesc, &eHandle2));

    constexpr size_t size = 128 * sizeof(uint32_t);
    auto data = allocHostMem(size);

    auto immCmdList = createCopyOnlyImmCmdList<gfxCoreFamily>();

    immCmdList->appendMemoryFill(data, data, 1, size, eHandle1, 0, nullptr, false);
    immCmdList->appendMemoryFill(data, data, 1, size, nullptr, 1, &eHandle2, false);

    context->freeMem(data);
    zeEventDestroy(eHandle1);
    zeEventDestroy(eHandle2);
    context->freeMem(hostAddress);
}

HWTEST2_F(MultiTileInOrderCmdListTests, givenDebugFlagSetWhenAskingForAtomicSignallingThenReturnTrue, IsAtLeastXeHpCore) {
    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();
    auto heaplessEnabled = immCmdList->isHeaplessModeEnabled();
    if (heaplessEnabled) {
        EXPECT_TRUE(immCmdList->inOrderAtomicSignalingEnabled);
        EXPECT_EQ(partitionCount, immCmdList->getInOrderIncrementValue());

    } else {
        EXPECT_FALSE(immCmdList->inOrderAtomicSignalingEnabled);
        EXPECT_EQ(1u, immCmdList->getInOrderIncrementValue());
    }

    debugManager.flags.InOrderAtomicSignallingEnabled.set(1);

    auto immCmdList2 = createMultiTileImmCmdList<gfxCoreFamily>();

    EXPECT_TRUE(immCmdList2->inOrderAtomicSignalingEnabled);
    EXPECT_EQ(partitionCount, immCmdList2->getInOrderIncrementValue());
}

HWTEST2_F(MultiTileInOrderCmdListTests, givenAtomicSignallingEnabledWhenSignallingCounterThenUseMiAtomicCmd, IsAtLeastXeHpCore) {
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using ATOMIC_OPCODES = typename FamilyType::MI_ATOMIC::ATOMIC_OPCODES;
    using DATA_SIZE = typename FamilyType::MI_ATOMIC::DATA_SIZE;

    debugManager.flags.InOrderAtomicSignallingEnabled.set(1);

    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    EXPECT_EQ(0u, immCmdList->inOrderExecInfo->getCounterValue());

    auto handle = events[0]->toHandle();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams, false);

    EXPECT_EQ(partitionCount, immCmdList->inOrderExecInfo->getCounterValue());

    size_t offset = cmdStream->getUsed();

    immCmdList->appendWaitOnEvents(1, &handle, nullptr, false, false, true, false, false, false);

    EXPECT_EQ(partitionCount * 2, immCmdList->inOrderExecInfo->getCounterValue());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

    auto miAtomics = findAll<MI_ATOMIC *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(1u, miAtomics.size());

    auto atomicCmd = genCmdCast<MI_ATOMIC *>(*miAtomics[0]);
    ASSERT_NE(nullptr, atomicCmd);

    auto gpuAddress = immCmdList->inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(gpuAddress, NEO::UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*atomicCmd));
    EXPECT_EQ(ATOMIC_OPCODES::ATOMIC_8B_INCREMENT, atomicCmd->getAtomicOpcode());
    EXPECT_EQ(DATA_SIZE::DATA_SIZE_QWORD, atomicCmd->getDataSize());
    EXPECT_EQ(0u, atomicCmd->getReturnDataControl());
    EXPECT_EQ(0u, atomicCmd->getCsStall());
}

HWTEST2_F(MultiTileInOrderCmdListTests, givenDuplicatedCounterStorageAndAtomicSignallingEnabledWhenSignallingCounterThenUseMiAtomicAndSdiCmd, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using ATOMIC_OPCODES = typename FamilyType::MI_ATOMIC::ATOMIC_OPCODES;
    using DATA_SIZE = typename FamilyType::MI_ATOMIC::DATA_SIZE;

    debugManager.flags.InOrderAtomicSignallingEnabled.set(1);
    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(1);

    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    EXPECT_EQ(0u, immCmdList->inOrderExecInfo->getCounterValue());

    auto handle = events[0]->toHandle();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams, false);

    EXPECT_EQ(partitionCount, immCmdList->inOrderExecInfo->getCounterValue());

    size_t offset = cmdStream->getUsed();

    immCmdList->appendWaitOnEvents(1, &handle, nullptr, false, false, true, false, false, false);

    EXPECT_EQ(partitionCount * 2, immCmdList->inOrderExecInfo->getCounterValue());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

    auto miAtomics = findAll<MI_ATOMIC *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(1u, miAtomics.size());

    auto atomicCmd = genCmdCast<MI_ATOMIC *>(*miAtomics[0]);
    ASSERT_NE(nullptr, atomicCmd);

    auto gpuAddress = immCmdList->inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(gpuAddress, NEO::UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*atomicCmd));
    EXPECT_EQ(ATOMIC_OPCODES::ATOMIC_8B_INCREMENT, atomicCmd->getAtomicOpcode());
    EXPECT_EQ(DATA_SIZE::DATA_SIZE_QWORD, atomicCmd->getDataSize());
    EXPECT_EQ(0u, atomicCmd->getReturnDataControl());
    EXPECT_EQ(0u, atomicCmd->getCsStall());

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*(++miAtomics[0]));
    ASSERT_NE(nullptr, sdiCmd);

    EXPECT_EQ(immCmdList->inOrderExecInfo->getHostCounterAllocation()->getGpuAddress(), sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(partitionCount * 2, sdiCmd->getDataDword0());
    EXPECT_TRUE(sdiCmd->getWorkloadPartitionIdOffsetEnable());
}

HWTEST2_F(MultiTileInOrderCmdListTests, givenDuplicatedCounterStorageAndWithoutAtomicSignallingEnabledWhenSignallingCounterThenUseTwoSdiCmds, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(1);

    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    uint64_t counterIncrement = immCmdList->getInOrderIncrementValue();
    uint64_t expectedCounter = 0u;

    EXPECT_EQ(expectedCounter, immCmdList->inOrderExecInfo->getCounterValue());

    auto handle = events[0]->toHandle();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams, false);

    expectedCounter += counterIncrement;
    EXPECT_EQ(expectedCounter, immCmdList->inOrderExecInfo->getCounterValue());

    size_t offset = cmdStream->getUsed();

    immCmdList->appendWaitOnEvents(1, &handle, nullptr, false, false, true, false, false, false);

    expectedCounter += counterIncrement;
    EXPECT_EQ(expectedCounter, immCmdList->inOrderExecInfo->getCounterValue());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

    auto sdiCmds = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());

    auto expectedSdiCmds = 0u;
    if (!immCmdList->inOrderAtomicSignalingEnabled) {
        expectedSdiCmds++;
    }
    if (immCmdList->inOrderExecInfo->isHostStorageDuplicated()) {
        expectedSdiCmds++;
    }

    EXPECT_EQ(expectedSdiCmds, sdiCmds.size());

    auto expectedSignalValue = expectedCounter;

    auto nextSdi = 0u;
    if (!immCmdList->inOrderAtomicSignalingEnabled) {
        auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*(sdiCmds[nextSdi]));
        ASSERT_NE(nullptr, sdiCmd);
        EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), sdiCmd->getAddress());
        EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
        EXPECT_EQ(expectedSignalValue, sdiCmd->getDataDword0());
        EXPECT_TRUE(sdiCmd->getWorkloadPartitionIdOffsetEnable());
        nextSdi++;
    }

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*(sdiCmds[nextSdi]));
    ASSERT_NE(nullptr, sdiCmd);

    EXPECT_EQ(immCmdList->inOrderExecInfo->getHostCounterAllocation()->getGpuAddress(), sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(expectedSignalValue, sdiCmd->getDataDword0());
    EXPECT_TRUE(sdiCmd->getWorkloadPartitionIdOffsetEnable());
}

HWTEST2_F(MultiTileInOrderCmdListTests, givenAtomicSignallingEnabledWhenWaitingForDependencyThenUseOnlyOneSemaphore, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    debugManager.flags.InOrderAtomicSignallingEnabled.set(1);

    auto immCmdList1 = createMultiTileImmCmdList<gfxCoreFamily>();
    auto immCmdList2 = createMultiTileImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);

    auto handle = events[0]->toHandle();

    immCmdList1->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams, false);

    EXPECT_EQ(partitionCount, immCmdList1->inOrderExecInfo->getCounterValue());

    auto cmdStream = immCmdList2->getCmdContainer().getCommandStream();

    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    size_t offset = cmdStream->getUsed();

    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &handle, launchParams, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

    auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(2u + (ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired() ? 1 : 0), semaphores.size());

    auto itor = cmdList.begin();

    // implicit dependency
    auto gpuAddress = immCmdList2->inOrderExecInfo->getBaseDeviceAddress();

    ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, partitionCount, gpuAddress, immCmdList2->isQwordInOrderCounter(), false));

    // event
    ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, partitionCount, events[0]->inOrderExecInfo->getBaseDeviceAddress(), immCmdList2->isQwordInOrderCounter(), false));
}

HWTEST2_F(MultiTileInOrderCmdListTests, givenMultiTileInOrderModeWhenProgrammingWaitOnEventsThenHandleAllEventPackets, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, false);
    auto eventHandle = events[0]->toHandle();

    size_t offset = cmdStream->getUsed();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);

    auto isCompactEvent = immCmdList->compactL3FlushEvent(immCmdList->getDcFlushRequired(events[0]->isSignalScope()));

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        if (isCompactEvent) {
            auto pcItors = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
            ASSERT_NE(pcItors.size(), 0u);
            auto pcCmd = genCmdCast<PIPE_CONTROL *>(*pcItors.back());

            uint64_t address = pcCmd->getAddressHigh();
            address <<= 32;
            address |= pcCmd->getAddress();
            EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), address);
            EXPECT_EQ(immCmdList->inOrderExecInfo->getCounterValue(), pcCmd->getImmediateData());
        }
    }

    offset = cmdStream->getUsed();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, launchParams, false);

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto itor = cmdList.begin();
        if (immCmdList->isQwordInOrderCounter()) {
            std::advance(itor, 2);
        }

        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);

        if (isCompactEvent) {
            ASSERT_EQ(nullptr, semaphoreCmd); // already waited on previous call
        } else {
            ASSERT_NE(nullptr, semaphoreCmd);

            if (immCmdList->isQwordInOrderCounter()) {
                std::advance(itor, -2);
            }

            auto gpuAddress = immCmdList->inOrderExecInfo->getBaseDeviceAddress();

            if (immCmdList->inOrderExecInfo->isHostStorageDuplicated()) {
                ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, 2, gpuAddress, immCmdList->isQwordInOrderCounter(), false));
            } else {
                ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, 1, gpuAddress, immCmdList->isQwordInOrderCounter(), false));
                ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, 1, gpuAddress + device->getL0GfxCoreHelper().getImmediateWritePostSyncOffset(), immCmdList->isQwordInOrderCounter(), false));
            }
        }
    }
}

HWTEST2_F(MultiTileInOrderCmdListTests, givenMultiTileInOrderModeWhenSignalingSyncAllocationThenEnablePartitionOffset, IsAtLeastXeHpCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    immCmdList->inOrderAtomicSignalingEnabled = false;
    immCmdList->appendSignalInOrderDependencyCounter(nullptr, false, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*cmdList.begin());
    ASSERT_NE(nullptr, sdiCmd);

    auto gpuAddress = immCmdList->inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(gpuAddress, sdiCmd->getAddress());
    EXPECT_TRUE(sdiCmd->getWorkloadPartitionIdOffsetEnable());
}

HWTEST2_F(MultiTileInOrderCmdListTests, givenMultiTileInOrderModeWhenCallingSyncThenHandleCompletion, IsAtLeastXeHpCore) {
    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    auto hostAddress0 = immCmdList->inOrderExecInfo->isHostStorageDuplicated() ? immCmdList->inOrderExecInfo->getBaseHostAddress() : static_cast<uint64_t *>(immCmdList->inOrderExecInfo->getDeviceCounterAllocation()->getUnderlyingBuffer());
    auto hostAddress1 = ptrOffset(hostAddress0, device->getL0GfxCoreHelper().getImmediateWritePostSyncOffset());

    *hostAddress0 = 0;
    *hostAddress1 = 0;
    EXPECT_EQ(ZE_RESULT_NOT_READY, immCmdList->hostSynchronize(0, false));
    EXPECT_EQ(ZE_RESULT_NOT_READY, events[0]->hostSynchronize(0));

    *hostAddress0 = 1;
    EXPECT_EQ(ZE_RESULT_NOT_READY, immCmdList->hostSynchronize(0, false));
    EXPECT_EQ(ZE_RESULT_NOT_READY, events[0]->hostSynchronize(0));

    *hostAddress0 = 0;
    *hostAddress1 = 1;
    EXPECT_EQ(ZE_RESULT_NOT_READY, immCmdList->hostSynchronize(0, false));
    EXPECT_EQ(ZE_RESULT_NOT_READY, events[0]->hostSynchronize(0));

    uint64_t waitvalue = immCmdList->inOrderExecInfo->getCounterValue();

    *hostAddress0 = waitvalue;
    *hostAddress1 = waitvalue;
    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->hostSynchronize(0, false));
    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(0));

    *hostAddress0 = 3;
    *hostAddress1 = 3;
    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->hostSynchronize(0, false));
    EXPECT_EQ(ZE_RESULT_SUCCESS, events[0]->hostSynchronize(0));
}

HWTEST2_F(MultiTileInOrderCmdListTests, givenMultiTileInOrderModeWhenProgrammingTimestampEventThenHandleChaining, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, true);
    auto eventHandle = events[0]->toHandle();
    events[0]->signalScope = 0;

    bool inOrderExecSignalRequired = (immCmdList->isInOrderExecutionEnabled() && !launchParams.isKernelSplitOperation && !launchParams.pipeControlSignalling);
    bool inOrderNonWalkerSignalling = immCmdList->isInOrderNonWalkerSignalingRequired(events[0].get());

    if (!inOrderExecSignalRequired || !inOrderNonWalkerSignalling) {
        GTEST_SKIP();
    }

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      cmdStream->getCpuBase(),
                                                      cmdStream->getUsed()));

    auto walkerItor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);

    auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(walkerItor, cmdList.end());
    ASSERT_NE(cmdList.end(), semaphoreItor);

    auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphoreItor));
    ASSERT_NE(nullptr, semaphoreCmd);

    auto eventEndGpuVa = events[0]->getCompletionFieldGpuAddress(device);

    if (eventEndGpuVa != semaphoreCmd->getSemaphoreGraphicsAddress()) {
        semaphoreItor = find<MI_SEMAPHORE_WAIT *>(++semaphoreItor, cmdList.end());
        ASSERT_NE(cmdList.end(), semaphoreItor);

        semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphoreItor));
        ASSERT_NE(nullptr, semaphoreCmd);
    }

    EXPECT_EQ(static_cast<uint32_t>(Event::State::STATE_CLEARED), semaphoreCmd->getSemaphoreDataDword());
    EXPECT_EQ(eventEndGpuVa, semaphoreCmd->getSemaphoreGraphicsAddress());

    semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(++semaphoreCmd);
    EXPECT_EQ(static_cast<uint32_t>(Event::State::STATE_CLEARED), semaphoreCmd->getSemaphoreDataDword());
    EXPECT_EQ(eventEndGpuVa + events[0]->getSinglePacketSize(), semaphoreCmd->getSemaphoreGraphicsAddress());
}

HWTEST2_F(MultiTileInOrderCmdListTests, givenMultiTileInOrderModeWhenProgrammingTimestampEventThenHandlePacketsChaining, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createMultiTileImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto eventPool = createEvents<FamilyType>(1, true);
    auto eventHandle = events[0]->toHandle();
    events[0]->signalScope = 0;

    immCmdList->signalAllEventPackets = true;
    events[0]->maxPacketCount = 4;

    bool inOrderExecSignalRequired = (immCmdList->isInOrderExecutionEnabled() && !launchParams.isKernelSplitOperation && !launchParams.pipeControlSignalling);
    bool inOrderNonWalkerSignalling = immCmdList->isInOrderNonWalkerSignalingRequired(events[0].get());

    if (!inOrderExecSignalRequired || !inOrderNonWalkerSignalling) {
        GTEST_SKIP();
    }

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, eventHandle, 0, nullptr, launchParams, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      cmdStream->getCpuBase(),
                                                      cmdStream->getUsed()));

    auto walkerItor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerItor);

    auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(walkerItor, cmdList.end());
    ASSERT_NE(cmdList.end(), semaphoreItor);

    auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphoreItor));
    ASSERT_NE(nullptr, semaphoreCmd);

    auto eventEndGpuVa = events[0]->getCompletionFieldGpuAddress(device);

    if (eventEndGpuVa != semaphoreCmd->getSemaphoreGraphicsAddress()) {
        semaphoreItor = find<MI_SEMAPHORE_WAIT *>(++semaphoreItor, cmdList.end());
        ASSERT_NE(cmdList.end(), semaphoreItor);

        semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphoreItor));
        ASSERT_NE(nullptr, semaphoreCmd);
    }

    EXPECT_EQ(static_cast<uint32_t>(Event::State::STATE_CLEARED), semaphoreCmd->getSemaphoreDataDword());
    EXPECT_EQ(eventEndGpuVa, semaphoreCmd->getSemaphoreGraphicsAddress());

    semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(++semaphoreCmd);
    auto offset = events[0]->getSinglePacketSize();
    EXPECT_EQ(static_cast<uint32_t>(Event::State::STATE_CLEARED), semaphoreCmd->getSemaphoreDataDword());
    EXPECT_EQ(eventEndGpuVa + offset, semaphoreCmd->getSemaphoreGraphicsAddress());

    semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(++semaphoreCmd);
    offset += events[0]->getSinglePacketSize();
    EXPECT_EQ(static_cast<uint32_t>(Event::State::STATE_CLEARED), semaphoreCmd->getSemaphoreDataDword());
    EXPECT_EQ(eventEndGpuVa + offset, semaphoreCmd->getSemaphoreGraphicsAddress());

    semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(++semaphoreCmd);
    offset += events[0]->getSinglePacketSize();
    EXPECT_EQ(static_cast<uint32_t>(Event::State::STATE_CLEARED), semaphoreCmd->getSemaphoreDataDword());
    EXPECT_EQ(eventEndGpuVa + offset, semaphoreCmd->getSemaphoreGraphicsAddress());
}

HWTEST2_F(MultiTileInOrderCmdListTests, whenUsingRegularCmdListThenAddWalkerToPatch, IsAtLeastXeHpCore) {
    using WalkerVariant = typename FamilyType::WalkerVariant;

    ze_command_queue_desc_t desc = {};

    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(true, false, false);
    auto regularCmdList = createRegularCmdList<gfxCoreFamily>(false);
    regularCmdList->partitionCount = 2;
    bool isHeaplessEnabled = regularCmdList->isHeaplessModeEnabled();

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();

    size_t offset = cmdStream->getUsed();

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    auto nSemaphores = regularCmdList->inOrderExecInfo->getNumDevicePartitionsToWait();
    auto nWalkers = 2u;

    ASSERT_EQ(nWalkers + nSemaphores, regularCmdList->inOrderPatchCmds.size()); // Walker + N x Semaphore + Walker

    auto lastWalkerI = nWalkers + nSemaphores - 1;
    WalkerVariant walkerVariantFromContainer1 = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(regularCmdList->inOrderPatchCmds[0].cmd1);
    WalkerVariant walkerVariantFromContainer2 = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(regularCmdList->inOrderPatchCmds[lastWalkerI].cmd1);
    std::visit([](auto &&walker1, auto &&walker2) {
        ASSERT_NE(nullptr, walker1);
        ASSERT_NE(nullptr, walker2);
    },
               walkerVariantFromContainer1, walkerVariantFromContainer2);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdStream->getCpuBase(), offset),
                                                      (cmdStream->getUsed() - offset)));

    auto itor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);
    WalkerVariant walkerVariantFromParser1 = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(*itor);

    itor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(++itor, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);
    WalkerVariant walkerVariantFromParser2 = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(*itor);

    EXPECT_EQ(isHeaplessEnabled ? 4u : 2u, regularCmdList->inOrderExecInfo->getCounterValue());

    std::visit([&](auto &&walkerFromParser1, auto &&walkerFromParser2, auto &&walkerFromContainer1, auto &&walkerFromContainer2) {
        auto verifyPatching = [&](uint64_t executionCounter) {
            auto appendValue = regularCmdList->inOrderExecInfo->getCounterValue() * executionCounter;

            if (isHeaplessEnabled) {
                EXPECT_EQ(0u, walkerFromContainer1->getPostSync().getImmediateData());
                EXPECT_EQ(0u, walkerFromParser1->getPostSync().getImmediateData());
                EXPECT_EQ(0u, walkerFromContainer2->getPostSync().getImmediateData());
                EXPECT_EQ(0u, walkerFromParser2->getPostSync().getImmediateData());
            } else {
                EXPECT_EQ(1u + appendValue, walkerFromContainer1->getPostSync().getImmediateData());
                EXPECT_EQ(1u + appendValue, walkerFromParser1->getPostSync().getImmediateData());
                EXPECT_EQ(2u + appendValue, walkerFromContainer2->getPostSync().getImmediateData());
                EXPECT_EQ(2u + appendValue, walkerFromParser2->getPostSync().getImmediateData());
            }
        };

        regularCmdList->close();

        auto handle = regularCmdList->toHandle();

        mockCmdQHw->executeCommandLists(1, &handle, nullptr, false, nullptr);
        verifyPatching(0);

        mockCmdQHw->executeCommandLists(1, &handle, nullptr, false, nullptr);
        verifyPatching(1);

        mockCmdQHw->executeCommandLists(1, &handle, nullptr, false, nullptr);
        verifyPatching(2);
    },
               walkerVariantFromParser1, walkerVariantFromParser2, walkerVariantFromContainer1, walkerVariantFromContainer2);
}

struct BcsSplitInOrderCmdListTests : public InOrderCmdListFixture {
    void SetUp() override {
        NEO::debugManager.flags.SplitBcsCopy.set(1);
        NEO::debugManager.flags.EnableFlushTaskSubmission.set(0);

        hwInfoBackup = std::make_unique<VariableBackup<HardwareInfo>>(defaultHwInfo.get());
        defaultHwInfo->capabilityTable.blitterOperationsSupported = true;
        defaultHwInfo->featureTable.ftrBcsInfo = 0b111111111;

        InOrderCmdListFixture::SetUp();
    }

    bool verifySplit(uint64_t expectedTaskCount) {
        auto &bcsSplit = static_cast<DeviceImp *>(device)->bcsSplit;

        for (uint32_t i = 0; i < numLinkCopyEngines; i++) {
            if (static_cast<CommandQueueImp *>(bcsSplit.cmdQs[0])->getTaskCount() != expectedTaskCount) {
                return false;
            }
        }

        return true;
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    DestroyableZeUniquePtr<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>> createBcsSplitImmCmdList() {
        auto cmdList = createCopyOnlyImmCmdList<gfxCoreFamily>();

        auto &bcsSplit = static_cast<DeviceImp *>(device)->bcsSplit;

        ze_command_queue_desc_t desc = {};
        desc.ordinal = static_cast<uint32_t>(device->getNEODevice()->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::copy));

        cmdList->isBcsSplitNeeded = bcsSplit.setupDevice(device->getHwInfo().platform.eProductFamily, false, &desc, cmdList->getCsr(false));
        cmdList->isFlushTaskSubmissionEnabled = false;

        return cmdList;
    }

    template <typename FamilyType, GFXCORE_FAMILY gfxCoreFamily>
    void verifySplitCmds(LinearStream &cmdStream, size_t streamOffset, L0::Device *device, uint64_t submissionId, WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>> &immCmdList,
                         uint64_t externalDependencyGpuVa);

    std::unique_ptr<VariableBackup<HardwareInfo>> hwInfoBackup;
    const uint32_t numLinkCopyEngines = 4;
};

template <typename FamilyType, GFXCORE_FAMILY gfxCoreFamily>
void BcsSplitInOrderCmdListTests::verifySplitCmds(LinearStream &cmdStream, size_t streamOffset, L0::Device *device, uint64_t submissionId,
                                                  WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>> &immCmdList, uint64_t externalDependencyGpuVa) {
    using XY_COPY_BLT = typename std::remove_const<decltype(FamilyType::cmdInitXyCopyBlt)>::type;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    auto &bcsSplit = static_cast<DeviceImp *>(device)->bcsSplit;

    auto inOrderExecInfo = immCmdList.inOrderExecInfo;

    auto counterGpuAddress = inOrderExecInfo->isHostStorageDuplicated() ? reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress()) : inOrderExecInfo->getBaseDeviceAddress();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream.getCpuBase(), streamOffset), (cmdStream.getUsed() - streamOffset)));

    auto itor = cmdList.begin();

    for (uint32_t i = 0; i < numLinkCopyEngines; i++) {
        auto beginItor = itor;

        auto signalSubCopyEventGpuVa = bcsSplit.events.subcopy[i + (submissionId * numLinkCopyEngines)]->getCompletionFieldGpuAddress(device);

        size_t numExpectedSemaphores = 0;

        if (submissionId > 0) {
            numExpectedSemaphores++;
            itor = find<MI_SEMAPHORE_WAIT *>(itor, cmdList.end());
            ASSERT_NE(cmdList.end(), itor);

            if (immCmdList.isQwordInOrderCounter()) {
                std::advance(itor, -2); // verify 2x LRI before semaphore
            }

            ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, submissionId, counterGpuAddress, immCmdList.isQwordInOrderCounter(), true));
        }

        if (externalDependencyGpuVa > 0) {
            numExpectedSemaphores++;
            itor = find<MI_SEMAPHORE_WAIT *>(itor, cmdList.end());
            ASSERT_NE(cmdList.end(), itor);
            auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);
            ASSERT_NE(nullptr, semaphoreCmd);

            EXPECT_EQ(externalDependencyGpuVa, semaphoreCmd->getSemaphoreGraphicsAddress());
        }

        itor = find<XY_COPY_BLT *>(itor, cmdList.end());
        ASSERT_NE(cmdList.end(), itor);
        ASSERT_NE(nullptr, genCmdCast<XY_COPY_BLT *>(*itor));

        auto flushDwItor = find<MI_FLUSH_DW *>(++itor, cmdList.end());
        ASSERT_NE(cmdList.end(), flushDwItor);

        auto signalSubCopyEvent = genCmdCast<MI_FLUSH_DW *>(*flushDwItor);
        ASSERT_NE(nullptr, signalSubCopyEvent);

        while (signalSubCopyEvent->getDestinationAddress() != signalSubCopyEventGpuVa) {
            flushDwItor = find<MI_FLUSH_DW *>(++flushDwItor, cmdList.end());
            ASSERT_NE(cmdList.end(), flushDwItor);

            signalSubCopyEvent = genCmdCast<MI_FLUSH_DW *>(*flushDwItor);
            ASSERT_NE(nullptr, signalSubCopyEvent);
        }

        itor = ++flushDwItor;

        auto semaphoreCmds = findAll<MI_SEMAPHORE_WAIT *>(beginItor, itor);
        EXPECT_EQ(numExpectedSemaphores, semaphoreCmds.size());
    }

    auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(itor, cmdList.end());

    if (submissionId > 0) {
        ASSERT_NE(cmdList.end(), semaphoreItor);
        if (immCmdList.isQwordInOrderCounter()) {
            std::advance(semaphoreItor, -2); // verify 2x LRI before semaphore
        }

        ASSERT_TRUE(verifyInOrderDependency<FamilyType>(semaphoreItor, submissionId, counterGpuAddress, immCmdList.isQwordInOrderCounter(), true));
    }

    for (uint32_t i = 0; i < numLinkCopyEngines; i++) {
        auto subCopyEventSemaphore = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreItor);
        ASSERT_NE(nullptr, subCopyEventSemaphore);

        EXPECT_EQ(bcsSplit.events.subcopy[i + (submissionId * numLinkCopyEngines)]->getCompletionFieldGpuAddress(device), subCopyEventSemaphore->getSemaphoreGraphicsAddress());

        itor = ++semaphoreItor;
    }

    ASSERT_NE(nullptr, genCmdCast<MI_FLUSH_DW *>(*itor)); // marker event

    auto implicitCounterSdi = genCmdCast<MI_STORE_DATA_IMM *>(*(++itor));
    ASSERT_NE(nullptr, implicitCounterSdi);

    EXPECT_EQ(counterGpuAddress, implicitCounterSdi->getAddress());
    EXPECT_EQ(submissionId + 1, implicitCounterSdi->getDataDword0());

    EXPECT_EQ(submissionId + 1, immCmdList.inOrderExecInfo->getCounterValue());

    auto sdiCmds = findAll<MI_STORE_DATA_IMM *>(++itor, cmdList.end());
    EXPECT_EQ(0u, sdiCmds.size());
}

HWTEST2_F(BcsSplitInOrderCmdListTests, givenBcsSplitEnabledWhenDispatchingCopyThenHandleInOrderSignaling, IsAtLeastXeHpcCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createBcsSplitImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    uint32_t copyData = 0;
    constexpr size_t copySize = 8 * MemoryConstants::megaByte;

    EXPECT_TRUE(verifySplit(0));

    immCmdList->appendMemoryCopy(&copyData, &copyData, copySize, nullptr, 0, nullptr, copyParams);

    EXPECT_TRUE(verifySplit(1));

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), semaphoreItor);

    auto sdiItor = find<MI_STORE_DATA_IMM *>(semaphoreItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    ASSERT_NE(nullptr, sdiCmd);

    auto gpuAddress = immCmdList->inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(gpuAddress, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(1u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());

    auto &bcsSplit = static_cast<DeviceImp *>(device)->bcsSplit;

    for (auto &event : bcsSplit.events.barrier) {
        EXPECT_FALSE(event->isCounterBased());
    }
    for (auto &event : bcsSplit.events.subcopy) {
        EXPECT_FALSE(event->isCounterBased());
    }
    for (auto &event : bcsSplit.events.marker) {
        EXPECT_FALSE(event->isCounterBased());
    }
}

HWTEST2_F(BcsSplitInOrderCmdListTests, givenBcsSplitEnabledWhenAppendingMemoryCopyAfterBarrierWithoutImplicitDependenciesThenHandleCorrectInOrderSignaling, IsAtLeastXeHpcCore) {
    auto immCmdList = createBcsSplitImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    uint32_t copyData = 0;
    constexpr size_t copySize = 8 * MemoryConstants::megaByte;

    *immCmdList->getCsr(false)->getBarrierCountTagAddress() = 0u;
    immCmdList->getCsr(false)->getNextBarrierCount();

    size_t offset = cmdStream->getUsed();

    immCmdList->appendMemoryCopy(&copyData, &copyData, copySize, nullptr, 0, nullptr, copyParams);

    // no implicit dependencies
    verifySplitCmds<FamilyType, gfxCoreFamily>(*cmdStream, offset, device, 0, *immCmdList, 0);
}

HWTEST2_F(BcsSplitInOrderCmdListTests, givenBcsSplitEnabledWhenAppendingMemoryCopyAfterBarrierWithImplicitDependenciesThenHandleCorrectInOrderSignaling, IsAtLeastXeHpcCore) {
    auto immCmdList = createBcsSplitImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    uint32_t copyData = 0;
    constexpr size_t copySize = 8 * MemoryConstants::megaByte;

    *immCmdList->getCsr(false)->getBarrierCountTagAddress() = 0u;
    immCmdList->getCsr(false)->getNextBarrierCount();

    immCmdList->appendMemoryCopy(&copyData, &copyData, copySize, nullptr, 0, nullptr, copyParams);

    size_t offset = cmdStream->getUsed();

    *immCmdList->getCsr(false)->getBarrierCountTagAddress() = 0u;
    immCmdList->getCsr(false)->getNextBarrierCount();
    immCmdList->appendMemoryCopy(&copyData, &copyData, copySize, nullptr, 0, nullptr, copyParams);

    // implicit dependencies
    verifySplitCmds<FamilyType, gfxCoreFamily>(*cmdStream, offset, device, 1, *immCmdList, 0);
}

HWTEST2_F(BcsSplitInOrderCmdListTests, givenBcsSplitEnabledWhenAppendingMemoryCopyWithEventDependencyThenRequiredSemaphores, IsAtLeastXeHpcCore) {
    auto immCmdList = createBcsSplitImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    uint32_t copyData = 0;
    constexpr size_t copySize = 8 * MemoryConstants::megaByte;

    auto eventPool = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    auto eventHandle = events[0]->toHandle();

    immCmdList->appendMemoryCopy(&copyData, &copyData, copySize, nullptr, 0, nullptr, copyParams);

    size_t offset = cmdStream->getUsed();

    immCmdList->appendMemoryCopy(&copyData, &copyData, copySize, nullptr, 1, &eventHandle, copyParams);

    verifySplitCmds<FamilyType, gfxCoreFamily>(*cmdStream, offset, device, 1, *immCmdList, events[0]->getCompletionFieldGpuAddress(device));
}

HWTEST2_F(BcsSplitInOrderCmdListTests, givenBcsSplitEnabledWhenDispatchingCopyRegionThenHandleInOrderSignaling, IsAtLeastXeHpcCore) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto immCmdList = createBcsSplitImmCmdList<gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    uint32_t copyData = 0;
    constexpr size_t copySize = 8 * MemoryConstants::megaByte;

    EXPECT_TRUE(verifySplit(0));

    ze_copy_region_t region = {0, 0, 0, copySize, 1, 1};

    immCmdList->appendMemoryCopyRegion(&copyData, &region, 1, 1, &copyData, &region, 1, 1, nullptr, 0, nullptr, copyParams);

    EXPECT_TRUE(verifySplit(1));

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), semaphoreItor);

    auto sdiItor = find<MI_STORE_DATA_IMM *>(semaphoreItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);

    auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    ASSERT_NE(nullptr, sdiCmd);

    auto gpuAddress = immCmdList->inOrderExecInfo->getBaseDeviceAddress();

    EXPECT_EQ(gpuAddress, sdiCmd->getAddress());
    EXPECT_EQ(immCmdList->isQwordInOrderCounter(), sdiCmd->getStoreQword());
    EXPECT_EQ(1u, sdiCmd->getDataDword0());
    EXPECT_EQ(0u, sdiCmd->getDataDword1());
}

HWTEST2_F(BcsSplitInOrderCmdListTests, givenImmediateCmdListWhenDispatchingWithRegularEventThenSwitchToCounterBased, IsAtLeastXeHpcCore) {
    auto immCmdList = createBcsSplitImmCmdList<gfxCoreFamily>();

    auto eventPool = createEvents<FamilyType>(1, true);

    auto eventHandle = events[0]->toHandle();
    constexpr size_t copySize = 8 * MemoryConstants::megaByte;

    uint32_t copyData[64] = {};

    events[0]->makeCounterBasedInitiallyDisabled(eventPool->getAllocation());
    immCmdList->appendMemoryCopy(&copyData, &copyData, copySize, eventHandle, 0, nullptr, copyParams);

    if (immCmdList->getDcFlushRequired(true)) {
        EXPECT_EQ(Event::CounterBasedMode::initiallyDisabled, events[0]->counterBasedMode);
    } else {
        EXPECT_EQ(Event::CounterBasedMode::implicitlyEnabled, events[0]->counterBasedMode);
    }

    EXPECT_TRUE(verifySplit(1));
}

} // namespace ult
} // namespace L0
