/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/program/kernel_info.h"

#include "shared/source/device/device.h"
#include "shared/source/device_binary_format/patchtokens_decoder.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/kernel_helpers.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/memory_manager.h"

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

WorkSizeInfo::WorkSizeInfo(uint32_t maxWorkGroupSize, bool hasBarriers, uint32_t simdSize, uint32_t slmTotalSize, const HardwareInfo *hwInfo, uint32_t numThreadsPerSubSlice, uint32_t localMemSize, bool imgUsed, bool yTiledSurface, bool disableEUFusion) {
    this->maxWorkGroupSize = maxWorkGroupSize;
    this->hasBarriers = hasBarriers;
    this->simdSize = simdSize;
    this->slmTotalSize = slmTotalSize;
    this->coreFamily = hwInfo->platform.eRenderCoreFamily;
    this->numThreadsPerSubSlice = numThreadsPerSubSlice;
    this->localMemSize = localMemSize;
    this->imgUsed = imgUsed;
    this->yTiledSurfaces = yTiledSurface;

    setMinWorkGroupSize(hwInfo, disableEUFusion);
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

void WorkSizeInfo::setMinWorkGroupSize(const HardwareInfo *hwInfo, bool disableEUFusion) {
    minWorkGroupSize = 0;
    if (hasBarriers) {
        uint32_t maxBarriersPerHSlice = (coreFamily >= IGFX_GEN9_CORE) ? 32 : 16;
        minWorkGroupSize = numThreadsPerSubSlice * simdSize / maxBarriersPerHSlice;
    }
    if (slmTotalSize > 0) {
        if (localMemSize < slmTotalSize) {
            PRINT_DEBUG_STRING(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Size of SLM (%u) larger than available (%u)\n", slmTotalSize, localMemSize);
        }
        UNRECOVERABLE_IF(localMemSize < slmTotalSize);
        minWorkGroupSize = std::max(maxWorkGroupSize / ((localMemSize / slmTotalSize)), minWorkGroupSize);
    }

    const auto &hwHelper = HwHelper::get(hwInfo->platform.eRenderCoreFamily);
    if (hwHelper.isFusedEuDispatchEnabled(*hwInfo, disableEUFusion)) {
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
    size_t samplerStateArraySize = getSamplerStateArrayCount() * HwHelper::get(hwInfo.platform.eRenderCoreFamily).getSamplerStateSize();
    return samplerStateArraySize;
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
    const auto allocType = internalIsa ? AllocationType::KERNEL_ISA_INTERNAL : AllocationType::KERNEL_ISA;

    if (device.getMemoryManager()->isKernelBinaryReuseEnabled()) {
        auto lock = device.getMemoryManager()->lockKernelAllocationMap();
        auto kernelName = this->kernelDescriptor.kernelMetadata.kernelName;
        auto &storedAllocations = device.getMemoryManager()->getKernelAllocationMap();
        auto kernelAllocations = storedAllocations.find(kernelName);
        if (kernelAllocations != storedAllocations.end()) {
            kernelAllocation = kernelAllocations->second.kernelAllocation;
            kernelAllocations->second.reuseCounter++;
            auto &hwInfo = device.getHardwareInfo();
            auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);

            return MemoryTransferHelper::transferMemoryToAllocation(hwInfoConfig.isBlitCopyRequiredForLocalMemory(hwInfo, *kernelAllocation),
                                                                    device, kernelAllocation, 0, heapInfo.pKernelHeap,
                                                                    static_cast<size_t>(kernelIsaSize));
        } else {
            kernelAllocation = device.getMemoryManager()->allocateGraphicsMemoryWithProperties({device.getRootDeviceIndex(), kernelIsaSize, allocType, device.getDeviceBitfield()});
            storedAllocations.insert(std::make_pair(kernelName, MemoryManager::KernelAllocationInfo(kernelAllocation, 1u)));
        }
    } else {
        kernelAllocation = device.getMemoryManager()->allocateGraphicsMemoryWithProperties({device.getRootDeviceIndex(), kernelIsaSize, allocType, device.getDeviceBitfield()});
    }

    if (!kernelAllocation) {
        return false;
    }

    auto &hwInfo = device.getHardwareInfo();
    auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);

    return MemoryTransferHelper::transferMemoryToAllocation(hwInfoConfig.isBlitCopyRequiredForLocalMemory(hwInfo, *kernelAllocation),
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
