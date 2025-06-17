/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/scratch_space_controller.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/program/sync_buffer_handler.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_operations_handler.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test_checks_shared.h"

#include "level_zero/core/source/builtin/builtin_functions_lib.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/core/test/unit_tests/mocks/mock_image.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/sources/helper/ze_object_utils.h"

#include "test_traits_common.h"

using namespace NEO;
#include "shared/test/common/test_macros/header/heapless_matchers.h"

namespace L0 {
namespace ult {

using MultiTileImmediateCommandListTest = Test<MultiTileCommandListFixture<true, false, false, -1>>;

HWTEST2_F(MultiTileImmediateCommandListTest, GivenMultiTileDeviceWhenCreatingImmediateCommandListThenExpectPartitionCountMatchTileCount, IsXeCore) {
    EXPECT_EQ(2u, device->getNEODevice()->getDeviceBitfield().count());
    EXPECT_EQ(2u, commandList->partitionCount);

    auto returnValue = commandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(2u, commandList->partitionCount);
}

HWTEST2_F(MultiTileImmediateCommandListTest, givenMultipleTilesWhenAllocatingBarrierSyncBufferThenEnsureCorrectSize, IsAtLeastXeCore) {
    EXPECT_EQ(2u, device->getNEODevice()->getDeviceBitfield().count());

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo()->gtSystemInfo.MultiTileArchInfo.TileCount = 3;

    Mock<KernelImp> mockKernel;

    auto cmdListImmediate = static_cast<CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily> *>(static_cast<L0::CommandListImp *>(commandList.get()));
    auto whiteBoxCmdList = static_cast<WhiteBox<::L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(cmdListImmediate);

    ze_group_count_t threadGroupDimensions = {};
    threadGroupDimensions.groupCountX = 32;
    threadGroupDimensions.groupCountY = 2;
    threadGroupDimensions.groupCountZ = 2;

    size_t requestedNumberOfWorkgroups = threadGroupDimensions.groupCountX * threadGroupDimensions.groupCountY * threadGroupDimensions.groupCountZ;

    size_t localRegionSize = 4;
    size_t patchIndex = 0;

    whiteBoxCmdList->programRegionGroupBarrier(mockKernel, threadGroupDimensions, localRegionSize, patchIndex);

    auto patchData = neoDevice->syncBufferHandler->obtainAllocationAndOffset(1);

    size_t expectedOffset = alignUp((requestedNumberOfWorkgroups / localRegionSize) * (localRegionSize + 1) * 2 * sizeof(uint32_t), MemoryConstants::cacheLineSize);

    EXPECT_EQ(patchData.second, expectedOffset);
}

using MultiTileImmediateInternalCommandListTest = Test<MultiTileCommandListFixture<true, true, false, -1>>;

HWTEST2_F(MultiTileImmediateInternalCommandListTest, GivenMultiTileDeviceWhenCreatingInternalImmediateCommandListThenExpectPartitionCountEqualOne, IsXeCore) {
    EXPECT_EQ(2u, device->getNEODevice()->getDeviceBitfield().count());
    EXPECT_EQ(1u, commandList->partitionCount);

    auto returnValue = commandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(1u, commandList->partitionCount);
}

using MultiTileCopyEngineCommandListTest = Test<MultiTileCommandListFixture<false, false, true, -1>>;

HWTEST2_F(MultiTileCopyEngineCommandListTest, GivenMultiTileDeviceWhenCreatingCopyEngineCommandListThenExpectPartitionCountEqualOne, IsXeCore) {
    EXPECT_EQ(2u, device->getNEODevice()->getDeviceBitfield().count());
    EXPECT_EQ(1u, commandList->partitionCount);

    auto returnValue = commandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(1u, commandList->partitionCount);
}

using CommandListExecuteImmediate = Test<DeviceFixture>;
HWTEST2_F(CommandListExecuteImmediate, whenExecutingCommandListImmediateWithFlushTaskThenRequiredStreamStateIsCorrectlyReported, IsAtMostXe3Core) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseImmediateFlushTask.set(0);

    auto &productHelper = device->getProductHelper();

    std::unique_ptr<L0::CommandList> commandList;
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    commandList.reset(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    auto &commandListImmediate = static_cast<MockCommandListImmediate<FamilyType::gfxCoreFamily> &>(*commandList);

    auto &currentCsrStreamProperties = commandListImmediate.getCsr(false)->getStreamProperties();

    commandListImmediate.requiredStreamState.frontEndState.computeDispatchAllWalkerEnable.value = 1;
    commandListImmediate.requiredStreamState.frontEndState.disableEUFusion.value = 1;
    commandListImmediate.requiredStreamState.frontEndState.disableOverdispatch.value = 1;
    commandListImmediate.requiredStreamState.stateComputeMode.isCoherencyRequired.value = 0;
    commandListImmediate.requiredStreamState.stateComputeMode.largeGrfMode.value = 1;
    commandListImmediate.requiredStreamState.stateComputeMode.threadArbitrationPolicy.value = NEO::ThreadArbitrationPolicy::RoundRobin;
    commandListImmediate.executeCommandListImmediateWithFlushTask(false, false, false, NEO::AppendOperations::kernel, false, false, nullptr, nullptr);

    NEO::StateComputeModePropertiesSupport scmPropertiesSupport = {};
    productHelper.fillScmPropertiesSupportStructure(scmPropertiesSupport);
    NEO::FrontEndPropertiesSupport frontEndPropertiesSupport = {};
    productHelper.fillFrontEndPropertiesSupportStructure(frontEndPropertiesSupport, device->getHwInfo());

    int expectedDisableOverdispatch = frontEndPropertiesSupport.disableOverdispatch;
    int expectedLargeGrfMode = scmPropertiesSupport.largeGrfMode ? 1 : -1;
    int expectedThreadArbitrationPolicy = scmPropertiesSupport.threadArbitrationPolicy ? NEO::ThreadArbitrationPolicy::RoundRobin : -1;

    int expectedComputeDispatchAllWalkerEnable = frontEndPropertiesSupport.computeDispatchAllWalker ? 1 : -1;
    int expectedDisableEuFusion = frontEndPropertiesSupport.disableEuFusion ? 1 : -1;
    expectedDisableOverdispatch = frontEndPropertiesSupport.disableOverdispatch ? expectedDisableOverdispatch : -1;

    EXPECT_EQ(expectedComputeDispatchAllWalkerEnable, currentCsrStreamProperties.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(expectedDisableEuFusion, currentCsrStreamProperties.frontEndState.disableEUFusion.value);
    EXPECT_EQ(expectedDisableOverdispatch, currentCsrStreamProperties.frontEndState.disableOverdispatch.value);

    EXPECT_EQ(expectedLargeGrfMode, currentCsrStreamProperties.stateComputeMode.largeGrfMode.value);
    EXPECT_EQ(expectedThreadArbitrationPolicy, currentCsrStreamProperties.stateComputeMode.threadArbitrationPolicy.value);

    commandListImmediate.requiredStreamState.frontEndState.computeDispatchAllWalkerEnable.value = 0;
    commandListImmediate.requiredStreamState.frontEndState.disableEUFusion.value = 0;
    commandListImmediate.requiredStreamState.frontEndState.disableOverdispatch.value = 0;
    commandListImmediate.requiredStreamState.stateComputeMode.isCoherencyRequired.value = 0;
    commandListImmediate.requiredStreamState.stateComputeMode.largeGrfMode.value = 0;
    commandListImmediate.requiredStreamState.stateComputeMode.threadArbitrationPolicy.value = NEO::ThreadArbitrationPolicy::AgeBased;
    commandListImmediate.executeCommandListImmediateWithFlushTask(false, false, false, NEO::AppendOperations::kernel, false, false, nullptr, nullptr);

    expectedLargeGrfMode = scmPropertiesSupport.largeGrfMode ? 0 : -1;
    expectedThreadArbitrationPolicy = scmPropertiesSupport.threadArbitrationPolicy ? NEO::ThreadArbitrationPolicy::AgeBased : -1;

    expectedComputeDispatchAllWalkerEnable = frontEndPropertiesSupport.computeDispatchAllWalker ? 0 : -1;
    expectedDisableOverdispatch = frontEndPropertiesSupport.disableOverdispatch ? 0 : -1;
    expectedDisableEuFusion = frontEndPropertiesSupport.disableEuFusion ? 0 : -1;

    EXPECT_EQ(expectedComputeDispatchAllWalkerEnable, currentCsrStreamProperties.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(expectedDisableEuFusion, currentCsrStreamProperties.frontEndState.disableEUFusion.value);
    EXPECT_EQ(expectedDisableOverdispatch, currentCsrStreamProperties.frontEndState.disableOverdispatch.value);

    EXPECT_EQ(expectedLargeGrfMode, currentCsrStreamProperties.stateComputeMode.largeGrfMode.value);
    EXPECT_EQ(expectedThreadArbitrationPolicy, currentCsrStreamProperties.stateComputeMode.threadArbitrationPolicy.value);
}

HWTEST_F(CommandListExecuteImmediate, whenExecutingCommandListImmediateWithFlushTaskThenContainsAnyKernelFlagIsReset) {
    std::unique_ptr<L0::CommandList> commandList;
    DebugManagerStateRestore restorer;
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;

    std::vector<std::pair<bool, bool>> flagCombinations = {
        {true, true},
        {true, false},
        {false, true},
        {false, false}};

    for (const auto &[forceMemoryPrefetch, enableBOChunkingPrefetch] : flagCombinations) {
        debugManager.flags.ForceMemoryPrefetchForKmdMigratedSharedAllocations.set(forceMemoryPrefetch);
        debugManager.flags.EnableBOChunkingPrefetch.set(enableBOChunkingPrefetch);

        commandList.reset(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
        auto &commandListImmediate = static_cast<MockCommandListImmediate<FamilyType::gfxCoreFamily> &>(*commandList);
        commandListImmediate.containsAnyKernel = true;
        commandListImmediate.executeCommandListImmediateWithFlushTask(false, false, false, NEO::AppendOperations::kernel, false, false, nullptr, nullptr);

        EXPECT_FALSE(commandListImmediate.containsAnyKernel);
    }
}

HWTEST_F(CommandListExecuteImmediate, whenExecutingCommandListImmediateWithFlushTaskThenSuccessIsReturned) {
    std::unique_ptr<L0::CommandList> commandList;
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    commandList.reset(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    auto &commandListImmediate = static_cast<MockCommandListImmediate<FamilyType::gfxCoreFamily> &>(*commandList);

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandListImmediate.executeCommandListImmediateWithFlushTask(false, false, false, NEO::AppendOperations::nonKernel, false, false, nullptr, nullptr));
}

HWTEST_F(CommandListExecuteImmediate, whenExecutingCommandListImmediateWithFlushTaskWithMemAdvicesThenMemAdvicesAreDispatchedAndSuccessIsReturned) {

    MockCommandListForExecuteMemAdvise<FamilyType::gfxCoreFamily> commandList;
    commandList.initialize(device, NEO::EngineGroupType::compute, 0u);

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    MockCommandStreamReceiver mockCommandStreamReceiver(*neoDevice->executionEnvironment, neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());
    Mock<CommandQueue> mockCommandQueue(device, &mockCommandStreamReceiver, &desc);
    auto oldCommandQueue = commandList.cmdQImmediate;
    commandList.cmdQImmediate = &mockCommandQueue;
    commandList.indirectAllocationsAllowed = false;

    commandList.getMemAdviseOperations().push_back(MemAdviseOperation(0, 0, 16, ZE_MEMORY_ADVICE_SET_PREFERRED_LOCATION));
    EXPECT_EQ(1u, commandList.getMemAdviseOperations().size());
    commandList.getMemAdviseOperations().push_back(MemAdviseOperation(0, 0, 8, ZE_MEMORY_ADVICE_SET_PREFERRED_LOCATION));
    EXPECT_EQ(2u, commandList.getMemAdviseOperations().size());

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList.executeCommandListImmediateWithFlushTask(false, false, false, NEO::AppendOperations::none, false, false, nullptr, nullptr));
    EXPECT_EQ(0u, commandList.getMemAdviseOperations().size());
    EXPECT_EQ(2u, commandList.executeMemAdviseCallCount);

    commandList.cmdQImmediate = oldCommandQueue;
}

HWTEST_F(CommandListExecuteImmediate, givenOutOfHostMemoryErrorOnFlushWhenExecutingCommandListImmediateWithFlushTaskThenProperErrorIsReturned) {
    std::unique_ptr<L0::CommandList> commandList;
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    commandList.reset(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    auto &commandListImmediate = static_cast<MockCommandListImmediate<FamilyType::gfxCoreFamily> &>(*commandList);

    auto &commandStreamReceiver = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfHostMemory;
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, commandListImmediate.executeCommandListImmediateWithFlushTask(false, false, false, NEO::AppendOperations::nonKernel, false, false, nullptr, nullptr));
}

HWTEST_F(CommandListExecuteImmediate, givenOutOfDeviceMemoryErrorOnFlushWhenExecutingCommandListImmediateWithFlushTaskThenProperErrorIsReturned) {
    std::unique_ptr<L0::CommandList> commandList;
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    commandList.reset(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    auto &commandListImmediate = static_cast<MockCommandListImmediate<FamilyType::gfxCoreFamily> &>(*commandList);

    auto &commandStreamReceiver = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfMemory;
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, commandListImmediate.executeCommandListImmediateWithFlushTask(false, false, false, NEO::AppendOperations::nonKernel, false, false, nullptr, nullptr));
}

HWTEST_F(CommandListExecuteImmediate, GivenImmediateCommandListWhenCommandListIsCreatedThenCsrStateIsNotSet) {
    std::unique_ptr<L0::CommandList> commandList;
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    commandList.reset(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    auto &commandListImmediate = static_cast<MockCommandListImmediate<FamilyType::gfxCoreFamily> &>(*commandList);
    if (commandListImmediate.isHeaplessStateInitEnabled()) {
        GTEST_SKIP();
    }

    auto &currentCsrStreamProperties = commandListImmediate.getCsr(false)->getStreamProperties();
    EXPECT_EQ(-1, currentCsrStreamProperties.stateComputeMode.isCoherencyRequired.value);
    EXPECT_EQ(-1, currentCsrStreamProperties.stateComputeMode.devicePreemptionMode.value);

    EXPECT_EQ(-1, currentCsrStreamProperties.frontEndState.disableOverdispatch.value);
    EXPECT_EQ(-1, currentCsrStreamProperties.frontEndState.singleSliceDispatchCcsMode.value);

    EXPECT_EQ(-1, currentCsrStreamProperties.pipelineSelect.modeSelected.value);
    EXPECT_EQ(-1, currentCsrStreamProperties.pipelineSelect.mediaSamplerDopClockGate.value);
}

struct CommandListTest : Test<DeviceFixture> {
    CmdListMemoryCopyParams copyParams = {};
};
using IsDcFlushSupportedPlatform = IsGen12LP;

HWTEST_F(CommandListTest, givenCopyCommandListWhenAppendCopyWithDependenciesThenDoNotTrackDependencies) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    queue->isCopyOnlyCommandQueue = true;

    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    cmdList.commandContainer.setImmediateCmdListCsr(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    ze_result_t result = ZE_RESULT_SUCCESS;
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x5678);
    auto zeEvent = event->toHandle();

    cmdList.appendMemoryCopy(dstPtr, srcPtr, sizeof(uint32_t), nullptr, 1, &zeEvent, copyParams);

    EXPECT_EQ(device->getNEODevice()->getDefaultEngine().commandStreamReceiver->peekBarrierCount(), 0u);

    cmdList.getCsr(false)->getInternalAllocationStorage()->getTemporaryAllocations().freeAllGraphicsAllocations(device->getNEODevice());
}

HWTEST_F(CommandListTest, givenCopyCommandListWhenAppendCopyRegionWithDependenciesThenDoNotTrackDependencies) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    queue->isCopyOnlyCommandQueue = true;

    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    cmdList.commandContainer.setImmediateCmdListCsr(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    ze_result_t result = ZE_RESULT_SUCCESS;
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x5678);
    auto zeEvent = event->toHandle();
    ze_copy_region_t region = {};

    cmdList.appendMemoryCopyRegion(dstPtr, &region, 0, 0, srcPtr, &region, 0, 0, nullptr, 1, &zeEvent, copyParams);

    EXPECT_EQ(device->getNEODevice()->getDefaultEngine().commandStreamReceiver->peekBarrierCount(), 0u);

    cmdList.getCsr(false)->getInternalAllocationStorage()->getTemporaryAllocations().freeAllGraphicsAllocations(device->getNEODevice());
}

HWTEST_F(CommandListTest, givenCopyCommandListWhenAppendFillWithDependenciesThenDoNotTrackDependencies) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    queue->isCopyOnlyCommandQueue = true;

    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    cmdList.commandContainer.setImmediateCmdListCsr(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    ze_result_t result = ZE_RESULT_SUCCESS;
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    uint32_t patter = 1;
    auto zeEvent = event->toHandle();

    cmdList.appendMemoryFill(srcPtr, &patter, 1, sizeof(uint32_t), nullptr, 1, &zeEvent, copyParams);

    EXPECT_EQ(device->getNEODevice()->getDefaultEngine().commandStreamReceiver->peekBarrierCount(), 0u);
}

HWTEST_F(CommandListTest, givenImmediateCommandListWhenAppendMemoryRangesBarrierUsingFlushTaskThenExpectCorrectExecuteCall) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    uint32_t numRanges = 1;
    const size_t rangeSizes = 1;
    const char *rangesBuffer[rangeSizes];
    const void **ranges = reinterpret_cast<const void **>(&rangesBuffer[0]);

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    cmdList.commandContainer.setImmediateCmdListCsr(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    result = cmdList.appendMemoryRangesBarrier(numRanges, &rangeSizes,
                                               ranges, nullptr, 0,
                                               nullptr);
    EXPECT_EQ(1u, cmdList.executeCommandListImmediateWithFlushTaskCalledCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST_F(CommandListTest, givenImmediateCommandListWhenFlushImmediateThenOverrideEventCsr) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    queue->isCopyOnlyCommandQueue = true;

    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    cmdList.commandContainer.setImmediateCmdListCsr(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    cmdList.cmdQImmediate = queue.get();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    MockCommandStreamReceiver mockCommandStreamReceiver(*neoDevice->executionEnvironment, neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    auto event = std::unique_ptr<Event>(static_cast<Event *>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device)));

    event->csrs[0] = &mockCommandStreamReceiver;
    cmdList.flushImmediate(ZE_RESULT_SUCCESS, false, false, false, NEO::AppendOperations::nonKernel, false, event->toHandle(), false, nullptr, nullptr);
    EXPECT_EQ(event->csrs[0], cmdList.getCsr(false));
}

HWTEST_F(CommandListTest, givenRegularCmdListWhenAskingForRelaxedOrderingThenReturnFalse) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    EXPECT_FALSE(commandList->isRelaxedOrderingDispatchAllowed(5, false));
}

HWTEST_F(CommandListTest, givenComputeCommandListAnd2dRegionWhenMemoryCopyRegionInExternalHostAllocationCalledThenBuiltinFlagAndDestinationAllocSystemIsSet) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 0, 2, 2, 1};
    ze_copy_region_t srcRegion = {4, 4, 0, 2, 2, 1};
    commandList->appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);
}

