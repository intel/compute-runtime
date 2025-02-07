/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string_helpers.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/platform/platform.h"
#include "opencl/source/program/program.h"

namespace NEO {

template <typename T>
T *Program::create(
    Context *pContext,
    const ClDeviceVector &deviceVector,
    const size_t *lengths,
    const unsigned char **binaries,
    cl_int *binaryStatus,
    cl_int &errcodeRet) {
    auto program = new T(pContext, false, deviceVector);

    cl_int retVal = CL_INVALID_PROGRAM;

    for (auto i = 0u; i < deviceVector.size(); i++) {
        auto device = deviceVector[i];
        retVal = program->createProgramFromBinary(binaries[i], lengths[i], *device); // NOLINT(clang-analyzer-core.CallAndMessage)
        if (retVal != CL_SUCCESS) {
            break;
        }
    }
    program->createdFrom = CreatedFrom::binary;

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
    Context *pContext,
    cl_uint count,
    const char **strings,
    const size_t *lengths,
    cl_int &errcodeRet) {
    std::string combinedString;
    size_t combinedStringSize = 0;
    T *program = nullptr;

    auto retVal = StringHelpers::createCombinedString(
        combinedString,
        combinedStringSize,
        count,
        strings,
        lengths);

    if (CL_SUCCESS == retVal) {

        auto ail = pContext->getDevice(0)->getRootDeviceEnvironment().getAILConfigurationHelper();
        if (ail) {
            ail->modifyKernelIfRequired(combinedString);
        }

        program = new T(pContext, false, pContext->getDevices());
        program->sourceCode.swap(combinedString);
        program->createdFrom = CreatedFrom::source;
    }

    errcodeRet = retVal;
    return program;
}

template <typename T>
T *Program::createBuiltInFromSource(
    const char *nullTerminatedString,
    Context *context,
    const ClDeviceVector &deviceVector,
    cl_int *errcodeRet) {
    cl_int retVal = CL_SUCCESS;
    T *program = nullptr;

    if (nullTerminatedString == nullptr) {
        retVal = CL_INVALID_VALUE;
    }

    if (retVal == CL_SUCCESS) {
        program = new T(context, true, deviceVector);
        program->sourceCode = nullTerminatedString;
        program->createdFrom = CreatedFrom::source;
    }

    if (errcodeRet) {
        *errcodeRet = retVal;
    }

    return program;
}

template <typename T>
T *Program::createBuiltInFromGenBinary(
    Context *context,
    const ClDeviceVector &deviceVector,
    const void *binary,
    size_t size,
    cl_int *errcodeRet) {
    cl_int retVal = CL_SUCCESS;
    T *program = nullptr;

    if ((binary == nullptr) || (size == 0)) {
        retVal = CL_INVALID_VALUE;
    }

    if (CL_SUCCESS == retVal) {

        program = new T(context, true, deviceVector);
        for (const auto &device : deviceVector) {
            if (program->buildInfos[device->getRootDeviceIndex()].packedDeviceBinarySize == 0) {
                program->replaceDeviceBinary(std::move(makeCopy(binary, size)), size, device->getRootDeviceIndex());
            }
        }
        program->setBuildStatusSuccess(deviceVector, CL_PROGRAM_BINARY_TYPE_EXECUTABLE);
        program->isCreatedFromBinary = true;
        program->createdFrom = CreatedFrom::binary;
    }

    if (errcodeRet) {
        *errcodeRet = retVal;
    }

    return program;
}

template <typename T>
T *Program::createFromIL(Context *context,
                         const void *il,
                         size_t length,
                         cl_int &errcodeRet) {
    errcodeRet = CL_SUCCESS;

    if ((il == nullptr) || (length == 0)) {
        errcodeRet = CL_INVALID_BINARY;
        return nullptr;
    }

    auto deviceVector = context->getDevices();
    T *program = new T(context, false, deviceVector);
    for (const auto &device : deviceVector) {
        errcodeRet = program->createProgramFromBinary(il, length, *device);
        if (errcodeRet != CL_SUCCESS) {
            break;
        }
    }
    program->createdFrom = CreatedFrom::il;

    if (errcodeRet != CL_SUCCESS) {
        delete program;
        program = nullptr;
    }

    return program;
}
} // namespace NEO
