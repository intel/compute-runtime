/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/image/image_surface_state.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/migration_sync_data.h"
#include "shared/source/memory_manager/multi_graphics_allocation.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/fixtures/memory_management_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/kernel_binary_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/extensions/public/cl_ext_private.h"
#include "opencl/source/helpers/mipmap.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mem_obj/image_compression_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_image.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/mocks/ult_cl_device_factory_with_platform.h"

#include "CL/cl.h"
#include "memory_properties_flags.h"

namespace NEO {
template <typename GfxFamily>
class RenderDispatcher;
} // namespace NEO

using namespace NEO;

static const unsigned int testImageDimensions = 45;
auto channelType = CL_UNORM_INT8;
auto channelOrder = CL_RGBA;
auto const elementSize = 4; // sizeof CL_RGBA * CL_UNORM_INT8

class CreateImageTest : public ClDeviceFixture,
                        public testing::TestWithParam<uint64_t /*cl_mem_flags*/>,
                        public CommandQueueHwFixture {
    typedef CommandQueueHwFixture CommandQueueFixture;

  public:
    CreateImageTest() {
    }
    Image *createImageWithFlags(cl_mem_flags flags) {
        return createImageWithFlags(flags, context);
    }
    Image *createImageWithFlags(cl_mem_flags flags, Context *context) {
        auto surfaceFormat = Image::getSurfaceFormatFromTable(
            flags, &imageFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
        return Image::create(context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getDevice(0)->getDevice()),
                             flags, 0, surfaceFormat, &imageDesc, nullptr, retVal);
    }

  protected:
    void SetUp() override {
        ClDeviceFixture::setUp();

        CommandQueueFixture::setUp(pClDevice, 0);
        flags = GetParam();

        // clang-format off
        imageFormat.image_channel_data_type = channelType;
        imageFormat.image_channel_order     = channelOrder;

        imageDesc.image_type        = CL_MEM_OBJECT_IMAGE2D;
        imageDesc.image_width       = testImageDimensions;
        imageDesc.image_height      = testImageDimensions;
        imageDesc.image_depth       = 0;
        imageDesc.image_array_size  = 1;
        imageDesc.image_row_pitch   = 0;
        imageDesc.image_slice_pitch = 0;
        imageDesc.num_mip_levels    = 0;
        imageDesc.num_samples       = 0;
        imageDesc.mem_object = NULL;
        // clang-format on
    }

    void TearDown() override {
        CommandQueueFixture::tearDown();
        ClDeviceFixture::tearDown();
    }

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_int retVal = CL_SUCCESS;
    cl_mem_flags flags = 0;
    unsigned char pHostPtr[testImageDimensions * testImageDimensions * elementSize * 4];
};

typedef CreateImageTest CreateImageNoHostPtr;

TEST(TestSliceAndRowPitch, Given1dImageWithZeroRowPitchAndZeroSlicePitchWhenGettingHostPtrSlicePitchAndRowPitchThenCorrectValuesAreReturned) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ForceLinearImages.set(true);

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_int retVal;
    MockContext context;

    const size_t width = 5;
    const size_t height = 3;
    const size_t depth = 2;
    char *hostPtr = (char *)alignedMalloc(width * height * depth * elementSize * 2, 64);

    imageFormat.image_channel_data_type = channelType;
    imageFormat.image_channel_order = channelOrder;

    imageDesc.num_mip_levels = 0;
    imageDesc.num_samples = 0;
    imageDesc.mem_object = NULL;

    // 1D image with 0 row_pitch and 0 slice_pitch
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = width;
    imageDesc.image_height = 0;
    imageDesc.image_depth = 0;
    imageDesc.image_array_size = 0;
    imageDesc.image_row_pitch = 0;
    imageDesc.image_slice_pitch = 0;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto image = Image::create(
        &context,
        ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
        flags,
        0,
        surfaceFormat,
        &imageDesc,
        hostPtr,
        retVal);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ(width * elementSize, image->getHostPtrRowPitch());
    EXPECT_EQ(0u, image->getHostPtrSlicePitch());

    delete image;
    alignedFree(hostPtr);
}

TEST(TestSliceAndRowPitch, Given1dImageWithNonZeroRowPitchAndZeroSlicePitchWhenGettingHostPtrSlicePitchAndRowPitchThenCorrectValuesAreReturned) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ForceLinearImages.set(true);

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_int retVal;
    MockContext context;

    const size_t width = 5;
    const size_t height = 3;
    const size_t depth = 2;
    char *hostPtr = (char *)alignedMalloc(width * height * depth * elementSize * 2, 64);

    imageFormat.image_channel_data_type = channelType;
    imageFormat.image_channel_order = channelOrder;

    imageDesc.num_mip_levels = 0;
    imageDesc.num_samples = 0;
    imageDesc.mem_object = NULL;

    // 1D image with non-zero row_pitch and 0 slice_pitch
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = width;
    imageDesc.image_height = 0;
    imageDesc.image_depth = 0;
    imageDesc.image_array_size = 0;
    imageDesc.image_row_pitch = (width + 1) * elementSize;
    imageDesc.image_slice_pitch = 0;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto image = Image::create(
        &context,
        ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
        flags,
        0,
        surfaceFormat,
        &imageDesc,
        hostPtr,
        retVal);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ((width + 1) * elementSize, image->getHostPtrRowPitch());
    EXPECT_EQ(0u, image->getHostPtrSlicePitch());

    delete image;
    alignedFree(hostPtr);
}

TEST(TestSliceAndRowPitch, Given2dImageWithNonZeroRowPitchAndZeroSlicePitchWhenGettingHostPtrSlicePitchAndRowPitchThenCorrectValuesAreReturned) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ForceLinearImages.set(true);

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_int retVal;
    MockContext context;

    const size_t width = 5;
    const size_t height = 3;
    const size_t depth = 2;
    char *hostPtr = (char *)alignedMalloc(width * height * depth * elementSize * 2, 64);

    imageFormat.image_channel_data_type = channelType;
    imageFormat.image_channel_order = channelOrder;

    imageDesc.num_mip_levels = 0;
    imageDesc.num_samples = 0;
    imageDesc.mem_object = NULL;

    // 2D image with non-zero row_pitch and 0 slice_pitch
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = width;
    imageDesc.image_height = height;
    imageDesc.image_depth = 0;
    imageDesc.image_array_size = 0;
    imageDesc.image_row_pitch = (width + 1) * elementSize;
    imageDesc.image_slice_pitch = 0;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto image = Image::create(
        &context,
        ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
        flags,
        0,
        surfaceFormat,
        &imageDesc,
        hostPtr,
        retVal);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ((width + 1) * elementSize, image->getHostPtrRowPitch());
    EXPECT_EQ(0u, image->getHostPtrSlicePitch());

    delete image;
    alignedFree(hostPtr);
}

TEST(TestSliceAndRowPitch, Given1dArrayWithNonZeroRowPitchAndZeroSlicePitchWhenGettingHostPtrSlicePitchAndRowPitchThenCorrectValuesAreReturned) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ForceLinearImages.set(true);

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_int retVal;
    MockContext context;

    const size_t width = 5;
    const size_t height = 3;
    const size_t depth = 2;
    char *hostPtr = (char *)alignedMalloc(width * height * depth * elementSize * 2, 64);

    imageFormat.image_channel_data_type = channelType;
    imageFormat.image_channel_order = channelOrder;

    imageDesc.num_mip_levels = 0;
    imageDesc.num_samples = 0;
    imageDesc.mem_object = NULL;

    // 1D ARRAY image with non-zero row_pitch and 0 slice_pitch
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D_ARRAY;
    imageDesc.image_width = width;
    imageDesc.image_height = 0;
    imageDesc.image_depth = 0;
    imageDesc.image_array_size = 2;
    imageDesc.image_row_pitch = (width + 1) * elementSize;
    imageDesc.image_slice_pitch = 0;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto image = Image::create(
        &context,
        ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
        flags,
        0,
        surfaceFormat,
        &imageDesc,
        hostPtr,
        retVal);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ((width + 1) * elementSize, image->getHostPtrRowPitch());
    EXPECT_EQ((width + 1) * elementSize, image->getHostPtrSlicePitch());

    delete image;
    alignedFree(hostPtr);
}

TEST(TestSliceAndRowPitch, Given2dArrayWithNonZeroRowPitchAndZeroSlicePitchWhenGettingHostPtrSlicePitchAndRowPitchThenCorrectValuesAreReturned) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ForceLinearImages.set(true);

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_int retVal;
    MockContext context;

    const size_t width = 5;
    const size_t height = 3;
    const size_t depth = 2;
    char *hostPtr = (char *)alignedMalloc(width * height * depth * elementSize * 2, 64);

    imageFormat.image_channel_data_type = channelType;
    imageFormat.image_channel_order = channelOrder;

    imageDesc.num_mip_levels = 0;
    imageDesc.num_samples = 0;
    imageDesc.mem_object = NULL;

    // 2D ARRAY image with non-zero row_pitch and 0 slice_pitch
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D_ARRAY;
    imageDesc.image_width = width;
    imageDesc.image_height = height;
    imageDesc.image_depth = 0;
    imageDesc.image_array_size = 2;
    imageDesc.image_row_pitch = (width + 1) * elementSize;
    imageDesc.image_slice_pitch = 0;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto image = Image::create(
        &context,
        ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
        flags,
        0,
        surfaceFormat,
        &imageDesc,
        hostPtr,
        retVal);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ((width + 1) * elementSize, image->getHostPtrRowPitch());
    EXPECT_EQ((width + 1) * elementSize * height, image->getHostPtrSlicePitch());

    delete image;
    alignedFree(hostPtr);
}

