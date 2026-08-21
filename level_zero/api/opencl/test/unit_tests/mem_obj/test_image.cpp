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

    static size_t expectedHostPtrSize(const Image &image, size_t rows, size_t slices) {
        const auto &imgInfo = image.getL0Object()->getImageInfo();
        return (slices - 1) * imgInfo.slicePitch + (rows - 1) * imgInfo.rowPitch + width * imgInfo.surfaceFormat->imageElementSizeInBytes;
    }

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

    EXPECT_EQ(expectedHostPtrSize(*image, height, 1u), image->getHostptrSize());
}

TEST_F(ImageHostPtrSizeTest, given3dImageWhenQueryingHostPtrSizeThenTrailingRowAndSliceAreNotIncluded) {
    cl_image_desc desc{};
    desc.image_type = CL_MEM_OBJECT_IMAGE3D;
    desc.image_width = width;
    desc.image_height = height;
    desc.image_depth = depth;

    auto image = createImage(desc);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ(expectedHostPtrSize(*image, height, depth), image->getHostptrSize());
}

TEST_F(ImageHostPtrSizeTest, given2dImageArrayWhenQueryingHostPtrSizeThenArraySizeDrivesSliceCount) {
    cl_image_desc desc{};
    desc.image_type = CL_MEM_OBJECT_IMAGE2D_ARRAY;
    desc.image_width = width;
    desc.image_height = height;
    desc.image_array_size = arraySize;

    auto image = createImage(desc);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ(expectedHostPtrSize(*image, height, arraySize), image->getHostptrSize());
}

TEST_F(ImageHostPtrSizeTest, given1dImageArrayWhenQueryingHostPtrSizeThenArraySizeDrivesSlicePitchCount) {
    cl_image_desc desc{};
    desc.image_type = CL_MEM_OBJECT_IMAGE1D_ARRAY;
    desc.image_width = width;
    desc.image_array_size = arraySize;

    auto image = createImage(desc);
    ASSERT_NE(nullptr, image);

    const auto &imgInfo = image->getL0Object()->getImageInfo();
    const auto expectedSize = (arraySize - 1) * imgInfo.slicePitch + width * imgInfo.surfaceFormat->imageElementSizeInBytes;

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
