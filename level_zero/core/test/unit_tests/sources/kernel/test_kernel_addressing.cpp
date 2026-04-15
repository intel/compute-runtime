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

        builtInMode = getDefaultBuiltInMode();
    }

    void TearDown() override {
        builtinLib.reset();
        DeviceFixture::tearDown();
    }

    NEO::BuiltIn::AddressingMode builtInMode{};
    std::unique_ptr<L0::BuiltInKernelLibImpl> builtinLib;
};

TEST_F(KernelAddressingTest,
       givenBuiltinCopyBufferToBufferKernelsWhenFetchedFromBuiltinLibThenCorrectArgumentSizesAreUsed) {
    {
        // BufferBuiltIn copyBufferBytes uses copyBufferToBufferBytesSingle without extra arguments
    }
    {
        auto kernel = builtinLib->getFunction(L0::BufferBuiltIn::copyBufferToBufferMiddle, builtInMode);
        ASSERT_NE(nullptr, kernel);
        auto kernelInfo = kernel->getImmutableData()->getKernelInfo();
        ASSERT_NE(nullptr, kernelInfo);

        EXPECT_EQ(kernelInfo->getArgDescriptorAt(2).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
        EXPECT_EQ(kernelInfo->getArgDescriptorAt(3).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
        EXPECT_EQ(kernelInfo->getArgDescriptorAt(4).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
    }
    {
        auto kernel = builtinLib->getFunction(L0::BufferBuiltIn::copyBufferToBufferSide, builtInMode);
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
        auto kernel = builtinLib->getFunction(L0::BufferBuiltIn::copyBufferRectBytes2d, builtInMode);
        ASSERT_NE(nullptr, kernel);
        auto kernelInfo = kernel->getImmutableData()->getKernelInfo();
        ASSERT_NE(nullptr, kernelInfo);

        EXPECT_EQ(kernelInfo->getArgDescriptorAt(2).as<ArgDescValue>().elements[0].size, 2 * sizeof(uint32_t));
        EXPECT_EQ(kernelInfo->getArgDescriptorAt(3).as<ArgDescValue>().elements[0].size, 2 * sizeof(uint32_t));
        EXPECT_EQ(kernelInfo->getArgDescriptorAt(4).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
        EXPECT_EQ(kernelInfo->getArgDescriptorAt(5).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
    }
    {
        auto kernel = builtinLib->getFunction(L0::BufferBuiltIn::copyBufferRectBytes3d, builtInMode);
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
        auto kernel = builtinLib->getFunction(L0::BufferBuiltIn::fillBufferImmediate, builtInMode);
        ASSERT_NE(nullptr, kernel);
        auto kernelInfo = kernel->getImmutableData()->getKernelInfo();
        ASSERT_NE(nullptr, kernelInfo);

        EXPECT_EQ(kernelInfo->getArgDescriptorAt(1).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
    }
    {
        auto kernel = builtinLib->getFunction(L0::BufferBuiltIn::fillBufferImmediateLeftOver, builtInMode);
        ASSERT_NE(nullptr, kernel);
        auto kernelInfo = kernel->getImmutableData()->getKernelInfo();
        ASSERT_NE(nullptr, kernelInfo);

        EXPECT_EQ(kernelInfo->getArgDescriptorAt(1).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
    }
    {
        auto kernel = builtinLib->getFunction(L0::BufferBuiltIn::fillBufferSSHOffset, builtInMode);
        ASSERT_NE(nullptr, kernel);
        auto kernelInfo = kernel->getImmutableData()->getKernelInfo();
        ASSERT_NE(nullptr, kernelInfo);

        EXPECT_EQ(kernelInfo->getArgDescriptorAt(1).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
        EXPECT_EQ(kernelInfo->getArgDescriptorAt(3).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
    }
    {
        auto kernel = builtinLib->getFunction(L0::BufferBuiltIn::fillBufferMiddle, builtInMode);
        ASSERT_NE(nullptr, kernel);
        auto kernelInfo = kernel->getImmutableData()->getKernelInfo();
        ASSERT_NE(nullptr, kernelInfo);

        EXPECT_EQ(kernelInfo->getArgDescriptorAt(1).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
        EXPECT_EQ(kernelInfo->getArgDescriptorAt(3).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
    }
    {
        auto kernel = builtinLib->getFunction(L0::BufferBuiltIn::fillBufferRightLeftover, builtInMode);
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
        auto kernel = builtinLib->getImageFunction(builtinType, builtInMode);
        ASSERT_NE(nullptr, kernel);
        auto kernelInfo = kernel->getImmutableData()->getKernelInfo();
        ASSERT_NE(nullptr, kernelInfo);

        EXPECT_EQ(kernelInfo->getArgDescriptorAt(2).template as<ArgDescValue>().elements[0].size,
                  sizeof(uint32_t));
        EXPECT_EQ(kernelInfo->getArgDescriptorAt(4).template as<ArgDescValue>().elements[0].size,
                  2 * sizeof(uint32_t));
    };

    testBuiltinType(L0::ImageBuiltIn::copyBufferToImage3d16Bytes);
    testBuiltinType(L0::ImageBuiltIn::copyBufferToImage3d2Bytes);
    testBuiltinType(L0::ImageBuiltIn::copyBufferToImage3d4Bytes);
    testBuiltinType(L0::ImageBuiltIn::copyBufferToImage3d3To4Bytes);
    testBuiltinType(L0::ImageBuiltIn::copyBufferToImage3d8Bytes);
    testBuiltinType(L0::ImageBuiltIn::copyBufferToImage3d6To8Bytes);
    testBuiltinType(L0::ImageBuiltIn::copyBufferToImage3dBytes);
    testBuiltinType(L0::ImageBuiltIn::copyBufferToImage3d16BytesAligned);
}

TEST_F(KernelAddressingTest,
       givenBuiltinCopyImage3dToBufferKernelsWhenFetchedFromBuiltinLibThenCorrectArgumentSizesAreUsed) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    auto testBuiltinType = [&](auto builtinType) {
        auto kernel = builtinLib->getImageFunction(builtinType, builtInMode);
        ASSERT_NE(nullptr, kernel);
        auto kernelInfo = kernel->getImmutableData()->getKernelInfo();
        ASSERT_NE(nullptr, kernelInfo);

        EXPECT_EQ(kernelInfo->getArgDescriptorAt(3).template as<ArgDescValue>().elements[0].size,
                  sizeof(uint32_t));
        EXPECT_EQ(kernelInfo->getArgDescriptorAt(4).template as<ArgDescValue>().elements[0].size,
                  2 * sizeof(uint32_t));
    };

    testBuiltinType(L0::ImageBuiltIn::copyImage3dToBuffer16Bytes);
    testBuiltinType(L0::ImageBuiltIn::copyImage3dToBuffer2Bytes);
    testBuiltinType(L0::ImageBuiltIn::copyImage3dToBuffer3Bytes);
    testBuiltinType(L0::ImageBuiltIn::copyImage3dToBuffer4Bytes);
    testBuiltinType(L0::ImageBuiltIn::copyImage3dToBuffer4To3Bytes);
    testBuiltinType(L0::ImageBuiltIn::copyImage3dToBuffer6Bytes);
    testBuiltinType(L0::ImageBuiltIn::copyImage3dToBuffer8Bytes);
    testBuiltinType(L0::ImageBuiltIn::copyImage3dToBuffer8To6Bytes);
    testBuiltinType(L0::ImageBuiltIn::copyImage3dToBufferBytes);
    testBuiltinType(L0::ImageBuiltIn::copyImage3dToBuffer16BytesAligned);
}

TEST(AddressingModeTest,
     whenStatelessAndWideadjustToWideStatelessIfRequiredIsCalledThenReturnsExpectedMode) {
    auto mode = NEO::BuiltIn::AddressingMode::getDefaultMode(false, true);
    auto adjusted = mode;
    adjusted.adjustToWideStatelessIfRequired(4ull * MemoryConstants::gigaByte);

    EXPECT_EQ(NEO::BuiltIn::AddressingMode::BufferMode::stateless, adjusted.bufferMode);
    EXPECT_TRUE(adjusted.wideMode);
}

TEST(AddressingModeTest,
     whenHeaplessAndWideadjustToWideStatelessIfRequiredIsCalledThenReturnsExpectedMode) {
    auto mode = NEO::BuiltIn::AddressingMode::getDefaultMode(true, true);
    auto adjusted = mode;
    adjusted.adjustToWideStatelessIfRequired(4ull * MemoryConstants::gigaByte);

    EXPECT_EQ(NEO::BuiltIn::AddressingMode::BufferMode::stateless, adjusted.bufferMode);
    EXPECT_TRUE(adjusted.wideMode);
}

TEST(AddressingModeTest,
     whenSmallBufferadjustToWideStatelessIfRequiredIsCalledThenModeIsUnchanged) {
    auto mode = NEO::BuiltIn::AddressingMode::getDefaultMode(false, false);
    auto adjusted = mode;
    adjusted.adjustToWideStatelessIfRequired(1024);

    EXPECT_EQ(mode, adjusted);
    EXPECT_FALSE(adjusted.wideMode);
}
} // namespace L0::ult
