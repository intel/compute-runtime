/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/kernel_helpers.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

struct KernelHelperMaxWorkGroupsTests : ::testing::Test {
    uint32_t simd = 8;
    uint32_t threadCount = 8 * 1024;
    uint32_t dssCount = 16;
    uint32_t availableSlm = 64 * MemoryConstants::kiloByte;
    uint32_t usedSlm = 0;
    uint32_t maxBarrierCount = 32;
    uint32_t numberOfBarriers = 0;
    uint32_t workDim = 3;
    size_t lws[3] = {10, 10, 10};

    uint32_t getMaxWorkGroupCount() {
        return KernelHelper::getMaxWorkGroupCount(simd, threadCount, dssCount, availableSlm, usedSlm,
                                                  maxBarrierCount, numberOfBarriers, workDim, lws);
    }
};

TEST_F(KernelHelperMaxWorkGroupsTests, GivenNoBarriersOrSlmUsedWhenCalculatingMaxWorkGroupsCountThenResultIsCalculatedWithSimd) {
    auto workGroupSize = lws[0] * lws[1] * lws[2];
    auto expected = threadCount / Math::divideAndRoundUp(workGroupSize, simd);
    EXPECT_EQ(expected, getMaxWorkGroupCount());
}

TEST_F(KernelHelperMaxWorkGroupsTests, GivenDebugFlagSetWhenGetMaxWorkGroupCountCalledThenReturnCorrectValue) {
    DebugManagerStateRestore restore;
    debugManager.flags.OverrideMaxWorkGroupCount.set(123);

    EXPECT_EQ(123u, getMaxWorkGroupCount());
}

TEST_F(KernelHelperMaxWorkGroupsTests, GivenBarriersWhenCalculatingMaxWorkGroupsCountThenResultIsCalculatedWithRegardToBarriersCount) {
    numberOfBarriers = 16;

    auto expected = dssCount * (maxBarrierCount / numberOfBarriers);
    EXPECT_EQ(expected, getMaxWorkGroupCount());
}

TEST_F(KernelHelperMaxWorkGroupsTests, GivenUsedSlmSizeWhenCalculatingMaxWorkGroupsCountThenResultIsCalculatedWithRegardToUsedSlmSize) {
    usedSlm = 4 * MemoryConstants::kiloByte;

    auto expected = availableSlm / usedSlm;
    EXPECT_EQ(expected, getMaxWorkGroupCount());
}

TEST_F(KernelHelperMaxWorkGroupsTests, GivenVariousValuesWhenCalculatingMaxWorkGroupsCountThenLowestResultIsAlwaysReturned) {
    usedSlm = 1 * MemoryConstants::kiloByte;
    numberOfBarriers = 1;
    dssCount = 1;

    workDim = 1;
    lws[0] = simd;
    threadCount = 1;
    EXPECT_EQ(1u, getMaxWorkGroupCount());

    threadCount = 1024;
    EXPECT_NE(1u, getMaxWorkGroupCount());

    numberOfBarriers = 32;
    EXPECT_EQ(1u, getMaxWorkGroupCount());

    numberOfBarriers = 1;
    EXPECT_NE(1u, getMaxWorkGroupCount());

    usedSlm = availableSlm;
    EXPECT_EQ(1u, getMaxWorkGroupCount());
}

using KernelHelperTest = Test<DeviceFixture>;

TEST_F(KernelHelperTest, GivenStatelessPrivateSizeGreaterThanGlobalSizeWhenCheckingIfThereIsEnaughSpaceThenOutOfMemReturned) {
    auto globalSize = pDevice->getDeviceInfo().globalMemSize;
    KernelDescriptor::KernelAttributes attributes = {};
    attributes.perHwThreadPrivateMemorySize = (static_cast<uint32_t>((globalSize + pDevice->getDeviceInfo().computeUnitsUsedForScratch) / pDevice->getDeviceInfo().computeUnitsUsedForScratch)) + 100;
    EXPECT_EQ(KernelHelper::checkIfThereIsSpaceForScratchOrPrivate(attributes, pDevice), KernelHelper::ErrorCode::OUT_OF_DEVICE_MEMORY);
}

