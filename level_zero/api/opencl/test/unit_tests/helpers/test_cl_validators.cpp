/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/helpers/cl_validators.h"
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

} // namespace ult
} // namespace LEO
} // namespace NEO
