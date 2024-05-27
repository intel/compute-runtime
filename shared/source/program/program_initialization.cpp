/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "program_initialization.h"

#include "shared/source/compiler_interface/linker.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/program/program_info.h"

namespace NEO {

GraphicsAllocation *allocateGlobalsSurface(NEO::SVMAllocsManager *const svmAllocManager, NEO::Device &device, size_t totalSize, size_t zeroInitSize, bool constant,
                                           LinkerInput *const linkerInput, const void *initData) {
    bool globalsAreExported = false;
    GraphicsAllocation *gpuAllocation = nullptr;
    const auto rootDeviceIndex = device.getRootDeviceIndex();
    const auto deviceBitfield = device.getDeviceBitfield();

    if (linkerInput != nullptr) {
        globalsAreExported = constant ? linkerInput->getTraits().exportsGlobalConstants : linkerInput->getTraits().exportsGlobalVariables;
    }
    const auto allocationType = constant ? AllocationType::constantSurface : AllocationType::globalSurface;
    if (globalsAreExported && (svmAllocManager != nullptr)) {
        RootDeviceIndicesContainer rootDeviceIndices;
        rootDeviceIndices.pushUnique(rootDeviceIndex);
        std::map<uint32_t, DeviceBitfield> subDeviceBitfields;
        subDeviceBitfields.insert({rootDeviceIndex, deviceBitfield});
        NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, subDeviceBitfields);
        unifiedMemoryProperties.device = &device;
        unifiedMemoryProperties.requestedAllocationType = allocationType;
        unifiedMemoryProperties.isInternalAllocation = true;
        auto ptr = svmAllocManager->createUnifiedMemoryAllocation(totalSize, unifiedMemoryProperties);
        DEBUG_BREAK_IF(ptr == nullptr);
        if (ptr == nullptr) {
            return nullptr;
        }
        auto usmAlloc = svmAllocManager->getSVMAlloc(ptr);
        UNRECOVERABLE_IF(usmAlloc == nullptr);
        gpuAllocation = usmAlloc->gpuAllocations.getGraphicsAllocation(rootDeviceIndex);
    } else {
        gpuAllocation = device.getMemoryManager()->allocateGraphicsMemoryWithProperties({rootDeviceIndex,
                                                                                         true, // allocateMemory
                                                                                         totalSize, allocationType,
                                                                                         false, // isMultiStorageAllocation
                                                                                         deviceBitfield});
    }

    if (!gpuAllocation) {
        return nullptr;
    }

    auto &rootDeviceEnvironment = device.getRootDeviceEnvironment();
    auto &productHelper = device.getProductHelper();

    bool isOnlyBssData = (totalSize == zeroInitSize);
    if (false == isOnlyBssData) {
        auto initSize = totalSize - zeroInitSize;
        auto success = MemoryTransferHelper::transferMemoryToAllocation(productHelper.isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, *gpuAllocation),
                                                                        device, gpuAllocation, 0, initData, initSize);
        UNRECOVERABLE_IF(!success);
    }
    return gpuAllocation;
}

} // namespace NEO
