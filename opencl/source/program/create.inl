/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/memory_manager/memory_constants.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/helpers/string_helpers.h"
#include "opencl/source/platform/platform.h"
#include "opencl/source/program/program.h"

#include "compiler_options.h"

namespace NEO {

template <typename T>
T *Program::create(
    cl_context context,
    cl_uint numDevices,
    const cl_device_id *deviceList,
    const size_t *lengths,
    const unsigned char **binaries,
    cl_int *binaryStatus,
    cl_int &errcodeRet) {
    auto pContext = castToObject<Context>(context);
    DEBUG_BREAK_IF(!pContext);

    auto program = new T(*pContext->getDevice(0)->getExecutionEnvironment(), pContext, false, &pContext->getDevice(0)->getDevice());

    auto retVal = program->createProgramFromBinary(binaries[0], lengths[0]);

    program->createdFrom = CreatedFrom::BINARY;

    if (binaryStatus) {
        DEBUG_BREAK_IF(retVal != CL_SUCCESS);
        *binaryStatus = CL_SUCCESS;
    }

    if (retVal != CL_SUCCESS) {
        delete program;
        program = nullptr;
    }

    errcodeRet = retVal;
    return program;
}

template <typename T>
T *Program::create(
    cl_context context,
    cl_uint count,
    const char **strings,
    const size_t *lengths,
    cl_int &errcodeRet) {
    std::string combinedString;
    size_t combinedStringSize = 0;
    T *program = nullptr;
    auto pContext = castToObject<Context>(context);
    DEBUG_BREAK_IF(!pContext);

    auto retVal = createCombinedString(
        combinedString,
        combinedStringSize,
        count,
        strings,
        lengths);

    if (CL_SUCCESS == retVal) {
        program = new T(*pContext->getDevice(0)->getExecutionEnvironment(), pContext, false, &pContext->getDevice(0)->getDevice());
        program->sourceCode.swap(combinedString);
        program->createdFrom = CreatedFrom::SOURCE;
    }

    errcodeRet = retVal;
    return program;
}

template <typename T>
T *Program::create(
    const char *nullTerminatedString,
    Context *context,
    ClDevice &device,
    bool isBuiltIn,
    cl_int *errcodeRet) {
    cl_int retVal = CL_SUCCESS;
    T *program = nullptr;

    if (nullTerminatedString == nullptr) {
        retVal = CL_INVALID_VALUE;
    }

    if (retVal == CL_SUCCESS) {
        program = new T(*device.getExecutionEnvironment());
        program->sourceCode = nullTerminatedString;
        program->createdFrom = CreatedFrom::SOURCE;
        program->context = context;
        program->isBuiltIn = isBuiltIn;
        if (program->context && !program->isBuiltIn) {
            program->context->incRefInternal();
        }
        program->pDevice = &device.getDevice();
        program->numDevices = 1;
        if (is32bit || DebugManager.flags.DisableStatelessToStatefulOptimization.get() || device.areSharedSystemAllocationsAllowed()) {
            CompilerOptions::concatenateAppend(program->internalOptions, CompilerOptions::greaterThan4gbBuffersRequired);
        }
    }

    if (errcodeRet) {
        *errcodeRet = retVal;
    }

    return program;
}

template <typename T>
T *Program::create(
    const char *nullTerminatedString,
    Context *context,
    Device &device,
    bool isBuiltIn,
    cl_int *errcodeRet) {
    return Program::create<T>(nullTerminatedString, context, *device.getSpecializedDevice<ClDevice>(), isBuiltIn, errcodeRet);
}

template <typename T>
T *Program::createFromGenBinary(
    ExecutionEnvironment &executionEnvironment,
    Context *context,
    const void *binary,
    size_t size,
    bool isBuiltIn,
    cl_int *errcodeRet,
    Device *device) {
    cl_int retVal = CL_SUCCESS;
    T *program = nullptr;

    if ((binary == nullptr) || (size == 0)) {
        retVal = CL_INVALID_VALUE;
    }

    if (CL_SUCCESS == retVal) {
        program = new T(executionEnvironment, context, isBuiltIn, device);
        program->numDevices = 1;
        program->replaceDeviceBinary(makeCopy(binary, size), size);
        program->isCreatedFromBinary = true;
        program->programBinaryType = CL_PROGRAM_BINARY_TYPE_EXECUTABLE;
        program->buildStatus = CL_BUILD_SUCCESS;
        program->createdFrom = CreatedFrom::BINARY;
    }

    if (errcodeRet) {
        *errcodeRet = retVal;
    }

    return program;
}

template <typename T>
T *Program::createFromIL(Context *ctx,
                         const void *il,
                         size_t length,
                         cl_int &errcodeRet) {
    errcodeRet = CL_SUCCESS;

    if ((il == nullptr) || (length == 0)) {
        errcodeRet = CL_INVALID_BINARY;
        return nullptr;
    }

    T *program = new T(*ctx->getDevice(0)->getExecutionEnvironment(), ctx, false, &ctx->getDevice(0)->getDevice());
    errcodeRet = program->createProgramFromBinary(il, length);
    program->createdFrom = CreatedFrom::IL;

    if (errcodeRet != CL_SUCCESS) {
        delete program;
        program = nullptr;
    }

    return program;
}
} // namespace NEO
