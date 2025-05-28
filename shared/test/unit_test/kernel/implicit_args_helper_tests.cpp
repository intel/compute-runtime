/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_walk_order.h"
#include "shared/source/helpers/per_thread_data.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/kernel/implicit_args_helper.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

TEST(ImplicitArgsHelperTest, whenLocalIdsAreGeneratedByRuntimeThenDimensionOrderIsTakedFromInput) {
    uint8_t inputDimensionOrder[3] = {2, 0, 1};
    for (auto i = 0u; i < HwWalkOrderHelper::walkOrderPossibilties; i++) {
        auto dimOrderForImplicitArgs = ImplicitArgsHelper::getDimensionOrderForLocalIds(inputDimensionOrder, std::make_pair(false, i));
        EXPECT_EQ(inputDimensionOrder[0], dimOrderForImplicitArgs[0]);
        EXPECT_EQ(inputDimensionOrder[1], dimOrderForImplicitArgs[1]);
        EXPECT_EQ(inputDimensionOrder[2], dimOrderForImplicitArgs[2]);
    }
    auto dimOrderForImplicitArgs = ImplicitArgsHelper::getDimensionOrderForLocalIds(inputDimensionOrder, {});
    EXPECT_EQ(inputDimensionOrder[0], dimOrderForImplicitArgs[0]);
    EXPECT_EQ(inputDimensionOrder[1], dimOrderForImplicitArgs[1]);
    EXPECT_EQ(inputDimensionOrder[2], dimOrderForImplicitArgs[2]);
}

TEST(ImplicitArgsHelperTest, givenIncorrectcInputWhenGettingDimensionOrderThenAbortIsCalled) {
    EXPECT_THROW(ImplicitArgsHelper::getDimensionOrderForLocalIds(nullptr, std::make_pair(false, 0u)), std::runtime_error);
    EXPECT_THROW(ImplicitArgsHelper::getDimensionOrderForLocalIds(nullptr, std::make_pair(true, HwWalkOrderHelper::walkOrderPossibilties)), std::runtime_error);
}

TEST(ImplicitArgsHelperTest, whenLocalIdsAreGeneratedByHwThenProperDimensionOrderIsReturned) {
    for (auto i = 0u; i < HwWalkOrderHelper::walkOrderPossibilties; i++) {
        auto dimOrderForImplicitArgs = ImplicitArgsHelper::getDimensionOrderForLocalIds(nullptr, std::make_pair(true, i));
        EXPECT_EQ(HwWalkOrderHelper::compatibleDimensionOrders[i], dimOrderForImplicitArgs);
    }
}

TEST(ImplicitArgsHelperTest, whenGettingGrfSizeForSimd1ThenSizeOfSingleLocalIdIsReturned) {
    EXPECT_EQ(3 * sizeof(uint16_t), ImplicitArgsHelper::getGrfSize(1u));
}

TEST(ImplicitArgsHelperTest, givenSimdGreaterThanOneWhenGettingGrfSizeThenGrfSize32IsReturned) {
    auto regularGrfsize = 32u;
    EXPECT_EQ(regularGrfsize, ImplicitArgsHelper::getGrfSize(8u));
    EXPECT_EQ(regularGrfsize, ImplicitArgsHelper::getGrfSize(16u));
    EXPECT_EQ(regularGrfsize, ImplicitArgsHelper::getGrfSize(32u));
}

TEST(ImplicitArgsHelperTest, givenNoImplicitArgsWhenGettingSizeForImplicitArgsProgrammingThenZeroIsReturned) {

    KernelDescriptor kernelDescriptor{};
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];
    EXPECT_EQ(0u, ImplicitArgsHelper::getSizeForImplicitArgsPatching(nullptr, kernelDescriptor, false, rootDeviceEnvironment));
}

