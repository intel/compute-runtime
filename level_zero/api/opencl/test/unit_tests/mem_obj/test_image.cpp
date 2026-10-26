/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/helpers/leo_cl_memory_properties_helpers.h"
#include "level_zero/api/opencl/source/mem_obj/leo_buffer.h"
#include "level_zero/api/opencl/source/mem_obj/leo_image.h"
#include "level_zero/api/opencl/test/common/fixtures/capturing_context.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include "CL/cl.h"

#include <limits>
#include <memory>
#include <vector>

namespace NEO {
namespace LEO {
namespace ult {

struct ImageDestructorTest : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();
        cl_device_id clDeviceId = clDevice;
        capturingContext = std::make_unique<CapturingContext>(driverHandle.get(), clDevice->getL0Handle());
        leoContext = std::make_unique<Context>(nullptr, capturingContext->toHandle(), 1, &clDeviceId, true);
    }

    void TearDown() override {
        leoContext.reset();
        capturingContext.reset();
        Test<OclFixture>::TearDown();
    }

    Buffer *createBuffer(void *usmPtr) {
        auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &clDevice->getDevice());
        return new Buffer(leoContext.get(), memoryProperties, CL_MEM_READ_WRITE, usmPtr, nullptr, sizeof(dummyStorage), false);
    }

    Image *createImageFrom(MemObj *parent) {
        auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &clDevice->getDevice());
        cl_image_format format{CL_R, CL_UNORM_INT8};
        // Null handle - the destructor then skips zeImageDestroy.
        return new Image(leoContext.get(), memoryProperties, CL_MEM_READ_WRITE, nullptr, nullptr, nullptr, false,
                         format, static_cast<cl_mem>(parent));
    }

    ClDevice *clDevice = nullptr;
    std::unique_ptr<CapturingContext> capturingContext;
    std::unique_ptr<Context> leoContext;

    uint64_t dummyStorage = 0u;
};

TEST_F(ImageDestructorTest, givenImageCreatedFromBufferWhenImageIsDestroyedThenParentBufferIsReleased) {
    auto buffer = createBuffer(&dummyStorage);
    const auto refCountWithoutImage = buffer->getRefInternalCount();

    auto image = createImageFrom(buffer);
    EXPECT_EQ(refCountWithoutImage + 1, buffer->getRefInternalCount());

    delete image;
    EXPECT_EQ(refCountWithoutImage, buffer->getRefInternalCount());

    delete buffer;
}

TEST_F(ImageDestructorTest, givenBufferReleasedBeforeImageCreatedFromItThenBufferStorageIsFreedOnlyAfterImageIsReleased) {
    void *usmPtr = &dummyStorage;
    auto buffer = createBuffer(usmPtr);
    auto image = createImageFrom(buffer);

    buffer->decRefApi();
    EXPECT_FALSE(capturingContext->freeMemExtArgs.wasCalled());

    image->decRefApi();

    ASSERT_EQ(1u, capturingContext->freeMemExtArgs.count());
    EXPECT_EQ(usmPtr, capturingContext->freeMemExtArgs[0].ptr);
}

TEST_F(ImageDestructorTest, givenParentBufferWithSharingHandlerWhenImageIsCreatedFromItThenHandlerIsInherited) {
    auto buffer = createBuffer(&dummyStorage);
    buffer->setSharingHandler(new SharingHandler());

    auto image = createImageFrom(buffer);
    EXPECT_EQ(buffer->peekSharingHandler(), image->peekSharingHandler());

    delete image;
    // Keeps ~Buffer on the plain USM free branch instead of alloc-data teardown.
    buffer->setSharingHandler(nullptr);
    delete buffer;
}

