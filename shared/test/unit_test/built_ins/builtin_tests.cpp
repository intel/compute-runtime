/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_builtinslib.h"
#include "shared/test/common/mocks/mock_compiler_product_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

#include <string>

using BuiltInSharedTest = Test<DeviceFixture>;

TEST_F(BuiltInSharedTest, whenTryingToGetBuiltinResourceForUnregisteredPlatformThenOnlyIntermediateFormatIsAvailable) {
    auto builtinsLib = std::make_unique<MockBuiltInResourceLoader>();
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo.ipVersion.value += 0xdead;
    const std::array<BuiltIn::BaseKernel, 12> builtinTypes{BuiltIn::BaseKernel::copyBufferToBuffer,
                                                           BuiltIn::BaseKernel::copyBufferRect,
                                                           BuiltIn::BaseKernel::fillBuffer,
                                                           BuiltIn::BaseKernel::copyBufferToImage3d,
                                                           BuiltIn::BaseKernel::copyImage3dToBuffer,
                                                           BuiltIn::BaseKernel::copyImageToImage1d,
                                                           BuiltIn::BaseKernel::copyImageToImage2d,
                                                           BuiltIn::BaseKernel::copyImageToImage3d,
                                                           BuiltIn::BaseKernel::fillImage1d,
                                                           BuiltIn::BaseKernel::fillImage2d,
                                                           BuiltIn::BaseKernel::fillImage3d,
                                                           BuiltIn::BaseKernel::fillImage1dBuffer};

    for (auto &builtinType : builtinTypes) {
        auto binaryBuiltinResource = builtinsLib->getBuiltinResource(builtinType, defaultBuiltInMode, BuiltIn::CodeType::binary, *pDevice);
        EXPECT_EQ(0U, binaryBuiltinResource.size());

        auto intermediateBuiltinResource = builtinsLib->getBuiltinResource(builtinType, defaultBuiltInMode, BuiltIn::CodeType::intermediate, *pDevice);
        EXPECT_NE(0U, intermediateBuiltinResource.size());
    }
}

HWTEST_F(BuiltInSharedTest, WhenGettingResourceNameThenResourceNamesAreCorrect) {
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    std::string deviceIpString = std::to_string(hwInfo.ipVersion.architecture) + "_" + std::to_string(hwInfo.ipVersion.release) + "_" + std::to_string(hwInfo.ipVersion.revision);

    const std::pair<BuiltIn::BaseKernel, const char *> testCases[] = {
        {BuiltIn::BaseKernel::auxTranslation, "aux_translation.builtin_kernel"},
        {BuiltIn::BaseKernel::copyBufferToBuffer, "copy_buffer_to_buffer.builtin_kernel"},
        {BuiltIn::BaseKernel::copyBufferRect, "copy_buffer_rect.builtin_kernel"},
        {BuiltIn::BaseKernel::fillBuffer, "fill_buffer.builtin_kernel"},
        {BuiltIn::BaseKernel::copyBufferToImage3d, "copy_buffer_to_image3d.builtin_kernel"},
        {BuiltIn::BaseKernel::copyImage3dToBuffer, "copy_image3d_to_buffer.builtin_kernel"},
        {BuiltIn::BaseKernel::copyImageToImage1d, "copy_image_to_image1d.builtin_kernel"},
        {BuiltIn::BaseKernel::copyImageToImage2d, "copy_image_to_image2d.builtin_kernel"},
        {BuiltIn::BaseKernel::copyImageToImage3d, "copy_image_to_image3d.builtin_kernel"},
        {BuiltIn::BaseKernel::fillImage1d, "fill_image1d.builtin_kernel"},
        {BuiltIn::BaseKernel::fillImage2d, "fill_image2d.builtin_kernel"},
        {BuiltIn::BaseKernel::fillImage3d, "fill_image3d.builtin_kernel"},
        {BuiltIn::BaseKernel::copyKernelTimestamps, "copy_kernel_timestamps.builtin_kernel"},
        {BuiltIn::BaseKernel::fillImage1dBuffer, "fill_image1d_buffer.builtin_kernel"}};

    for (auto &[builtinType, expectedName] : testCases) {
        auto resourceNames = BuiltIn::getResourceNames(builtinType, defaultBuiltInMode, BuiltIn::CodeType::binary, *pDevice);

        std::string expectedAddressingModePrefix = defaultBuiltInMode.toString();
        std::string expectedResourceNameGeneric = expectedAddressingModePrefix + expectedName + ".bin";
        std::string expectedResourceNameForRelease = deviceIpString + "_" + expectedResourceNameGeneric;

        EXPECT_EQ(1u, resourceNames.size());
        EXPECT_EQ(resourceNames[0], expectedResourceNameForRelease);
    }
}

