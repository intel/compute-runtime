/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/program/kernel_info.h"
#include "shared/test/common/test_macros/test_checks_shared.h"

#include "level_zero/core/source/builtin/builtin_functions_lib.h"
#include "level_zero/core/source/builtin/builtin_functions_lib_impl.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/kernel/kernel.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0::ult {
struct KernelAddressingTest : public DeviceFixture, public ::testing::Test {
    void SetUp() override {
        DeviceFixture::setUp();

        builtinLib = std::make_unique<L0::BuiltInKernelLibImpl>(device, neoDevice->getBuiltIns());
        ASSERT_NE(nullptr, builtinLib.get());

        auto &compilerProductHelper = device->getCompilerProductHelper();
        isHeapless = compilerProductHelper.isHeaplessModeEnabled(neoDevice->getHardwareInfo());
        isStateless = compilerProductHelper.isForceToStatelessRequired();
    }

    void TearDown() override {
        builtinLib.reset();
        DeviceFixture::tearDown();
    }

    bool isHeapless = false;
    bool isStateless = false;
    std::unique_ptr<L0::BuiltInKernelLibImpl> builtinLib;
};

TEST_F(KernelAddressingTest,
       givenBuiltinCopyBufferToBufferKernelsWhenFetchedFromBuiltinLibThenCorrectArgumentSizesAreUsed) {
    {
        // BufferBuiltIn copyBufferBytes uses copyBufferToBufferBytesSingle without extra arguments
    } {
        const auto builtinType = BuiltInHelper::adjustBufferBuiltIn<L0::BufferBuiltIn::copyBufferToBufferMiddle>(isStateless, isHeapless);

        auto kernel = builtinLib->getFunction(builtinType);
        ASSERT_NE(nullptr, kernel);
        auto kernelInfo = kernel->getImmutableData()->getKernelInfo();
        ASSERT_NE(nullptr, kernelInfo);

        EXPECT_EQ(kernelInfo->getArgDescriptorAt(2).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
        EXPECT_EQ(kernelInfo->getArgDescriptorAt(3).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
        EXPECT_EQ(kernelInfo->getArgDescriptorAt(4).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
    }
    {
        const auto builtinType = BuiltInHelper::adjustBufferBuiltIn<L0::BufferBuiltIn::copyBufferToBufferSide>(isStateless, isHeapless);

        auto kernel = builtinLib->getFunction(builtinType);
        ASSERT_NE(nullptr, kernel);
        auto kernelInfo = kernel->getImmutableData()->getKernelInfo();
        ASSERT_NE(nullptr, kernelInfo);

        EXPECT_EQ(kernelInfo->getArgDescriptorAt(2).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
        EXPECT_EQ(kernelInfo->getArgDescriptorAt(3).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
        EXPECT_EQ(kernelInfo->getArgDescriptorAt(4).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
    }
}

TEST_F(KernelAddressingTest,
       givenBuiltinCopyBufferRectKernelsWhenFetchedFromBuiltinLibThenCorrectArgumentSizesAreUsed) {
    {
        const auto builtinType = BuiltInHelper::adjustBufferBuiltIn<L0::BufferBuiltIn::copyBufferRectBytes2d>(isStateless, isHeapless);

        auto kernel = builtinLib->getFunction(builtinType);
        ASSERT_NE(nullptr, kernel);
        auto kernelInfo = kernel->getImmutableData()->getKernelInfo();
        ASSERT_NE(nullptr, kernelInfo);

        EXPECT_EQ(kernelInfo->getArgDescriptorAt(2).as<ArgDescValue>().elements[0].size, 2 * sizeof(uint32_t));
        EXPECT_EQ(kernelInfo->getArgDescriptorAt(3).as<ArgDescValue>().elements[0].size, 2 * sizeof(uint32_t));
        EXPECT_EQ(kernelInfo->getArgDescriptorAt(4).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
        EXPECT_EQ(kernelInfo->getArgDescriptorAt(5).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
    }
    {
        const auto builtinType = BuiltInHelper::adjustBufferBuiltIn<L0::BufferBuiltIn::copyBufferRectBytes3d>(isStateless, isHeapless);

        auto kernel = builtinLib->getFunction(builtinType);
        ASSERT_NE(nullptr, kernel);
        auto kernelInfo = kernel->getImmutableData()->getKernelInfo();
        ASSERT_NE(nullptr, kernelInfo);

        EXPECT_EQ(kernelInfo->getArgDescriptorAt(2).as<ArgDescValue>().elements[0].size, 4 * sizeof(uint32_t));
        EXPECT_EQ(kernelInfo->getArgDescriptorAt(3).as<ArgDescValue>().elements[0].size, 4 * sizeof(uint32_t));
        EXPECT_EQ(kernelInfo->getArgDescriptorAt(4).as<ArgDescValue>().elements[0].size, 2 * sizeof(uint32_t));
        EXPECT_EQ(kernelInfo->getArgDescriptorAt(5).as<ArgDescValue>().elements[0].size, 2 * sizeof(uint32_t));
    }
}

TEST_F(KernelAddressingTest, givenBuiltinFillBufferKernelsWhenFetchedFromProgramThenCorrectArgumentSizesAreUsed) {
    {
        const auto builtinType = BuiltInHelper::adjustBufferBuiltIn<L0::BufferBuiltIn::fillBufferImmediate>(isStateless, isHeapless);

        auto kernel = builtinLib->getFunction(builtinType);
        ASSERT_NE(nullptr, kernel);
        auto kernelInfo = kernel->getImmutableData()->getKernelInfo();
        ASSERT_NE(nullptr, kernelInfo);

        EXPECT_EQ(kernelInfo->getArgDescriptorAt(1).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
    }
    {
        const auto builtinType = BuiltInHelper::adjustBufferBuiltIn<L0::BufferBuiltIn::fillBufferImmediateLeftOver>(isStateless, isHeapless);

        auto kernel = builtinLib->getFunction(builtinType);
        ASSERT_NE(nullptr, kernel);
        auto kernelInfo = kernel->getImmutableData()->getKernelInfo();
        ASSERT_NE(nullptr, kernelInfo);

        EXPECT_EQ(kernelInfo->getArgDescriptorAt(1).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
    }
    {
        const auto builtinType = BuiltInHelper::adjustBufferBuiltIn<L0::BufferBuiltIn::fillBufferSSHOffset>(isStateless, isHeapless);

        auto kernel = builtinLib->getFunction(builtinType);
        ASSERT_NE(nullptr, kernel);
        auto kernelInfo = kernel->getImmutableData()->getKernelInfo();
        ASSERT_NE(nullptr, kernelInfo);

        EXPECT_EQ(kernelInfo->getArgDescriptorAt(1).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
        EXPECT_EQ(kernelInfo->getArgDescriptorAt(3).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
    }
    {
        const auto builtinType = BuiltInHelper::adjustBufferBuiltIn<L0::BufferBuiltIn::fillBufferMiddle>(isStateless, isHeapless);

        auto kernel = builtinLib->getFunction(builtinType);
        ASSERT_NE(nullptr, kernel);
        auto kernelInfo = kernel->getImmutableData()->getKernelInfo();
        ASSERT_NE(nullptr, kernelInfo);

        EXPECT_EQ(kernelInfo->getArgDescriptorAt(1).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
        EXPECT_EQ(kernelInfo->getArgDescriptorAt(3).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
    }
    {
        const auto builtinType = BuiltInHelper::adjustBufferBuiltIn<L0::BufferBuiltIn::fillBufferRightLeftover>(isStateless, isHeapless);

        auto kernel = builtinLib->getFunction(builtinType);
        ASSERT_NE(nullptr, kernel);
        auto kernelInfo = kernel->getImmutableData()->getKernelInfo();
        ASSERT_NE(nullptr, kernelInfo);

        EXPECT_EQ(kernelInfo->getArgDescriptorAt(1).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
        EXPECT_EQ(kernelInfo->getArgDescriptorAt(3).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
    }
}

TEST_F(KernelAddressingTest,
       givenBuiltinCopyBufferToImage3dKernelsWhenFetchedFromBuiltinLibThenCorrectArgumentSizesAreUsed) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    auto testBuiltinType = [&](auto builtinType) {
        auto kernel = builtinLib->getImageFunction(builtinType);
        ASSERT_NE(nullptr, kernel);
        auto kernelInfo = kernel->getImmutableData()->getKernelInfo();
        ASSERT_NE(nullptr, kernelInfo);

        EXPECT_EQ(kernelInfo->getArgDescriptorAt(2).template as<ArgDescValue>().elements[0].size,
                  sizeof(uint32_t));
        EXPECT_EQ(kernelInfo->getArgDescriptorAt(4).template as<ArgDescValue>().elements[0].size,
                  2 * sizeof(uint32_t));
    };

    testBuiltinType(BuiltInHelper::adjustImageBuiltIn<L0::ImageBuiltIn::copyBufferToImage3d16Bytes>(isStateless, isHeapless));
    testBuiltinType(BuiltInHelper::adjustImageBuiltIn<L0::ImageBuiltIn::copyBufferToImage3d2Bytes>(isStateless, isHeapless));
    testBuiltinType(BuiltInHelper::adjustImageBuiltIn<L0::ImageBuiltIn::copyBufferToImage3d4Bytes>(isStateless, isHeapless));
    testBuiltinType(BuiltInHelper::adjustImageBuiltIn<L0::ImageBuiltIn::copyBufferToImage3d3To4Bytes>(isStateless, isHeapless));
    testBuiltinType(BuiltInHelper::adjustImageBuiltIn<L0::ImageBuiltIn::copyBufferToImage3d8Bytes>(isStateless, isHeapless));
    testBuiltinType(BuiltInHelper::adjustImageBuiltIn<L0::ImageBuiltIn::copyBufferToImage3d6To8Bytes>(isStateless, isHeapless));
    testBuiltinType(BuiltInHelper::adjustImageBuiltIn<L0::ImageBuiltIn::copyBufferToImage3dBytes>(isStateless, isHeapless));
    testBuiltinType(BuiltInHelper::adjustImageBuiltIn<L0::ImageBuiltIn::copyBufferToImage3d16BytesAligned>(isStateless, isHeapless));
}

TEST_F(KernelAddressingTest,
       givenBuiltinCopyImage3dToBufferKernelsWhenFetchedFromBuiltinLibThenCorrectArgumentSizesAreUsed) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    auto testBuiltinType = [&](auto builtinType) {
        auto kernel = builtinLib->getImageFunction(builtinType);
        ASSERT_NE(nullptr, kernel);
        auto kernelInfo = kernel->getImmutableData()->getKernelInfo();
        ASSERT_NE(nullptr, kernelInfo);

        EXPECT_EQ(kernelInfo->getArgDescriptorAt(3).template as<ArgDescValue>().elements[0].size,
                  sizeof(uint32_t));
        EXPECT_EQ(kernelInfo->getArgDescriptorAt(4).template as<ArgDescValue>().elements[0].size,
                  2 * sizeof(uint32_t));
    };

    testBuiltinType(BuiltInHelper::adjustImageBuiltIn<L0::ImageBuiltIn::copyImage3dToBuffer16Bytes>(isStateless, isHeapless));
    testBuiltinType(BuiltInHelper::adjustImageBuiltIn<L0::ImageBuiltIn::copyImage3dToBuffer2Bytes>(isStateless, isHeapless));
    testBuiltinType(BuiltInHelper::adjustImageBuiltIn<L0::ImageBuiltIn::copyImage3dToBuffer3Bytes>(isStateless, isHeapless));
    testBuiltinType(BuiltInHelper::adjustImageBuiltIn<L0::ImageBuiltIn::copyImage3dToBuffer4Bytes>(isStateless, isHeapless));
    testBuiltinType(BuiltInHelper::adjustImageBuiltIn<L0::ImageBuiltIn::copyImage3dToBuffer4To3Bytes>(isStateless, isHeapless));
    testBuiltinType(BuiltInHelper::adjustImageBuiltIn<L0::ImageBuiltIn::copyImage3dToBuffer6Bytes>(isStateless, isHeapless));
    testBuiltinType(BuiltInHelper::adjustImageBuiltIn<L0::ImageBuiltIn::copyImage3dToBuffer8Bytes>(isStateless, isHeapless));
    testBuiltinType(BuiltInHelper::adjustImageBuiltIn<L0::ImageBuiltIn::copyImage3dToBuffer8To6Bytes>(isStateless, isHeapless));
    testBuiltinType(BuiltInHelper::adjustImageBuiltIn<L0::ImageBuiltIn::copyImage3dToBufferBytes>(isStateless, isHeapless));
    testBuiltinType(BuiltInHelper::adjustImageBuiltIn<L0::ImageBuiltIn::copyImage3dToBuffer16BytesAligned>(isStateless, isHeapless));
}

TEST(BuiltInHelperTest,
     whenStatelessAndWideAdjustBuiltinTypeIsCalledThenReturnsExpectedBuiltinForHeaplessVariants) {
    const bool isStateless = true;
    const bool isWide = true;

    for (bool isHeapless : {false, true}) {
        EXPECT_EQ(isHeapless ? BufferBuiltIn::copyBufferBytesWideStatelessHeapless
                             : BufferBuiltIn::copyBufferBytesWideStateless,
                  BuiltInHelper::adjustBufferBuiltIn<BufferBuiltIn::copyBufferBytes>(isStateless, isHeapless, isWide));

        EXPECT_EQ(isHeapless ? BufferBuiltIn::copyBufferToBufferMiddleWideStatelessHeapless
                             : BufferBuiltIn::copyBufferToBufferMiddleWideStateless,
                  BuiltInHelper::adjustBufferBuiltIn<BufferBuiltIn::copyBufferToBufferMiddle>(isStateless, isHeapless, isWide));

        EXPECT_EQ(isHeapless ? BufferBuiltIn::copyBufferToBufferSideWideStatelessHeapless
                             : BufferBuiltIn::copyBufferToBufferSideWideStateless,
                  BuiltInHelper::adjustBufferBuiltIn<BufferBuiltIn::copyBufferToBufferSide>(isStateless, isHeapless, isWide));

        EXPECT_EQ(isHeapless ? BufferBuiltIn::fillBufferImmediateWideStatelessHeapless
                             : BufferBuiltIn::fillBufferImmediateWideStateless,
                  BuiltInHelper::adjustBufferBuiltIn<BufferBuiltIn::fillBufferImmediate>(isStateless, isHeapless, isWide));

        EXPECT_EQ(isHeapless ? BufferBuiltIn::fillBufferImmediateLeftOverWideStatelessHeapless
                             : BufferBuiltIn::fillBufferImmediateLeftOverWideStateless,
                  BuiltInHelper::adjustBufferBuiltIn<BufferBuiltIn::fillBufferImmediateLeftOver>(isStateless, isHeapless, isWide));

        EXPECT_EQ(isHeapless ? BufferBuiltIn::fillBufferMiddleWideStatelessHeapless
                             : BufferBuiltIn::fillBufferMiddleWideStateless,
                  BuiltInHelper::adjustBufferBuiltIn<BufferBuiltIn::fillBufferMiddle>(isStateless, isHeapless, isWide));

        EXPECT_EQ(isHeapless ? BufferBuiltIn::fillBufferRightLeftoverWideStatelessHeapless
                             : BufferBuiltIn::fillBufferRightLeftoverWideStateless,
                  BuiltInHelper::adjustBufferBuiltIn<BufferBuiltIn::fillBufferRightLeftover>(isStateless, isHeapless, isWide));
    }
}
} // namespace L0::ult