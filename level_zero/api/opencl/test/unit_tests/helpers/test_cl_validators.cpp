/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/leo_cl_memory_properties_helpers.h"
#include "level_zero/api/opencl/source/helpers/leo_cl_validators.h"
#include "level_zero/api/opencl/source/mem_obj/leo_buffer.h"
#include "level_zero/api/opencl/source/mem_obj/leo_image.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include "CL/cl.h"
#include "CL/cl_ext.h"

namespace NEO {
namespace LEO {
namespace ult {

TEST(ValidateVoidPtrTests, givenNullPtrWhenValidateObjectThenReturnsCLInvalidValue) {
    EXPECT_EQ(CL_INVALID_VALUE, validateObject(static_cast<void *>(nullptr)));
}

TEST(ValidateVoidPtrTests, givenNonNullPtrWhenValidateObjectThenReturnsSuccess) {
    int dummy = 0;
    EXPECT_EQ(CL_SUCCESS, validateObject(static_cast<void *>(&dummy)));
}

TEST(ValidateBoolTests, givenFalseWhenValidateObjectThenReturnsCLInvalidValue) {
    EXPECT_EQ(CL_INVALID_VALUE, validateObject(false));
}

TEST(ValidateBoolTests, givenTrueWhenValidateObjectThenReturnsSuccess) {
    EXPECT_EQ(CL_SUCCESS, validateObject(true));
}

TEST(ValidateNonZeroBufferSizeTests, givenZeroSizeWhenValidateObjectThenReturnsCLInvalidBufferSize) {
    EXPECT_EQ(CL_INVALID_BUFFER_SIZE, validateObject(static_cast<NonZeroBufferSize>(0)));
}

TEST(ValidateNonZeroBufferSizeTests, givenNonZeroSizeWhenValidateObjectThenReturnsSuccess) {
    EXPECT_EQ(CL_SUCCESS, validateObject(static_cast<NonZeroBufferSize>(1)));
    EXPECT_EQ(CL_SUCCESS, validateObject(static_cast<NonZeroBufferSize>(1024)));
}

TEST(ValidatePatternSizeTests, givenZeroSizeWhenValidateObjectThenReturnsCLInvalidValue) {
    EXPECT_EQ(CL_INVALID_VALUE, validateObject(static_cast<PatternSize>(0)));
}

TEST(ValidatePatternSizeTests, givenValidPowerOfTwoSizesWhenValidateObjectThenReturnsSuccess) {
    EXPECT_EQ(CL_SUCCESS, validateObject(static_cast<PatternSize>(1)));
    EXPECT_EQ(CL_SUCCESS, validateObject(static_cast<PatternSize>(16)));
    EXPECT_EQ(CL_SUCCESS, validateObject(static_cast<PatternSize>(128)));
}

TEST(ValidatePatternSizeTests, givenNonPowerOfTwoSizeWhenValidateObjectThenReturnsCLInvalidValue) {
    EXPECT_EQ(CL_INVALID_VALUE, validateObject(static_cast<PatternSize>(3)));
}

TEST(ValidatePatternSizeTests, givenSizeAbove128WhenValidateObjectThenReturnsCLInvalidValue) {
    EXPECT_EQ(CL_INVALID_VALUE, validateObject(static_cast<PatternSize>(129)));
    EXPECT_EQ(CL_INVALID_VALUE, validateObject(static_cast<PatternSize>(256)));
}

TEST(ValidateNullHandleTests, givenNullContextWhenValidateObjectThenReturnsCLInvalidContext) {
    EXPECT_EQ(CL_INVALID_CONTEXT, validateObject(static_cast<cl_context>(nullptr)));
}

TEST(ValidateNullHandleTests, givenNullDeviceWhenValidateObjectThenReturnsCLInvalidDevice) {
    EXPECT_EQ(CL_INVALID_DEVICE, validateObject(static_cast<cl_device_id>(nullptr)));
}

TEST(ValidateNullHandleTests, givenNullPlatformWhenValidateObjectThenReturnsCLInvalidPlatform) {
    EXPECT_EQ(CL_INVALID_PLATFORM, validateObject(static_cast<cl_platform_id>(nullptr)));
}

TEST(ValidateNullHandleTests, givenNullCommandQueueWhenValidateObjectThenReturnsCLInvalidCommandQueue) {
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, validateObject(static_cast<cl_command_queue>(nullptr)));
}

TEST(ValidateNullHandleTests, givenNullEventWhenValidateObjectThenReturnsCLInvalidEvent) {
    EXPECT_EQ(CL_INVALID_EVENT, validateObject(static_cast<cl_event>(nullptr)));
}

TEST(ValidateNullHandleTests, givenNullMemObjWhenValidateObjectThenReturnsCLInvalidMemObject) {
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, validateObject(static_cast<cl_mem>(nullptr)));
}

TEST(ValidateNullHandleTests, givenNullSamplerWhenValidateObjectThenReturnsCLInvalidSampler) {
    EXPECT_EQ(CL_INVALID_SAMPLER, validateObject(static_cast<cl_sampler>(nullptr)));
}

TEST(ValidateNullHandleTests, givenNullProgramWhenValidateObjectThenReturnsCLInvalidProgram) {
    EXPECT_EQ(CL_INVALID_PROGRAM, validateObject(static_cast<cl_program>(nullptr)));
}

TEST(ValidateNullHandleTests, givenNullKernelWhenValidateObjectThenReturnsCLInvalidKernel) {
    EXPECT_EQ(CL_INVALID_KERNEL, validateObject(static_cast<cl_kernel>(nullptr)));
}

TEST(ValidateEventWaitListTests, givenEmptyNullListWhenValidateObjectThenReturnsSuccess) {
    EventWaitList ewl{};
    EXPECT_EQ(CL_SUCCESS, validateObject(ewl));
}

TEST(ValidateEventWaitListTests, givenEmptyNonNullDataWhenValidateObjectThenReturnsCLInvalidEventWaitList) {
    cl_event fakeEvent = reinterpret_cast<cl_event>(0x1);
    EventWaitList ewl{&fakeEvent, static_cast<size_t>(0)};
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, validateObject(ewl));
}

