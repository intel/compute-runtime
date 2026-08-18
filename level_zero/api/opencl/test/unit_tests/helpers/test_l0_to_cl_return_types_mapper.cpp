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

TEST(L0ToClResultMapperTests, givenNotReadyWhenMapResultThenReturnsCLProfilingInfoNotAvailable) {
    cl_int result = L0ToClResultMapper(ZE_RESULT_NOT_READY);
    EXPECT_EQ(CL_PROFILING_INFO_NOT_AVAILABLE, result);
}

TEST(L0ToClResultMapperTests, givenUnavailableDeviceStatesWhenMapResultThenReturnsCLDeviceNotAvailable) {
    EXPECT_EQ(CL_DEVICE_NOT_AVAILABLE, static_cast<cl_int>(L0ToClResultMapper(ZE_RESULT_ERROR_DEVICE_REQUIRES_RESET)));
    EXPECT_EQ(CL_DEVICE_NOT_AVAILABLE, static_cast<cl_int>(L0ToClResultMapper(ZE_RESULT_ERROR_DEVICE_IN_LOW_POWER_STATE)));
}

TEST(L0ToClResultMapperTests, givenInvalidNativeBinaryWhenMapResultThenReturnsCLInvalidBinary) {
    cl_int result = L0ToClResultMapper(ZE_RESULT_ERROR_INVALID_NATIVE_BINARY);
    EXPECT_EQ(CL_INVALID_BINARY, result);
}

TEST(L0ToClResultMapperTests, givenSizeErrorsWhenMapResultThenReturnsCLInvalidBufferSize) {
    EXPECT_EQ(CL_INVALID_BUFFER_SIZE, static_cast<cl_int>(L0ToClResultMapper(ZE_RESULT_ERROR_INVALID_SIZE)));
    EXPECT_EQ(CL_INVALID_BUFFER_SIZE, static_cast<cl_int>(L0ToClResultMapper(ZE_RESULT_ERROR_UNSUPPORTED_SIZE)));
}

TEST(L0ToClResultMapperTests, givenUnsupportedFeatureWhenMapResultThenReturnsCLInvalidOperation) {
    cl_int result = L0ToClResultMapper(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    EXPECT_EQ(CL_INVALID_OPERATION, result);
}

TEST(L0ToClResultMapperTests, givenInvalidGlobalWidthDimensionWhenMapResultThenReturnsCLInvalidGlobalWorkSize) {
    cl_int result = L0ToClResultMapper(ZE_RESULT_ERROR_INVALID_GLOBAL_WIDTH_DIMENSION);
    EXPECT_EQ(CL_INVALID_GLOBAL_WORK_SIZE, result);
}

TEST(L0ToClResultMapperTests, givenNameLookupFailuresWhenMapResultThenReturnsCLInvalidKernelName) {
    EXPECT_EQ(CL_INVALID_KERNEL_NAME, static_cast<cl_int>(L0ToClResultMapper(ZE_RESULT_ERROR_INVALID_GLOBAL_NAME)));
    EXPECT_EQ(CL_INVALID_KERNEL_NAME, static_cast<cl_int>(L0ToClResultMapper(ZE_RESULT_ERROR_INVALID_FUNCTION_NAME)));
}

TEST(L0ToClResultMapperTests, givenInvalidKernelAttributeValueWhenMapResultThenReturnsCLInvalidKernel) {
    cl_int result = L0ToClResultMapper(ZE_RESULT_ERROR_INVALID_KERNEL_ATTRIBUTE_VALUE);
    EXPECT_EQ(CL_INVALID_KERNEL, result);
}

TEST(L0ToClResultMapperTests, givenInvalidModuleUnlinkedWhenMapResultThenReturnsCLInvalidProgram) {
    cl_int result = L0ToClResultMapper(ZE_RESULT_ERROR_INVALID_MODULE_UNLINKED);
    EXPECT_EQ(CL_INVALID_PROGRAM, result);
}

TEST(L0ToClResultMapperTests, givenInvalidCommandListTypeWhenMapResultThenReturnsCLInvalidCommandQueue) {
    cl_int result = L0ToClResultMapper(ZE_RESULT_ERROR_INVALID_COMMAND_LIST_TYPE);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, result);
}

TEST(L0ToClResultMapperTests, givenGenericArgumentErrorsWhenMapResultThenReturnsCLInvalidValue) {
    const ze_result_t genericErrors[] = {ZE_RESULT_ERROR_UNINITIALIZED,
                                         ZE_RESULT_ERROR_INVALID_ARGUMENT,
                                         ZE_RESULT_ERROR_INVALID_NULL_HANDLE,
                                         ZE_RESULT_ERROR_INVALID_NULL_POINTER,
                                         ZE_RESULT_ERROR_INVALID_ENUMERATION,
                                         ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION,
                                         ZE_RESULT_ERROR_UNSUPPORTED_ALIGNMENT,
                                         ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE,
                                         ZE_RESULT_ERROR_NOT_AVAILABLE};

    for (auto zeResult : genericErrors) {
        EXPECT_EQ(CL_INVALID_VALUE, static_cast<cl_int>(L0ToClResultMapper(zeResult))) << "unexpected mapping for " << static_cast<uint32_t>(zeResult);
    }
}

TEST(L0ToClResultMapperTests, givenSuccessWhenMapResultThenTheFastPathAndTableAgree) {
    static_assert(static_cast<cl_int>(L0ToClResultMapper(ZE_RESULT_SUCCESS)) == CL_SUCCESS);
    EXPECT_EQ(CL_SUCCESS, static_cast<cl_int>(L0ToClResultMapper(ZE_RESULT_SUCCESS)));
}

TEST(L0ToClResultMapperTests, givenUnmappedResultsWhenMapResultThenReturnsCLInvalidValue) {
    EXPECT_EQ(CL_INVALID_VALUE, static_cast<cl_int>(L0ToClResultMapper(ZE_RESULT_ERROR_UNKNOWN)));
    EXPECT_EQ(CL_INVALID_VALUE, static_cast<cl_int>(L0ToClResultMapper(static_cast<ze_result_t>(0x7ffffffe))));
}

} // namespace ult
} // namespace LEO
} // namespace NEO
