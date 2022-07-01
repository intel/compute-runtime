/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_handle_allocator.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_gmm_client_context.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/test_macros/hw_test.h"

extern PRODUCT_FAMILY productFamily;

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
    NEO::HardwareInfo hwInfo;
    hwInfo.platform.eProductFamily = productFamily;
    NEO::MockGmmClientContext gmmClientCtx{nullptr, &hwInfo};
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
    NEO::HardwareInfo hwInfo;
    hwInfo.platform.eProductFamily = productFamily;
    NEO::MockGmmClientContext gmmClientCtx{nullptr, &hwInfo};
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
    NEO::HardwareInfo hwInfo;
    hwInfo.platform.eProductFamily = productFamily;
    NEO::MockGmmClientContext gmmClientCtx{nullptr, &hwInfo};
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

TEST(GmmHelperTests, WhenInitializingGmmHelperThenCorrectAddressWidthIsSet) {
    auto hwInfo = *NEO::defaultHwInfo;

    hwInfo.capabilityTable.gpuAddressSpace = maxNBitValue(48);
    auto gmmHelper = std::make_unique<NEO::GmmHelper>(nullptr, &hwInfo);

    auto addressWidth = gmmHelper->getAddressWidth();
    EXPECT_EQ(48u, addressWidth);

    hwInfo.capabilityTable.gpuAddressSpace = maxNBitValue(36);
    gmmHelper = std::make_unique<NEO::GmmHelper>(nullptr, &hwInfo);

    addressWidth = gmmHelper->getAddressWidth();
    EXPECT_EQ(48u, addressWidth);

    hwInfo.capabilityTable.gpuAddressSpace = maxNBitValue(57);
    gmmHelper = std::make_unique<NEO::GmmHelper>(nullptr, &hwInfo);

    addressWidth = gmmHelper->getAddressWidth();
    EXPECT_EQ(57u, addressWidth);
}