TEST(ValidateEventWaitListTests, givenListWithNullEventWhenValidateObjectThenReturnsCLInvalidEventWaitList) {
    cl_event nullEvent = nullptr;
    EventWaitList ewl{&nullEvent, 1};
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, validateObject(ewl));
}

TEST(ValidateDeviceListTests, givenEmptyNullListWhenValidateObjectThenReturnsSuccess) {
    DeviceList dl{};
    EXPECT_EQ(CL_SUCCESS, validateObject(dl));
}

TEST(ValidateDeviceListTests, givenEmptyNonNullDataWhenValidateObjectThenReturnsCLInvalidValue) {
    cl_device_id fakeDevice = reinterpret_cast<cl_device_id>(0x1);
    DeviceList dl{&fakeDevice, static_cast<size_t>(0)};
    EXPECT_EQ(CL_INVALID_VALUE, validateObject(dl));
}

TEST(ValidateMemObjListTests, givenEmptyNullListWhenValidateObjectThenReturnsSuccess) {
    MemObjList mol{};
    EXPECT_EQ(CL_SUCCESS, validateObject(mol));
}

TEST(ValidateMemObjListTests, givenEmptyNonNullDataWhenValidateObjectThenReturnsCLInvalidValue) {
    cl_mem fakeMem = reinterpret_cast<cl_mem>(0x1);
    MemObjList mol{&fakeMem, static_cast<size_t>(0)};
    EXPECT_EQ(CL_INVALID_VALUE, validateObject(mol));
}

TEST(ValidateMemObjListTests, givenListWithNullMemObjWhenValidateObjectThenReturnsCLInvalidMemObject) {
    cl_mem nullMem = nullptr;
    MemObjList mol{&nullMem, 1};
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, validateObject(mol));
}

TEST(ValidateYuvOperationTests, givenNullOriginWhenValidateYuvOperationThenReturnsCLInvalidValue) {
    size_t region[3] = {2, 2, 1};
    EXPECT_EQ(CL_INVALID_VALUE, validateYuvOperation(nullptr, region));
}

TEST(ValidateYuvOperationTests, givenNullRegionWhenValidateYuvOperationThenReturnsCLInvalidValue) {
    size_t origin[3] = {0, 0, 0};
    EXPECT_EQ(CL_INVALID_VALUE, validateYuvOperation(origin, nullptr));
}

