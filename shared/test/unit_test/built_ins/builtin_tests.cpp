/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_builtinslib.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

#include <string>

using BuiltInSharedTest = Test<DeviceFixture>;

HWTEST2_F(BuiltInSharedTest, givenUseBindlessBuiltinDisabledWhenBinExtensionPassedThenNameHasBindfulPrefix, HasStatefulSupport) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.UseBindlessMode.set(0);
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    std::string deviceIpString = std::to_string(hwInfo.ipVersion.architecture) + "_" + std::to_string(hwInfo.ipVersion.release) + "_" + std::to_string(hwInfo.ipVersion.revision);

    auto resourceNames = getBuiltinResourceNames(EBuiltInOps::copyBufferToBuffer, BuiltinCode::ECodeType::Binary, *pDevice);

    std::string expectedResourceNameGeneric = "bindful_copy_buffer_to_buffer.builtin_kernel.bin";
    std::string expectedResourceNameForRelease = deviceIpString + "_" + expectedResourceNameGeneric;

    EXPECT_EQ(1u, resourceNames.size());
    EXPECT_EQ(resourceNames[0], expectedResourceNameForRelease);
}

HWTEST2_F(BuiltInSharedTest, givenUseBindlessBuiltinEnabledWhenBinExtensionPassedThenNameHasBindlessPrefix, HasStatefulSupport) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.UseBindlessMode.set(1);
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    std::string deviceIpString = std::to_string(hwInfo.ipVersion.architecture) + "_" + std::to_string(hwInfo.ipVersion.release) + "_" + std::to_string(hwInfo.ipVersion.revision);

    auto resourceNames = getBuiltinResourceNames(EBuiltInOps::copyBufferToBuffer, BuiltinCode::ECodeType::Binary, *pDevice);

    std::string expectedResourceNameGeneric = "bindless_copy_buffer_to_buffer.builtin_kernel.bin";
    std::string expectedResourceNameForRelease = deviceIpString + "_" + expectedResourceNameGeneric;

    EXPECT_EQ(1u, resourceNames.size());
    EXPECT_EQ(resourceNames[0], expectedResourceNameForRelease);
}

HWTEST_F(BuiltInSharedTest, whenTryingToGetBuiltinResourceForUnregisteredPlatformThenOnlyIntermediateFormatIsAvailable) {
    auto builtinsLib = std::make_unique<MockBuiltinsLib>();
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo.ipVersion.value += 0xdead;
    const std::array<uint32_t, 11> builtinTypes{EBuiltInOps::copyBufferToBuffer,
                                                EBuiltInOps::copyBufferRect,
                                                EBuiltInOps::fillBuffer,
                                                EBuiltInOps::copyBufferToImage3d,
                                                EBuiltInOps::copyImage3dToBuffer,
                                                EBuiltInOps::copyImageToImage1d,
                                                EBuiltInOps::copyImageToImage2d,
                                                EBuiltInOps::copyImageToImage3d,
                                                EBuiltInOps::fillImage1d,
                                                EBuiltInOps::fillImage2d,
                                                EBuiltInOps::fillImage3d};

    for (auto &builtinType : builtinTypes) {
        auto binaryBuiltinResource = builtinsLib->getBuiltinResource(builtinType, BuiltinCode::ECodeType::Binary, *pDevice);
        EXPECT_EQ(0U, binaryBuiltinResource.size());

        auto intermediateBuiltinResource = builtinsLib->getBuiltinResource(builtinType, BuiltinCode::ECodeType::Intermediate, *pDevice);
        EXPECT_NE(0U, intermediateBuiltinResource.size());
    }
}

