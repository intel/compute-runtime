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
#include "runtime/api/cl_types.h"
#include "runtime/helpers/base_object.h"
#include "runtime/helpers/error_mappers.h"
#include <utility>

namespace OCLRT {

// Provide some aggregators...
typedef std::pair<uint32_t, const cl_event *> EventWaitList;
typedef std::pair<uint32_t, const cl_device_id *> DeviceList;

// Custom validators
enum NonZeroBufferSize : size_t;
enum PatternSize : size_t;

template <typename CLType, typename InternalType>
CLType WithCastToInternal(CLType clObject, InternalType **internalObject) {
    *internalObject = OCLRT::castToObject<InternalType>(clObject);
    return (*internalObject) ? clObject : nullptr;
}

// This is the default instance of validateObject.
// It should be specialized for specific types.
template <typename Type>
inline cl_int validateObject(Type object) {
    return CL_SUCCESS;
}

// Example of specialization.
cl_int validateObject(void *ptr);
cl_int validateObject(cl_context context);
cl_int validateObject(cl_device_id device);
cl_int validateObject(cl_platform_id platform);
cl_int validateObject(cl_command_queue commandQueue);
cl_int validateObject(cl_event platform);
cl_int validateObject(cl_mem mem);
cl_int validateObject(cl_sampler sampler);
cl_int validateObject(cl_program program);
cl_int validateObject(cl_kernel kernel);
cl_int validateObject(const EventWaitList &eventWaitList);
cl_int validateObject(const DeviceList &deviceList);
cl_int validateObject(const NonZeroBufferSize &nzbs);
cl_int validateObject(const PatternSize &ps);

// This is the sentinel for the follow variadic template definition.
inline cl_int validateObjects() {
    return CL_SUCCESS;
}

// This provides variadic object validation.
// It automatically checks for nullptrs and then passes
// onto type specific validator.
template <typename Type, typename... Types>
inline cl_int validateObjects(const Type &object, Types... rest) {
    auto retVal = validateObject(object);
    return CL_SUCCESS != retVal ? retVal : validateObjects(rest...);
}

template <typename Type, typename... Types>
inline cl_int validateObjects(Type *object, Types... rest) {
    auto retVal = object ? validateObject(object) : NullObjectErrorMapper<Type *>::retVal;
    return CL_SUCCESS != retVal ? retVal : validateObjects(rest...);
}

template <typename T = void>
bool areNotNullptr() {
    return true;
}

template <typename T, typename... RT>
bool areNotNullptr(T t, RT... rt) {
    return (t != nullptr) && areNotNullptr<RT...>(rt...);
}

cl_int validateYuvOperation(const size_t *origin, const size_t *region);
bool IsPackedYuvImage(const cl_image_format *imageFormat);
bool IsNV12Image(const cl_image_format *imageFormat);
}