TEST(ValidateYuvOperationTests, givenOddOriginXWhenValidateYuvOperationThenReturnsCLInvalidValue) {
    size_t origin[3] = {1, 0, 0};
    size_t region[3] = {2, 2, 1};
    EXPECT_EQ(CL_INVALID_VALUE, validateYuvOperation(origin, region));
}

TEST(ValidateYuvOperationTests, givenOddRegionXWhenValidateYuvOperationThenReturnsCLInvalidValue) {
    size_t origin[3] = {0, 0, 0};
    size_t region[3] = {3, 2, 1};
    EXPECT_EQ(CL_INVALID_VALUE, validateYuvOperation(origin, region));
}

TEST(ValidateYuvOperationTests, givenEvenOriginAndRegionXWhenValidateYuvOperationThenReturnsSuccess) {
    size_t origin[3] = {0, 0, 0};
    size_t region[3] = {4, 2, 1};
    EXPECT_EQ(CL_SUCCESS, validateYuvOperation(origin, region));
}

TEST(IsPackedYuvImageTests, givenNullImageFormatWhenIsPackedYuvImageThenReturnsFalse) {
    EXPECT_FALSE(isPackedYuvImage(nullptr));
}

TEST(IsPackedYuvImageTests, givenYuyvFormatWhenIsPackedYuvImageThenReturnsTrue) {
    cl_image_format fmt{CL_YUYV_INTEL, CL_UNORM_INT8};
    EXPECT_TRUE(isPackedYuvImage(&fmt));
}

TEST(IsPackedYuvImageTests, givenUyvyFormatWhenIsPackedYuvImageThenReturnsTrue) {
    cl_image_format fmt{CL_UYVY_INTEL, CL_UNORM_INT8};
    EXPECT_TRUE(isPackedYuvImage(&fmt));
}

TEST(IsPackedYuvImageTests, givenRgbaFormatWhenIsPackedYuvImageThenReturnsFalse) {
    cl_image_format fmt{CL_RGBA, CL_UNORM_INT8};
    EXPECT_FALSE(isPackedYuvImage(&fmt));
}

TEST(IsNV12ImageTests, givenNullImageFormatWhenIsNV12ImageThenReturnsFalse) {
    EXPECT_FALSE(isNV12Image(nullptr));
}

TEST(IsNV12ImageTests, givenNV12FormatWhenIsNV12ImageThenReturnsTrue) {
    cl_image_format fmt{CL_NV12_INTEL, CL_UNORM_INT8};
    EXPECT_TRUE(isNV12Image(&fmt));
}

TEST(IsNV12ImageTests, givenRgbaFormatWhenIsNV12ImageThenReturnsFalse) {
    cl_image_format fmt{CL_RGBA, CL_UNORM_INT8};
    EXPECT_FALSE(isNV12Image(&fmt));
}

TEST(ValidateImageCopyTests, givenDifferentChannelOrdersWhenValidateImageCopyThenReturnsCLImageFormatMismatch) {
    cl_image_format srcFormat{CL_RGBA, CL_UNORM_INT8};
    cl_image_format dstFormat{CL_BGRA, CL_UNORM_INT8};
    size_t origin[3] = {0, 0, 0};
    size_t region[3] = {4, 2, 1};
    EXPECT_EQ(CL_IMAGE_FORMAT_MISMATCH, validateImageCopy(srcFormat, dstFormat, origin, origin, region));
}

TEST(ValidateImageCopyTests, givenDifferentChannelDataTypesWhenValidateImageCopyThenReturnsCLImageFormatMismatch) {
    cl_image_format srcFormat{CL_RGBA, CL_UNORM_INT8};
    cl_image_format dstFormat{CL_RGBA, CL_UNORM_INT16};
    size_t origin[3] = {0, 0, 0};
    size_t region[3] = {4, 2, 1};
    EXPECT_EQ(CL_IMAGE_FORMAT_MISMATCH, validateImageCopy(srcFormat, dstFormat, origin, origin, region));
}

TEST(ValidateImageCopyTests, givenMatchingNonYuvFormatsWithOddOriginWhenValidateImageCopyThenReturnsSuccess) {
    cl_image_format format{CL_RGBA, CL_UNORM_INT8};
    size_t origin[3] = {1, 0, 0};
    size_t region[3] = {3, 2, 1};
    EXPECT_EQ(CL_SUCCESS, validateImageCopy(format, format, origin, origin, region));
}

