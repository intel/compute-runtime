/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include <level_zero/ze_api.h>

#include "CL/cl.h"

namespace NEO {
namespace LEO {
namespace ult {

TEST(L0ToClResultMapperTests, givenSuccessWhenMapResultThenReturnsSuccess) {
    cl_int result = L0ToClResultMapper(ZE_RESULT_SUCCESS);
    EXPECT_EQ(CL_SUCCESS, result);
}

TEST(L0ToClResultMapperTests, givenOutOfHostMemoryWhenMapResultThenReturnsCLOutOfHostMemory) {
    cl_int result = L0ToClResultMapper(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY);
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, result);
}

TEST(L0ToClResultMapperTests, givenOutOfDeviceMemoryWhenMapResultThenReturnsCLOutOfResources) {
    cl_int result = L0ToClResultMapper(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY);
    EXPECT_EQ(CL_OUT_OF_RESOURCES, result);
}

TEST(L0ToClResultMapperTests, givenModuleBuildFailureWhenMapResultThenReturnsCLBuildProgramFailure) {
    cl_int result = L0ToClResultMapper(ZE_RESULT_ERROR_MODULE_BUILD_FAILURE);
    EXPECT_EQ(CL_BUILD_PROGRAM_FAILURE, result);
}

TEST(L0ToClResultMapperTests, givenModuleLinkFailureWhenMapResultThenReturnsCLLinkProgramFailure) {
    cl_int result = L0ToClResultMapper(ZE_RESULT_ERROR_MODULE_LINK_FAILURE);
    EXPECT_EQ(CL_LINK_PROGRAM_FAILURE, result);
}

TEST(L0ToClResultMapperTests, givenInvalidKernelNameWhenMapResultThenReturnsCLInvalidKernelName) {
    cl_int result = L0ToClResultMapper(ZE_RESULT_ERROR_INVALID_KERNEL_NAME);
    EXPECT_EQ(CL_INVALID_KERNEL_NAME, result);
}

TEST(L0ToClResultMapperTests, givenInvalidGroupSizeDimensionWhenMapResultThenReturnsCLInvalidWorkGroupSize) {
    cl_int result = L0ToClResultMapper(ZE_RESULT_ERROR_INVALID_GROUP_SIZE_DIMENSION);
    EXPECT_EQ(CL_INVALID_WORK_GROUP_SIZE, result);
}

TEST(L0ToClResultMapperTests, givenInvalidKernelArgumentIndexWhenMapResultThenReturnsCLInvalidArgIndex) {
    cl_int result = L0ToClResultMapper(ZE_RESULT_ERROR_INVALID_KERNEL_ARGUMENT_INDEX);
    EXPECT_EQ(CL_INVALID_ARG_INDEX, result);
}

TEST(L0ToClResultMapperTests, givenInvalidKernelArgumentSizeWhenMapResultThenReturnsCLInvalidArgSize) {
    cl_int result = L0ToClResultMapper(ZE_RESULT_ERROR_INVALID_KERNEL_ARGUMENT_SIZE);
    EXPECT_EQ(CL_INVALID_ARG_SIZE, result);
}

TEST(L0ToClResultMapperTests, givenDeviceLostWhenMapResultThenReturnsCLDeviceNotAvailable) {
    cl_int result = L0ToClResultMapper(ZE_RESULT_ERROR_DEVICE_LOST);
    EXPECT_EQ(CL_DEVICE_NOT_AVAILABLE, result);
}

TEST(L0ToClResultMapperTests, givenUnsupportedImageFormatWhenMapResultThenReturnsCLImageFormatNotSupported) {
    cl_int result = L0ToClResultMapper(ZE_RESULT_ERROR_UNSUPPORTED_IMAGE_FORMAT);
    EXPECT_EQ(CL_IMAGE_FORMAT_NOT_SUPPORTED, result);
}

TEST(L0ToClResultMapperTests, givenOverlappingRegionsWhenMapResultThenReturnsCLMemCopyOverlap) {
    cl_int result = L0ToClResultMapper(ZE_RESULT_ERROR_OVERLAPPING_REGIONS);
    EXPECT_EQ(CL_MEM_COPY_OVERLAP, result);
}

TEST(L0ToClResultMapperTests, givenUnknownErrorWhenMapResultThenReturnsCLInvalidValue) {
    cl_int result = L0ToClResultMapper(ZE_RESULT_FORCE_UINT32);
    EXPECT_EQ(CL_INVALID_VALUE, result);
}

TEST(L0ToClResultMapperTests, givenInvalidSynchronizationObjectWhenMapResultThenReturnsCLInvalidEvent) {
    cl_int result = L0ToClResultMapper(ZE_RESULT_ERROR_INVALID_SYNCHRONIZATION_OBJECT);
    EXPECT_EQ(CL_INVALID_EVENT, result);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
