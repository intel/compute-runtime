/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "CL/cl.h"

#include <cstdint>
#include <string>

struct KernelArgPatchInfo {
    uint32_t crossthreadOffset = 0;
    uint32_t size = 0;
    uint32_t sourceOffset = 0;
};

struct KernelArgInfo {
    static constexpr uint32_t undefinedOffset = (uint32_t)-1;

    std::string name;
    std::string typeStr;
    std::string accessQualifierStr;
    std::string addressQualifierStr;
    std::string typeQualifierStr;
    uint32_t offsetHeap = 0;
    std::vector<KernelArgPatchInfo> kernelArgPatchInfoVector;
    uint32_t slmAlignment = 0;
    bool isImage = false;
    bool isMediaImage = false;
    bool isMediaBlockImage = false;
    bool isSampler = false;
    bool isAccelerator = false;
    bool isDeviceQueue = false;
    bool isBuffer = false;
    bool pureStatefulBufferAccess = false;
    uint32_t samplerArgumentType = 0;
    uint32_t offsetImgWidth = undefinedOffset;
    uint32_t offsetImgHeight = undefinedOffset;
    uint32_t offsetImgDepth = undefinedOffset;
    uint32_t offsetChannelDataType = undefinedOffset;
    uint32_t offsetChannelOrder = undefinedOffset;
    uint32_t offsetArraySize = undefinedOffset;
    uint32_t offsetNumSamples = undefinedOffset;
    uint32_t offsetSamplerSnapWa = undefinedOffset;
    uint32_t offsetSamplerAddressingMode = undefinedOffset;
    uint32_t offsetSamplerNormalizedCoords = undefinedOffset;
    uint32_t offsetVmeMbBlockType = undefinedOffset;
    uint32_t offsetVmeSubpixelMode = undefinedOffset;
    uint32_t offsetVmeSadAdjustMode = undefinedOffset;
    uint32_t offsetVmeSearchPathType = undefinedOffset;
    uint32_t offsetObjectId = undefinedOffset;
    uint32_t offsetBufferOffset = undefinedOffset;
    uint32_t offsetNumMipLevels = undefinedOffset;

    bool needPatch = false;
    bool isTransformable = false;

    cl_kernel_arg_access_qualifier accessQualifier = CL_KERNEL_ARG_ACCESS_NONE;
    cl_kernel_arg_address_qualifier addressQualifier = CL_KERNEL_ARG_ADDRESS_GLOBAL;
    cl_kernel_arg_type_qualifier typeQualifier = CL_KERNEL_ARG_TYPE_NONE;

    KernelArgInfo() = default;
};