TEST(ValidateImageCopyTests, givenPackedYuvWithOddSrcOriginWhenValidateImageCopyThenReturnsCLInvalidValue) {
    cl_image_format format{CL_YUYV_INTEL, CL_UNORM_INT8};
    size_t srcOrigin[3] = {1, 0, 0};
    size_t dstOrigin[3] = {0, 0, 0};
    size_t region[3] = {4, 2, 1};
    EXPECT_EQ(CL_INVALID_VALUE, validateImageCopy(format, format, srcOrigin, dstOrigin, region));
}

TEST(ValidateImageCopyTests, givenPackedYuvWithOddDstOriginWhenValidateImageCopyThenReturnsCLInvalidValue) {
    cl_image_format format{CL_UYVY_INTEL, CL_UNORM_INT8};
    size_t srcOrigin[3] = {0, 0, 0};
    size_t dstOrigin[3] = {1, 0, 0};
    size_t region[3] = {4, 2, 1};
    EXPECT_EQ(CL_INVALID_VALUE, validateImageCopy(format, format, srcOrigin, dstOrigin, region));
}

TEST(ValidateImageCopyTests, givenPackedYuvWithOddRegionWhenValidateImageCopyThenReturnsCLInvalidValue) {
    cl_image_format format{CL_YVYU_INTEL, CL_UNORM_INT8};
    size_t origin[3] = {0, 0, 0};
    size_t region[3] = {3, 2, 1};
    EXPECT_EQ(CL_INVALID_VALUE, validateImageCopy(format, format, origin, origin, region));
}

TEST(ValidateImageCopyTests, givenPackedYuvWithEvenOriginAndRegionWhenValidateImageCopyThenReturnsSuccess) {
    cl_image_format format{CL_VYUY_INTEL, CL_UNORM_INT8};
    size_t origin[3] = {2, 0, 0};
    size_t region[3] = {4, 2, 1};
    EXPECT_EQ(CL_SUCCESS, validateImageCopy(format, format, origin, origin, region));
}

TEST(ValidateImageCopyTests, givenPackedYuvWithNonZeroDstSliceOriginWhenValidateImageCopyThenReturnsCLInvalidValue) {
    cl_image_format format{CL_YUYV_INTEL, CL_UNORM_INT8};
    size_t srcOrigin[3] = {0, 0, 0};
    size_t dstOrigin[3] = {0, 0, 1};
    size_t region[3] = {4, 2, 1};
    EXPECT_EQ(CL_INVALID_VALUE, validateImageCopy(format, format, srcOrigin, dstOrigin, region));
}

TEST(ValidateImageCopyTests, givenNonYuvFormatWithNonZeroDstSliceOriginWhenValidateImageCopyThenReturnsSuccess) {
    cl_image_format format{CL_RGBA, CL_UNORM_INT8};
    size_t srcOrigin[3] = {0, 0, 0};
    size_t dstOrigin[3] = {0, 0, 1};
    size_t region[3] = {4, 2, 1};
    EXPECT_EQ(CL_SUCCESS, validateImageCopy(format, format, srcOrigin, dstOrigin, region));
}

TEST(ValidateImageFormatTests, givenNullFormatWhenValidateImageFormatThenReturnsCLInvalidImageFormatDescriptor) {
    EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, validateImageFormat(nullptr));
}

TEST(ValidateImageFormatTests, givenLegalChannelOrderAndDataTypePairWhenValidateImageFormatThenReturnsSuccess) {
    const cl_image_format formats[] = {
        {CL_R, CL_FLOAT},
        {CL_RGBA, CL_UNSIGNED_INT32},
        {CL_INTENSITY, CL_UNORM_INT8},
        {CL_RGB, CL_UNORM_SHORT_565},
        {CL_BGRA, CL_SNORM_INT8},
        {CL_sBGRA, CL_UNORM_INT8},
        {CL_DEPTH, CL_UNORM_INT16},
        {CL_DEPTH_STENCIL, CL_UNORM_INT24},
        {CL_NV12_INTEL, CL_UNORM_INT8},
        {CL_UYVY_INTEL, CL_UNORM_INT8}};

    for (const auto &format : formats) {
        EXPECT_EQ(CL_SUCCESS, validateImageFormat(&format))
            << "channel order " << format.image_channel_order
            << ", data type " << format.image_channel_data_type;
    }
}

