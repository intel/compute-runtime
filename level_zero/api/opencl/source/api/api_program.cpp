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
#include "level_zero/api/opencl/source/tracing/tracing_notify.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver.h"
#include <level_zero/ze_api.h>

#include "CL/cl.h"

cl_program CL_API_CALL clCreateProgramWithSource(cl_context context,
                                                 cl_uint count,
                                                 const char **strings,
                                                 const size_t *lengths,
                                                 cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateProgramWithSource, &context, &count, &strings, &lengths, &errcodeRet);
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    auto [retVal, pContext] = NEO::LEO::validateAndCast(std::make_tuple(context), std::make_tuple(const_cast<char **>(strings)));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        err.set(retVal);
        cl_program tracingRetVal = nullptr;
        TRACING_EXIT(ClCreateProgramWithSource, &tracingRetVal);
        return tracingRetVal;
    }

    if (count == 0) [[unlikely]] {
        err.set(CL_INVALID_VALUE);
        cl_program tracingRetVal = nullptr;
        TRACING_EXIT(ClCreateProgramWithSource, &tracingRetVal);
        return tracingRetVal;
    }

    cl_program tracingRetVal = new NEO::LEO::Program(pContext, count, strings, lengths);
    TRACING_EXIT(ClCreateProgramWithSource, &tracingRetVal);
    return tracingRetVal;
}

cl_program CL_API_CALL clCreateProgramWithBinary(cl_context context,
                                                 cl_uint numDevices,
                                                 const cl_device_id *deviceList,
                                                 const size_t *lengths,
                                                 const unsigned char **binaries,
                                                 cl_int *binaryStatus,
                                                 cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateProgramWithBinary, &context, &numDevices, &deviceList, &lengths, &binaries, &binaryStatus, &errcodeRet);
    auto [status, pContext] = NEO::LEO::validateAndCast(std::make_tuple(context));
    if (status != CL_SUCCESS) {
        ErrorCodeHelper(errcodeRet, status);
        if (binaryStatus) {
            binaryStatus[0] = status;
        }
        cl_program tracingRetVal = new NEO::LEO::Program(nullptr);
        TRACING_EXIT(ClCreateProgramWithBinary, &tracingRetVal);
        return tracingRetVal;
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
    cl_program tracingRetVal = pProgram;
    TRACING_EXIT(ClCreateProgramWithBinary, &tracingRetVal);
    return tracingRetVal;
}

cl_program CL_API_CALL clCreateProgramWithIL(cl_context context,
                                             const void *il,
                                             size_t length,
                                             cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateProgramWithIl, &context, &il, &length, &errcodeRet);
    auto [status, pContext] = NEO::LEO::validateAndCast(std::make_tuple(context));
    if (status != CL_SUCCESS) {
        ErrorCodeHelper(errcodeRet, status);
        cl_program tracingRetVal = new NEO::LEO::Program(nullptr);
        TRACING_EXIT(ClCreateProgramWithIl, &tracingRetVal);
        return tracingRetVal;
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
    cl_program tracingRetVal = pProgram;
    TRACING_EXIT(ClCreateProgramWithIl, &tracingRetVal);
    return tracingRetVal;
}

cl_program CL_API_CALL clCreateProgramWithILKHR(cl_context context,
                                                const void *il,
                                                size_t length,
                                                cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateProgramWithILKHR, &context, &il, &length, &errcodeRet);
    cl_program tracingRetVal = clCreateProgramWithIL(context, il, length, errcodeRet);
    TRACING_EXIT(ClCreateProgramWithILKHR, &tracingRetVal);
    return tracingRetVal;
}