TEST(ImplicitArgsHelperTest, givenImplicitArgsWithoutImplicitArgsBufferOffsetInPayloadMappingWhenGettingSizeForImplicitArgsProgrammingThenCorrectSizeIsReturned) {
    ImplicitArgs implicitArgs{};
    implicitArgs.v0.header.structSize = ImplicitArgsV0::getSize();
    implicitArgs.v0.header.structVersion = 0;

    KernelDescriptor kernelDescriptor{};

    EXPECT_TRUE(isUndefinedOffset<>(kernelDescriptor.payloadMappings.implicitArgs.implicitArgsBuffer));

    implicitArgs.v0.simdWidth = 32;
    implicitArgs.v0.localSizeX = 2;
    implicitArgs.v0.localSizeY = 3;
    implicitArgs.v0.localSizeZ = 4;

    auto totalWorkgroupSize = implicitArgs.v0.localSizeX * implicitArgs.v0.localSizeY * implicitArgs.v0.localSizeZ;

    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];
    auto localIdsSize = alignUp(PerThreadDataHelper::getPerThreadDataSizeTotal(implicitArgs.v0.simdWidth, 32u /* grfSize */, GrfConfig::defaultGrfNumber /* numGrf */, 3u /* num channels */, totalWorkgroupSize, rootDeviceEnvironment), MemoryConstants::cacheLineSize);
    EXPECT_EQ(localIdsSize + ImplicitArgsV0::getAlignedSize(), ImplicitArgsHelper::getSizeForImplicitArgsPatching(&implicitArgs, kernelDescriptor, false, rootDeviceEnvironment));
}

TEST(ImplicitArgsHelperTest, givenImplicitArgsWithImplicitArgsBufferOffsetInPayloadMappingWhenGettingSizeForImplicitArgsProgrammingThenCorrectSizeIsReturned) {
    ImplicitArgs implicitArgs{};
    implicitArgs.v0.header.structSize = ImplicitArgsV0::getSize();
    implicitArgs.v0.header.structVersion = 0;

    KernelDescriptor kernelDescriptor{};
    kernelDescriptor.payloadMappings.implicitArgs.implicitArgsBuffer = 0x10;
    EXPECT_TRUE(isValidOffset<>(kernelDescriptor.payloadMappings.implicitArgs.implicitArgsBuffer));

    implicitArgs.v0.simdWidth = 32;
    implicitArgs.v0.localSizeX = 2;
    implicitArgs.v0.localSizeY = 3;
    implicitArgs.v0.localSizeZ = 4;
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];
    EXPECT_EQ(alignUp(implicitArgs.v0.header.structSize, MemoryConstants::cacheLineSize), implicitArgs.getAlignedSize());
    EXPECT_EQ(alignUp(implicitArgs.v0.header.structSize, MemoryConstants::cacheLineSize), ImplicitArgsHelper::getSizeForImplicitArgsPatching(&implicitArgs, kernelDescriptor, false, rootDeviceEnvironment));
}