TEST(ValidateImageFormatTests, givenIllegalChannelOrderOrDataTypeWhenValidateImageFormatThenReturnsCLInvalidImageFormatDescriptor) {
    const cl_image_format formats[] = {
        {CL_RGBA, 0xdeadbeef},
        {0xdeadbeef, CL_UNORM_INT8},
        {CL_INTENSITY, CL_SIGNED_INT8},
        {CL_RGB, CL_FLOAT},
        {CL_BGRA, CL_UNORM_INT16},
        {CL_NV12_INTEL, CL_HALF_FLOAT}};

    for (const auto &format : formats) {
        EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, validateImageFormat(&format))
            << "channel order " << format.image_channel_order
            << ", data type " << format.image_channel_data_type;
    }
}

constexpr size_t uint32Max = std::numeric_limits<uint32_t>::max();
constexpr size_t aboveUint32Max = uint32Max + 1u;

TEST(FitsInUint32Tests, givenValueUpToUint32MaxWhenCheckingThenReturnsTrue) {
    EXPECT_TRUE(fitsInUint32(0u));
    EXPECT_TRUE(fitsInUint32(uint32Max));
}

TEST(FitsInUint32Tests, givenValueAboveUint32MaxWhenCheckingThenReturnsFalse) {
    EXPECT_FALSE(fitsInUint32(aboveUint32Max));
    EXPECT_FALSE(fitsInUint32(std::numeric_limits<size_t>::max()));
}

TEST(RectArgsFitInUint32Tests, givenAllArgumentsWithinUint32WhenCheckingThenReturnsTrue) {
    const size_t origin[3] = {1u, 2u, 3u};
    const size_t region[3] = {uint32Max, 5u, 6u};
    EXPECT_TRUE(rectArgsFitInUint32(origin, region, uint32Max, uint32Max));
}

TEST(RectArgsFitInUint32Tests, givenOriginOrRegionComponentAboveUint32MaxWhenCheckingThenReturnsFalse) {
    for (uint32_t fieldIndex = 0; fieldIndex < 6u; fieldIndex++) {
        size_t origin[3] = {1u, 2u, 3u};
        size_t region[3] = {4u, 5u, 6u};
        auto &field = fieldIndex < 3u ? origin[fieldIndex] : region[fieldIndex - 3u];
        field = aboveUint32Max;
        EXPECT_FALSE(rectArgsFitInUint32(origin, region, 16u, 64u)) << "field index " << fieldIndex;
    }
}

TEST(RectArgsFitInUint32Tests, givenPitchAboveUint32MaxWhenCheckingThenReturnsFalse) {
    const size_t origin[3] = {1u, 2u, 3u};
    const size_t region[3] = {4u, 5u, 6u};
    EXPECT_FALSE(rectArgsFitInUint32(origin, region, aboveUint32Max, 64u));
    EXPECT_FALSE(rectArgsFitInUint32(origin, region, 16u, aboveUint32Max));
}

TEST(ValidateNullHandleTests, givenNullCommandBufferWhenValidateObjectThenReturnsCLInvalidCommandBufferKhr) {
    EXPECT_EQ(CL_INVALID_COMMAND_BUFFER_KHR, validateObject(static_cast<cl_command_buffer_khr>(nullptr)));
}

TEST(ValidateEventWaitListTests, givenNullDataWithNonZeroSizeWhenValidateObjectThenReturnsCLInvalidEventWaitList) {
    EventWaitList waitList(static_cast<const cl_event *>(nullptr), 2u);
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, validateObject(waitList));
}

TEST(ValidateDeviceListTests, givenNullDataWithNonZeroSizeWhenValidateObjectThenReturnsCLInvalidValue) {
    DeviceList deviceList(static_cast<const cl_device_id *>(nullptr), 2u);
    EXPECT_EQ(CL_INVALID_VALUE, validateObject(deviceList));
}

TEST(ValidateDeviceListTests, givenListWithNullDeviceWhenValidateObjectThenReturnsCLInvalidDevice) {
    cl_device_id devices[] = {nullptr};
    DeviceList deviceList(devices, 1u);
    EXPECT_EQ(CL_INVALID_DEVICE, validateObject(deviceList));
}

TEST(ValidateMemObjListTests, givenNullDataWithNonZeroSizeWhenValidateObjectThenReturnsCLInvalidValue) {
    MemObjList memObjList(static_cast<const cl_mem *>(nullptr), 2u);
    EXPECT_EQ(CL_INVALID_VALUE, validateObject(memObjList));
}

