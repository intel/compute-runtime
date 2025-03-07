/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/blit_properties.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_gmm_client_context.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

#include <memory>

namespace NEO {
class BlitPropertiesTests : public Test<DeviceFixture> {
  public:
    void SetUp() override {
        Test<DeviceFixture>::SetUp();
        gmmSrc = std::make_unique<MockGmm>(pDevice->getGmmHelper());
        gmmDst = std::make_unique<MockGmm>(pDevice->getGmmHelper());
        resourceInfoSrc = static_cast<MockGmmResourceInfo *>(gmmSrc->gmmResourceInfo.get());
        resourceInfoDst = static_cast<MockGmmResourceInfo *>(gmmSrc->gmmResourceInfo.get());
        mockAllocationSrc = std::make_unique<MockGraphicsAllocation>(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                                                     reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                                     MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
        mockAllocationDst = std::make_unique<MockGraphicsAllocation>(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                                                     reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                                     MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
        mockAllocationSrc->setGmm(gmmSrc.get(), 0);
        mockAllocationDst->setGmm(gmmSrc.get(), 0);
        blitProperties.srcAllocation = mockAllocationSrc.get();
        blitProperties.dstAllocation = mockAllocationDst.get();
        blitProperties.srcSize = size;
        blitProperties.dstSize = size;
        blitProperties.copySize = size;
        blitProperties.srcOffset = size;
        blitProperties.dstOffset = size;

        BlitProperties blitProperties;
    }
    void TearDown() override {
        Test<DeviceFixture>::TearDown();
    }
    std::unique_ptr<MockGmm> gmmSrc;
    std::unique_ptr<MockGmm> gmmDst;
    std::unique_ptr<MockGraphicsAllocation> mockAllocationSrc;
    std::unique_ptr<MockGraphicsAllocation> mockAllocationDst;
    MockGmmResourceInfo *resourceInfoSrc;
    MockGmmResourceInfo *resourceInfoDst;
    BlitProperties blitProperties{};
    Vec3<size_t> size = {8, 8, 1};
};

TEST_F(BlitPropertiesTests, givenBlitPropertiesWhenSrcIs1DTiledArrayThenTransformFrom1dArrayTo2DArray) {
    resourceInfoSrc->getResourceFlags()->Info.Tile64 = 1;
    resourceInfoSrc->mockResourceCreateParams.Type = GMM_RESOURCE_TYPE::RESOURCE_1D;
    resourceInfoSrc->mockResourceCreateParams.ArraySize = 8;
    blitProperties.transform1DArrayTo2DArrayIfNeeded();

    EXPECT_EQ(blitProperties.srcSize.y, 1u);
    EXPECT_EQ(blitProperties.srcSize.z, size.y);

    EXPECT_EQ(blitProperties.dstSize.y, 1u);
    EXPECT_EQ(blitProperties.dstSize.z, size.y);

    EXPECT_EQ(blitProperties.copySize.y, 1u);
    EXPECT_EQ(blitProperties.copySize.z, size.y);

    EXPECT_EQ(blitProperties.srcOffset.y, 0u);
    EXPECT_EQ(blitProperties.srcOffset.z, size.y);

    EXPECT_EQ(blitProperties.dstOffset.y, 0u);
    EXPECT_EQ(blitProperties.dstOffset.z, size.y);
}

TEST_F(BlitPropertiesTests, givenBlitPropertiesWhenDstIs1DTiledArrayThenTransformFrom1dArrayTo2DArray) {
    resourceInfoDst->getResourceFlags()->Info.Tile64 = 1;
    resourceInfoDst->mockResourceCreateParams.Type = GMM_RESOURCE_TYPE::RESOURCE_1D;
    resourceInfoDst->mockResourceCreateParams.ArraySize = 8;
    blitProperties.transform1DArrayTo2DArrayIfNeeded();

    EXPECT_EQ(blitProperties.srcSize.y, 1u);
    EXPECT_EQ(blitProperties.srcSize.z, size.y);

    EXPECT_EQ(blitProperties.dstSize.y, 1u);
    EXPECT_EQ(blitProperties.dstSize.z, size.y);

    EXPECT_EQ(blitProperties.copySize.y, 1u);
    EXPECT_EQ(blitProperties.copySize.z, size.y);

    EXPECT_EQ(blitProperties.srcOffset.y, 0u);
    EXPECT_EQ(blitProperties.srcOffset.z, size.y);

    EXPECT_EQ(blitProperties.dstOffset.y, 0u);
    EXPECT_EQ(blitProperties.dstOffset.z, size.y);
}
TEST_F(BlitPropertiesTests, givenBlitPropertiesWhenDstAndSrcIsNot1DTiledArrayThenSizeAndOffsetNotchanged) {
    blitProperties.transform1DArrayTo2DArrayIfNeeded();

    EXPECT_EQ(blitProperties.srcSize.y, size.y);
    EXPECT_EQ(blitProperties.srcSize.z, size.z);

    EXPECT_EQ(blitProperties.dstSize.y, size.y);
    EXPECT_EQ(blitProperties.dstSize.z, size.z);

    EXPECT_EQ(blitProperties.copySize.y, size.y);
    EXPECT_EQ(blitProperties.copySize.z, size.z);

    EXPECT_EQ(blitProperties.srcOffset.y, size.y);
    EXPECT_EQ(blitProperties.srcOffset.z, size.z);

    EXPECT_EQ(blitProperties.dstOffset.y, size.y);
    EXPECT_EQ(blitProperties.dstOffset.z, size.z);
}

TEST_F(BlitPropertiesTests, givenGmmResInfoForTiled1DArrayWhenIs1DTiledArrayCalledThenTrueReturned) {
    resourceInfoSrc->getResourceFlags()->Info.Tile64 = 1;
    resourceInfoSrc->mockResourceCreateParams.Type = GMM_RESOURCE_TYPE::RESOURCE_1D;
    resourceInfoSrc->mockResourceCreateParams.ArraySize = 8;
    EXPECT_TRUE(blitProperties.is1DTiledArray(resourceInfoSrc));
}
TEST_F(BlitPropertiesTests, givenGmmResInfoForTiled2DArrayWhenIs1DTiledArrayCalledThenFalseReturned) {
    resourceInfoSrc->getResourceFlags()->Info.Tile64 = 1;
    resourceInfoSrc->mockResourceCreateParams.Type = GMM_RESOURCE_TYPE::RESOURCE_2D;
    resourceInfoSrc->mockResourceCreateParams.ArraySize = 8;
    EXPECT_FALSE(blitProperties.is1DTiledArray(resourceInfoSrc));
}

TEST_F(BlitPropertiesTests, givenGmmResInfoForNotTiled1DArrayWhenIs1DTiledArrayCalledThenFalseReturned) {
    resourceInfoSrc->getResourceFlags()->Info.Tile64 = 0;
    resourceInfoSrc->mockResourceCreateParams.Type = GMM_RESOURCE_TYPE::RESOURCE_1D;
    resourceInfoSrc->mockResourceCreateParams.ArraySize = 8;
    EXPECT_FALSE(blitProperties.is1DTiledArray(resourceInfoSrc));
}
TEST_F(BlitPropertiesTests, givenGmmResInfoForTiled1DWhenIs1DTiledArrayCalledThenFalseReturned) {
    resourceInfoSrc->getResourceFlags()->Info.Tile64 = 1;
    resourceInfoSrc->mockResourceCreateParams.Type = GMM_RESOURCE_TYPE::RESOURCE_1D;
    resourceInfoSrc->mockResourceCreateParams.ArraySize = 1;
    EXPECT_FALSE(blitProperties.is1DTiledArray(resourceInfoSrc));
}
} // namespace NEO
