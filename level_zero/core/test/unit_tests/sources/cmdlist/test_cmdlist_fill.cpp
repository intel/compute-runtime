/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/builtin/builtin_functions_lib_impl.h"
#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.inl"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

#include <limits>

namespace L0 {
namespace ult {

using AppendFillTest = Test<AppendFillFixture>;

HWTEST2_F(AppendFillTest,
          givenCallToAppendMemoryFillWithImmediateValueThenSuccessIsReturned, MatchAny) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    auto result = commandList->appendMemoryFill(immediateDstPtr, &immediatePattern,
                                                sizeof(immediatePattern),
                                                immediateAllocSize, nullptr, 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(AppendFillTest,
          givenCallToAppendMemoryFillThenSuccessIsReturned, MatchAny) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    auto result = commandList->appendMemoryFill(dstPtr, pattern, patternSize, allocSize, nullptr, 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(AppendFillTest,
          givenCallToAppendMemoryFillWithAppendLaunchKernelFailureThenSuccessIsNotReturned, MatchAny) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    commandList->thresholdOfCallsToAppendLaunchKernelWithParamsToFail = 0;

    auto result = commandList->appendMemoryFill(dstPtr, pattern, patternSize, allocSize, nullptr, 0, nullptr, false);
    EXPECT_NE(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(AppendFillTest,
          givenTwoCallsToAppendMemoryFillWithSamePatternThenAllocationIsCreatedForEachCall, MatchAny) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    ze_result_t result = commandList->appendMemoryFill(dstPtr, pattern, 4, allocSize, nullptr, 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t patternAllocationsVectorSize = commandList->patternAllocations.size();
    EXPECT_EQ(patternAllocationsVectorSize, 1u);

    uint8_t *newDstPtr = new uint8_t[allocSize];
    result = commandList->appendMemoryFill(newDstPtr, pattern, patternSize, allocSize, nullptr, 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t newPatternAllocationsVectorSize = commandList->patternAllocations.size();

    EXPECT_GT(newPatternAllocationsVectorSize, patternAllocationsVectorSize);

    delete[] newDstPtr;
}

HWTEST2_F(AppendFillTest,
          givenTwoCallsToAppendMemoryFillWithDifferentPatternsThenAllocationIsCreatedForEachPattern, MatchAny) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    ze_result_t result = commandList->appendMemoryFill(dstPtr, pattern, 4, allocSize, nullptr, 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t patternAllocationsVectorSize = commandList->patternAllocations.size();
    EXPECT_EQ(patternAllocationsVectorSize, 1u);

    uint8_t newPattern[patternSize] = {1, 2, 3, 4};
    result = commandList->appendMemoryFill(dstPtr, newPattern, patternSize, allocSize, nullptr, 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t newPatternAllocationsVectorSize = commandList->patternAllocations.size();

    EXPECT_EQ(patternAllocationsVectorSize + 1u, newPatternAllocationsVectorSize);
}

HWTEST2_F(AppendFillTest,
          givenAppendMemoryFillWhenPatternSizeIsOneThenDispatchOneKernel, MatchAny) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    int pattern = 0;
    const size_t size = 1024 * 1024;
    uint8_t *ptr = new uint8_t[size];
    ze_result_t result = commandList->appendMemoryFill(ptr, &pattern, 1, size, nullptr, 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, commandList->numberOfCallsToAppendLaunchKernelWithParams);
    EXPECT_EQ(size, commandList->xGroupSizes[0] * commandList->threadGroupDimensions[0].groupCountX * 16);
    delete[] ptr;
}

HWTEST2_F(AppendFillTest,
          givenAppendMemoryFillWithUnalignedSizeWhenPatternSizeIsOneThenDispatchTwoKernels, MatchAny) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    int pattern = 0;
    const size_t size = 1025;
    uint8_t *ptr = new uint8_t[size];
    ze_result_t result = commandList->appendMemoryFill(ptr, &pattern, 1, size, nullptr, 0, nullptr, false);
    size_t filledSize = commandList->xGroupSizes[0] * commandList->threadGroupDimensions[0].groupCountX * 16;
    filledSize += commandList->xGroupSizes[1] * commandList->threadGroupDimensions[1].groupCountX;
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(2u, commandList->numberOfCallsToAppendLaunchKernelWithParams);
    EXPECT_EQ(size, filledSize);
    delete[] ptr;
}

HWTEST2_F(AppendFillTest,
          givenAppendMemoryFillWithSizeBelow16WhenPatternSizeIsOneThenDispatchTwoKernels, MatchAny) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    int pattern = 0;
    const size_t size = 4;
    uint8_t *ptr = new uint8_t[size];
    ze_result_t result = commandList->appendMemoryFill(ptr, &pattern, 1, size, nullptr, 0, nullptr, false);
    size_t filledSize = commandList->xGroupSizes[0] * commandList->threadGroupDimensions[0].groupCountX * 16;
    filledSize += commandList->xGroupSizes[1] * commandList->threadGroupDimensions[1].groupCountX;
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(2u, commandList->numberOfCallsToAppendLaunchKernelWithParams);
    EXPECT_EQ(size, filledSize);
    delete[] ptr;
}

HWTEST2_F(AppendFillTest,
          givenAppendMemoryFillWithSizeBelowMaxWorkgroupSizeWhenPatternSizeIsOneThenDispatchOneKernel, MatchAny) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    int pattern = 0;
    const size_t size = neoDevice->getDeviceInfo().maxWorkGroupSize / 2;
    uint8_t *ptr = new uint8_t[size];
    ze_result_t result = commandList->appendMemoryFill(ptr, &pattern, 1, size, nullptr, 0, nullptr, false);
    size_t filledSize = commandList->xGroupSizes[0] * commandList->threadGroupDimensions[0].groupCountX * 16;
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, commandList->numberOfCallsToAppendLaunchKernelWithParams);
    EXPECT_EQ(size, filledSize);
    delete[] ptr;
}

HWTEST2_F(AppendFillTest,
          givenAppendMemoryFillWhenPatternSizeIsOneThenGroupCountIsCorrect, MatchAny) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    int pattern = 0;
    const size_t size = 1024 * 1024;
    uint8_t *ptr = new uint8_t[size];
    ze_result_t result = commandList->appendMemoryFill(ptr, &pattern, 1, size, nullptr, 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto groupSize = device->getDeviceInfo().maxWorkGroupSize;
    auto dataTypeSize = sizeof(uint32_t) * 4;
    auto expectedGroupCount = size / (dataTypeSize * groupSize);
    EXPECT_EQ(expectedGroupCount, commandList->threadGroupDimensions[0].groupCountX);
    delete[] ptr;
}

HWTEST2_F(AppendFillTest,
          givenAppendMemoryFillWhenPtrWithOffsetAndPatternSizeIsOneThenThreeKernelsDispatched, MatchAny) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    int pattern = 0;
    uint32_t offset = 1;
    const size_t size = 1024;
    uint8_t *ptr = new uint8_t[size];
    ze_result_t result = commandList->appendMemoryFill(ptr + offset, &pattern, 1, size - offset, nullptr, 0, nullptr, false);
    size_t filledSize = commandList->xGroupSizes[0] * commandList->threadGroupDimensions[0].groupCountX;
    filledSize += commandList->xGroupSizes[1] * commandList->threadGroupDimensions[1].groupCountX * 16;
    filledSize += commandList->xGroupSizes[2] * commandList->threadGroupDimensions[2].groupCountX;
    EXPECT_EQ(sizeof(uint32_t) - offset, commandList->xGroupSizes[0]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(3u, commandList->numberOfCallsToAppendLaunchKernelWithParams);
    EXPECT_EQ(size - offset, filledSize);
    delete[] ptr;
}

HWTEST2_F(AppendFillTest,
          givenAppendMemoryFillWhenPtrWithOffsetAndSmallSizeAndPatternSizeIsOneThenTwoKernelsDispatched, MatchAny) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    int pattern = 0;
    uint32_t offset = 1;
    const size_t size = 2;
    uint8_t *ptr = new uint8_t[size];
    ze_result_t result = commandList->appendMemoryFill(ptr + offset, &pattern, 1, size - offset, nullptr, 0, nullptr, false);
    size_t filledSize = commandList->xGroupSizes[0] * commandList->threadGroupDimensions[0].groupCountX * 16;
    filledSize += commandList->xGroupSizes[1] * commandList->threadGroupDimensions[1].groupCountX;
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(2u, commandList->numberOfCallsToAppendLaunchKernelWithParams);
    EXPECT_EQ(size - offset, filledSize);
    delete[] ptr;
}

HWTEST2_F(AppendFillTest,
          givenAppendMemoryFillWhenPtrWithOffsetAndFailAppendUnalignedFillKernelThenReturnError, MatchAny) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->thresholdOfCallsToAppendLaunchKernelWithParamsToFail = 0;
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    int pattern = 0;
    uint32_t offset = 1;
    const size_t size = 1024;
    uint8_t *ptr = new uint8_t[size];
    ze_result_t result = commandList->appendMemoryFill(ptr + offset, &pattern, 1, size - offset, nullptr, 0, nullptr, false);
    EXPECT_NE(ZE_RESULT_SUCCESS, result);
    delete[] ptr;
}

