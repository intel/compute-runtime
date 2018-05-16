/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

    auto program = new T(pContext);

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
        program = new T(pContext);
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
    cl_int *errcodeRet) {
    cl_int retVal = CL_SUCCESS;
    T *program = nullptr;

    if (nullTerminatedString == nullptr) {
        retVal = CL_INVALID_VALUE;
    }

    if (retVal == CL_SUCCESS) {
        program = new T();
        program->setSource((char *)nullTerminatedString);
        program->context = context;
        if (program->context) {
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
T *Program::createFromIL(Context *ctx,
                         const void *il,
                         size_t length,
                         cl_int &errcodeRet) {
    errcodeRet = CL_SUCCESS;

    if ((il == nullptr) || (length == 0)) {
        errcodeRet = CL_INVALID_BINARY;
        return nullptr;
    }

    T *program = new T(ctx);
    errcodeRet = program->createProgramFromBinary(il, length);
    if (errcodeRet != CL_SUCCESS) {
        delete program;
        program = nullptr;
    }

    return program;
}
} // namespace OCLRT
