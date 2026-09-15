/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/extensions/public/cl_ext_private.h"
#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/leo_surface_formats.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include "CL/cl.h"
#include "CL/cl_ext.h"

#include <algorithm>
#include <memory>
#include <vector>

namespace NEO {
namespace LEO {
namespace ult {

struct ContextImageFormatsFixture : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();
        cl_device_id clDeviceId = clDevice;
        context = std::make_unique<Context>(nullptr, this->L0::ult::DeviceFixture::context->toHandle(), 1, &clDeviceId, true);

        nv12Enabled = clDevice->getDeviceInfo().nv12Extension;
        packedYuvEnabled = clDevice->getDeviceInfo().packedYuvExtension;
    }

    void TearDown() override {
        context.reset();
        Test<OclFixture>::TearDown();
    }

    cl_uint countFormats(cl_mem_flags flags, cl_mem_object_type imageType) {
        cl_uint numFormats = 0u;
        EXPECT_EQ(CL_SUCCESS, context->getSupportedImageFormats(flags, imageType, 0, nullptr, &numFormats));
        return numFormats;
    }

    std::vector<cl_image_format> listFormats(cl_mem_flags flags, cl_mem_object_type imageType) {
        const auto numFormats = countFormats(flags, imageType);
        std::vector<cl_image_format> formats(numFormats);
        EXPECT_EQ(CL_SUCCESS, context->getSupportedImageFormats(flags, imageType, numFormats, formats.data(), nullptr));
        return formats;
    }

    static bool contains(const std::vector<cl_image_format> &formats, cl_channel_order channelOrder) {
        return std::any_of(formats.begin(), formats.end(), [channelOrder](const cl_image_format &format) {
            return format.image_channel_order == channelOrder;
        });
    }

