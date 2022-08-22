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

using BuiltInSharedTest = Test<DeviceFixture>;

HWTEST2_F(BuiltInSharedTest, givenUseBindlessBuiltinDisabledWhenBinExtensionPassedThenNameHasBindfulPrefix, HasStatefulSupport) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseBindlessMode.set(0);
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    const std::string platformName = getFamilyNameWithType(hwInfo);
    const std::string revId = std::to_string(hwInfo.platform.usRevId);
    const std::string defaultRevId = std::to_string(HwHelper::get(hwInfo.platform.eRenderCoreFamily).getDefaultRevisionId(hwInfo));

    auto resourceNames = getBuiltinResourceNames(EBuiltInOps::CopyBufferToBuffer, BuiltinCode::ECodeType::Binary, *pDevice);

    std::string expectedResourceNameGeneric = "bindful_copy_buffer_to_buffer.builtin_kernel.bin";
    std::string expectedResourceNameForPlatform = platformName + "_" + defaultRevId + "_" + expectedResourceNameGeneric;
    std::string expectedResourceNameForPlatformAndStepping = platformName + "_" + revId + "_" + expectedResourceNameGeneric;

    EXPECT_EQ(resourceNames[0], expectedResourceNameForPlatformAndStepping);
    EXPECT_EQ(resourceNames[1], expectedResourceNameForPlatform);
    EXPECT_EQ(resourceNames[2], expectedResourceNameGeneric);
}

HWTEST2_F(BuiltInSharedTest, givenUseBindlessBuiltinEnabledWhenBinExtensionPassedThenNameHasBindlessPrefix, HasStatefulSupport) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseBindlessMode.set(1);
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    const std::string platformName = getFamilyNameWithType(hwInfo);
    const std::string revId = std::to_string(hwInfo.platform.usRevId);
    const std::string defaultRevId = std::to_string(HwHelper::get(hwInfo.platform.eRenderCoreFamily).getDefaultRevisionId(hwInfo));

    auto resourceNames = getBuiltinResourceNames(EBuiltInOps::CopyBufferToBuffer, BuiltinCode::ECodeType::Binary, *pDevice);

    std::string expectedResourceNameGeneric = "bindless_copy_buffer_to_buffer.builtin_kernel.bin";
    std::string expectedResourceNameForPlatform = platformName + "_" + defaultRevId + "_" + expectedResourceNameGeneric;
    std::string expectedResourceNameForPlatformAndStepping = platformName + "_" + revId + "_" + expectedResourceNameGeneric;

    EXPECT_EQ(resourceNames[0], expectedResourceNameForPlatformAndStepping);
    EXPECT_EQ(resourceNames[1], expectedResourceNameForPlatform);
    EXPECT_EQ(resourceNames[2], expectedResourceNameGeneric);
}

HWTEST2_F(BuiltInSharedTest, GivenBuiltinTypeBinaryWhenGettingBuiltinResourceForNotRegisteredRevisionThenBuiltinFromDefaultRevisionIsTaken, HasStatefulSupport) {
    auto builtinsLib = std::make_unique<MockBuiltinsLib>();
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo.platform.usRevId += 0xdead;
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
    UltDeviceFactory deviceFactory{1, 0};
    auto pDeviceWithDefaultRevision = deviceFactory.rootDevices[0];
    for (auto &builtinType : builtinTypes) {
        auto builtinResource = builtinsLib->getBuiltinResource(builtinType, BuiltinCode::ECodeType::Binary, *pDevice);
        EXPECT_NE(0U, builtinResource.size());

        auto expectedBuiltinResource = builtinsLib->getBuiltinResource(builtinType, BuiltinCode::ECodeType::Binary, *pDeviceWithDefaultRevision);
        EXPECT_EQ(expectedBuiltinResource.size(), builtinResource.size());
    }
}

HWTEST2_F(BuiltInSharedTest, GivenStatelessBuiltinWhenGettingResourceNameThenAddressingIsStateless, HasStatefulSupport) {
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    const std::string platformName = getFamilyNameWithType(hwInfo);
    const std::string revId = std::to_string(hwInfo.platform.usRevId);
    const std::string defaultRevId = std::to_string(HwHelper::get(hwInfo.platform.eRenderCoreFamily).getDefaultRevisionId(hwInfo));

    auto resourceNames = getBuiltinResourceNames(EBuiltInOps::CopyBufferToBufferStateless, BuiltinCode::ECodeType::Binary, *pDevice);

    std::string expectedResourceNameGeneric = "stateless_copy_buffer_to_buffer_stateless.builtin_kernel.bin";
    std::string expectedResourceNameForPlatform = platformName + "_" + defaultRevId + "_" + expectedResourceNameGeneric;
    std::string expectedResourceNameForPlatformAndStepping = platformName + "_" + revId + "_" + expectedResourceNameGeneric;

    EXPECT_EQ(resourceNames[0], expectedResourceNameForPlatformAndStepping);
    EXPECT_EQ(resourceNames[1], expectedResourceNameForPlatform);
    EXPECT_EQ(resourceNames[2], expectedResourceNameGeneric);
}