TEST(TestSliceAndRowPitch, Given2dArrayWithZeroRowPitchAndNonZeroSlicePitchWhenGettingHostPtrSlicePitchAndRowPitchThenCorrectValuesAreReturned) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ForceLinearImages.set(true);

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_int retVal;
    MockContext context;

    const size_t width = 5;
    const size_t height = 3;
    const size_t depth = 2;
    char *hostPtr = (char *)alignedMalloc(width * height * depth * elementSize * 2, 64);

    imageFormat.image_channel_data_type = channelType;
    imageFormat.image_channel_order = channelOrder;

    imageDesc.num_mip_levels = 0;
    imageDesc.num_samples = 0;
    imageDesc.mem_object = NULL;

    // 2D ARRAY image with zero row_pitch and non-zero slice_pitch
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D_ARRAY;
    imageDesc.image_width = width;
    imageDesc.image_height = height;
    imageDesc.image_depth = 0;
    imageDesc.image_array_size = 2;
    imageDesc.image_row_pitch = 0;
    imageDesc.image_slice_pitch = (width + 1) * elementSize * height;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto image = Image::create(
        &context,
        ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
        flags,
        0,
        surfaceFormat,
        &imageDesc,
        hostPtr,
        retVal);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ(width * elementSize, image->getHostPtrRowPitch());
    EXPECT_EQ((width + 1) * elementSize * height, image->getHostPtrSlicePitch());

    delete image;
    alignedFree(hostPtr);
}

TEST(TestSliceAndRowPitch, Given2dArrayWithNonZeroRowPitchAndNonZeroSlicePitchWhenGettingHostPtrSlicePitchAndRowPitchThenCorrectValuesAreReturned) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ForceLinearImages.set(true);

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_int retVal;
    MockContext context;

    const size_t width = 5;
    const size_t height = 3;
    const size_t depth = 2;
    char *hostPtr = (char *)alignedMalloc(width * height * depth * elementSize * 2, 64);

    imageFormat.image_channel_data_type = channelType;
    imageFormat.image_channel_order = channelOrder;

    imageDesc.num_mip_levels = 0;
    imageDesc.num_samples = 0;
    imageDesc.mem_object = NULL;

    // 2D ARRAY image with non-zero row_pitch and non-zero slice_pitch
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D_ARRAY;
    imageDesc.image_width = width;
    imageDesc.image_height = height;
    imageDesc.image_depth = 0;
    imageDesc.image_array_size = 2;
    imageDesc.image_row_pitch = (width + 1) * elementSize;
    imageDesc.image_slice_pitch = (width + 1) * elementSize * height;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto image = Image::create(
        &context,
        ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
        flags,
        0,
        surfaceFormat,
        &imageDesc,
        hostPtr,
        retVal);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ((width + 1) * elementSize, image->getHostPtrRowPitch());
    EXPECT_EQ((width + 1) * elementSize * height, image->getHostPtrSlicePitch());

    delete image;
    alignedFree(hostPtr);
}

TEST(TestSliceAndRowPitch, Given2dArrayWithNonZeroRowPitchAndNonZeroSlicePitchGreaterThanRowPitchTimesHeightWhenGettingHostPtrSlicePitchAndRowPitchThenCorrectValuesAreReturned) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ForceLinearImages.set(true);

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_int retVal;
    MockContext context;

    const size_t width = 5;
    const size_t height = 3;
    const size_t depth = 2;
    char *hostPtr = (char *)alignedMalloc(width * height * depth * elementSize * 2, 64);

    imageFormat.image_channel_data_type = channelType;
    imageFormat.image_channel_order = channelOrder;

    imageDesc.num_mip_levels = 0;
    imageDesc.num_samples = 0;
    imageDesc.mem_object = NULL;

    // 2D ARRAY image with non-zero row_pitch and non-zero slice_pitch > row_pitch * height
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D_ARRAY;
    imageDesc.image_width = width;
    imageDesc.image_height = height;
    imageDesc.image_depth = 0;
    imageDesc.image_array_size = 2;
    imageDesc.image_row_pitch = (width + 1) * elementSize;
    imageDesc.image_slice_pitch = (width + 1) * elementSize * (height + 1);

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto image = Image::create(
        &context,
        ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
        flags,
        0,
        surfaceFormat,
        &imageDesc,
        hostPtr,
        retVal);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ((width + 1) * elementSize, image->getHostPtrRowPitch());
    EXPECT_EQ((width + 1) * elementSize * (height + 1), image->getHostPtrSlicePitch());

    delete image;
    alignedFree(hostPtr);
}

TEST(TestCreateImageUseHostPtr, GivenDifferenHostPtrAlignmentsWhenCheckingMemoryALignmentThenCorrectValueIsReturned) {
    KernelBinaryHelper kbHelper(KernelBinaryHelper::BUILT_INS_WITH_IMAGES);

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_int retVal;
    MockContext context;

    const size_t width = 4;
    const size_t height = 32;

    imageFormat.image_channel_data_type = channelType;
    imageFormat.image_channel_order = channelOrder;

    imageDesc.num_mip_levels = 0;
    imageDesc.num_samples = 0;
    imageDesc.mem_object = NULL;

    // 2D image with 0 row_pitch and 0 slice_pitch
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = width;
    imageDesc.image_height = height;
    imageDesc.image_depth = 0;
    imageDesc.image_array_size = 0;
    imageDesc.image_row_pitch = alignUp(alignUp(width, 4) * 4, 0x80); // row pitch for tiled img
    imageDesc.image_slice_pitch = 0;

    void *pageAlignedPointer = alignedMalloc(imageDesc.image_row_pitch * height * 1 * 4 + 256, 4096);
    void *hostPtr[] = {ptrOffset(pageAlignedPointer, 16),   // 16 - byte alignment
                       ptrOffset(pageAlignedPointer, 32),   // 32 - byte alignment
                       ptrOffset(pageAlignedPointer, 64),   // 64 - byte alignment
                       ptrOffset(pageAlignedPointer, 128)}; // 128 - byte alignment

    bool result[] = {false,
                     false,
                     true,
                     true};

    cl_mem_flags flags = CL_MEM_HOST_NO_ACCESS | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    for (int i = 0; i < 4; i++) {
        auto image = Image::create(
            &context,
            ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
            flags,
            0,
            surfaceFormat,
            &imageDesc,
            hostPtr[i],
            retVal);
        ASSERT_NE(nullptr, image);

        auto address = image->getCpuAddress();
        if (result[i] && image->isMemObjZeroCopy()) {
            EXPECT_EQ(hostPtr[i], address);
        } else {
            EXPECT_NE(hostPtr[i], address);
        }
        delete image;
    }
    alignedFree(pageAlignedPointer);
}

TEST(TestCreateImageUseHostPtr, givenZeroCopyImageValuesWhenUsingHostPtrThenZeroCopyImageIsCreated) {
    cl_int retVal = CL_SUCCESS;
    MockContext context;
    cl_image_desc imageDesc = {};
    imageDesc.image_width = 4096;
    imageDesc.image_height = 1;
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;

    cl_image_format imageFormat = {};
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto hostPtr = alignedMalloc(imageDesc.image_width * surfaceFormat->surfaceFormat.imageElementSizeInBytes,
                                 MemoryConstants::cacheLineSize);

    auto image = std::unique_ptr<Image>(Image::create(
        &context,
        ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
        flags,
        0,
        surfaceFormat,
        &imageDesc,
        hostPtr,
        retVal));

    auto allocation = image->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex());
    EXPECT_NE(nullptr, image);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(image->isMemObjZeroCopy());
    EXPECT_EQ(hostPtr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(nullptr, image->getMapAllocation(context.getDevice(0)->getRootDeviceIndex()));

    alignedFree(hostPtr);
}

TEST_P(CreateImageNoHostPtr, GivenMissingPitchWhenImageIsCreatedThenConstructorFillsMissingData) {
    auto image = createImageWithFlags(flags);

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, image);

    const auto &imageDesc = image->getImageDesc();

    // Sometimes the user doesn't pass image_row/slice_pitch during a create.
    // Ensure the driver fills in the missing data.
    EXPECT_NE(0u, imageDesc.image_row_pitch);
    EXPECT_GE(imageDesc.image_slice_pitch, imageDesc.image_row_pitch);
    delete image;
}

TEST_P(CreateImageNoHostPtr, whenImageIsCreatedThenItHasProperAccessAndCacheProperties) {
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(2);
    auto context = std::make_unique<MockContext>();
    auto image = createImageWithFlags(flags, context.get());
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, image);

    auto allocation = image->getGraphicsAllocation(context->getDevice(0)->getRootDeviceIndex());
    EXPECT_TRUE(allocation->getAllocationType() == AllocationType::image);

    auto isImageWritable = !(flags & (CL_MEM_READ_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS));
    EXPECT_EQ(isImageWritable, allocation->isMemObjectsAllocationWithWritableFlags());

    auto isReadOnly = isValueSet(flags, CL_MEM_READ_ONLY);
    EXPECT_NE(isReadOnly, allocation->isFlushL3Required());
    delete image;
}

// Parameterized test that tests image creation with all flags that should be
// valid with a nullptr host ptr
static cl_mem_flags noHostPtrFlags[] = {
    CL_MEM_READ_WRITE,
    CL_MEM_WRITE_ONLY,
    CL_MEM_READ_ONLY,
    CL_MEM_HOST_READ_ONLY,
    CL_MEM_HOST_WRITE_ONLY,
    CL_MEM_HOST_NO_ACCESS};

INSTANTIATE_TEST_SUITE_P(
    CreateImageTest_Create,
    CreateImageNoHostPtr,
    testing::ValuesIn(noHostPtrFlags));

struct CreateImageHostPtr
    : public CreateImageTest,
      public MemoryManagementFixture {
    typedef CreateImageTest BaseClass;

    CreateImageHostPtr() {
    }

    void SetUp() override {
        MemoryManagementFixture::setUp();
        BaseClass::SetUp();
    }

    void TearDown() override {
        delete image;
        BaseClass::TearDown();
        platformsImpl->clear();
        MemoryManagementFixture::tearDown();
    }

    Image *createImage(cl_int &retVal) {
        auto surfaceFormat = Image::getSurfaceFormatFromTable(
            flags, &imageFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
        return Image::create(
            context,
            ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getDevice(0)->getDevice()),
            flags,
            0,
            surfaceFormat,
            &imageDesc,
            pHostPtr,
            retVal);
    }

    cl_int retVal = CL_INVALID_VALUE;
    Image *image = nullptr;
};

