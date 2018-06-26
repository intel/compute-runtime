/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#include "runtime/mem_obj/image.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/platform/platform.h"
#include "unit_tests/aub_tests/command_stream/aub_command_stream_fixture.h"
#include "unit_tests/command_queue/command_enqueue_fixture.h"
#include "test.h"
#include "unit_tests/mocks/mock_gmm.h"

extern GFXCORE_FAMILY renderCoreFamily;

using namespace OCLRT;

static const unsigned int testImageDimensions = 17;
auto const elementSize = 4; //sizeof CL_RGBA * CL_UNORM_INT8
static cl_mem_object_type ImgArrayTypes[] = {
    CL_MEM_OBJECT_IMAGE1D_ARRAY,
    CL_MEM_OBJECT_IMAGE2D_ARRAY};

struct AUBCreateImage
    : public CommandDeviceFixture,
      public AUBCommandStreamFixture,
      public ::testing::Test {
    typedef AUBCommandStreamFixture CommandStreamFixture;

    using AUBCommandStreamFixture::SetUp;

    void SetUp() override {
        constructPlatform();
        CommandDeviceFixture::SetUp(cl_command_queue_properties(0));
        CommandStreamFixture::SetUp(pCmdQ);

        imageFormat.image_channel_data_type = CL_UNORM_INT8;
        imageFormat.image_channel_order = CL_RGBA;

        imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
        imageDesc.image_width = testImageDimensions;
        imageDesc.image_height = testImageDimensions;
        imageDesc.image_depth = 0;
        imageDesc.image_array_size = 10;
        imageDesc.image_row_pitch = 0;
        imageDesc.image_slice_pitch = 0;
        imageDesc.num_mip_levels = 0;
        imageDesc.num_samples = 0;
        imageDesc.mem_object = NULL;
    }

    void TearDown() override {
        delete image;
        //delete context;
        CommandStreamFixture::TearDown();
        CommandDeviceFixture::TearDown();
        platformImpl.reset(nullptr);
    }

    //MockContext *context;
    Image *image = nullptr;
    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_int retVal;
    unsigned char pHostPtr[512 * testImageDimensions * elementSize * 4];
};

struct AUBCreateImageArray : public AUBCreateImage,
                             public ::testing::WithParamInterface<uint32_t /*cl_mem_object_type*/> {
    void SetUp() override {
        AUBCreateImage::SetUp();
    }
    void TearDown() override {
        AUBCreateImage::TearDown();
    }
};

INSTANTIATE_TEST_CASE_P(
    CreateImgTest_Arrays,
    AUBCreateImageArray,
    testing::ValuesIn(ImgArrayTypes));

