/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/register_offsets.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.inl"

namespace L0 {
namespace ult {
template <typename Type>
struct WhiteBox;

using AppendFillTest = Test<AppendFillFixture>;

HWTEST_F(AppendFillTest, givenCallToAppendMemoryFillWithImmediateValueThenSuccessIsReturned) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    CmdListMemoryCopyParams copyParams = {};
    auto result = commandList->appendMemoryFill(immediateDstPtr, &immediatePattern,
                                                sizeof(immediatePattern),
                                                immediateAllocSize, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST_F(AppendFillTest, givenCallToAppendMemoryFillThenSuccessIsReturned) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    CmdListMemoryCopyParams copyParams = {};
    auto result = commandList->appendMemoryFill(dstPtr, pattern, patternSize, allocSize, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST_F(AppendFillTest, givenCallToAppendMemoryFillWithAppendLaunchKernelFailureThenSuccessIsNotReturned) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    commandList->thresholdOfCallsToAppendLaunchKernelWithParamsToFail = 0;

    CmdListMemoryCopyParams copyParams = {};
    auto result = commandList->appendMemoryFill(dstPtr, pattern, patternSize, allocSize, nullptr, 0, nullptr, copyParams);
    EXPECT_NE(ZE_RESULT_SUCCESS, result);
}

HWTEST_F(AppendFillTest, givenCallToAppendMemoryFillWithDataSizeNotAlignedToBothSizeOfFillDataAndMaxWgsThenUseFill) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    const auto patternSize = 4;
    const auto allocSize = sizeof(uint32_t) * 4 * device->getDeviceInfo().maxWorkGroupSize + 1;
    size_t patternTagsVectorSizeBefore = commandList->patternTags.size();
    CmdListMemoryCopyParams copyParams = {};
    ze_result_t result = commandList->appendMemoryFill(dstPtr, pattern, patternSize, allocSize, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t patternTagsVectorSize = commandList->patternTags.size();
    EXPECT_NE(patternTagsVectorSize, patternTagsVectorSizeBefore);
    EXPECT_EQ(0u, commandList->patternAllocations.size());
}

HWTEST_F(AppendFillTest, givenCallToAppendMemoryFillWithPatternSizeLessOrEqualThanFourButUnalignedSizeThenUseFill) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    for (const auto patternSize : {1, 2, 4}) {
        size_t patternTagsVectorSizeBefore = commandList->patternTags.size();
        CmdListMemoryCopyParams copyParams = {};
        ze_result_t result = commandList->appendMemoryFill(dstPtr, pattern, patternSize, allocSize, nullptr, 0, nullptr, copyParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        size_t patternTagsVectorSize = commandList->patternTags.size();
        if (patternSize == 1) {
            EXPECT_EQ(patternTagsVectorSize, patternTagsVectorSizeBefore);
        } else {
            EXPECT_NE(patternTagsVectorSize, patternTagsVectorSizeBefore);
        }
        EXPECT_EQ(0u, commandList->patternAllocations.size());
    }
}

HWTEST_F(AppendFillTest, givenCallToAppendMemoryFillWithPatternSizeLessOrEqualThanFourThenUseImmmediateFill) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    for (const auto patternSize : {1, 2, 4}) {
        CmdListMemoryCopyParams copyParams = {};
        ze_result_t result = commandList->appendMemoryFill(dstPtr, pattern, patternSize, 256, nullptr, 0, nullptr, copyParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        size_t patternAllocationsVectorSize = commandList->patternAllocations.size();
        EXPECT_EQ(patternAllocationsVectorSize, 0u);
        EXPECT_EQ(0u, commandList->patternTags.size());
    }
}

HWTEST_F(AppendFillTest, givenTwoCallsToAppendMemoryFillWithSamePatternThenAllocationIsCreatedForEachCall) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    CmdListMemoryCopyParams copyParams = {};
    char pattern[65] = {};
    ze_result_t result = commandList->appendMemoryFill(dstPtr, pattern, sizeof(pattern), allocSize, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t patternAllocationsVectorSize = commandList->patternAllocations.size();
    EXPECT_EQ(patternAllocationsVectorSize, 1u);

    uint8_t *newDstPtr = new uint8_t[allocSize];
    result = commandList->appendMemoryFill(newDstPtr, pattern, sizeof(pattern), allocSize, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t newPatternAllocationsVectorSize = commandList->patternAllocations.size();

    EXPECT_GT(newPatternAllocationsVectorSize, patternAllocationsVectorSize);

    delete[] newDstPtr;
}

HWTEST_F(AppendFillTest, givenTwoCallsToAppendMemoryFillWithDifferentPatternsThenAllocationIsCreatedForEachPattern) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    CmdListMemoryCopyParams copyParams = {};
    char pattern[65] = {};
    ze_result_t result = commandList->appendMemoryFill(dstPtr, pattern, sizeof(pattern), allocSize, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t patternAllocationsVectorSize = commandList->patternAllocations.size();
    EXPECT_EQ(patternAllocationsVectorSize, 1u);

    char newPattern[66] = {};
    result = commandList->appendMemoryFill(dstPtr, newPattern, sizeof(newPattern), allocSize, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t newPatternAllocationsVectorSize = commandList->patternAllocations.size();

    EXPECT_EQ(patternAllocationsVectorSize + 1u, newPatternAllocationsVectorSize);
}

HWTEST_F(AppendFillTest, givenTwoCallsToAppendMemoryFillWithSamePatternThenTagIsCreatedForEachCall) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    CmdListMemoryCopyParams copyParams = {};
    ze_result_t result = commandList->appendMemoryFill(dstPtr, pattern, 8, allocSize, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t patternTagsVectorSize = commandList->patternTags.size();
    EXPECT_EQ(patternTagsVectorSize, 1u);

    uint8_t *newDstPtr = new uint8_t[allocSize];
    result = commandList->appendMemoryFill(newDstPtr, pattern, patternSize, allocSize, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t newPatternTagsVectorSize = commandList->patternTags.size();

    EXPECT_GT(newPatternTagsVectorSize, patternTagsVectorSize);

    delete[] newDstPtr;
}

HWTEST_F(AppendFillTest, givenTwoCallsToAppendMemoryFillWithDifferentPatternsThenTagIsCreatedForEachPattern) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    CmdListMemoryCopyParams copyParams = {};
    ze_result_t result = commandList->appendMemoryFill(dstPtr, pattern, 8, allocSize, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t patternTagsVectorSize = commandList->patternTags.size();
    EXPECT_EQ(patternTagsVectorSize, 1u);

    uint8_t newPattern[patternSize] = {1, 2, 3, 4};
    result = commandList->appendMemoryFill(dstPtr, newPattern, patternSize, allocSize, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t newPatternTagsVectorSize = commandList->patternTags.size();

    EXPECT_EQ(patternTagsVectorSize + 1u, newPatternTagsVectorSize);
}

HWTEST_F(AppendFillTest, givenAppendMemoryFillWhenPatternSizeIsOneThenDispatchOneKernel) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    int pattern = 0;
    const size_t size = 1024 * 1024;
    uint8_t *ptr = new uint8_t[size];
    CmdListMemoryCopyParams copyParams = {};
    ze_result_t result = commandList->appendMemoryFill(ptr, &pattern, 1, size, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, commandList->numberOfCallsToAppendLaunchKernelWithParams);
    EXPECT_EQ(size, commandList->xGroupSizes[0] * commandList->threadGroupDimensions[0].groupCountX * 16);
    delete[] ptr;
}

HWTEST_F(AppendFillTest, givenAppendMemoryFillWithSharedSystemUsmAndMemAdviseThenReturnSuccess) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableSharedSystemUsmSupport.set(1);
    debugManager.flags.TreatNonUsmForTransfersAsSharedSystem.set(1);
    debugManager.flags.EmitMemAdvisePriorToCopyForNonUsm.set(1);

    auto commandList = std::make_unique<WhiteBox<MockCommandList<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    VariableBackup<uint64_t> sharedSystemMemCapabilities{&hwInfo.capabilityTable.sharedSystemMemCapabilities};
    sharedSystemMemCapabilities = 0xf;

    driverHandle->forceFalseFromfindAllocationDataForRange = true;

    int pattern = 0;
    const size_t size = 17;
    uint8_t *ptr = new uint8_t[size];
    CmdListMemoryCopyParams copyParams = {};
    ze_result_t result = commandList->appendMemoryFill(ptr, &pattern, 1, size, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    delete[] ptr;
}

HWTEST_F(AppendFillTest, givenAppendMemoryFillWithSharedSystemUsmAndNoMemAdviseThenReturnSuccess) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableSharedSystemUsmSupport.set(1);
    debugManager.flags.TreatNonUsmForTransfersAsSharedSystem.set(1);

    auto commandList = std::make_unique<WhiteBox<MockCommandList<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    VariableBackup<uint64_t> sharedSystemMemCapabilities{&hwInfo.capabilityTable.sharedSystemMemCapabilities};
    sharedSystemMemCapabilities = 0xf;

    driverHandle->forceFalseFromfindAllocationDataForRange = true;

    int pattern = 0;
    const size_t size = 17;
    uint8_t *ptr = new uint8_t[size];
    CmdListMemoryCopyParams copyParams = {};
    ze_result_t result = commandList->appendMemoryFill(ptr, &pattern, 1, size, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    delete[] ptr;
}

HWTEST_F(AppendFillTest, givenAppendMemoryFillWithSharedSystemUsmAndTreatNonUsmForTransfersAsSharedSystemNotSetReturnSuccessLegacyMode) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableSharedSystemUsmSupport.set(1);
    debugManager.flags.TreatNonUsmForTransfersAsSharedSystem.set(-1);

    auto commandList = std::make_unique<WhiteBox<MockCommandList<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    VariableBackup<uint64_t> sharedSystemMemCapabilities{&hwInfo.capabilityTable.sharedSystemMemCapabilities};
    sharedSystemMemCapabilities = 0xf;

    driverHandle->forceFalseFromfindAllocationDataForRange = true;

    int pattern = 0;
    const size_t size = 17;
    uint8_t *ptr = new uint8_t[size];
    CmdListMemoryCopyParams copyParams = {};
    ze_result_t result = commandList->appendMemoryFill(ptr, &pattern, 1, size, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    delete[] ptr;
}

HWTEST_F(AppendFillTest, givenAppendMemoryFillWithSharedSystemUsmAndNoDebugFlagsSetReturnError) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableSharedSystemUsmSupport.set(-1);
    debugManager.flags.TreatNonUsmForTransfersAsSharedSystem.set(-1);

    auto commandList = std::make_unique<WhiteBox<MockCommandList<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    VariableBackup<uint64_t> sharedSystemMemCapabilities{&hwInfo.capabilityTable.sharedSystemMemCapabilities};
    sharedSystemMemCapabilities = 0xf;

    driverHandle->forceFalseFromfindAllocationDataForRange = true;

    int pattern = 0;
    const size_t size = 17;
    uint8_t *ptr = new uint8_t[size];
    CmdListMemoryCopyParams copyParams = {};
    ze_result_t result = commandList->appendMemoryFill(ptr, &pattern, 1, size, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    delete[] ptr;
}

HWTEST_F(AppendFillTest, givenAppendMemoryFillWithUnalignedSizeWhenPatternSizeIsOneThenDispatchTwoKernels) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    int pattern = 0;
    const size_t size = 1025;
    uint8_t *ptr = new uint8_t[size];
    CmdListMemoryCopyParams copyParams = {};
    ze_result_t result = commandList->appendMemoryFill(ptr, &pattern, 1, size, nullptr, 0, nullptr, copyParams);
    size_t filledSize = commandList->xGroupSizes[0] * commandList->threadGroupDimensions[0].groupCountX * 16;
    filledSize += commandList->xGroupSizes[1] * commandList->threadGroupDimensions[1].groupCountX;
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(2u, commandList->numberOfCallsToAppendLaunchKernelWithParams);
    EXPECT_EQ(size, filledSize);
    delete[] ptr;
}

HWTEST_F(AppendFillTest, givenAppendMemoryFillWithSizeBelow16WhenPatternSizeIsOneThenDispatchTwoKernels) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    int pattern = 0;
    const size_t size = 4;
    uint8_t *ptr = new uint8_t[size];
    CmdListMemoryCopyParams copyParams = {};
    ze_result_t result = commandList->appendMemoryFill(ptr, &pattern, 1, size, nullptr, 0, nullptr, copyParams);
    size_t filledSize = commandList->xGroupSizes[0] * commandList->threadGroupDimensions[0].groupCountX * 16;
    filledSize += commandList->xGroupSizes[1] * commandList->threadGroupDimensions[1].groupCountX;
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(2u, commandList->numberOfCallsToAppendLaunchKernelWithParams);
    EXPECT_EQ(size, filledSize);
    delete[] ptr;
}

HWTEST_F(AppendFillTest, givenAppendMemoryFillWithSizeBelowMaxWorkgroupSizeWhenPatternSizeIsOneThenDispatchOneKernel) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    int pattern = 0;
    const size_t size = neoDevice->getDeviceInfo().maxWorkGroupSize / 2;
    uint8_t *ptr = new uint8_t[size];
    CmdListMemoryCopyParams copyParams = {};
    ze_result_t result = commandList->appendMemoryFill(ptr, &pattern, 1, size, nullptr, 0, nullptr, copyParams);
    size_t filledSize = commandList->xGroupSizes[0] * commandList->threadGroupDimensions[0].groupCountX * 16;
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, commandList->numberOfCallsToAppendLaunchKernelWithParams);
    EXPECT_EQ(size, filledSize);
    delete[] ptr;
}

HWTEST_F(AppendFillTest, givenAppendMemoryFillWhenPatternSizeIsOneThenGroupCountIsCorrect) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    int pattern = 0;
    const size_t size = 1024 * 1024;
    uint8_t *ptr = new uint8_t[size];
    CmdListMemoryCopyParams copyParams = {};
    ze_result_t result = commandList->appendMemoryFill(ptr, &pattern, 1, size, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto groupSize = device->getDeviceInfo().maxWorkGroupSize;
    auto dataTypeSize = sizeof(uint32_t) * 4;
    auto expectedGroupCount = size / (dataTypeSize * groupSize);
    EXPECT_EQ(expectedGroupCount, commandList->threadGroupDimensions[0].groupCountX);
    delete[] ptr;
}

HWTEST_F(AppendFillTest, givenAppendMemoryFillWhenPtrWithOffsetAndPatternSizeIsOneThenThreeKernelsDispatched) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    int pattern = 0;
    uint32_t offset = 1;
    const size_t size = 1024;
    uint8_t *ptr = new uint8_t[size];
    CmdListMemoryCopyParams copyParams = {};
    ze_result_t result = commandList->appendMemoryFill(ptr + offset, &pattern, 1, size - offset, nullptr, 0, nullptr, copyParams);
    size_t filledSize = commandList->xGroupSizes[0] * commandList->threadGroupDimensions[0].groupCountX;
    filledSize += commandList->xGroupSizes[1] * commandList->threadGroupDimensions[1].groupCountX * 16;
    filledSize += commandList->xGroupSizes[2] * commandList->threadGroupDimensions[2].groupCountX;
    EXPECT_EQ(sizeof(uint32_t) - offset, commandList->xGroupSizes[0]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(3u, commandList->numberOfCallsToAppendLaunchKernelWithParams);
    EXPECT_EQ(size - offset, filledSize);
    delete[] ptr;
}

HWTEST_F(AppendFillTest, givenAppendMemoryFillWhenPtrWithOffsetAndSmallSizeAndPatternSizeIsOneThenTwoKernelsDispatched) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    int pattern = 0;
    uint32_t offset = 1;
    const size_t size = 2;
    uint8_t *ptr = new uint8_t[size];
    CmdListMemoryCopyParams copyParams = {};
    ze_result_t result = commandList->appendMemoryFill(ptr + offset, &pattern, 1, size - offset, nullptr, 0, nullptr, copyParams);
    size_t filledSize = commandList->xGroupSizes[0] * commandList->threadGroupDimensions[0].groupCountX * 16;
    filledSize += commandList->xGroupSizes[1] * commandList->threadGroupDimensions[1].groupCountX;
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(2u, commandList->numberOfCallsToAppendLaunchKernelWithParams);
    EXPECT_EQ(size - offset, filledSize);
    delete[] ptr;
}

HWTEST_F(AppendFillTest, givenAppendMemoryFillWhenPtrWithOffsetAndFailAppendUnalignedFillKernelThenReturnError) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<FamilyType::gfxCoreFamily>>>();
    commandList->thresholdOfCallsToAppendLaunchKernelWithParamsToFail = 0;
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    int pattern = 0;
    uint32_t offset = 1;
    const size_t size = 1024;
    uint8_t *ptr = new uint8_t[size];
    CmdListMemoryCopyParams copyParams = {};
    ze_result_t result = commandList->appendMemoryFill(ptr + offset, &pattern, 1, size - offset, nullptr, 0, nullptr, copyParams);
    EXPECT_NE(ZE_RESULT_SUCCESS, result);
    delete[] ptr;
}