TEST_F(BuiltInSharedTest, GivenRequestedTypeIntermediateWhenGettingResourceNamesThenReturnForReleaseAndGenericResourceNames) {

    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    std::string deviceIpString = std::to_string(hwInfo.ipVersion.architecture) + "_" + std::to_string(hwInfo.ipVersion.release) + "_" + std::to_string(hwInfo.ipVersion.revision);

    const std::pair<BuiltIn::BaseKernel, const char *> testCases[] = {
        {BuiltIn::BaseKernel::auxTranslation, "aux_translation.builtin_kernel"},
        {BuiltIn::BaseKernel::copyBufferToBuffer, "copy_buffer_to_buffer.builtin_kernel"},
        {BuiltIn::BaseKernel::copyBufferRect, "copy_buffer_rect.builtin_kernel"},
        {BuiltIn::BaseKernel::fillBuffer, "fill_buffer.builtin_kernel"},
        {BuiltIn::BaseKernel::copyBufferToImage3d, "copy_buffer_to_image3d.builtin_kernel"},
        {BuiltIn::BaseKernel::copyImage3dToBuffer, "copy_image3d_to_buffer.builtin_kernel"},
        {BuiltIn::BaseKernel::copyImageToImage1d, "copy_image_to_image1d.builtin_kernel"},
        {BuiltIn::BaseKernel::copyImageToImage2d, "copy_image_to_image2d.builtin_kernel"},
        {BuiltIn::BaseKernel::copyImageToImage3d, "copy_image_to_image3d.builtin_kernel"},
        {BuiltIn::BaseKernel::fillImage1d, "fill_image1d.builtin_kernel"},
        {BuiltIn::BaseKernel::fillImage2d, "fill_image2d.builtin_kernel"},
        {BuiltIn::BaseKernel::fillImage3d, "fill_image3d.builtin_kernel"},
        {BuiltIn::BaseKernel::copyKernelTimestamps, "copy_kernel_timestamps.builtin_kernel"},
        {BuiltIn::BaseKernel::fillImage1dBuffer, "fill_image1d_buffer.builtin_kernel"}};

    for (auto &[builtinType, expectedName] : testCases) {

        auto resourceNames = BuiltIn::getResourceNames(builtinType, defaultBuiltInMode, BuiltIn::CodeType::intermediate, *pDevice);

        std::string expectedAddressingModePrefix = defaultBuiltInMode.toString();
        std::string expectedResourceNameGeneric = expectedAddressingModePrefix + expectedName + ".spv";
        std::string expectedResourceNameForRelease = deviceIpString + "_" + expectedResourceNameGeneric;

        EXPECT_EQ(2u, resourceNames.size());
        EXPECT_EQ(resourceNames[0], expectedResourceNameForRelease);
        EXPECT_EQ(resourceNames[1], expectedResourceNameGeneric);
    }
}

TEST_F(BuiltInSharedTest, GivenRequestedTypeSourceWhenGettingResourceNamesThenReturnForReleaseAndGenericResourceNames) {
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    std::string deviceIpString = std::to_string(hwInfo.ipVersion.architecture) + "_" + std::to_string(hwInfo.ipVersion.release) + "_" + std::to_string(hwInfo.ipVersion.revision);

    const std::pair<BuiltIn::BaseKernel, const char *> testCases[] = {
        {BuiltIn::BaseKernel::auxTranslation, "aux_translation.builtin_kernel"},
        {BuiltIn::BaseKernel::copyBufferToBuffer, "copy_buffer_to_buffer.builtin_kernel"},
        {BuiltIn::BaseKernel::copyBufferRect, "copy_buffer_rect.builtin_kernel"},
        {BuiltIn::BaseKernel::fillBuffer, "fill_buffer.builtin_kernel"},
        {BuiltIn::BaseKernel::copyBufferToImage3d, "copy_buffer_to_image3d.builtin_kernel"},
        {BuiltIn::BaseKernel::copyImage3dToBuffer, "copy_image3d_to_buffer.builtin_kernel"},
        {BuiltIn::BaseKernel::copyImageToImage1d, "copy_image_to_image1d.builtin_kernel"},
        {BuiltIn::BaseKernel::copyImageToImage2d, "copy_image_to_image2d.builtin_kernel"},
        {BuiltIn::BaseKernel::copyImageToImage3d, "copy_image_to_image3d.builtin_kernel"},
        {BuiltIn::BaseKernel::fillImage1d, "fill_image1d.builtin_kernel"},
        {BuiltIn::BaseKernel::fillImage2d, "fill_image2d.builtin_kernel"},
        {BuiltIn::BaseKernel::fillImage3d, "fill_image3d.builtin_kernel"},
        {BuiltIn::BaseKernel::copyKernelTimestamps, "copy_kernel_timestamps.builtin_kernel"},
        {BuiltIn::BaseKernel::fillImage1dBuffer, "fill_image1d_buffer.builtin_kernel"}};

    for (auto &[builtinType, expectedName] : testCases) {
        auto resourceNames = BuiltIn::getResourceNames(builtinType, defaultBuiltInMode, BuiltIn::CodeType::source, *pDevice);

        std::string expectedResourceNameGeneric = expectedName;
        expectedResourceNameGeneric += ".cl";
        std::string expectedResourceNameForRelease = deviceIpString + "_" + expectedResourceNameGeneric;

        EXPECT_EQ(2u, resourceNames.size());
        EXPECT_EQ(resourceNames[0], expectedResourceNameForRelease);
        EXPECT_EQ(resourceNames[1], expectedResourceNameGeneric);
    }
}

