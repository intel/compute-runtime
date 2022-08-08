/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/test/common/built_ins/built_in_tests_shared.inl"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

#include <string>

using namespace NEO;

TEST(BuiltInTestsL0, givenUseBindlessBuiltinInApiDependentModeWhenBinExtensionPassedThenNameHasCorrectPrefix) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseBindlessMode.set(-1);
    EBuiltInOps::Type builtin = EBuiltInOps::CopyBufferToBuffer;
    const std::string extension = ".bin";
    const std::string platformName = "platformName";
    const uint32_t deviceRevId = 123;

    std::string prefix = ApiSpecificConfig::getBindlessConfiguration() ? "bindless" : "bindful";

    std::string resourceNameGeneric = createBuiltinResourceName(builtin, extension);
    std::string resourceNameForPlatform = createBuiltinResourceName(builtin, extension, platformName);
    std::string resourceNameForPlatformAndStepping = createBuiltinResourceName(builtin, extension, platformName, deviceRevId);

    std::string expectedResourceNameGeneric = prefix + "_copy_buffer_to_buffer.builtin_kernel.bin";

    std::string expectedResourceNameForPlatform = platformName.c_str();
    expectedResourceNameForPlatform += "_0_" + prefix + "_copy_buffer_to_buffer.builtin_kernel.bin";

    std::string expectedResourceNameForPlatformAndStepping = platformName.c_str();
    expectedResourceNameForPlatformAndStepping += "_";
    expectedResourceNameForPlatformAndStepping += std::to_string(deviceRevId).c_str();
    expectedResourceNameForPlatformAndStepping += "_" + prefix + "_copy_buffer_to_buffer.builtin_kernel.bin";

    EXPECT_EQ(0, strcmp(expectedResourceNameGeneric.c_str(), resourceNameGeneric.c_str()));
    EXPECT_EQ(0, strcmp(expectedResourceNameForPlatform.c_str(), resourceNameForPlatform.c_str()));
    EXPECT_EQ(0, strcmp(expectedResourceNameForPlatformAndStepping.c_str(), resourceNameForPlatformAndStepping.c_str()));
}
TEST(BuiltInTestsL0, givenUseBindlessBuiltinDisabledInL0ApiWhenBinExtensionPassedThenNameHasBindfulPrefix) {
    givenUseBindlessBuiltinDisabledWhenBinExtensionPassedThenNameHasBindfulPrefix();
}

TEST(BuiltInTestsL0, givenUseBindlessBuiltinEnabledInL0ApiWhenBinExtensionPassedThenNameHasBindlessPrefix) {
    givenUseBindlessBuiltinEnabledWhenBinExtensionPassedThenNameHasBindlessPrefix();
}

TEST(BuiltInTestsL0, GivenBindlessConfigWhenGettingBuiltinResourceThenResourceSizeIsNonZero) {

    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseBindlessMode.set(1);

    auto hwInfo = *defaultHwInfo;
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    auto mockBuiltinsLib = std::unique_ptr<MockBuiltinsLib>(new MockBuiltinsLib());

    ASSERT_LE(2u, mockBuiltinsLib->allStorages.size());

    bool bindlessFound = false;

    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    std::string resourceNameGeneric = createBuiltinResourceName(EBuiltInOps::CopyBufferToBuffer, BuiltinCode::getExtension(BuiltinCode::ECodeType::Binary));
    std::string resourceNameForPlatformType = createBuiltinResourceName(EBuiltInOps::CopyBufferToBuffer, BuiltinCode::getExtension(BuiltinCode::ECodeType::Binary),
                                                                        getFamilyNameWithType(hwInfo),
                                                                        hwHelper.getDefaultRevisionId(hwInfo));
    std::string resourceNameForPlatformTypeAndStepping = createBuiltinResourceName(EBuiltInOps::CopyBufferToBuffer, BuiltinCode::getExtension(BuiltinCode::ECodeType::Binary),
                                                                                   getFamilyNameWithType(hwInfo),
                                                                                   hwInfo.platform.usRevId);

    for (const auto &storage : mockBuiltinsLib->allStorages) {
        if (storage->load(resourceNameForPlatformTypeAndStepping).size() != 0) {
            bindlessFound = true;
        }
    }

    EXPECT_TRUE(bindlessFound);

    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyBufferToBuffer, BuiltinCode::ECodeType::Binary, *device).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyBufferRect, BuiltinCode::ECodeType::Binary, *device).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::FillBuffer, BuiltinCode::ECodeType::Binary, *device).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::QueryKernelTimestamps, BuiltinCode::ECodeType::Binary, *device).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::COUNT, BuiltinCode::ECodeType::Binary, *device).size());
}