TEST(ImplicitArgsHelperTest, givenImplicitArgsWithoutImplicitArgsBufferOffsetInPayloadMappingWhenPatchingImplicitArgsThenOnlyProperRegionIsPatched) {
    ImplicitArgs implicitArgs{};
    implicitArgs.v0.header.structSize = ImplicitArgsV0::getSize();
    implicitArgs.v0.header.structVersion = 0;

    void *outImplicitArgs = nullptr;
    KernelDescriptor kernelDescriptor{};
    kernelDescriptor.kernelAttributes.workgroupDimensionsOrder[0] = 0;
    kernelDescriptor.kernelAttributes.workgroupDimensionsOrder[1] = 1;
    kernelDescriptor.kernelAttributes.workgroupDimensionsOrder[2] = 2;

    EXPECT_TRUE(isUndefinedOffset<>(kernelDescriptor.payloadMappings.implicitArgs.implicitArgsBuffer));

    implicitArgs.v0.simdWidth = 1;
    implicitArgs.v0.localSizeX = 2;
    implicitArgs.v0.localSizeY = 3;
    implicitArgs.v0.localSizeZ = 4;
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];
    auto totalSizeForPatching = ImplicitArgsHelper::getSizeForImplicitArgsPatching(&implicitArgs, kernelDescriptor, false, rootDeviceEnvironment);

    auto totalWorkgroupSize = implicitArgs.v0.localSizeX * implicitArgs.v0.localSizeY * implicitArgs.v0.localSizeZ;
    auto localIdsPatchingSize = totalWorkgroupSize * 3 * sizeof(uint16_t);
    auto localIdsOffset = alignUp(localIdsPatchingSize, MemoryConstants::cacheLineSize);

    auto memoryToPatch = std::make_unique<uint8_t[]>(totalSizeForPatching);

    uint8_t pattern = 0xcd;

    memset(memoryToPatch.get(), pattern, totalSizeForPatching);

    auto retVal = ImplicitArgsHelper::patchImplicitArgs(memoryToPatch.get(), implicitArgs, kernelDescriptor, {}, rootDeviceEnvironment, &outImplicitArgs);

    EXPECT_EQ(retVal, ptrOffset(memoryToPatch.get(), totalSizeForPatching));

    void *expectedImplicitArgsPtr = ptrOffset(memoryToPatch.get(), localIdsOffset);
    EXPECT_EQ(expectedImplicitArgsPtr, outImplicitArgs);

    uint32_t offset = 0;

    for (; offset < localIdsPatchingSize; offset++) {
        EXPECT_NE(pattern, memoryToPatch.get()[offset]) << offset;
    }

    for (; offset < totalSizeForPatching - ImplicitArgsV0::getAlignedSize(); offset++) {
        EXPECT_EQ(pattern, memoryToPatch.get()[offset]);
    }

    for (; offset < totalSizeForPatching - (ImplicitArgsV0::getAlignedSize() - ImplicitArgsV0::getSize()); offset++) {
        EXPECT_NE(pattern, memoryToPatch.get()[offset]);
    }
    for (; offset < totalSizeForPatching; offset++) {
        EXPECT_EQ(pattern, memoryToPatch.get()[offset]);
    }
}

TEST(ImplicitArgsHelperTest, givenImplicitArgsWithImplicitArgsBufferOffsetInPayloadMappingWhenPatchingImplicitArgsThenOnlyProperRegionIsPatched) {
    ImplicitArgs implicitArgs{};
    implicitArgs.v0.header.structSize = ImplicitArgsV0::getSize();
    implicitArgs.v0.header.structVersion = 0;

    void *outImplicitArgs = nullptr;
    KernelDescriptor kernelDescriptor{};
    kernelDescriptor.payloadMappings.implicitArgs.implicitArgsBuffer = 0x10;
    EXPECT_TRUE(isValidOffset<>(kernelDescriptor.payloadMappings.implicitArgs.implicitArgsBuffer));

    implicitArgs.v0.simdWidth = 32;
    implicitArgs.v0.localSizeX = 2;
    implicitArgs.v0.localSizeY = 3;
    implicitArgs.v0.localSizeZ = 4;
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];
    auto totalSizeForPatching = ImplicitArgsHelper::getSizeForImplicitArgsPatching(&implicitArgs, kernelDescriptor, false, rootDeviceEnvironment);

    EXPECT_EQ(ImplicitArgsV0::getAlignedSize(), totalSizeForPatching);

    auto memoryToPatch = std::make_unique<uint8_t[]>(totalSizeForPatching);

    uint8_t pattern = 0xcd;

    memset(memoryToPatch.get(), pattern, totalSizeForPatching);

    auto retVal = ImplicitArgsHelper::patchImplicitArgs(memoryToPatch.get(), implicitArgs, kernelDescriptor, {}, rootDeviceEnvironment, &outImplicitArgs);

    EXPECT_EQ(retVal, ptrOffset(memoryToPatch.get(), totalSizeForPatching));

    void *expectedImplicitArgsPtr = memoryToPatch.get();
    EXPECT_EQ(expectedImplicitArgsPtr, outImplicitArgs);

    uint32_t offset = 0;

    for (; offset < ImplicitArgsV0::getSize(); offset++) {
        EXPECT_NE(pattern, memoryToPatch.get()[offset]);
    }

    for (; offset < totalSizeForPatching; offset++) {
        EXPECT_EQ(pattern, memoryToPatch.get()[offset]);
    }
}

