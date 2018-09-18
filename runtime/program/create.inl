/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/program/program.h"
#include "runtime/context/context.h"

namespace OCLRT {

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

    auto program = new T(*pContext->getDevice(0)->getExecutionEnvironment(), pContext, false);

    auto retVal = program->createProgramFromBinary(binaries[0], lengths[0]);

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
        program = new T(*pContext->getDevice(0)->getExecutionEnvironment(), pContext, false);
        program->sourceCode.swap(combinedString);
    }

    errcodeRet = retVal;
    return program;
}

template <typename T>
T *Program::create(
    const char *nullTerminatedString,
    Context *context,
    Device &device,
    bool isBuiltIn,
    cl_int *errcodeRet) {
    cl_int retVal = CL_SUCCESS;
    T *program = nullptr;

    if (nullTerminatedString == nullptr) {
        retVal = CL_INVALID_VALUE;
    }

    if (retVal == CL_SUCCESS) {
        program = new T(*device.getExecutionEnvironment());
        program->setSource((char *)nullTerminatedString);
        program->context = context;
        program->isBuiltIn = isBuiltIn;
        if (program->context && !program->isBuiltIn) {
            program->context->incRefInternal();
        }
        program->pDevice = &device;
        program->numDevices = 1;
        if (is32bit || DebugManager.flags.DisableStatelessToStatefulOptimization.get()) {
            program->internalOptions += "-cl-intel-greater-than-4GB-buffer-required";
        }
    }

    if (errcodeRet) {
        *errcodeRet = retVal;
    }

    return program;
}

template <typename T>
T *Program::createFromGenBinary(
    ExecutionEnvironment &executionEnvironment,
    Context *context,
    const void *binary,
    size_t size,
    bool isBuiltIn,
    cl_int *errcodeRet) {
    cl_int retVal = CL_SUCCESS;
    T *program = nullptr;

    if ((binary == nullptr) || (size == 0)) {
        retVal = CL_INVALID_VALUE;
    }

    if (CL_SUCCESS == retVal) {
        program = new T(executionEnvironment, context, isBuiltIn);
        program->numDevices = 1;
        program->storeGenBinary(binary, size);
        program->isCreatedFromBinary = true;
        program->programBinaryType = CL_PROGRAM_BINARY_TYPE_EXECUTABLE;
        program->isProgramBinaryResolved = true;
        program->buildStatus = CL_BUILD_SUCCESS;
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

    T *program = new T(*ctx->getDevice(0)->getExecutionEnvironment(), ctx, false);
    errcodeRet = program->createProgramFromBinary(il, length);
    if (errcodeRet != CL_SUCCESS) {
        delete program;
        program = nullptr;
    }

    return program;
}
} // namespace OCLRT