TEST_F(BuiltInSharedTest, GivenValidBuiltinTypeAndExtensionWhenCreatingBuiltinResourceNameThenCorrectNameIsReturned) {

    const std::pair<BuiltIn::BaseKernel, const char *> testCases[] = {
        {BuiltIn::BaseKernel::auxTranslation, "aux_translation.builtin_kernel"},
        {BuiltIn::BaseKernel::copyBufferToBuffer, "copy_buffer_to_buffer.builtin_kernel"},
        {BuiltIn::BaseKernel::copyBufferRect, "copy_buffer_rect.builtin_kernel"},
        {BuiltIn::BaseKernel::fillBuffer, "fill_buffer.builtin_kernel"},
        {BuiltIn::BaseKernel::copyBufferToImage3d, "copy_buffer_to_image3d.builtin_kernel"},
        {BuiltIn::BaseKernel::copyImage3dToBuffer, "copy_image3d_to_buffer.builtin_kernel"},
        {BuiltIn::BaseKernel::copyImageToImage1d, "copy_image_to_image1d.builtin_kernel"},
        {BuiltIn::BaseKernel::copyImageToImage2d, "copy_image_to_image2d.builtin_kernel"},
        {BuiltIn::BaseKernel::copyImageToImage3d, "copy_image_to_image3d.builtin_kernel"},
        {BuiltIn::BaseKernel::fillImage1d, "fill_image1d.builtin_kernel"},
        {BuiltIn::BaseKernel::fillImage2d, "fill_image2d.builtin_kernel"},
        {BuiltIn::BaseKernel::fillImage3d, "fill_image3d.builtin_kernel"},
        {BuiltIn::BaseKernel::copyKernelTimestamps, "copy_kernel_timestamps.builtin_kernel"},
        {BuiltIn::BaseKernel::fillImage1dBuffer, "fill_image1d_buffer.builtin_kernel"}};

    for (const auto &[type, name] : testCases) {
        std::string builtinResourceName = BuiltIn::createResourceName(type, ".bin");
        std::string expectedBuiltinResourceName = std::string(name) + ".bin";
        EXPECT_EQ(expectedBuiltinResourceName, builtinResourceName);
    }
}

HWTEST_F(BuiltInSharedTest, GivenValidBuiltinTypeAndAnyTypeWhenGettingBuiltinCodeThenNonEmptyBuiltinIsReturned) {
    auto builtinsLib = std::make_unique<MockBuiltInResourceLoader>();

    auto builtinCode = builtinsLib->getBuiltinCode(BuiltIn::BaseKernel::copyBufferToBuffer, defaultBuiltInMode, BuiltIn::CodeType::any, *pDevice);
    EXPECT_EQ(BuiltIn::CodeType::binary, builtinCode.type);
    EXPECT_NE(0U, builtinCode.resource.size());
}