struct ImageHostPtrSizeTest : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();
        if (!clDevice->getHardwareInfo().capabilityTable.supportsImages) {
            GTEST_SKIP() << "Product does not support images";
        }
        cl_device_id clDeviceId = clDevice;
        leoContext = std::make_unique<Context>(nullptr, this->L0::ult::DeviceFixture::context->toHandle(), 1, &clDeviceId, true);
        ASSERT_EQ(CL_SUCCESS, leoContext->initialize());
    }

    void TearDown() override {
        for (auto &createdImage : createdImages) {
            EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(createdImage));
        }
        createdImages.clear();
        leoContext.reset();
        Test<OclFixture>::TearDown();
    }

    Image *createImage(const cl_image_desc &desc) {
        cl_image_format format{CL_RGBA, CL_UNORM_INT8};
        cl_int errcode = CL_INVALID_VALUE;
        auto createdImage = clCreateImage(leoContext.get(), CL_MEM_READ_WRITE, &format, &desc, nullptr, &errcode);
        EXPECT_EQ(CL_SUCCESS, errcode);
        if (createdImage == nullptr) {
            return nullptr;
        }
        createdImages.push_back(createdImage);
        return static_cast<Image *>(castToObject<MemObj>(createdImage));
    }

    static size_t expectedHostPtrSize(size_t rows, size_t slices) {
        const size_t rowPitch = width * pixelSize;
        return (slices - 1) * rowPitch * rows + (rows - 1) * rowPitch + width * pixelSize;
    }

    static constexpr size_t pixelSize = 4;
    static constexpr size_t width = 16;
    static constexpr size_t height = 8;
    static constexpr size_t depth = 4;
    static constexpr size_t arraySize = 4;

    ClDevice *clDevice = nullptr;
    std::unique_ptr<Context> leoContext;
    std::vector<cl_mem> createdImages;
};

TEST_F(ImageHostPtrSizeTest, given2dImageWhenQueryingHostPtrSizeThenTrailingRowIsNotIncluded) {
    cl_image_desc desc{};
    desc.image_type = CL_MEM_OBJECT_IMAGE2D;
    desc.image_width = width;
    desc.image_height = height;

    auto image = createImage(desc);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ(expectedHostPtrSize(height, 1u), image->getHostptrSize());
}

TEST_F(ImageHostPtrSizeTest, given3dImageWhenQueryingHostPtrSizeThenTrailingRowAndSliceAreNotIncluded) {
    cl_image_desc desc{};
    desc.image_type = CL_MEM_OBJECT_IMAGE3D;
    desc.image_width = width;
    desc.image_height = height;
    desc.image_depth = depth;

    auto image = createImage(desc);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ(expectedHostPtrSize(height, depth), image->getHostptrSize());
}

TEST_F(ImageHostPtrSizeTest, given2dImageArrayWhenQueryingHostPtrSizeThenArraySizeDrivesSliceCount) {
    cl_image_desc desc{};
    desc.image_type = CL_MEM_OBJECT_IMAGE2D_ARRAY;
    desc.image_width = width;
    desc.image_height = height;
    desc.image_array_size = arraySize;

    auto image = createImage(desc);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ(expectedHostPtrSize(height, arraySize), image->getHostptrSize());
}

TEST_F(ImageHostPtrSizeTest, given1dImageArrayWhenQueryingHostPtrSizeThenArraySizeDrivesSlicePitchCount) {
    cl_image_desc desc{};
    desc.image_type = CL_MEM_OBJECT_IMAGE1D_ARRAY;
    desc.image_width = width;
    desc.image_array_size = arraySize;

    auto image = createImage(desc);
    ASSERT_NE(nullptr, image);

    const auto expectedSize = (arraySize - 1) * width * pixelSize + width * pixelSize;

    EXPECT_EQ(expectedSize, image->getHostptrSize());
}

TEST_F(ImageHostPtrSizeTest, given2dImageWhenQueryingHostPtrSizeThenItIsSmallerThanOffsetOfFullImageEnd) {
    cl_image_desc desc{};
    desc.image_type = CL_MEM_OBJECT_IMAGE2D;
    desc.image_width = width;
    desc.image_height = height;

    auto image = createImage(desc);
    ASSERT_NE(nullptr, image);

    EXPECT_LT(image->getHostptrSize(), image->calculateTotalSizeForImage({width, height, 1u}));
}

TEST_F(ImageHostPtrSizeTest, givenMipmapped3dImageWhenQueryingHostLayoutThenMipLevelsArePackedOneAfterAnother) {
    cl_image_desc desc{};
    desc.image_type = CL_MEM_OBJECT_IMAGE3D;
    desc.image_width = width;
    desc.image_height = height;
    desc.image_depth = depth;
    desc.num_mip_levels = 3;

    auto image = createImage(desc);
    ASSERT_NE(nullptr, image);

    const size_t mipLevel1Offset = expectedHostPtrSize(height, depth);
    const size_t mipLevel1RowPitch = (width / 2) * pixelSize;
    const size_t mipLevel1SlicePitch = mipLevel1RowPitch * (height / 2);
    const size_t mipLevel2Offset = mipLevel1Offset + (depth / 2 - 1) * mipLevel1SlicePitch + (height / 2) * mipLevel1RowPitch;
    const auto mipLevel2Layout = image->getMipLevelHostLayout(2u);
    EXPECT_EQ(mipLevel2Offset, mipLevel2Layout.offset);
    EXPECT_EQ(4 * pixelSize, mipLevel2Layout.rowPitch);
    EXPECT_EQ(2 * 4 * pixelSize, mipLevel2Layout.slicePitch);
    EXPECT_EQ(mipLevel2Offset + 2 * 4 * pixelSize, image->getHostptrSize());
}

