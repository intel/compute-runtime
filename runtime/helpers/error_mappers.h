/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

template <typename Type>
struct NullObjectErrorMapper {
    static const cl_int retVal = CL_SUCCESS;
};

// clang-format off
template <> struct NullObjectErrorMapper<cl_command_queue> { static const cl_int retVal = CL_INVALID_COMMAND_QUEUE; };
template <> struct NullObjectErrorMapper<cl_context> { static const cl_int retVal = CL_INVALID_CONTEXT; };
template <> struct NullObjectErrorMapper<cl_device_id> { static const cl_int retVal = CL_INVALID_DEVICE; };
template <> struct NullObjectErrorMapper<cl_event> { static const cl_int retVal = CL_INVALID_EVENT; };
template <> struct NullObjectErrorMapper<cl_kernel> { static const cl_int retVal = CL_INVALID_KERNEL; };
template <> struct NullObjectErrorMapper<cl_mem> { static const cl_int retVal = CL_INVALID_MEM_OBJECT; };
template <> struct NullObjectErrorMapper<cl_platform_id> { static const cl_int retVal = CL_INVALID_PLATFORM; };
template <> struct NullObjectErrorMapper<cl_program> { static const cl_int retVal = CL_INVALID_PROGRAM; };
template <> struct NullObjectErrorMapper<cl_sampler> { static const cl_int retVal = CL_INVALID_SAMPLER; };
template <> struct NullObjectErrorMapper<void *> { static const cl_int retVal = CL_INVALID_VALUE; };
template <> struct NullObjectErrorMapper<const void *> { static const cl_int retVal = CL_INVALID_VALUE; };
// clang-format on

// defaults to CL_SUCCESS
template <typename Type>
struct InvalidObjectErrorMapper {
    static const cl_int retVal = CL_SUCCESS;
};

// clang-format off
// Special case the ones we do have proper validation for.
template <> struct InvalidObjectErrorMapper<cl_command_queue> { static const cl_int retVal = NullObjectErrorMapper<cl_command_queue>::retVal; };
template <> struct InvalidObjectErrorMapper<cl_context> { static const cl_int retVal = NullObjectErrorMapper<cl_context>::retVal; };
template <> struct InvalidObjectErrorMapper<cl_device_id> { static const cl_int retVal = NullObjectErrorMapper<cl_device_id>::retVal; };
template <> struct InvalidObjectErrorMapper<cl_platform_id> { static const cl_int retVal = NullObjectErrorMapper<cl_platform_id>::retVal; };
template <> struct InvalidObjectErrorMapper<cl_event> { static const cl_int retVal = NullObjectErrorMapper<cl_event>::retVal; };
template <> struct InvalidObjectErrorMapper<cl_mem> { static const cl_int retVal = NullObjectErrorMapper<cl_mem>::retVal; };
template <> struct InvalidObjectErrorMapper<cl_sampler> { static const cl_int retVal = NullObjectErrorMapper<cl_sampler>::retVal; };
template <> struct InvalidObjectErrorMapper<cl_program> { static const cl_int retVal = NullObjectErrorMapper<cl_program>::retVal; };
template <> struct InvalidObjectErrorMapper<cl_kernel> { static const cl_int retVal = NullObjectErrorMapper<cl_kernel>::retVal; };
// clang-format on