HWTEST2_F(AppendFillTest,
          givenCallToAppendMemoryFillWithSizeNotMultipleOfPatternSizeThenSuccessIsReturned, MatchAny) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    size_t nonMultipleSize = allocSize + 1;
    uint8_t *nonMultipleDstPtr = new uint8_t[nonMultipleSize];
    auto result = commandList->appendMemoryFill(nonMultipleDstPtr, pattern, 4, nonMultipleSize, nullptr, 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    delete[] nonMultipleDstPtr;
}

HWTEST2_F(AppendFillTest,
          givenCallToAppendMemoryFillWithSizeNotMultipleOfPatternSizeAndAppendLaunchKernelFailureOnRemainderThenSuccessIsNotReturned, MatchAny) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    commandList->thresholdOfCallsToAppendLaunchKernelWithParamsToFail = 1;

    size_t nonMultipleSize = allocSize + 1;
    uint8_t *nonMultipleDstPtr = new uint8_t[nonMultipleSize];
    auto result = commandList->appendMemoryFill(nonMultipleDstPtr, pattern, 4, nonMultipleSize, nullptr, 0, nullptr, false);
    EXPECT_NE(ZE_RESULT_SUCCESS, result);

    delete[] nonMultipleDstPtr;
}

HWTEST2_F(AppendFillTest,
          givenCallToAppendMemoryFillWithImmediateValueWhenTimestampEventUsesRegistersThenSinglePacketUsesRegisterProfiling, IsGen12LP) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using GPGPU_WALKER = typename GfxFamily::GPGPU_WALKER;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    uint64_t globalStartAddress = event->getGpuAddress(device) + event->getGlobalStartOffset();
    uint64_t contextStartAddress = event->getGpuAddress(device) + event->getContextStartOffset();
    uint64_t globalEndAddress = event->getGpuAddress(device) + event->getGlobalEndOffset();
    uint64_t contextEndAddress = event->getGpuAddress(device) + event->getContextEndOffset();

    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    result = commandList->appendMemoryFill(immediateDstPtr, &immediatePattern,
                                           sizeof(immediatePattern),
                                           immediateAllocSize, event->toHandle(), 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(1u, event->getPacketsInUse());
    EXPECT_EQ(1u, event->getKernelCount());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0),
        commandList->commandContainer.getCommandStream()->getUsed()));

    auto itorWalkers = findAll<GPGPU_WALKER *>(cmdList.begin(), cmdList.end());
    auto begin = cmdList.begin();
    ASSERT_EQ(2u, itorWalkers.size());
    auto secondWalker = itorWalkers[1];

    validateTimestampRegisters<FamilyType>(cmdList,
                                           begin,
                                           RegisterOffsets::globalTimestampLdw, globalStartAddress,
                                           RegisterOffsets::gpThreadTimeRegAddressOffsetLow, contextStartAddress,
                                           false);

    validateTimestampRegisters<FamilyType>(cmdList,
                                           secondWalker,
                                           RegisterOffsets::globalTimestampLdw, globalEndAddress,
                                           RegisterOffsets::gpThreadTimeRegAddressOffsetLow, contextEndAddress,
                                           false);
}

