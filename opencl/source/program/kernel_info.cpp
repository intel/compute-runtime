/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/patchtokens_decoder.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/kernel_helpers.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/sampler/sampler.h"

#include "hw_cmds.h"

#include <cstdint>
#include <cstring>
#include <map>
#include <sstream>
#include <unordered_map>

namespace NEO {

struct KernelArgumentType {
    const char *argTypeQualifier;
    uint64_t argTypeQualifierValue;
};

std::map<std::string, size_t> typeSizeMap = {
    {"char", sizeof(cl_char)},
    {"char2", sizeof(cl_char2)},
    {"char3", sizeof(cl_char3)},
    {"char4", sizeof(cl_char4)},
    {"char8", sizeof(cl_char8)},
    {"char16", sizeof(cl_char16)},

    {"uchar", sizeof(cl_uchar)},
    {"uchar2", sizeof(cl_uchar2)},
    {"uchar3", sizeof(cl_uchar3)},
    {"uchar4", sizeof(cl_uchar4)},
    {"uchar8", sizeof(cl_uchar8)},
    {"uchar16", sizeof(cl_uchar16)},

    {"short", sizeof(cl_short)},
    {"short2", sizeof(cl_short2)},
    {"short3", sizeof(cl_short3)},
    {"short4", sizeof(cl_short4)},
    {"short8", sizeof(cl_short8)},
    {"short16", sizeof(cl_short16)},

    {"ushort", sizeof(cl_ushort)},
    {"ushort2", sizeof(cl_ushort2)},
    {"ushort3", sizeof(cl_ushort3)},
    {"ushort4", sizeof(cl_ushort4)},
    {"ushort8", sizeof(cl_ushort8)},
    {"ushort16", sizeof(cl_ushort16)},

    {"int", sizeof(cl_int)},
    {"int2", sizeof(cl_int2)},
    {"int3", sizeof(cl_int3)},
    {"int4", sizeof(cl_int4)},
    {"int8", sizeof(cl_int8)},
    {"int16", sizeof(cl_int16)},

    {"uint", sizeof(cl_uint)},
    {"uint2", sizeof(cl_uint2)},
    {"uint3", sizeof(cl_uint3)},
    {"uint4", sizeof(cl_uint4)},
    {"uint8", sizeof(cl_uint8)},
    {"uint16", sizeof(cl_uint16)},

    {"long", sizeof(cl_long)},
    {"long2", sizeof(cl_long2)},
    {"long3", sizeof(cl_long3)},
    {"long4", sizeof(cl_long4)},
    {"long8", sizeof(cl_long8)},
    {"long16", sizeof(cl_long16)},

    {"ulong", sizeof(cl_ulong)},
    {"ulong2", sizeof(cl_ulong2)},
    {"ulong3", sizeof(cl_ulong3)},
    {"ulong4", sizeof(cl_ulong4)},
    {"ulong8", sizeof(cl_ulong8)},
    {"ulong16", sizeof(cl_ulong16)},

    {"half", sizeof(cl_half)},

    {"float", sizeof(cl_float)},
    {"float2", sizeof(cl_float2)},
    {"float3", sizeof(cl_float3)},
    {"float4", sizeof(cl_float4)},
    {"float8", sizeof(cl_float8)},
    {"float16", sizeof(cl_float16)},

#ifdef cl_khr_fp16
    {"half2", sizeof(cl_half2)},
    {"half3", sizeof(cl_half3)},
    {"half4", sizeof(cl_half4)},
    {"half8", sizeof(cl_half8)},
    {"half16", sizeof(cl_half16)},
#endif

    {"double", sizeof(cl_double)},
    {"double2", sizeof(cl_double2)},
    {"double3", sizeof(cl_double3)},
    {"double4", sizeof(cl_double4)},
    {"double8", sizeof(cl_double8)},
    {"double16", sizeof(cl_double16)},
};

WorkSizeInfo::WorkSizeInfo(uint32_t maxWorkGroupSize, bool hasBarriers, uint32_t simdSize, uint32_t slmTotalSize, const HardwareInfo *hwInfo, uint32_t numThreadsPerSubSlice, uint32_t localMemSize, bool imgUsed, bool yTiledSurface) {
    this->maxWorkGroupSize = maxWorkGroupSize;
    this->hasBarriers = hasBarriers;
    this->simdSize = simdSize;
    this->slmTotalSize = slmTotalSize;
    this->coreFamily = hwInfo->platform.eRenderCoreFamily;
    this->numThreadsPerSubSlice = numThreadsPerSubSlice;
    this->localMemSize = localMemSize;
    this->imgUsed = imgUsed;
    this->yTiledSurfaces = yTiledSurface;

    setMinWorkGroupSize(hwInfo);
}

WorkSizeInfo::WorkSizeInfo(const DispatchInfo &dispatchInfo) {
    auto &device = dispatchInfo.getClDevice();
    const auto &kernelInfo = dispatchInfo.getKernel()->getKernelInfo();
    this->maxWorkGroupSize = dispatchInfo.getKernel()->getMaxKernelWorkGroupSize();
    this->hasBarriers = kernelInfo.kernelDescriptor.kernelAttributes.usesBarriers();
    this->simdSize = static_cast<uint32_t>(kernelInfo.getMaxSimdSize());
    this->slmTotalSize = static_cast<uint32_t>(dispatchInfo.getKernel()->getSlmTotalSize());
    this->coreFamily = device.getHardwareInfo().platform.eRenderCoreFamily;
    this->numThreadsPerSubSlice = static_cast<uint32_t>(device.getSharedDeviceInfo().maxNumEUsPerSubSlice) *
                                  device.getSharedDeviceInfo().numThreadsPerEU;
    this->localMemSize = static_cast<uint32_t>(device.getSharedDeviceInfo().localMemSize);

    setIfUseImg(kernelInfo);
    setMinWorkGroupSize(&device.getHardwareInfo());
}
void WorkSizeInfo::setIfUseImg(const KernelInfo &kernelInfo) {
    for (const auto &arg : kernelInfo.kernelDescriptor.payloadMappings.explicitArgs) {
        if (arg.is<ArgDescriptor::ArgTImage>()) {
            imgUsed = true;
            yTiledSurfaces = true;
            return;
        }
    }
}
void WorkSizeInfo::setMinWorkGroupSize(const HardwareInfo *hwInfo) {
    minWorkGroupSize = 0;
    if (hasBarriers) {
        uint32_t maxBarriersPerHSlice = (coreFamily >= IGFX_GEN9_CORE) ? 32 : 16;
        minWorkGroupSize = numThreadsPerSubSlice * simdSize / maxBarriersPerHSlice;
    }
    if (slmTotalSize > 0) {
        UNRECOVERABLE_IF(localMemSize < slmTotalSize);
        minWorkGroupSize = std::max(maxWorkGroupSize / ((localMemSize / slmTotalSize)), minWorkGroupSize);
    }

    const auto &hwHelper = HwHelper::get(hwInfo->platform.eRenderCoreFamily);
    if (hwHelper.isFusedEuDispatchEnabled(*hwInfo)) {
        minWorkGroupSize *= 2;
    }
}
void WorkSizeInfo::checkRatio(const size_t workItems[3]) {
    if (slmTotalSize > 0) {
        useRatio = true;
        targetRatio = log((float)workItems[0]) - log((float)workItems[1]);
        useStrictRatio = false;
    } else if (yTiledSurfaces == true) {
        useRatio = true;
        targetRatio = YTilingRatioValue;
        useStrictRatio = true;
    }
}

KernelInfo::~KernelInfo() {
    delete[] crossThreadData;
}

size_t KernelInfo::getSamplerStateArrayCount() const {
    return kernelDescriptor.payloadMappings.samplerTable.numSamplers;
}
size_t KernelInfo::getSamplerStateArraySize(const HardwareInfo &hwInfo) const {
    size_t samplerStateArraySize = getSamplerStateArrayCount() * Sampler::getSamplerStateSize(hwInfo);
    return samplerStateArraySize;
}

size_t KernelInfo::getBorderColorStateSize() const {
    size_t borderColorSize = 0;
    if (kernelDescriptor.payloadMappings.samplerTable.numSamplers > 0U) {
        borderColorSize = kernelDescriptor.payloadMappings.samplerTable.tableOffset - kernelDescriptor.payloadMappings.samplerTable.borderColor;
    }
    return borderColorSize;
}

size_t KernelInfo::getBorderColorOffset() const {
    size_t borderColorOffset = 0;
    if (kernelDescriptor.payloadMappings.samplerTable.numSamplers > 0U) {
        borderColorOffset = kernelDescriptor.payloadMappings.samplerTable.borderColor;
    }
    return borderColorOffset;
}

uint32_t KernelInfo::getConstantBufferSize() const {
    return kernelDescriptor.kernelAttributes.crossThreadDataSize;
}
int32_t KernelInfo::getArgNumByName(const char *name) const {
    int32_t argNum = 0;
    for (const auto &argMeta : kernelDescriptor.explicitArgsExtendedMetadata) {
        if (argMeta.argName.compare(name) == 0) {
            return argNum;
        }
        ++argNum;
    }
    return -1;
}

bool KernelInfo::createKernelAllocation(const Device &device, bool internalIsa) {
    UNRECOVERABLE_IF(kernelAllocation);
    auto kernelIsaSize = heapInfo.KernelHeapSize;
    const auto allocType = internalIsa ? GraphicsAllocation::AllocationType::KERNEL_ISA_INTERNAL : GraphicsAllocation::AllocationType::KERNEL_ISA;
    kernelAllocation = device.getMemoryManager()->allocateGraphicsMemoryWithProperties({device.getRootDeviceIndex(), kernelIsaSize, allocType, device.getDeviceBitfield()});
    if (!kernelAllocation) {
        return false;
    }

    auto &hwInfo = device.getHardwareInfo();
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    return MemoryTransferHelper::transferMemoryToAllocation(hwHelper.isBlitCopyRequiredForLocalMemory(hwInfo, *kernelAllocation),
                                                            device, kernelAllocation, 0, heapInfo.pKernelHeap,
                                                            static_cast<size_t>(kernelIsaSize));
}

void KernelInfo::apply(const DeviceInfoKernelPayloadConstants &constants) {
    if (nullptr == this->crossThreadData) {
        return;
    }

    const auto &implicitArgs = kernelDescriptor.payloadMappings.implicitArgs;
    const auto privateMemorySize = static_cast<uint32_t>(KernelHelper::getPrivateSurfaceSize(kernelDescriptor.kernelAttributes.perHwThreadPrivateMemorySize,
                                                                                             constants.computeUnitsUsedForScratch));

    auto setIfValidOffset = [&](auto value, NEO::CrossThreadDataOffset offset) {
        if (isValidOffset(offset)) {
            *ptrOffset(reinterpret_cast<decltype(value) *>(crossThreadData), offset) = value;
        }
    };
    setIfValidOffset(reinterpret_cast<uintptr_t>(constants.slmWindow), implicitArgs.localMemoryStatelessWindowStartAddres);
    setIfValidOffset(constants.slmWindowSize, implicitArgs.localMemoryStatelessWindowSize);
    setIfValidOffset(privateMemorySize, implicitArgs.privateMemorySize);
    setIfValidOffset(constants.maxWorkGroupSize, implicitArgs.maxWorkGroupSize);
}

std::string concatenateKernelNames(ArrayRef<KernelInfo *> kernelInfos) {
    std::string semiColonDelimitedKernelNameStr;

    for (const auto &kernelInfo : kernelInfos) {
        if (!semiColonDelimitedKernelNameStr.empty()) {
            semiColonDelimitedKernelNameStr += ';';
        }
        semiColonDelimitedKernelNameStr += kernelInfo->kernelDescriptor.kernelMetadata.kernelName;
    }

    return semiColonDelimitedKernelNameStr;
}

} // namespace NEO
