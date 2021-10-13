/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_builtinslib.h"
#include "shared/test/unit_test/built_ins/built_in_tests_shared.inl"

#include "test.h"

using namespace NEO;

TEST(BuiltInTestsL0, givenUseBindlessBuiltinInApiDependentModeWhenBinExtensionPassedThenNameHasBindfulPrefix) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseBindlessMode.set(-1);
    EBuiltInOps::Type builtin = EBuiltInOps::CopyBufferToBuffer;
    const std::string extension = ".bin";
    const std::string platformName = "platformName";
    const uint32_t deviceRevId = 123;

    std::string resourceNameGeneric = createBuiltinResourceName(builtin, extension);
    std::string resourceNameForPlatformAndStepping = createBuiltinResourceName(builtin, extension, platformName, deviceRevId);

    std::string expectedResourceNameGeneric = "bindful_copy_buffer_to_buffer.builtin_kernel.bin";

    std::string expectedResourceNameForPlatformAndStepping = platformName.c_str();
    expectedResourceNameForPlatformAndStepping += "_";
    expectedResourceNameForPlatformAndStepping += std::to_string(deviceRevId).c_str();
    expectedResourceNameForPlatformAndStepping += "_bindful_copy_buffer_to_buffer.builtin_kernel.bin";

    EXPECT_EQ(0, strcmp(expectedResourceNameGeneric.c_str(), resourceNameGeneric.c_str()));
    EXPECT_EQ(0, strcmp(expectedResourceNameForPlatformAndStepping.c_str(), resourceNameForPlatformAndStepping.c_str()));
}
TEST(BuiltInTestsL0, givenUseBindlessBuiltinDisabledInL0ApiWhenBinExtensionPassedThenNameHasBindfulPrefix) {
    givenUseBindlessBuiltinDisabledWhenBinExtensionPassedThenNameHasBindfulPrefix();
}

TEST(BuiltInTestsL0, givenUseBindlessBuiltinEnabledInL0ApiWhenBinExtensionPassedThenNameHasBindlessPrefix) {
    givenUseBindlessBuiltinEnabledWhenBinExtensionPassedThenNameHasBindlessPrefix();
}

using BuiltInTestL0 = Test<DeviceFixture>;

TEST_F(BuiltInTestL0, GivenBuiltinTypeBinaryWhenGettingBuiltinResourceForNotRegisteredRevisionThenResourceSizeIsZero) {
    pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->platform.usRevId += 0xdead;
    auto mockBuiltinsLib = std::unique_ptr<MockBuiltinsLib>(new MockBuiltinsLib());

    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyBufferToBuffer, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyBufferRect, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::FillBuffer, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyBufferToImage3d, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyImage3dToBuffer, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyImageToImage1d, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyImageToImage2d, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyImageToImage3d, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::FillImage1d, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::FillImage2d, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::FillImage3d, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::COUNT, BuiltinCode::ECodeType::Binary, *pDevice).size());
}