HWTEST_F(CommandListTest, givenComputeCommandListAnd2dRegionWhenMemoryCopyRegionInUsmHostAllocationCalledThenBuiltinFlagAndDestinationAllocSystemIsSet) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    constexpr size_t allocSize = 4096;
    void *dstBuffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocHostMem(&hostDesc, allocSize, allocSize, &dstBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    ze_copy_region_t dstRegion = {4, 4, 0, 2, 2, 1};
    ze_copy_region_t srcRegion = {4, 4, 0, 2, 2, 1};
    commandList->appendMemoryCopyRegion(dstBuffer, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

HWTEST_F(CommandListTest, givenComputeCommandListAnd2dRegionWhenMemoryCopyRegionInUsmDeviceAllocationCalledThenBuiltinFlagIsSetAndDestinationAllocSystemFlagNotSet) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    constexpr size_t size = 4096u;
    constexpr size_t alignment = 4096u;
    void *dstBuffer = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(),
                                          &deviceDesc,
                                          size, alignment, &dstBuffer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    ze_copy_region_t dstRegion = {4, 4, 0, 2, 2, 1};
    ze_copy_region_t srcRegion = {4, 4, 0, 2, 2, 1};
    commandList->appendMemoryCopyRegion(dstBuffer, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

HWTEST_F(CommandListTest, givenComputeCommandListAndRegionMemoryCopyFrom2dSourceImageto3dDestImageThenBuiltinFlagIsSetAndDestinationAllocSystemFlagNotSet) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    constexpr size_t size = 4096u;
    constexpr size_t alignment = 4096u;
    void *dstBuffer = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(),
                                          &deviceDesc,
                                          size, alignment, &dstBuffer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    ze_copy_region_t dstRegion = {4, 4, 1, 2, 2, 1};
    ze_copy_region_t srcRegion = {4, 4, 0, 2, 2, 1};
    commandList->appendMemoryCopyRegion(dstBuffer, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

HWTEST_F(CommandListTest, givenComputeCommandListAndRegionMemoryCopyFrom3dSourceImageto2dDestImageThenBuiltinFlagIsSetAndDestinationAllocSystemFlagNotSet) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    constexpr size_t size = 4096u;
    constexpr size_t alignment = 4096u;
    void *dstBuffer = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(),
                                          &deviceDesc,
                                          size, alignment, &dstBuffer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    ze_copy_region_t dstRegion = {4, 4, 0, 2, 2, 1};
    ze_copy_region_t srcRegion = {4, 4, 1, 2, 2, 1};
    commandList->appendMemoryCopyRegion(dstBuffer, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

HWTEST_F(CommandListTest, givenComputeCommandListAnd3dRegionWhenMemoryCopyRegionInExternalHostAllocationCalledThenBuiltinAndDestinationAllocSystemFlagIsSet) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    commandList->appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);
}

HWTEST_F(CommandListTest, givenComputeCommandListAnd3dRegionWhenMemoryCopyRegionInUsmHostAllocationCalledThenBuiltinAndDestinationAllocSystemFlagIsSet) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    constexpr size_t allocSize = 4096;
    void *dstBuffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocHostMem(&hostDesc, allocSize, allocSize, &dstBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    commandList->appendMemoryCopyRegion(dstBuffer, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

HWTEST_F(CommandListTest, givenComputeCommandListAnd3dRegionWhenMemoryCopyRegionInUsmDeviceAllocationCalledThenBuiltinFlagIsSetAndDestinationAllocSystemFlagNotSet) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    constexpr size_t size = 4096u;
    constexpr size_t alignment = 4096u;
    void *dstBuffer = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(),
                                          &deviceDesc,
                                          size, alignment, &dstBuffer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    commandList->appendMemoryCopyRegion(dstBuffer, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

using ImageSupport = IsNotAnyGfxCores<IGFX_XE_HPC_CORE>;

HWTEST2_F(CommandListTest, givenComputeCommandListWhenCopyFromImageToImageTheBuiltinFlagIsSet, ImageSupport) {
    auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::copyImageRegion);
    auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
    mockBuiltinKernel->setArgRedescribedImageCallBase = false;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto imageHwSrc = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto imageHwDst = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    imageHwSrc->initialize(device, &zeDesc);
    imageHwDst->initialize(device, &zeDesc);

    ze_image_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    ze_image_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    CmdListMemoryCopyParams copyParams = {};
    commandList->appendImageCopyRegion(imageHwDst->toHandle(), imageHwSrc->toHandle(), &dstRegion, &srcRegion, nullptr, 0, nullptr, copyParams);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenCopyFromImageToExternalHostMemoryThenBuiltinFlagAndDestinationAllocSystemIsSet, ImageSupport) {
    auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::copyImageRegion);
    auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
    mockBuiltinKernel->setArgRedescribedImageCallBase = false;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    void *dstPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto imageHw = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    imageHw->initialize(device, &zeDesc);
    CmdListMemoryCopyParams copyParams = {};
    ze_image_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    commandList->appendImageCopyToMemory(dstPtr, imageHw->toHandle(), &srcRegion, nullptr, 0, nullptr, copyParams);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenCopyFromImageToUsmHostMemoryThenBuiltinFlagAndDestinationAllocSystemIsSet, ImageSupport) {
    auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::copyImageRegion);
    auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
    mockBuiltinKernel->setArgRedescribedImageCallBase = false;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    constexpr size_t allocSize = 4096;
    void *dstBuffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocHostMem(&hostDesc, allocSize, allocSize, &dstBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto imageHw = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    imageHw->initialize(device, &zeDesc);
    CmdListMemoryCopyParams copyParams = {};
    ze_image_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    commandList->appendImageCopyToMemory(dstBuffer, imageHw->toHandle(), &srcRegion, nullptr, 0, nullptr, copyParams);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenCopyFromImageToUsmDeviceMemoryThenBuiltinFlagIsSetAndDestinationAllocSystemNotSet, ImageSupport) {
    auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::copyImageRegion);
    auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
    mockBuiltinKernel->setArgRedescribedImageCallBase = false;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    constexpr size_t size = 4096u;
    constexpr size_t alignment = 4096u;
    void *dstBuffer = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(),
                                          &deviceDesc,
                                          size, alignment, &dstBuffer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto imageHw = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    imageHw->initialize(device, &zeDesc);

    CmdListMemoryCopyParams copyParams = {};
    ze_image_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    commandList->appendImageCopyToMemory(dstBuffer, imageHw->toHandle(), &srcRegion, nullptr, 0, nullptr, copyParams);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenImageCopyFromMemoryThenBuiltinFlagIsSet, ImageSupport) {
    auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::copyImageRegion);
    auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
    mockBuiltinKernel->setArgRedescribedImageCallBase = false;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.type = ZE_IMAGE_TYPE_3D;
    zeDesc.height = 2;
    zeDesc.depth = 2;
    auto imageHw = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    imageHw->initialize(device, &zeDesc);

    commandList->appendImageCopyFromMemory(imageHw->toHandle(), srcPtr, nullptr, nullptr, 0, nullptr, copyParams);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
}

struct HeaplessSupportedMatch {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::heaplessAllowed;
    }
};

HWTEST2_F(CommandListTest, givenHeaplessWhenAppendImageCopyFromMemoryThenCorrectRowAndSlicePitchArePassed, HeaplessSupportedMatch) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    for (bool heaplessEnabled : {false, true}) {
        ImageBuiltin func = heaplessEnabled ? ImageBuiltin::copyBufferToImage3dBytesHeapless : ImageBuiltin::copyBufferToImage3dBytes;
        auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(func);
        auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
        mockBuiltinKernel->checkPassedArgumentValues = true;
        mockBuiltinKernel->setArgRedescribedImageCallBase = false;
        mockBuiltinKernel->passedArgumentValues.clear();
        mockBuiltinKernel->passedArgumentValues.resize(5);

        auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
        commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
        commandList->heaplessModeEnabled = heaplessEnabled;
        commandList->scratchAddressPatchingEnabled = true;

        void *srcPtr = reinterpret_cast<void *>(0x1234);

        ze_image_desc_t zeDesc = {};
        zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
        zeDesc.type = ZE_IMAGE_TYPE_3D;
        zeDesc.width = 4;
        zeDesc.height = 2;
        zeDesc.depth = 2;

        ze_image_region_t dstImgRegion = {2, 1, 1, 4, 2, 2};

        auto imageHw = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
        imageHw->initialize(device, &zeDesc);
        auto bytesPerPixel = static_cast<uint32_t>(imageHw->getImageInfo().surfaceFormat->imageElementSizeInBytes);

        commandList->appendImageCopyFromMemory(imageHw->toHandle(), srcPtr, &dstImgRegion, nullptr, 0, nullptr, copyParams);
        EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);

        auto passedArgSizeRowSlicePitch = mockBuiltinKernel->passedArgumentValues[4u].size();
        auto *passedArgRowSlicePitch = mockBuiltinKernel->passedArgumentValues[4u].data();

        if (heaplessEnabled) {
            EXPECT_EQ(sizeof(uint64_t) * 2, passedArgSizeRowSlicePitch);
            uint64_t expectedPitch[] = {dstImgRegion.width * bytesPerPixel, dstImgRegion.height * (dstImgRegion.width * bytesPerPixel)};
            EXPECT_EQ(0, memcmp(passedArgRowSlicePitch, expectedPitch, passedArgSizeRowSlicePitch));
        } else {
            EXPECT_EQ(sizeof(uint32_t) * 2, passedArgSizeRowSlicePitch);
            uint32_t expectedPitch[] = {dstImgRegion.width * bytesPerPixel, dstImgRegion.height * (dstImgRegion.width * bytesPerPixel)};
            EXPECT_EQ(0, memcmp(passedArgRowSlicePitch, expectedPitch, passedArgSizeRowSlicePitch));
        }
    }
}

