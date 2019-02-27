/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/get_info.h"

#include "runtime/context/context.h"
#include "runtime/helpers/base_object.h"
#include "runtime/helpers/validators.h"

#include "program.h"

namespace OCLRT {

cl_int Program::getInfo(cl_program_info paramName, size_t paramValueSize,
                        void *paramValue, size_t *paramValueSizeRet) {
    cl_int retVal = CL_SUCCESS;
    const void *pSrc = nullptr;
    size_t srcSize = 0;
    size_t retSize = 0;
    std::string kernelNamesString;
    cl_device_id device_id = pDevice;
    cl_uint refCount = 0;
    size_t numKernels;
    cl_context clContext = context;

    switch (paramName) {
    case CL_PROGRAM_CONTEXT:
        pSrc = &clContext;
        retSize = srcSize = sizeof(clContext);
        break;

    case CL_PROGRAM_BINARIES:
        resolveProgramBinary();
        pSrc = elfBinary.data();
        retSize = sizeof(void **);
        srcSize = elfBinarySize;
        if (paramValue != nullptr) {
            if (paramValueSize < retSize) {
                retVal = CL_INVALID_VALUE;
                break;
            }
            paramValueSize = srcSize;
            paramValue = *(void **)paramValue;
        }
        break;

    case CL_PROGRAM_BINARY_SIZES:
        resolveProgramBinary();
        pSrc = &elfBinarySize;
        retSize = srcSize = sizeof(size_t *);
        break;

    case CL_PROGRAM_KERNEL_NAMES:
        kernelNamesString = getKernelNamesString();
        pSrc = kernelNamesString.c_str();
        retSize = srcSize = kernelNamesString.length() + 1;

        if (buildStatus != CL_BUILD_SUCCESS) {
            retVal = CL_INVALID_PROGRAM_EXECUTABLE;
        }
        break;

    case CL_PROGRAM_NUM_KERNELS:
        numKernels = kernelInfoArray.size();
        pSrc = &numKernels;
        retSize = srcSize = sizeof(numKernels);

        if (buildStatus != CL_BUILD_SUCCESS) {
            retVal = CL_INVALID_PROGRAM_EXECUTABLE;
        }
        break;

    case CL_PROGRAM_NUM_DEVICES:
        pSrc = &numDevices;
        retSize = srcSize = sizeof(cl_uint);
        break;

    case CL_PROGRAM_DEVICES:
        pSrc = &device_id;
        retSize = srcSize = sizeof(cl_device_id);
        break;

    case CL_PROGRAM_REFERENCE_COUNT:
        refCount = static_cast<cl_uint>(this->getReference());
        retSize = srcSize = sizeof(refCount);
        pSrc = &refCount;
        break;

    case CL_PROGRAM_SOURCE:
        pSrc = sourceCode.c_str();
        retSize = srcSize = strlen(sourceCode.c_str()) + 1;
        break;

    case CL_PROGRAM_IL:
        pSrc = sourceCode.data();
        retSize = srcSize = sourceCode.size();
        if (!Program::isValidSpirvBinary(pSrc, srcSize)) {
            if (paramValueSizeRet) {
                *paramValueSizeRet = 0;
            }
            return CL_SUCCESS;
        }
        break;

    case CL_PROGRAM_DEBUG_INFO_SIZES_INTEL:
        resolveProgramBinary();
        retSize = srcSize = sizeof(debugDataSize);
        pSrc = &debugDataSize;
        break;

    case CL_PROGRAM_DEBUG_INFO_INTEL:
        resolveProgramBinary();
        pSrc = debugData;
        retSize = numDevices * sizeof(void **);
        srcSize = debugDataSize;
        if (paramValue != nullptr) {
            if (paramValueSize < retSize) {
                retVal = CL_INVALID_VALUE;
                break;
            }
            paramValueSize = srcSize;
            paramValue = *(void **)paramValue;
        }
        break;

    default:
        retVal = CL_INVALID_VALUE;
        break;
    }

    retVal = (retVal == CL_SUCCESS)
                 ? ::getInfo(paramValue, paramValueSize, pSrc, srcSize)
                 : retVal;
    if (paramValueSizeRet) {
        *paramValueSizeRet = retSize;
    }
    return retVal;
}

cl_int Program::getBuildInfo(cl_device_id device, cl_program_build_info paramName,
                             size_t paramValueSize, void *paramValue, size_t *paramValueSizeRet) const {
    cl_int retVal = CL_SUCCESS;
    const void *pSrc = nullptr;
    size_t srcSize = 0;
    size_t retSize = 0;
    cl_device_id device_id = pDevice;

    if (device != device_id) {
        return CL_INVALID_DEVICE;
    }

    retVal = validateObjects(device);
    if (retVal != CL_SUCCESS) {
        return CL_INVALID_DEVICE;
    }

    auto pDev = castToObject<Device>(device);

    switch (paramName) {
    case CL_PROGRAM_BUILD_STATUS:
        srcSize = retSize = sizeof(cl_build_status);
        pSrc = &buildStatus;
        break;

    case CL_PROGRAM_BUILD_OPTIONS:
        srcSize = retSize = strlen(options.c_str()) + 1;
        pSrc = options.c_str();
        break;

    case CL_PROGRAM_BUILD_LOG: {
        const char *pBuildLog = getBuildLog(pDev);

        if (pBuildLog != nullptr) {
            pSrc = pBuildLog;
            srcSize = retSize = strlen(pBuildLog) + 1;
        } else {
            pSrc = "";
            srcSize = retSize = 1;
        }
    } break;

    case CL_PROGRAM_BINARY_TYPE:
        srcSize = retSize = sizeof(cl_program_binary_type);
        pSrc = &programBinaryType;
        break;

    case CL_PROGRAM_BUILD_GLOBAL_VARIABLE_TOTAL_SIZE:
        pSrc = &globalVarTotalSize;
        retSize = srcSize = sizeof(size_t);
        break;

    default:
        retVal = CL_INVALID_VALUE;
        break;
    }

    retVal = (retVal == CL_SUCCESS)
                 ? ::getInfo(paramValue, paramValueSize, pSrc, srcSize)
                 : retVal;

    if (paramValueSizeRet) {
        *paramValueSizeRet = retSize;
    }

    return retVal;
}
} // namespace OCLRT
