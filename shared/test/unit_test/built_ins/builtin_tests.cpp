/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/helpers/compiler_product_helper.h"
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

    auto resourceNames = getBuiltinResourceNames(EBuiltInOps::copyBufferToBuffer, BuiltinCode::ECodeType::binary, *pDevice);

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

    auto resourceNames = getBuiltinResourceNames(EBuiltInOps::copyBufferToBuffer, BuiltinCode::ECodeType::binary, *pDevice);

    std::string expectedResourceNameGeneric = "bindless_copy_buffer_to_buffer.builtin_kernel.bin";
    std::string expectedResourceNameForRelease = deviceIpString + "_" + expectedResourceNameGeneric;

    EXPECT_EQ(1u, resourceNames.size());
    EXPECT_EQ(resourceNames[0], expectedResourceNameForRelease);
}

HWTEST_F(BuiltInSharedTest, whenTryingToGetBuiltinResourceForUnregisteredPlatformThenOnlyIntermediateFormatIsAvailable) {
    auto builtinsLib = std::make_unique<MockBuiltinsLib>();
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo.ipVersion.value += 0xdead;
    const std::array<uint32_t, 12> builtinTypes{EBuiltInOps::copyBufferToBuffer,
                                                EBuiltInOps::copyBufferRect,
                                                EBuiltInOps::fillBuffer,
                                                EBuiltInOps::copyBufferToImage3d,
                                                EBuiltInOps::copyImage3dToBuffer,
                                                EBuiltInOps::copyImageToImage1d,
                                                EBuiltInOps::copyImageToImage2d,
                                                EBuiltInOps::copyImageToImage3d,
                                                EBuiltInOps::fillImage1d,
                                                EBuiltInOps::fillImage2d,
                                                EBuiltInOps::fillImage3d,
                                                EBuiltInOps::fillImage1dBuffer};

    for (auto &builtinType : builtinTypes) {
        auto binaryBuiltinResource = builtinsLib->getBuiltinResource(builtinType, BuiltinCode::ECodeType::binary, *pDevice);
        EXPECT_EQ(0U, binaryBuiltinResource.size());

        auto intermediateBuiltinResource = builtinsLib->getBuiltinResource(builtinType, BuiltinCode::ECodeType::intermediate, *pDevice);
        EXPECT_NE(0U, intermediateBuiltinResource.size());
    }
}

HWTEST2_F(BuiltInSharedTest, GivenStatelessBuiltinWhenGettingResourceNameThenAddressingIsStateless, HasStatefulSupport) {
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    std::string deviceIpString = std::to_string(hwInfo.ipVersion.architecture) + "_" + std::to_string(hwInfo.ipVersion.release) + "_" + std::to_string(hwInfo.ipVersion.revision);

    auto resourceNames = getBuiltinResourceNames(EBuiltInOps::copyBufferToBufferStateless, BuiltinCode::ECodeType::binary, *pDevice);

    std::string expectedResourceNameGeneric = "stateless_copy_buffer_to_buffer_stateless.builtin_kernel.bin";
    std::string expectedResourceNameForRelease = deviceIpString + "_" + expectedResourceNameGeneric;

    EXPECT_EQ(1u, resourceNames.size());
    EXPECT_EQ(resourceNames[0], expectedResourceNameForRelease);
}

