/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/get_info.h"

#include "shared/source/device/device.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/program/kernel_info.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/helpers/base_object.h"
#include "opencl/source/helpers/cl_validators.h"
#include "opencl/source/helpers/get_info_status_mapper.h"

#include "program.h"

namespace NEO {

cl_int Program::getInfo(cl_program_info paramName, size_t paramValueSize,
                        void *paramValue, size_t *paramValueSizeRet) {
    cl_int retVal = CL_SUCCESS;
    const void *pSrc = nullptr;
    size_t srcSize = GetInfo::invalidSourceSize;
    size_t retSize = 0;
    std::string kernelNamesString;
    cl_uint refCount = 0;
    size_t numKernels;
    cl_context clContext = context;
    cl_uint clFalse = CL_FALSE;
    std::vector<cl_device_id> devicesToExpose;
    StackVec<size_t, 1> binarySizes;
    StackVec<size_t, 1> debugDataSizes;
    uint32_t numDevices = static_cast<uint32_t>(clDevices.size());

    switch (paramName) {
    case CL_PROGRAM_CONTEXT:
        pSrc = &clContext;
        retSize = srcSize = sizeof(clContext);
        break;

    case CL_PROGRAM_BINARIES: {
        auto requiredSize = clDevices.size() * sizeof(const unsigned char **);
        if (!paramValue) {
            retSize = requiredSize;
            srcSize = 0u;
            break;
        }
        if (paramValueSize < requiredSize) {
            retVal = CL_INVALID_VALUE;
            break;
        }
        auto outputBinaries = reinterpret_cast<unsigned char **>(paramValue);
        for (auto i = 0u; i < clDevices.size(); i++) {
            if (outputBinaries[i] == nullptr) {
                continue;
            }
            auto rootDeviceIndex = clDevices[i]->getRootDeviceIndex();
            auto binarySize = buildInfos[rootDeviceIndex].packedDeviceBinarySize;
            memcpy_s(outputBinaries[i], binarySize, buildInfos[rootDeviceIndex].packedDeviceBinary.get(), binarySize);
        }
        GetInfo::setParamValueReturnSize(paramValueSizeRet, requiredSize, GetInfoStatus::SUCCESS);
        return CL_SUCCESS;
    } break;

    case CL_PROGRAM_BINARY_SIZES:
        for (auto i = 0u; i < clDevices.size(); i++) {
            auto rootDeviceIndex = clDevices[i]->getRootDeviceIndex();
            packDeviceBinary(*clDevices[i]);
            binarySizes.push_back(buildInfos[rootDeviceIndex].packedDeviceBinarySize);
        }

        pSrc = binarySizes.data();
        retSize = srcSize = binarySizes.size() * sizeof(cl_device_id);
        break;

    case CL_PROGRAM_KERNEL_NAMES:
        kernelNamesString = concatenateKernelNames(buildInfos[clDevices[0]->getRootDeviceIndex()].kernelInfoArray);
        pSrc = kernelNamesString.c_str();
        retSize = srcSize = kernelNamesString.length() + 1;

        if (!isBuilt()) {
            retVal = CL_INVALID_PROGRAM_EXECUTABLE;
        }
        break;

    case CL_PROGRAM_NUM_KERNELS:
        numKernels = getNumKernels();
        pSrc = &numKernels;
        retSize = srcSize = sizeof(numKernels);

        if (!isBuilt()) {
            retVal = CL_INVALID_PROGRAM_EXECUTABLE;
        }
        break;

    case CL_PROGRAM_NUM_DEVICES:
        pSrc = &numDevices;
        retSize = srcSize = sizeof(cl_uint);
        break;

    case CL_PROGRAM_DEVICES:
        clDevices.toDeviceIDs(devicesToExpose);
        pSrc = devicesToExpose.data();
        retSize = srcSize = devicesToExpose.size() * sizeof(cl_device_id);
        break;

    case CL_PROGRAM_REFERENCE_COUNT:
        refCount = static_cast<cl_uint>(this->getReference());
        retSize = srcSize = sizeof(refCount);
        pSrc = &refCount;
        break;

    case CL_PROGRAM_SOURCE:
        if (createdFrom == CreatedFrom::SOURCE) {
            pSrc = sourceCode.c_str();
            retSize = srcSize = strlen(sourceCode.c_str()) + 1;
        } else {
            if (paramValueSizeRet) {
                *paramValueSizeRet = 0;
            }
            return CL_SUCCESS;
        }
        break;

    case CL_PROGRAM_IL:
        if (createdFrom != CreatedFrom::IL) {
            if (paramValueSizeRet) {
                *paramValueSizeRet = 0;
            }
            return CL_SUCCESS;
        }
        pSrc = irBinary.get();
        retSize = srcSize = irBinarySize;
        break;

    case CL_PROGRAM_DEBUG_INFO_SIZES_INTEL:
        for (auto i = 0u; i < clDevices.size(); i++) {
            auto rootDeviceIndex = clDevices[i]->getRootDeviceIndex();
            if (nullptr == buildInfos[rootDeviceIndex].debugData) {
                auto refBin = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(buildInfos[rootDeviceIndex].unpackedDeviceBinary.get()), buildInfos[rootDeviceIndex].unpackedDeviceBinarySize);
                if (isDeviceBinaryFormat<DeviceBinaryFormat::Zebin>(refBin)) {
                    createDebugZebin(rootDeviceIndex);
                } else
                    continue;
            }
            debugDataSizes.push_back(buildInfos[rootDeviceIndex].debugDataSize);
        }
        pSrc = debugDataSizes.data();
        retSize = srcSize = debugDataSizes.size() * sizeof(cl_device_id);
        break;

    case CL_PROGRAM_DEBUG_INFO_INTEL: {
        auto requiredSize = numDevices * sizeof(void **);
        if (paramValue == nullptr) {
            retSize = requiredSize;
            srcSize = 0u;
            break;
        }
        if (paramValueSize < requiredSize) {
            retVal = CL_INVALID_VALUE;
            break;
        }
        auto outputDebugData = reinterpret_cast<unsigned char **>(paramValue);
        for (auto i = 0u; i < clDevices.size(); i++) {
            auto rootDeviceIndex = clDevices[i]->getRootDeviceIndex();
            if (nullptr == buildInfos[rootDeviceIndex].debugData) {
                auto refBin = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(buildInfos[rootDeviceIndex].unpackedDeviceBinary.get()), buildInfos[rootDeviceIndex].unpackedDeviceBinarySize);
                if (isDeviceBinaryFormat<DeviceBinaryFormat::Zebin>(refBin)) {
                    createDebugZebin(rootDeviceIndex);
                } else
                    continue;
            }
            auto dbgDataSize = buildInfos[rootDeviceIndex].debugDataSize;
            memcpy_s(outputDebugData[i], dbgDataSize, buildInfos[rootDeviceIndex].debugData.get(), dbgDataSize);
        }
        GetInfo::setParamValueReturnSize(paramValueSizeRet, requiredSize, GetInfoStatus::SUCCESS);
        return CL_SUCCESS;
    } break;

