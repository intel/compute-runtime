/*
 * Copyright (C) 2020-2026 Intel Corporation
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
#include "shared/test/common/mocks/mock_compiler_product_helper.h"
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

    auto resourceNames = BuiltIn::getResourceNames(BuiltIn::Group::copyBufferToBuffer, BuiltIn::CodeType::binary, *pDevice);

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

    auto resourceNames = BuiltIn::getResourceNames(BuiltIn::Group::copyBufferToBuffer, BuiltIn::CodeType::binary, *pDevice);

    std::string expectedResourceNameGeneric = "bindless_copy_buffer_to_buffer.builtin_kernel.bin";
    std::string expectedResourceNameForRelease = deviceIpString + "_" + expectedResourceNameGeneric;

    EXPECT_EQ(1u, resourceNames.size());
    EXPECT_EQ(resourceNames[0], expectedResourceNameForRelease);
}

TEST_F(BuiltInSharedTest, whenTryingToGetBuiltinResourceForUnregisteredPlatformThenOnlyIntermediateFormatIsAvailable) {
    auto builtinsLib = std::make_unique<MockBuiltInResourceLoader>();
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo.ipVersion.value += 0xdead;
    const std::array<BuiltIn::Group, 12> builtinTypes{BuiltIn::Group::copyBufferToBuffer,
                                                      BuiltIn::Group::copyBufferRect,
                                                      BuiltIn::Group::fillBuffer,
                                                      BuiltIn::Group::copyBufferToImage3d,
                                                      BuiltIn::Group::copyImage3dToBuffer,
                                                      BuiltIn::Group::copyImageToImage1d,
                                                      BuiltIn::Group::copyImageToImage2d,
                                                      BuiltIn::Group::copyImageToImage3d,
                                                      BuiltIn::Group::fillImage1d,
                                                      BuiltIn::Group::fillImage2d,
                                                      BuiltIn::Group::fillImage3d,
                                                      BuiltIn::Group::fillImage1dBuffer};

    for (auto &builtinType : builtinTypes) {
        auto binaryBuiltinResource = builtinsLib->getBuiltinResource(builtinType, BuiltIn::CodeType::binary, *pDevice);
        EXPECT_EQ(0U, binaryBuiltinResource.size());

        auto intermediateBuiltinResource = builtinsLib->getBuiltinResource(builtinType, BuiltIn::CodeType::intermediate, *pDevice);
        EXPECT_NE(0U, intermediateBuiltinResource.size());
    }
}

HWTEST2_F(BuiltInSharedTest, GivenStatelessBuiltinWhenGettingResourceNameThenAddressingIsStateless, HasStatefulSupport) {
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    std::string deviceIpString = std::to_string(hwInfo.ipVersion.architecture) + "_" + std::to_string(hwInfo.ipVersion.release) + "_" + std::to_string(hwInfo.ipVersion.revision);

    auto resourceNames = BuiltIn::getResourceNames(BuiltIn::Group::copyBufferToBufferStateless, BuiltIn::CodeType::binary, *pDevice);

    std::string expectedResourceNameGeneric = "stateless_copy_buffer_to_buffer_stateless.builtin_kernel.bin";
    std::string expectedResourceNameForRelease = deviceIpString + "_" + expectedResourceNameGeneric;

    EXPECT_EQ(1u, resourceNames.size());
    EXPECT_EQ(resourceNames[0], expectedResourceNameForRelease);
}

HWTEST2_F(BuiltInSharedTest, GivenPlatformWithoutStatefulAddresingSupportWhenGettingResourceNamesThenStatelessResourceNameIsReturned, HasNoStatefulSupport) {
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    std::string deviceIpString = std::to_string(hwInfo.ipVersion.architecture) + "_" + std::to_string(hwInfo.ipVersion.release) + "_" + std::to_string(hwInfo.ipVersion.revision);
    {
        auto resourceNames = BuiltIn::getResourceNames(BuiltIn::Group::copyBufferToBuffer, BuiltIn::CodeType::binary, *pDevice);
        std::string expectedResourceName = deviceIpString + "_stateless_copy_buffer_to_buffer.builtin_kernel.bin";
        EXPECT_EQ(1u, resourceNames.size());
        EXPECT_EQ(resourceNames[0], expectedResourceName);
    }

    {
        auto resourceNames = BuiltIn::getResourceNames(BuiltIn::Group::copyBufferToBufferStateless, BuiltIn::CodeType::binary, *pDevice);
        std::string expectedResourceName = deviceIpString + "_stateless_copy_buffer_to_buffer_stateless.builtin_kernel.bin";
        EXPECT_EQ(1u, resourceNames.size());
        EXPECT_EQ(resourceNames[0], expectedResourceName);
    }
}

TEST_F(BuiltInSharedTest, GivenRequestedTypeIntermediateWhenGettingResourceNamesThenReturnForReleaseAndGenericResourceNames) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.UseBindlessMode.set(0);
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    std::string deviceIpString = std::to_string(hwInfo.ipVersion.architecture) + "_" + std::to_string(hwInfo.ipVersion.release) + "_" + std::to_string(hwInfo.ipVersion.revision);

    auto resourceNames = BuiltIn::getResourceNames(BuiltIn::Group::copyBufferToBuffer, BuiltIn::CodeType::intermediate, *pDevice);

    std::string expectedResourceNameGeneric = "copy_buffer_to_buffer.builtin_kernel.spv";
    std::string expectedResourceNameForRelease = deviceIpString + "_" + expectedResourceNameGeneric;

    EXPECT_EQ(2u, resourceNames.size());
    EXPECT_EQ(resourceNames[0], expectedResourceNameForRelease);
    EXPECT_EQ(resourceNames[1], expectedResourceNameGeneric);
}

TEST_F(BuiltInSharedTest, GivenValidBuiltinTypeAndExtensionWhenCreatingBuiltinResourceNameThenCorrectNameIsReturned) {

    const std::pair<BuiltIn::Group, const char *> testCases[] = {
        {BuiltIn::Group::auxTranslation, "aux_translation.builtin_kernel"},
        {BuiltIn::Group::copyBufferToBuffer, "copy_buffer_to_buffer.builtin_kernel"},
        {BuiltIn::Group::copyBufferToBufferStateless, "copy_buffer_to_buffer_stateless.builtin_kernel"},
        {BuiltIn::Group::copyBufferToBufferStatelessHeapless, "copy_buffer_to_buffer_stateless.builtin_kernel"},
        {BuiltIn::Group::copyBufferRect, "copy_buffer_rect.builtin_kernel"},
        {BuiltIn::Group::copyBufferRectStateless, "copy_buffer_rect_stateless.builtin_kernel"},
        {BuiltIn::Group::copyBufferRectStatelessHeapless, "copy_buffer_rect_stateless.builtin_kernel"},
        {BuiltIn::Group::fillBuffer, "fill_buffer.builtin_kernel"},
        {BuiltIn::Group::fillBufferStateless, "fill_buffer_stateless.builtin_kernel"},
        {BuiltIn::Group::fillBufferStatelessHeapless, "fill_buffer_stateless.builtin_kernel"},
        {BuiltIn::Group::copyBufferToImage3d, "copy_buffer_to_image3d.builtin_kernel"},
        {BuiltIn::Group::copyBufferToImage3dStateless, "copy_buffer_to_image3d_stateless.builtin_kernel"},
        {BuiltIn::Group::copyBufferToImage3dStatelessHeapless, "copy_buffer_to_image3d_stateless.builtin_kernel"},
        {BuiltIn::Group::copyImage3dToBuffer, "copy_image3d_to_buffer.builtin_kernel"},
        {BuiltIn::Group::copyImage3dToBufferStateless, "copy_image3d_to_buffer_stateless.builtin_kernel"},
        {BuiltIn::Group::copyImage3dToBufferStatelessHeapless, "copy_image3d_to_buffer_stateless.builtin_kernel"},
        {BuiltIn::Group::copyImageToImage1d, "copy_image_to_image1d.builtin_kernel"},
        {BuiltIn::Group::copyImageToImage1dHeapless, "copy_image_to_image1d.builtin_kernel"},
        {BuiltIn::Group::copyImageToImage2d, "copy_image_to_image2d.builtin_kernel"},
        {BuiltIn::Group::copyImageToImage2dHeapless, "copy_image_to_image2d.builtin_kernel"},
        {BuiltIn::Group::copyImageToImage3d, "copy_image_to_image3d.builtin_kernel"},
        {BuiltIn::Group::copyImageToImage3dHeapless, "copy_image_to_image3d.builtin_kernel"},
        {BuiltIn::Group::fillImage1d, "fill_image1d.builtin_kernel"},
        {BuiltIn::Group::fillImage1dHeapless, "fill_image1d.builtin_kernel"},
        {BuiltIn::Group::fillImage2d, "fill_image2d.builtin_kernel"},
        {BuiltIn::Group::fillImage2dHeapless, "fill_image2d.builtin_kernel"},
        {BuiltIn::Group::fillImage3d, "fill_image3d.builtin_kernel"},
        {BuiltIn::Group::fillImage3dHeapless, "fill_image3d.builtin_kernel"},
        {BuiltIn::Group::queryKernelTimestamps, "copy_kernel_timestamps.builtin_kernel"},
        {BuiltIn::Group::fillImage1dBuffer, "fill_image1d_buffer.builtin_kernel"},
        {BuiltIn::Group::fillImage1dBufferHeapless, "fill_image1d_buffer.builtin_kernel"}};

    for (const auto &[type, name] : testCases) {
        std::string builtinResourceName = BuiltIn::createResourceName(type, ".bin");
        std::string expectedBuiltinResourceName = std::string(name) + ".bin";
        EXPECT_EQ(expectedBuiltinResourceName, builtinResourceName);
    }
}

TEST_F(BuiltInSharedTest, GivenValidBuiltinTypeAndAnyTypeWhenGettingBuiltinCodeThenNonEmptyBuiltinIsReturned) {
    auto builtinsLib = std::make_unique<MockBuiltInResourceLoader>();
    auto builtinCode = builtinsLib->getBuiltinCode(BuiltIn::Group::copyBufferToBuffer, BuiltIn::CodeType::any, *pDevice);
    EXPECT_EQ(BuiltIn::CodeType::binary, builtinCode.type);
    EXPECT_NE(0U, builtinCode.resource.size());
}

TEST_F(BuiltInSharedTest, GivenHeaplessModeEnabledWhenGetBuiltinResourceNamesIsCalledThenResourceNameIsCorrect) {

    class BuiltinMockCompilerProductHelper : public MockCompilerProductHelper {
      public:
        bool isHeaplessModeEnabled(const HardwareInfo &hwInfo) const override {
            return true;
        }
    };

    pDevice->executionEnvironment->rootDeviceEnvironments[0]->compilerProductHelper.reset(new BuiltinMockCompilerProductHelper());

    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();

    std::string deviceIpString = std::to_string(hwInfo.ipVersion.architecture) + "_" + std::to_string(hwInfo.ipVersion.release) + "_" + std::to_string(hwInfo.ipVersion.revision);

    struct TestParam {
        std::string builtInGroupAsString;
        BuiltIn::Group builtinType;
    };

    TestParam params[] = {
        {"copy_buffer_to_buffer_stateless", BuiltIn::Group::copyBufferToBufferStatelessHeapless},
        {"copy_buffer_rect_stateless", BuiltIn::Group::copyBufferRectStatelessHeapless},
        {"fill_buffer_stateless", BuiltIn::Group::fillBufferStatelessHeapless}};

    for (auto &[builtInGroupAsString, builtInGroup] : params) {

        auto resourceNames = BuiltIn::getResourceNames(builtInGroup, BuiltIn::CodeType::binary, *pDevice);

        std::string expectedResourceNameGeneric = "stateless_heapless_" + builtInGroupAsString + ".builtin_kernel.bin";
        std::string expectedResourceNameForRelease = deviceIpString + "_" + expectedResourceNameGeneric;

        EXPECT_EQ(1u, resourceNames.size());
        EXPECT_EQ(resourceNames[0], expectedResourceNameForRelease);
    }
}

TEST_F(BuiltInSharedTest, GivenRequestedTypeWhenGettingResourceNamesThenReturnReleaseForAllWideOps) {
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    std::string deviceIpString = std::to_string(hwInfo.ipVersion.architecture) + "_" +
                                 std::to_string(hwInfo.ipVersion.release) + "_" +
                                 std::to_string(hwInfo.ipVersion.revision);

    struct WideCase {
        BuiltIn::Group op;
        const char *genericResource;
    };

    const WideCase wideCases[] = {
        {BuiltIn::Group::copyBufferToBufferWideStateless, "wide_stateless_copy_buffer_to_buffer_stateless.builtin_kernel.bin"},
        {BuiltIn::Group::copyBufferToBufferWideStatelessHeapless, "wide_stateless_heapless_copy_buffer_to_buffer_stateless.builtin_kernel.bin"},
        {BuiltIn::Group::copyBufferRectWideStateless, "wide_stateless_copy_buffer_rect_stateless.builtin_kernel.bin"},
        {BuiltIn::Group::copyBufferRectWideStatelessHeapless, "wide_stateless_heapless_copy_buffer_rect_stateless.builtin_kernel.bin"},
        {BuiltIn::Group::fillBufferWideStateless, "wide_stateless_fill_buffer_stateless.builtin_kernel.bin"},
        {BuiltIn::Group::fillBufferWideStatelessHeapless, "wide_stateless_heapless_fill_buffer_stateless.builtin_kernel.bin"},
        {BuiltIn::Group::copyBufferToImage3dWideStateless, "wide_stateless_copy_buffer_to_image3d_stateless.builtin_kernel.bin"},
        {BuiltIn::Group::copyBufferToImage3dWideStatelessHeapless, "wide_stateless_heapless_copy_buffer_to_image3d_stateless.builtin_kernel.bin"},
        {BuiltIn::Group::copyImage3dToBufferWideStateless, "wide_stateless_copy_image3d_to_buffer_stateless.builtin_kernel.bin"},
        {BuiltIn::Group::copyImage3dToBufferWideStatelessHeapless, "wide_stateless_heapless_copy_image3d_to_buffer_stateless.builtin_kernel.bin"},
    };

    for (const auto &tc : wideCases) {
        auto resourceNames = BuiltIn::getResourceNames(tc.op, BuiltIn::CodeType::binary, *pDevice);

        std::string expectedGeneric = tc.genericResource;
        std::string expectedForRelease = deviceIpString + "_" + expectedGeneric;

        EXPECT_EQ(1u, resourceNames.size());
        EXPECT_EQ(expectedForRelease, resourceNames[0]);
    }
}

TEST_F(BuiltInSharedTest, GivenRequestedTypeIntermediateWhenGettingWideStatelessBuiltinsThenReturnForReleaseAndGenericResourceNames) {
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    std::string deviceIpString = std::to_string(hwInfo.ipVersion.architecture) + "_" + std::to_string(hwInfo.ipVersion.release) + "_" + std::to_string(hwInfo.ipVersion.revision);

    auto resourceNames = BuiltIn::getResourceNames(BuiltIn::Group::copyBufferToBufferWideStateless, BuiltIn::CodeType::intermediate, *pDevice);

    std::string expectedResourceNameGeneric = "wide_stateless_copy_buffer_to_buffer_stateless.builtin_kernel.spv";
    std::string expectedResourceNameForRelease = deviceIpString + "_" + expectedResourceNameGeneric;

    EXPECT_EQ(2u, resourceNames.size());
    EXPECT_EQ(resourceNames[0], expectedResourceNameForRelease);
    EXPECT_EQ(resourceNames[1], expectedResourceNameGeneric);
}

TEST_F(BuiltInSharedTest, GivenRequestedTypeSourceWhenGettingWideStatelessBuiltinsThenReturnForReleaseAndGenericResourceNames) {
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    std::string deviceIpString = std::to_string(hwInfo.ipVersion.architecture) + "_" + std::to_string(hwInfo.ipVersion.release) + "_" + std::to_string(hwInfo.ipVersion.revision);

    auto resourceNames = BuiltIn::getResourceNames(BuiltIn::Group::copyBufferToBufferWideStateless, BuiltIn::CodeType::source, *pDevice);

    std::string expectedResourceNameGeneric = "copy_buffer_to_buffer_stateless.builtin_kernel.cl";
    std::string expectedResourceNameForRelease = deviceIpString + "_" + expectedResourceNameGeneric;

    EXPECT_EQ(2u, resourceNames.size());
    EXPECT_EQ(resourceNames[0], expectedResourceNameForRelease);
    EXPECT_EQ(resourceNames[1], expectedResourceNameGeneric);
}