TEST_F(BuiltInSharedTest, GivenRequestedTypeWhenGettingResourceNamesThenReturnReleaseForAllWideOps) {
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    std::string deviceIpString = std::to_string(hwInfo.ipVersion.architecture) + "_" +
                                 std::to_string(hwInfo.ipVersion.release) + "_" +
                                 std::to_string(hwInfo.ipVersion.revision);

    struct WideCase {
        BuiltIn::BaseKernel kernel;
        BuiltIn::AddressingMode mode;
        const char *filename;
    };

    const WideCase wideCases[] = {
        {BuiltIn::BaseKernel::copyBufferToBuffer, BuiltIn::bindfulImageStatelessBufferWide, "bindful_image_stateless_buffer_wide_copy_buffer_to_buffer.builtin_kernel.bin"},
        {BuiltIn::BaseKernel::copyBufferToBuffer, BuiltIn::bindlessImageStatelessBufferWide, "bindless_image_stateless_buffer_wide_copy_buffer_to_buffer.builtin_kernel.bin"},
        {BuiltIn::BaseKernel::copyBufferRect, BuiltIn::bindfulImageStatelessBufferWide, "bindful_image_stateless_buffer_wide_copy_buffer_rect.builtin_kernel.bin"},
        {BuiltIn::BaseKernel::copyBufferRect, BuiltIn::bindlessImageStatelessBufferWide, "bindless_image_stateless_buffer_wide_copy_buffer_rect.builtin_kernel.bin"},
        {BuiltIn::BaseKernel::fillBuffer, BuiltIn::bindfulImageStatelessBufferWide, "bindful_image_stateless_buffer_wide_fill_buffer.builtin_kernel.bin"},
        {BuiltIn::BaseKernel::fillBuffer, BuiltIn::bindlessImageStatelessBufferWide, "bindless_image_stateless_buffer_wide_fill_buffer.builtin_kernel.bin"},
        {BuiltIn::BaseKernel::copyBufferToImage3d, BuiltIn::bindfulImageStatelessBufferWide, "bindful_image_stateless_buffer_wide_copy_buffer_to_image3d.builtin_kernel.bin"},
        {BuiltIn::BaseKernel::copyBufferToImage3d, BuiltIn::bindlessImageStatelessBufferWide, "bindless_image_stateless_buffer_wide_copy_buffer_to_image3d.builtin_kernel.bin"},
        {BuiltIn::BaseKernel::copyImage3dToBuffer, BuiltIn::bindfulImageStatelessBufferWide, "bindful_image_stateless_buffer_wide_copy_image3d_to_buffer.builtin_kernel.bin"},
        {BuiltIn::BaseKernel::copyImage3dToBuffer, BuiltIn::bindlessImageStatelessBufferWide, "bindless_image_stateless_buffer_wide_copy_image3d_to_buffer.builtin_kernel.bin"},
    };

    for (const auto &[baseKernel, addressingMode, filename] : wideCases) {
        auto resourceNames = BuiltIn::getResourceNames(baseKernel, addressingMode, BuiltIn::CodeType::binary, *pDevice);

        std::string expectedGeneric = filename;
        std::string expectedForRelease = deviceIpString + "_" + expectedGeneric;

        EXPECT_EQ(1u, resourceNames.size());
        EXPECT_EQ(expectedForRelease, resourceNames[0]);
    }
}

TEST_F(BuiltInSharedTest, GivenRequestedTypeIntermediateWhenGettingWideStatelessBuiltinsThenReturnForReleaseAndGenericResourceNames) {
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    std::string deviceIpString = std::to_string(hwInfo.ipVersion.architecture) + "_" + std::to_string(hwInfo.ipVersion.release) + "_" + std::to_string(hwInfo.ipVersion.revision);

    auto resourceNames = BuiltIn::getResourceNames(BuiltIn::BaseKernel::copyBufferToBuffer, BuiltIn::bindfulImageStatelessBufferWide, BuiltIn::CodeType::intermediate, *pDevice);

    std::string expectedResourceNameGeneric = "bindful_image_stateless_buffer_wide_copy_buffer_to_buffer.builtin_kernel.spv";
    std::string expectedResourceNameForRelease = deviceIpString + "_" + expectedResourceNameGeneric;

    EXPECT_EQ(2u, resourceNames.size());
    EXPECT_EQ(resourceNames[0], expectedResourceNameForRelease);
    EXPECT_EQ(resourceNames[1], expectedResourceNameGeneric);
}

class FileStorageTests : public ::testing::Test {
  protected:
    class MockFileStorage : public BuiltIn::FileStorage {
      public:
        MockFileStorage() : BuiltIn::FileStorage("root") {}
        BuiltIn::Resource loadImpl(const std::string &fullResourceName) override {
            return BuiltIn::FileStorage::loadImpl(fullResourceName);
        }
    } storage;
};

TEST_F(FileStorageTests, GivenFiledNameWhenLoadingImplKernelFromFileStorageThenValidPtrIsReturnedForExisitngKernels) {
    VariableBackup<long int> ftellReturnBackup(&NEO::IoFunctions::mockFtellReturn, 4L);
    VariableBackup<size_t> freadReturnBackup(&NEO::IoFunctions::mockFreadReturn, 4u);
    BuiltIn::Resource br = storage.loadImpl("copybuffer.cl");
    EXPECT_NE(0u, br.size());

    VariableBackup<FILE *> fopenReturnedBackup(&NEO::IoFunctions::mockFopenReturned, nullptr);
    BuiltIn::Resource bnr = storage.loadImpl("unknown.cl");
    EXPECT_EQ(0u, bnr.size());
}

