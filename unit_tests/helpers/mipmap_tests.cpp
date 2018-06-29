/*
* Copyright (c) 2018, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/

#include "runtime/helpers/mipmap.h"
#include "runtime/mem_obj/image.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_gmm.h"
#include "unit_tests/mocks/mock_image.h"

#include "gtest/gtest.h"

using namespace OCLRT;

constexpr size_t testOrigin[]{2, 3, 5, 7};

typedef ::testing::TestWithParam<std::pair<uint32_t, size_t>> MipLevelTest;

TEST_P(MipLevelTest, givenMemObjectTypeReturnProperMipLevel) {
    auto pair = GetParam();
    EXPECT_EQ(static_cast<uint32_t>(pair.second), findMipLevel(pair.first, testOrigin));
}

INSTANTIATE_TEST_CASE_P(MipLevel,
                        MipLevelTest,
                        ::testing::Values(std::make_pair(CL_MEM_OBJECT_IMAGE1D, testOrigin[1]),
                                          std::make_pair(CL_MEM_OBJECT_IMAGE1D_ARRAY, testOrigin[2]),
                                          std::make_pair(CL_MEM_OBJECT_IMAGE2D, testOrigin[2]),
                                          std::make_pair(CL_MEM_OBJECT_IMAGE2D_ARRAY, testOrigin[3]),
                                          std::make_pair(CL_MEM_OBJECT_IMAGE3D, testOrigin[3]),
                                          std::make_pair(CL_MEM_OBJECT_BUFFER, 0U),
                                          std::make_pair(CL_MEM_OBJECT_IMAGE1D_BUFFER, 0U),
                                          std::make_pair(CL_MEM_OBJECT_PIPE, 0U)));

typedef ::testing::TestWithParam<std::pair<uint32_t, uint32_t>> MipLevelOriginIdxTest;

TEST_P(MipLevelOriginIdxTest, givenMemObjectTypeReturnProperMipLevelOriginIdx) {
    auto pair = GetParam();
    EXPECT_EQ(static_cast<uint32_t>(pair.second), getMipLevelOriginIdx(pair.first));
}

INSTANTIATE_TEST_CASE_P(MipLevelOriginIdx,
                        MipLevelOriginIdxTest,
                        ::testing::Values(std::make_pair(CL_MEM_OBJECT_IMAGE1D, 1U),
                                          std::make_pair(CL_MEM_OBJECT_IMAGE1D_ARRAY, 2U),
                                          std::make_pair(CL_MEM_OBJECT_IMAGE2D, 2U),
                                          std::make_pair(CL_MEM_OBJECT_IMAGE2D_ARRAY, 3U),
                                          std::make_pair(CL_MEM_OBJECT_IMAGE3D, 3U),
                                          std::make_pair(CL_MEM_OBJECT_IMAGE1D_BUFFER, 0U),
                                          std::make_pair(CL_MEM_OBJECT_BUFFER, static_cast<uint32_t>(-1)),
                                          std::make_pair(CL_MEM_OBJECT_PIPE, static_cast<uint32_t>(-1))));

TEST(MipmapHelper, givenClImageDescWithoutMipLevelsWhenIsMipMappedIsCalledThenFalseIsReturned) {
    cl_image_desc desc = {};
    desc.num_mip_levels = 0;
    EXPECT_FALSE(OCLRT::isMipMapped(desc));
    desc.num_mip_levels = 1;
    EXPECT_FALSE(OCLRT::isMipMapped(desc));
}

TEST(MipmapHelper, givenClImageDescWithMipLevelsWhenIsMipMappedIsCalledThenTrueIsReturned) {
    cl_image_desc desc = {};
    desc.num_mip_levels = 2;
    EXPECT_TRUE(OCLRT::isMipMapped(desc));
}

TEST(MipmapHelper, givenBufferWhenIsMipMappedIsCalledThenFalseIsReturned) {
    MockBuffer buffer;
    EXPECT_FALSE(OCLRT::isMipMapped(&buffer));
}

struct MockMipMapGmmResourceInfo : GmmResourceInfo {
    using GmmResourceInfo::GmmResourceInfo;
    GMM_STATUS getOffset(GMM_REQ_OFFSET_INFO &reqOffsetInfo) override {
        receivedMipLevel = reqOffsetInfo.MipLevel;
        receivedArrayIndex = reqOffsetInfo.ArrayIndex;
        receivedSlice = reqOffsetInfo.Slice;
        receivedLockFlagVal = reqOffsetInfo.ReqLock;

        reqOffsetInfo.Lock.Offset = getExpectedReturnOffset();

        return GMM_SUCCESS;
    }

    uint32_t receivedLockFlagVal = false;
    uint32_t receivedMipLevel = 0U;
    uint32_t receivedArrayIndex = 0U;
    uint32_t receivedSlice = 0U;

    static constexpr uint32_t getExpectedReturnOffset() {
        return 13;
    }
};

struct MockImage : MockImageBase {
    std::unique_ptr<Gmm> mockGmm;

    MockImage() : MockImageBase() {
        mockGmm.reset(new Gmm(nullptr, 0, false));
        graphicsAllocation->gmm = mockGmm.get();
        mockGmm->gmmResourceInfo.reset(new MockMipMapGmmResourceInfo());
    }

    MockMipMapGmmResourceInfo *getResourceInfo() {
        return static_cast<MockMipMapGmmResourceInfo *>(mockGmm->gmmResourceInfo.get());
    }
};

TEST(MipmapHelper, givenImageWithoutMipLevelsWhenIsMipMappedIsCalledThenFalseIsReturned) {
    MockImage image;
    image.imageDesc.num_mip_levels = 0;
    EXPECT_FALSE(OCLRT::isMipMapped(&image));
    image.imageDesc.num_mip_levels = 1;
    EXPECT_FALSE(OCLRT::isMipMapped(&image));
}

TEST(MipmapHelper, givenImageWithMipLevelsWhenIsMipMappedIsCalledThenTrueIsReturned) {
    MockImage image;
    image.imageDesc.num_mip_levels = 2;
    EXPECT_TRUE(OCLRT::isMipMapped(&image));
}

TEST(MipmapHelper, givenImageWithoutMipLevelsWhenGetMipOffsetIsCalledThenZeroIsReturned) {
    MockImage image;
    image.imageDesc.num_mip_levels = 1;

    auto offset = getMipOffset(&image, testOrigin);

    EXPECT_EQ(0U, offset);
}

struct MipOffsetTestExpVal {
    uint32_t expectedSlice = 0;
    uint32_t expectedArrayIndex = 0;

    static MipOffsetTestExpVal ExpectSlice(size_t expectedSlice) {
        MipOffsetTestExpVal ret = {};
        ret.expectedSlice = static_cast<uint32_t>(expectedSlice);
        return ret;
    }

    static MipOffsetTestExpVal ExpectArrayIndex(size_t expectedArrayIndex) {
        MipOffsetTestExpVal ret = {};
        ret.expectedArrayIndex = static_cast<uint32_t>(expectedArrayIndex);
        return ret;
    }

    static MipOffsetTestExpVal ExpectNoSliceOrArrayIndex() {
        return MipOffsetTestExpVal{};
    }
};

typedef ::testing::TestWithParam<std::pair<uint32_t, MipOffsetTestExpVal>> MipOffsetTest;

TEST_P(MipOffsetTest, givenImageWithMipLevelsWhenGetMipOffsetIsCalledThenProperOffsetIsReturned) {
    auto pair = GetParam();

    MockImage image;
    image.imageDesc.num_mip_levels = 16;
    image.imageDesc.image_type = pair.first;

    auto offset = getMipOffset(&image, testOrigin);

    EXPECT_EQ(MockMipMapGmmResourceInfo::getExpectedReturnOffset(), offset);
    EXPECT_EQ(1U, image.getResourceInfo()->receivedLockFlagVal);
    EXPECT_EQ(findMipLevel(image.imageDesc.image_type, testOrigin), image.getResourceInfo()->receivedMipLevel);

    EXPECT_EQ(pair.second.expectedSlice, image.getResourceInfo()->receivedSlice);
    EXPECT_EQ(pair.second.expectedArrayIndex, image.getResourceInfo()->receivedArrayIndex);
}

INSTANTIATE_TEST_CASE_P(MipmapOffset,
                        MipOffsetTest,
                        ::testing::Values(std::make_pair(CL_MEM_OBJECT_IMAGE1D, MipOffsetTestExpVal::ExpectNoSliceOrArrayIndex()),
                                          std::make_pair(CL_MEM_OBJECT_IMAGE1D_ARRAY, MipOffsetTestExpVal::ExpectArrayIndex(testOrigin[1])),
                                          std::make_pair(CL_MEM_OBJECT_IMAGE2D, MipOffsetTestExpVal::ExpectNoSliceOrArrayIndex()),
                                          std::make_pair(CL_MEM_OBJECT_IMAGE2D_ARRAY, MipOffsetTestExpVal::ExpectArrayIndex(testOrigin[2])),
                                          std::make_pair(CL_MEM_OBJECT_IMAGE3D, MipOffsetTestExpVal::ExpectSlice(testOrigin[2]))));