TEST(ValidateTypedMemObjTests, givenNullBufferHandleWhenValidateObjectThenReturnsCLInvalidMemObject) {
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, validateObject(BufferObj{nullptr}));
}

TEST(ValidateTypedMemObjTests, givenNullImageHandleWhenValidateObjectThenReturnsCLInvalidMemObject) {
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, validateObject(ImageObj{nullptr}));
}

TEST(ValidateAndCastTests, givenSingleNullHandleWhenValidateAndCastThenReturnsErrorAndNullPointer) {
    auto [errCode, context] = validateAndCast(std::make_tuple(static_cast<cl_context>(nullptr)));
    EXPECT_EQ(CL_INVALID_CONTEXT, errCode);
    EXPECT_EQ(nullptr, context);
}

TEST(ValidateAndCastTests, givenMultipleNullHandlesWhenValidateAndCastThenFirstFailureIsReported) {
    auto [errCode, context, queue] = validateAndCast(std::make_tuple(static_cast<cl_context>(nullptr),
                                                                     static_cast<cl_command_queue>(nullptr)));
    EXPECT_EQ(CL_INVALID_CONTEXT, errCode);
    EXPECT_EQ(nullptr, context);
    EXPECT_EQ(nullptr, queue);
}

TEST(ValidateAndCastTests, givenReorderedNullHandlesWhenValidateAndCastThenFirstFailureIsReported) {
    auto [errCode, queue, context] = validateAndCast(std::make_tuple(static_cast<cl_command_queue>(nullptr),
                                                                     static_cast<cl_context>(nullptr)));
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, errCode);
    EXPECT_EQ(nullptr, queue);
    EXPECT_EQ(nullptr, context);
}

TEST(ValidateAndCastTests, givenFailingNonCastObjectWhenValidateAndCastThenCastObjectsAreNotEvaluated) {
    auto [errCode, context] = validateAndCast(std::make_tuple(static_cast<cl_context>(nullptr)),
                                              std::make_tuple(static_cast<void *>(nullptr)));
    EXPECT_EQ(CL_INVALID_VALUE, errCode);
    EXPECT_EQ(nullptr, context);
}

TEST(ValidateAndCastTests, givenPassingNonCastObjectsAndFailingCastObjectWhenValidateAndCastThenCastErrorIsReported) {
    int dummy = 0;
    auto [errCode, context] = validateAndCast(std::make_tuple(static_cast<cl_context>(nullptr)),
                                              std::make_tuple(static_cast<void *>(&dummy), true));
    EXPECT_EQ(CL_INVALID_CONTEXT, errCode);
    EXPECT_EQ(nullptr, context);
}

TEST(ValidateAndCastTests, givenFirstFailingNonCastObjectWhenValidateAndCastThenItsErrorIsReported) {
    int dummy = 0;
    auto [errCode, context] = validateAndCast(std::make_tuple(static_cast<cl_context>(nullptr)),
                                              std::make_tuple(static_cast<NonZeroBufferSize>(0),
                                                              static_cast<void *>(&dummy)));
    EXPECT_EQ(CL_INVALID_BUFFER_SIZE, errCode);
    EXPECT_EQ(nullptr, context);
}

TEST(ValidateAndCastTests, givenNoCastObjectsAndPassingNonCastObjectsWhenValidateAndCastThenReturnsSuccess) {
    auto result = validateAndCast(std::make_tuple(), std::make_tuple(true));
    EXPECT_EQ(CL_SUCCESS, std::get<0>(result));
}

TEST(ValidateAndCastTests, givenNoObjectsAtAllWhenValidateAndCastThenReturnsSuccess) {
    auto result = validateAndCast(std::make_tuple());
    EXPECT_EQ(CL_SUCCESS, std::get<0>(result));
}

struct ImageDescriptorValidatorFixture : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();

        imageDesc = {};
        imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
        imageDesc.image_width = 16;
        imageDesc.image_height = 16;
    }

    cl_int validate(cl_mem_flags flags, const cl_image_format &imageFormat, const void *hostPtr = nullptr) {
        auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &clDevice->getDevice());
        return validateStandaloneImageDescriptor(*clDevice, memoryProperties, flags, &imageFormat, &imageDesc, hostPtr);
    }

    ClDevice *clDevice = nullptr;
    cl_image_desc imageDesc{};
};