TEST_F(ImageHostPtrSizeTest, givenMipmapped2dImageArrayWhenQueryingHostLayoutThenArraySizeIsNotReducedForMipLevel) {
    cl_image_desc desc{};
    desc.image_type = CL_MEM_OBJECT_IMAGE2D_ARRAY;
    desc.image_width = width;
    desc.image_height = height;
    desc.image_array_size = arraySize;
    desc.num_mip_levels = 2;

    auto image = createImage(desc);
    ASSERT_NE(nullptr, image);

    const size_t mipLevel1Offset = expectedHostPtrSize(height, arraySize);
    const size_t mipLevel1RowPitch = (width / 2) * pixelSize;
    const size_t mipLevel1SlicePitch = mipLevel1RowPitch * (height / 2);
    const auto mipLevel1Layout = image->getMipLevelHostLayout(1u);
    EXPECT_EQ(mipLevel1Offset, mipLevel1Layout.offset);
    EXPECT_EQ(mipLevel1RowPitch, mipLevel1Layout.rowPitch);
    EXPECT_EQ(mipLevel1SlicePitch, mipLevel1Layout.slicePitch);
    EXPECT_EQ(mipLevel1Offset + (arraySize - 1) * mipLevel1SlicePitch + (height / 2) * mipLevel1RowPitch, image->getHostptrSize());
}

TEST_F(ImageHostPtrSizeTest, givenMipmapped1dImageArrayWhenQueryingHostLayoutThenSlicePitchIsMipLevelRowPitch) {
    cl_image_desc desc{};
    desc.image_type = CL_MEM_OBJECT_IMAGE1D_ARRAY;
    desc.image_width = width;
    desc.image_array_size = arraySize;
    desc.num_mip_levels = 2;

    auto image = createImage(desc);
    ASSERT_NE(nullptr, image);

    const size_t mipLevel1Offset = arraySize * width * pixelSize;
    const size_t mipLevel1RowPitch = (width / 2) * pixelSize;
    const auto mipLevel1Layout = image->getMipLevelHostLayout(1u);
    EXPECT_EQ(mipLevel1Offset, mipLevel1Layout.offset);
    EXPECT_EQ(mipLevel1RowPitch, mipLevel1Layout.rowPitch);
    EXPECT_EQ(mipLevel1RowPitch, mipLevel1Layout.slicePitch);
    EXPECT_EQ(mipLevel1Offset + arraySize * mipLevel1RowPitch, image->getHostptrSize());
}

struct ImageGetInfoPitchTest : public ImageHostPtrSizeTest {
    static size_t queryInfo(Image &image, cl_image_info paramName) {
        size_t value = std::numeric_limits<size_t>::max();
        EXPECT_EQ(CL_SUCCESS, image.getImageInfo(paramName, sizeof(value), &value, nullptr));
        return value;
    }

    static constexpr size_t unalignedWidth = 329;
    static constexpr size_t elementSize = 4;
};

TEST_F(ImageGetInfoPitchTest, given1dImageWithoutRowPitchWhenQueryingPitchesThenRowPitchIsWidthTimesElementSizeAndSlicePitchIsZero) {
    cl_image_desc desc{};
    desc.image_type = CL_MEM_OBJECT_IMAGE1D;
    desc.image_width = unalignedWidth;

    auto image = createImage(desc);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ(unalignedWidth * elementSize, queryInfo(*image, CL_IMAGE_ROW_PITCH));
    EXPECT_EQ(0u, queryInfo(*image, CL_IMAGE_SLICE_PITCH));
}

TEST_F(ImageGetInfoPitchTest, given2dImageWithoutRowPitchWhenQueryingPitchesThenRowPitchIsWidthTimesElementSizeAndSlicePitchIsZero) {
    cl_image_desc desc{};
    desc.image_type = CL_MEM_OBJECT_IMAGE2D;
    desc.image_width = unalignedWidth;
    desc.image_height = height;

    auto image = createImage(desc);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ(unalignedWidth * elementSize, queryInfo(*image, CL_IMAGE_ROW_PITCH));
    EXPECT_EQ(0u, queryInfo(*image, CL_IMAGE_SLICE_PITCH));
}