TEST_F(KernelHelperTest, GivenScratchSizeGreaterThanGlobalSizeWhenCheckingIfThereIsEnaughSpaceThenOutOfMemReturned) {
    auto globalSize = pDevice->getDeviceInfo().globalMemSize;
    KernelDescriptor::KernelAttributes attributes = {};
    attributes.perThreadScratchSize[0] = (static_cast<uint32_t>((globalSize + pDevice->getDeviceInfo().computeUnitsUsedForScratch) / pDevice->getDeviceInfo().computeUnitsUsedForScratch)) + 100;
    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    if (attributes.perThreadScratchSize[0] > gfxCoreHelper.getMaxScratchSize()) {
        EXPECT_EQ(KernelHelper::checkIfThereIsSpaceForScratchOrPrivate(attributes, pDevice), KernelHelper::ErrorCode::INVALID_KERNEL);
    } else {
        EXPECT_EQ(KernelHelper::checkIfThereIsSpaceForScratchOrPrivate(attributes, pDevice), KernelHelper::ErrorCode::OUT_OF_DEVICE_MEMORY);
    }
}

TEST_F(KernelHelperTest, GivenScratchPrivateSizeGreaterThanGlobalSizeWhenCheckingIfThereIsEnaughSpaceThenOutOfMemReturned) {
    auto globalSize = pDevice->getDeviceInfo().globalMemSize;
    KernelDescriptor::KernelAttributes attributes = {};
    attributes.perThreadScratchSize[1] = (static_cast<uint32_t>((globalSize + pDevice->getDeviceInfo().computeUnitsUsedForScratch) / pDevice->getDeviceInfo().computeUnitsUsedForScratch)) + 100;
    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    if (attributes.perThreadScratchSize[1] > gfxCoreHelper.getMaxScratchSize()) {
        EXPECT_EQ(KernelHelper::checkIfThereIsSpaceForScratchOrPrivate(attributes, pDevice), KernelHelper::ErrorCode::INVALID_KERNEL);
    } else {
        EXPECT_EQ(KernelHelper::checkIfThereIsSpaceForScratchOrPrivate(attributes, pDevice), KernelHelper::ErrorCode::OUT_OF_DEVICE_MEMORY);
    }
}

TEST_F(KernelHelperTest, GivenScratchAndPrivateSizeLessThanGlobalSizeWhenCheckingIfThereIsEnaughSpaceThenSuccessReturned) {
    auto globalSize = pDevice->getDeviceInfo().globalMemSize;
    KernelDescriptor::KernelAttributes attributes = {};
    auto size = (static_cast<uint32_t>((globalSize + pDevice->getDeviceInfo().computeUnitsUsedForScratch) / pDevice->getDeviceInfo().computeUnitsUsedForScratch)) - 100;
    attributes.perHwThreadPrivateMemorySize = size;
    auto &gfxCoreHelper = pDevice->getRootDeviceEnvironment().getHelper<NEO::GfxCoreHelper>();
    uint32_t maxScratchSize = gfxCoreHelper.getMaxScratchSize();
    attributes.perThreadScratchSize[0] = (size > maxScratchSize) ? maxScratchSize : size;
    attributes.perThreadScratchSize[1] = (size > maxScratchSize) ? maxScratchSize : size;
    EXPECT_EQ(KernelHelper::checkIfThereIsSpaceForScratchOrPrivate(attributes, pDevice), KernelHelper::ErrorCode::SUCCESS);
}

TEST_F(KernelHelperTest, GivenScratchSizeGreaterThanMaxScratchSizeWhenCheckingIfThereIsEnaughSpaceThenInvalidKernelIsReturned) {
    KernelDescriptor::KernelAttributes attributes = {};
    auto &gfxCoreHelper = pDevice->getRootDeviceEnvironment().getHelper<NEO::GfxCoreHelper>();
    uint32_t maxScratchSize = gfxCoreHelper.getMaxScratchSize();
    attributes.perHwThreadPrivateMemorySize = 0x10;
    attributes.perThreadScratchSize[0] = maxScratchSize + 1;
    attributes.perThreadScratchSize[1] = 0x10;
    EXPECT_EQ(KernelHelper::checkIfThereIsSpaceForScratchOrPrivate(attributes, pDevice), KernelHelper::ErrorCode::INVALID_KERNEL);
}