TEST(ImplicitArgsV0Test, givenImplicitArgsV0WhenSettingFieldsThenCorrectFieldsAreSet) {
    ImplicitArgs implicitArgs{};
    implicitArgs.v0.header.structSize = ImplicitArgsV0::getSize();
    implicitArgs.v0.header.structVersion = 0;

    EXPECT_EQ(ImplicitArgsV0::getSize(), implicitArgs.getSize());

    implicitArgs.setAssertBufferPtr(0x4567000);
    implicitArgs.setGlobalOffset(5, 6, 7);
    implicitArgs.setGlobalSize(1, 2, 3);
    implicitArgs.setGroupCount(10, 20, 30);
    implicitArgs.setLocalSize(8, 9, 11);
    implicitArgs.setLocalIdTablePtr(0x5699000);
    implicitArgs.setPrintfBuffer(0xff000);
    implicitArgs.setNumWorkDim(16);
    implicitArgs.setRtGlobalBufferPtr(0x1000123400);
    implicitArgs.setSimdWidth(32);

    EXPECT_EQ(0x4567000u, implicitArgs.v0.assertBufferPtr);

    EXPECT_EQ(5u, implicitArgs.v0.globalOffsetX);
    EXPECT_EQ(6u, implicitArgs.v0.globalOffsetY);
    EXPECT_EQ(7u, implicitArgs.v0.globalOffsetZ);

    EXPECT_EQ(1u, implicitArgs.v0.globalSizeX);
    EXPECT_EQ(2u, implicitArgs.v0.globalSizeY);
    EXPECT_EQ(3u, implicitArgs.v0.globalSizeZ);

    EXPECT_EQ(10u, implicitArgs.v0.groupCountX);
    EXPECT_EQ(20u, implicitArgs.v0.groupCountY);
    EXPECT_EQ(30u, implicitArgs.v0.groupCountZ);

    EXPECT_EQ(8u, implicitArgs.v0.localSizeX);
    EXPECT_EQ(9u, implicitArgs.v0.localSizeY);
    EXPECT_EQ(11u, implicitArgs.v0.localSizeZ);

    EXPECT_EQ(0x5699000u, implicitArgs.v0.localIdTablePtr);
    EXPECT_EQ(0xff000u, implicitArgs.v0.printfBufferPtr);
    EXPECT_EQ(16u, implicitArgs.v0.numWorkDim);
    EXPECT_EQ(0x1000123400u, implicitArgs.v0.rtGlobalBufferPtr);
}

