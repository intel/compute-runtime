/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_builtinslib.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

#include <string>

namespace NEO {

void givenUseBindlessBuiltinDisabledWhenBinExtensionPassedThenNameHasBindfulPrefix() {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseBindlessMode.set(0);
    EBuiltInOps::Type builtin = EBuiltInOps::CopyBufferToBuffer;
    const std::string extension = ".bin";
    const std::string platformName = "platformName";
    const uint32_t deviceRevId = 123;

    std::string resourceNameGeneric = createBuiltinResourceName(builtin, extension);
    std::string resourceNameForPlatform = createBuiltinResourceName(builtin, extension, platformName);
    std::string resourceNameForPlatformAndStepping = createBuiltinResourceName(builtin, extension, platformName, deviceRevId);

    std::string expectedResourceNameGeneric = "bindful_copy_buffer_to_buffer.builtin_kernel.bin";

    std::string expectedResourceNameForPlatform = platformName.c_str();
    expectedResourceNameForPlatform += "_0_bindful_copy_buffer_to_buffer.builtin_kernel.bin";

    std::string expectedResourceNameForPlatformAndStepping = platformName.c_str();
    expectedResourceNameForPlatformAndStepping += "_";
    expectedResourceNameForPlatformAndStepping += std::to_string(deviceRevId).c_str();
    expectedResourceNameForPlatformAndStepping += "_bindful_copy_buffer_to_buffer.builtin_kernel.bin";

    EXPECT_EQ(0, strcmp(expectedResourceNameGeneric.c_str(), resourceNameGeneric.c_str()));
    EXPECT_EQ(0, strcmp(expectedResourceNameForPlatform.c_str(), resourceNameForPlatform.c_str()));
    EXPECT_EQ(0, strcmp(expectedResourceNameForPlatformAndStepping.c_str(), resourceNameForPlatformAndStepping.c_str()));
}

void givenUseBindlessBuiltinEnabledWhenBinExtensionPassedThenNameHasBindlessPrefix() {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseBindlessMode.set(1);
    EBuiltInOps::Type builtin = EBuiltInOps::CopyBufferToBuffer;
    const std::string extension = ".bin";
    const std::string platformName = "skl";
    const uint32_t deviceRevId = 9;

    std::string resourceNameGeneric = createBuiltinResourceName(builtin, extension);
    std::string resourceNameForPlatform = createBuiltinResourceName(builtin, extension, platformName);
    std::string resourceNameForPlatformAndStepping = createBuiltinResourceName(builtin, extension, platformName, deviceRevId);

    std::string expectedResourceNameGeneric = "bindless_copy_buffer_to_buffer.builtin_kernel.bin";

    std::string expectedResourceNameForPlatform = platformName.c_str();
    expectedResourceNameForPlatform += "_0_bindless_copy_buffer_to_buffer.builtin_kernel.bin";

    std::string expectedResourceNameForPlatformAndStepping = platformName.c_str();
    expectedResourceNameForPlatformAndStepping += "_";
    expectedResourceNameForPlatformAndStepping += std::to_string(deviceRevId).c_str();
    expectedResourceNameForPlatformAndStepping += "_bindless_copy_buffer_to_buffer.builtin_kernel.bin";

    EXPECT_EQ(0, strcmp(expectedResourceNameGeneric.c_str(), resourceNameGeneric.c_str()));
    EXPECT_EQ(0, strcmp(expectedResourceNameForPlatform.c_str(), resourceNameForPlatform.c_str()));
    EXPECT_EQ(0, strcmp(expectedResourceNameForPlatformAndStepping.c_str(), resourceNameForPlatformAndStepping.c_str()));
}
} // namespace NEO

using namespace NEO;

using BuiltInSharedTest = Test<DeviceFixture>;
HWTEST_F(BuiltInSharedTest, GivenBuiltinTypeBinaryWhenGettingBuiltinResourceForNotRegisteredRevisionThenBuiltinFromDefaultRevisionIsTaken) {
    pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->platform.usRevId += 0xdead;
    auto mockBuiltinsLib = std::unique_ptr<MockBuiltinsLib>(new MockBuiltinsLib());

    const std::array<uint32_t, 11> builtinTypes{EBuiltInOps::CopyBufferToBuffer,
                                                EBuiltInOps::CopyBufferRect,
                                                EBuiltInOps::FillBuffer,
                                                EBuiltInOps::CopyBufferToImage3d,
                                                EBuiltInOps::CopyImage3dToBuffer,
                                                EBuiltInOps::CopyImageToImage1d,
                                                EBuiltInOps::CopyImageToImage2d,
                                                EBuiltInOps::CopyImageToImage3d,
                                                EBuiltInOps::FillImage1d,
                                                EBuiltInOps::FillImage2d,
                                                EBuiltInOps::FillImage3d};

    for (auto &builtinType : builtinTypes) {
        EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(builtinType, BuiltinCode::ECodeType::Binary, *pDevice).size());
    }

    UltDeviceFactory deviceFactory{1, 0};
    auto pDeviceWithDefaultRevision = deviceFactory.rootDevices[0];
    for (auto &builtinType : builtinTypes) {
        EXPECT_EQ(mockBuiltinsLib->getBuiltinResource(builtinType, BuiltinCode::ECodeType::Binary, *pDeviceWithDefaultRevision).size(), mockBuiltinsLib->getBuiltinResource(builtinType, BuiltinCode::ECodeType::Binary, *pDevice).size());
    }
}
