/*
 * Copyright (C) 2022-2024 Intel Corporation
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
        auto dimOrderForImplicitArgs = ImplicitArgsHelper::getDimensionOrderForLocalIds(inputDimensionOrder, std::make_pair(true, i));
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
    EXPECT_THROW(ImplicitArgsHelper::getDimensionOrderForLocalIds(nullptr, std::make_pair(true, 0u)), std::runtime_error);
    EXPECT_THROW(ImplicitArgsHelper::getDimensionOrderForLocalIds(nullptr, std::make_pair(false, HwWalkOrderHelper::walkOrderPossibilties)), std::runtime_error);
}

TEST(ImplicitArgsHelperTest, whenLocalIdsAreGeneratedByHwThenProperDimensionOrderIsReturned) {
    for (auto i = 0u; i < HwWalkOrderHelper::walkOrderPossibilties; i++) {
        auto dimOrderForImplicitArgs = ImplicitArgsHelper::getDimensionOrderForLocalIds(nullptr, std::make_pair(false, i));
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
    ImplicitArgs implicitArgs{ImplicitArgs::getSize()};

    KernelDescriptor kernelDescriptor{};

    EXPECT_TRUE(isUndefinedOffset<>(kernelDescriptor.payloadMappings.implicitArgs.implicitArgsBuffer));

    implicitArgs.simdWidth = 32;
    implicitArgs.localSizeX = 2;
    implicitArgs.localSizeY = 3;
    implicitArgs.localSizeZ = 4;

    auto totalWorkgroupSize = implicitArgs.localSizeX * implicitArgs.localSizeY * implicitArgs.localSizeZ;

    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];
    auto localIdsSize = alignUp(PerThreadDataHelper::getPerThreadDataSizeTotal(implicitArgs.simdWidth, 32u /* grfSize */, GrfConfig::defaultGrfNumber /* numGrf */, 3u /* num channels */, totalWorkgroupSize, false, rootDeviceEnvironment), MemoryConstants::cacheLineSize);
    EXPECT_EQ(localIdsSize + ImplicitArgs::getSize(), ImplicitArgsHelper::getSizeForImplicitArgsPatching(&implicitArgs, kernelDescriptor, false, rootDeviceEnvironment));
}

TEST(ImplicitArgsHelperTest, givenImplicitArgsWithImplicitArgsBufferOffsetInPayloadMappingWhenGettingSizeForImplicitArgsProgrammingThenCorrectSizeIsReturned) {
    ImplicitArgs implicitArgs{ImplicitArgs::getSize()};

    KernelDescriptor kernelDescriptor{};
    kernelDescriptor.payloadMappings.implicitArgs.implicitArgsBuffer = 0x10;
    EXPECT_TRUE(isValidOffset<>(kernelDescriptor.payloadMappings.implicitArgs.implicitArgsBuffer));

    implicitArgs.simdWidth = 32;
    implicitArgs.localSizeX = 2;
    implicitArgs.localSizeY = 3;
    implicitArgs.localSizeZ = 4;
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];
    EXPECT_EQ(alignUp(implicitArgs.structSize, MemoryConstants::cacheLineSize), ImplicitArgsHelper::getSizeForImplicitArgsPatching(&implicitArgs, kernelDescriptor, false, rootDeviceEnvironment));
}

TEST(ImplicitArgsHelperTest, givenImplicitArgsWithoutImplicitArgsBufferOffsetInPayloadMappingWhenPatchingImplicitArgsThenOnlyProperRegionIsPatched) {
    ImplicitArgs implicitArgs{ImplicitArgs::getSize()};

    void *outImplicitArgs = nullptr;
    KernelDescriptor kernelDescriptor{};
    kernelDescriptor.kernelAttributes.workgroupDimensionsOrder[0] = 0;
    kernelDescriptor.kernelAttributes.workgroupDimensionsOrder[1] = 1;
    kernelDescriptor.kernelAttributes.workgroupDimensionsOrder[2] = 2;

    EXPECT_TRUE(isUndefinedOffset<>(kernelDescriptor.payloadMappings.implicitArgs.implicitArgsBuffer));

    implicitArgs.simdWidth = 1;
    implicitArgs.localSizeX = 2;
    implicitArgs.localSizeY = 3;
    implicitArgs.localSizeZ = 4;
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];
    auto totalSizeForPatching = ImplicitArgsHelper::getSizeForImplicitArgsPatching(&implicitArgs, kernelDescriptor, false, rootDeviceEnvironment);

    auto totalWorkgroupSize = implicitArgs.localSizeX * implicitArgs.localSizeY * implicitArgs.localSizeZ;
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

    for (; offset < totalSizeForPatching - ImplicitArgs::getSize(); offset++) {
        EXPECT_EQ(pattern, memoryToPatch.get()[offset]);
    }

    for (; offset < totalSizeForPatching; offset++) {
        EXPECT_NE(pattern, memoryToPatch.get()[offset]);
    }
}

TEST(ImplicitArgsHelperTest, givenImplicitArgsWithImplicitArgsBufferOffsetInPayloadMappingWhenPatchingImplicitArgsThenOnlyProperRegionIsPatched) {
    ImplicitArgs implicitArgs{ImplicitArgs::getSize()};
    void *outImplicitArgs = nullptr;
    KernelDescriptor kernelDescriptor{};
    kernelDescriptor.payloadMappings.implicitArgs.implicitArgsBuffer = 0x10;
    EXPECT_TRUE(isValidOffset<>(kernelDescriptor.payloadMappings.implicitArgs.implicitArgsBuffer));

    implicitArgs.simdWidth = 32;
    implicitArgs.localSizeX = 2;
    implicitArgs.localSizeY = 3;
    implicitArgs.localSizeZ = 4;
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];
    auto totalSizeForPatching = ImplicitArgsHelper::getSizeForImplicitArgsPatching(&implicitArgs, kernelDescriptor, false, rootDeviceEnvironment);

    EXPECT_EQ(alignUp(ImplicitArgs::getSize(), MemoryConstants::cacheLineSize), totalSizeForPatching);

    auto memoryToPatch = std::make_unique<uint8_t[]>(totalSizeForPatching);

    uint8_t pattern = 0xcd;

    memset(memoryToPatch.get(), pattern, totalSizeForPatching);

    auto retVal = ImplicitArgsHelper::patchImplicitArgs(memoryToPatch.get(), implicitArgs, kernelDescriptor, {}, rootDeviceEnvironment, &outImplicitArgs);

    EXPECT_EQ(retVal, ptrOffset(memoryToPatch.get(), totalSizeForPatching));

    void *expectedImplicitArgsPtr = memoryToPatch.get();
    EXPECT_EQ(expectedImplicitArgsPtr, outImplicitArgs);

    uint32_t offset = 0;

    for (; offset < ImplicitArgs::getSize(); offset++) {
        EXPECT_NE(pattern, memoryToPatch.get()[offset]);
    }

    for (; offset < totalSizeForPatching; offset++) {
        EXPECT_EQ(pattern, memoryToPatch.get()[offset]);
    }
}
