/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/builtin/builtin_functions_lib.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.inl"
#include "level_zero/core/test/unit_tests/mocks/mock_builtin_functions_lib_impl.h"

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

HWTEST_F(AppendFillTest, givenZeroCopySvmAllocationWhenAppendMemoryFillCalledThenDestinationIsTreatedAsSystemMemory) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    driverHandle->allocationTypeToReturn = NEO::AllocationType::svmZeroCopy;

    CmdListMemoryCopyParams copyParams = {};
    auto result = commandList->appendMemoryFill(dstPtr, pattern, patternSize, allocSize, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);
}

HWTEST_F(AppendFillTest, givenDeviceStorageSvmAllocationWhenAppendMemoryFillCalledThenDestinationIsNotTreatedAsSystemMemory) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    driverHandle->allocationTypeToReturn = NEO::AllocationType::svmGpu;

    CmdListMemoryCopyParams copyParams = {};
    auto result = commandList->appendMemoryFill(dstPtr, pattern, patternSize, allocSize, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);
}

HWTEST_F(AppendFillTest, givenZeroPatternSizeWhenAppendMemoryFillCalledThenInvalidSizeIsReturned) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    CmdListMemoryCopyParams copyParams = {};
    auto result = commandList->appendMemoryFill(dstPtr, pattern, 0u, allocSize, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_SIZE, result);
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
    const size_t size = 4 * MemoryConstants::kiloByte;
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

HWTEST_F(AppendFillTest, givenAppendMemoryFillWithSharedSystemUsmAndDebugFlagSetToDisableThenReturnError) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableSharedSystemUsmSupport.set(0);
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
    const size_t size = 32 * device->getDeviceInfo().maxWorkGroupSize;
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

struct RecordingFillBuiltInKernelLib : MockBuiltInKernelLibImpl {
    static constexpr uint32_t fillDstOffsetArgIndex = 1;
    static constexpr uint32_t fillPatternSizeArgIndex = 3;
    static constexpr uint32_t fillArgCount = fillPatternSizeArgIndex + 1;

    RecordingFillBuiltInKernelLib(L0::Device *device, NEO::BuiltIns *builtInsLib) : MockBuiltInKernelLibImpl(device, builtInsLib) {
        auto recordingKernel = static_cast<Mock<::L0::KernelImp> *>(dummyKernel.get());
        recordingKernel->checkPassedArgumentValues = true;
        recordingKernel->passedArgumentValues.resize(fillArgCount);
    }

    Kernel *getFunction(BufferBuiltIn func, const NEO::BuiltIn::AddressingMode &mode) override {
        requests.push_back({func, mode});
        return dummyKernel.get();
    }

    std::vector<NEO::BuiltIn::AddressingMode> modesOf(BufferBuiltIn func) const {
        std::vector<NEO::BuiltIn::AddressingMode> modes;
        for (const auto &request : requests) {
            if (request.first == func) {
                modes.push_back(request.second);
            }
        }
        return modes;
    }

    std::vector<std::pair<BufferBuiltIn, NEO::BuiltIn::AddressingMode>> requests;
};

template <GFXCORE_FAMILY gfxCoreFamily>
class MockCommandListRecordingFillDispatches : public WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> {
  public:
    ze_result_t appendLaunchKernelSplit(::L0::Kernel *kernel,
                                        const ze_group_count_t &threadGroupDimensions,
                                        ::L0::Event *event,
                                        CmdListKernelLaunchParams &launchParams) override {
        groupCounts.push_back(threadGroupDimensions.groupCountX);
        groupSizes.push_back(kernel->getGroupSize()[0]);
        dstOffsetArgs.push_back(static_cast<Mock<::L0::KernelImp> *>(kernel)->passedArgumentValues[RecordingFillBuiltInKernelLib::fillDstOffsetArgIndex]);
        return ZE_RESULT_SUCCESS;
    }

    std::vector<uint32_t> groupCounts;
    std::vector<uint32_t> groupSizes;
    std::vector<std::vector<uint8_t>> dstOffsetArgs;
};

struct AppendFillRecordingTest : AppendFillTest {
    static constexpr uintptr_t alignedFillDstAddress = 0x800000000 + 256;
    static constexpr size_t immediateFillMiddleElSize = sizeof(uint32_t) * 4;

    void SetUp() override {
        AppendFillTest::SetUp();
        auto recordingLib = std::make_unique<RecordingFillBuiltInKernelLib>(device, device->getNEODevice()->getBuiltIns());
        builtIns = recordingLib.get();
        builtInsBackup = std::make_unique<NonCopyableVariableBackup<std::unique_ptr<BuiltInKernelLib>>>(&device->builtins, std::move(recordingLib));
    }

