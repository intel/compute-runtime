/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/aligned_memory.h"
#include "core/helpers/ptr_math.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/mem_obj/image.h"
#include "test.h"
#include "unit_tests/aub_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/mocks/mock_context.h"

using namespace NEO;

struct AUBImageUnaligned
    : public CommandEnqueueAUBFixture,
      public ::testing::Test {

    void SetUp() override {
        if (!(platformDevices[0]->capabilityTable.supportsImages)) {
            GTEST_SKIP();
        }
        CommandEnqueueAUBFixture::SetUp();
    }

    void TearDown() override {
        CommandEnqueueAUBFixture::TearDown();
    }

    template <typename FamilyType>
    void testReadImageUnaligned(size_t offset, size_t size, size_t pixelSize) {
        MockContext context(&pCmdQ->getDevice());

        char srcMemory[] = "_ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnoprstuwxyz";
        const auto bufferSize = sizeof(srcMemory) - 1;
        char *imageMemory = &srcMemory[1]; //ensure non cacheline-aligned hostPtr to create non-zerocopy image
        void *dstMemory = alignedMalloc(bufferSize, MemoryConstants::pageSize);
        memset(dstMemory, 0, bufferSize);
        char referenceMemory[bufferSize] = {0};

        const size_t testWidth = bufferSize / 4 / pixelSize;
        const size_t testHeight = 4;
        const size_t testDepth = 1;

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

        cl_mem_flags flags = CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE;
        auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
        auto retVal = CL_INVALID_VALUE;

        auto image = std::unique_ptr<Image>(Image::create(
            &context,
            MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0, 0),
            flags,
            0,
            surfaceFormat,
            &imageDesc,
            imageMemory,
            retVal));
        ASSERT_NE(nullptr, image);
        EXPECT_FALSE(image->isMemObjZeroCopy());

        auto graphicsAllocation = createResidentAllocationAndStoreItInCsr(dstMemory, bufferSize);
        auto dstMemoryGPUPtr = reinterpret_cast<char *>(graphicsAllocation->getGpuAddress());

        const size_t origin[3] = {0, 1, 0};
        const size_t region[3] = {size, 1, 1};

        size_t inputRowPitch = testWidth;
        size_t inputSlicePitch = inputRowPitch * testHeight;

        retVal = pCmdQ->enqueueReadImage(
            image.get(),
            CL_FALSE,
            origin,
            region,
            inputRowPitch,
            inputSlicePitch,
            ptrOffset(dstMemory, offset),
            nullptr,
            0,
            nullptr,
            nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);

        retVal = pCmdQ->flush();
        EXPECT_EQ(CL_SUCCESS, retVal);

        AUBCommandStreamFixture::expectMemory<FamilyType>(dstMemoryGPUPtr, referenceMemory, offset);
        AUBCommandStreamFixture::expectMemory<FamilyType>(ptrOffset(dstMemoryGPUPtr, offset), &imageMemory[inputRowPitch * origin[1] * pixelSize], size * pixelSize);
        AUBCommandStreamFixture::expectMemory<FamilyType>(ptrOffset(dstMemoryGPUPtr, size * pixelSize + offset), referenceMemory, bufferSize - offset - size * pixelSize);
        pCmdQ->finish();
        alignedFree(dstMemory);
    }

    template <typename FamilyType>
    void testWriteImageUnaligned(size_t offset, size_t size, size_t pixelSize) {
        DebugManagerStateRestore restorer;
        DebugManager.flags.ForceLinearImages.set(true);
        MockContext context(&pCmdQ->getDevice());

        char srcMemory[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnoprstuwxyz";
        const auto bufferSize = sizeof(srcMemory);
        char dstMemory[bufferSize + 1] = {0};
        char *imageMemory = &dstMemory[1]; //ensure non cacheline-aligned hostPtr to create non-zerocopy image
        char referenceMemory[bufferSize] = {0};

        const size_t testWidth = bufferSize / 4 / pixelSize;
        const size_t testHeight = 4;
        const size_t testDepth = 1;

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

        cl_mem_flags flags = CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE;
        auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
        auto retVal = CL_INVALID_VALUE;

        auto image = std::unique_ptr<Image>(Image::create(
            &context,
            MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0, 0),
            flags,
            0,
            surfaceFormat,
            &imageDesc,
            imageMemory,
            retVal));
        ASSERT_NE(nullptr, image);
        EXPECT_FALSE(image->isMemObjZeroCopy());

        auto dstMemoryGPUPtr = reinterpret_cast<char *>(image->getGraphicsAllocation()->getGpuAddress());

        const size_t origin[3] = {0, 1, 0};    // write first row
        const size_t region[3] = {size, 1, 1}; // write only "size" number of pixels

        size_t inputRowPitch = testWidth;
        size_t inputSlicePitch = inputRowPitch * testHeight;

        retVal = pCmdQ->enqueueWriteImage(
            image.get(),
            CL_TRUE,
            origin,
            region,
            inputRowPitch,
            inputSlicePitch,
            ptrOffset(srcMemory, offset),
            nullptr,
            0,
            nullptr,
            nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);

        pCmdQ->finish();

        auto imageRowPitch = image->getImageDesc().image_row_pitch;

        AUBCommandStreamFixture::expectMemory<FamilyType>(dstMemoryGPUPtr, referenceMemory, inputRowPitch * pixelSize);                                                       // validate zero row is not written
        AUBCommandStreamFixture::expectMemory<FamilyType>(ptrOffset(dstMemoryGPUPtr, imageRowPitch), &srcMemory[offset], size * pixelSize);                                   // validate first row is written,
        AUBCommandStreamFixture::expectMemory<FamilyType>(ptrOffset(dstMemoryGPUPtr, imageRowPitch + size * pixelSize), referenceMemory, (inputRowPitch - size) * pixelSize); // only size number of pixels, with correct data
        for (uint32_t row = 2; row < testHeight; row++) {
            AUBCommandStreamFixture::expectMemory<FamilyType>(ptrOffset(dstMemoryGPUPtr, row * imageRowPitch), referenceMemory, inputRowPitch * pixelSize); // next image rows shouldn;t be modified
        }
    }
};