HWTEST_F(AppendFillTest, givenCallToAppendMemoryFillWithSizeNotMultipleOfPatternSizeThenSuccessIsReturned) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    size_t nonMultipleSize = allocSize + 1;
    uint8_t *nonMultipleDstPtr = new uint8_t[nonMultipleSize];
    CmdListMemoryCopyParams copyParams = {};
    auto result = commandList->appendMemoryFill(nonMultipleDstPtr, pattern, 4, nonMultipleSize, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    delete[] nonMultipleDstPtr;
}

HWTEST_F(AppendFillTest, givenCallToAppendMemoryFillWithSizeNotMultipleOfPatternSizeAndAppendLaunchKernelFailureOnRemainderThenSuccessIsNotReturned) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    commandList->thresholdOfCallsToAppendLaunchKernelWithParamsToFail = 1;

    size_t nonMultipleSize = allocSize + 1;
    uint8_t *nonMultipleDstPtr = new uint8_t[nonMultipleSize];
    CmdListMemoryCopyParams copyParams = {};
    auto result = commandList->appendMemoryFill(nonMultipleDstPtr, pattern, 4, nonMultipleSize, nullptr, 0, nullptr, copyParams);
    EXPECT_NE(ZE_RESULT_SUCCESS, result);

    delete[] nonMultipleDstPtr;
}