TEST_F(FileStorageTests, GivenFseekToEndFailsWhenLoadingImplFromFileStorageThenEmptyResourceIsReturnedAndFileIsClosed) {
    VariableBackup<uint32_t> fcloseCalledBackup(&NEO::IoFunctions::mockFcloseCalled, 0u);
    VariableBackup<int> fseekReturnBackup(&NEO::IoFunctions::mockFseekReturn, -1);
    BuiltIn::Resource res = storage.loadImpl("file.cl");
    EXPECT_EQ(0u, res.size());
    EXPECT_EQ(1u, NEO::IoFunctions::mockFcloseCalled);
}

TEST_F(FileStorageTests, GivenFtellFailsWhenLoadingImplFromFileStorageThenEmptyResourceIsReturnedAndFileIsClosed) {
    VariableBackup<uint32_t> fcloseCalledBackup(&NEO::IoFunctions::mockFcloseCalled, 0u);
    VariableBackup<int> fseekReturnBackup(&NEO::IoFunctions::mockFseekReturn, 0);
    VariableBackup<long int> ftellReturnBackup(&NEO::IoFunctions::mockFtellReturn, -1L);
    BuiltIn::Resource res = storage.loadImpl("file.cl");
    EXPECT_EQ(0u, res.size());
    EXPECT_EQ(1u, NEO::IoFunctions::mockFcloseCalled);
}

TEST_F(FileStorageTests, GivenFseekToStartFailsWhenLoadingImplFromFileStorageThenEmptyResourceIsReturnedAndFileIsClosed) {
    VariableBackup<uint32_t> fcloseCalledBackup(&NEO::IoFunctions::mockFcloseCalled, 0u);
    VariableBackup<uint32_t> fseekCalledBackup(&NEO::IoFunctions::mockFseekCalled, 0u);
    VariableBackup<uint32_t> failAfterNFseekBackup(&NEO::IoFunctions::failAfterNFseekCount, 1u);
    VariableBackup<int> fseekReturnBackup(&NEO::IoFunctions::mockFseekReturn, 0);
    VariableBackup<long int> ftellReturnBackup(&NEO::IoFunctions::mockFtellReturn, 4L);
    BuiltIn::Resource res = storage.loadImpl("file.cl");
    EXPECT_EQ(0u, res.size());
    EXPECT_EQ(1u, NEO::IoFunctions::mockFcloseCalled);
}

TEST_F(FileStorageTests, GivenFreadReturnsFewerBytesThanExpectedWhenLoadingImplFromFileStorageThenEmptyResourceIsReturned) {
    VariableBackup<uint32_t> fcloseCalledBackup(&NEO::IoFunctions::mockFcloseCalled, 0u);
    VariableBackup<int> fseekReturnBackup(&NEO::IoFunctions::mockFseekReturn, 0);
    VariableBackup<long int> ftellReturnBackup(&NEO::IoFunctions::mockFtellReturn, 4L);
    VariableBackup<size_t> freadReturnBackup(&NEO::IoFunctions::mockFreadReturn, 2u);
    BuiltIn::Resource res = storage.loadImpl("file.cl");
    EXPECT_EQ(0u, res.size());
    EXPECT_EQ(1u, NEO::IoFunctions::mockFcloseCalled);
}

TEST_F(BuiltInSharedTest, GivenRequestedTypeSourceWhenGettingWideStatelessBuiltinsThenReturnForReleaseAndGenericResourceNames) {
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    std::string deviceIpString = std::to_string(hwInfo.ipVersion.architecture) + "_" + std::to_string(hwInfo.ipVersion.release) + "_" + std::to_string(hwInfo.ipVersion.revision);

    auto resourceNames = BuiltIn::getResourceNames(BuiltIn::BaseKernel::copyBufferToBuffer, BuiltIn::bindfulImageStatelessBufferWide, BuiltIn::CodeType::source, *pDevice);

    std::string expectedResourceNameGeneric = "copy_buffer_to_buffer.builtin_kernel.cl";
    std::string expectedResourceNameForRelease = deviceIpString + "_" + expectedResourceNameGeneric;

    EXPECT_EQ(2u, resourceNames.size());
    EXPECT_EQ(resourceNames[0], expectedResourceNameForRelease);
    EXPECT_EQ(resourceNames[1], expectedResourceNameGeneric);
}