TEST_P(CreateImageHostPtr, WhenImageIsCreatedThenResidencyIsFalse) {
    image = createImage(retVal);
    ASSERT_NE(nullptr, image);

    auto allocation = image->getGraphicsAllocation(context->getDevice(0)->getRootDeviceIndex());
    EXPECT_FALSE(allocation->isResident(pDevice->getDefaultEngine().osContext->getContextId()));
}

TEST_P(CreateImageHostPtr, WhenCheckingAddressThenAlllocationDependsOnSizeRelativeToPage) {
    image = createImage(retVal);
    auto allocation = image->getGraphicsAllocation(context->getDevice(0)->getRootDeviceIndex());
    ASSERT_NE(nullptr, image);

    auto address = image->getBasePtrForMap(0);
    EXPECT_NE(nullptr, address);

    if (!(flags & CL_MEM_USE_HOST_PTR)) {
        EXPECT_EQ(nullptr, image->getHostPtr());
    }

    if (flags & CL_MEM_USE_HOST_PTR) {
        // if size fits within a page then zero copy can be applied, if not RT needs to do a copy of image
        auto computedSize = imageDesc.image_width * elementSize * alignUp(imageDesc.image_height, 4) * imageDesc.image_array_size;
        auto ptrSize = imageDesc.image_width * elementSize * imageDesc.image_height * imageDesc.image_array_size;
        auto alignedRequiredSize = alignSizeWholePage(static_cast<void *>(pHostPtr), computedSize);
        auto alignedPtrSize = alignSizeWholePage(static_cast<void *>(pHostPtr), ptrSize);

        size_t halignReq = imageDesc.image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY ? 64 : 1;
        auto rowPitch = imageDesc.image_width * elementSize;
        auto slicePitch = rowPitch * imageDesc.image_height;
        auto requiredRowPitch = alignUp(imageDesc.image_width, halignReq) * elementSize;
        auto requiredSlicePitch = requiredRowPitch * alignUp(imageDesc.image_height, 4);

        bool copyRequired = (alignedRequiredSize > alignedPtrSize) | (requiredRowPitch != rowPitch) | (slicePitch != requiredSlicePitch);

        EXPECT_EQ(pHostPtr, address);
        EXPECT_EQ(pHostPtr, image->getHostPtr());

        if (copyRequired) {
            EXPECT_FALSE(image->isMemObjZeroCopy());
        }
    } else {
        EXPECT_NE(pHostPtr, address);
    }

    if (flags & CL_MEM_COPY_HOST_PTR && image->isMemObjZeroCopy()) {
        // Buffer should contain a copy of host memory
        EXPECT_EQ(0, memcmp(pHostPtr, allocation->getUnderlyingBuffer(), sizeof(testImageDimensions)));
    }
}

TEST_P(CreateImageHostPtr, WhenGettingImageDescThenCorrectValuesAreReturned) {
    image = createImage(retVal);
    ASSERT_NE(nullptr, image);
    auto allocation = image->getGraphicsAllocation(context->getDevice(0)->getRootDeviceIndex());

    const auto &imageDesc = image->getImageDesc();
    // clang-format off
    EXPECT_EQ(this->imageDesc.image_type,        imageDesc.image_type);
    EXPECT_EQ(this->imageDesc.image_width,       imageDesc.image_width);
    EXPECT_EQ(this->imageDesc.image_height,      imageDesc.image_height);
    EXPECT_EQ(this->imageDesc.image_depth,       imageDesc.image_depth);
    EXPECT_EQ(0u,                                imageDesc.image_array_size);
    EXPECT_NE(0u,                                imageDesc.image_row_pitch);
    EXPECT_GE(imageDesc.image_slice_pitch,       imageDesc.image_row_pitch);
    EXPECT_EQ(this->imageDesc.num_mip_levels,    imageDesc.num_mip_levels);
    EXPECT_EQ(this->imageDesc.num_samples,       imageDesc.num_samples);
    EXPECT_EQ(this->imageDesc.buffer,            imageDesc.buffer);
    EXPECT_EQ(this->imageDesc.mem_object,        imageDesc.mem_object);
    // clang-format on

    EXPECT_EQ(image->getHostPtrRowPitch(), static_cast<size_t>(imageDesc.image_width * elementSize));
    // Only 3D, and array images can have slice pitch
    int isArrayOr3DType = 0;
    if (this->imageDesc.image_type == CL_MEM_OBJECT_IMAGE3D ||
        this->imageDesc.image_type == CL_MEM_OBJECT_IMAGE2D_ARRAY ||
        this->imageDesc.image_type == CL_MEM_OBJECT_IMAGE2D_ARRAY) {
        isArrayOr3DType = 1;
    }

    EXPECT_EQ(image->getHostPtrSlicePitch(), static_cast<size_t>(imageDesc.image_width * elementSize * imageDesc.image_height) * isArrayOr3DType);
    EXPECT_EQ(image->getImageCount(), 1u);
    EXPECT_NE(0u, image->getSize());
    EXPECT_FALSE(image->getIsDisplayable());
    EXPECT_NE(nullptr, allocation);
}

TEST_P(CreateImageHostPtr, GivenFailedAllocationInjectionWhenCheckingAllocationThenOnlyFailedAllocationReturnsNull) {
    InjectedFunction method = [this](size_t failureIndex) {
        // System under test
        image = createImage(retVal);

        if (MemoryManagement::nonfailingAllocation == failureIndex) {
            EXPECT_EQ(CL_SUCCESS, retVal);
            EXPECT_NE(nullptr, image);
        } else {
            EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal) << "for allocation " << failureIndex;
            EXPECT_EQ(nullptr, image);
        }

        delete image;
        image = nullptr;
    };
    injectFailures(method, 4); // check only first 5 allocations - avoid checks on writeImg call allocations for tiled imgs
}

TEST_P(CreateImageHostPtr, givenLinearImageWhenFailedAtCreationThenReturnError) {
    DebugManagerStateRestore restore;
    debugManager.flags.ForceLinearImages.set(true);

    InjectedFunction method = [this](size_t failureIndex) {
        // System under test
        image = createImage(retVal);

        if (MemoryManagement::nonfailingAllocation == failureIndex) {
            EXPECT_EQ(CL_SUCCESS, retVal);
            EXPECT_NE(nullptr, image);
        } else {
            EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal) << "for allocation " << failureIndex;
            EXPECT_EQ(nullptr, image);
        }

        delete image;
        image = nullptr;
    };
    injectFailures(method, 4); // check only first 5 allocations - avoid checks on writeImg call allocations for tiled imgs
}

TEST_P(CreateImageHostPtr, WhenWritingOutsideAllocatedMemoryWhileCreatingImageThenWriteIsHandledCorrectly) {
    pClDevice->getPlatform()->getSVMAllocsManager()->cleanupUSMAllocCaches();
    auto mockMemoryManager = new MockMemoryManager(*pDevice->executionEnvironment);
    pDevice->injectMemoryManager(mockMemoryManager);
    context->memoryManager = mockMemoryManager;
    mockMemoryManager->redundancyRatio = 2;
    memset(pHostPtr, 1, testImageDimensions * testImageDimensions * elementSize * 4);
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D_ARRAY;
    imageDesc.image_height = 1;
    imageDesc.image_row_pitch = elementSize * imageDesc.image_width + 1;
    image = createImage(retVal);
    auto allocation = image->getGraphicsAllocation(pDevice->getRootDeviceIndex());

    char *memory = reinterpret_cast<char *>(allocation->getUnderlyingBuffer());
    auto memorySize = allocation->getUnderlyingBufferSize() / 2;
    for (size_t i = 0; i < image->getHostPtrSlicePitch(); ++i) {
        if (i < imageDesc.image_width * elementSize) {
            EXPECT_EQ(1, memory[i]);
        } else {
            EXPECT_EQ(0, memory[i]);
        }
    }
    for (size_t i = 0; i < memorySize; ++i) {
        EXPECT_EQ(0, memory[memorySize + i]);
    }

    mockMemoryManager->redundancyRatio = 1;
}

struct ModifyableImage {
    enum { flags = 0 };
    static cl_image_format imageFormat;
    static cl_image_desc imageDesc;
    static void *hostPtr;
    static NEO::Context *context;
};

void *ModifyableImage::hostPtr = nullptr;
NEO::Context *ModifyableImage::context = nullptr;
cl_image_format ModifyableImage::imageFormat;
cl_image_desc ModifyableImage::imageDesc;

class ImageTransfer : public ::testing::Test {
  public:
    void SetUp() override {
        context = new MockContext();
        ASSERT_NE(context, nullptr);
        ModifyableImage::context = context;
        ModifyableImage::hostPtr = nullptr;
        ModifyableImage::imageFormat = {CL_R, CL_FLOAT};
        ModifyableImage::imageDesc = {CL_MEM_OBJECT_IMAGE1D, 512, 0, 0, 0, 0, 0, 0, 0, {nullptr}};
        hostPtr = nullptr;
        unalignedHostPtr = nullptr;
    }

    void TearDown() override {
        if (context)
            delete context;
        if (hostPtr)
            alignedFree(hostPtr);
    }

    void createHostPtrs(size_t imageSize) {
        hostPtr = alignedMalloc(imageSize + 100, 4096);
        unalignedHostPtr = (char *)hostPtr + 4;
        memset(hostPtr, 0, imageSize + 100);
        memset(unalignedHostPtr, 1, imageSize);
    }

    MockContext *context;
    void *hostPtr;
    void *unalignedHostPtr;
};