TEST_F(KernelHelperTest, GivenScratchPrivateSizeGreaterThanMaxScratchSizeWhenCheckingIfThereIsEnaughSpaceThenInvalidKernelIsReturned) {
    KernelDescriptor::KernelAttributes attributes = {};
    auto &gfxCoreHelper = pDevice->getRootDeviceEnvironment().getHelper<NEO::GfxCoreHelper>();
    uint32_t maxScratchSize = gfxCoreHelper.getMaxScratchSize();
    attributes.perHwThreadPrivateMemorySize = 0x10;
    attributes.perThreadScratchSize[0] = 0x10;
    attributes.perThreadScratchSize[1] = maxScratchSize + 1;
    EXPECT_EQ(KernelHelper::checkIfThereIsSpaceForScratchOrPrivate(attributes, pDevice), KernelHelper::ErrorCode::INVALID_KERNEL);
}

TEST_F(KernelHelperTest, GivenScratchAndEqualsZeroWhenCheckingIfThereIsEnaughSpaceThenSuccessIsReturned) {
    KernelDescriptor::KernelAttributes attributes = {};
    attributes.perHwThreadPrivateMemorySize = 0;
    attributes.perThreadScratchSize[0] = 0;
    attributes.perThreadScratchSize[1] = 0;
    EXPECT_EQ(KernelHelper::checkIfThereIsSpaceForScratchOrPrivate(attributes, pDevice), KernelHelper::ErrorCode::SUCCESS);
}

TEST_F(KernelHelperTest, GivenScratchEqualsZeroAndPrivetGreaterThanZeroWhenCheckingIfThereIsEnaughSpaceThenSuccessIsReturned) {
    KernelDescriptor::KernelAttributes attributes = {};
    attributes.perHwThreadPrivateMemorySize = 0x10;
    attributes.perThreadScratchSize[0] = 0;
    attributes.perThreadScratchSize[1] = 0;
    EXPECT_EQ(KernelHelper::checkIfThereIsSpaceForScratchOrPrivate(attributes, pDevice), KernelHelper::ErrorCode::SUCCESS);
}

TEST_F(KernelHelperTest, GivenNoPtrByValueWhenCheckingIsAnyArgumentPtrByValueThenFalseIsReturned) {
    KernelDescriptor kernelDescriptor;
    auto pointerArg = ArgDescriptor(ArgDescriptor::ArgTPointer);

    auto valueArg = ArgDescriptor(ArgDescriptor::ArgTValue);
    ArgDescValue::Element element;
    element.isPtr = false;
    valueArg.as<ArgDescValue>().elements.push_back(element);

    kernelDescriptor.payloadMappings.explicitArgs.push_back(pointerArg);
    kernelDescriptor.payloadMappings.explicitArgs.push_back(valueArg);
    EXPECT_FALSE(KernelHelper::isAnyArgumentPtrByValue(kernelDescriptor));
}

TEST_F(KernelHelperTest, GivenPtrByValueWhenCheckingIsAnyArgumentPtrByValueThenTrueIsReturned) {
    KernelDescriptor kernelDescriptor;
    auto pointerArg = ArgDescriptor(ArgDescriptor::ArgTPointer);

    auto valueArg = ArgDescriptor(ArgDescriptor::ArgTValue);
    ArgDescValue::Element element;
    element.isPtr = true;
    valueArg.as<ArgDescValue>().elements.push_back(element);

    kernelDescriptor.payloadMappings.explicitArgs.push_back(pointerArg);
    kernelDescriptor.payloadMappings.explicitArgs.push_back(valueArg);
    EXPECT_TRUE(KernelHelper::isAnyArgumentPtrByValue(kernelDescriptor));
}