HWTEST2_F(BuiltInSharedTest, GivenPlatformWithoutStatefulAddresingSupportWhenGettingResourceNamesThenStatelessResourceNameIsReturned, HasNoStatefulSupport) {
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    std::string deviceIpString = std::to_string(hwInfo.ipVersion.architecture) + "_" + std::to_string(hwInfo.ipVersion.release) + "_" + std::to_string(hwInfo.ipVersion.revision);
    {
        auto resourceNames = getBuiltinResourceNames(EBuiltInOps::copyBufferToBuffer, BuiltinCode::ECodeType::binary, *pDevice);
        std::string expectedResourceName = deviceIpString + "_stateless_copy_buffer_to_buffer.builtin_kernel.bin";
        EXPECT_EQ(1u, resourceNames.size());
        EXPECT_EQ(resourceNames[0], expectedResourceName);
    }

    {
        auto resourceNames = getBuiltinResourceNames(EBuiltInOps::copyBufferToBufferStateless, BuiltinCode::ECodeType::binary, *pDevice);
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

    auto resourceNames = getBuiltinResourceNames(EBuiltInOps::copyBufferToBuffer, BuiltinCode::ECodeType::intermediate, *pDevice);

    std::string expectedResourceNameGeneric = "copy_buffer_to_buffer.builtin_kernel.spv";
    std::string expectedResourceNameForRelease = deviceIpString + "_" + expectedResourceNameGeneric;

    EXPECT_EQ(2u, resourceNames.size());
    EXPECT_EQ(resourceNames[0], expectedResourceNameForRelease);
    EXPECT_EQ(resourceNames[1], expectedResourceNameGeneric);
}

HWTEST_F(BuiltInSharedTest, GivenValidBuiltinTypeAndExtensionWhenCreatingBuiltinResourceNameThenCorrectNameIsReturned) {

    const std::pair<EBuiltInOps::Type, const char *> testCases[] = {
        {EBuiltInOps::auxTranslation, "aux_translation.builtin_kernel"},
        {EBuiltInOps::copyBufferToBuffer, "copy_buffer_to_buffer.builtin_kernel"},
        {EBuiltInOps::copyBufferToBufferStateless, "copy_buffer_to_buffer_stateless.builtin_kernel"},
        {EBuiltInOps::copyBufferToBufferStatelessHeapless, "copy_buffer_to_buffer_stateless.builtin_kernel"},
        {EBuiltInOps::copyBufferRect, "copy_buffer_rect.builtin_kernel"},
        {EBuiltInOps::copyBufferRectStateless, "copy_buffer_rect_stateless.builtin_kernel"},
        {EBuiltInOps::copyBufferRectStatelessHeapless, "copy_buffer_rect_stateless.builtin_kernel"},
        {EBuiltInOps::fillBuffer, "fill_buffer.builtin_kernel"},
        {EBuiltInOps::fillBufferStateless, "fill_buffer_stateless.builtin_kernel"},
        {EBuiltInOps::fillBufferStatelessHeapless, "fill_buffer_stateless.builtin_kernel"},
        {EBuiltInOps::copyBufferToImage3d, "copy_buffer_to_image3d.builtin_kernel"},
        {EBuiltInOps::copyBufferToImage3dStateless, "copy_buffer_to_image3d_stateless.builtin_kernel"},
        {EBuiltInOps::copyBufferToImage3dHeapless, "copy_buffer_to_image3d_stateless.builtin_kernel"},
        {EBuiltInOps::copyImage3dToBuffer, "copy_image3d_to_buffer.builtin_kernel"},
        {EBuiltInOps::copyImage3dToBufferStateless, "copy_image3d_to_buffer_stateless.builtin_kernel"},
        {EBuiltInOps::copyImage3dToBufferHeapless, "copy_image3d_to_buffer_stateless.builtin_kernel"},
        {EBuiltInOps::copyImageToImage1d, "copy_image_to_image1d.builtin_kernel"},
        {EBuiltInOps::copyImageToImage1dHeapless, "copy_image_to_image1d.builtin_kernel"},
        {EBuiltInOps::copyImageToImage2d, "copy_image_to_image2d.builtin_kernel"},
        {EBuiltInOps::copyImageToImage2dHeapless, "copy_image_to_image2d.builtin_kernel"},
        {EBuiltInOps::copyImageToImage3d, "copy_image_to_image3d.builtin_kernel"},
        {EBuiltInOps::copyImageToImage3dHeapless, "copy_image_to_image3d.builtin_kernel"},
        {EBuiltInOps::fillImage1d, "fill_image1d.builtin_kernel"},
        {EBuiltInOps::fillImage1dHeapless, "fill_image1d.builtin_kernel"},
        {EBuiltInOps::fillImage2d, "fill_image2d.builtin_kernel"},
        {EBuiltInOps::fillImage2dHeapless, "fill_image2d.builtin_kernel"},
        {EBuiltInOps::fillImage3d, "fill_image3d.builtin_kernel"},
        {EBuiltInOps::fillImage3dHeapless, "fill_image3d.builtin_kernel"},
        {EBuiltInOps::queryKernelTimestamps, "copy_kernel_timestamps.builtin_kernel"},
        {EBuiltInOps::fillImage1dBuffer, "fill_image1d_buffer.builtin_kernel"},
        {EBuiltInOps::fillImage1dBufferHeapless, "fill_image1d_buffer.builtin_kernel"}};

    for (const auto &[type, name] : testCases) {
        std::string builtinResourceName = createBuiltinResourceName(type, ".bin");
        std::string expectedBuiltinResourceName = std::string(name) + ".bin";
        EXPECT_EQ(expectedBuiltinResourceName, builtinResourceName);
    }
}

HWTEST_F(BuiltInSharedTest, GivenValidBuiltinTypeAndAnyTypeWhenGettingBuiltinCodeThenNonEmptyBuiltinIsReturned) {
    auto builtinsLib = std::make_unique<MockBuiltinsLib>();
    auto builtinCode = builtinsLib->getBuiltinCode(EBuiltInOps::copyBufferToBuffer, BuiltinCode::ECodeType::any, *pDevice);
    EXPECT_EQ(BuiltinCode::ECodeType::binary, builtinCode.type);
    EXPECT_NE(0U, builtinCode.resource.size());
}

HWTEST2_F(BuiltInSharedTest, GivenHeaplessModeEnabledWhenGetBuiltinResourceNamesIsCalledThenResourceNameIsCorrect, MatchAny) {

    class MockCompilerProductHelper : public CompilerProductHelperHw<productFamily> {
      public:
        bool isHeaplessModeEnabled(const HardwareInfo &hwInfo) const override {
            return true;
        }
    };

    pDevice->executionEnvironment->rootDeviceEnvironments[0]->compilerProductHelper.reset(new MockCompilerProductHelper());

    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();

    std::string deviceIpString = std::to_string(hwInfo.ipVersion.architecture) + "_" + std::to_string(hwInfo.ipVersion.release) + "_" + std::to_string(hwInfo.ipVersion.revision);

    struct TestParam {
        std::string builtInTypeAsString;
        EBuiltInOps::Type builtinType;
    };

    TestParam params[] = {
        {"copy_buffer_to_buffer_stateless", EBuiltInOps::copyBufferToBufferStatelessHeapless},
        {"copy_buffer_rect_stateless", EBuiltInOps::copyBufferRectStatelessHeapless},
        {"fill_buffer_stateless", EBuiltInOps::fillBufferStatelessHeapless}};

    for (auto &[builtInTypeAsString, builtInType] : params) {

        auto resourceNames = getBuiltinResourceNames(builtInType, BuiltinCode::ECodeType::binary, *pDevice);

        std::string expectedResourceNameGeneric = "heapless_" + builtInTypeAsString + ".builtin_kernel.bin";
        std::string expectedResourceNameForRelease = deviceIpString + "_" + expectedResourceNameGeneric;

        EXPECT_EQ(1u, resourceNames.size());
        EXPECT_EQ(resourceNames[0], expectedResourceNameForRelease);
    }
}