TEST_F(ImageTransfer, GivenNonZeroCopyImageWhenDataTransferredFromHostPtrToMemStorageThenNoOverflowOfHostPtr) {
    size_t imageSize = 512 * 4;
    createHostPtrs(imageSize);

    ModifyableImage::imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    ModifyableImage::imageDesc.image_width = 512;
    ModifyableImage::imageDesc.image_height = 0;
    ModifyableImage::imageDesc.image_row_pitch = 0;
    ModifyableImage::imageDesc.image_array_size = 0;
    ModifyableImage::imageFormat.image_channel_order = CL_R;
    ModifyableImage::imageFormat.image_channel_data_type = CL_FLOAT;

    ModifyableImage::hostPtr = unalignedHostPtr;
    Image *imageNonZeroCopy = ImageHelperUlt<ImageUseHostPtr<ModifyableImage>>::create();

    ASSERT_NE(nullptr, imageNonZeroCopy);

    void *memoryStorage = imageNonZeroCopy->getCpuAddress();
    size_t memoryStorageSize = imageNonZeroCopy->getSize();

    ASSERT_NE(memoryStorage, unalignedHostPtr);

    int result = memcmp(memoryStorage, unalignedHostPtr, imageSize);
    EXPECT_EQ(0, result);

    memset(memoryStorage, 0, memoryStorageSize);
    memset((char *)unalignedHostPtr + imageSize, 2, 100 - 4);

    auto &imgDesc = imageNonZeroCopy->getImageDesc();
    MemObjOffsetArray copyOffset = {{0, 0, 0}};
    MemObjSizeArray copySize = {{imgDesc.image_width, imgDesc.image_height, imgDesc.image_depth}};
    imageNonZeroCopy->transferDataFromHostPtr(copySize, copyOffset);

    void *foundData = memchr(memoryStorage, 2, memoryStorageSize);
    EXPECT_EQ(0, foundData);
    delete imageNonZeroCopy;
}

TEST_F(ImageTransfer, GivenNonZeroCopyNonZeroRowPitchImageWhenDataIsTransferredFromHostPtrToMemStorageThenDestinationIsNotOverflowed) {
    ModifyableImage::imageDesc.image_width = 16;
    ModifyableImage::imageDesc.image_row_pitch = 65;
    ModifyableImage::imageFormat.image_channel_data_type = CL_UNORM_INT8;

    size_t imageSize = ModifyableImage::imageDesc.image_row_pitch;
    size_t imageWidth = ModifyableImage::imageDesc.image_width;

    createHostPtrs(imageSize);

    ModifyableImage::hostPtr = unalignedHostPtr;
    Image *imageNonZeroCopy = ImageHelperUlt<ImageUseHostPtr<ModifyableImage>>::create();

    ASSERT_NE(nullptr, imageNonZeroCopy);

    void *memoryStorage = imageNonZeroCopy->getCpuAddress();
    size_t memoryStorageSize = imageNonZeroCopy->getSize();

    ASSERT_NE(memoryStorage, unalignedHostPtr);

    int result = memcmp(memoryStorage, unalignedHostPtr, imageWidth);
    EXPECT_EQ(0, result);

    memset(memoryStorage, 0, memoryStorageSize);
    memset((char *)unalignedHostPtr + imageSize, 2, 100 - 4);

    auto &imgDesc = imageNonZeroCopy->getImageDesc();
    MemObjOffsetArray copyOffset = {{0, 0, 0}};
    MemObjSizeArray copySize = {{imgDesc.image_width, imgDesc.image_height, imgDesc.image_depth}};
    imageNonZeroCopy->transferDataFromHostPtr(copySize, copyOffset);

    void *foundData = memchr(memoryStorage, 2, memoryStorageSize);
    EXPECT_EQ(0, foundData);
    delete imageNonZeroCopy;
}

TEST_F(ImageTransfer, GivenNonZeroCopyNonZeroRowPitchWithExtraBytes1DArrayImageWhenDataIsTransferredForthAndBackThenDataValidates) {
    ModifyableImage::imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D_ARRAY;
    ModifyableImage::imageDesc.image_width = 5;
    ModifyableImage::imageDesc.image_row_pitch = 28; // == (4 * 5) row bytes + (4 * 2) extra bytes
    ModifyableImage::imageDesc.image_array_size = 3;
    ModifyableImage::imageFormat.image_channel_order = CL_RGBA;
    ModifyableImage::imageFormat.image_channel_data_type = CL_UNORM_INT8;

    const size_t imageWidth = ModifyableImage::imageDesc.image_width;
    const size_t imageRowPitchInPixels = ModifyableImage::imageDesc.image_row_pitch / 4;
    const size_t imageHeight = 1;
    const size_t imageCount = ModifyableImage::imageDesc.image_array_size;

    size_t imageSize = ModifyableImage::imageDesc.image_row_pitch * imageHeight * imageCount;

    createHostPtrs(imageSize);

    uint32_t *row = static_cast<uint32_t *>(unalignedHostPtr);

    for (uint32_t arrayIndex = 0; arrayIndex < imageCount; ++arrayIndex) {
        for (uint32_t pixelInRow = 0; pixelInRow < imageRowPitchInPixels; ++pixelInRow) {
            if (pixelInRow < imageWidth) {
                row[pixelInRow] = pixelInRow;
            } else {
                row[pixelInRow] = 66;
            }
        }
        row = row + imageRowPitchInPixels;
    }

    ModifyableImage::hostPtr = unalignedHostPtr;
    Image *imageNonZeroCopy = ImageHelperUlt<ImageUseHostPtr<ModifyableImage>>::create();

    ASSERT_NE(nullptr, imageNonZeroCopy);

    void *memoryStorage = imageNonZeroCopy->getCpuAddress();

    ASSERT_NE(memoryStorage, unalignedHostPtr);

    size_t internalSlicePitch = imageNonZeroCopy->getImageDesc().image_slice_pitch;

    // Check twice, once after image create, and second time after transfer from HostPtrToMemoryStorage
    // when these paths are unified, only one check will be enough
    for (size_t run = 0; run < 2; ++run) {

        row = static_cast<uint32_t *>(unalignedHostPtr);
        unsigned char *internalRow = static_cast<unsigned char *>(memoryStorage);

        if (run == 1) {
            auto &imgDesc = imageNonZeroCopy->getImageDesc();
            MemObjOffsetArray copyOffset = {{0, 0, 0}};
            MemObjSizeArray copySize = {{imgDesc.image_width, imgDesc.image_height, imgDesc.image_depth}};
            imageNonZeroCopy->transferDataFromHostPtr(copySize, copyOffset);
        }

        for (size_t arrayIndex = 0; arrayIndex < imageCount; ++arrayIndex) {
            for (size_t pixelInRow = 0; pixelInRow < imageRowPitchInPixels; ++pixelInRow) {
                if (pixelInRow < imageWidth) {
                    if (memcmp(&row[pixelInRow], &internalRow[pixelInRow * 4], 4)) {
                        EXPECT_FALSE(1) << "Data in memory storage did not validate, row: " << pixelInRow << " array: " << arrayIndex << "\n";
                    }
                } else {
                    // Change extra bytes pattern
                    row[pixelInRow] = 55;
                }
            }
            row = row + imageRowPitchInPixels;
            internalRow = internalRow + internalSlicePitch;
        }
    }

    auto &imgDesc = imageNonZeroCopy->getImageDesc();
    MemObjOffsetArray copyOffset = {{0, 0, 0}};
    MemObjSizeArray copySize = {{imgDesc.image_width, imgDesc.image_height, imgDesc.image_depth}};
    imageNonZeroCopy->transferDataToHostPtr(copySize, copyOffset);

    row = static_cast<uint32_t *>(unalignedHostPtr);

    for (size_t arrayIndex = 0; arrayIndex < imageCount; ++arrayIndex) {
        for (size_t pixelInRow = 0; pixelInRow < imageRowPitchInPixels; ++pixelInRow) {
            if (pixelInRow < imageWidth) {
                if (row[pixelInRow] != pixelInRow) {
                    EXPECT_FALSE(1) << "Data under host_ptr did not validate, row: " << pixelInRow << " array: " << arrayIndex << "\n";
                }
            } else if (row[pixelInRow] != 55) {
                EXPECT_FALSE(1) << "Data under host_ptr corrupted in extra bytes, row: " << pixelInRow << " array: " << arrayIndex << "\n";
            }
        }
        row = row + imageRowPitchInPixels;
    }

    delete imageNonZeroCopy;
}

// Parameterized test that tests image creation with all flags that should be
// valid with a valid host ptr
static cl_mem_flags validHostPtrFlags[] = {
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

INSTANTIATE_TEST_SUITE_P(
    CreateImageTest_Create,
    CreateImageHostPtr,
    testing::ValuesIn(validHostPtrFlags));

TEST(ImageGetSurfaceFormatInfoTest, givenNullptrFormatWhenGetSurfaceFormatInfoIsCalledThenReturnsNullptr) {
    MockContext context;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(0, nullptr, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    EXPECT_EQ(nullptr, surfaceFormat);
}

HWTEST_F(ImageCompressionTests, givenTiledImageWhenCreatingAllocationThenPreferCompression) {
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 5;
    imageDesc.image_height = 5;
    MockContext context;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);

    auto image = std::unique_ptr<Image>(Image::create(
        mockContext.get(), ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
        flags, 0, surfaceFormat, &imageDesc, nullptr, retVal));
    ASSERT_NE(nullptr, image);
    EXPECT_EQ(defaultHwInfo->capabilityTable.supportsImages, image->isTiledAllocation());
    EXPECT_TRUE(myMemoryManager->mockMethodCalled);

    auto compressionAllowed = !context.getDevice(0)->getProductHelper().isCompressionForbidden(*defaultHwInfo);
    if (compressionAllowed) {
        EXPECT_EQ(defaultHwInfo->capabilityTable.supportsImages, myMemoryManager->capturedPreferCompressed);
    } else {
        EXPECT_FALSE(myMemoryManager->capturedPreferCompressed);
    }
}

TEST_F(ImageCompressionTests, givenNonTiledImageWhenCreatingAllocationThenDontPreferCompression) {
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 5;
    MockContext context;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);

    auto image = std::unique_ptr<Image>(Image::create(
        mockContext.get(), ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
        flags, 0, surfaceFormat, &imageDesc, nullptr, retVal));
    ASSERT_NE(nullptr, image);
    EXPECT_FALSE(image->isTiledAllocation());
    EXPECT_TRUE(myMemoryManager->mockMethodCalled);
    EXPECT_FALSE(myMemoryManager->capturedPreferCompressed);
}

