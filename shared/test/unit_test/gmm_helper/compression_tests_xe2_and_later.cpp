/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/fixtures/mock_execution_environment_gmm_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"

#include <memory>

using MockExecutionEnvironmentGmmTest = ::Test<NEO::MockExecutionEnvironmentGmmFixture>;
using ProductHelperXe2Tests = ProductHelperTest;

using namespace NEO;
using namespace ::testing;

struct GmmAdditionalCompressionSettingsTests : public MockExecutionEnvironmentGmmTest {

    void SetUp() override {
        MockExecutionEnvironmentGmmFixture::setUp();
        localPlatformDevice = *defaultHwInfo;
        localPlatformDevice.featureTable.flags.ftrLocalMemory = true;
        localPlatformDevice.featureTable.flags.ftrXe2Compression = true;
        localPlatformDevice.capabilityTable.ftrRenderCompressedImages = true;
        localPlatformDevice.capabilityTable.ftrRenderCompressedBuffers = true;
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&localPlatformDevice);
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    }

    void setupImgInfo() {
        imgDesc.imageType = ImageType::image1D;
        imgDesc.imageWidth = 2;
        imgDesc.imageHeight = 2;
        imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
        imgInfo.useLocalMemory = true;
        imgInfo.surfaceFormat = &MockGmmParams::mockSurfaceFormat;
        imgInfo.plane = GMM_YUV_PLANE::GMM_NO_PLANE;
    }

    ImageDescriptor imgDesc = {};
    ImageInfo imgInfo = {};
    HardwareInfo localPlatformDevice{};
};

using ProductHelperAtLeastXe2Tests = ::Test<DeviceFixture>;

HWTEST2_F(GmmAdditionalCompressionSettingsTests, whenApplyAdditionalCompressionSettingsThenSetNotCompressed, IsAtLeastXe2HpgCore) {
    auto gmm = std::make_unique<MockGmm>(getGmmHelper());
    gmm->resourceParams.Flags.Info.NotCompressed = 0;

    auto &gfxCoreHelper = this->executionEnvironment->rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    gfxCoreHelper.applyAdditionalCompressionSettings(*gmm, true);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.NotCompressed);

    gfxCoreHelper.applyAdditionalCompressionSettings(*gmm, false);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.NotCompressed);
}

HWTEST2_F(GmmAdditionalCompressionSettingsTests, givenDebugFlagSetWhenCreatingGmmResourceThenPrintCompressionSettings, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintGmmCompressionParams.set(true);

    StreamCapture capture;
    capture.captureStdout();

    auto gmm = std::make_unique<MockGmm>(getGmmHelper());
    auto &flags = gmm->resourceParams.Flags;

    std::string output = capture.getCapturedStdout();
    ASSERT_NE(0u, output.size());

    char expectedStr[512] = {};
    snprintf(expectedStr, 512, "\n\tFlags.Info.NotCompressed: %u", flags.Info.NotCompressed);

    EXPECT_TRUE(hasSubstr(output, std::string(expectedStr)));
}

HWTEST2_F(GmmAdditionalCompressionSettingsTests, givenCompressionIsFalseAndCacheableTypeThenSetNotCompressedAndCacheable, IsAtLeastXe2HpgCore) {
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, StorageInfo{}, gmmRequirements);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.NotCompressed);
    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getProductHelper();
    EXPECT_EQ(productHelper.isCachingOnCpuAvailable(), gmm->resourceParams.Flags.Info.Cacheable);
}

HWTEST2_F(GmmAdditionalCompressionSettingsTests, givenCompressionIsTrueAndCacheableTypeThenClearNotCompressedAndClearCacheable, IsAtLeastXe2HpgCore) {
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = true;
    auto gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, StorageInfo{}, gmmRequirements);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.NotCompressed);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.Cacheable);
}

HWTEST2_F(GmmAdditionalCompressionSettingsTests, givenCompressionIsFalseAndNotCacheableTypeThenSetNotCompressedAndClearCacheable, IsAtLeastXe2HpgCore) {
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED, StorageInfo{}, gmmRequirements);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.NotCompressed);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.Cacheable);
}

HWTEST2_F(GmmAdditionalCompressionSettingsTests, givenCompressionIsTrueAndNotCacheableTypeThenClearNotCompressedAndClearCacheable, IsAtLeastXe2HpgCore) {
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = true;
    auto gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED, StorageInfo{}, gmmRequirements);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.NotCompressed);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.Cacheable);
}

HWTEST2_F(GmmAdditionalCompressionSettingsTests, givenCompressionIsTrueAndImageLocalMemoryIsTrueWhenUseApplyAuxFlagsForImageThenClearNotCompressed, IsAtLeastXe2HpgCore) {
    setupImgInfo();
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::make_unique<MockGmm>(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, StorageInfo{}, gmmRequirements);
    gmm->setupImageResourceParams(imgInfo, true);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.NotCompressed);

    gmm->setCompressionEnabled(false);

    imgInfo.useLocalMemory = false;
    gmm->setupImageResourceParams(imgInfo, false);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.NotCompressed);

    imgInfo.useLocalMemory = true;
    gmm->setupImageResourceParams(imgInfo, false);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.NotCompressed);

    imgInfo.useLocalMemory = false;
    gmm->setupImageResourceParams(imgInfo, true);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.NotCompressed);

    imgInfo.useLocalMemory = true;
    gmm->setupImageResourceParams(imgInfo, true);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.NotCompressed);
}

