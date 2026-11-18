/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/get_info.h"

#include "level_zero/api/opencl/source/api/leo_api.h"
#include "level_zero/api/opencl/source/tracing/leo_tracing_notify.h"

#include "CL/cl.h"

cl_accelerator_intel CL_API_CALL clCreateAcceleratorINTEL(
    cl_context context,
    cl_accelerator_type_intel acceleratorType,
    size_t descriptorSize,
    const void *descriptor,
    cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateAcceleratorINTEL, &context, &acceleratorType, &descriptorSize, &descriptor, &errcodeRet);
    ErrorCodeHelper err(errcodeRet, CL_INVALID_OPERATION);
    return tracingScope.exit(nullptr);
}

cl_int CL_API_CALL clRetainAcceleratorINTEL(
    cl_accelerator_intel accelerator) {
    TRACING_ENTER(ClRetainAcceleratorINTEL, &accelerator);
    return tracingScope.exit(CL_INVALID_OPERATION);
}

cl_int CL_API_CALL clGetAcceleratorInfoINTEL(
    cl_accelerator_intel accelerator,
    cl_accelerator_info_intel paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetAcceleratorInfoINTEL, &accelerator, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    return tracingScope.exit(CL_INVALID_OPERATION);
}

cl_int CL_API_CALL clReleaseAcceleratorINTEL(
    cl_accelerator_intel accelerator) {
    TRACING_ENTER(ClReleaseAcceleratorINTEL, &accelerator);
    return tracingScope.exit(CL_INVALID_OPERATION);
}