HWTEST_F(ImageCompressionTests, givenTiledImageAndVariousFlagsWhenCreatingAllocationThenCorrectlySetPreferCompression) {
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 5;
    imageDesc.image_height = 5;

    auto newFlags = flags | CL_MEM_COMPRESSED_HINT_INTEL;
    MockContext context;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        newFlags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto image = std::unique_ptr<Image>(Image::create(
        mockContext.get(), ClMemoryPropertiesHelper::createMemoryProperties(newFlags, 0, 0, &context.getDevice(0)->getDevice()),
        newFlags, 0, surfaceFormat, &imageDesc, nullptr, retVal));
    ASSERT_NE(nullptr, image);
    EXPECT_EQ(defaultHwInfo->capabilityTable.supportsImages, image->isTiledAllocation());
    EXPECT_TRUE(myMemoryManager->mockMethodCalled);

    auto compressionAllowed = !context.getDevice(0)->getProductHelper().isCompressionForbidden(*defaultHwInfo);
    if (compressionAllowed) {
        EXPECT_EQ(defaultHwInfo->capabilityTable.supportsImages, myMemoryManager->capturedPreferCompressed);
    } else {
        EXPECT_FALSE(myMemoryManager->capturedPreferCompressed);
    }

    newFlags = flags | CL_MEM_UNCOMPRESSED_HINT_INTEL;
    surfaceFormat = Image::getSurfaceFormatFromTable(
        newFlags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    image = std::unique_ptr<Image>(Image::create(
        mockContext.get(), ClMemoryPropertiesHelper::createMemoryProperties(newFlags, 0, 0, &context.getDevice(0)->getDevice()),
        newFlags, 0, surfaceFormat, &imageDesc, nullptr, retVal));
    ASSERT_NE(nullptr, image);
    EXPECT_EQ(defaultHwInfo->capabilityTable.supportsImages, image->isTiledAllocation());
    EXPECT_TRUE(myMemoryManager->mockMethodCalled);
    EXPECT_FALSE(myMemoryManager->capturedPreferCompressed);
}

TEST_F(ImageCompressionTests, givenNonTiledImageAndVariousFlagsWhenCreatingAllocationThenDontPreferCompression) {
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 5;

    auto newFlags = flags | CL_MEM_COMPRESSED_HINT_INTEL;
    MockContext context;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        newFlags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto image = std::unique_ptr<Image>(Image::create(
        mockContext.get(), ClMemoryPropertiesHelper::createMemoryProperties(newFlags, 0, 0, &context.getDevice(0)->getDevice()),
        newFlags, 0, surfaceFormat, &imageDesc, nullptr, retVal));
    ASSERT_NE(nullptr, image);
    EXPECT_FALSE(image->isTiledAllocation());
    EXPECT_TRUE(myMemoryManager->mockMethodCalled);
    EXPECT_FALSE(myMemoryManager->capturedPreferCompressed);

    newFlags = flags | CL_MEM_UNCOMPRESSED_HINT_INTEL;
    surfaceFormat = Image::getSurfaceFormatFromTable(
        newFlags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    image = std::unique_ptr<Image>(Image::create(
        mockContext.get(), ClMemoryPropertiesHelper::createMemoryProperties(newFlags, 0, 0, &context.getDevice(0)->getDevice()),
        newFlags, 0, surfaceFormat, &imageDesc, nullptr, retVal));
    ASSERT_NE(nullptr, image);
    EXPECT_FALSE(image->isTiledAllocation());
    EXPECT_TRUE(myMemoryManager->mockMethodCalled);
    EXPECT_FALSE(myMemoryManager->capturedPreferCompressed);
}

TEST(ImageTest, givenImageWhenGettingCompressionOfImageThenCorrectValueIsReturned) {
    MockContext context;
    std::unique_ptr<Image> image(ImageHelperUlt<Image3dDefaults>::create(&context));
    EXPECT_NE(nullptr, image);

    auto allocation = image->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex());
    allocation->getDefaultGmm()->setCompressionEnabled(true);
    size_t sizeReturned = 0;
    cl_bool usesCompression;
    cl_int retVal = CL_SUCCESS;
    retVal = image->getMemObjectInfo(
        CL_MEM_USES_COMPRESSION_INTEL,
        sizeof(cl_bool),
        &usesCompression,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(sizeof(cl_bool), sizeReturned);
    EXPECT_TRUE(usesCompression);

    allocation->getDefaultGmm()->setCompressionEnabled(false);
    sizeReturned = 0;
    usesCompression = cl_bool{CL_FALSE};
    retVal = image->getMemObjectInfo(
        CL_MEM_USES_COMPRESSION_INTEL,
        sizeof(cl_bool),
        &usesCompression,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(sizeof(cl_bool), sizeReturned);
    EXPECT_FALSE(usesCompression);
}

using ImageTests = ::testing::Test;
HWTEST_F(ImageTests, givenImageWhenAskedForPtrOffsetForGpuMappingThenReturnCorrectValue) {
    if (!defaultHwInfo->capabilityTable.supportsImages) {
        GTEST_SKIP();
    }
    MockContext ctx;
    std::unique_ptr<Image> image(ImageHelperUlt<Image3dDefaults>::create(&ctx));
    EXPECT_FALSE(image->mappingOnCpuAllowed());

    MemObjOffsetArray origin = {{4, 5, 6}};

    auto retOffset = image->calculateOffsetForMapping(origin);
    size_t expectedOffset = image->getSurfaceFormatInfo().surfaceFormat.imageElementSizeInBytes * origin[0] +
                            image->getHostPtrRowPitch() * origin[1] + image->getHostPtrSlicePitch() * origin[2];

    EXPECT_EQ(expectedOffset, retOffset);
}

TEST(ImageTest, givenImageWhenAskedForMcsInfoThenDefaultValuesAreReturned) {
    MockContext ctx;
    std::unique_ptr<Image> image(ImageHelperUlt<Image3dDefaults>::create(&ctx));

    auto mcsInfo = image->getMcsSurfaceInfo();
    EXPECT_EQ(0u, mcsInfo.multisampleCount);
    EXPECT_EQ(0u, mcsInfo.qPitch);
    EXPECT_EQ(0u, mcsInfo.pitch);
}

TEST(ImageTest, givenImageWhenAskedForPtrOffsetForCpuMappingThenReturnCorrectValue) {
    DebugManagerStateRestore restore;
    debugManager.flags.ForceLinearImages.set(true);
    MockContext ctx;
    std::unique_ptr<Image> image(ImageHelperUlt<Image3dDefaults>::create(&ctx));
    EXPECT_TRUE(image->mappingOnCpuAllowed());

    MemObjOffsetArray origin = {{4, 5, 6}};

    auto retOffset = image->calculateOffsetForMapping(origin);
    size_t expectedOffset = image->getSurfaceFormatInfo().surfaceFormat.imageElementSizeInBytes * origin[0] +
                            image->getImageDesc().image_row_pitch * origin[1] +
                            image->getImageDesc().image_slice_pitch * origin[2];

    EXPECT_EQ(expectedOffset, retOffset);
}

TEST(ImageTest, given1DArrayImageWhenAskedForPtrOffsetForMappingThenReturnCorrectValue) {
    MockContext ctx;
    std::unique_ptr<Image> image(ImageHelperUlt<Image1dArrayDefaults>::create(&ctx));

    MemObjOffsetArray origin = {{4, 5, 0}};

    auto retOffset = image->calculateOffsetForMapping(origin);
    size_t expectedOffset = image->getSurfaceFormatInfo().surfaceFormat.imageElementSizeInBytes * origin[0] +
                            image->getImageDesc().image_slice_pitch * origin[1];

    EXPECT_EQ(expectedOffset, retOffset);
}

HWTEST_F(ImageTests, givenImageWhenAskedForPtrLengthForGpuMappingThenReturnCorrectValue) {
    if (!defaultHwInfo->capabilityTable.supportsImages) {
        GTEST_SKIP();
    }
    MockContext ctx;
    std::unique_ptr<Image> image(ImageHelperUlt<Image3dDefaults>::create(&ctx));
    EXPECT_FALSE(image->mappingOnCpuAllowed());

    MemObjSizeArray region = {{4, 5, 6}};

    auto retLength = image->calculateMappedPtrLength(region);
    size_t expectedLength = image->getSurfaceFormatInfo().surfaceFormat.imageElementSizeInBytes * region[0] +
                            image->getHostPtrRowPitch() * region[1] + image->getHostPtrSlicePitch() * region[2];

    EXPECT_EQ(expectedLength, retLength);
}

TEST(ImageTest, givenImageWhenAskedForPtrLengthForCpuMappingThenReturnCorrectValue) {
    DebugManagerStateRestore restore;
    debugManager.flags.ForceLinearImages.set(true);
    MockContext ctx;
    std::unique_ptr<Image> image(ImageHelperUlt<Image3dDefaults>::create(&ctx));
    EXPECT_TRUE(image->mappingOnCpuAllowed());

    MemObjSizeArray region = {{4, 5, 6}};

    auto retLength = image->calculateMappedPtrLength(region);
    size_t expectedLength = image->getSurfaceFormatInfo().surfaceFormat.imageElementSizeInBytes * region[0] +
                            image->getImageDesc().image_row_pitch * region[1] +
                            image->getImageDesc().image_slice_pitch * region[2];

    EXPECT_EQ(expectedLength, retLength);
}

TEST(ImageTest, givenMipMapImage3DWhenAskedForPtrOffsetForGpuMappingThenReturnOffsetWithSlicePitch) {
    MockContext ctx;
    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE3D;
    imageDesc.image_width = 5;
    imageDesc.image_height = 5;
    imageDesc.image_depth = 5;
    imageDesc.num_mip_levels = 2;

    std::unique_ptr<Image> image(ImageHelperUlt<Image3dDefaults>::create(&ctx, &imageDesc));
    EXPECT_FALSE(image->mappingOnCpuAllowed());

    MemObjOffsetArray origin{{1, 1, 1}};

    auto retOffset = image->calculateOffsetForMapping(origin);
    size_t expectedOffset = image->getSurfaceFormatInfo().surfaceFormat.imageElementSizeInBytes * origin[0] +
                            image->getHostPtrRowPitch() * origin[1] + image->getHostPtrSlicePitch() * origin[2];

    EXPECT_EQ(expectedOffset, retOffset);
}

TEST(ImageTest, givenMipMapImage2DArrayWhenAskedForPtrOffsetForGpuMappingThenReturnOffsetWithSlicePitch) {
    MockContext ctx;
    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D_ARRAY;
    imageDesc.image_width = 5;
    imageDesc.image_height = 5;
    imageDesc.image_array_size = 5;
    imageDesc.num_mip_levels = 2;

    std::unique_ptr<Image> image(ImageHelperUlt<Image2dArrayDefaults>::create(&ctx, &imageDesc));
    EXPECT_FALSE(image->mappingOnCpuAllowed());

    MemObjOffsetArray origin{{1, 1, 1}};

    auto retOffset = image->calculateOffsetForMapping(origin);
    size_t expectedOffset = image->getSurfaceFormatInfo().surfaceFormat.imageElementSizeInBytes * origin[0] +
                            image->getHostPtrRowPitch() * origin[1] + image->getHostPtrSlicePitch() * origin[2];

    EXPECT_EQ(expectedOffset, retOffset);
}

TEST(ImageTest, givenNonMipMapImage2DArrayWhenAskedForPtrOffsetForGpuMappingThenReturnOffsetWithSlicePitch) {
    MockContext ctx;
    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D_ARRAY;
    imageDesc.image_width = 5;
    imageDesc.image_height = 5;
    imageDesc.image_array_size = 5;
    imageDesc.num_mip_levels = 1;

    std::unique_ptr<Image> image(ImageHelperUlt<Image2dArrayDefaults>::create(&ctx, &imageDesc));
    EXPECT_FALSE(image->mappingOnCpuAllowed());

    MemObjOffsetArray origin{{1, 1, 1}};

    auto retOffset = image->calculateOffsetForMapping(origin);
    size_t expectedOffset = image->getSurfaceFormatInfo().surfaceFormat.imageElementSizeInBytes * origin[0] +
                            image->getHostPtrRowPitch() * origin[1] + image->getHostPtrSlicePitch() * origin[2];

    EXPECT_EQ(expectedOffset, retOffset);
}

TEST(ImageTest, givenMipMapImage1DArrayWhenAskedForPtrOffsetForGpuMappingThenReturnOffsetWithSlicePitch) {
    MockContext ctx;
    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D_ARRAY;
    imageDesc.image_width = 5;
    imageDesc.image_array_size = 5;
    imageDesc.num_mip_levels = 2;

    std::unique_ptr<Image> image(ImageHelperUlt<Image1dArrayDefaults>::create(&ctx, &imageDesc));
    EXPECT_FALSE(image->mappingOnCpuAllowed());

    MemObjOffsetArray origin{{1, 1, 0}};

    auto retOffset = image->calculateOffsetForMapping(origin);
    size_t expectedOffset = image->getSurfaceFormatInfo().surfaceFormat.imageElementSizeInBytes * origin[0] + image->getHostPtrSlicePitch() * origin[1];

    EXPECT_EQ(expectedOffset, retOffset);
}

TEST(ImageTest, givenClMemForceLinearStorageSetWhenCreateImageThenDisallowTiling) {
    cl_int retVal = CL_SUCCESS;
    MockContext context;
    cl_image_desc imageDesc = {};
    imageDesc.image_width = 4096;
    imageDesc.image_height = 1;
    imageDesc.image_depth = 1;
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE3D;

    cl_image_format imageFormat = {};
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);

    auto image = std::unique_ptr<Image>(Image::create(
        &context,
        ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
        flags,
        0,
        surfaceFormat,
        &imageDesc,
        nullptr,
        retVal));

    EXPECT_FALSE(image->isTiledAllocation());
    EXPECT_NE(nullptr, image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(ImageTest, givenClMemCopyHostPointerPassedToImageCreateWhenAllocationIsNotInSystemMemoryPoolThenAllocationIsWrittenByEnqueueWriteImage) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    auto *memoryManager = new MockMemoryManagerFailFirstAllocation(*executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);
    memoryManager->returnBaseAllocateGraphicsMemoryInDevicePool = true;
    auto device = std::make_unique<MockClDevice>(MockDevice::create<MockDevice>(executionEnvironment, 0));

    MockContext ctx(device.get());

    char memory[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto taskCount = device->getGpgpuCommandStreamReceiver().peekLatestFlushedTaskCount();

    cl_int retVal = 0;
    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR;

    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 1;
    imageDesc.image_height = 1;
    imageDesc.image_row_pitch = sizeof(memory);

    cl_image_format imageFormat = {};
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_R;
    MockContext context;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    memoryManager->returnAllocateNonSystemGraphicsMemoryInDevicePool = true;
    std::unique_ptr<Image> image(
        Image::create(&ctx, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                      flags, 0, surfaceFormat, &imageDesc, memory, retVal));
    EXPECT_NE(nullptr, image);

    auto taskCountSent = device->getGpgpuCommandStreamReceiver().peekLatestFlushedTaskCount();
    EXPECT_LT(taskCount, taskCountSent);
}

using ImageDirectSubmissionTest = testing::Test;

HWTEST_F(ImageDirectSubmissionTest, givenImageCreatedWhenDestrucedThenVerifyTaskCountTick) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);
    DebugManagerStateRestore restore;
    debugManager.flags.EnableDirectSubmission.set(1);

    MockClDevice mockClDevice(new MockDevice());
    MockContext ctx(&mockClDevice);
    MockCommandQueueHw<FamilyType> mockCmdQueueHw{&ctx, &mockClDevice, nullptr};
    constexpr static TaskCountType taskCountReady = 3u;
    auto &ultCsr = mockCmdQueueHw.getUltCommandStreamReceiver();
    ultCsr.heapStorageRequiresRecyclingTag = false;

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(ultCsr);
    ultCsr.directSubmission.reset(directSubmission);
    ultCsr.directSubmissionAvailable = true;

    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 1;
    imageDesc.image_height = 1;

    std::unique_ptr<Image> image(ImageHelperUlt<Image1dDefaults>::create(&ctx, &imageDesc));
    EXPECT_NE(nullptr, image);
    image->getGraphicsAllocation(0u)->setAllocationType(AllocationType::image);
    image->getGraphicsAllocation(mockClDevice.getRootDeviceIndex())->updateTaskCount(taskCountReady, mockClDevice.getDefaultEngine().osContext->getContextId());

    ultCsr.initializeTagAllocation();
    ultCsr.taskCount.store(9u);

    const auto taskCountBefore = mockClDevice.getGpgpuCommandStreamReceiver().peekTaskCount();

    image.reset();

    auto taskCountSent = mockClDevice.getGpgpuCommandStreamReceiver().peekTaskCount();

    EXPECT_LT(taskCountBefore, taskCountSent);
}

struct ImageConvertTypeTest
    : public ::testing::Test {

    void SetUp() override {
    }

    void TearDown() override {
    }

    std::array<std::pair<uint32_t, ImageType>, 7> types = {{std::make_pair<>(CL_MEM_OBJECT_IMAGE1D, ImageType::image1D),
                                                            std::make_pair<>(CL_MEM_OBJECT_IMAGE2D, ImageType::image2D),
                                                            std::make_pair<>(CL_MEM_OBJECT_IMAGE3D, ImageType::image3D),
                                                            std::make_pair<>(CL_MEM_OBJECT_IMAGE1D_ARRAY, ImageType::image1DArray),
                                                            std::make_pair<>(CL_MEM_OBJECT_IMAGE2D_ARRAY, ImageType::image2DArray),
                                                            std::make_pair<>(CL_MEM_OBJECT_IMAGE1D_BUFFER, ImageType::image1DBuffer),
                                                            std::make_pair<>(0, ImageType::invalid)}};
};

TEST_F(ImageConvertTypeTest, givenClMemObjectTypeWhenConvertedThenCorrectImageTypeIsReturned) {

    for (size_t i = 0; i < types.size(); i++) {
        EXPECT_EQ(types[i].second, Image::convertType(static_cast<cl_mem_object_type>(types[i].first)));
    }
}

TEST_F(ImageConvertTypeTest, givenImageTypeWhenConvertedThenCorrectClMemObjectTypeIsReturned) {

    for (size_t i = 0; i < types.size(); i++) {
        EXPECT_EQ(static_cast<cl_mem_object_type>(types[i].first), Image::convertType(types[i].second));
    }
}

TEST(ImageConvertDescriptorTest, givenClImageDescWhenConvertedThenCorrectImageDescriptorIsReturned) {
    cl_image_desc clDesc = {CL_MEM_OBJECT_IMAGE1D, 16, 24, 1, 1, 1024, 2048, 1, 3, {nullptr}};
    auto desc = Image::convertDescriptor(clDesc);

    EXPECT_EQ(ImageType::image1D, desc.imageType);
    EXPECT_EQ(clDesc.image_array_size, desc.imageArraySize);
    EXPECT_EQ(clDesc.image_depth, desc.imageDepth);
    EXPECT_EQ(clDesc.image_height, desc.imageHeight);
    EXPECT_EQ(clDesc.image_row_pitch, desc.imageRowPitch);
    EXPECT_EQ(clDesc.image_slice_pitch, desc.imageSlicePitch);
    EXPECT_EQ(clDesc.image_width, desc.imageWidth);
    EXPECT_EQ(clDesc.num_mip_levels, desc.numMipLevels);
    EXPECT_EQ(clDesc.num_samples, desc.numSamples);
    EXPECT_FALSE(desc.fromParent);

    cl_mem temporary = reinterpret_cast<cl_mem>(0x1234);
    clDesc.mem_object = temporary;

    desc = Image::convertDescriptor(clDesc);
    EXPECT_TRUE(desc.fromParent);
}

TEST(ImageConvertDescriptorTest, givenImageDescriptorWhenConvertedThenCorrectClImageDescIsReturned) {
    ImageDescriptor desc = {ImageType::image2D, 16, 24, 1, 1, 1024, 2048, 1, 3, false};
    auto clDesc = Image::convertDescriptor(desc);

    EXPECT_EQ(clDesc.image_type, static_cast<cl_mem_object_type>(CL_MEM_OBJECT_IMAGE2D));
    EXPECT_EQ(clDesc.image_array_size, desc.imageArraySize);
    EXPECT_EQ(clDesc.image_depth, desc.imageDepth);
    EXPECT_EQ(clDesc.image_height, desc.imageHeight);
    EXPECT_EQ(clDesc.image_row_pitch, desc.imageRowPitch);
    EXPECT_EQ(clDesc.image_slice_pitch, desc.imageSlicePitch);
    EXPECT_EQ(clDesc.image_width, desc.imageWidth);
    EXPECT_EQ(clDesc.num_mip_levels, desc.numMipLevels);
    EXPECT_EQ(clDesc.num_samples, desc.numSamples);
    EXPECT_EQ(nullptr, clDesc.mem_object);
}

TEST(ImageTest, givenImageWhenValidateRegionAndOriginIsCalledThenAdditionalOriginAndRegionCoordinatesAreAnalyzed) {
    size_t origin[3]{};
    size_t region[3]{1, 1, 1};

    for (uint32_t imageType : {CL_MEM_OBJECT_IMAGE2D, CL_MEM_OBJECT_IMAGE2D_ARRAY, CL_MEM_OBJECT_IMAGE3D}) {
        cl_image_desc desc = {};
        desc.image_type = imageType;

        EXPECT_EQ(CL_INVALID_VALUE, Image::validateRegionAndOrigin(origin, region, desc));

        desc.image_width = 1;
        EXPECT_EQ(CL_INVALID_VALUE, Image::validateRegionAndOrigin(origin, region, desc));

        desc.image_height = 1;
        desc.image_depth = 1;
        desc.image_array_size = 1;
        EXPECT_EQ(CL_SUCCESS, Image::validateRegionAndOrigin(origin, region, desc));

        if (imageType == CL_MEM_OBJECT_IMAGE3D) {
            desc.image_depth = 0;
            EXPECT_EQ(CL_INVALID_VALUE, Image::validateRegionAndOrigin(origin, region, desc));
        }
    }
}

TEST(ImageTest, givenImageArrayWhenValidateRegionAndOriginIsCalledThenAdditionalOriginAndRegionCoordinatesAreAnalyzed) {
    size_t region[3]{1, 1, 1};
    size_t origin[3]{};
    cl_image_desc desc = {};

    desc.image_type = CL_MEM_OBJECT_IMAGE1D_ARRAY;
    desc.image_width = 1;
    EXPECT_EQ(CL_INVALID_VALUE, Image::validateRegionAndOrigin(origin, region, desc));

    desc.image_array_size = 1;
    EXPECT_EQ(CL_SUCCESS, Image::validateRegionAndOrigin(origin, region, desc));

    desc.image_type = CL_MEM_OBJECT_IMAGE2D_ARRAY;
    desc.image_array_size = 0;
    desc.image_width = 1;
    desc.image_height = 1;
    EXPECT_EQ(CL_INVALID_VALUE, Image::validateRegionAndOrigin(origin, region, desc));

    desc.image_array_size = 1;
    EXPECT_EQ(CL_SUCCESS, Image::validateRegionAndOrigin(origin, region, desc));
}

typedef ::testing::TestWithParam<uint32_t> MipLevelCoordinateTest;

TEST_P(MipLevelCoordinateTest, givenMipmappedImageWhenValidateRegionAndOriginIsCalledThenAdditionalOriginCoordinateIsAnalyzed) {
    size_t origin[4]{};
    size_t region[3]{1, 1, 1};
    cl_image_desc desc = {};
    desc.image_type = GetParam();
    desc.num_mip_levels = 2;
    desc.image_width = 1;
    desc.image_height = 1;
    desc.image_depth = 1;
    desc.image_array_size = 1;
    origin[getMipLevelOriginIdx(desc.image_type)] = 1;
    EXPECT_EQ(CL_SUCCESS, Image::validateRegionAndOrigin(origin, region, desc));
    origin[getMipLevelOriginIdx(desc.image_type)] = 2;
    EXPECT_EQ(CL_INVALID_MIP_LEVEL, Image::validateRegionAndOrigin(origin, region, desc));
}

INSTANTIATE_TEST_SUITE_P(MipLevelCoordinate,
                         MipLevelCoordinateTest,
                         ::testing::Values(CL_MEM_OBJECT_IMAGE1D, CL_MEM_OBJECT_IMAGE1D_ARRAY, CL_MEM_OBJECT_IMAGE2D,
                                           CL_MEM_OBJECT_IMAGE2D_ARRAY, CL_MEM_OBJECT_IMAGE3D));

typedef ::testing::TestWithParam<std::pair<uint32_t, bool>> HasSlicesTest;

TEST_P(HasSlicesTest, givenMemObjectTypeWhenHasSlicesIsCalledThenReturnsTrueIfTypeDefinesObjectWithSlicePitch) {
    auto pair = GetParam();
    EXPECT_EQ(pair.second, Image::hasSlices(pair.first));
}

INSTANTIATE_TEST_SUITE_P(HasSlices,
                         HasSlicesTest,
                         ::testing::Values(std::make_pair(CL_MEM_OBJECT_IMAGE1D, false),
                                           std::make_pair(CL_MEM_OBJECT_IMAGE1D_ARRAY, true),
                                           std::make_pair(CL_MEM_OBJECT_IMAGE2D, false),
                                           std::make_pair(CL_MEM_OBJECT_IMAGE2D_ARRAY, true),
                                           std::make_pair(CL_MEM_OBJECT_IMAGE3D, true),
                                           std::make_pair(CL_MEM_OBJECT_BUFFER, false),
                                           std::make_pair(CL_MEM_OBJECT_PIPE, false)));

typedef ::testing::Test ImageTransformTest;
HWTEST_F(ImageTransformTest, givenSurfaceStateWhenTransformImage3dTo2dArrayIsCalledThenSurface2dArrayIsSet) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;
    MockContext context;
    auto image = std::unique_ptr<Image>(ImageHelperUlt<Image3dDefaults>::create(&context));
    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());
    surfaceState.setSurfaceType(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_3D);
    surfaceState.setSurfaceArray(false);
    imageHw->transformImage3dTo2dArray(reinterpret_cast<void *>(&surfaceState));
    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D, surfaceState.getSurfaceType());
    EXPECT_TRUE(surfaceState.getSurfaceArray());
}

