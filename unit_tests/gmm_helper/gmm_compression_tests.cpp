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

#include "gtest/gtest.h"
#include "runtime/helpers/hw_info.h"
#include "unit_tests/mocks/mock_gmm.h"

using namespace ::testing;
using namespace OCLRT;

struct GmmCompressionTests : public ::testing::Test {
    void SetUp() override {
        localPlatformDevice = **platformDevices;
        localPlatformDevice.capabilityTable.ftrCompression = true;
        setupImgInfo();
    }

    void setupImgInfo() {
        imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
        imgDesc.image_width = 2;
        imgDesc.image_height = 2;
        imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
        imgInfo.preferRenderCompression = true;
    }

    HardwareInfo localPlatformDevice = {};
    cl_image_desc imgDesc = {};
    ImageInfo imgInfo = {};
};

TEST_F(GmmCompressionTests, givenPreferRenderCompressionAndCompressionFtrEnabledWhenQueryingThenSetAppropriateFlags) {
    auto queryGmm = MockGmm::queryImgParams(imgInfo, &localPlatformDevice);

    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Info.Linear);
    EXPECT_EQ(1u, queryGmm->resourceParams.Flags.Info.TiledY);
    EXPECT_EQ(1u, queryGmm->resourceParams.Flags.Info.RenderCompressed);
    EXPECT_EQ(1u, queryGmm->resourceParams.Flags.Gpu.CCS);
    EXPECT_EQ(1u, queryGmm->resourceParams.Flags.Gpu.UnifiedAuxSurface);
    EXPECT_TRUE(queryGmm->isRenderCompressed);
}

TEST_F(GmmCompressionTests, givenPreferRenderCompressionAndCompressionFtrDisabledWhenQueryingThenSetAppropriateFlags) {
    localPlatformDevice.capabilityTable.ftrCompression = false;

    auto queryGmm = MockGmm::queryImgParams(imgInfo, &localPlatformDevice);

    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Info.RenderCompressed);
    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Gpu.CCS);
    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Gpu.UnifiedAuxSurface);
    EXPECT_FALSE(queryGmm->isRenderCompressed);
}

TEST_F(GmmCompressionTests, givenPreferRenderCompressionDisabledAndCompressionFtrEnabledWhenQueryingThenSetAppropriateFlags) {
    imgInfo.preferRenderCompression = false;

    auto queryGmm = MockGmm::queryImgParams(imgInfo, &localPlatformDevice);

    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Info.RenderCompressed);
    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Gpu.CCS);
    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Gpu.UnifiedAuxSurface);
    EXPECT_FALSE(queryGmm->isRenderCompressed);
}

TEST_F(GmmCompressionTests, givenSupportedAuxL1FormatWhenQueryingThenAllow) {
    auto queryGmm = MockGmm::queryImgParams(imgInfo, &localPlatformDevice);
    auto resourceFormat = queryGmm->gmmResourceInfo->getResourceFormat();

    EXPECT_TRUE(queryGmm->auxFormatSupported(resourceFormat));
    EXPECT_TRUE(queryGmm->isRenderCompressed);
}

TEST_F(GmmCompressionTests, givenNotSupportedAuxL1FormatWhenQueryingThenDisallow) {
    imgInfo.surfaceFormat = &readOnlyDepthSurfaceFormats[2];

    auto queryGmm = MockGmm::queryImgParams(imgInfo, &localPlatformDevice);
    auto resourceFormat = queryGmm->gmmResourceInfo->getResourceFormat();

    EXPECT_FALSE(queryGmm->auxFormatSupported(resourceFormat));
    EXPECT_FALSE(queryGmm->isRenderCompressed);
}