    void TearDown() override {
        builtInsBackup.reset();
        AppendFillTest::TearDown();
    }

    template <typename FamilyType>
    auto appendFill(void *ptr, const void *pattern, size_t patternSize, size_t size) {
        auto commandList = std::make_unique<MockCommandListRecordingFillDispatches<FamilyType::gfxCoreFamily>>();
        commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
        CmdListMemoryCopyParams copyParams = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendMemoryFill(ptr, pattern, patternSize, size, nullptr, 0, nullptr, copyParams));
        return commandList;
    }

    RecordingFillBuiltInKernelLib *builtIns = nullptr;
    std::unique_ptr<NonCopyableVariableBackup<std::unique_ptr<BuiltInKernelLib>>> builtInsBackup;
};

HWTEST_F(AppendFillRecordingTest, givenOneBytePatternFillAbove16GBAtUnalignedPtrWhenAppendMemoryFillCalledThenRightLeftOverIsWide) {
    constexpr uint64_t dstOffset = 1;
    constexpr size_t leftLeftOverSize = sizeof(uint32_t) - dstOffset;
    constexpr size_t mainFillSize = 16 * MemoryConstants::gigaByte;
    constexpr size_t rightLeftOverSize = immediateFillMiddleElSize - 1;
    constexpr size_t size = leftLeftOverSize + mainFillSize + rightLeftOverSize;
    void *dstPtr = reinterpret_cast<void *>(alignedFillDstAddress + dstOffset);
    uint8_t onePattern = 0x11;

    auto commandList = appendFill<FamilyType>(dstPtr, &onePattern, sizeof(onePattern), size);
    ASSERT_EQ(3u, commandList->groupCounts.size());
    EXPECT_EQ(toArgumentBytes(uint64_t{dstOffset + leftLeftOverSize + mainFillSize}), commandList->dstOffsetArgs[2]);

    auto mainModes = builtIns->modesOf(BufferBuiltIn::fillBufferImmediate);
    ASSERT_EQ(1u, mainModes.size());
    EXPECT_TRUE(mainModes[0].wideMode);

    auto leftOverModes = builtIns->modesOf(BufferBuiltIn::fillBufferImmediateLeftOver);
    ASSERT_EQ(2u, leftOverModes.size());
    EXPECT_FALSE(leftOverModes[0].wideMode);
    EXPECT_TRUE(leftOverModes[1].wideMode);
}

HWTEST2_F(AppendFillRecordingTest, givenStatefulBuiltInsAndOneBytePatternFillOf4GBPlusOneAtUnalignedPtrWhenAppendMemoryFillCalledThenRightLeftOverIsWideStateless, IsAtMostXeHpgCore) {
    constexpr uint64_t dstOffset = 1;
    constexpr size_t size = NEO::BuiltIn::AddressingMode::wideModeThreshold + 1;
    void *dstPtr = reinterpret_cast<void *>(alignedFillDstAddress + dstOffset);
    uint8_t onePattern = 0x11;

    auto commandList = appendFill<FamilyType>(dstPtr, &onePattern, sizeof(onePattern), size);
    ASSERT_NE(NEO::BuiltIn::AddressingMode::BufferMode::stateless, commandList->defaultBuiltInMode.bufferMode);

    auto leftOverModes = builtIns->modesOf(BufferBuiltIn::fillBufferImmediateLeftOver);
    ASSERT_EQ(2u, leftOverModes.size());
    EXPECT_EQ(commandList->defaultBuiltInMode, leftOverModes[0]);
    EXPECT_TRUE(leftOverModes[1].wideMode);
    EXPECT_EQ(NEO::BuiltIn::AddressingMode::BufferMode::stateless, leftOverModes[1].bufferMode);
}

HWTEST_F(AppendFillRecordingTest, givenSixteenBytePatternFillOf17GBWhenAppendMemoryFillCalledThenMainDispatchCoversFullSize) {
    constexpr size_t size = 17 * MemoryConstants::gigaByte;
    void *dstPtr = reinterpret_cast<void *>(alignedFillDstAddress);
    uint8_t pattern[immediateFillMiddleElSize] = {};
    constexpr uint64_t workItems = size / sizeof(uint32_t);

    auto commandList = appendFill<FamilyType>(dstPtr, pattern, sizeof(pattern), size);
    ASSERT_EQ(1u, commandList->groupCounts.size());
    auto middleModes = builtIns->modesOf(BufferBuiltIn::fillBufferMiddle);
    ASSERT_EQ(1u, middleModes.size());
    EXPECT_TRUE(middleModes[0].wideMode);

    const uint64_t dispatchedWorkItems = static_cast<uint64_t>(commandList->groupCounts[0]) * commandList->groupSizes[0];
    EXPECT_EQ(workItems, dispatchedWorkItems);
}