HWTEST_F(ImageTransformTest, givenSurfaceStateWhenTransformImage2dArrayTo3dIsCalledThenSurface3dIsSet) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;
    MockContext context;
    auto image = std::unique_ptr<Image>(ImageHelperUlt<Image3dDefaults>::create(&context));
    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());
    surfaceState.setSurfaceType(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D);
    surfaceState.setSurfaceArray(true);
    imageHw->transformImage2dArrayTo3d(reinterpret_cast<void *>(&surfaceState));
    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_3D, surfaceState.getSurfaceType());
    EXPECT_FALSE(surfaceState.getSurfaceArray());
}

HWTEST_F(ImageTransformTest, givenSurfaceBaseAddressAndUnifiedSurfaceWhenSetUnifiedAuxAddressCalledThenAddressIsSet) {
    MockContext context;
    auto image = std::unique_ptr<Image>(ImageHelperUlt<Image3dDefaults>::create(&context));
    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::unique_ptr<Gmm>(new Gmm(context.getDevice(0)->getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    uint64_t surfBsaseAddress = 0xABCDEF1000;
    surfaceState.setSurfaceBaseAddress(surfBsaseAddress);
    auto mockResource = reinterpret_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());
    mockResource->setUnifiedAuxTranslationCapable();

    EXPECT_EQ(0u, surfaceState.getAuxiliarySurfaceBaseAddress());

    ImageSurfaceStateHelper<FamilyType>::setUnifiedAuxBaseAddress(&surfaceState, gmm.get());
    uint64_t offset = gmm->gmmResourceInfo->getUnifiedAuxSurfaceOffset(GMM_UNIFIED_AUX_TYPE::GMM_AUX_SURF);

    EXPECT_EQ(surfBsaseAddress + offset, surfaceState.getAuxiliarySurfaceBaseAddress());
}

TEST(ImageTest, givenImageWhenFillRegionIsCalledThenProperRegionIsSet) {
    MockContext context;

    {
        size_t region[3] = {};
        std::unique_ptr<Image> image(Image1dHelperUlt<>::create(&context));

        image->fillImageRegion(region);

        EXPECT_EQ(Image1dDefaults::imageDesc.image_width, region[0]);
        EXPECT_EQ(1u, region[1]);
        EXPECT_EQ(1u, region[2]);
    }
    {
        size_t region[3] = {};
        std::unique_ptr<Image> image(Image1dArrayHelperUlt<>::create(&context));

        image->fillImageRegion(region);

        EXPECT_EQ(Image1dArrayDefaults::imageDesc.image_width, region[0]);
        EXPECT_EQ(Image1dArrayDefaults::imageDesc.image_array_size, region[1]);
        EXPECT_EQ(1u, region[2]);
    }
    {
        size_t region[3] = {};
        std::unique_ptr<Image> image(Image1dBufferHelperUlt<>::create(&context));

        image->fillImageRegion(region);

        EXPECT_EQ(Image1dBufferDefaults::imageDesc.image_width, region[0]);
        EXPECT_EQ(1u, region[1]);
        EXPECT_EQ(1u, region[2]);
    }
    {
        size_t region[3] = {};
        std::unique_ptr<Image> image(Image2dHelperUlt<>::create(&context));

        image->fillImageRegion(region);

        EXPECT_EQ(Image2dDefaults::imageDesc.image_width, region[0]);
        EXPECT_EQ(Image2dDefaults::imageDesc.image_height, region[1]);
        EXPECT_EQ(1u, region[2]);
    }
    {
        size_t region[3] = {};
        std::unique_ptr<Image> image(Image2dArrayHelperUlt<>::create(&context));

        image->fillImageRegion(region);

        EXPECT_EQ(Image2dArrayDefaults::imageDesc.image_width, region[0]);
        EXPECT_EQ(Image2dArrayDefaults::imageDesc.image_height, region[1]);
        EXPECT_EQ(Image2dArrayDefaults::imageDesc.image_array_size, region[2]);
    }
    {
        size_t region[3] = {};
        std::unique_ptr<Image> image(Image3dHelperUlt<>::create(&context));

        image->fillImageRegion(region);

        EXPECT_EQ(Image3dDefaults::imageDesc.image_width, region[0]);
        EXPECT_EQ(Image3dDefaults::imageDesc.image_height, region[1]);
        EXPECT_EQ(Image3dDefaults::imageDesc.image_depth, region[2]);
    }
}

TEST(ImageTest, givenMultiDeviceEnvironmentWhenReleaseImageFromBufferThenMainBufferProperlyDereferenced) {
    MockDefaultContext context;
    int32_t retVal;

    auto *buffer = Buffer::create(&context, CL_MEM_READ_WRITE,
                                  MemoryConstants::pageSize, nullptr, retVal);

    auto imageDesc = Image2dDefaults::imageDesc;

    cl_mem clBuffer = buffer;
    imageDesc.mem_object = clBuffer;
    auto image = Image2dHelperUlt<>::create(&context, &imageDesc);
    EXPECT_EQ(3u, buffer->getMultiGraphicsAllocation().getGraphicsAllocations().size());
    EXPECT_EQ(3u, image->getMultiGraphicsAllocation().getGraphicsAllocations().size());

    EXPECT_EQ(2, buffer->getRefInternalCount());
    image->release();
    EXPECT_EQ(1, buffer->getRefInternalCount());

    buffer->release();
}

TEST(ImageTest, givenPropertiesWithClDeviceHandleListKHRWhenCreateImageThenCorrectImageIsSet) {
    MockDefaultContext context(1);
    auto clDevice = context.getDevice(1);
    auto clDevice2 = context.getDevice(2);
    cl_device_id deviceId = clDevice;
    cl_device_id deviceId2 = clDevice2;

    cl_mem_properties_intel properties[] = {
        CL_MEM_DEVICE_HANDLE_LIST_KHR,
        reinterpret_cast<cl_mem_properties_intel>(deviceId),
        reinterpret_cast<cl_mem_properties_intel>(deviceId2),
        CL_MEM_DEVICE_HANDLE_LIST_END_KHR,
        0};

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ForceLinearImages.set(true);

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_int retVal;

    char hostPtr[elementSize * 2 + 64]{};

    imageFormat.image_channel_data_type = channelType;
    imageFormat.image_channel_order = channelOrder;

    imageDesc.num_mip_levels = 0;
    imageDesc.num_samples = 0;
    imageDesc.mem_object = NULL;
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 1;
    imageDesc.image_height = 0;
    imageDesc.image_depth = 0;
    imageDesc.image_array_size = 0;
    imageDesc.image_row_pitch = 0;
    imageDesc.image_slice_pitch = 0;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR;
    cl_mem_flags_intel flagsIntel = 0;
    cl_mem_alloc_flags_intel allocflags = 0;
    MemoryProperties memoryProperties{};

    ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                    ClMemoryPropertiesHelper::ObjType::image, context);

    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);

    auto image = Image::create(
        &context,
        memoryProperties,
        flags,
        0,
        surfaceFormat,
        &imageDesc,
        hostPtr,
        retVal);

    EXPECT_EQ(retVal, CL_SUCCESS);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ(image->getGraphicsAllocation(0), nullptr);
    EXPECT_NE(image->getGraphicsAllocation(1), nullptr);
    EXPECT_NE(image->getGraphicsAllocation(2), nullptr);

    EXPECT_EQ(static_cast<size_t>(elementSize), image->getHostPtrRowPitch());
    EXPECT_EQ(0u, image->getHostPtrSlicePitch());

    delete image;
}

