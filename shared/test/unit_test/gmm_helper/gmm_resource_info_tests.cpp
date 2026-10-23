/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_handle_allocator.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gmm_client_context.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"

#include "gtest/gtest.h"

#include <algorithm>
#include <utility>
#include <vector>

using namespace NEO;

struct MockGmmHandleAllocator : NEO::GmmHandleAllocator {
    void *createHandle(const GMM_RESOURCE_INFO *gmmResourceInfo) override {
        auto ret = new char;
        createdHandles.push_back(ret);
        return ret;
    }
    bool openHandle(void *handle, GMM_RESOURCE_INFO *dstResInfo, size_t handleSize) override {
        openedHandles.push_back(reinterpret_cast<char *>(handle));
        return true;
    }
    void destroyHandle(void *handle) override {
        destroyedHandles.push_back(reinterpret_cast<char *>(handle));
        delete reinterpret_cast<char *>(handle);
    }
    size_t getHandleSize() override {
        return sizeof(char);
    }

    std::vector<char *> createdHandles;
    std::vector<char *> openedHandles;
    std::vector<char *> destroyedHandles;
};

TEST(GmmResourceInfo, WhenGmmHandleAllocatorIsPresentThenItsBeingUsedForCreatingGmmResourceInfoHandles) {
    auto hwInfo = *defaultHwInfo;
    MockExecutionEnvironment executionEnvironment{&hwInfo};
    NEO::MockGmmClientContext gmmClientCtx{};
    gmmClientCtx.initialize(*executionEnvironment.rootDeviceEnvironments[0]);
    gmmClientCtx.setHandleAllocator(std::make_unique<MockGmmHandleAllocator>());
    auto handleAllocator = static_cast<MockGmmHandleAllocator *>(gmmClientCtx.getHandleAllocator());

    GMM_RESCREATE_PARAMS createParams = {};
    createParams.Type = RESOURCE_BUFFER;
    createParams.Format = GMM_FORMAT_R8G8B8A8_SINT;
    auto resInfo = static_cast<NEO::MockGmmResourceInfo *>(NEO::GmmResourceInfo::create(nullptr, &createParams));
    ASSERT_NE(nullptr, resInfo);
    resInfo->clientContext = &gmmClientCtx;

    resInfo->createResourceInfo(nullptr);

    auto &created = handleAllocator->createdHandles;
    auto &destroyed = handleAllocator->destroyedHandles;
    EXPECT_EQ(1U, created.size());
    EXPECT_EQ(0U, destroyed.size());

    EXPECT_EQ(handleAllocator->getHandleSize(), resInfo->GmmResourceInfo::peekHandleSize());

    auto handle = resInfo->GmmResourceInfo::peekHandle();
    EXPECT_NE(nullptr, handle);
    EXPECT_NE(created.end(), std::find(created.begin(), created.end(), handle));
    delete resInfo;

    EXPECT_EQ(1U, created.size());
    EXPECT_EQ(1U, destroyed.size());
    EXPECT_NE(destroyed.end(), std::find(destroyed.begin(), destroyed.end(), handle));
}

TEST(GmmResourceInfo, GivenGmmResourceInfoAndHandleAllocatorInClientContextWhenDecodingResourceInfoThenExistingHandleIsOpened) {
    auto hwInfo = *defaultHwInfo;
    MockExecutionEnvironment executionEnvironment{&hwInfo};
    NEO::MockGmmClientContext gmmClientCtx{};
    gmmClientCtx.initialize(*executionEnvironment.rootDeviceEnvironments[0]);
    gmmClientCtx.setHandleAllocator(std::make_unique<MockGmmHandleAllocator>());
    auto handleAllocator = static_cast<MockGmmHandleAllocator *>(gmmClientCtx.getHandleAllocator());

    GMM_RESCREATE_PARAMS createParams = {};
    createParams.Type = RESOURCE_BUFFER;
    createParams.Format = GMM_FORMAT_R8G8B8A8_SINT;
    auto resInfo = static_cast<NEO::MockGmmResourceInfo *>(NEO::GmmResourceInfo::create(nullptr, &createParams));
    ASSERT_NE(nullptr, resInfo);
    resInfo->clientContext = &gmmClientCtx;
    resInfo->createResourceInfo(nullptr);

    auto &created = handleAllocator->createdHandles;
    auto &opened = handleAllocator->openedHandles;
    auto &destroyed = handleAllocator->destroyedHandles;

    EXPECT_EQ(0U, opened.size());

    auto resInfo2 = static_cast<NEO::MockGmmResourceInfo *>(NEO::GmmResourceInfo::create(nullptr, reinterpret_cast<GMM_RESOURCE_INFO *>(resInfo), true));
    ASSERT_NE(nullptr, resInfo2);
    resInfo2->clientContext = &gmmClientCtx;
    resInfo2->decodeResourceInfo(static_cast<GMM_RESOURCE_INFO *>(resInfo->GmmResourceInfo::peekHandle()));

    EXPECT_EQ(2U, created.size());
    EXPECT_EQ(1U, opened.size());
    EXPECT_EQ(0U, destroyed.size());

    EXPECT_EQ(handleAllocator->getHandleSize(), resInfo->GmmResourceInfo::peekHandleSize());

    auto handle = resInfo->GmmResourceInfo::peekHandle();
    EXPECT_NE(nullptr, handle);
    EXPECT_NE(created.end(), std::find(created.begin(), created.end(), handle));
    EXPECT_NE(opened.end(), std::find(opened.begin(), opened.end(), handle));

    auto handle2 = resInfo2->GmmResourceInfo::peekHandle();
    EXPECT_NE(nullptr, handle2);

    EXPECT_EQ(0u, resInfo2->refreshHandleCalled);

    resInfo2->refreshHandle();

    EXPECT_EQ(2U, created.size());
    EXPECT_EQ(2U, opened.size());
    EXPECT_EQ(0U, destroyed.size());
    EXPECT_EQ(1u, resInfo2->refreshHandleCalled);

    delete resInfo;
    delete resInfo2;

    EXPECT_EQ(2U, created.size());
    EXPECT_EQ(2U, opened.size());
    EXPECT_EQ(2U, destroyed.size());
    EXPECT_NE(destroyed.end(), std::find(destroyed.begin(), destroyed.end(), handle));
}