HWTEST2_F(CommandListTest, givenHeaplessWhenAppendImageCopyToMemoryThenCorrectRowAndSlicePitchArePassed, HeaplessSupportedMatch) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    for (bool heaplessEnabled : {false, true}) {
        ImageBuiltin func = heaplessEnabled ? ImageBuiltin::copyImage3dToBufferBytesHeapless : ImageBuiltin::copyImage3dToBufferBytes;

        auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(func);
        auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);

        mockBuiltinKernel->checkPassedArgumentValues = true;
        mockBuiltinKernel->setArgRedescribedImageCallBase = false;
        mockBuiltinKernel->passedArgumentValues.clear();
        mockBuiltinKernel->passedArgumentValues.resize(5);

        auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
        commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
        commandList->heaplessModeEnabled = heaplessEnabled;
        commandList->scratchAddressPatchingEnabled = true;

        void *dstPtr = reinterpret_cast<void *>(0x1234);

        ze_image_desc_t zeDesc = {};
        zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
        zeDesc.type = ZE_IMAGE_TYPE_3D;
        zeDesc.width = 4;
        zeDesc.height = 2;
        zeDesc.depth = 2;

        ze_image_region_t srcImgRegion = {2, 1, 1, 4, 2, 2};

        auto imageHw = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
        imageHw->initialize(device, &zeDesc);
        auto bytesPerPixel = static_cast<uint32_t>(imageHw->getImageInfo().surfaceFormat->imageElementSizeInBytes);
        CmdListMemoryCopyParams copyParams = {};
        commandList->appendImageCopyToMemory(dstPtr, imageHw->toHandle(), &srcImgRegion, nullptr, 0, nullptr, copyParams);
        EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);

        auto passedArgSizeRowSlicePitch = mockBuiltinKernel->passedArgumentValues[4u].size();
        auto *passedArgRowSlicePitch = mockBuiltinKernel->passedArgumentValues[4u].data();

        if (heaplessEnabled) {
            EXPECT_EQ(sizeof(uint64_t) * 2, passedArgSizeRowSlicePitch);
            uint64_t expectedPitch[] = {srcImgRegion.width * bytesPerPixel, srcImgRegion.height * (srcImgRegion.width * bytesPerPixel)};
            EXPECT_EQ(0, memcmp(passedArgRowSlicePitch, expectedPitch, passedArgSizeRowSlicePitch));
        } else {
            EXPECT_EQ(sizeof(uint32_t) * 2, passedArgSizeRowSlicePitch);
            uint32_t expectedPitch[] = {srcImgRegion.width * bytesPerPixel, srcImgRegion.height * (srcImgRegion.width * bytesPerPixel)};
            EXPECT_EQ(0, memcmp(passedArgRowSlicePitch, expectedPitch, passedArgSizeRowSlicePitch));
        }
    }
}

HWTEST_F(CommandListTest, givenComputeCommandListWhenMemoryCopyInExternalHostAllocationThenBuiltinFlagAndDestinationAllocSystemIsSet) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr, copyParams);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isKernelSplitOperation);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);
}

HWTEST_F(CommandListTest, givenComputeCommandListWhenMemoryCopyInUsmHostAllocationThenBuiltinFlagAndDestinationAllocSystemIsSet) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    constexpr size_t allocSize = 4096;
    void *dstBuffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocHostMem(&hostDesc, allocSize, allocSize, &dstBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    void *srcPtr = reinterpret_cast<void *>(0x1234);

    commandList->appendMemoryCopy(dstBuffer, srcPtr, 8, nullptr, 0, nullptr, copyParams);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isKernelSplitOperation);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

HWTEST_F(CommandListTest, givenComputeCommandListWhenMemoryCopyInUsmDeviceAllocationThenBuiltinFlagIsSetAndDestinationAllocSystemNotSet) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    constexpr size_t size = 4096u;
    constexpr size_t alignment = 4096u;
    void *dstBuffer = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(),
                                          &deviceDesc,
                                          size, alignment, &dstBuffer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    void *srcPtr = reinterpret_cast<void *>(0x1234);

    commandList->appendMemoryCopy(dstBuffer, srcPtr, 8, nullptr, 0, nullptr, copyParams);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isKernelSplitOperation);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