HWTEST2_F(GmmAdditionalCompressionSettingsTests, givenEnabledAndPreferredE2ECWhenApplyingForBuffersThenSetValidFlags, IsAtLeastXe2HpgCore) {
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    gmm->resourceParams = {};

    localPlatformDevice.capabilityTable.ftrRenderCompressedBuffers = true;

    gmm->applyAuxFlagsForBuffer(true);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Gpu.CCS);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Gpu.UnifiedAuxSurface);
    EXPECT_TRUE(gmm->isCompressionEnabled());
}

HWTEST2_F(GmmAdditionalCompressionSettingsTests, givenDisabledE2ECAndEnabledDebugFlagWhenApplyingForBuffersThenSetValidFlags, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore restore;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    Gmm gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
    gmm.resourceParams = {};

    debugManager.flags.RenderCompressedBuffersEnabled.set(1);
    localPlatformDevice.capabilityTable.ftrRenderCompressedBuffers = false;

    gmm.applyAuxFlagsForBuffer(true);
    EXPECT_EQ(1u, gmm.resourceParams.Flags.Gpu.CCS);
    EXPECT_EQ(1u, gmm.resourceParams.Flags.Gpu.UnifiedAuxSurface);
    EXPECT_TRUE(gmm.isCompressionEnabled());

    gmm.resourceParams = {};
    gmm.setCompressionEnabled(false);
    debugManager.flags.RenderCompressedBuffersEnabled.set(0);
    localPlatformDevice.capabilityTable.ftrRenderCompressedBuffers = true;

    gmm.applyAuxFlagsForBuffer(true);
    EXPECT_EQ(0u, gmm.resourceParams.Flags.Gpu.CCS);
    EXPECT_EQ(0u, gmm.resourceParams.Flags.Gpu.UnifiedAuxSurface);
    EXPECT_FALSE(gmm.isCompressionEnabled());
}

HWTEST2_F(GmmAdditionalCompressionSettingsTests, givenAllValidInputsWhenQueryingThenSetAppropriateFlags, IsAtLeastXe2HpgCore) {
    setupImgInfo();
    EXPECT_TRUE(localPlatformDevice.capabilityTable.ftrRenderCompressedImages);
    EXPECT_TRUE(imgInfo.surfaceFormat->gmmSurfaceFormat != GMM_RESOURCE_FORMAT::GMM_FORMAT_NV12);
    EXPECT_TRUE(imgInfo.plane == GMM_YUV_PLANE_ENUM::GMM_NO_PLANE);

    auto queryGmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, true);
    auto resourceFormat = queryGmm->gmmResourceInfo->getResourceFormat();
    auto compressionFormat = getGmmClientContext()->getSurfaceStateCompressionFormat(resourceFormat);
    EXPECT_GT(compressionFormat, 0u);

    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Info.TiledY);
    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Info.Linear);
    EXPECT_EQ(1u, queryGmm->resourceParams.Flags.Gpu.CCS);
    EXPECT_EQ(1u, queryGmm->resourceParams.Flags.Gpu.UnifiedAuxSurface);
    EXPECT_EQ(1u, queryGmm->resourceParams.Flags.Gpu.IndirectClearColor);
    EXPECT_TRUE(queryGmm->isCompressionEnabled());
}

HWTEST2_F(GmmAdditionalCompressionSettingsTests, givenNotAllowedCompressionAndEnabledDebugFlagWhenQueryingThenSetAppropriateFlags, IsAtLeastXe2HpgCore) {
    setupImgInfo();
    DebugManagerStateRestore restore;
    debugManager.flags.RenderCompressedImagesEnabled.set(1);
    localPlatformDevice.capabilityTable.ftrRenderCompressedImages = false;
    auto queryGmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, true);

    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Info.Linear);
    EXPECT_EQ(1u, queryGmm->resourceParams.Flags.Gpu.CCS);
    EXPECT_EQ(1u, queryGmm->resourceParams.Flags.Gpu.UnifiedAuxSurface);
    EXPECT_EQ(1u, queryGmm->resourceParams.Flags.Gpu.IndirectClearColor);
    EXPECT_TRUE(queryGmm->isCompressionEnabled());

    debugManager.flags.RenderCompressedImagesEnabled.set(0);
    queryGmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, true);

    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Gpu.CCS);
    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Gpu.UnifiedAuxSurface);
    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Gpu.IndirectClearColor);
    EXPECT_FALSE(queryGmm->isCompressionEnabled());
}
