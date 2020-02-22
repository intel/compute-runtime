/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/api/cl_types.h"
#include "opencl/source/helpers/base_object.h"
#include "opencl/source/helpers/error_mappers.h"

#include <utility>

namespace NEO {

// Provide some aggregators...
typedef std::pair<uint32_t, const cl_event *> EventWaitList;
typedef std::pair<uint32_t, const cl_device_id *> DeviceList;
typedef std::pair<uint32_t, const cl_mem *> MemObjList;

// Custom validators
enum NonZeroBufferSize : size_t;
enum PatternSize : size_t;

template <typename CLType, typename InternalType>
CLType WithCastToInternal(CLType clObject, InternalType **internalObject) {
    *internalObject = NEO::castToObject<InternalType>(clObject);
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
cl_int validateObject(const MemObjList &memObjList);
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
} // namespace NEO