TEST_F(ImageDescriptorValidatorFixture, givenSupportedFormatAndValidDescriptorWhenValidatingThenReturnsSuccess) {
    cl_image_format format{CL_RGBA, CL_UNORM_INT8};
    EXPECT_EQ(CL_SUCCESS, validate(CL_MEM_READ_WRITE, format));
}

TEST_F(ImageDescriptorValidatorFixture, givenUnsupportedFormatWhenValidatingThenReturnsCLImageFormatNotSupported) {
    cl_image_format format{CL_RGB, CL_UNORM_SHORT_565};
    EXPECT_EQ(CL_IMAGE_FORMAT_NOT_SUPPORTED, validate(CL_MEM_READ_WRITE, format));
}

TEST_F(ImageDescriptorValidatorFixture, givenImage2dExceedingDeviceWidthWhenValidatingThenReturnsCLInvalidImageSize) {
    cl_image_format format{CL_RGBA, CL_UNORM_INT8};
    imageDesc.image_width = clDevice->getSharedDeviceInfo().image2DMaxWidth + 1;
    EXPECT_EQ(CL_INVALID_IMAGE_SIZE, validate(CL_MEM_READ_WRITE, format));
}

TEST_F(ImageDescriptorValidatorFixture, givenImage2dExceedingDeviceHeightWhenValidatingThenReturnsCLInvalidImageSize) {
    cl_image_format format{CL_RGBA, CL_UNORM_INT8};
    imageDesc.image_height = clDevice->getSharedDeviceInfo().image2DMaxHeight + 1;
    EXPECT_EQ(CL_INVALID_IMAGE_SIZE, validate(CL_MEM_READ_WRITE, format));
}

TEST_F(ImageDescriptorValidatorFixture, givenImage2dWithZeroWidthWhenValidatingThenReturnsCLInvalidImageDescriptor) {
    cl_image_format format{CL_RGBA, CL_UNORM_INT8};
    imageDesc.image_width = 0;
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, validate(CL_MEM_READ_WRITE, format));
}

TEST_F(ImageDescriptorValidatorFixture, givenImage2dWithZeroHeightWhenValidatingThenReturnsCLInvalidImageDescriptor) {
    cl_image_format format{CL_RGBA, CL_UNORM_INT8};
    imageDesc.image_height = 0;
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, validate(CL_MEM_READ_WRITE, format));
}

TEST_F(ImageDescriptorValidatorFixture, givenNonImage2dTypeWhenValidatingThenSizeChecksAreSkipped) {
    cl_image_format format{CL_RGBA, CL_UNORM_INT8};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE3D;
    imageDesc.image_width = 0;
    imageDesc.image_height = 0;
    EXPECT_EQ(CL_SUCCESS, validate(CL_MEM_READ_WRITE, format));
}

TEST_F(ImageDescriptorValidatorFixture, givenNonZeroRowPitchWithoutHostPtrWhenValidatingThenReturnsCLInvalidImageDescriptor) {
    cl_image_format format{CL_RGBA, CL_UNORM_INT8};
    imageDesc.image_row_pitch = 64;
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, validate(CL_MEM_READ_WRITE, format));
}

TEST_F(ImageDescriptorValidatorFixture, givenRowPitchNotMultipleOfElementSizeWhenValidatingThenReturnsCLInvalidImageDescriptor) {
    cl_image_format format{CL_RGBA, CL_UNORM_INT8};
    uint64_t hostStorage = 0u;
    imageDesc.image_row_pitch = 65;
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, validate(CL_MEM_READ_WRITE, format, &hostStorage));
}

TEST_F(ImageDescriptorValidatorFixture, givenRowPitchSmallerThanRowWhenValidatingThenReturnsCLInvalidImageDescriptor) {
    cl_image_format format{CL_RGBA, CL_UNORM_INT8};
    uint64_t hostStorage = 0u;
    imageDesc.image_row_pitch = 32;
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, validate(CL_MEM_READ_WRITE, format, &hostStorage));
}

TEST_F(ImageDescriptorValidatorFixture, givenExactRowPitchWithHostPtrWhenValidatingThenReturnsSuccess) {
    cl_image_format format{CL_RGBA, CL_UNORM_INT8};
    uint64_t hostStorage = 0u;
    imageDesc.image_row_pitch = imageDesc.image_width * 4;
    EXPECT_EQ(CL_SUCCESS, validate(CL_MEM_READ_WRITE, format, &hostStorage));
}

