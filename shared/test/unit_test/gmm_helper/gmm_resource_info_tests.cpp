/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_handle_allocator.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/test/common/mocks/mock_gmm_client_context.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/test_macros/test.h"

extern PRODUCT_FAMILY productFamily;

struct MockGmmHandleAllocator : NEO::GmmHandleAllocator {
    void *createHandle(const GMM_RESOURCE_INFO *gmmResourceInfo) override {
        auto ret = new char;
        createdHandles.push_back(ret);
        return ret;
    }
    void destroyHandle(void *handle) override {
        destroyedHandles.push_back(reinterpret_cast<char *>(handle));
        delete reinterpret_cast<char *>(handle);
    }
    size_t getHandleSize() override {
        return sizeof(char);
    }

    std::vector<char *> createdHandles;
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

TEST(GmmResourceInfo, WhenUsingBaseHandleAllocatorThenHandlesAreEmpty) {
    NEO::GmmHandleAllocator defaultAllocator;
    EXPECT_EQ(0U, defaultAllocator.getHandleSize());
    auto handle = defaultAllocator.createHandle(nullptr);
    EXPECT_EQ(nullptr, handle);
    defaultAllocator.destroyHandle(handle);
}