TEST_F(ImageGetInfoPitchTest, given3dImageWithoutPitchesWhenQueryingPitchesThenSlicePitchIsRowPitchTimesHeight) {
    cl_image_desc desc{};
    desc.image_type = CL_MEM_OBJECT_IMAGE3D;
    desc.image_width = unalignedWidth;
    desc.image_height = height;
    desc.image_depth = depth;

    auto image = createImage(desc);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ(unalignedWidth * elementSize, queryInfo(*image, CL_IMAGE_ROW_PITCH));
    EXPECT_EQ(unalignedWidth * elementSize * height, queryInfo(*image, CL_IMAGE_SLICE_PITCH));
}

TEST_F(ImageGetInfoPitchTest, given2dImageArrayWithoutPitchesWhenQueryingPitchesThenSlicePitchIsRowPitchTimesHeight) {
    cl_image_desc desc{};
    desc.image_type = CL_MEM_OBJECT_IMAGE2D_ARRAY;
    desc.image_width = unalignedWidth;
    desc.image_height = height;
    desc.image_array_size = arraySize;

    auto image = createImage(desc);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ(unalignedWidth * elementSize, queryInfo(*image, CL_IMAGE_ROW_PITCH));
    EXPECT_EQ(unalignedWidth * elementSize * height, queryInfo(*image, CL_IMAGE_SLICE_PITCH));
}

TEST_F(ImageGetInfoPitchTest, given1dImageArrayWithoutPitchesWhenQueryingPitchesThenSlicePitchIsRowPitch) {
    cl_image_desc desc{};
    desc.image_type = CL_MEM_OBJECT_IMAGE1D_ARRAY;
    desc.image_width = unalignedWidth;
    desc.image_array_size = arraySize;

    auto image = createImage(desc);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ(unalignedWidth * elementSize, queryInfo(*image, CL_IMAGE_ROW_PITCH));
    EXPECT_EQ(unalignedWidth * elementSize, queryInfo(*image, CL_IMAGE_SLICE_PITCH));
}

TEST_F(ImageGetInfoPitchTest, given3dImageWithHostPtrPitchesWhenQueryingPitchesThenHostPtrPitchesAreReturned) {
    cl_image_desc desc{};
    desc.image_type = CL_MEM_OBJECT_IMAGE3D;
    desc.image_width = unalignedWidth;
    desc.image_height = height;
    desc.image_depth = depth;

    auto image = createImage(desc);
    ASSERT_NE(nullptr, image);

    const size_t hostRowPitch = unalignedWidth * elementSize + 12;
    const size_t hostSlicePitch = hostRowPitch * height + 36;
    image->setHostPtrRowPitch(hostRowPitch);
    image->setHostPtrSlicePitch(hostSlicePitch);

    EXPECT_EQ(hostRowPitch, queryInfo(*image, CL_IMAGE_ROW_PITCH));
    EXPECT_EQ(hostSlicePitch, queryInfo(*image, CL_IMAGE_SLICE_PITCH));
}

TEST_F(ImageGetInfoPitchTest, given3dImageWithHostPtrRowPitchOnlyWhenQueryingSlicePitchThenItIsHostRowPitchTimesHeight) {
    cl_image_desc desc{};
    desc.image_type = CL_MEM_OBJECT_IMAGE3D;
    desc.image_width = unalignedWidth;
    desc.image_height = height;
    desc.image_depth = depth;

    auto image = createImage(desc);
    ASSERT_NE(nullptr, image);

    const size_t hostRowPitch = unalignedWidth * elementSize + 12;
    image->setHostPtrRowPitch(hostRowPitch);

    EXPECT_EQ(hostRowPitch, queryInfo(*image, CL_IMAGE_ROW_PITCH));
    EXPECT_EQ(hostRowPitch * height, queryInfo(*image, CL_IMAGE_SLICE_PITCH));
}