HWTEST_F(AppendFillRecordingTest, givenSixteenBytePatternFillAbove2To32ElementsWithElementRemainderWhenAppendMemoryFillCalledThenRightLeftOverFitsInOneGroup) {
    const size_t maxWgSize = device->getDeviceInfo().maxWorkGroupSize;
    const size_t mainGroupSize = maxWgSize / sizeof(uint32_t);
    const size_t tailElementsLargerThanMaxWgSizeInBytes = mainGroupSize + 1;
    const size_t rightLeftOverSize = (tailElementsLargerThanMaxWgSizeInBytes % mainGroupSize) * sizeof(uint32_t);
    const size_t mainFillSize = 16 * MemoryConstants::gigaByte;
    const size_t size = mainFillSize + tailElementsLargerThanMaxWgSizeInBytes * sizeof(uint32_t);
    void *dstPtr = reinterpret_cast<void *>(alignedFillDstAddress);
    uint8_t pattern[immediateFillMiddleElSize] = {};

    auto commandList = appendFill<FamilyType>(dstPtr, pattern, sizeof(pattern), size);
    ASSERT_EQ(2u, commandList->groupCounts.size());
    ASSERT_EQ(1u, builtIns->modesOf(BufferBuiltIn::fillBufferRightLeftover).size());

    EXPECT_EQ(size - rightLeftOverSize, static_cast<uint64_t>(commandList->groupCounts[0]) * commandList->groupSizes[0] * sizeof(uint32_t));
    EXPECT_EQ(1u, commandList->groupCounts[1]);
    EXPECT_EQ(rightLeftOverSize, commandList->groupSizes[1]);
    EXPECT_EQ(toArgumentBytes(static_cast<uint64_t>(size - rightLeftOverSize)), commandList->dstOffsetArgs[1]);
}

HWTEST_F(AppendFillRecordingTest, givenSixteenBytePatternFillOf8GBPlusTwoAtAlignedPtrWhenAppendMemoryFillCalledThenRightLeftOverGetsFull64BitOffset) {
    constexpr size_t mainFillSize = 8 * MemoryConstants::gigaByte;
    constexpr size_t rightLeftOverSize = 2;
    constexpr size_t size = mainFillSize + rightLeftOverSize;
    void *dstPtr = reinterpret_cast<void *>(alignedFillDstAddress);
    uint8_t pattern[immediateFillMiddleElSize] = {};

    auto commandList = appendFill<FamilyType>(dstPtr, pattern, sizeof(pattern), size);
    ASSERT_EQ(2u, commandList->groupCounts.size());

    auto rightLeftOverModes = builtIns->modesOf(BufferBuiltIn::fillBufferRightLeftover);
    ASSERT_EQ(1u, rightLeftOverModes.size());
    EXPECT_TRUE(rightLeftOverModes[0].wideMode);

    EXPECT_EQ(mainFillSize, static_cast<uint64_t>(commandList->groupCounts[0]) * commandList->groupSizes[0] * sizeof(uint32_t));
    EXPECT_EQ(1u, commandList->groupCounts[1]);
    EXPECT_EQ(rightLeftOverSize, commandList->groupSizes[1]);
    EXPECT_EQ(toArgumentBytes(uint64_t{mainFillSize}), commandList->dstOffsetArgs[1]);
}

HWTEST_F(AppendFillRecordingTest, givenEightBytePatternFillOfTenBytesAtOffsetInImportedHostPtrWhenAppendMemoryFillCalledThenRightLeftOverOffsetIncludesBaseOffset) {
    constexpr size_t size = 10;
    constexpr uint64_t dstOffset = 4;
    constexpr size_t mainFillSize = size - size % sizeof(uint32_t);
    constexpr size_t rightLeftOverSize = size % sizeof(uint32_t);
    void *importedPtr = reinterpret_cast<void *>(alignedFillDstAddress);
    void *dstPtr = ptrOffset(importedPtr, dstOffset);
    uint8_t pattern[8] = {};
    driverHandle->forceFalseFromfindAllocationDataForRange = true;
    ASSERT_EQ(ZE_RESULT_SUCCESS, driverHandle->importExternalPointer(importedPtr, MemoryConstants::pageSize));

    auto commandList = appendFill<FamilyType>(dstPtr, pattern, sizeof(pattern), size);
    ASSERT_EQ(2u, commandList->groupCounts.size());
    ASSERT_EQ(1u, builtIns->modesOf(BufferBuiltIn::fillBufferMiddle).size());
    ASSERT_EQ(1u, builtIns->modesOf(BufferBuiltIn::fillBufferRightLeftover).size());

    EXPECT_EQ(mainFillSize, static_cast<uint64_t>(commandList->groupCounts[0]) * commandList->groupSizes[0] * sizeof(uint32_t));
    EXPECT_EQ(toArgumentBytes(uint64_t{dstOffset}), commandList->dstOffsetArgs[0]);

    EXPECT_EQ(1u, commandList->groupCounts[1]);
    EXPECT_EQ(rightLeftOverSize, commandList->groupSizes[1]);
    EXPECT_EQ(toArgumentBytes(uint64_t{dstOffset + mainFillSize}), commandList->dstOffsetArgs[1]);

    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->releaseImportedPointer(importedPtr));
}

