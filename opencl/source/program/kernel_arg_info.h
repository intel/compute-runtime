/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/compiler_interface/compiler_options/compiler_options_base.h"
#include "shared/source/kernel/kernel_arg_descriptor.h"
#include "shared/source/utilities/const_stringref.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

namespace NEO {

struct KernelArgPatchInfo {
    uint32_t crossthreadOffset = 0;
    uint32_t size = 0;
    uint32_t sourceOffset = 0;
};

struct KernelArgInfo {
    KernelArgInfo() {
        this->metadataExtended = std::make_unique<ArgTypeMetadataExtended>();
    }
    ~KernelArgInfo() = default;
    KernelArgInfo(const KernelArgInfo &rhs) = delete;
    KernelArgInfo &operator=(const KernelArgInfo &) = delete;
    KernelArgInfo(KernelArgInfo &&) = default;
    KernelArgInfo &operator=(KernelArgInfo &&) = default;

    static constexpr uint32_t undefinedOffset = (uint32_t)-1;

    ArgTypeTraits metadata;
    std::unique_ptr<ArgTypeMetadataExtended> metadataExtended;

    uint32_t slmAlignment = 0;
    bool isImage = false;
    bool isMediaImage = false;
    bool isMediaBlockImage = false;
    bool isSampler = false;
    bool isAccelerator = false;
    bool isDeviceQueue = false;
    bool isBuffer = false;
    bool pureStatefulBufferAccess = false;
    bool isReadOnly = false;
    bool needPatch = false;
    bool isTransformable = false;

    uint32_t offsetHeap = 0;
    std::vector<KernelArgPatchInfo> kernelArgPatchInfoVector;
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
    uint32_t offsetFlatBaseOffset = undefinedOffset;
    uint32_t offsetFlatWidth = undefinedOffset;
    uint32_t offsetFlatHeight = undefinedOffset;
    uint32_t offsetFlatPitch = undefinedOffset;
};

} // namespace NEO
