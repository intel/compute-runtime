/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/program/kernel_info.h"

#include "shared/source/device/device.h"
#include "shared/source/device_binary_format/elf/zebin_elf.h"
#include "shared/source/helpers/kernel_helpers.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"

#include <cstdint>
#include <unordered_map>

namespace NEO {

struct KernelArgumentType {
    const char *argTypeQualifier;
    uint64_t argTypeQualifierValue;
};

KernelInfo::~KernelInfo() {
    delete[] crossThreadData;
}

size_t KernelInfo::getSamplerStateArrayCount() const {
    return kernelDescriptor.payloadMappings.samplerTable.numSamplers;
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
            auto &productHelper = device.getProductHelper();

            return MemoryTransferHelper::transferMemoryToAllocation(productHelper.isBlitCopyRequiredForLocalMemory(hwInfo, *kernelAllocation),
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
    auto &productHelper = device.getProductHelper();

    return MemoryTransferHelper::transferMemoryToAllocation(productHelper.isBlitCopyRequiredForLocalMemory(hwInfo, *kernelAllocation),
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
        const auto &kernelName = kernelInfo->kernelDescriptor.kernelMetadata.kernelName;
        if (kernelName == NEO::Elf::SectionsNamesZebin::externalFunctions) {
            continue;
        }

        if (!semiColonDelimitedKernelNameStr.empty()) {
            semiColonDelimitedKernelNameStr += ';';
        }
        semiColonDelimitedKernelNameStr += kernelName;
    }

    return semiColonDelimitedKernelNameStr;
}

} // namespace NEO