HWTEST_F(AppendFillRecordingTest, givenTwoBytePatternFillOf4GBPlusOneAtUnalignedPtrWhenAppendMemoryFillCalledThenDispatchesCoverFullSizeWith64BitOffsets) {
    constexpr size_t size = NEO::BuiltIn::AddressingMode::wideModeThreshold + 1;
    constexpr uint64_t dstOffset = 1;
    void *dstPtr = reinterpret_cast<void *>(alignedFillDstAddress + dstOffset);
    uint8_t pattern[2] = {0x11, 0x22};
    const uint64_t maxWgSize = device->getDeviceInfo().maxWorkGroupSize;

    auto commandList = appendFill<FamilyType>(dstPtr, pattern, sizeof(pattern), size);
    ASSERT_EQ(2u, commandList->groupCounts.size());

    auto rightLeftOverModes = builtIns->modesOf(BufferBuiltIn::fillBufferRightLeftover);
    ASSERT_EQ(2u, rightLeftOverModes.size());
    for (const auto &mode : rightLeftOverModes) {
        EXPECT_TRUE(mode.wideMode);
    }

    const uint64_t mainBytes = static_cast<uint64_t>(commandList->groupCounts[0]) * commandList->groupSizes[0];
    EXPECT_EQ(maxWgSize, commandList->groupSizes[0]);
    EXPECT_EQ(toArgumentBytes(uint64_t{dstOffset}), commandList->dstOffsetArgs[0]);

    EXPECT_EQ(1u, commandList->groupCounts[1]);
    EXPECT_EQ(size - mainBytes, commandList->groupSizes[1]);
    EXPECT_LT(commandList->groupSizes[1], maxWgSize);
    EXPECT_EQ(toArgumentBytes(uint64_t{dstOffset + mainBytes}), commandList->dstOffsetArgs[1]);
}

HWTEST_F(AppendFillTest, givenCallToAppendMemoryFillWithDataSizeNotAlignedToSizeOfFillDataAndAppendLaunchKernelFailureOnRemainderThenSuccessIsNotReturned) {
    auto commandList = std::make_unique<WhiteBox<MockCommandList<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    commandList->thresholdOfCallsToAppendLaunchKernelWithParamsToFail = 1;

    // allocSize is a multiple of the pattern size, but not of the fill data size, so a remainder kernel is dispatched after the main one
    constexpr size_t twoBytePatternSize = 2;
    static_assert(allocSize % twoBytePatternSize == 0);
    static_assert(allocSize % sizeof(uint32_t) != 0);

    CmdListMemoryCopyParams copyParams = {};
    auto result = commandList->appendMemoryFill(dstPtr, pattern, twoBytePatternSize, allocSize, nullptr, 0, nullptr, copyParams);
    EXPECT_NE(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(AppendFillTest,
          givenCallToAppendMemoryFillWithImmediateValueWhenTimestampEventUsesRegistersThenSinglePacketUsesRegisterProfiling, IsGen12LP) {
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));

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
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));

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

HWTEST_F(AppendFillTest, givenInvalidExtWhenAppendMemoryFillWithParametersCalledThenErrorIsReturned) {
    MockCommandList<FamilyType::gfxCoreFamily> commandList;
    commandList.initialize(device, NEO::EngineGroupType::copy, 0u);

    uint32_t dstBuffer = 0;
    uint8_t pattern = 1;
    ze_base_desc_t desc{};
    CmdListMemoryCopyParams memoryCopyParams{};
    ze_result_t result = commandList.appendMemoryFillWithParameters(&dstBuffer, &pattern, sizeof(pattern), sizeof(dstBuffer), &desc, nullptr, 0, nullptr, memoryCopyParams);
    EXPECT_NE(ZE_RESULT_SUCCESS, result);
}

} // namespace ult
} // namespace L0