HWTEST_P(AUBCreateImageArray, CheckArrayImages) {
    imageDesc.image_type = GetParam();
    if (imageDesc.image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY) {
        imageDesc.image_height = 1;
    }
    cl_mem_flags flags = CL_MEM_COPY_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    auto imgInfo = MockGmm::initImgInfo(imageDesc, 0, surfaceFormat);
    auto queryGmm = MockGmm::queryImgParams(imgInfo);

    //allocate host_ptr
    auto pixelSize = 4;
    auto storageSize = imageDesc.image_array_size * pixelSize * imageDesc.image_width * imageDesc.image_height;

    int *hostPtr = new int[storageSize];

    for (auto i = 0u; i < storageSize; i++) {
        hostPtr[i] = i;
    }

    auto image = Image::create(
        context,
        flags,
        surfaceFormat,
        &imageDesc,
        hostPtr,
        retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ(image->getSize(), imgInfo.size);
    EXPECT_EQ(image->getImageDesc().image_slice_pitch, imgInfo.slicePitch);
    EXPECT_EQ(image->getImageDesc().image_row_pitch, imgInfo.rowPitch);
    EXPECT_GE(image->getImageDesc().image_slice_pitch, image->getImageDesc().image_row_pitch);
    EXPECT_EQ(image->getQPitch(), imgInfo.qPitch);
    EXPECT_EQ(image->getCubeFaceIndex(), static_cast<uint32_t>(__GMM_NO_CUBE_MAP));

    auto address = (int *)image->getCpuAddress();

    ASSERT_NE(nullptr, address);

    auto imageHeight = imageDesc.image_height;

    uint32_t *readMemory = nullptr;
    if (image->allowTiling()) {
        readMemory = new uint32_t[image->getSize()];
        size_t imgOrigin[] = {0, 0, 0};
        size_t imgRegion[] = {imageDesc.image_width, imageDesc.image_height, imageDesc.image_array_size};
        retVal = pCmdQ->enqueueReadImage(image, CL_TRUE, imgOrigin, imgRegion, imgInfo.rowPitch, imgInfo.slicePitch,
                                         readMemory, 0, nullptr, nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    auto currentCounter = 0;
    for (auto array = 0u; array < imageDesc.image_array_size; array++) {
        for (auto height = 0u; height < imageHeight; height++) {
            for (auto element = 0u; element < imageDesc.image_width; element++) {
                auto offset = (array * imgInfo.slicePitch + element * pixelSize + height * imgInfo.rowPitch) / 4;
                if (image->allowTiling()) {
                    AUBCommandStreamFixture::expectMemory<FamilyType>(&readMemory[offset], &currentCounter, pixelSize);
                } else {
                    EXPECT_EQ(currentCounter, address[offset]);
                }
                currentCounter++;
            }
        }
    }

    if (readMemory) {
        delete readMemory;
    }
    delete image;
    delete[] hostPtr;
}

struct AUBCreateImageHostPtr : public AUBCreateImage,
                               public ::testing::WithParamInterface<uint64_t /*cl_mem_object_type*/> {
    void SetUp() override {
        AUBCreateImage::SetUp();
    }
    void TearDown() override {
        AUBCreateImage::TearDown();
    }
};

static cl_mem_flags hostPtrFlags[] = {
    0 | CL_MEM_USE_HOST_PTR,
    CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
    CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR,
    CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
    CL_MEM_HOST_READ_ONLY | CL_MEM_USE_HOST_PTR,
    CL_MEM_HOST_WRITE_ONLY | CL_MEM_USE_HOST_PTR,
    CL_MEM_HOST_NO_ACCESS | CL_MEM_USE_HOST_PTR,
    0 | CL_MEM_COPY_HOST_PTR,
    CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
    CL_MEM_WRITE_ONLY | CL_MEM_COPY_HOST_PTR,
    CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
    CL_MEM_HOST_READ_ONLY | CL_MEM_COPY_HOST_PTR,
    CL_MEM_HOST_WRITE_ONLY | CL_MEM_COPY_HOST_PTR,
    CL_MEM_HOST_NO_ACCESS | CL_MEM_COPY_HOST_PTR};

INSTANTIATE_TEST_CASE_P(
    CreateImgTest_HostPtr,
    AUBCreateImageHostPtr,
    testing::ValuesIn(hostPtrFlags));

HWTEST_P(AUBCreateImageHostPtr, imageWithDoubledRowPitchThatIsCreatedWithCopyHostPtrFlagHasProperRowPitchSet) {
    auto flags = GetParam();
    if (flags & CL_MEM_COPY_HOST_PTR) {
        auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
        auto imgInfo = MockGmm::initImgInfo(imageDesc, 0, surfaceFormat);

        MockGmm::queryImgParams(imgInfo);
        auto lineWidth = imageDesc.image_width * elementSize;
        auto passedRowPitch = imgInfo.rowPitch * 2;
        imageDesc.image_row_pitch = passedRowPitch;

        char counter = 0;

        char *data = (char *)pHostPtr;
        auto heightToCopy = imageDesc.image_height;

        while (heightToCopy--) {
            for (unsigned int i = 0; i < imageDesc.image_width * elementSize; i++) {
                data[i] = counter++;
            }

            data += passedRowPitch;
        }

        image = Image::create(context, flags, surfaceFormat, &imageDesc, pHostPtr, retVal);
        ASSERT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(image->getImageDesc().image_row_pitch, imgInfo.rowPitch);
        EXPECT_EQ(image->getHostPtrRowPitch(), (size_t)passedRowPitch);
        EXPECT_EQ(image->getSize(), imgInfo.size);
        EXPECT_EQ(image->getImageDesc().image_slice_pitch, imgInfo.slicePitch);
        EXPECT_GE(image->getImageDesc().image_slice_pitch, image->getImageDesc().image_row_pitch);
        EXPECT_EQ(image->getQPitch(), imgInfo.qPitch);
        EXPECT_EQ(image->getCubeFaceIndex(), static_cast<uint32_t>(__GMM_NO_CUBE_MAP));

        //now check if data is properly propagated to image

        heightToCopy = imageDesc.image_height;
        auto imageStorage = static_cast<uint8_t *>(image->getCpuAddress());
        data = (char *)pHostPtr;

        uint8_t *readMemory = nullptr;
        if (image->allowTiling()) {
            readMemory = new uint8_t[testImageDimensions * testImageDimensions * elementSize * 4];
            size_t imgOrigin[] = {0, 0, 0};
            size_t imgRegion[] = {imageDesc.image_width, imageDesc.image_height, imageDesc.image_depth ? imageDesc.image_depth : 1};
            retVal = pCmdQ->enqueueReadImage(image, CL_TRUE, imgOrigin, imgRegion, 0, 0, readMemory, 0, nullptr, nullptr);
            EXPECT_EQ(CL_SUCCESS, retVal);
            imageStorage = readMemory;
        }

        while (heightToCopy--) {
            for (unsigned int i = 0; i < imageDesc.image_width * elementSize; i++) {
                if (image->allowTiling()) {
                    AUBCommandStreamFixture::expectMemory<FamilyType>(&imageStorage[i], &data[i], 1);
                } else {
                    EXPECT_EQ(imageStorage[i], data[i]);
                }
            }
            data += passedRowPitch;
            imageStorage += lineWidth;
        }

        if (readMemory)
            delete readMemory;
    }
}

HWTEST_P(AUBCreateImageHostPtr, imageWithRowPitchCreatedWithUseHostPtrFlagCopiedActuallyVerifyMapImageData) {
    auto flags = GetParam();
    if (flags & CL_MEM_USE_HOST_PTR) {
        imageDesc.image_width = 546;
        imageDesc.image_height = 1;
        auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
        auto imgInfo = MockGmm::initImgInfo(imageDesc, 0, surfaceFormat);
        MockGmm::queryImgParams(imgInfo);
        auto passedRowPitch = imgInfo.rowPitch + 32;
        imageDesc.image_row_pitch = passedRowPitch;
        unsigned char *pUseHostPtr = new unsigned char[passedRowPitch * imageDesc.image_height * elementSize];

        char counter = 0;

        char *data = (char *)pUseHostPtr;
        auto heightToCopy = imageDesc.image_height;

        while (heightToCopy--) {
            for (unsigned int i = 0; i < imageDesc.image_width * elementSize; i++) {
                data[i] = counter++;
            }

            data += passedRowPitch;
        }
        image = Image::create(
            context,
            flags,
            surfaceFormat,
            &imageDesc,
            pUseHostPtr,
            retVal);
        ASSERT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(image->getImageDesc().image_row_pitch, imgInfo.rowPitch);
        EXPECT_EQ(image->getHostPtrRowPitch(), (size_t)passedRowPitch);
        EXPECT_EQ(image->getSize(), imgInfo.size);
        EXPECT_EQ(image->getImageDesc().image_slice_pitch, imgInfo.slicePitch);
        EXPECT_GE(image->getImageDesc().image_slice_pitch, image->getImageDesc().image_row_pitch);
        EXPECT_EQ(image->getQPitch(), imgInfo.qPitch);
        EXPECT_EQ(image->getCubeFaceIndex(), static_cast<uint32_t>(__GMM_NO_CUBE_MAP));

        //now check if data is properly propagated to image
        auto mapFlags = CL_MAP_READ;
        const size_t origin[3] = {0, 0, 0};
        const size_t region[3] = {imageDesc.image_width, imageDesc.image_height, 1};
        size_t imageRowPitch = 0;
        size_t imageSlicePitch = 0;
        auto ptr = pCmdQ->enqueueMapImage(
            image,
            true,
            mapFlags,
            origin,
            region,
            &imageRowPitch,
            &imageSlicePitch,
            0,
            nullptr,
            nullptr,
            retVal);
        if (image->isMemObjZeroCopy()) {
            EXPECT_EQ(image->getCpuAddress(), ptr);
        } else {
            EXPECT_NE(image->getCpuAddress(), ptr);
        }
        size_t imageRowPitchRef = 0;
        image->getImageInfo(CL_IMAGE_ROW_PITCH, sizeof(imageRowPitchRef), &imageRowPitchRef, nullptr);

        // Only ZeroCopy HOST_PTR image has the same row_pitch as the one from map, otherwise mapped ptr may have different row_pitch
        if (image->isMemObjZeroCopy()) {
            EXPECT_EQ(imageRowPitch, imageRowPitchRef);
        }

        size_t imageSlicePitchRef = 0;
        image->getImageInfo(CL_IMAGE_SLICE_PITCH, sizeof(imageSlicePitchRef), &imageSlicePitchRef, nullptr);

        // Only ZeroCopy HOST_PTR image has the same slice_pitch as the one from map, otherwise mapped ptr may have different slice_pitch
        if (image->isMemObjZeroCopy()) {
            EXPECT_EQ(imageSlicePitch, imageSlicePitchRef);
        }

        heightToCopy = imageDesc.image_height;
        char *imageStorage = (char *)ptr;
        data = (char *)pUseHostPtr;

        while (heightToCopy--) {
            for (unsigned int i = 0; i < imageDesc.image_width * elementSize; i++) {
                if (image->allowTiling()) {
                    AUBCommandStreamFixture::expectMemory<FamilyType>(&imageStorage[i], &data[i], 1);
                } else {
                    EXPECT_EQ(imageStorage[i], data[i]);
                }
            }
            data += passedRowPitch;
            imageStorage += imageRowPitch;
        }

        retVal = clEnqueueUnmapMemObject(pCmdQ, image, ptr, 0, nullptr, nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);
        delete[] pUseHostPtr;
    }
}

HWTEST_F(AUBCreateImage, image3DCreatedWithDoubledSlicePitchWhenQueriedForDataReturnsProperData) {
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE3D;
    imageDesc.image_depth = testImageDimensions;

    auto imageSize = imageDesc.image_width * imageDesc.image_height * imageDesc.image_depth * 4 * 2;

    auto computedSlicePitch = imageDesc.image_width * alignUp(imageDesc.image_height, 4) * 4;
    auto inputSlicePitch = computedSlicePitch * 2;

    imageDesc.image_slice_pitch = inputSlicePitch;

    auto host_ptr = alignedMalloc(inputSlicePitch * imageDesc.image_depth, 4096);

    auto counter = 0;
    char *data = (char *)host_ptr;
    auto depthToCopy = imageDesc.image_depth;

    while (depthToCopy--) {
        for (unsigned int i = 0; i < imageDesc.image_width * 4 * imageDesc.image_height; i++) {
            data[i] = counter++;
        }

        data += inputSlicePitch;
    }
    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    auto image = Image::create(context, flags, surfaceFormat,
                               &imageDesc, host_ptr, retVal);

    depthToCopy = imageDesc.image_depth;
    auto imageStorage = (uint8_t *)image->getCpuAddress();
    data = (char *)host_ptr;

    uint8_t *readMemory = nullptr;
    if (image->allowTiling()) {
        readMemory = new uint8_t[imageSize];
        size_t imgOrigin[] = {0, 0, 0};
        size_t imgRegion[] = {imageDesc.image_width, imageDesc.image_height, imageDesc.image_depth};
        retVal = pCmdQ->enqueueReadImage(image, CL_TRUE, imgOrigin, imgRegion, 0, computedSlicePitch, readMemory, 0, nullptr, nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);
        imageStorage = readMemory;
    }

    while (depthToCopy--) {
        for (unsigned int i = 0; i < imageDesc.image_width * 4 * imageDesc.image_height; i++) {
            if (image->allowTiling()) {
                AUBCommandStreamFixture::expectMemory<FamilyType>(&imageStorage[i], &data[i], 1);
            } else {
                EXPECT_EQ(imageStorage[i], data[i]);
            }
        }
        data += inputSlicePitch;
        imageStorage += computedSlicePitch;
    }

    delete image;
    alignedFree(host_ptr);
    if (readMemory) {
        delete readMemory;
    }
}
