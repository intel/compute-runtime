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

        builtinLib = std::make_unique<L0::BuiltinFunctionsLibImpl>(device, neoDevice->getBuiltIns());
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
    std::unique_ptr<L0::BuiltinFunctionsLibImpl> builtinLib;
};

TEST_F(KernelAddressingTest,
       givenBuiltinCopyBufferToBufferKernelsWhenFetchedFromBuiltinLibThenCorrectArgumentSizesAreUsed) {
    {
        // Builtin copyBufferBytes uses copyBufferToBufferBytesSingle without extra arguments
    } {
        const auto builtinType = BuiltinTypeHelper::adjustBuiltinType<L0::Builtin::copyBufferToBufferMiddle>(isStateless, isHeapless);

        auto kernel = builtinLib->getFunction(builtinType);
        ASSERT_NE(nullptr, kernel);
        auto kernelInfo = kernel->getImmutableData()->getKernelInfo();
        ASSERT_NE(nullptr, kernelInfo);

        EXPECT_EQ(kernelInfo->getArgDescriptorAt(2).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
        EXPECT_EQ(kernelInfo->getArgDescriptorAt(3).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
        EXPECT_EQ(kernelInfo->getArgDescriptorAt(4).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
    }
    {
        const auto builtinType = BuiltinTypeHelper::adjustBuiltinType<L0::Builtin::copyBufferToBufferSide>(isStateless, isHeapless);

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
        const auto builtinType = BuiltinTypeHelper::adjustBuiltinType<L0::Builtin::copyBufferRectBytes2d>(isStateless, isHeapless);

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
        const auto builtinType = BuiltinTypeHelper::adjustBuiltinType<L0::Builtin::copyBufferRectBytes3d>(isStateless, isHeapless);

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
        const auto builtinType = BuiltinTypeHelper::adjustBuiltinType<L0::Builtin::fillBufferImmediate>(isStateless, isHeapless);

        auto kernel = builtinLib->getFunction(builtinType);
        ASSERT_NE(nullptr, kernel);
        auto kernelInfo = kernel->getImmutableData()->getKernelInfo();
        ASSERT_NE(nullptr, kernelInfo);

        EXPECT_EQ(kernelInfo->getArgDescriptorAt(1).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
    }
    {
        const auto builtinType = BuiltinTypeHelper::adjustBuiltinType<L0::Builtin::fillBufferImmediateLeftOver>(isStateless, isHeapless);

        auto kernel = builtinLib->getFunction(builtinType);
        ASSERT_NE(nullptr, kernel);
        auto kernelInfo = kernel->getImmutableData()->getKernelInfo();
        ASSERT_NE(nullptr, kernelInfo);

        EXPECT_EQ(kernelInfo->getArgDescriptorAt(1).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
    }
    {
        const auto builtinType = BuiltinTypeHelper::adjustBuiltinType<L0::Builtin::fillBufferSSHOffset>(isStateless, isHeapless);

        auto kernel = builtinLib->getFunction(builtinType);
        ASSERT_NE(nullptr, kernel);
        auto kernelInfo = kernel->getImmutableData()->getKernelInfo();
        ASSERT_NE(nullptr, kernelInfo);

        EXPECT_EQ(kernelInfo->getArgDescriptorAt(1).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
        EXPECT_EQ(kernelInfo->getArgDescriptorAt(3).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
    }
    {
        const auto builtinType = BuiltinTypeHelper::adjustBuiltinType<L0::Builtin::fillBufferMiddle>(isStateless, isHeapless);

        auto kernel = builtinLib->getFunction(builtinType);
        ASSERT_NE(nullptr, kernel);
        auto kernelInfo = kernel->getImmutableData()->getKernelInfo();
        ASSERT_NE(nullptr, kernelInfo);

        EXPECT_EQ(kernelInfo->getArgDescriptorAt(1).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
        EXPECT_EQ(kernelInfo->getArgDescriptorAt(3).as<ArgDescValue>().elements[0].size, sizeof(uint32_t));
    }
    {
        const auto builtinType = BuiltinTypeHelper::adjustBuiltinType<L0::Builtin::fillBufferRightLeftover>(isStateless, isHeapless);

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

    testBuiltinType(BuiltinTypeHelper::adjustImageBuiltinType<L0::ImageBuiltin::copyBufferToImage3d16Bytes>(isStateless, isHeapless));
    testBuiltinType(BuiltinTypeHelper::adjustImageBuiltinType<L0::ImageBuiltin::copyBufferToImage3d2Bytes>(isStateless, isHeapless));
    testBuiltinType(BuiltinTypeHelper::adjustImageBuiltinType<L0::ImageBuiltin::copyBufferToImage3d4Bytes>(isStateless, isHeapless));
    testBuiltinType(BuiltinTypeHelper::adjustImageBuiltinType<L0::ImageBuiltin::copyBufferToImage3d3To4Bytes>(isStateless, isHeapless));
    testBuiltinType(BuiltinTypeHelper::adjustImageBuiltinType<L0::ImageBuiltin::copyBufferToImage3d8Bytes>(isStateless, isHeapless));
    testBuiltinType(BuiltinTypeHelper::adjustImageBuiltinType<L0::ImageBuiltin::copyBufferToImage3d6To8Bytes>(isStateless, isHeapless));
    testBuiltinType(BuiltinTypeHelper::adjustImageBuiltinType<L0::ImageBuiltin::copyBufferToImage3dBytes>(isStateless, isHeapless));
    testBuiltinType(BuiltinTypeHelper::adjustImageBuiltinType<L0::ImageBuiltin::copyBufferToImage3d16BytesAligned>(isStateless, isHeapless));
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

    testBuiltinType(BuiltinTypeHelper::adjustImageBuiltinType<L0::ImageBuiltin::copyImage3dToBuffer16Bytes>(isStateless, isHeapless));
    testBuiltinType(BuiltinTypeHelper::adjustImageBuiltinType<L0::ImageBuiltin::copyImage3dToBuffer2Bytes>(isStateless, isHeapless));
    testBuiltinType(BuiltinTypeHelper::adjustImageBuiltinType<L0::ImageBuiltin::copyImage3dToBuffer3Bytes>(isStateless, isHeapless));
    testBuiltinType(BuiltinTypeHelper::adjustImageBuiltinType<L0::ImageBuiltin::copyImage3dToBuffer4Bytes>(isStateless, isHeapless));
    testBuiltinType(BuiltinTypeHelper::adjustImageBuiltinType<L0::ImageBuiltin::copyImage3dToBuffer4To3Bytes>(isStateless, isHeapless));
    testBuiltinType(BuiltinTypeHelper::adjustImageBuiltinType<L0::ImageBuiltin::copyImage3dToBuffer6Bytes>(isStateless, isHeapless));
    testBuiltinType(BuiltinTypeHelper::adjustImageBuiltinType<L0::ImageBuiltin::copyImage3dToBuffer8Bytes>(isStateless, isHeapless));
    testBuiltinType(BuiltinTypeHelper::adjustImageBuiltinType<L0::ImageBuiltin::copyImage3dToBuffer8To6Bytes>(isStateless, isHeapless));
    testBuiltinType(BuiltinTypeHelper::adjustImageBuiltinType<L0::ImageBuiltin::copyImage3dToBufferBytes>(isStateless, isHeapless));
    testBuiltinType(BuiltinTypeHelper::adjustImageBuiltinType<L0::ImageBuiltin::copyImage3dToBuffer16BytesAligned>(isStateless, isHeapless));
}
} // namespace L0::ult