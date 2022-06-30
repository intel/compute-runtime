/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_image.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"

namespace L0 {
namespace ult {

using MultiTileImmediateCommandListTest = Test<MultiTileCommandListFixture<true, false, false>>;

HWTEST2_F(MultiTileImmediateCommandListTest, GivenMultiTileDeviceWhenCreatingImmediateCommandListThenExpectPartitionCountMatchTileCount, IsWithinXeGfxFamily) {
    EXPECT_EQ(2u, device->getNEODevice()->getDeviceBitfield().count());
    EXPECT_EQ(2u, commandList->partitionCount);

    auto returnValue = commandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(2u, commandList->partitionCount);
}

using MultiTileImmediateInternalCommandListTest = Test<MultiTileCommandListFixture<true, true, false>>;

HWTEST2_F(MultiTileImmediateInternalCommandListTest, GivenMultiTileDeviceWhenCreatingInternalImmediateCommandListThenExpectPartitionCountEqualOne, IsWithinXeGfxFamily) {
    EXPECT_EQ(2u, device->getNEODevice()->getDeviceBitfield().count());
    EXPECT_EQ(1u, commandList->partitionCount);

    auto returnValue = commandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(1u, commandList->partitionCount);
}

using MultiTileCopyEngineCommandListTest = Test<MultiTileCommandListFixture<false, false, true>>;

HWTEST2_F(MultiTileCopyEngineCommandListTest, GivenMultiTileDeviceWhenCreatingCopyEngineCommandListThenExpectPartitionCountEqualOne, IsWithinXeGfxFamily) {
    EXPECT_EQ(2u, device->getNEODevice()->getDeviceBitfield().count());
    EXPECT_EQ(1u, commandList->partitionCount);

    auto returnValue = commandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(1u, commandList->partitionCount);
}

using CommandListExecuteImmediate = Test<DeviceFixture>;
HWTEST2_F(CommandListExecuteImmediate, whenExecutingCommandListImmediateWithFlushTaskThenRequiredStreamStateIsCorrectlyReported, IsAtLeastSkl) {
    auto &hwHelper = NEO::HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily);
    auto &hwInfoConfig = *NEO::HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    std::unique_ptr<L0::CommandList> commandList;
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    commandList.reset(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));
    auto &commandListImmediate = static_cast<MockCommandListImmediate<gfxCoreFamily> &>(*commandList);

    auto &currentCsrStreamProperties = commandListImmediate.csr->getStreamProperties();

    commandListImmediate.requiredStreamState.frontEndState.computeDispatchAllWalkerEnable.value = 1;
    commandListImmediate.requiredStreamState.frontEndState.disableEUFusion.value = 1;
    commandListImmediate.requiredStreamState.frontEndState.disableOverdispatch.value = 1;
    commandListImmediate.requiredStreamState.stateComputeMode.isCoherencyRequired.value = 1;
    commandListImmediate.requiredStreamState.stateComputeMode.largeGrfMode.value = 1;
    commandListImmediate.requiredStreamState.stateComputeMode.threadArbitrationPolicy.value = NEO::ThreadArbitrationPolicy::RoundRobin;
    commandListImmediate.executeCommandListImmediateWithFlushTask(false);