using MultiRootDeviceImageTest = ::testing::Test;
HWTEST2_F(MultiRootDeviceImageTest, givenHostPtrToCopyWhenImageIsCreatedWithMultiStorageThenMemoryIsPutInFirstDeviceInContext, MatchAny) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    cl_int retVal = 0;
    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR;

    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 4;
    imageDesc.image_height = 1;
    imageDesc.image_row_pitch = 4;

    cl_image_format imageFormat = {};
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_R;

    UltClDeviceFactory deviceFactory{2, 0};
    auto memoryManager = static_cast<MockMemoryManager *>(deviceFactory.rootDevices[0]->getMemoryManager());
    for (auto &rootDeviceIndex : {0, 1}) {
        memoryManager->localMemorySupported[rootDeviceIndex] = true;
    }
    {
        cl_device_id deviceIds[] = {
            deviceFactory.rootDevices[0],
            deviceFactory.rootDevices[1]};
        MockContext context{nullptr, nullptr};
        context.initializeWithDevices(ClDeviceVector{deviceIds, 2}, false);

        uint32_t data{};

        auto surfaceFormat = Image::getSurfaceFormatFromTable(
            flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
        std::unique_ptr<Image> image(
            Image::create(&context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                          flags, 0, surfaceFormat, &imageDesc, &data, retVal));
        EXPECT_NE(nullptr, image);

        EXPECT_EQ(2u, context.getRootDeviceIndices().size());
        EXPECT_EQ(0u, image->getMultiGraphicsAllocation().getMigrationSyncData()->getCurrentLocation());
    }
    {
        cl_device_id deviceIds[] = {
            deviceFactory.rootDevices[1],
            deviceFactory.rootDevices[0]};
        MockContext context{nullptr, nullptr};
        context.initializeWithDevices(ClDeviceVector{deviceIds, 2}, false);

        uint32_t data{};

        auto surfaceFormat = Image::getSurfaceFormatFromTable(
            flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
        std::unique_ptr<Image> image(
            Image::create(&context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                          flags, 0, surfaceFormat, &imageDesc, &data, retVal));
        EXPECT_NE(nullptr, image);

        EXPECT_EQ(2u, context.getRootDeviceIndices().size());
        EXPECT_EQ(1u, image->getMultiGraphicsAllocation().getMigrationSyncData()->getCurrentLocation());
    }
}

TEST(ImageTest, givenImageWhenTruePassedToSet3DUavOrRtvThenValueInImageIsSetToTrue) {
    MockImageBase img;
    img.setAs3DUavOrRtvImage(true);
    EXPECT_TRUE(img.is3DUAVOrRTV);
}

TEST(ImageTest, givenImageWhenFalsePassedToSet3DUavOrRtvThenValueInImageIsSetToFalse) {
    MockImageBase img;
    img.setAs3DUavOrRtvImage(false);
    EXPECT_FALSE(img.is3DUAVOrRTV);
}

using ImageAdjustDepthTests = ::testing::Test;

HWTEST2_F(ImageAdjustDepthTests, givenSurfaceStateWhenImage3DRTVOrUAVThenDepthCalculationIsDone, IsAtLeastXe2HpgCore) {

    typename FamilyType::RENDER_SURFACE_STATE ss;
    uint32_t minArrayElement = 1;
    uint32_t renderTargetViewExtent = 1;
    uint32_t originalDepth = 10;
    uint32_t mipCount = 1;
    ss.setDepth(originalDepth);

    ImageHw<FamilyType>::adjustDepthLimitations(&ss, minArrayElement, renderTargetViewExtent, originalDepth, mipCount, true);
    EXPECT_NE(ss.getDepth(), originalDepth);
}

HWTEST2_F(ImageAdjustDepthTests, givenSurfaceStateWhenImageIsNot3DRTVOrUAVThenDepthCalculationIsDone, IsAtLeastXe2HpgCore) {
    typename FamilyType::RENDER_SURFACE_STATE ss;
    uint32_t minArrayElement = 1;
    uint32_t renderTargetViewExtent = 1;
    uint32_t originalDepth = 10;
    uint32_t mipCount = 1;
    ss.setDepth(originalDepth);

    ImageHw<FamilyType>::adjustDepthLimitations(&ss, minArrayElement, renderTargetViewExtent, originalDepth, mipCount, false);
    EXPECT_EQ(ss.getDepth(), originalDepth);
}

HWTEST2_F(ImageAdjustDepthTests, givenSurfaceStateWhenAdjustDepthReturnFalseThenOriginalDepthIsUsed, IsAtMostXeCore) {
    typename FamilyType::RENDER_SURFACE_STATE ss;
    uint32_t minArrayElement = 1;
    uint32_t renderTargetViewExtent = 1;
    uint32_t originalDepth = 10;
    uint32_t mipCount = 1;
    ss.setDepth(originalDepth);

    ImageHw<FamilyType>::adjustDepthLimitations(&ss, minArrayElement, renderTargetViewExtent, originalDepth, mipCount, true);
    EXPECT_EQ(ss.getDepth(), originalDepth);
}