HWTEST2_F(AppendFillTest,
          givenCallToAppendMemoryFillWhenTimestampEventUsesRegistersThenSinglePacketUsesRegisterProfiling, IsGen12LP) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using GPGPU_WALKER = typename GfxFamily::GPGPU_WALKER;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    uint64_t globalStartAddress = event->getGpuAddress(device) + event->getGlobalStartOffset();
    uint64_t contextStartAddress = event->getGpuAddress(device) + event->getContextStartOffset();
    uint64_t globalEndAddress = event->getGpuAddress(device) + event->getGlobalEndOffset();
    uint64_t contextEndAddress = event->getGpuAddress(device) + event->getContextEndOffset();

    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    result = commandList->appendMemoryFill(dstPtr, pattern, patternSize, allocSize, event->toHandle(), 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(1u, event->getPacketsInUse());
    EXPECT_EQ(1u, event->getKernelCount());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0),
        commandList->commandContainer.getCommandStream()->getUsed()));

    auto itorWalkers = findAll<GPGPU_WALKER *>(cmdList.begin(), cmdList.end());
    auto begin = cmdList.begin();
    ASSERT_EQ(2u, itorWalkers.size());
    auto secondWalker = itorWalkers[1];

    validateTimestampRegisters<FamilyType>(cmdList,
                                           begin,
                                           RegisterOffsets::globalTimestampLdw, globalStartAddress,
                                           RegisterOffsets::gpThreadTimeRegAddressOffsetLow, contextStartAddress,
                                           false);

    validateTimestampRegisters<FamilyType>(cmdList,
                                           secondWalker,
                                           RegisterOffsets::globalTimestampLdw, globalEndAddress,
                                           RegisterOffsets::gpThreadTimeRegAddressOffsetLow, contextEndAddress,
                                           false);
}

} // namespace ult
} // namespace L0