    int expectedDisableOverdispatch = hwInfoConfig.isDisableOverdispatchAvailable(*defaultHwInfo);
    bool expectedIsCoherencyRequired = hwHelper.forceNonGpuCoherencyWA(true);
    int expectedLargeGrfMode = hwInfoConfig.isGrfNumReportedWithScm() ? 1 : -1;
    int expectedThreadArbitrationPolicy = hwInfoConfig.isThreadArbitrationPolicyReportedWithScm() ? NEO::ThreadArbitrationPolicy::RoundRobin : -1;
    EXPECT_EQ(1, currentCsrStreamProperties.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(1, currentCsrStreamProperties.frontEndState.disableEUFusion.value);
    EXPECT_EQ(expectedDisableOverdispatch, currentCsrStreamProperties.frontEndState.disableOverdispatch.value);
    EXPECT_EQ(expectedIsCoherencyRequired, currentCsrStreamProperties.stateComputeMode.isCoherencyRequired.value);
    EXPECT_EQ(expectedLargeGrfMode, currentCsrStreamProperties.stateComputeMode.largeGrfMode.value);
    EXPECT_EQ(expectedThreadArbitrationPolicy, currentCsrStreamProperties.stateComputeMode.threadArbitrationPolicy.value);

    commandListImmediate.requiredStreamState.frontEndState.computeDispatchAllWalkerEnable.value = 0;
    commandListImmediate.requiredStreamState.frontEndState.disableEUFusion.value = 0;
    commandListImmediate.requiredStreamState.frontEndState.disableOverdispatch.value = 0;
    commandListImmediate.requiredStreamState.stateComputeMode.isCoherencyRequired.value = 0;
    commandListImmediate.requiredStreamState.stateComputeMode.largeGrfMode.value = 0;
    commandListImmediate.requiredStreamState.stateComputeMode.threadArbitrationPolicy.value = NEO::ThreadArbitrationPolicy::AgeBased;
    commandListImmediate.executeCommandListImmediateWithFlushTask(false);

    expectedLargeGrfMode = hwInfoConfig.isGrfNumReportedWithScm() ? 0 : -1;
    expectedThreadArbitrationPolicy = hwInfoConfig.isThreadArbitrationPolicyReportedWithScm() ? NEO::ThreadArbitrationPolicy::AgeBased : -1;
    EXPECT_EQ(0, currentCsrStreamProperties.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(0, currentCsrStreamProperties.frontEndState.disableEUFusion.value);
    EXPECT_EQ(0, currentCsrStreamProperties.frontEndState.disableOverdispatch.value);
    EXPECT_EQ(0, currentCsrStreamProperties.stateComputeMode.isCoherencyRequired.value);
    EXPECT_EQ(expectedLargeGrfMode, currentCsrStreamProperties.stateComputeMode.largeGrfMode.value);
    EXPECT_EQ(expectedThreadArbitrationPolicy, currentCsrStreamProperties.stateComputeMode.threadArbitrationPolicy.value);
}

HWTEST2_F(CommandListExecuteImmediate, whenExecutingCommandListImmediateWithFlushTaskThenContainsAnyKernelFlagIsReset, IsAtLeastSkl) {
    std::unique_ptr<L0::CommandList> commandList;
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    commandList.reset(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));
    auto &commandListImmediate = static_cast<MockCommandListImmediate<gfxCoreFamily> &>(*commandList);

    commandListImmediate.containsAnyKernel = true;
    commandListImmediate.executeCommandListImmediateWithFlushTask(false);
    EXPECT_FALSE(commandListImmediate.containsAnyKernel);
}

using CommandListTest = Test<DeviceFixture>;
using IsDcFlushSupportedPlatform = IsWithinGfxCore<IGFX_GEN9_CORE, IGFX_XE_HP_CORE>;

HWTEST2_F(CommandListTest, givenCopyCommandListWhenRequiredFlushOperationThenExpectNoPipeControl, IsDcFlushSupportedPlatform) {
    EXPECT_TRUE(NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getHwInfo()));

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t returnValue = commandList->initialize(device, NEO::EngineGroupType::Copy, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto &commandContainer = commandList->commandContainer;

    size_t usedBefore = commandContainer.getCommandStream()->getUsed();
    commandList->addFlushRequiredCommand(true, nullptr);
    size_t usedAfter = commandContainer.getCommandStream()->getUsed();
    EXPECT_EQ(usedBefore, usedAfter);
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenRequiredFlushOperationThenExpectPipeControlWithDcFlush, IsDcFlushSupportedPlatform) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    EXPECT_TRUE(NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getHwInfo()));

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t returnValue = commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto &commandContainer = commandList->commandContainer;

    size_t usedBefore = commandContainer.getCommandStream()->getUsed();
    commandList->addFlushRequiredCommand(true, nullptr);
    size_t usedAfter = commandContainer.getCommandStream()->getUsed();
    EXPECT_EQ(sizeof(PIPE_CONTROL), usedAfter - usedBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), usedBefore),
        usedAfter - usedBefore));
    auto pipeControl = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(pipeControl, cmdList.end());
    auto cmdPipeControl = genCmdCast<PIPE_CONTROL *>(*pipeControl);
    EXPECT_TRUE(cmdPipeControl->getDcFlushEnable());
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenNoRequiredFlushOperationThenExpectNoPipeControl, IsDcFlushSupportedPlatform) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    EXPECT_TRUE(NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getHwInfo()));

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t returnValue = commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto &commandContainer = commandList->commandContainer;

    size_t usedBefore = commandContainer.getCommandStream()->getUsed();
    commandList->addFlushRequiredCommand(false, nullptr);
    size_t usedAfter = commandContainer.getCommandStream()->getUsed();
    EXPECT_EQ(usedBefore, usedAfter);
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenRequiredFlushOperationAndNoSignalScopeEventThenExpectPipeControlWithDcFlush, IsDcFlushSupportedPlatform) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    EXPECT_TRUE(NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getHwInfo()));

    ze_result_t result;
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t returnValue = commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto &commandContainer = commandList->commandContainer;

    size_t usedBefore = commandContainer.getCommandStream()->getUsed();
    commandList->addFlushRequiredCommand(true, event.get());
    size_t usedAfter = commandContainer.getCommandStream()->getUsed();
    EXPECT_EQ(sizeof(PIPE_CONTROL), usedAfter - usedBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), usedBefore),
        usedAfter - usedBefore));
    auto pipeControl = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(pipeControl, cmdList.end());
    auto cmdPipeControl = genCmdCast<PIPE_CONTROL *>(*pipeControl);
    EXPECT_TRUE(cmdPipeControl->getDcFlushEnable());
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenRequiredFlushOperationAndSignalScopeEventThenExpectNoPipeControl, IsDcFlushSupportedPlatform) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    EXPECT_TRUE(NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getHwInfo()));

    ze_result_t result;
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t returnValue = commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto &commandContainer = commandList->commandContainer;

    size_t usedBefore = commandContainer.getCommandStream()->getUsed();
    commandList->addFlushRequiredCommand(true, event.get());
    size_t usedAfter = commandContainer.getCommandStream()->getUsed();
    EXPECT_EQ(usedBefore, usedAfter);
}