TEST(ImplicitArgsV1Test, givenImplicitArgsV1WhenSettingFieldsThenCorrectFieldsAreSet) {
    ImplicitArgs implicitArgs{};
    implicitArgs.v1.header.structSize = ImplicitArgsV1::getSize();
    implicitArgs.v1.header.structVersion = 1;

    EXPECT_EQ(ImplicitArgsV1::getSize(), implicitArgs.getSize());

    implicitArgs.setAssertBufferPtr(0x4567000);
    implicitArgs.setGlobalOffset(5, 6, 7);
    implicitArgs.setGlobalSize(1, 2, 3);
    implicitArgs.setGroupCount(10, 20, 30);
    implicitArgs.setLocalSize(8, 9, 11);
    implicitArgs.setLocalIdTablePtr(0x5699000);
    implicitArgs.setPrintfBuffer(0xff000);
    implicitArgs.setNumWorkDim(16);
    implicitArgs.setRtGlobalBufferPtr(0x1000123400);
    implicitArgs.setSimdWidth(32);

    EXPECT_EQ(0x4567000u, implicitArgs.v1.assertBufferPtr);

    EXPECT_EQ(5u, implicitArgs.v1.globalOffsetX);
    EXPECT_EQ(6u, implicitArgs.v1.globalOffsetY);
    EXPECT_EQ(7u, implicitArgs.v1.globalOffsetZ);

    EXPECT_EQ(1u, implicitArgs.v1.globalSizeX);
    EXPECT_EQ(2u, implicitArgs.v1.globalSizeY);
    EXPECT_EQ(3u, implicitArgs.v1.globalSizeZ);

    EXPECT_EQ(10u, implicitArgs.v1.groupCountX);
    EXPECT_EQ(20u, implicitArgs.v1.groupCountY);
    EXPECT_EQ(30u, implicitArgs.v1.groupCountZ);

    EXPECT_EQ(8u, implicitArgs.v1.localSizeX);
    EXPECT_EQ(9u, implicitArgs.v1.localSizeY);
    EXPECT_EQ(11u, implicitArgs.v1.localSizeZ);

    EXPECT_EQ(0x5699000u, implicitArgs.v1.localIdTablePtr);
    EXPECT_EQ(0xff000u, implicitArgs.v1.printfBufferPtr);
    EXPECT_EQ(16u, implicitArgs.v1.numWorkDim);
    EXPECT_EQ(0x1000123400u, implicitArgs.v1.rtGlobalBufferPtr);
}

TEST(ImplicitArgsV1Test, givenImplicitArgsWithUnknownVersionWhenSettingFieldsThenFieldsAreNotPopulated) {
    ImplicitArgs implicitArgs{};

    memset(&implicitArgs, 0, sizeof(implicitArgs));

    implicitArgs.v1.header.structSize = ImplicitArgsV1::getSize();
    implicitArgs.v1.header.structVersion = 2; // unknown version

    EXPECT_EQ(0u, implicitArgs.getSize());

    implicitArgs.setAssertBufferPtr(0x4567000);
    implicitArgs.setGlobalOffset(5, 6, 7);
    implicitArgs.setGlobalSize(1, 2, 3);
    implicitArgs.setGroupCount(10, 20, 30);
    implicitArgs.setLocalSize(8, 9, 11);
    implicitArgs.setLocalIdTablePtr(0x5699000);
    implicitArgs.setPrintfBuffer(0xff000);
    implicitArgs.setNumWorkDim(16);
    implicitArgs.setRtGlobalBufferPtr(0x1000123400);
    implicitArgs.setSimdWidth(32);

    EXPECT_EQ(0u, implicitArgs.v1.assertBufferPtr);

    EXPECT_EQ(0u, implicitArgs.v1.globalOffsetX);
    EXPECT_EQ(0u, implicitArgs.v1.globalOffsetY);
    EXPECT_EQ(0u, implicitArgs.v1.globalOffsetZ);

    EXPECT_EQ(0u, implicitArgs.v1.globalSizeX);
    EXPECT_EQ(0u, implicitArgs.v1.globalSizeY);
    EXPECT_EQ(0u, implicitArgs.v1.globalSizeZ);

    EXPECT_EQ(0u, implicitArgs.v1.groupCountX);
    EXPECT_EQ(0u, implicitArgs.v1.groupCountY);
    EXPECT_EQ(0u, implicitArgs.v1.groupCountZ);

    EXPECT_EQ(0u, implicitArgs.v1.localSizeX);
    EXPECT_EQ(0u, implicitArgs.v1.localSizeY);
    EXPECT_EQ(0u, implicitArgs.v1.localSizeZ);

    EXPECT_EQ(0u, implicitArgs.v1.localIdTablePtr);
    EXPECT_EQ(0u, implicitArgs.v1.printfBufferPtr);
    EXPECT_EQ(0u, implicitArgs.v1.numWorkDim);
    EXPECT_EQ(0u, implicitArgs.v1.rtGlobalBufferPtr);
}
