/*
 * Copyright (c) 2017, Intel Corporation
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