    ClDevice *clDevice = nullptr;
    std::unique_ptr<Context> context;
    bool nv12Enabled = false;
    bool packedYuvEnabled = false;
};

TEST_F(ContextImageFormatsFixture, givenReadOnlyFlagAndImage2dWhenQueryingCountThenReadOnlyAndDepthTablesAreIncluded) {
    auto expected = SurfaceFormats::readOnly().size() + SurfaceFormats::readOnlyDepth().size();
    if (nv12Enabled) {
        expected += SurfaceFormats::planarYuv().size();
    }
    if (packedYuvEnabled) {
        expected += SurfaceFormats::packedYuv().size();
    }

    EXPECT_EQ(static_cast<cl_uint>(expected), countFormats(CL_MEM_READ_ONLY, CL_MEM_OBJECT_IMAGE2D));
}

TEST_F(ContextImageFormatsFixture, givenReadOnlyFlagAndImage3dWhenQueryingCountThenOnlyReadOnlyTableIsIncluded) {
    EXPECT_EQ(static_cast<cl_uint>(SurfaceFormats::readOnly().size()),
              countFormats(CL_MEM_READ_ONLY, CL_MEM_OBJECT_IMAGE3D));
}

TEST_F(ContextImageFormatsFixture, givenReadOnlyFlagAndImage2dArrayWhenQueryingCountThenDepthIsIncludedButYuvIsNot) {
    const auto expected = SurfaceFormats::readOnly().size() + SurfaceFormats::readOnlyDepth().size();
    EXPECT_EQ(static_cast<cl_uint>(expected), countFormats(CL_MEM_READ_ONLY, CL_MEM_OBJECT_IMAGE2D_ARRAY));
}

TEST_F(ContextImageFormatsFixture, givenWriteOnlyFlagAndImage2dWhenQueryingCountThenWriteOnlyAndReadWriteDepthAreIncluded) {
    const auto expected = SurfaceFormats::writeOnly().size() + SurfaceFormats::readWriteDepth().size();
    EXPECT_EQ(static_cast<cl_uint>(expected), countFormats(CL_MEM_WRITE_ONLY, CL_MEM_OBJECT_IMAGE2D));
}

TEST_F(ContextImageFormatsFixture, givenWriteOnlyFlagAndImage3dWhenQueryingCountThenOnlyWriteOnlyTableIsIncluded) {
    EXPECT_EQ(static_cast<cl_uint>(SurfaceFormats::writeOnly().size()),
              countFormats(CL_MEM_WRITE_ONLY, CL_MEM_OBJECT_IMAGE3D));
}

TEST_F(ContextImageFormatsFixture, givenNoAccessFlagsAndImage2dWhenQueryingCountThenReadWriteAndDepthAreIncluded) {
    const auto expected = SurfaceFormats::readWrite().size() + SurfaceFormats::readWriteDepth().size();
    EXPECT_EQ(static_cast<cl_uint>(expected), countFormats(0, CL_MEM_OBJECT_IMAGE2D));
}

TEST_F(ContextImageFormatsFixture, givenReadWriteFlagAndImage1dWhenQueryingCountThenOnlyReadWriteTableIsIncluded) {
    EXPECT_EQ(static_cast<cl_uint>(SurfaceFormats::readWrite().size()),
              countFormats(CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE1D));
}

TEST_F(ContextImageFormatsFixture, givenNoAccessIntelFlagAndImage2dWhenQueryingCountThenPlanarYuvFollowsNv12Support) {
    const auto actual = countFormats(CL_MEM_NO_ACCESS_INTEL, CL_MEM_OBJECT_IMAGE2D);

    if (nv12Enabled) {
        EXPECT_EQ(static_cast<cl_uint>(SurfaceFormats::readOnly().size() + SurfaceFormats::planarYuv().size()), actual);
    } else {
        EXPECT_EQ(static_cast<cl_uint>(SurfaceFormats::readWrite().size() + SurfaceFormats::readWriteDepth().size()), actual);
    }
}

TEST_F(ContextImageFormatsFixture, givenReadOnlyFlagAndImage2dWhenListingFormatsThenDepthFormatsArePresent) {
    auto formats = listFormats(CL_MEM_READ_ONLY, CL_MEM_OBJECT_IMAGE2D);
    EXPECT_TRUE(contains(formats, CL_DEPTH));
    EXPECT_TRUE(contains(formats, CL_DEPTH_STENCIL));
}

TEST_F(ContextImageFormatsFixture, givenReadOnlyFlagAndImage3dWhenListingFormatsThenDepthFormatsAreAbsent) {
    auto formats = listFormats(CL_MEM_READ_ONLY, CL_MEM_OBJECT_IMAGE3D);
    EXPECT_FALSE(contains(formats, CL_DEPTH));
    EXPECT_FALSE(contains(formats, CL_DEPTH_STENCIL));
}

TEST_F(ContextImageFormatsFixture, givenWriteOnlyFlagAndImage2dWhenListingFormatsThenSrgbIsAbsentButDepthIsPresent) {
    auto formats = listFormats(CL_MEM_WRITE_ONLY, CL_MEM_OBJECT_IMAGE2D);
    EXPECT_FALSE(contains(formats, CL_sRGBA));
    EXPECT_TRUE(contains(formats, CL_DEPTH));
    EXPECT_FALSE(contains(formats, CL_DEPTH_STENCIL));
}

TEST_F(ContextImageFormatsFixture, givenReadOnlyFlagWhenListingFormatsThenSrgbIsPresent) {
    auto formats = listFormats(CL_MEM_READ_ONLY, CL_MEM_OBJECT_IMAGE2D);
    EXPECT_TRUE(contains(formats, CL_sRGBA));
    EXPECT_TRUE(contains(formats, CL_sBGRA));
}

TEST_F(ContextImageFormatsFixture, givenNv12SupportedWhenListingReadOnlyImage2dFormatsThenPlanarYuvIsPresent) {
    if (!nv12Enabled) {
        GTEST_SKIP() << "Product does not enable the NV12 extension";
    }
    auto formats = listFormats(CL_MEM_READ_ONLY, CL_MEM_OBJECT_IMAGE2D);
    EXPECT_TRUE(contains(formats, CL_NV12_INTEL));
}

TEST_F(ContextImageFormatsFixture, givenNv12SupportedWhenListingReadOnlyImage3dFormatsThenPlanarYuvIsAbsent) {
    if (!nv12Enabled) {
        GTEST_SKIP() << "Product does not enable the NV12 extension";
    }
    auto formats = listFormats(CL_MEM_READ_ONLY, CL_MEM_OBJECT_IMAGE3D);
    EXPECT_FALSE(contains(formats, CL_NV12_INTEL));
}

TEST_F(ContextImageFormatsFixture, givenPackedYuvSupportedWhenListingReadOnlyImage2dFormatsThenPackedYuvIsPresent) {
    if (!packedYuvEnabled) {
        GTEST_SKIP() << "Product does not enable the packed YUV extension";
    }
    auto formats = listFormats(CL_MEM_READ_ONLY, CL_MEM_OBJECT_IMAGE2D);
    EXPECT_TRUE(contains(formats, CL_YUYV_INTEL));
}

TEST_F(ContextImageFormatsFixture, givenSmallerNumEntriesWhenListingFormatsThenOnlyThatManyAreWrittenButCountIsFull) {
    const auto totalFormats = countFormats(CL_MEM_READ_ONLY, CL_MEM_OBJECT_IMAGE2D);
    ASSERT_GT(totalFormats, 2u);

    constexpr cl_uint requested = 2u;
    std::vector<cl_image_format> formats(totalFormats, cl_image_format{0u, 0u});
    cl_uint numFormats = 0u;

    EXPECT_EQ(CL_SUCCESS, context->getSupportedImageFormats(CL_MEM_READ_ONLY, CL_MEM_OBJECT_IMAGE2D,
                                                            requested, formats.data(), &numFormats));

    EXPECT_EQ(totalFormats, numFormats);
    EXPECT_NE(0u, formats[0].image_channel_order);
    EXPECT_NE(0u, formats[1].image_channel_order);
    for (size_t i = requested; i < formats.size(); i++) {
        EXPECT_EQ(0u, formats[i].image_channel_order) << "entry " << i << " written past numEntries";
    }
}

TEST_F(ContextImageFormatsFixture, givenNullCountOutputWhenListingFormatsThenCallStillSucceeds) {
    const auto totalFormats = countFormats(CL_MEM_READ_ONLY, CL_MEM_OBJECT_IMAGE2D);
    std::vector<cl_image_format> formats(totalFormats);

    EXPECT_EQ(CL_SUCCESS, context->getSupportedImageFormats(CL_MEM_READ_ONLY, CL_MEM_OBJECT_IMAGE2D,
                                                            totalFormats, formats.data(), nullptr));
}

TEST_F(ContextImageFormatsFixture, givenNullFormatsBufferWithNonZeroEntriesWhenQueryingThenOnlyCountIsProduced) {
    cl_uint numFormats = 0u;
    EXPECT_EQ(CL_SUCCESS, context->getSupportedImageFormats(CL_MEM_READ_ONLY, CL_MEM_OBJECT_IMAGE2D,
                                                            16u, nullptr, &numFormats));
    EXPECT_EQ(countFormats(CL_MEM_READ_ONLY, CL_MEM_OBJECT_IMAGE2D), numFormats);
}

TEST_F(ContextImageFormatsFixture, givenReadOnlyAndWriteOnlyFlagsWhenQueryingThenReadOnlyPathWins) {
    EXPECT_EQ(countFormats(CL_MEM_READ_ONLY, CL_MEM_OBJECT_IMAGE2D),
              countFormats(CL_MEM_READ_ONLY | CL_MEM_WRITE_ONLY, CL_MEM_OBJECT_IMAGE2D));
}

TEST_F(ContextImageFormatsFixture, givenEveryListedFormatWhenInspectedThenChannelOrderAndTypeAreSet) {
    const cl_mem_object_type imageTypes[] = {CL_MEM_OBJECT_IMAGE1D, CL_MEM_OBJECT_IMAGE2D,
                                             CL_MEM_OBJECT_IMAGE2D_ARRAY, CL_MEM_OBJECT_IMAGE3D};
    const cl_mem_flags flagSets[] = {CL_MEM_READ_ONLY, CL_MEM_WRITE_ONLY, CL_MEM_READ_WRITE, 0};

    for (auto imageType : imageTypes) {
        for (auto flags : flagSets) {
            auto formats = listFormats(flags, imageType);
            EXPECT_FALSE(formats.empty()) << "type " << imageType << " flags " << flags;
            for (const auto &format : formats) {
                EXPECT_NE(0u, format.image_channel_order);
                EXPECT_NE(0u, format.image_channel_data_type);
            }
        }
    }
}

} // namespace ult
} // namespace LEO
} // namespace NEO