HWTEST_F(CommandListTest, givenComputeCommandListWhenMemoryCopyWithReservedDeviceAllocationThenResidencyContainerHasImplicitMappedAllocations) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    driverHandle->devices[0]->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();

    void *dstBuffer = nullptr;
    size_t size = MemoryConstants::pageSize64k;
    size_t reservationSize = size * 2;

    auto res = context->reserveVirtualMem(nullptr, reservationSize, &dstBuffer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    ze_physical_mem_desc_t desc = {};
    desc.size = size;
    ze_physical_mem_handle_t phPhysicalMemory;
    res = context->createPhysicalMem(device->toHandle(), &desc, &phPhysicalMemory);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    ze_physical_mem_handle_t phPhysicalMemory2;
    res = context->createPhysicalMem(device->toHandle(), &desc, &phPhysicalMemory2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = context->mapVirtualMem(dstBuffer, size, phPhysicalMemory, 0, ZE_MEMORY_ACCESS_ATTRIBUTE_READWRITE);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    void *offsetAddress = reinterpret_cast<void *>(reinterpret_cast<uint64_t>(dstBuffer) + size);
    res = context->mapVirtualMem(offsetAddress, size, phPhysicalMemory2, 0, ZE_MEMORY_ACCESS_ATTRIBUTE_READWRITE);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    void *srcPtr = reinterpret_cast<void *>(0x1234);

    commandList->appendMemoryCopy(dstBuffer, srcPtr, size, nullptr, 0, nullptr, copyParams);

    bool phys2Resident = false;
    for (auto alloc : commandList->getCmdContainer().getResidencyContainer()) {
        if (alloc && alloc->getGpuAddress() == reinterpret_cast<uint64_t>(offsetAddress)) {
            phys2Resident = true;
        }
    }

    EXPECT_TRUE(phys2Resident);
    res = context->unMapVirtualMem(dstBuffer, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = context->unMapVirtualMem(offsetAddress, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = context->freeVirtualMem(dstBuffer, reservationSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = context->destroyPhysicalMem(phPhysicalMemory);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = context->destroyPhysicalMem(phPhysicalMemory2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

HWTEST_F(CommandListTest, givenComputeCommandListWhenMemoryCopyWithOneReservedDeviceAllocationMappedToFullReservationThenExtendedBufferSizeIsZero) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    driverHandle->devices[0]->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();

    void *dstBuffer = nullptr;
    size_t size = MemoryConstants::pageSize64k;
    size_t reservationSize = size * 2;

    auto res = context->reserveVirtualMem(nullptr, reservationSize, &dstBuffer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    ze_physical_mem_desc_t desc = {};
    desc.size = reservationSize;
    ze_physical_mem_handle_t phPhysicalMemory;
    res = context->createPhysicalMem(device->toHandle(), &desc, &phPhysicalMemory);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = context->mapVirtualMem(dstBuffer, reservationSize, phPhysicalMemory, 0, ZE_MEMORY_ACCESS_ATTRIBUTE_READWRITE);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    void *srcPtr = reinterpret_cast<void *>(0x1234);

    commandList->appendMemoryCopy(dstBuffer, srcPtr, reservationSize, nullptr, 0, nullptr, copyParams);

    res = context->unMapVirtualMem(dstBuffer, reservationSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = context->freeVirtualMem(dstBuffer, reservationSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = context->destroyPhysicalMem(phPhysicalMemory);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

HWTEST2_F(CommandListTest, givenStatelessWhenAppendMemoryFillIsCalledThenCorrectBuiltinIsUsed, IsAtLeastXeHpcCore) {
    auto &compilerProductHelper = device->getCompilerProductHelper();
    ASSERT_TRUE(compilerProductHelper.isForceToStatelessRequired());

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    constexpr size_t allocSize = 4096;
    constexpr size_t patternSize = 8;
    uint8_t pattern[patternSize] = {1, 2, 3, 4};

    void *dstBuffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocHostMem(&hostDesc, allocSize, allocSize, &dstBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->appendMemoryFill(dstBuffer, pattern, patternSize, allocSize, nullptr, 0, nullptr, copyParams);

    bool isStateless = true;
    bool isHeapless = commandList->isHeaplessModeEnabled();
    auto builtin = BuiltinTypeHelper::adjustBuiltinType<Builtin::fillBufferMiddle>(isStateless, isHeapless);

    Kernel *expectedBuiltinKernel = device->getBuiltinFunctionsLib()->getFunction(builtin);

    EXPECT_EQ(expectedBuiltinKernel, commandList->kernelUsed);

    context->freeMem(dstBuffer);
}

HWTEST_F(CommandListTest, givenComputeCommandListWhenMemoryFillInUsmHostThenBuiltinFlagAndDestinationAllocSystemIsSet) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    constexpr size_t allocSize = 4096;
    constexpr size_t patternSize = 8;
    uint8_t pattern[patternSize] = {1, 2, 3, 4};

    void *dstBuffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocHostMem(&hostDesc, allocSize, allocSize, &dstBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->appendMemoryFill(dstBuffer, pattern, patternSize, allocSize, nullptr, 0, nullptr, copyParams);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isKernelSplitOperation);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    commandList->appendMemoryFill(dstBuffer, pattern, 1, allocSize, nullptr, 0, nullptr, copyParams);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isKernelSplitOperation);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

HWTEST_F(CommandListTest, givenComputeCommandListWhenMemoryFillInUsmDeviceThenBuiltinFlagIsSetAndDestinationAllocSystemNotSet) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    constexpr size_t patternSize = 8;
    uint8_t pattern[patternSize] = {1, 2, 3, 4};

    constexpr size_t size = 4096u;
    constexpr size_t alignment = 4096u;
    void *dstBuffer = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(),
                                          &deviceDesc,
                                          size, alignment, &dstBuffer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->appendMemoryFill(dstBuffer, pattern, patternSize, size, nullptr, 0, nullptr, copyParams);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isKernelSplitOperation);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    commandList->appendMemoryFill(dstBuffer, pattern, 1, size, nullptr, 0, nullptr, copyParams);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isKernelSplitOperation);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

HWTEST_F(CommandListTest, givenComputeCommandListWhenMemoryFillRequiresMultiKernelsThenSplitFlagIsSet) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    constexpr size_t patternSize = 8;
    uint8_t pattern[patternSize] = {1, 2, 3, 4};

    constexpr size_t size = 4096u;
    constexpr size_t alignment = 4096u;
    void *dstBuffer = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(),
                                          &deviceDesc,
                                          size, alignment, &dstBuffer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    constexpr size_t fillSize = size - 1;

    commandList->appendMemoryFill(dstBuffer, pattern, patternSize, fillSize, nullptr, 0, nullptr, copyParams);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isKernelSplitOperation);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    commandList->appendMemoryFill(dstBuffer, pattern, 1, fillSize, nullptr, 0, nullptr, copyParams);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isKernelSplitOperation);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

TEST(CommandList, whenAsMutableIsCalledNullptrIsReturned) {
    MockCommandList cmdList;
    EXPECT_EQ(nullptr, cmdList.asMutable());
}

TEST(CommandList, WhenConsumeTextureCacheFlushPendingThenReturnsCurrentValueAndClearsFlag) {
    MockCommandList cmdList;
    {
        cmdList.setTextureCacheFlushPending(false);
        EXPECT_FALSE(cmdList.consumeTextureCacheFlushPending());
        EXPECT_FALSE(cmdList.isTextureCacheFlushPending());
    }
    {
        cmdList.setTextureCacheFlushPending(true);
        EXPECT_TRUE(cmdList.consumeTextureCacheFlushPending());
        EXPECT_FALSE(cmdList.isTextureCacheFlushPending());
    }
}

class MockCommandQueueIndirectAccess : public Mock<CommandQueue> {
  public:
    MockCommandQueueIndirectAccess(L0::Device *device, NEO::CommandStreamReceiver *csr, const ze_command_queue_desc_t *desc) : Mock(device, csr, desc) {}
    void handleIndirectAllocationResidency(UnifiedMemoryControls unifiedMemoryControls, std::unique_lock<std::mutex> &lockForIndirect, bool performMigration) override {
        handleIndirectAllocationResidencyCalledTimes++;
    }
    uint32_t handleIndirectAllocationResidencyCalledTimes = 0;
};

HWTEST_F(CommandListTest, givenCmdListWithIndirectAccessWhenExecutingCommandListImmediateWithFlushTaskThenHandleIndirectAccessCalled) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    auto &commandListImmediate = static_cast<MockCommandListImmediate<FamilyType::gfxCoreFamily> &>(*commandList);

    MockCommandStreamReceiver mockCommandStreamReceiver(*neoDevice->executionEnvironment, neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());
    MockCommandQueueIndirectAccess mockCommandQueue(device, &mockCommandStreamReceiver, &desc);

    auto oldCommandQueue = commandListImmediate.cmdQImmediate;
    commandListImmediate.cmdQImmediate = &mockCommandQueue;
    commandListImmediate.indirectAllocationsAllowed = true;
    commandListImmediate.executeCommandListImmediateWithFlushTask(false, false, false, NEO::AppendOperations::nonKernel, false, false, nullptr, nullptr);
    EXPECT_EQ(mockCommandQueue.handleIndirectAllocationResidencyCalledTimes, 1u);
    commandListImmediate.cmdQImmediate = oldCommandQueue;
}

HWTEST_F(CommandListTest, givenRegularCmdListWithIndirectAccessWhenExecutingRegularOnImmediateCommandListThenHandleIndirectAccessCalled) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandListImmediate(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::compute, returnValue));
    auto &mockCommandListImmediate = static_cast<MockCommandListImmediate<FamilyType::gfxCoreFamily> &>(*commandListImmediate);

    MockCommandStreamReceiver mockCommandStreamReceiver(*neoDevice->executionEnvironment, neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());
    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<FamilyType::gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(false, false, true);

    auto oldCommandQueue = mockCommandListImmediate.cmdQImmediate;
    mockCommandListImmediate.cmdQImmediate = mockCmdQHw.get();

    std::unique_ptr<L0::CommandList> commandListRegular(CommandList::create(productFamily,
                                                                            device,
                                                                            NEO::EngineGroupType::compute,
                                                                            0u,
                                                                            returnValue, false));
    ASSERT_NE(nullptr, commandListRegular);
    auto &mockCommandListRegular = static_cast<CommandListCoreFamily<FamilyType::gfxCoreFamily> &>(*commandListRegular);
    mockCommandListRegular.indirectAllocationsAllowed = true;
    commandListRegular->close();

    auto cmdListHandle = commandListRegular->toHandle();
    returnValue = commandListImmediate->appendCommandLists(1, &cmdListHandle, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_EQ(mockCmdQHw->handleIndirectAllocationResidencyCalledTimes, 1u);
    mockCommandListImmediate.cmdQImmediate = oldCommandQueue;
}

HWTEST_F(CommandListTest, givenCmdListWithNoIndirectAccessWhenExecutingCommandListImmediateWithFlushTaskThenHandleIndirectAccessNotCalled) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    auto &commandListImmediate = static_cast<MockCommandListImmediate<FamilyType::gfxCoreFamily> &>(*commandList);

    MockCommandStreamReceiver mockCommandStreamReceiver(*neoDevice->executionEnvironment, neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());
    MockCommandQueueIndirectAccess mockCommandQueue(device, &mockCommandStreamReceiver, &desc);

    auto oldCommandQueue = commandListImmediate.cmdQImmediate;
    commandListImmediate.cmdQImmediate = &mockCommandQueue;
    commandListImmediate.indirectAllocationsAllowed = false;
    commandListImmediate.executeCommandListImmediateWithFlushTask(false, false, false, NEO::AppendOperations::nonKernel, false, false, nullptr, nullptr);
    EXPECT_EQ(mockCommandQueue.handleIndirectAllocationResidencyCalledTimes, 0u);
    commandListImmediate.cmdQImmediate = oldCommandQueue;
}

using ImmediateCmdListSharedHeapsTest = Test<ImmediateCmdListSharedHeapsFixture>;
HWTEST2_F(ImmediateCmdListSharedHeapsTest, givenMultipleCommandListsUsingSharedHeapsWhenDispatchingKernelThenExpectSingleSbaCommandAndHeapsReused, IsHeapfulSupported) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    auto bindlessHeapsHelper = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.get();
    auto &cmdContainer = commandListImmediate->commandContainer;

    EXPECT_TRUE(commandListImmediate->immediateCmdListHeapSharing);

    EXPECT_EQ(1u, cmdContainer.getNumIddPerBlock());
    EXPECT_TRUE(cmdContainer.immediateCmdListSharedHeap(HeapType::dynamicState));
    EXPECT_TRUE(cmdContainer.immediateCmdListSharedHeap(HeapType::surfaceState));

    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();

    if (ultCsr.heaplessStateInitialized) {
        GTEST_SKIP();
    }
    auto &csrStream = ultCsr.commandStream;

    const ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = ZE_RESULT_SUCCESS;

    auto csrDshHeap = &ultCsr.getIndirectHeap(HeapType::dynamicState, MemoryConstants::pageSize64k);
    auto csrSshHeap = &ultCsr.getIndirectHeap(HeapType::surfaceState, MemoryConstants::pageSize64k);

    size_t dshUsed = csrDshHeap->getUsed();
    size_t sshUsed = csrSshHeap->getUsed();

    size_t csrUsedBefore = csrStream.getUsed();
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t csrUsedAfter = csrStream.getUsed();

    NEO::IndirectHeap *containerDshHeap = cmdContainer.getIndirectHeap(HeapType::dynamicState);
    NEO::IndirectHeap *containerSshHeap = cmdContainer.getIndirectHeap(HeapType::surfaceState);

    if (this->dshRequired) {
        EXPECT_EQ(csrDshHeap, containerDshHeap);
    } else {
        EXPECT_EQ(nullptr, containerDshHeap);
    }
    EXPECT_EQ(csrSshHeap, containerSshHeap);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        (csrUsedAfter - csrUsedBefore)));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto &sbaCmd = *genCmdCast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);
    if (this->dshRequired) {
        EXPECT_TRUE(sbaCmd.getDynamicStateBaseAddressModifyEnable());
        if (bindlessHeapsHelper) {
            EXPECT_EQ(bindlessHeapsHelper->getGlobalHeapsBase(), sbaCmd.getDynamicStateBaseAddress());
        } else {
            EXPECT_EQ(csrDshHeap->getHeapGpuBase(), sbaCmd.getDynamicStateBaseAddress());
        }
    } else {
        EXPECT_FALSE(sbaCmd.getDynamicStateBaseAddressModifyEnable());
        EXPECT_EQ(0u, sbaCmd.getDynamicStateBaseAddress());
    }
    EXPECT_TRUE(sbaCmd.getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(csrSshHeap->getHeapGpuBase(), sbaCmd.getSurfaceStateBaseAddress());

    dshUsed = csrDshHeap->getUsed() - dshUsed;
    sshUsed = csrSshHeap->getUsed() - sshUsed;
    if (this->dshRequired) {
        EXPECT_LT(0u, dshUsed);
    } else {
        EXPECT_EQ(0u, dshUsed);
    }
    EXPECT_LT(0u, sshUsed);

    size_t dshEstimated = NEO::EncodeDispatchKernel<FamilyType>::getSizeRequiredDsh(kernel->getKernelDescriptor(), cmdContainer.getNumIddPerBlock());
    size_t sshEstimated = NEO::EncodeDispatchKernel<FamilyType>::getSizeRequiredSsh(*kernel->getImmutableData()->getKernelInfo());

    EXPECT_GE(dshEstimated, dshUsed);
    EXPECT_GE(sshEstimated, sshUsed);

    auto &cmdContainerCoexisting = commandListImmediateCoexisting->commandContainer;
    EXPECT_EQ(1u, cmdContainerCoexisting.getNumIddPerBlock());
    EXPECT_TRUE(cmdContainerCoexisting.immediateCmdListSharedHeap(HeapType::dynamicState));
    EXPECT_TRUE(cmdContainerCoexisting.immediateCmdListSharedHeap(HeapType::surfaceState));

    dshUsed = csrDshHeap->getUsed();
    sshUsed = csrSshHeap->getUsed();

    csrUsedBefore = csrStream.getUsed();
    result = commandListImmediateCoexisting->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    csrUsedAfter = csrStream.getUsed();

    auto containerDshHeapCoexisting = cmdContainerCoexisting.getIndirectHeap(HeapType::dynamicState);
    auto containerSshHeapCoexisting = cmdContainerCoexisting.getIndirectHeap(HeapType::surfaceState);

    size_t dshAlignment = NEO::EncodeDispatchKernel<FamilyType>::getDefaultDshAlignment();
    size_t sshAlignment = NEO::EncodeDispatchKernel<FamilyType>::getDefaultSshAlignment();

    void *ptr = containerSshHeapCoexisting->getSpace(0);
    size_t expectedSshAlignedSize = sshEstimated + ptrDiff(alignUp(ptr, sshAlignment), ptr);

    size_t expectedDshAlignedSize = dshEstimated;
    if (this->dshRequired) {
        ptr = containerDshHeapCoexisting->getSpace(0);
        expectedDshAlignedSize += ptrDiff(alignUp(ptr, dshAlignment), ptr);

        EXPECT_EQ(csrDshHeap, containerDshHeapCoexisting);
    } else {
        EXPECT_EQ(nullptr, containerDshHeapCoexisting);
    }
    EXPECT_EQ(csrSshHeap, containerSshHeapCoexisting);

    cmdList.clear();
    sbaCmds.clear();

    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        (csrUsedAfter - csrUsedBefore)));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(0u, sbaCmds.size());

    dshUsed = csrDshHeap->getUsed() - dshUsed;
    sshUsed = csrSshHeap->getUsed() - sshUsed;

    if (this->dshRequired) {
        EXPECT_LT(0u, dshUsed);
    } else {
        EXPECT_EQ(0u, dshUsed);
    }
    EXPECT_LT(0u, sshUsed);

    EXPECT_GE(expectedDshAlignedSize, dshUsed);
    EXPECT_GE(expectedSshAlignedSize, sshUsed);
}

using CommandListStateBaseAddressGlobalStatelessTest = Test<CommandListGlobalHeapsFixture<static_cast<int32_t>(NEO::HeapAddressModel::globalStateless)>>;
HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest, givenGlobalStatelessWhenExecutingCommandListThenMakeAllocationResident, IsHeapfulSupportedAndAtLeastXeCore) {

    EXPECT_EQ(NEO::HeapAddressModel::globalStateless, commandList->cmdListHeapAddressModel);
    EXPECT_EQ(NEO::HeapAddressModel::globalStateless, commandListImmediate->cmdListHeapAddressModel);
    EXPECT_EQ(NEO::HeapAddressModel::globalStateless, commandQueue->cmdListHeapAddressModel);

    ASSERT_EQ(commandListImmediate->getCsr(false), commandQueue->getCsr());
    auto globalStatelessAlloc = commandListImmediate->getCsr(false)->getGlobalStatelessHeapAllocation();
    EXPECT_NE(nullptr, globalStatelessAlloc);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(commandListImmediate->getCsr(false));
    ultCsr->storeMakeResidentAllocations = true;

    commandList->close();

    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    auto result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(ultCsr->isMadeResident(globalStatelessAlloc));
}

HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest,
          givenGlobalStatelessWhenExecutingRegularCommandListThenBaseAddressPropertiesSetCorrectlyAndCommandProperlyDispatched,
          IsWithinXeCoreAndXe3Core) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    commandList->heaplessModeEnabled = false;
    commandQueue->heaplessModeEnabled = false;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &container = commandList->getCmdContainer();

    auto statlessMocs = getMocs(true);
    auto ioBaseAddress = container.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapGpuBase();
    auto ioSize = container.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapSizeInPages();

    auto &requiredState = commandList->requiredStreamState.stateBaseAddress;
    auto &finalState = commandList->finalStreamState.stateBaseAddress;

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), requiredState.statelessMocs.value);

    EXPECT_EQ(-1, requiredState.surfaceStateBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), requiredState.surfaceStateSize.value);
    EXPECT_EQ(-1, requiredState.dynamicStateBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), requiredState.dynamicStateSize.value);

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), requiredState.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, requiredState.indirectObjectSize.value);

    EXPECT_EQ(-1, requiredState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), requiredState.bindingTablePoolSize.value);

    EXPECT_EQ(finalState.surfaceStateBaseAddress.value, requiredState.surfaceStateBaseAddress.value);
    EXPECT_EQ(finalState.surfaceStateSize.value, requiredState.surfaceStateSize.value);

    EXPECT_EQ(finalState.dynamicStateBaseAddress.value, requiredState.dynamicStateBaseAddress.value);
    EXPECT_EQ(finalState.dynamicStateSize.value, requiredState.dynamicStateSize.value);

    EXPECT_EQ(finalState.indirectObjectBaseAddress.value, requiredState.indirectObjectBaseAddress.value);
    EXPECT_EQ(finalState.indirectObjectSize.value, requiredState.indirectObjectSize.value);

    EXPECT_EQ(finalState.bindingTablePoolBaseAddress.value, requiredState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(finalState.bindingTablePoolSize.value, requiredState.bindingTablePoolSize.value);

    EXPECT_EQ(finalState.statelessMocs.value, requiredState.statelessMocs.value);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    size_t queueBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueAfter = cmdQueueStream.getUsed();

    auto globalSurfaceHeap = commandQueue->getCsr()->getGlobalStatelessHeap();

    auto ssBaseAddress = globalSurfaceHeap->getHeapGpuBase();
    auto ssSize = globalSurfaceHeap->getHeapSizeInPages();

    auto &csrState = commandQueue->getCsr()->getStreamProperties().stateBaseAddress;

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddress), csrState.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSize, csrState.surfaceStateSize.value);

    EXPECT_EQ(-1, csrState.dynamicStateBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), csrState.dynamicStateSize.value);

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), csrState.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, csrState.indirectObjectSize.value);

    EXPECT_EQ(-1, csrState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), csrState.bindingTablePoolSize.value);

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), csrState.statelessMocs.value);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
    EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
    EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
    EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddress, sbaCmd->getSurfaceStateBaseAddress());

    EXPECT_TRUE(sbaCmd->getGeneralStateBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd->getGeneralStateBufferSizeModifyEnable());

    auto ioBaseAddressDecanonized = neoDevice->getGmmHelper()->decanonize(ioBaseAddress);
    EXPECT_EQ(ioBaseAddressDecanonized, sbaCmd->getGeneralStateBaseAddress());
    EXPECT_EQ(ioSize, sbaCmd->getGeneralStateBufferSize());

    EXPECT_EQ((statlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());
}

HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest,
          givenGlobalStatelessWhenExecutingImmediateCommandListThenBaseAddressPropertiesSetCorrectlyAndCommandProperlyDispatched,
          IsHeapfulSupportedAndAtLeastXeCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    auto &csrImmediate = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = csrImmediate.commandStream;
    auto &csrState = csrImmediate.getStreamProperties().stateBaseAddress;
    auto globalSurfaceHeap = csrImmediate.getGlobalStatelessHeap();

    size_t csrUsedBefore = csrStream.getUsed();
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t csrUsedAfter = csrStream.getUsed();

    auto &container = commandListImmediate->getCmdContainer();
    auto statlessMocs = getMocs(true);
    auto ioBaseAddress = container.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapGpuBase();
    auto ioSize = container.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapSizeInPages();

    auto ssBaseAddress = globalSurfaceHeap->getHeapGpuBase();
    auto ssSize = globalSurfaceHeap->getHeapSizeInPages();

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddress), csrState.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSize, csrState.surfaceStateSize.value);

    EXPECT_EQ(-1, csrState.dynamicStateBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), csrState.dynamicStateSize.value);

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), csrState.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, csrState.indirectObjectSize.value);

    EXPECT_EQ(-1, csrState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), csrState.bindingTablePoolSize.value);

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), csrState.statelessMocs.value);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        csrUsedAfter - csrUsedBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
    EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
    EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
    EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddress, sbaCmd->getSurfaceStateBaseAddress());

    EXPECT_TRUE(sbaCmd->getGeneralStateBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd->getGeneralStateBufferSizeModifyEnable());

    auto ioBaseAddressDecanonized = neoDevice->getGmmHelper()->decanonize(ioBaseAddress);
    EXPECT_EQ(ioBaseAddressDecanonized, sbaCmd->getGeneralStateBaseAddress());
    EXPECT_EQ(ioSize, sbaCmd->getGeneralStateBufferSize());

    EXPECT_EQ((statlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());
}

HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest,
          givenGlobalStatelessWhenExecutingRegularCommandListAndImmediateCommandListThenBaseAddressPropertiesSetCorrectlyAndCommandProperlyDispatchedOnlyOnce,
          IsWithinXeCoreAndXe3Core) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    commandQueue->heaplessModeEnabled = false;
    commandList->heaplessModeEnabled = false;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &container = commandList->getCmdContainer();

    auto statlessMocs = getMocs(true);
    auto ioBaseAddress = container.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapGpuBase();
    auto ioSize = container.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapSizeInPages();

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    size_t queueBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueAfter = cmdQueueStream.getUsed();

    auto globalSurfaceHeap = commandQueue->getCsr()->getGlobalStatelessHeap();

    auto ssBaseAddress = globalSurfaceHeap->getHeapGpuBase();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
    EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
    EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
    EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddress, sbaCmd->getSurfaceStateBaseAddress());

    EXPECT_TRUE(sbaCmd->getGeneralStateBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd->getGeneralStateBufferSizeModifyEnable());

    auto ioBaseAddressDecanonized = neoDevice->getGmmHelper()->decanonize(ioBaseAddress);
    EXPECT_EQ(ioBaseAddressDecanonized, sbaCmd->getGeneralStateBaseAddress());
    EXPECT_EQ(ioSize, sbaCmd->getGeneralStateBufferSize());

    EXPECT_EQ((statlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());

    auto &csrImmediate = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = csrImmediate.commandStream;

    size_t csrUsedBefore = csrStream.getUsed();
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t csrUsedAfter = csrStream.getUsed();

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        csrUsedAfter - csrUsedBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(0u, sbaCmds.size());
}

HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest,
          givenGlobalStatelessWhenExecutingImmediateCommandListAndRegularCommandListThenBaseAddressPropertiesSetCorrectlyAndCommandProperlyDispatchedOnlyOnce,
          IsHeapfulSupportedAndAtLeastXeCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    auto &csrImmediate = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = csrImmediate.commandStream;
    auto globalSurfaceHeap = csrImmediate.getGlobalStatelessHeap();

    size_t csrUsedBefore = csrStream.getUsed();
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t csrUsedAfter = csrStream.getUsed();

    auto &container = commandListImmediate->getCmdContainer();
    auto statlessMocs = getMocs(true);
    auto ioBaseAddress = container.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapGpuBase();
    auto ioSize = container.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapSizeInPages();

    auto ssBaseAddress = globalSurfaceHeap->getHeapGpuBase();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        csrUsedAfter - csrUsedBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
    EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
    EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
    EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddress, sbaCmd->getSurfaceStateBaseAddress());

    EXPECT_TRUE(sbaCmd->getGeneralStateBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd->getGeneralStateBufferSizeModifyEnable());

    auto ioBaseAddressDecanonized = neoDevice->getGmmHelper()->decanonize(ioBaseAddress);
    EXPECT_EQ(ioBaseAddressDecanonized, sbaCmd->getGeneralStateBaseAddress());
    EXPECT_EQ(ioSize, sbaCmd->getGeneralStateBufferSize());

    EXPECT_EQ((statlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());

    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    size_t queueBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueAfter = cmdQueueStream.getUsed();

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(0u, sbaCmds.size());
}

HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest,
          givenGlobalStatelessWhenExecutingRegularCommandListAndPrivateHeapsCommandListThenBaseAddressPropertiesSetCorrectlyAndCommandProperlyDispatched,
          IsWithinXeCoreAndXe3Core) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    commandQueue->heaplessModeEnabled = false;
    commandList->heaplessModeEnabled = false;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &container = commandList->getCmdContainer();

    auto statlessMocs = getMocs(true);
    auto ioBaseAddress = container.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapGpuBase();
    auto ioSize = container.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapSizeInPages();

    auto &requiredState = commandList->requiredStreamState.stateBaseAddress;
    auto &finalState = commandList->finalStreamState.stateBaseAddress;

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), requiredState.statelessMocs.value);

    EXPECT_EQ(-1, requiredState.surfaceStateBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), requiredState.surfaceStateSize.value);
    EXPECT_EQ(-1, requiredState.dynamicStateBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), requiredState.dynamicStateSize.value);

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), requiredState.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, requiredState.indirectObjectSize.value);

    EXPECT_EQ(-1, requiredState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), requiredState.bindingTablePoolSize.value);

    EXPECT_EQ(finalState.surfaceStateBaseAddress.value, requiredState.surfaceStateBaseAddress.value);
    EXPECT_EQ(finalState.surfaceStateSize.value, requiredState.surfaceStateSize.value);

    EXPECT_EQ(finalState.dynamicStateBaseAddress.value, requiredState.dynamicStateBaseAddress.value);
    EXPECT_EQ(finalState.dynamicStateSize.value, requiredState.dynamicStateSize.value);

    EXPECT_EQ(finalState.indirectObjectBaseAddress.value, requiredState.indirectObjectBaseAddress.value);
    EXPECT_EQ(finalState.indirectObjectSize.value, requiredState.indirectObjectSize.value);

    EXPECT_EQ(finalState.bindingTablePoolBaseAddress.value, requiredState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(finalState.bindingTablePoolSize.value, requiredState.bindingTablePoolSize.value);

    EXPECT_EQ(finalState.statelessMocs.value, requiredState.statelessMocs.value);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    size_t queueBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueAfter = cmdQueueStream.getUsed();

    auto globalSurfaceHeap = commandQueue->getCsr()->getGlobalStatelessHeap();

    auto ssBaseAddress = globalSurfaceHeap->getHeapGpuBase();
    auto ssSize = globalSurfaceHeap->getHeapSizeInPages();

    auto &csrState = commandQueue->getCsr()->getStreamProperties().stateBaseAddress;

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddress), csrState.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSize, csrState.surfaceStateSize.value);

    EXPECT_EQ(-1, csrState.dynamicStateBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), csrState.dynamicStateSize.value);

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), csrState.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, csrState.indirectObjectSize.value);

    EXPECT_EQ(-1, csrState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), csrState.bindingTablePoolSize.value);

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), csrState.statelessMocs.value);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
    EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
    EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
    EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddress, sbaCmd->getSurfaceStateBaseAddress());

    EXPECT_TRUE(sbaCmd->getGeneralStateBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd->getGeneralStateBufferSizeModifyEnable());
    auto ioBaseAddressDecanonized = neoDevice->getGmmHelper()->decanonize(ioBaseAddress);
    EXPECT_EQ(ioBaseAddressDecanonized, sbaCmd->getGeneralStateBaseAddress());
    EXPECT_EQ(ioSize, sbaCmd->getGeneralStateBufferSize());

    EXPECT_EQ((statlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());

    result = commandListPrivateHeap->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &containerPrivateHeap = commandListPrivateHeap->getCmdContainer();

    auto sshPrivateHeap = containerPrivateHeap.getIndirectHeap(NEO::HeapType::surfaceState);
    auto ssBaseAddressPrivateHeap = sshPrivateHeap->getHeapGpuBase();
    auto ssSizePrivateHeap = sshPrivateHeap->getHeapSizeInPages();

    uint64_t dsBaseAddressPrivateHeap = -1;
    size_t dsSizePrivateHeap = static_cast<size_t>(-1);

    auto dshPrivateHeap = containerPrivateHeap.getIndirectHeap(NEO::HeapType::dynamicState);
    if (!this->dshRequired) {
        EXPECT_EQ(nullptr, dshPrivateHeap);
    } else {
        EXPECT_NE(nullptr, dshPrivateHeap);
    }
    if (dshPrivateHeap) {
        dsBaseAddressPrivateHeap = dshPrivateHeap->getHeapGpuBase();
        dsSizePrivateHeap = dshPrivateHeap->getHeapSizeInPages();
    }

    auto &requiredStatePrivateHeap = commandListPrivateHeap->requiredStreamState.stateBaseAddress;
    auto &finalStatePrivateHeap = commandListPrivateHeap->finalStreamState.stateBaseAddress;

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), requiredStatePrivateHeap.statelessMocs.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), requiredStatePrivateHeap.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, requiredStatePrivateHeap.surfaceStateSize.value);
    EXPECT_EQ(static_cast<int64_t>(dsBaseAddressPrivateHeap), requiredStatePrivateHeap.dynamicStateBaseAddress.value);
    EXPECT_EQ(dsSizePrivateHeap, requiredStatePrivateHeap.dynamicStateSize.value);

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), requiredStatePrivateHeap.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, requiredStatePrivateHeap.indirectObjectSize.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), requiredStatePrivateHeap.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, requiredStatePrivateHeap.bindingTablePoolSize.value);

    EXPECT_EQ(finalStatePrivateHeap.surfaceStateBaseAddress.value, requiredStatePrivateHeap.surfaceStateBaseAddress.value);
    EXPECT_EQ(finalStatePrivateHeap.surfaceStateSize.value, requiredStatePrivateHeap.surfaceStateSize.value);

    EXPECT_EQ(finalStatePrivateHeap.dynamicStateBaseAddress.value, requiredStatePrivateHeap.dynamicStateBaseAddress.value);
    EXPECT_EQ(finalStatePrivateHeap.dynamicStateSize.value, requiredStatePrivateHeap.dynamicStateSize.value);

    EXPECT_EQ(finalStatePrivateHeap.indirectObjectBaseAddress.value, requiredStatePrivateHeap.indirectObjectBaseAddress.value);
    EXPECT_EQ(finalStatePrivateHeap.indirectObjectSize.value, requiredStatePrivateHeap.indirectObjectSize.value);

    EXPECT_EQ(finalStatePrivateHeap.bindingTablePoolBaseAddress.value, requiredStatePrivateHeap.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(finalStatePrivateHeap.bindingTablePoolSize.value, requiredStatePrivateHeap.bindingTablePoolSize.value);

    EXPECT_EQ(finalStatePrivateHeap.statelessMocs.value, requiredStatePrivateHeap.statelessMocs.value);

    result = commandListPrivateHeap->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    queueBefore = cmdQueueStream.getUsed();
    cmdListHandle = commandListPrivateHeap->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    queueAfter = cmdQueueStream.getUsed();

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), csrState.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, csrState.surfaceStateSize.value);

    if (dshPrivateHeap) {
        EXPECT_EQ(static_cast<int64_t>(dsBaseAddressPrivateHeap), csrState.dynamicStateBaseAddress.value);
        EXPECT_EQ(dsSizePrivateHeap, csrState.dynamicStateSize.value);
    } else {
        EXPECT_EQ(-1, csrState.dynamicStateBaseAddress.value);
        EXPECT_EQ(static_cast<size_t>(-1), csrState.dynamicStateSize.value);
    }

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), csrState.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, csrState.indirectObjectSize.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), csrState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, csrState.bindingTablePoolSize.value);

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), csrState.statelessMocs.value);

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    if (dshPrivateHeap) {
        EXPECT_TRUE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_TRUE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(dsBaseAddressPrivateHeap, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(dsSizePrivateHeap, sbaCmd->getDynamicStateBufferSize());
    } else {
        EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());
    }

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddressPrivateHeap, sbaCmd->getSurfaceStateBaseAddress());

    EXPECT_TRUE(sbaCmd->getGeneralStateBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd->getGeneralStateBufferSizeModifyEnable());
    EXPECT_EQ(ioBaseAddressDecanonized, sbaCmd->getGeneralStateBaseAddress());
    EXPECT_EQ(ioSize, sbaCmd->getGeneralStateBufferSize());

    EXPECT_EQ((statlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());
}

HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest,
          givenGlobalStatelessWhenExecutingPrivateHeapsCommandListAndRegularCommandListThenBaseAddressPropertiesSetCorrectlyAndCommandProperlyDispatched,
          IsHeapfulSupportedAndAtLeastXeCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    auto result = commandListPrivateHeap->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &containerPrivateHeap = commandListPrivateHeap->getCmdContainer();

    auto statlessMocs = getMocs(true);
    auto ioBaseAddress = containerPrivateHeap.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapGpuBase();
    auto ioSize = containerPrivateHeap.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapSizeInPages();

    auto sshPrivateHeap = containerPrivateHeap.getIndirectHeap(NEO::HeapType::surfaceState);
    auto ssBaseAddressPrivateHeap = sshPrivateHeap->getHeapGpuBase();
    auto ssSizePrivateHeap = sshPrivateHeap->getHeapSizeInPages();

    uint64_t dsBaseAddressPrivateHeap = -1;
    size_t dsSizePrivateHeap = static_cast<size_t>(-1);

    auto dshPrivateHeap = containerPrivateHeap.getIndirectHeap(NEO::HeapType::dynamicState);
    if (!this->dshRequired) {
        EXPECT_EQ(nullptr, dshPrivateHeap);
    } else {
        EXPECT_NE(nullptr, dshPrivateHeap);
    }
    if (dshPrivateHeap) {
        dsBaseAddressPrivateHeap = dshPrivateHeap->getHeapGpuBase();
        dsSizePrivateHeap = dshPrivateHeap->getHeapSizeInPages();
    }

    auto &requiredStatePrivateHeap = commandListPrivateHeap->requiredStreamState.stateBaseAddress;
    auto &finalStatePrivateHeap = commandListPrivateHeap->finalStreamState.stateBaseAddress;

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), requiredStatePrivateHeap.statelessMocs.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), requiredStatePrivateHeap.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, requiredStatePrivateHeap.surfaceStateSize.value);
    EXPECT_EQ(static_cast<int64_t>(dsBaseAddressPrivateHeap), requiredStatePrivateHeap.dynamicStateBaseAddress.value);
    EXPECT_EQ(dsSizePrivateHeap, requiredStatePrivateHeap.dynamicStateSize.value);

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), requiredStatePrivateHeap.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, requiredStatePrivateHeap.indirectObjectSize.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), requiredStatePrivateHeap.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, requiredStatePrivateHeap.bindingTablePoolSize.value);

    EXPECT_EQ(finalStatePrivateHeap.surfaceStateBaseAddress.value, requiredStatePrivateHeap.surfaceStateBaseAddress.value);
    EXPECT_EQ(finalStatePrivateHeap.surfaceStateSize.value, requiredStatePrivateHeap.surfaceStateSize.value);

    EXPECT_EQ(finalStatePrivateHeap.dynamicStateBaseAddress.value, requiredStatePrivateHeap.dynamicStateBaseAddress.value);
    EXPECT_EQ(finalStatePrivateHeap.dynamicStateSize.value, requiredStatePrivateHeap.dynamicStateSize.value);

    EXPECT_EQ(finalStatePrivateHeap.indirectObjectBaseAddress.value, requiredStatePrivateHeap.indirectObjectBaseAddress.value);
    EXPECT_EQ(finalStatePrivateHeap.indirectObjectSize.value, requiredStatePrivateHeap.indirectObjectSize.value);

    EXPECT_EQ(finalStatePrivateHeap.bindingTablePoolBaseAddress.value, requiredStatePrivateHeap.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(finalStatePrivateHeap.bindingTablePoolSize.value, requiredStatePrivateHeap.bindingTablePoolSize.value);

    EXPECT_EQ(finalStatePrivateHeap.statelessMocs.value, requiredStatePrivateHeap.statelessMocs.value);

    result = commandListPrivateHeap->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    auto &csrState = commandQueue->getCsr()->getStreamProperties().stateBaseAddress;

    size_t queueBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandListPrivateHeap->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueAfter = cmdQueueStream.getUsed();

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), csrState.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, csrState.surfaceStateSize.value);

    if (dshPrivateHeap) {
        EXPECT_EQ(static_cast<int64_t>(dsBaseAddressPrivateHeap), csrState.dynamicStateBaseAddress.value);
        EXPECT_EQ(dsSizePrivateHeap, csrState.dynamicStateSize.value);
    } else {
        EXPECT_EQ(-1, csrState.dynamicStateBaseAddress.value);
        EXPECT_EQ(static_cast<size_t>(-1), csrState.dynamicStateSize.value);
    }

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), csrState.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, csrState.indirectObjectSize.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), csrState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, csrState.bindingTablePoolSize.value);

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), csrState.statelessMocs.value);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    if (dshPrivateHeap) {
        EXPECT_TRUE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_TRUE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(dsBaseAddressPrivateHeap, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(dsSizePrivateHeap, sbaCmd->getDynamicStateBufferSize());
    } else {
        EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());
    }

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddressPrivateHeap, sbaCmd->getSurfaceStateBaseAddress());

    EXPECT_TRUE(sbaCmd->getGeneralStateBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd->getGeneralStateBufferSizeModifyEnable());
    auto ioBaseAddressDecanonized = neoDevice->getGmmHelper()->decanonize(ioBaseAddress);
    EXPECT_EQ(ioBaseAddressDecanonized, sbaCmd->getGeneralStateBaseAddress());
    EXPECT_EQ(ioSize, sbaCmd->getGeneralStateBufferSize());

    EXPECT_EQ((statlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());

    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &requiredState = commandList->requiredStreamState.stateBaseAddress;
    auto &finalState = commandList->finalStreamState.stateBaseAddress;

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), requiredState.statelessMocs.value);

    EXPECT_EQ(-1, requiredState.surfaceStateBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), requiredState.surfaceStateSize.value);
    EXPECT_EQ(-1, requiredState.dynamicStateBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), requiredState.dynamicStateSize.value);

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), requiredState.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, requiredState.indirectObjectSize.value);

    EXPECT_EQ(-1, requiredState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), requiredState.bindingTablePoolSize.value);

    EXPECT_EQ(finalState.surfaceStateBaseAddress.value, requiredState.surfaceStateBaseAddress.value);
    EXPECT_EQ(finalState.surfaceStateSize.value, requiredState.surfaceStateSize.value);

    EXPECT_EQ(finalState.dynamicStateBaseAddress.value, requiredState.dynamicStateBaseAddress.value);
    EXPECT_EQ(finalState.dynamicStateSize.value, requiredState.dynamicStateSize.value);

    EXPECT_EQ(finalState.indirectObjectBaseAddress.value, requiredState.indirectObjectBaseAddress.value);
    EXPECT_EQ(finalState.indirectObjectSize.value, requiredState.indirectObjectSize.value);

    EXPECT_EQ(finalState.bindingTablePoolBaseAddress.value, requiredState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(finalState.bindingTablePoolSize.value, requiredState.bindingTablePoolSize.value);

    EXPECT_EQ(finalState.statelessMocs.value, requiredState.statelessMocs.value);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    queueBefore = cmdQueueStream.getUsed();
    cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    queueAfter = cmdQueueStream.getUsed();

    auto globalSurfaceHeap = commandQueue->getCsr()->getGlobalStatelessHeap();

    auto ssBaseAddress = globalSurfaceHeap->getHeapGpuBase();
    auto ssSize = globalSurfaceHeap->getHeapSizeInPages();

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddress), csrState.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSize, csrState.surfaceStateSize.value);

    if (dshPrivateHeap) {
        EXPECT_EQ(static_cast<int64_t>(dsBaseAddressPrivateHeap), csrState.dynamicStateBaseAddress.value);
        EXPECT_EQ(dsSizePrivateHeap, csrState.dynamicStateSize.value);
    } else {
        EXPECT_EQ(-1, csrState.dynamicStateBaseAddress.value);
        EXPECT_EQ(static_cast<size_t>(-1), csrState.dynamicStateSize.value);
    }

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), csrState.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, csrState.indirectObjectSize.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), csrState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, csrState.bindingTablePoolSize.value);

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), csrState.statelessMocs.value);

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    if (dshPrivateHeap) {
        EXPECT_TRUE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_TRUE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(dsBaseAddressPrivateHeap, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(dsSizePrivateHeap, sbaCmd->getDynamicStateBufferSize());
    } else {
        EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());
    }

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddress, sbaCmd->getSurfaceStateBaseAddress());

    EXPECT_TRUE(sbaCmd->getGeneralStateBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd->getGeneralStateBufferSizeModifyEnable());
    EXPECT_EQ(ioBaseAddressDecanonized, sbaCmd->getGeneralStateBaseAddress());
    EXPECT_EQ(ioSize, sbaCmd->getGeneralStateBufferSize());

    EXPECT_EQ((statlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());
}

HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest,
          givenGlobalStatelessWhenExecutingImmediateCommandListAndPrivateHeapsCommandListThenBaseAddressPropertiesSetCorrectlyAndCommandProperlyDispatched,
          IsHeapfulSupportedAndAtLeastXeCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    auto &csrImmediate = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = csrImmediate.commandStream;
    auto &csrState = csrImmediate.getStreamProperties().stateBaseAddress;
    auto globalSurfaceHeap = csrImmediate.getGlobalStatelessHeap();

    size_t csrUsedBefore = csrStream.getUsed();
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t csrUsedAfter = csrStream.getUsed();

    auto &container = commandListImmediate->getCmdContainer();
    auto statlessMocs = getMocs(true);
    auto ioBaseAddress = container.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapGpuBase();
    auto ioSize = container.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapSizeInPages();

    auto ssBaseAddress = globalSurfaceHeap->getHeapGpuBase();
    auto ssSize = globalSurfaceHeap->getHeapSizeInPages();

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddress), csrState.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSize, csrState.surfaceStateSize.value);

    EXPECT_EQ(-1, csrState.dynamicStateBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), csrState.dynamicStateSize.value);

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), csrState.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, csrState.indirectObjectSize.value);

    EXPECT_EQ(-1, csrState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), csrState.bindingTablePoolSize.value);

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), csrState.statelessMocs.value);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        csrUsedAfter - csrUsedBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
    EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
    EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
    EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddress, sbaCmd->getSurfaceStateBaseAddress());

    EXPECT_TRUE(sbaCmd->getGeneralStateBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd->getGeneralStateBufferSizeModifyEnable());

    auto ioBaseAddressDecanonized = neoDevice->getGmmHelper()->decanonize(ioBaseAddress);
    EXPECT_EQ(ioBaseAddressDecanonized, sbaCmd->getGeneralStateBaseAddress());
    EXPECT_EQ(ioSize, sbaCmd->getGeneralStateBufferSize());

    EXPECT_EQ((statlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());

    result = commandListPrivateHeap->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &containerPrivateHeap = commandListPrivateHeap->getCmdContainer();

    auto sshPrivateHeap = containerPrivateHeap.getIndirectHeap(NEO::HeapType::surfaceState);
    auto ssBaseAddressPrivateHeap = sshPrivateHeap->getHeapGpuBase();
    auto ssSizePrivateHeap = sshPrivateHeap->getHeapSizeInPages();

    uint64_t dsBaseAddressPrivateHeap = -1;
    size_t dsSizePrivateHeap = static_cast<size_t>(-1);

    auto dshPrivateHeap = containerPrivateHeap.getIndirectHeap(NEO::HeapType::dynamicState);
    if (!this->dshRequired) {
        EXPECT_EQ(nullptr, dshPrivateHeap);
    } else {
        EXPECT_NE(nullptr, dshPrivateHeap);
    }
    if (dshPrivateHeap) {
        dsBaseAddressPrivateHeap = dshPrivateHeap->getHeapGpuBase();
        dsSizePrivateHeap = dshPrivateHeap->getHeapSizeInPages();
    }

    auto &requiredStatePrivateHeap = commandListPrivateHeap->requiredStreamState.stateBaseAddress;
    auto &finalStatePrivateHeap = commandListPrivateHeap->finalStreamState.stateBaseAddress;

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), requiredStatePrivateHeap.statelessMocs.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), requiredStatePrivateHeap.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, requiredStatePrivateHeap.surfaceStateSize.value);
    EXPECT_EQ(static_cast<int64_t>(dsBaseAddressPrivateHeap), requiredStatePrivateHeap.dynamicStateBaseAddress.value);
    EXPECT_EQ(dsSizePrivateHeap, requiredStatePrivateHeap.dynamicStateSize.value);

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), requiredStatePrivateHeap.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, requiredStatePrivateHeap.indirectObjectSize.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), requiredStatePrivateHeap.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, requiredStatePrivateHeap.bindingTablePoolSize.value);

    EXPECT_EQ(finalStatePrivateHeap.surfaceStateBaseAddress.value, requiredStatePrivateHeap.surfaceStateBaseAddress.value);
    EXPECT_EQ(finalStatePrivateHeap.surfaceStateSize.value, requiredStatePrivateHeap.surfaceStateSize.value);

    EXPECT_EQ(finalStatePrivateHeap.dynamicStateBaseAddress.value, requiredStatePrivateHeap.dynamicStateBaseAddress.value);
    EXPECT_EQ(finalStatePrivateHeap.dynamicStateSize.value, requiredStatePrivateHeap.dynamicStateSize.value);

    EXPECT_EQ(finalStatePrivateHeap.indirectObjectBaseAddress.value, requiredStatePrivateHeap.indirectObjectBaseAddress.value);
    EXPECT_EQ(finalStatePrivateHeap.indirectObjectSize.value, requiredStatePrivateHeap.indirectObjectSize.value);

    EXPECT_EQ(finalStatePrivateHeap.bindingTablePoolBaseAddress.value, requiredStatePrivateHeap.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(finalStatePrivateHeap.bindingTablePoolSize.value, requiredStatePrivateHeap.bindingTablePoolSize.value);

    EXPECT_EQ(finalStatePrivateHeap.statelessMocs.value, requiredStatePrivateHeap.statelessMocs.value);

    result = commandListPrivateHeap->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    size_t queueBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandListPrivateHeap->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueAfter = cmdQueueStream.getUsed();

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), csrState.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, csrState.surfaceStateSize.value);

    if (dshPrivateHeap) {
        EXPECT_EQ(static_cast<int64_t>(dsBaseAddressPrivateHeap), csrState.dynamicStateBaseAddress.value);
        EXPECT_EQ(dsSizePrivateHeap, csrState.dynamicStateSize.value);
    } else {
        EXPECT_EQ(-1, csrState.dynamicStateBaseAddress.value);
        EXPECT_EQ(static_cast<size_t>(-1), csrState.dynamicStateSize.value);
    }

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), csrState.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, csrState.indirectObjectSize.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), csrState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, csrState.bindingTablePoolSize.value);

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), csrState.statelessMocs.value);

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    if (dshPrivateHeap) {
        EXPECT_TRUE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_TRUE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(dsBaseAddressPrivateHeap, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(dsSizePrivateHeap, sbaCmd->getDynamicStateBufferSize());
    } else {
        EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());
    }

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddressPrivateHeap, sbaCmd->getSurfaceStateBaseAddress());

    EXPECT_TRUE(sbaCmd->getGeneralStateBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd->getGeneralStateBufferSizeModifyEnable());
    EXPECT_EQ(ioBaseAddressDecanonized, sbaCmd->getGeneralStateBaseAddress());
    EXPECT_EQ(ioSize, sbaCmd->getGeneralStateBufferSize());

    EXPECT_EQ((statlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());
}

HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest,
          givenGlobalStatelessWhenExecutingPrivateHeapsCommandListAndImmediateCommandListThenBaseAddressPropertiesSetCorrectlyAndCommandProperlyDispatched,
          IsHeapfulSupportedAndAtLeastXeCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    auto result = commandListPrivateHeap->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &containerPrivateHeap = commandListPrivateHeap->getCmdContainer();

    auto statlessMocs = getMocs(true);
    auto ioBaseAddress = containerPrivateHeap.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapGpuBase();
    auto ioSize = containerPrivateHeap.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapSizeInPages();

    auto sshPrivateHeap = containerPrivateHeap.getIndirectHeap(NEO::HeapType::surfaceState);
    auto ssBaseAddressPrivateHeap = sshPrivateHeap->getHeapGpuBase();
    auto ssSizePrivateHeap = sshPrivateHeap->getHeapSizeInPages();

    uint64_t dsBaseAddressPrivateHeap = -1;
    size_t dsSizePrivateHeap = static_cast<size_t>(-1);

    auto dshPrivateHeap = containerPrivateHeap.getIndirectHeap(NEO::HeapType::dynamicState);
    if (!this->dshRequired) {
        EXPECT_EQ(nullptr, dshPrivateHeap);
    } else {
        EXPECT_NE(nullptr, dshPrivateHeap);
    }
    if (dshPrivateHeap) {
        dsBaseAddressPrivateHeap = dshPrivateHeap->getHeapGpuBase();
        dsSizePrivateHeap = dshPrivateHeap->getHeapSizeInPages();
    }

    auto &requiredStatePrivateHeap = commandListPrivateHeap->requiredStreamState.stateBaseAddress;
    auto &finalStatePrivateHeap = commandListPrivateHeap->finalStreamState.stateBaseAddress;

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), requiredStatePrivateHeap.statelessMocs.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), requiredStatePrivateHeap.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, requiredStatePrivateHeap.surfaceStateSize.value);
    EXPECT_EQ(static_cast<int64_t>(dsBaseAddressPrivateHeap), requiredStatePrivateHeap.dynamicStateBaseAddress.value);
    EXPECT_EQ(dsSizePrivateHeap, requiredStatePrivateHeap.dynamicStateSize.value);

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), requiredStatePrivateHeap.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, requiredStatePrivateHeap.indirectObjectSize.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), requiredStatePrivateHeap.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, requiredStatePrivateHeap.bindingTablePoolSize.value);

    EXPECT_EQ(finalStatePrivateHeap.surfaceStateBaseAddress.value, requiredStatePrivateHeap.surfaceStateBaseAddress.value);
    EXPECT_EQ(finalStatePrivateHeap.surfaceStateSize.value, requiredStatePrivateHeap.surfaceStateSize.value);

    EXPECT_EQ(finalStatePrivateHeap.dynamicStateBaseAddress.value, requiredStatePrivateHeap.dynamicStateBaseAddress.value);
    EXPECT_EQ(finalStatePrivateHeap.dynamicStateSize.value, requiredStatePrivateHeap.dynamicStateSize.value);

    EXPECT_EQ(finalStatePrivateHeap.indirectObjectBaseAddress.value, requiredStatePrivateHeap.indirectObjectBaseAddress.value);
    EXPECT_EQ(finalStatePrivateHeap.indirectObjectSize.value, requiredStatePrivateHeap.indirectObjectSize.value);

    EXPECT_EQ(finalStatePrivateHeap.bindingTablePoolBaseAddress.value, requiredStatePrivateHeap.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(finalStatePrivateHeap.bindingTablePoolSize.value, requiredStatePrivateHeap.bindingTablePoolSize.value);

    EXPECT_EQ(finalStatePrivateHeap.statelessMocs.value, requiredStatePrivateHeap.statelessMocs.value);

    result = commandListPrivateHeap->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    auto &csrState = commandQueue->getCsr()->getStreamProperties().stateBaseAddress;

    size_t queueBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandListPrivateHeap->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueAfter = cmdQueueStream.getUsed();

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), csrState.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, csrState.surfaceStateSize.value);

    if (dshPrivateHeap) {
        EXPECT_EQ(static_cast<int64_t>(dsBaseAddressPrivateHeap), csrState.dynamicStateBaseAddress.value);
        EXPECT_EQ(dsSizePrivateHeap, csrState.dynamicStateSize.value);
    } else {
        EXPECT_EQ(-1, csrState.dynamicStateBaseAddress.value);
        EXPECT_EQ(static_cast<size_t>(-1), csrState.dynamicStateSize.value);
    }

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), csrState.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, csrState.indirectObjectSize.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), csrState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, csrState.bindingTablePoolSize.value);

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), csrState.statelessMocs.value);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    if (dshPrivateHeap) {
        EXPECT_TRUE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_TRUE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(dsBaseAddressPrivateHeap, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(dsSizePrivateHeap, sbaCmd->getDynamicStateBufferSize());
    } else {
        EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());
    }

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddressPrivateHeap, sbaCmd->getSurfaceStateBaseAddress());

    EXPECT_TRUE(sbaCmd->getGeneralStateBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd->getGeneralStateBufferSizeModifyEnable());
    auto ioBaseAddressDecanonized = neoDevice->getGmmHelper()->decanonize(ioBaseAddress);
    EXPECT_EQ(ioBaseAddressDecanonized, sbaCmd->getGeneralStateBaseAddress());
    EXPECT_EQ(ioSize, sbaCmd->getGeneralStateBufferSize());

    EXPECT_EQ((statlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());

    auto &csrImmediate = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = csrImmediate.commandStream;
    auto globalSurfaceHeap = csrImmediate.getGlobalStatelessHeap();

    size_t csrUsedBefore = csrStream.getUsed();
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t csrUsedAfter = csrStream.getUsed();

    auto ssBaseAddress = globalSurfaceHeap->getHeapGpuBase();
    auto ssSize = globalSurfaceHeap->getHeapSizeInPages();

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddress), csrState.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSize, csrState.surfaceStateSize.value);

    if (dshPrivateHeap) {
        EXPECT_EQ(static_cast<int64_t>(dsBaseAddressPrivateHeap), csrState.dynamicStateBaseAddress.value);
        EXPECT_EQ(dsSizePrivateHeap, csrState.dynamicStateSize.value);
    } else {
        EXPECT_EQ(-1, csrState.dynamicStateBaseAddress.value);
        EXPECT_EQ(static_cast<size_t>(-1), csrState.dynamicStateSize.value);
    }

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), csrState.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, csrState.indirectObjectSize.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), csrState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, csrState.bindingTablePoolSize.value);

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), csrState.statelessMocs.value);

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        csrUsedAfter - csrUsedBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
    EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
    EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
    EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddress, sbaCmd->getSurfaceStateBaseAddress());

    EXPECT_TRUE(sbaCmd->getGeneralStateBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd->getGeneralStateBufferSizeModifyEnable());
    EXPECT_EQ(ioBaseAddressDecanonized, sbaCmd->getGeneralStateBaseAddress());
    EXPECT_EQ(ioSize, sbaCmd->getGeneralStateBufferSize());

    EXPECT_EQ((statlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());
}

HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest,
          givenGlobalStatelessKernelUsingScratchSpaceWhenExecutingRegularCommandListThenBaseAddressAndFrontEndStateCommandsProperlyDispatched,
          IsHeapfulSupportedAndAtLeastXeCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using CFE_STATE = typename FamilyType::CFE_STATE;
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    mockKernelImmData->kernelDescriptor->kernelAttributes.perThreadScratchSize[0] = 0x100;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    size_t queueBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueAfter = cmdQueueStream.getUsed();

    auto globalSurfaceHeap = commandQueue->getCsr()->getGlobalStatelessHeap();

    auto ssBaseAddress = globalSurfaceHeap->getHeapGpuBase();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddress, sbaCmd->getSurfaceStateBaseAddress());

    auto frontEndCmds = findAll<CFE_STATE *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(1u, frontEndCmds.size());

    constexpr size_t expectedScratchOffset = 2 * sizeof(RENDER_SURFACE_STATE);

    auto frontEndCmd = reinterpret_cast<CFE_STATE *>(*frontEndCmds[0]);
    EXPECT_EQ(expectedScratchOffset, frontEndCmd->getScratchSpaceBuffer());

    auto scratchSpaceController = commandQueue->csr->getScratchSpaceController();
    EXPECT_EQ(expectedScratchOffset, scratchSpaceController->getScratchPatchAddress());

    auto surfaceStateHeapAlloc = globalSurfaceHeap->getGraphicsAllocation();
    void *scratchSurfaceStateBuffer = ptrOffset(surfaceStateHeapAlloc->getUnderlyingBuffer(), expectedScratchOffset);
    auto scratchSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(scratchSurfaceStateBuffer);

    auto scratchAllocation = scratchSpaceController->getScratchSpaceSlot0Allocation();
    EXPECT_EQ(scratchAllocation->getGpuAddress(), scratchSurfaceState->getSurfaceBaseAddress());
}

HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest,
          givenGlobalStatelessKernelUsingScratchSpaceWhenExecutingImmediateCommandListThenBaseAddressAndFrontEndStateCommandsProperlyDispatched,
          IsHeapfulSupportedAndAtLeastXeCore) {

    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using CFE_STATE = typename FamilyType::CFE_STATE;
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    mockKernelImmData->kernelDescriptor->kernelAttributes.perThreadScratchSize[0] = 0x100;

    auto &csrImmediate = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csrImmediate.heaplessModeEnabled = false;
    auto &csrStream = csrImmediate.commandStream;
    auto globalSurfaceHeap = csrImmediate.getGlobalStatelessHeap();

    size_t csrUsedBefore = csrStream.getUsed();
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t csrUsedAfter = csrStream.getUsed();

    auto ssBaseAddress = globalSurfaceHeap->getHeapGpuBase();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        csrUsedAfter - csrUsedBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddress, sbaCmd->getSurfaceStateBaseAddress());

    auto frontEndCmds = findAll<CFE_STATE *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(1u, frontEndCmds.size());

    constexpr size_t expectedScratchOffset = 2 * sizeof(RENDER_SURFACE_STATE);

    auto frontEndCmd = reinterpret_cast<CFE_STATE *>(*frontEndCmds[0]);
    EXPECT_EQ(expectedScratchOffset, frontEndCmd->getScratchSpaceBuffer());

    auto scratchSpaceController = commandQueue->csr->getScratchSpaceController();
    EXPECT_EQ(expectedScratchOffset, scratchSpaceController->getScratchPatchAddress());

    auto surfaceStateHeapAlloc = globalSurfaceHeap->getGraphicsAllocation();
    void *scratchSurfaceStateBuffer = ptrOffset(surfaceStateHeapAlloc->getUnderlyingBuffer(), expectedScratchOffset);
    auto scratchSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(scratchSurfaceStateBuffer);

    auto scratchAllocation = scratchSpaceController->getScratchSpaceSlot0Allocation();
    EXPECT_EQ(scratchAllocation->getGpuAddress(), scratchSurfaceState->getSurfaceBaseAddress());
}

HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest,
          givenCommandListNotUsingPrivateSurfaceHeapWhenCommandListDestroyedThenCsrDoesNotDispatchStateCacheFlush,
          IsHeapfulSupportedAndAtLeastXeCore) {
    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = csr.commandStream;

    ze_result_t returnValue;
    L0::ult::CommandList *cmdListObject = CommandList::whiteboxCast(CommandList::create(productFamily, device, engineGroupType, 0u, returnValue, false));

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    cmdListObject->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    returnValue = cmdListObject->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto cmdListHandle = cmdListObject->toHandle();
    auto usedBefore = csrStream.getUsed();

    returnValue = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    returnValue = cmdListObject->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_EQ(usedBefore, csrStream.getUsed());
}

HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest,
          givenCommandListUsingGlobalHeapsWhenCommandListCreatedThenNoStateHeapAllocationsCreated,
          IsHeapfulSupportedAndAtLeastXeCore) {
    auto &container = commandList->getCmdContainer();

    auto ssh = container.getIndirectHeap(NEO::HeapType::surfaceState);
    EXPECT_EQ(nullptr, ssh);

    auto dsh = container.getIndirectHeap(NEO::HeapType::dynamicState);
    EXPECT_EQ(nullptr, dsh);
}
HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest,
          givenUserKernelCreatedByIgcUsingStatefulAccessWhenAppendingKernelOnGlobalStatelessThenExpectError,
          IsHeapfulSupportedAndAtLeastXeCore) {
    module->translationUnit->isGeneratedByIgc = true;
    module->type = ModuleType::user;
    mockKernelImmData->kernelDescriptor->payloadMappings.explicitArgs.resize(1);

    auto ptrArg = ArgDescriptor(ArgDescriptor::argTPointer);
    ptrArg.as<ArgDescPointer>().bindless = 0x40;
    mockKernelImmData->kernelDescriptor->payloadMappings.explicitArgs[0] = ptrArg;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    ptrArg.as<ArgDescPointer>().bindless = undefined<CrossThreadDataOffset>;
    ptrArg.as<ArgDescPointer>().bindful = 0x40;
    mockKernelImmData->kernelDescriptor->payloadMappings.explicitArgs[0] = ptrArg;

    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest,
          givenUserKernelNotCreatedByIgcUsingStatefulAccessWhenAppendingKernelOnGlobalStatelessThenExpectSuccess,
          IsHeapfulSupportedAndAtLeastXeCore) {
    module->translationUnit->isGeneratedByIgc = false;
    module->type = ModuleType::user;
    mockKernelImmData->kernelDescriptor->payloadMappings.explicitArgs.resize(1);

    auto ptrArg = ArgDescriptor(ArgDescriptor::argTPointer);
    ptrArg.as<ArgDescPointer>().bindless = 0x40;
    mockKernelImmData->kernelDescriptor->payloadMappings.explicitArgs[0] = ptrArg;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ptrArg.as<ArgDescPointer>().bindless = undefined<CrossThreadDataOffset>;
    ptrArg.as<ArgDescPointer>().bindful = 0x40;
    mockKernelImmData->kernelDescriptor->payloadMappings.explicitArgs[0] = ptrArg;

    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest,
          givenBuiltinKernelCreatedByIgcUsingStatefulAccessWhenAppendingKernelOnGlobalStatelessThenExpectSuccess,
          IsHeapfulSupportedAndAtLeastXeCore) {
    module->translationUnit->isGeneratedByIgc = true;
    module->type = ModuleType::builtin;
    mockKernelImmData->kernelDescriptor->payloadMappings.explicitArgs.resize(1);

    auto ptrArg = ArgDescriptor(ArgDescriptor::argTPointer);
    ptrArg.as<ArgDescPointer>().bindless = 0x40;
    mockKernelImmData->kernelDescriptor->payloadMappings.explicitArgs[0] = ptrArg;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ptrArg.as<ArgDescPointer>().bindless = undefined<CrossThreadDataOffset>;
    ptrArg.as<ArgDescPointer>().bindful = 0x40;
    mockKernelImmData->kernelDescriptor->payloadMappings.explicitArgs[0] = ptrArg;

    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest,
          givenUserKernelCreatedByIgcUsingStatelessAccessWhenAppendingKernelOnGlobalStatelessThenExpectSuccess,
          IsHeapfulSupportedAndAtLeastXeCore) {
    module->translationUnit->isGeneratedByIgc = true;
    module->type = ModuleType::user;
    mockKernelImmData->kernelDescriptor->payloadMappings.explicitArgs.resize(1);

    auto ptrArg = ArgDescriptor(ArgDescriptor::argTPointer);
    ptrArg.as<ArgDescPointer>().bindless = undefined<CrossThreadDataOffset>;
    ptrArg.as<ArgDescPointer>().bindful = undefined<CrossThreadDataOffset>;
    mockKernelImmData->kernelDescriptor->payloadMappings.explicitArgs[0] = ptrArg;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest,
          givenCommandListUsingGlobalHeapsWhenAppendingCopyKernelThenStatelessKernelUsedAndNoSurfaceHeapUsed,
          IsHeapfulSupportedAndAtLeastXeCore) {
    auto &container = commandList->getCmdContainer();

    auto ssh = container.getIndirectHeap(NEO::HeapType::surfaceState);
    EXPECT_EQ(nullptr, ssh);

    void *hostPtr = nullptr;
    void *devicePtr = nullptr;

    constexpr size_t size = 1;
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocHostMem(&hostDesc, size, 1u, &hostPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_device_mem_alloc_desc_t deviceDesc = {};
    result = context->allocDeviceMem(device->toHandle(), &deviceDesc, size, 1u, &devicePtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    CmdListMemoryCopyParams copyParams = {};
    result = commandList->appendMemoryCopy(devicePtr, hostPtr, size, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ssh = container.getIndirectHeap(NEO::HeapType::surfaceState);
    EXPECT_EQ(nullptr, ssh);

    context->freeMem(hostPtr);
    context->freeMem(devicePtr);
}

HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest,
          givenCommandListUsingGlobalHeapsWhenAppendingFillKernelThenStatelessKernelUsedAndNoSurfaceHeapUsed,
          IsHeapfulSupportedAndAtLeastXeCore) {
    auto &container = commandList->getCmdContainer();

    auto ssh = container.getIndirectHeap(NEO::HeapType::surfaceState);
    EXPECT_EQ(nullptr, ssh);

    char pattern = 1;
    void *patternPtr = &pattern;
    void *devicePtr = nullptr;

    constexpr size_t size = 128;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, size, 1u, &devicePtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    CmdListMemoryCopyParams copyParams = {};
    result = commandList->appendMemoryFill(devicePtr, patternPtr, sizeof(pattern), size, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ssh = container.getIndirectHeap(NEO::HeapType::surfaceState);
    EXPECT_EQ(nullptr, ssh);

    context->freeMem(devicePtr);
}

HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest,
          givenCommandListUsingGlobalHeapsWhenAppendingPageFaultCopyThenStatelessKernelUsedAndNoSurfaceHeapUsed,
          IsHeapfulSupportedAndAtLeastXeCore) {
    auto &container = commandList->getCmdContainer();

    auto ssh = container.getIndirectHeap(NEO::HeapType::surfaceState);
    EXPECT_EQ(nullptr, ssh);

    constexpr size_t size = 64;
    uint8_t *buffer[size];
    uint64_t gpuAddress = 0x1200;

    NEO::MockGraphicsAllocation mockSrcAllocation(buffer, gpuAddress, size);
    NEO::MockGraphicsAllocation mockDstAllocation(buffer, gpuAddress, size);

    auto result = commandList->appendPageFaultCopy(&mockDstAllocation, &mockSrcAllocation, size, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ssh = container.getIndirectHeap(NEO::HeapType::surfaceState);
    EXPECT_EQ(nullptr, ssh);
}

HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest,
          givenCommandQueueUsingGlobalStatelessWhenQueueInHeaplessModeThenUsingScratchControllerAndHeapAllocationFromPrimaryCsr,
          IsHeapfulSupportedAndAtLeastXeCore) {
    auto defaultCsr = neoDevice->getDefaultEngine().commandStreamReceiver;
    defaultCsr->createGlobalStatelessHeap();

    auto otherCsr = std::unique_ptr<UltCommandStreamReceiver<FamilyType>>(static_cast<UltCommandStreamReceiver<FamilyType> *>(createCommandStream(*device->getNEODevice()->getExecutionEnvironment(), 0, 1)));

    otherCsr->setupContext(*neoDevice->getDefaultEngine().osContext);
    otherCsr->initializeResources(false, neoDevice->getPreemptionMode());
    otherCsr->initializeTagAllocation();
    otherCsr->createGlobalFenceAllocation();
    otherCsr->createGlobalStatelessHeap();

    ze_command_queue_desc_t desc = {};
    auto otherCommandQueue = new MockCommandQueueHw<FamilyType::gfxCoreFamily>(device, otherCsr.get(), &desc);
    otherCommandQueue->initialize(false, false, false);
    otherCommandQueue->heaplessModeEnabled = true;

    commandList->close();
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();

    auto result = otherCommandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(otherCommandQueue->csr->getScratchSpaceController(), otherCommandQueue->recordedScratchController);
    EXPECT_EQ(otherCommandQueue->csr->getGlobalStatelessHeapAllocation(), otherCommandQueue->recordedGlobalStatelessAllocation);
    EXPECT_FALSE(otherCommandQueue->recordedLockScratchController);
    otherCommandQueue->destroy();
}

struct ContextGroupStateBaseAddressGlobalStatelessFixture : public CommandListGlobalHeapsFixture<static_cast<int32_t>(NEO::HeapAddressModel::globalStateless)> {
    using BaseClass = CommandListGlobalHeapsFixture<static_cast<int32_t>(NEO::HeapAddressModel::globalStateless)>;
    void setUp() {
        debugManager.flags.ContextGroupSize.set(5);
        BaseClass::setUpParams(static_cast<int32_t>(NEO::HeapAddressModel::globalStateless));
    }
    DebugManagerStateRestore restorer;
};

using ContextGroupStateBaseAddressGlobalStatelessTest = Test<ContextGroupStateBaseAddressGlobalStatelessFixture>;
HWTEST2_F(ContextGroupStateBaseAddressGlobalStatelessTest,
          givenContextGroupEnabledAndCommandQueueUsingGlobalStatelessWhenQueueInHeaplessModeThenUsingScratchControllerAndHeapAllocationFromPrimaryCsr,
          IsHeapfulSupportedAndAtLeastXeCore) {

    HardwareInfo hwInfo = *defaultHwInfo;
    if (hwInfo.capabilityTable.defaultEngineType != aub_stream::EngineType::ENGINE_CCS) {
        GTEST_SKIP();
    }

    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    auto neoDevice = std::unique_ptr<NEO::MockDevice>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo));

    auto defaultCsr = neoDevice->getDefaultEngine().commandStreamReceiver;
    defaultCsr->createGlobalStatelessHeap();

    NEO::EngineTypeUsage engineTypeUsage;
    engineTypeUsage.first = hwInfo.capabilityTable.defaultEngineType;
    engineTypeUsage.second = NEO::EngineUsage::regular;
    auto primaryCsr = neoDevice->getSecondaryEngineCsr(engineTypeUsage, 0, false)->commandStreamReceiver;
    EXPECT_EQ(nullptr, primaryCsr->getOsContext().getPrimaryContext());
    EXPECT_TRUE(primaryCsr->getOsContext().isPartOfContextGroup());

    auto secondaryCsr = neoDevice->getSecondaryEngineCsr(engineTypeUsage, 0, false)->commandStreamReceiver;

    ze_command_queue_desc_t desc = {};
    auto otherCommandQueue = new MockCommandQueueHw<FamilyType::gfxCoreFamily>(device, secondaryCsr, &desc);
    otherCommandQueue->initialize(false, false, false);
    otherCommandQueue->heaplessModeEnabled = true;
    ze_result_t returnValue;
    commandList.reset(CommandList::whiteboxCast(CommandList::create(productFamily, device, engineGroupType, 0u, returnValue, false)));
    commandList->close();
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();

    auto result = otherCommandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(primaryCsr->getScratchSpaceController(), otherCommandQueue->recordedScratchController);
    EXPECT_EQ(primaryCsr->getGlobalStatelessHeapAllocation(), otherCommandQueue->recordedGlobalStatelessAllocation);
    EXPECT_TRUE(otherCommandQueue->recordedLockScratchController);
    otherCommandQueue->destroy();
}

HWTEST2_F(ContextGroupStateBaseAddressGlobalStatelessTest,
          givenHeaplessModeAndContextGroupEnabledWhenExecutingImmCommandListThenScratchControllerAndHeapAllocationFromPrimaryCsrIsUsed,
          IsHeapfulSupportedAndAtLeastXeCore) {

    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    constexpr bool heaplessModeEnabled = FamilyType::template isHeaplessMode<DefaultWalkerType>();
    HardwareInfo hwInfo = *defaultHwInfo;
    if (!heaplessModeEnabled || hwInfo.capabilityTable.defaultEngineType != aub_stream::EngineType::ENGINE_CCS) {
        GTEST_SKIP();
    }

    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.flags.ftrRcsNode = false;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    auto neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);

    MockDeviceImp l0Device(neoDevice);
    l0Device.setDriverHandle(device->getDriverHandle());

    auto defaultCsr = neoDevice->getDefaultEngine().commandStreamReceiver;
    defaultCsr->createGlobalStatelessHeap();

    NEO::EngineTypeUsage engineTypeUsage;
    engineTypeUsage.first = hwInfo.capabilityTable.defaultEngineType;
    engineTypeUsage.second = NEO::EngineUsage::regular;
    auto primaryCsr = neoDevice->getSecondaryEngineCsr(engineTypeUsage, 0, false)->commandStreamReceiver;
    EXPECT_EQ(nullptr, primaryCsr->getOsContext().getPrimaryContext());
    EXPECT_TRUE(primaryCsr->getOsContext().isPartOfContextGroup());

    [[maybe_unused]] auto secondaryCsr = neoDevice->getSecondaryEngineCsr(engineTypeUsage, 0, false)->commandStreamReceiver;

    ze_command_queue_desc_t queueDesc{ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    queueDesc.ordinal = 0u;
    queueDesc.index = 0u;
    queueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;

    ze_result_t returnValue;
    commandListImmediate.reset(CommandList::whiteboxCast(CommandList::createImmediate(productFamily, &l0Device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue)));
    commandListImmediate->heaplessModeEnabled = true;

    const ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    mockKernelImmData->kernelDescriptor->kernelAttributes.perThreadScratchSize[0] = 0x40;

    EXPECT_EQ(nullptr, primaryCsr->getScratchSpaceController()->getScratchSpaceSlot0Allocation());

    returnValue = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_NE(nullptr, primaryCsr->getScratchSpaceController()->getScratchSpaceSlot0Allocation());
    EXPECT_NE(primaryCsr, commandListImmediate->getCsr(false));
    commandListImmediate.reset();
}

HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest, givenGlobalStatelessAndHeaplessModeWhenExecutingCommandListThenMakeAllocationResident, IsHeapfulSupportedAndAtLeastXeCore) {
    EXPECT_EQ(NEO::HeapAddressModel::globalStateless, commandList->cmdListHeapAddressModel);
    EXPECT_EQ(NEO::HeapAddressModel::globalStateless, commandListImmediate->cmdListHeapAddressModel);
    EXPECT_EQ(NEO::HeapAddressModel::globalStateless, commandQueue->cmdListHeapAddressModel);

    commandQueue->heaplessModeEnabled = true;

    ASSERT_EQ(commandListImmediate->getCsr(false), commandQueue->getCsr());
    auto globalStatelessAlloc = commandListImmediate->getCsr(false)->getGlobalStatelessHeapAllocation();
    EXPECT_NE(nullptr, globalStatelessAlloc);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(commandListImmediate->getCsr(false));
    ultCsr->storeMakeResidentAllocations = true;

    commandList->close();

    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    auto result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(ultCsr->isMadeResident(globalStatelessAlloc));
}

} // namespace ult
} // namespace L0