TEST(GmmResourceInfo, GivenResourceInfoWhenRefreshIsCalledTiwceThenOpenHandleIsCalledTwice) {
    auto hwInfo = *defaultHwInfo;
    MockExecutionEnvironment executionEnvironment{&hwInfo};
    NEO::MockGmmClientContext gmmClientCtx{};
    gmmClientCtx.initialize(*executionEnvironment.rootDeviceEnvironments[0]);
    gmmClientCtx.setHandleAllocator(std::make_unique<MockGmmHandleAllocator>());
    auto handleAllocator = static_cast<MockGmmHandleAllocator *>(gmmClientCtx.getHandleAllocator());

    GMM_RESCREATE_PARAMS createParams = {};
    createParams.Type = RESOURCE_BUFFER;
    createParams.Format = GMM_FORMAT_R8G8B8A8_SINT;
    auto resInfo = static_cast<NEO::MockGmmResourceInfo *>(NEO::GmmResourceInfo::create(nullptr, &createParams));
    ASSERT_NE(nullptr, resInfo);
    resInfo->clientContext = &gmmClientCtx;
    resInfo->createResourceInfo(nullptr);

    auto &created = handleAllocator->createdHandles;
    auto &opened = handleAllocator->openedHandles;
    auto &destroyed = handleAllocator->destroyedHandles;

    EXPECT_EQ(0U, opened.size());

    auto resInfo2 = static_cast<NEO::MockGmmResourceInfo *>(NEO::GmmResourceInfo::create(nullptr, reinterpret_cast<GMM_RESOURCE_INFO *>(resInfo), true));
    ASSERT_NE(nullptr, resInfo2);
    resInfo2->clientContext = &gmmClientCtx;
    resInfo2->decodeResourceInfo(static_cast<GMM_RESOURCE_INFO *>(resInfo->GmmResourceInfo::peekHandle()));

    EXPECT_EQ(2U, created.size());
    EXPECT_EQ(1U, opened.size());
    EXPECT_EQ(0U, destroyed.size());

    EXPECT_EQ(handleAllocator->getHandleSize(), resInfo->GmmResourceInfo::peekHandleSize());

    auto handle = resInfo->GmmResourceInfo::peekHandle();
    EXPECT_NE(nullptr, handle);
    EXPECT_NE(created.end(), std::find(created.begin(), created.end(), handle));
    EXPECT_NE(opened.end(), std::find(opened.begin(), opened.end(), handle));

    auto handle2 = resInfo2->GmmResourceInfo::peekHandle();
    EXPECT_NE(nullptr, handle2);

    EXPECT_EQ(0u, resInfo2->refreshHandleCalled);

    resInfo2->refreshHandle();
    resInfo2->refreshHandle();

    EXPECT_EQ(2U, created.size());
    EXPECT_EQ(3U, opened.size());
    EXPECT_EQ(0U, destroyed.size());
    EXPECT_EQ(2u, resInfo2->refreshHandleCalled);

    delete resInfo;
    delete resInfo2;

    EXPECT_EQ(2U, created.size());
    EXPECT_EQ(3U, opened.size());
    EXPECT_EQ(2U, destroyed.size());
    EXPECT_NE(destroyed.end(), std::find(destroyed.begin(), destroyed.end(), handle));
}

TEST(GmmResourceInfo, WhenUsingBaseHandleAllocatorThenHandlesAreEmpty) {
    NEO::GmmHandleAllocator defaultAllocator;
    EXPECT_EQ(0U, defaultAllocator.getHandleSize());
    auto handle = defaultAllocator.createHandle(nullptr);
    EXPECT_EQ(nullptr, handle);
    defaultAllocator.destroyHandle(handle);
}

