/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/get_info.h"
#include "shared/source/program/kernel_info.h"

#include "level_zero/api/opencl/source/api/api.h"
#include "level_zero/api/opencl/source/context/context.h"
#include "level_zero/api/opencl/source/helpers/base_object.h"
#include "level_zero/api/opencl/source/helpers/cl_validators.h"
#include "level_zero/api/opencl/source/program/program.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver.h"
#include <level_zero/ze_api.h>

#include "CL/cl.h"

cl_program CL_API_CALL clCreateProgramWithSource(cl_context context,
                                                 cl_uint count,
                                                 const char **strings,
                                                 const size_t *lengths,
                                                 cl_int *errcodeRet) {
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    auto [retVal, pContext] = NEO::LEO::validateAndCast(std::make_tuple(context), std::make_tuple(const_cast<char **>(strings)));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        err.set(retVal);
        return nullptr;
    }

    if (count == 0) [[unlikely]] {
        err.set(CL_INVALID_VALUE);
        return nullptr;
    }

    return new NEO::LEO::Program(pContext, count, strings, lengths);
}

cl_program CL_API_CALL clCreateProgramWithBinary(cl_context context,
                                                 cl_uint numDevices,
                                                 const cl_device_id *deviceList,
                                                 const size_t *lengths,
                                                 const unsigned char **binaries,
                                                 cl_int *binaryStatus,
                                                 cl_int *errcodeRet) {
    auto [status, pContext] = NEO::LEO::validateAndCast(std::make_tuple(context));
    if (status != CL_SUCCESS) {
        ErrorCodeHelper(errcodeRet, status);
        if (binaryStatus) {
            binaryStatus[0] = status;
        }
        return new NEO::LEO::Program(nullptr);
    }

    if (lengths == nullptr ||
        lengths[0] == 0u ||
        binaries == nullptr ||
        binaries[0] == nullptr ||
        numDevices == 0u ||
        deviceList == nullptr) {
        status = CL_INVALID_VALUE;
    }

    auto pProgram = new NEO::LEO::Program(pContext);
    if (CL_SUCCESS == status) {
        for (cl_uint i = 0; i < numDevices; ++i) {
            auto deviceStatus = pProgram->createFromBinaryOrIl(deviceList[i], lengths[i], binaries[i]);
            if (binaryStatus) {
                binaryStatus[i] = deviceStatus;
            }
            if (CL_SUCCESS != deviceStatus) {
                status = deviceStatus;
                break;
            }
        }
    } else if (binaryStatus) {
        binaryStatus[0] = status;
    }
    ErrorCodeHelper(errcodeRet, status);
    return pProgram;
}

cl_program CL_API_CALL clCreateProgramWithIL(cl_context context,
                                             const void *il,
                                             size_t length,
                                             cl_int *errcodeRet) {
    auto [status, pContext] = NEO::LEO::validateAndCast(std::make_tuple(context));
    if (status != CL_SUCCESS) {
        ErrorCodeHelper(errcodeRet, status);
        return new NEO::LEO::Program(nullptr);
    }

    if (il == nullptr) {
        status = CL_INVALID_VALUE;
    } else if (0u == length) {
        status = CL_INVALID_VALUE;
    }

    auto pProgram = new NEO::LEO::Program(pContext);
    if (CL_SUCCESS == status) {
        status = pProgram->createFromBinaryOrIl(pContext->getClDevice(), length, reinterpret_cast<const uint8_t *>(il));
    }

    ErrorCodeHelper(errcodeRet, status);
    return pProgram;
}

cl_program CL_API_CALL clCreateProgramWithILKHR(cl_context context,
                                                const void *il,
                                                size_t length,
                                                cl_int *errcodeRet) {
    return clCreateProgramWithIL(context, il, length, errcodeRet);
}

cl_program CL_API_CALL clCreateProgramWithBuiltInKernels(cl_context context,
                                                         cl_uint numDevices,
                                                         const cl_device_id *deviceList,
                                                         const char *kernelNames,
                                                         cl_int *errcodeRet) {
    return nullptr;
}

cl_int CL_API_CALL clRetainProgram(cl_program program) {
    auto [retVal, pProgram] = NEO::LEO::validateAndCast(std::make_tuple(program));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    pProgram->incRefApi();
    return CL_SUCCESS;
}

cl_int CL_API_CALL clReleaseProgram(cl_program program) {
    auto [retVal, pProgram] = NEO::LEO::validateAndCast(std::make_tuple(program));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    pProgram->decRefApi();
    return CL_SUCCESS;
}