    case CL_PROGRAM_SCOPE_GLOBAL_CTORS_PRESENT:
    case CL_PROGRAM_SCOPE_GLOBAL_DTORS_PRESENT:
        retSize = srcSize = sizeof(clFalse);
        pSrc = &clFalse;
        break;

    default:
        retVal = CL_INVALID_VALUE;
        break;
    }

    auto getInfoStatus = GetInfoStatus::INVALID_VALUE;
    if (retVal == CL_SUCCESS) {
        getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, pSrc, srcSize);
        retVal = changeGetInfoStatusToCLResultType(getInfoStatus);
    }
    GetInfo::setParamValueReturnSize(paramValueSizeRet, retSize, getInfoStatus);
    return retVal;
}

cl_int Program::getBuildInfo(cl_device_id device, cl_program_build_info paramName,
                             size_t paramValueSize, void *paramValue, size_t *paramValueSizeRet) const {
    cl_int retVal = CL_SUCCESS;
    const void *pSrc = nullptr;
    size_t srcSize = GetInfo::invalidSourceSize;
    size_t retSize = 0;

    auto pClDev = castToObject<ClDevice>(device);
    auto rootDeviceIndex = pClDev->getRootDeviceIndex();

    switch (paramName) {
    case CL_PROGRAM_BUILD_STATUS:
        srcSize = retSize = sizeof(cl_build_status);
        pSrc = &deviceBuildInfos.at(pClDev).buildStatus;
        break;

    case CL_PROGRAM_BUILD_OPTIONS:
        srcSize = retSize = strlen(options.c_str()) + 1;
        pSrc = options.c_str();
        break;

    case CL_PROGRAM_BUILD_LOG: {
        const char *pBuildLog = getBuildLog(pClDev->getRootDeviceIndex());

        pSrc = pBuildLog;
        srcSize = retSize = strlen(pBuildLog) + 1;
    } break;

    case CL_PROGRAM_BINARY_TYPE:
        srcSize = retSize = sizeof(cl_program_binary_type);
        pSrc = &deviceBuildInfos.at(pClDev).programBinaryType;
        break;

    case CL_PROGRAM_BUILD_GLOBAL_VARIABLE_TOTAL_SIZE:
        pSrc = &buildInfos[rootDeviceIndex].globalVarTotalSize;
        retSize = srcSize = sizeof(size_t);
        break;

    default:
        retVal = CL_INVALID_VALUE;
        break;
    }

    auto getInfoStatus = GetInfoStatus::INVALID_VALUE;
    if (retVal == CL_SUCCESS) {
        getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, pSrc, srcSize);
        retVal = changeGetInfoStatusToCLResultType(getInfoStatus);
    }
    GetInfo::setParamValueReturnSize(paramValueSizeRet, retSize, getInfoStatus);

    return retVal;
}
} // namespace NEO