HWTEST2_F(BuiltInSharedTest, GivenPlatformWithoutStatefulAddresingSupportWhenGettingResourceNamesThenStatelessResourceNameIsReturned, HasNoStatefulSupport) {
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    const std::string platformName = getFamilyNameWithType(hwInfo);
    const std::string revId = std::to_string(hwInfo.platform.usRevId);

    {
        auto resourceNames = getBuiltinResourceNames(EBuiltInOps::CopyBufferToBuffer, BuiltinCode::ECodeType::Binary, *pDevice);
        std::string expectedResourceName = platformName + "_" + revId + "_stateless_copy_buffer_to_buffer.builtin_kernel.bin";
        EXPECT_EQ(resourceNames[0], expectedResourceName);
    }

    {
        auto resourceNames = getBuiltinResourceNames(EBuiltInOps::CopyBufferToBufferStateless, BuiltinCode::ECodeType::Binary, *pDevice);
        std::string expectedResourceName = platformName + "_" + revId + "_stateless_copy_buffer_to_buffer_stateless.builtin_kernel.bin";
        EXPECT_EQ(resourceNames[0], expectedResourceName);
    }
}

HWTEST_F(BuiltInSharedTest, GivenRequestedTypeIntermediateWhenGettingResourceNamesThenCorrectNameIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseBindlessMode.set(0);
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    const std::string platformName = getFamilyNameWithType(hwInfo);
    const std::string revId = std::to_string(hwInfo.platform.usRevId);
    const std::string defaultRevId = std::to_string(HwHelper::get(hwInfo.platform.eRenderCoreFamily).getDefaultRevisionId(hwInfo));

    auto resourceNames = getBuiltinResourceNames(EBuiltInOps::CopyBufferToBuffer, BuiltinCode::ECodeType::Intermediate, *pDevice);

    std::string expectedResourceNameGeneric = "copy_buffer_to_buffer.builtin_kernel.bc";
    std::string expectedResourceNameForPlatform = platformName + "_" + defaultRevId + "_" + expectedResourceNameGeneric;
    std::string expectedResourceNameForPlatformAndStepping = platformName + "_" + revId + "_" + expectedResourceNameGeneric;

    EXPECT_EQ(resourceNames[0], expectedResourceNameForPlatformAndStepping);
    EXPECT_EQ(resourceNames[1], expectedResourceNameForPlatform);
    EXPECT_EQ(resourceNames[2], expectedResourceNameGeneric);
}

HWTEST_F(BuiltInSharedTest, GivenValidBuiltinTypeAndExtensionWhenCreatingBuiltinResourceNameThenCorrectNameIsReturned) {
    std::string builtinResourceName = createBuiltinResourceName(EBuiltInOps::CopyBufferToBuffer, ".bin");
    std::string expectedBuiltinResourceName = std::string(getBuiltinAsString(EBuiltInOps::CopyBufferToBuffer)) + ".bin";
    EXPECT_EQ(expectedBuiltinResourceName, builtinResourceName);
}

HWTEST_F(BuiltInSharedTest, GivenValidBuiltinTypeAndAnyTypeWhenGettingBuiltinCodeThenNonEmptyBuiltinIsReturned) {
    auto builtinsLib = std::make_unique<MockBuiltinsLib>();

    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    const std::string platformName = getFamilyNameWithType(hwInfo);
    const std::string revId = std::to_string(hwInfo.platform.usRevId);
    const std::string defaultRevId = std::to_string(HwHelper::get(hwInfo.platform.eRenderCoreFamily).getDefaultRevisionId(hwInfo));

    auto builtinCode = builtinsLib->getBuiltinCode(EBuiltInOps::CopyBufferToBuffer, BuiltinCode::ECodeType::Any, *pDevice);
    EXPECT_EQ(BuiltinCode::ECodeType::Binary, builtinCode.type);
    EXPECT_NE(0U, builtinCode.resource.size());
}