cl_int CL_API_CALL clBuildProgram(cl_program program,
                                  cl_uint numDevices,
                                  const cl_device_id *deviceList,
                                  const char *options,
                                  void(CL_CALLBACK *funcNotify)(cl_program program, void *userData),
                                  void *userData) {
    auto [validationResult, pProgram] = NEO::LEO::validateAndCast(std::make_tuple(program));
    if (validationResult != CL_SUCCESS) [[unlikely]] {
        return validationResult;
    }

    if (!NEO::LEO::Program::isValidCallback(funcNotify, userData)) [[unlikely]] {
        return CL_INVALID_VALUE;
    }

    return pProgram->build(options, funcNotify, userData);
}

cl_int CL_API_CALL clCompileProgram(cl_program program,
                                    cl_uint numDevices,
                                    const cl_device_id *deviceList,
                                    const char *options,
                                    cl_uint numInputHeaders,
                                    const cl_program *inputHeaders,
                                    const char **headerIncludeNames,
                                    void(CL_CALLBACK *funcNotify)(cl_program program, void *userData),
                                    void *userData) {
    auto [validationResult, pProgram] = NEO::LEO::validateAndCast(std::make_tuple(program));
    if (validationResult != CL_SUCCESS) [[unlikely]] {
        return validationResult;
    }

    if (!NEO::LEO::Program::isValidCallback(funcNotify, userData)) [[unlikely]] {
        return CL_INVALID_VALUE;
    }

    return pProgram->compile(options, numInputHeaders, inputHeaders, headerIncludeNames, funcNotify, userData);
}

cl_program CL_API_CALL clLinkProgram(cl_context context,
                                     cl_uint numDevices,
                                     const cl_device_id *deviceList,
                                     const char *options,
                                     cl_uint numInputPrograms,
                                     const cl_program *inputPrograms,
                                     void(CL_CALLBACK *funcNotify)(cl_program program, void *userData),
                                     void *userData,
                                     cl_int *errcodeRet) {
    ErrorCodeHelper errcodeHelper(errcodeRet, CL_SUCCESS);

    auto [retVal, pContext] = NEO::LEO::validateAndCast(std::make_tuple(context));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        errcodeHelper.set(retVal);
        return nullptr;
    }

    if (!NEO::LEO::Program::isValidCallback(funcNotify, userData)) [[unlikely]] {
        errcodeHelper.set(CL_INVALID_VALUE);
        return nullptr;
    }

    auto retProgram = std::make_unique<NEO::LEO::Program>(pContext);
    retProgram->setBuildStatus(CL_BUILD_ERROR);

    if (numInputPrograms == 0 || inputPrograms == nullptr) [[unlikely]] {
        errcodeHelper.set(CL_INVALID_VALUE);
        retProgram->invokeCallback(funcNotify, userData);
        return retProgram.release();
    }

    const cl_int linkResult = retProgram->link(options, numInputPrograms, inputPrograms);
    if (CL_SUCCESS != linkResult) [[unlikely]] {
        errcodeHelper.set(linkResult);
        retProgram->invokeCallback(funcNotify, userData);
        return retProgram.release();
    }

    retProgram->setBuildStatus(CL_BUILD_SUCCESS);
    retProgram->invokeCallback(funcNotify, userData);
    return retProgram.release();
}

cl_int CL_API_CALL clGetProgramInfo(cl_program program,
                                    cl_program_info paramName,
                                    size_t paramValueSize,
                                    void *paramValue,
                                    size_t *paramValueSizeRet) {
    auto [retVal, pProgram] = NEO::LEO::validateAndCast(std::make_tuple(program));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    return pProgram->getInfo(paramName, paramValueSize, paramValue, paramValueSizeRet);
}

cl_int CL_API_CALL clGetProgramBuildInfo(cl_program program,
                                         cl_device_id device,
                                         cl_program_build_info paramName,
                                         size_t paramValueSize,
                                         void *paramValue,
                                         size_t *paramValueSizeRet) {
    auto [retVal, pProgram] = NEO::LEO::validateAndCast(std::make_tuple(program));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    return pProgram->getBuildInfo(device, paramName, paramValueSize, paramValue, paramValueSizeRet);
}

cl_int CL_API_CALL clSetProgramReleaseCallback(cl_program program,
                                               void(CL_CALLBACK *pfnNotify)(cl_program /* program */, void * /* user_data */),
                                               void *userData) {
    return CL_INVALID_OPERATION;
}

cl_int CL_API_CALL clSetProgramSpecializationConstant(cl_program program, cl_uint specId, size_t specSize, const void *specValue) {
    auto [retVal, pProgram] = NEO::LEO::validateAndCast(std::make_tuple(program), std::make_tuple(const_cast<void *>(specValue)));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    return pProgram->setProgramSpecializationConstant(specId, specSize, specValue);
}