cl_program CL_API_CALL clCreateProgramWithBuiltInKernels(cl_context context,
                                                         cl_uint numDevices,
                                                         const cl_device_id *deviceList,
                                                         const char *kernelNames,
                                                         cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateProgramWithBuiltInKernels, &context, &numDevices, &deviceList, &kernelNames, &errcodeRet);
    cl_program tracingRetVal = nullptr;
    TRACING_EXIT(ClCreateProgramWithBuiltInKernels, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clRetainProgram(cl_program program) {
    TRACING_ENTER(ClRetainProgram, &program);
    auto [retVal, pProgram] = NEO::LEO::validateAndCast(std::make_tuple(program));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClRetainProgram, &retVal);
        return retVal;
    }

    pProgram->incRefApi();
    cl_int tracingRetVal = CL_SUCCESS;
    TRACING_EXIT(ClRetainProgram, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clReleaseProgram(cl_program program) {
    TRACING_ENTER(ClReleaseProgram, &program);
    auto [retVal, pProgram] = NEO::LEO::validateAndCast(std::make_tuple(program));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClReleaseProgram, &retVal);
        return retVal;
    }

    pProgram->decRefApi();
    cl_int tracingRetVal = CL_SUCCESS;
    TRACING_EXIT(ClReleaseProgram, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clBuildProgram(cl_program program,
                                  cl_uint numDevices,
                                  const cl_device_id *deviceList,
                                  const char *options,
                                  void(CL_CALLBACK *funcNotify)(cl_program program, void *userData),
                                  void *userData) {
    TRACING_ENTER(ClBuildProgram, &program, &numDevices, &deviceList, &options, &funcNotify, &userData);
    auto [validationResult, pProgram] = NEO::LEO::validateAndCast(std::make_tuple(program));
    if (validationResult != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClBuildProgram, &validationResult);
        return validationResult;
    }

    if (!NEO::LEO::Program::isValidCallback(funcNotify, userData)) [[unlikely]] {
        cl_int tracingRetVal = CL_INVALID_VALUE;
        TRACING_EXIT(ClBuildProgram, &tracingRetVal);
        return tracingRetVal;
    }

    cl_int tracingRetVal = pProgram->build(options, funcNotify, userData);
    TRACING_EXIT(ClBuildProgram, &tracingRetVal);
    return tracingRetVal;
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
    TRACING_ENTER(ClCompileProgram, &program, &numDevices, &deviceList, &options, &numInputHeaders, &inputHeaders, &headerIncludeNames, &funcNotify, &userData);
    auto [validationResult, pProgram] = NEO::LEO::validateAndCast(std::make_tuple(program));
    if (validationResult != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClCompileProgram, &validationResult);
        return validationResult;
    }

    if (!NEO::LEO::Program::isValidCallback(funcNotify, userData)) [[unlikely]] {
        cl_int tracingRetVal = CL_INVALID_VALUE;
        TRACING_EXIT(ClCompileProgram, &tracingRetVal);
        return tracingRetVal;
    }

    cl_int tracingRetVal = pProgram->compile(options, numInputHeaders, inputHeaders, headerIncludeNames, funcNotify, userData);
    TRACING_EXIT(ClCompileProgram, &tracingRetVal);
    return tracingRetVal;
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
    TRACING_ENTER(ClLinkProgram, &context, &numDevices, &deviceList, &options, &numInputPrograms, &inputPrograms, &funcNotify, &userData, &errcodeRet);
    ErrorCodeHelper errcodeHelper(errcodeRet, CL_SUCCESS);

    auto [retVal, pContext] = NEO::LEO::validateAndCast(std::make_tuple(context));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        errcodeHelper.set(retVal);
        cl_program tracingRetVal = nullptr;
        TRACING_EXIT(ClLinkProgram, &tracingRetVal);
        return tracingRetVal;
    }

    if (!NEO::LEO::Program::isValidCallback(funcNotify, userData)) [[unlikely]] {
        errcodeHelper.set(CL_INVALID_VALUE);
        cl_program tracingRetVal = nullptr;
        TRACING_EXIT(ClLinkProgram, &tracingRetVal);
        return tracingRetVal;
    }

    auto retProgram = std::make_unique<NEO::LEO::Program>(pContext);
    retProgram->setBuildStatus(CL_BUILD_ERROR);

    if (numInputPrograms == 0 || inputPrograms == nullptr) [[unlikely]] {
        errcodeHelper.set(CL_INVALID_VALUE);
        retProgram->invokeCallback(funcNotify, userData);
        cl_program tracingRetVal = retProgram.release();
        TRACING_EXIT(ClLinkProgram, &tracingRetVal);
        return tracingRetVal;
    }

    const cl_int linkResult = retProgram->link(options, numInputPrograms, inputPrograms);
    if (CL_SUCCESS != linkResult) [[unlikely]] {
        errcodeHelper.set(linkResult);
        retProgram->invokeCallback(funcNotify, userData);
        cl_program tracingRetVal = retProgram.release();
        TRACING_EXIT(ClLinkProgram, &tracingRetVal);
        return tracingRetVal;
    }

    retProgram->setBuildStatus(CL_BUILD_SUCCESS);
    retProgram->invokeCallback(funcNotify, userData);
    cl_program tracingRetVal = retProgram.release();
    TRACING_EXIT(ClLinkProgram, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clGetProgramInfo(cl_program program,
                                    cl_program_info paramName,
                                    size_t paramValueSize,
                                    void *paramValue,
                                    size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetProgramInfo, &program, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    auto [retVal, pProgram] = NEO::LEO::validateAndCast(std::make_tuple(program));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClGetProgramInfo, &retVal);
        return retVal;
    }

    cl_int tracingRetVal = pProgram->getInfo(paramName, paramValueSize, paramValue, paramValueSizeRet);
    TRACING_EXIT(ClGetProgramInfo, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clGetProgramBuildInfo(cl_program program,
                                         cl_device_id device,
                                         cl_program_build_info paramName,
                                         size_t paramValueSize,
                                         void *paramValue,
                                         size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetProgramBuildInfo, &program, &device, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    auto [retVal, pProgram] = NEO::LEO::validateAndCast(std::make_tuple(program));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClGetProgramBuildInfo, &retVal);
        return retVal;
    }

    cl_int tracingRetVal = pProgram->getBuildInfo(device, paramName, paramValueSize, paramValue, paramValueSizeRet);
    TRACING_EXIT(ClGetProgramBuildInfo, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clSetProgramReleaseCallback(cl_program program,
                                               void(CL_CALLBACK *pfnNotify)(cl_program /* program */, void * /* user_data */),
                                               void *userData) {
    TRACING_ENTER(ClSetProgramReleaseCallback, &program, &pfnNotify, &userData);
    cl_int tracingRetVal = CL_INVALID_OPERATION;
    TRACING_EXIT(ClSetProgramReleaseCallback, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clSetProgramSpecializationConstant(cl_program program, cl_uint specId, size_t specSize, const void *specValue) {
    TRACING_ENTER(ClSetProgramSpecializationConstant, &program, &specId, &specSize, &specValue);
    auto [retVal, pProgram] = NEO::LEO::validateAndCast(std::make_tuple(program), std::make_tuple(const_cast<void *>(specValue)));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClSetProgramSpecializationConstant, &retVal);
        return retVal;
    }

    cl_int tracingRetVal = pProgram->setProgramSpecializationConstant(specId, specSize, specValue);
    TRACING_EXIT(ClSetProgramSpecializationConstant, &tracingRetVal);
    return tracingRetVal;
}