HWTEST2_F(BuiltInSharedTest, GivenStatelessBuiltinWhenGettingResourceNameThenAddressingIsStateless, HasStatefulSupport) {
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    std::string deviceIpString = std::to_string(hwInfo.ipVersion.architecture) + "_" + std::to_string(hwInfo.ipVersion.release) + "_" + std::to_string(hwInfo.ipVersion.revision);

    auto resourceNames = getBuiltinResourceNames(EBuiltInOps::copyBufferToBufferStateless, BuiltinCode::ECodeType::Binary, *pDevice);

    std::string expectedResourceNameGeneric = "stateless_copy_buffer_to_buffer_stateless.builtin_kernel.bin";
    std::string expectedResourceNameForRelease = deviceIpString + "_" + expectedResourceNameGeneric;

    EXPECT_EQ(1u, resourceNames.size());
    EXPECT_EQ(resourceNames[0], expectedResourceNameForRelease);
}

HWTEST2_F(BuiltInSharedTest, GivenPlatformWithoutStatefulAddresingSupportWhenGettingResourceNamesThenStatelessResourceNameIsReturned, HasNoStatefulSupport) {
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    std::string deviceIpString = std::to_string(hwInfo.ipVersion.architecture) + "_" + std::to_string(hwInfo.ipVersion.release) + "_" + std::to_string(hwInfo.ipVersion.revision);
    {
        auto resourceNames = getBuiltinResourceNames(EBuiltInOps::copyBufferToBuffer, BuiltinCode::ECodeType::Binary, *pDevice);
        std::string expectedResourceName = deviceIpString + "_stateless_copy_buffer_to_buffer.builtin_kernel.bin";
        EXPECT_EQ(1u, resourceNames.size());
        EXPECT_EQ(resourceNames[0], expectedResourceName);
    }

    {
        auto resourceNames = getBuiltinResourceNames(EBuiltInOps::copyBufferToBufferStateless, BuiltinCode::ECodeType::Binary, *pDevice);
        std::string expectedResourceName = deviceIpString + "_stateless_copy_buffer_to_buffer_stateless.builtin_kernel.bin";
        EXPECT_EQ(1u, resourceNames.size());
        EXPECT_EQ(resourceNames[0], expectedResourceName);
    }
}

HWTEST_F(BuiltInSharedTest, GivenRequestedTypeIntermediateWhenGettingResourceNamesThenReturnForReleaseAndGenericResourceNames) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.UseBindlessMode.set(0);
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    std::string deviceIpString = std::to_string(hwInfo.ipVersion.architecture) + "_" + std::to_string(hwInfo.ipVersion.release) + "_" + std::to_string(hwInfo.ipVersion.revision);

    auto resourceNames = getBuiltinResourceNames(EBuiltInOps::copyBufferToBuffer, BuiltinCode::ECodeType::Intermediate, *pDevice);

    std::string expectedResourceNameGeneric = "copy_buffer_to_buffer.builtin_kernel.bc";
    std::string expectedResourceNameForRelease = deviceIpString + "_" + expectedResourceNameGeneric;

    EXPECT_EQ(2u, resourceNames.size());
    EXPECT_EQ(resourceNames[0], expectedResourceNameForRelease);
    EXPECT_EQ(resourceNames[1], expectedResourceNameGeneric);
}

HWTEST_F(BuiltInSharedTest, GivenValidBuiltinTypeAndExtensionWhenCreatingBuiltinResourceNameThenCorrectNameIsReturned) {
    std::string builtinResourceName = createBuiltinResourceName(EBuiltInOps::copyBufferToBuffer, ".bin");
    std::string expectedBuiltinResourceName = std::string(getBuiltinAsString(EBuiltInOps::copyBufferToBuffer)) + ".bin";
    EXPECT_EQ(expectedBuiltinResourceName, builtinResourceName);
}

HWTEST_F(BuiltInSharedTest, GivenValidBuiltinTypeAndAnyTypeWhenGettingBuiltinCodeThenNonEmptyBuiltinIsReturned) {
    auto builtinsLib = std::make_unique<MockBuiltinsLib>();
    auto builtinCode = builtinsLib->getBuiltinCode(EBuiltInOps::copyBufferToBuffer, BuiltinCode::ECodeType::Any, *pDevice);
    EXPECT_EQ(BuiltinCode::ECodeType::Binary, builtinCode.type);
    EXPECT_NE(0U, builtinCode.resource.size());
}