TEST_F(ImageGetInfoPitchTest, given2dImageWithUnalignedWidthWhenCalculatingHostPtrOffsetAndSizeThenHostRowPitchIsUsed) {
    cl_image_desc desc{};
    desc.image_type = CL_MEM_OBJECT_IMAGE2D;
    desc.image_width = unalignedWidth;
    desc.image_height = height;

    auto image = createImage(desc);
    ASSERT_NE(nullptr, image);

    const size_t hostRowPitch = unalignedWidth * elementSize;
    EXPECT_EQ(2 * hostRowPitch + 3 * elementSize, image->calculateHostPtrOffsetForImage({3u, 2u, 0u}, image->getMipLevelHostLayout(0u)));
    EXPECT_EQ((height - 1) * hostRowPitch + unalignedWidth * elementSize, image->getHostptrSize());
}

TEST_F(ImageGetInfoPitchTest, given3dImageWithUnalignedWidthWhenCalculatingHostPtrOffsetAndSizeThenHostPitchesAreUsed) {
    cl_image_desc desc{};
    desc.image_type = CL_MEM_OBJECT_IMAGE3D;
    desc.image_width = unalignedWidth;
    desc.image_height = height;
    desc.image_depth = depth;

    auto image = createImage(desc);
    ASSERT_NE(nullptr, image);

    const size_t hostRowPitch = unalignedWidth * elementSize;
    const size_t hostSlicePitch = hostRowPitch * height;
    EXPECT_EQ(2 * hostSlicePitch + 1 * hostRowPitch + 5 * elementSize, image->calculateHostPtrOffsetForImage({5u, 1u, 2u}, image->getMipLevelHostLayout(0u)));
    EXPECT_EQ((depth - 1) * hostSlicePitch + (height - 1) * hostRowPitch + unalignedWidth * elementSize, image->getHostptrSize());
}

TEST_F(ImageGetInfoPitchTest, given1dImageArrayWithUnalignedWidthWhenCalculatingHostPtrOffsetAndSizeThenHostRowPitchSeparatesLayers) {
    cl_image_desc desc{};
    desc.image_type = CL_MEM_OBJECT_IMAGE1D_ARRAY;
    desc.image_width = unalignedWidth;
    desc.image_array_size = arraySize;

    auto image = createImage(desc);
    ASSERT_NE(nullptr, image);

    const size_t hostRowPitch = unalignedWidth * elementSize;
    EXPECT_EQ(3 * hostRowPitch + 7 * elementSize, image->calculateHostPtrOffsetForImage({7u, 3u, 0u}, image->getMipLevelHostLayout(0u)));
    EXPECT_EQ((arraySize - 1) * hostRowPitch + unalignedWidth * elementSize, image->getHostptrSize());
}

TEST_F(ImageGetInfoPitchTest, given2dImageArrayWithHostPtrPitchesWhenCalculatingHostPtrOffsetThenHostPtrPitchesAreUsed) {
    cl_image_desc desc{};
    desc.image_type = CL_MEM_OBJECT_IMAGE2D_ARRAY;
    desc.image_width = unalignedWidth;
    desc.image_height = height;
    desc.image_array_size = arraySize;

    auto image = createImage(desc);
    ASSERT_NE(nullptr, image);

    const size_t hostRowPitch = unalignedWidth * elementSize + 12;
    const size_t hostSlicePitch = hostRowPitch * height + 36;
    image->setHostPtrRowPitch(hostRowPitch);
    image->setHostPtrSlicePitch(hostSlicePitch);

    EXPECT_EQ(3 * hostSlicePitch + 2 * hostRowPitch + 1 * elementSize, image->calculateHostPtrOffsetForImage({1u, 2u, 3u}, image->getMipLevelHostLayout(0u)));
}

TEST(ImageFormatConversionTest, givenPackedYuvChannelOrderWhenConvertingToL0FormatThenLayoutIsMapped) {
    const std::pair<cl_channel_order, ze_image_format_layout_t> packedYuvFormats[] = {
        {CL_YUYV_INTEL, ZE_IMAGE_FORMAT_LAYOUT_YUYV},
        {CL_UYVY_INTEL, ZE_IMAGE_FORMAT_LAYOUT_UYVY},
        {CL_YVYU_INTEL, ZE_IMAGE_FORMAT_LAYOUT_YVYU},
        {CL_VYUY_INTEL, ZE_IMAGE_FORMAT_LAYOUT_VYUY}};

    for (const auto &[channelOrder, expectedLayout] : packedYuvFormats) {
        ze_image_format_t l0Format{};
        Image::clToL0ImageFormat(l0Format, channelOrder, CL_UNORM_INT8);

        EXPECT_EQ(expectedLayout, l0Format.layout);
    }
}

} // namespace ult
} // namespace LEO
} // namespace NEO