HWTEST2_F(AppendFillTest,
          givenCallToAppendMemoryFillWithImmediateValueWhenTimestampEventUsesRegistersThenSinglePacketUsesRegisterProfiling, IsGen12LP) {
    using GfxFamily = typename NEO::GfxFamilyMapper<FamilyType::gfxCoreFamily>::GfxFamily;
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

    auto commandList = std::make_unique<WhiteBox<MockCommandList<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    CmdListMemoryCopyParams copyParams = {};
    result = commandList->appendMemoryFill(immediateDstPtr, &immediatePattern,
                                           sizeof(immediatePattern),
                                           immediateAllocSize, event->toHandle(), 0, nullptr, copyParams);
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
                                           false,
                                           true);

    validateTimestampRegisters<FamilyType>(cmdList,
                                           secondWalker,
                                           RegisterOffsets::globalTimestampLdw, globalEndAddress,
                                           RegisterOffsets::gpThreadTimeRegAddressOffsetLow, contextEndAddress,
                                           false,
                                           true);
}

HWTEST2_F(AppendFillTest,
          givenCallToAppendMemoryFillWhenTimestampEventUsesRegistersThenSinglePacketUsesRegisterProfiling, IsGen12LP) {
    using GfxFamily = typename NEO::GfxFamilyMapper<FamilyType::gfxCoreFamily>::GfxFamily;
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

    auto commandList = std::make_unique<WhiteBox<MockCommandList<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    CmdListMemoryCopyParams copyParams = {};
    result = commandList->appendMemoryFill(dstPtr, pattern, patternSize, allocSize, event->toHandle(), 0, nullptr, copyParams);
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
                                           false,
                                           true);

    validateTimestampRegisters<FamilyType>(cmdList,
                                           secondWalker,
                                           RegisterOffsets::globalTimestampLdw, globalEndAddress,
                                           RegisterOffsets::gpThreadTimeRegAddressOffsetLow, contextEndAddress,
                                           false,
                                           true);
}

} // namespace ult
} // namespace L0