TEST(GmmResourceInfo, GivenEmptyHandleWhenUsingBaseHandleAllocatorThenOpenHandleReturnsTrue) {
    NEO::GmmHandleAllocator defaultAllocator;
    auto handle = defaultAllocator.createHandle(nullptr);
    auto isNewHandleOpen = defaultAllocator.openHandle(handle, nullptr, defaultAllocator.getHandleSize());
    EXPECT_TRUE(isNewHandleOpen);
    defaultAllocator.destroyHandle(handle);
}

TEST(GmmResourceInfoFlagsTest, givenTilingFlagsWhenGettingTilingModeThenTilingModeIsReturned) {
    for (const auto &[setFlag, expectedTilingMode] : std::vector<std::pair<void (*)(GMM_RESOURCE_FLAG &), ImageTilingMode>>{
             {[](GMM_RESOURCE_FLAG &) {}, ImageTilingMode::notTiled},
             {[](GMM_RESOURCE_FLAG &flags) { flags.Info.Linear = 1; }, ImageTilingMode::notTiled},
             {[](GMM_RESOURCE_FLAG &flags) { flags.Info.TiledW = 1; }, ImageTilingMode::tiledW},
             {[](GMM_RESOURCE_FLAG &flags) { flags.Info.TiledX = 1; }, ImageTilingMode::tiledX},
             {[](GMM_RESOURCE_FLAG &flags) { flags.Info.TiledY = 1; }, ImageTilingMode::tiledY},
             {[](GMM_RESOURCE_FLAG &flags) { flags.Info.Tile4 = 1; }, ImageTilingMode::tiled4},
             {[](GMM_RESOURCE_FLAG &flags) { flags.Info.Tile64 = 1; }, ImageTilingMode::tiled64}}) {
        MockGmmResourceInfoWithFlags resourceInfo;
        setFlag(resourceInfo.resourceFlags);
        EXPECT_EQ(expectedTilingMode, resourceInfo.getTilingMode());
    }
}

TEST(GmmResourceInfoFlagsTest, givenCompressionFlagsWhenGettingCompressionTypeThenCompressionTypeIsReturned) {
    MockGmmResourceInfoWithFlags resourceInfo;
    EXPECT_EQ(GmmCompressionType::none, resourceInfo.getCompressionType());

    resourceInfo.resourceFlags.Info.RenderCompressed = 1;
    EXPECT_EQ(GmmCompressionType::render, resourceInfo.getCompressionType());

    resourceInfo.resourceFlags.Info.RenderCompressed = 0;
    resourceInfo.resourceFlags.Info.MediaCompressed = 1;
    EXPECT_EQ(GmmCompressionType::media, resourceInfo.getCompressionType());

    resourceInfo.resourceFlags.Info.RenderCompressed = 1;
    EXPECT_THROW(resourceInfo.getCompressionType(), std::exception);
}

TEST(GmmResourceInfoFlagsTest, givenCompressedResourceWithNotCompressedFlagWhenGettingCompressionTypeThenNoneIsReturned) {
    for (const auto mediaCompressed : {0u, 1u}) {
        MockGmmResourceInfoWithFlags resourceInfo;
        resourceInfo.resourceFlags.Info.MediaCompressed = mediaCompressed;
        resourceInfo.resourceFlags.Info.RenderCompressed = 1u - mediaCompressed;

        resourceInfo.resourceFlags.Info.NotCompressed = 1;
        EXPECT_EQ(GmmCompressionType::none, resourceInfo.getCompressionType());
    }
}

TEST(GmmResourceInfoFlagsTest, givenSurfaceFlagsWhenQueryingSurfacePropertiesThenFlagValuesAreReturned) {
    MockGmmResourceInfoWithFlags resourceInfo;
    EXPECT_FALSE(resourceInfo.isCacheable());
    EXPECT_FALSE(resourceInfo.isDepthResource());
    EXPECT_FALSE(resourceInfo.isMCS());
    EXPECT_FALSE(resourceInfo.isIndirectClearColor());
    EXPECT_FALSE(resourceInfo.isCCS());

    resourceInfo.resourceFlags.Info.Cacheable = 1;
    resourceInfo.resourceFlags.Gpu.Depth = 1;
    resourceInfo.resourceFlags.Gpu.MCS = 1;
    resourceInfo.resourceFlags.Gpu.IndirectClearColor = 1;
    resourceInfo.resourceFlags.Gpu.CCS = 1;
    EXPECT_TRUE(resourceInfo.isCacheable());
    EXPECT_TRUE(resourceInfo.isDepthResource());
    EXPECT_TRUE(resourceInfo.isMCS());
    EXPECT_TRUE(resourceInfo.isIndirectClearColor());
    EXPECT_TRUE(resourceInfo.isCCS());
}