HWTEST2_F(CommandListTest, givenImmediateCommandListWhenAppendMemoryRangesBarrierUsingFlushTaskThenExpectCorrectExecuteCall, IsAtLeastSkl) {
    ze_result_t result;
    uint32_t numRanges = 1;
    const size_t rangeSizes = 1;
    const char *rangesBuffer[rangeSizes];
    const void **ranges = reinterpret_cast<const void **>(&rangesBuffer[0]);

    MockCommandListImmediateHw<gfxCoreFamily> cmdList;
    cmdList.isFlushTaskSubmissionEnabled = true;
    cmdList.cmdListType = CommandList::CommandListType::TYPE_IMMEDIATE;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    result = cmdList.appendMemoryRangesBarrier(numRanges, &rangeSizes,
                                               ranges, nullptr, 0,
                                               nullptr);
    EXPECT_EQ(0u, cmdList.executeCommandListImmediateCalledCount);
    EXPECT_EQ(1u, cmdList.executeCommandListImmediateWithFlushTaskCalledCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(CommandListTest, givenImmediateCommandListWhenAppendMemoryRangesBarrierNotUsingFlushTaskThenExpectCorrectExecuteCall, IsAtLeastSkl) {
    ze_result_t result;
    uint32_t numRanges = 1;
    const size_t rangeSizes = 1;
    const char *rangesBuffer[rangeSizes];
    const void **ranges = reinterpret_cast<const void **>(&rangesBuffer[0]);

    MockCommandListImmediateHw<gfxCoreFamily> cmdList;
    cmdList.isFlushTaskSubmissionEnabled = false;
    cmdList.cmdListType = CommandList::CommandListType::TYPE_IMMEDIATE;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    result = cmdList.appendMemoryRangesBarrier(numRanges, &rangeSizes,
                                               ranges, nullptr, 0,
                                               nullptr);
    EXPECT_EQ(1u, cmdList.executeCommandListImmediateCalledCount);
    EXPECT_EQ(0u, cmdList.executeCommandListImmediateWithFlushTaskCalledCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(CommandListTest,
          givenComputeCommandListAnd2dRegionWhenMemoryCopyRegionInExternalHostAllocationCalledThenBuiltinFlagAndDestinationAllocSystemIsSet, IsAtLeastSkl) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 0, 2, 2, 1};
    ze_copy_region_t srcRegion = {4, 4, 0, 2, 2, 1};
    commandList->appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);
}

HWTEST2_F(CommandListTest,
          givenComputeCommandListAnd2dRegionWhenMemoryCopyRegionInUsmHostAllocationCalledThenBuiltinFlagAndDestinationAllocSystemIsSet, IsAtLeastSkl) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    constexpr size_t allocSize = 4096;
    void *dstBuffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocHostMem(&hostDesc, allocSize, allocSize, &dstBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    ze_copy_region_t dstRegion = {4, 4, 0, 2, 2, 1};
    ze_copy_region_t srcRegion = {4, 4, 0, 2, 2, 1};
    commandList->appendMemoryCopyRegion(dstBuffer, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

HWTEST2_F(CommandListTest,
          givenComputeCommandListAnd2dRegionWhenMemoryCopyRegionInUsmDeviceAllocationCalledThenBuiltinFlagIsSetAndDestinationAllocSystemFlagNotSet, IsAtLeastSkl) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

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
    commandList->appendMemoryCopyRegion(dstBuffer, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

HWTEST2_F(CommandListTest,
          givenComputeCommandListAnd3dRegionWhenMemoryCopyRegionInExternalHostAllocationCalledThenBuiltinAndDestinationAllocSystemFlagIsSet, IsAtLeastSkl) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    commandList->appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);
}

HWTEST2_F(CommandListTest,
          givenComputeCommandListAnd3dRegionWhenMemoryCopyRegionInUsmHostAllocationCalledThenBuiltinAndDestinationAllocSystemFlagIsSet, IsAtLeastSkl) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    constexpr size_t allocSize = 4096;
    void *dstBuffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocHostMem(&hostDesc, allocSize, allocSize, &dstBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    commandList->appendMemoryCopyRegion(dstBuffer, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

HWTEST2_F(CommandListTest,
          givenComputeCommandListAnd3dRegionWhenMemoryCopyRegionInUsmDeviceAllocationCalledThenBuiltinFlagIsSetAndDestinationAllocSystemFlagNotSet, IsAtLeastSkl) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

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
    commandList->appendMemoryCopyRegion(dstBuffer, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

using ImageSupport = IsNotAnyGfxCores<IGFX_GEN8_CORE, IGFX_XE_HPC_CORE>;

HWTEST2_F(CommandListTest, givenComputeCommandListWhenCopyFromImageToImageTheBuiltinFlagIsSet, ImageSupport) {
    auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::CopyImageRegion);
    auto mockBuiltinKernel = static_cast<Mock<::L0::Kernel> *>(kernel);
    mockBuiltinKernel->setArgRedescribedImageCallBase = false;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto imageHwSrc = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    auto imageHwDst = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHwSrc->initialize(device, &zeDesc);
    imageHwDst->initialize(device, &zeDesc);

    ze_image_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    ze_image_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    commandList->appendImageCopyRegion(imageHwDst->toHandle(), imageHwSrc->toHandle(), &dstRegion, &srcRegion, nullptr, 0, nullptr);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenCopyFromImageToExternalHostMemoryThenBuiltinFlagAndDestinationAllocSystemIsSet, ImageSupport) {
    auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::CopyImageRegion);
    auto mockBuiltinKernel = static_cast<Mock<::L0::Kernel> *>(kernel);
    mockBuiltinKernel->setArgRedescribedImageCallBase = false;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    void *dstPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto imageHw = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHw->initialize(device, &zeDesc);

    ze_image_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    commandList->appendImageCopyToMemory(dstPtr, imageHw->toHandle(), &srcRegion, nullptr, 0, nullptr);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenCopyFromImageToUsmHostMemoryThenBuiltinFlagAndDestinationAllocSystemIsSet, ImageSupport) {
    auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::CopyImageRegion);
    auto mockBuiltinKernel = static_cast<Mock<::L0::Kernel> *>(kernel);
    mockBuiltinKernel->setArgRedescribedImageCallBase = false;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    constexpr size_t allocSize = 4096;
    void *dstBuffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocHostMem(&hostDesc, allocSize, allocSize, &dstBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto imageHw = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHw->initialize(device, &zeDesc);

    ze_image_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    commandList->appendImageCopyToMemory(dstBuffer, imageHw->toHandle(), &srcRegion, nullptr, 0, nullptr);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenCopyFromImageToUsmDeviceMemoryThenBuiltinFlagIsSetAndDestinationAllocSystemNotSet, ImageSupport) {
    auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::CopyImageRegion);
    auto mockBuiltinKernel = static_cast<Mock<::L0::Kernel> *>(kernel);
    mockBuiltinKernel->setArgRedescribedImageCallBase = false;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

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
    auto imageHw = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHw->initialize(device, &zeDesc);

    ze_image_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    commandList->appendImageCopyToMemory(dstBuffer, imageHw->toHandle(), &srcRegion, nullptr, 0, nullptr);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenImageCopyFromMemoryThenBuiltinFlagIsSet, ImageSupport) {
    auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::CopyImageRegion);
    auto mockBuiltinKernel = static_cast<Mock<::L0::Kernel> *>(kernel);
    mockBuiltinKernel->setArgRedescribedImageCallBase = false;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.type = ZE_IMAGE_TYPE_3D;
    zeDesc.height = 2;
    zeDesc.depth = 2;
    auto imageHw = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHw->initialize(device, &zeDesc);

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    commandList->appendImageCopyFromMemory(imageHw->toHandle(), srcPtr, nullptr, nullptr, 0, nullptr);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenMemoryCopyInExternalHostAllocationThenBuiltinFlagAndDestinationAllocSystemIsSet, IsAtLeastSkl) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isKernelSplitOperation);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenMemoryCopyInUsmHostAllocationThenBuiltinFlagAndDestinationAllocSystemIsSet, IsAtLeastSkl) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    constexpr size_t allocSize = 4096;
    void *dstBuffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocHostMem(&hostDesc, allocSize, allocSize, &dstBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    void *srcPtr = reinterpret_cast<void *>(0x1234);

    commandList->appendMemoryCopy(dstBuffer, srcPtr, 8, nullptr, 0, nullptr);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isKernelSplitOperation);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenMemoryCopyInUsmDeviceAllocationThenBuiltinFlagIsSetAndDestinationAllocSystemNotSet, IsAtLeastSkl) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    constexpr size_t size = 4096u;
    constexpr size_t alignment = 4096u;
    void *dstBuffer = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(),
                                          &deviceDesc,
                                          size, alignment, &dstBuffer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    void *srcPtr = reinterpret_cast<void *>(0x1234);

    commandList->appendMemoryCopy(dstBuffer, srcPtr, 8, nullptr, 0, nullptr);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isKernelSplitOperation);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenMemoryFillInUsmHostThenBuiltinFlagAndDestinationAllocSystemIsSet, IsAtLeastSkl) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    constexpr size_t allocSize = 4096;
    constexpr size_t patternSize = 8;
    uint8_t pattern[patternSize] = {1, 2, 3, 4};

    void *dstBuffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocHostMem(&hostDesc, allocSize, allocSize, &dstBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->appendMemoryFill(dstBuffer, pattern, patternSize, allocSize, nullptr, 0, nullptr);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isKernelSplitOperation);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenMemoryFillInUsmDeviceThenBuiltinFlagIsSetAndDestinationAllocSystemNotSet, IsAtLeastSkl) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

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

    commandList->appendMemoryFill(dstBuffer, pattern, patternSize, size, nullptr, 0, nullptr);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isKernelSplitOperation);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

} // namespace ult
} // namespace L0
