/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/helpers/leo_error_mappers.h"

#include "CL/cl.h"
#include "CL/cl_ext.h"

namespace NEO {
namespace LEO {
namespace ult {

template <typename T>
cl_int nullMapped() {
    return NullObjectErrorMapper<T>::retVal;
}

template <typename T>
cl_int invalidMapped() {
    return InvalidObjectErrorMapper<T>::retVal;
}

TEST(NullObjectErrorMapperTests, givenSpecializedHandleTypesWhenQueryingRetValThenReturnsMatchingClError) {
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, nullMapped<cl_command_queue>());
    EXPECT_EQ(CL_INVALID_CONTEXT, nullMapped<cl_context>());
    EXPECT_EQ(CL_INVALID_DEVICE, nullMapped<cl_device_id>());
    EXPECT_EQ(CL_INVALID_EVENT, nullMapped<cl_event>());
    EXPECT_EQ(CL_INVALID_KERNEL, nullMapped<cl_kernel>());
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, nullMapped<cl_mem>());
    EXPECT_EQ(CL_INVALID_PLATFORM, nullMapped<cl_platform_id>());
    EXPECT_EQ(CL_INVALID_PROGRAM, nullMapped<cl_program>());
    EXPECT_EQ(CL_INVALID_SAMPLER, nullMapped<cl_sampler>());
}

TEST(NullObjectErrorMapperTests, givenVoidPointerTypesWhenQueryingRetValThenReturnsCLInvalidValue) {
    EXPECT_EQ(CL_INVALID_VALUE, nullMapped<void *>());
    EXPECT_EQ(CL_INVALID_VALUE, nullMapped<const void *>());
}

TEST(NullObjectErrorMapperTests, givenUnspecializedTypeWhenQueryingRetValThenReturnsSuccess) {
    EXPECT_EQ(CL_SUCCESS, nullMapped<int>());
    EXPECT_EQ(CL_SUCCESS, nullMapped<size_t>());
    EXPECT_EQ(CL_SUCCESS, nullMapped<cl_int *>());
    EXPECT_EQ(CL_SUCCESS, nullMapped<cl_command_buffer_khr>());
}

TEST(InvalidObjectErrorMapperTests, givenSpecializedHandleTypesWhenQueryingRetValThenMatchesNullObjectMapper) {
    EXPECT_EQ(nullMapped<cl_command_queue>(), invalidMapped<cl_command_queue>());
    EXPECT_EQ(nullMapped<cl_context>(), invalidMapped<cl_context>());
    EXPECT_EQ(nullMapped<cl_device_id>(), invalidMapped<cl_device_id>());
    EXPECT_EQ(nullMapped<cl_platform_id>(), invalidMapped<cl_platform_id>());
    EXPECT_EQ(nullMapped<cl_event>(), invalidMapped<cl_event>());
    EXPECT_EQ(nullMapped<cl_mem>(), invalidMapped<cl_mem>());
    EXPECT_EQ(nullMapped<cl_sampler>(), invalidMapped<cl_sampler>());
    EXPECT_EQ(nullMapped<cl_program>(), invalidMapped<cl_program>());
    EXPECT_EQ(nullMapped<cl_kernel>(), invalidMapped<cl_kernel>());
}

TEST(InvalidObjectErrorMapperTests, givenTypeWithoutValidationWhenQueryingRetValThenReturnsSuccess) {
    EXPECT_EQ(CL_SUCCESS, invalidMapped<void *>());
    EXPECT_EQ(CL_SUCCESS, invalidMapped<const void *>());
    EXPECT_EQ(CL_SUCCESS, invalidMapped<int>());
    EXPECT_EQ(CL_SUCCESS, invalidMapped<cl_command_buffer_khr>());
}

TEST(InvalidObjectErrorMapperTests, givenValidatedHandleTypesWhenComparedWithNonValidatedThenOnlyValidatedReportErrors) {
    EXPECT_NE(CL_SUCCESS, invalidMapped<cl_context>());
    EXPECT_NE(CL_SUCCESS, invalidMapped<cl_mem>());
    EXPECT_EQ(CL_SUCCESS, invalidMapped<void *>());
}

} // namespace ult
} // namespace LEO
} // namespace NEO