TEST_F(ImageDescriptorValidatorFixture, givenPackedYuvWithoutReadOnlyFlagWhenValidatingThenReturnsCLInvalidValue) {
    cl_image_format format{CL_YUYV_INTEL, CL_UNORM_INT8};
    EXPECT_EQ(CL_INVALID_VALUE, validate(CL_MEM_READ_WRITE, format));
}

TEST_F(ImageDescriptorValidatorFixture, givenPackedYuvWithOddWidthWhenValidatingThenReturnsCLInvalidImageDescriptor) {
    cl_image_format format{CL_YUYV_INTEL, CL_UNORM_INT8};
    imageDesc.image_width = 15;
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, validate(CL_MEM_READ_ONLY, format));
}

TEST_F(ImageDescriptorValidatorFixture, givenPackedYuvWithNonTwoDimensionalTypeWhenValidatingThenReturnsCLInvalidImageDescriptor) {
    cl_image_format format{CL_YUYV_INTEL, CL_UNORM_INT8};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE3D;
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, validate(CL_MEM_READ_ONLY, format));
}

TEST_F(ImageDescriptorValidatorFixture, givenValidPackedYuvWhenValidatingThenReturnsSuccess) {
    cl_image_format format{CL_YUYV_INTEL, CL_UNORM_INT8};
    EXPECT_EQ(CL_SUCCESS, validate(CL_MEM_READ_ONLY, format));
}

TEST_F(ImageDescriptorValidatorFixture, givenPlanarYuvWithoutHostNoAccessFlagWhenValidatingThenReturnsCLInvalidValue) {
    cl_image_format format{CL_NV12_INTEL, CL_UNORM_INT8};
    EXPECT_EQ(CL_INVALID_VALUE, validate(CL_MEM_READ_WRITE, format));
}

TEST_F(ImageDescriptorValidatorFixture, givenPlanarYuvWithUnalignedWidthWhenValidatingThenReturnsCLInvalidImageDescriptor) {
    cl_image_format format{CL_NV12_INTEL, CL_UNORM_INT8};
    imageDesc.image_width = 18;
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, validate(CL_MEM_HOST_NO_ACCESS, format));
}

TEST_F(ImageDescriptorValidatorFixture, givenPlanarYuvWithUnalignedHeightWhenValidatingThenReturnsCLInvalidImageDescriptor) {
    cl_image_format format{CL_NV12_INTEL, CL_UNORM_INT8};
    imageDesc.image_height = 18;
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, validate(CL_MEM_HOST_NO_ACCESS, format));
}

TEST_F(ImageDescriptorValidatorFixture, givenPlanarYuvWithNonTwoDimensionalTypeWhenValidatingThenReturnsCLInvalidImageDescriptor) {
    cl_image_format format{CL_NV12_INTEL, CL_UNORM_INT8};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE3D;
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, validate(CL_MEM_HOST_NO_ACCESS, format));
}

TEST_F(ImageDescriptorValidatorFixture, givenPlanarYuvExceedingDeviceHeightWhenValidatingThenReturnsCLInvalidImageSize) {
    cl_image_format format{CL_NV12_INTEL, CL_UNORM_INT8};
    const auto planarYuvMaxHeight = clDevice->getDeviceInfo().planarYuvMaxHeight;
    ASSERT_GT(planarYuvMaxHeight, 0u);
    ASSERT_LT(planarYuvMaxHeight, clDevice->getSharedDeviceInfo().image2DMaxHeight);

    imageDesc.image_height = alignUp(planarYuvMaxHeight + 1, 4u);
    ASSERT_LE(imageDesc.image_height, clDevice->getSharedDeviceInfo().image2DMaxHeight);
    EXPECT_EQ(CL_INVALID_IMAGE_SIZE, validate(CL_MEM_HOST_NO_ACCESS, format));
}

TEST_F(ImageDescriptorValidatorFixture, givenValidPlanarYuvWhenValidatingThenReturnsSuccess) {
    cl_image_format format{CL_NV12_INTEL, CL_UNORM_INT8};
    EXPECT_EQ(CL_SUCCESS, validate(CL_MEM_HOST_NO_ACCESS, format));
}

} // namespace ult
} // namespace LEO
} // namespace NEO
