/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/aub_tests/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using namespace NEO;

struct AUBImageUnaligned
    : public CommandEnqueueAUBFixture,
      public ::testing::Test {

    void SetUp() override {
        if (!(defaultHwInfo->capabilityTable.supportsImages)) {
            GTEST_SKIP();
        }
        CommandEnqueueAUBFixture::SetUp();
    }

    void TearDown() override {
        CommandEnqueueAUBFixture::TearDown();
    }

    template <typename FamilyType>
    void testReadImageUnaligned(size_t offset, size_t size, size_t pixelSize) {
        MockContext context(pCmdQ->getDevice().getSpecializedDevice<ClDevice>());

        const size_t testWidth = 14 / pixelSize;
        const size_t testHeight = 4;
        const size_t testDepth = 1;
        auto numPixels = testWidth * testHeight * testDepth;

        cl_image_format imageFormat;
        cl_image_desc imageDesc;
        imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
        switch (pixelSize) {
        case 1:
            imageFormat.image_channel_order = CL_R;
            break;
        case 2:
            imageFormat.image_channel_order = CL_RG;
            break;
        case 3:
            ASSERT_TRUE(false);
            break;
        case 4:
            imageFormat.image_channel_order = CL_RGBA;
            break;
        }
        imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
        imageDesc.image_width = testWidth;
        imageDesc.image_height = testHeight;
        imageDesc.image_depth = testDepth;
        imageDesc.image_array_size = 1;
        imageDesc.image_row_pitch = 0;
        imageDesc.image_slice_pitch = 0;
        imageDesc.num_mip_levels = 0;
        imageDesc.num_samples = 0;
        imageDesc.mem_object = NULL;

        auto dstMemoryAligned = alignedMalloc(pixelSize * numPixels, MemoryConstants::cacheLineSize);
        memset(dstMemoryAligned, 0x0, pixelSize * numPixels);

        auto srcMemoryAligned = alignedMalloc(4 + pixelSize * numPixels, 4);
        auto srcMemoryUnaligned = reinterpret_cast<uint8_t *>(ptrOffset(srcMemoryAligned, 4));
        for (auto i = 0u; i < pixelSize * numPixels; ++i) {
            srcMemoryUnaligned[i] = static_cast<uint8_t>(i);
        }

        cl_mem_flags flags = CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE;
        auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, pClDevice->getHardwareInfo().capabilityTable.supportsOcl21Features);
        auto retVal = CL_INVALID_VALUE;
        auto image = std::unique_ptr<Image>(Image::create(
            &context,
            ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
            flags,
            0,
            surfaceFormat,
            &imageDesc,
            srcMemoryUnaligned,
            retVal));
        ASSERT_NE(nullptr, image);
        EXPECT_FALSE(image->isMemObjZeroCopy());

        auto graphicsAllocation = createResidentAllocationAndStoreItInCsr(dstMemoryAligned, pixelSize * numPixels);
        auto dstMemoryGPUPtr = reinterpret_cast<uint8_t *>(graphicsAllocation->getGpuAddress());

        const size_t origin[3] = {0, 1, 0};
        const size_t region[3] = {size, 1, 1};
        size_t inputRowPitch = testWidth * pixelSize;
        size_t inputSlicePitch = inputRowPitch * testHeight;
        retVal = pCmdQ->enqueueReadImage(
            image.get(),
            CL_FALSE,
            origin,
            region,
            inputRowPitch,
            inputSlicePitch,
            ptrOffset(dstMemoryAligned, offset),
            nullptr,
            0,
            nullptr,
            nullptr);

        EXPECT_EQ(CL_SUCCESS, retVal);
        pCmdQ->finish();

        std::vector<uint8_t> referenceMemory(pixelSize * numPixels, 0x0);

        AUBCommandStreamFixture::expectMemory<FamilyType>(dstMemoryGPUPtr, referenceMemory.data(), offset);
        AUBCommandStreamFixture::expectMemory<FamilyType>(ptrOffset(dstMemoryGPUPtr, offset), &srcMemoryUnaligned[inputRowPitch * origin[1]], size * pixelSize);
        AUBCommandStreamFixture::expectMemory<FamilyType>(ptrOffset(dstMemoryGPUPtr, offset + size * pixelSize), referenceMemory.data(), pixelSize * numPixels - offset - size * pixelSize);

        alignedFree(dstMemoryAligned);
        alignedFree(srcMemoryAligned);
    }

    template <typename FamilyType>
    void testWriteImageUnaligned(size_t offset, size_t size, size_t pixelSize) {
        DebugManagerStateRestore restorer;
        DebugManager.flags.ForceLinearImages.set(true);
        MockContext context(pCmdQ->getDevice().getSpecializedDevice<ClDevice>());

        const size_t testWidth = 14 / pixelSize;
        const size_t testHeight = 4;
        const size_t testDepth = 1;
        auto numPixels = testWidth * testHeight * testDepth;

        cl_image_format imageFormat;
        cl_image_desc imageDesc;
        imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
        switch (pixelSize) {
        case 1:
            imageFormat.image_channel_order = CL_R;
            break;
        case 2:
            imageFormat.image_channel_order = CL_RG;
            break;
        case 3:
            ASSERT_TRUE(false);
            break;
        case 4:
            imageFormat.image_channel_order = CL_RGBA;
            break;
        }
        imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
        imageDesc.image_width = testWidth;
        imageDesc.image_height = testHeight;
        imageDesc.image_depth = testDepth;
        imageDesc.image_array_size = 1;
        imageDesc.image_row_pitch = 0;
        imageDesc.image_slice_pitch = 0;
        imageDesc.num_mip_levels = 0;
        imageDesc.num_samples = 0;
        imageDesc.mem_object = NULL;

        auto srcMemoryAligned = alignedMalloc(4 + pixelSize * numPixels, MemoryConstants::cacheLineSize);
        memset(srcMemoryAligned, 0x0, 4 + pixelSize * numPixels);
        auto srcMemoryUnaligned = ptrOffset(reinterpret_cast<uint8_t *>(srcMemoryAligned), 4); //ensure non cacheline-aligned (but aligned to 4) hostPtr to create non-zerocopy image

        cl_mem_flags flags = CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE;
        auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, pClDevice->getHardwareInfo().capabilityTable.supportsOcl21Features);
        auto retVal = CL_INVALID_VALUE;

        auto image = std::unique_ptr<Image>(Image::create(
            &context,
            ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
            flags,
            0,
            surfaceFormat,
            &imageDesc,
            srcMemoryUnaligned,
            retVal));
        ASSERT_NE(nullptr, image);
        EXPECT_FALSE(image->isMemObjZeroCopy());

        for (auto i = 0u; i < pixelSize * numPixels; ++i) {
            srcMemoryUnaligned[i] = static_cast<uint8_t>(i);
        }

        auto dstMemoryGPUPtr = reinterpret_cast<uint8_t *>(image->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex())->getGpuAddress());

        const size_t origin[3] = {0, 1, 0};    // write first row
        const size_t region[3] = {size, 1, 1}; // write only "size" number of pixels
        size_t inputRowPitch = testWidth * pixelSize;
        size_t inputSlicePitch = inputRowPitch * testHeight;
        retVal = pCmdQ->enqueueWriteImage(
            image.get(),
            CL_TRUE,
            origin,
            region,
            inputRowPitch,
            inputSlicePitch,
            ptrOffset(srcMemoryUnaligned, offset),
            nullptr,
            0,
            nullptr,
            nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);

        pCmdQ->finish();
        EXPECT_EQ(CL_SUCCESS, retVal);

        auto imageRowPitch = image->getImageDesc().image_row_pitch;
        std::vector<uint8_t> referenceMemory(inputRowPitch * pixelSize, 0x0);

        AUBCommandStreamFixture::expectMemory<FamilyType>(dstMemoryGPUPtr, referenceMemory.data(), inputRowPitch);                                                                 // validate zero row is not written
        AUBCommandStreamFixture::expectMemory<FamilyType>(ptrOffset(dstMemoryGPUPtr, imageRowPitch), &srcMemoryUnaligned[offset], size * pixelSize);                               // validate first row is written with correct data
        AUBCommandStreamFixture::expectMemory<FamilyType>(ptrOffset(dstMemoryGPUPtr, imageRowPitch + size * pixelSize), referenceMemory.data(), inputRowPitch - size * pixelSize); // validate the remaining bytes of first row are not written
        for (uint32_t row = 2; row < testHeight; row++) {
            AUBCommandStreamFixture::expectMemory<FamilyType>(ptrOffset(dstMemoryGPUPtr, row * imageRowPitch), referenceMemory.data(), inputRowPitch); // validate the remaining rows are not written
        }

        alignedFree(srcMemoryAligned);
    }
};
