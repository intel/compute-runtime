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
