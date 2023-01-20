/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "program_initialization.h"

#include "shared/source/compiler_interface/linker.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/program/program_info.h"

namespace NEO {

GraphicsAllocation *allocateGlobalsSurface(NEO::SVMAllocsManager *const svmAllocManager, NEO::Device &device, size_t size, bool constant,
                                           LinkerInput *const linkerInput, const void *initData) {
    bool globalsAreExported = false;
    GraphicsAllocation *gpuAllocation = nullptr;
    auto rootDeviceIndex = device.getRootDeviceIndex();
    auto deviceBitfield = device.getDeviceBitfield();

    if (linkerInput != nullptr) {
        globalsAreExported = constant ? linkerInput->getTraits().exportsGlobalConstants : linkerInput->getTraits().exportsGlobalVariables;
    }

    if (globalsAreExported && (svmAllocManager != nullptr)) {
        RootDeviceIndicesContainer rootDeviceIndices;
        rootDeviceIndices.push_back(rootDeviceIndex);
        std::map<uint32_t, DeviceBitfield> subDeviceBitfields;
        subDeviceBitfields.insert({rootDeviceIndex, deviceBitfield});
        NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::DEVICE_UNIFIED_MEMORY, rootDeviceIndices, subDeviceBitfields);
        unifiedMemoryProperties.device = &device;
        auto ptr = svmAllocManager->createUnifiedMemoryAllocation(size, unifiedMemoryProperties);
        DEBUG_BREAK_IF(ptr == nullptr);
        if (ptr == nullptr) {
            return nullptr;
        }
        auto usmAlloc = svmAllocManager->getSVMAlloc(ptr);
        UNRECOVERABLE_IF(usmAlloc == nullptr);
        gpuAllocation = usmAlloc->gpuAllocations.getGraphicsAllocation(rootDeviceIndex);
    } else {
        auto allocationType = constant ? AllocationType::CONSTANT_SURFACE : AllocationType::GLOBAL_SURFACE;
        gpuAllocation = device.getMemoryManager()->allocateGraphicsMemoryWithProperties({rootDeviceIndex,
                                                                                         true, // allocateMemory
                                                                                         size, allocationType,
                                                                                         false, // isMultiStorageAllocation
                                                                                         deviceBitfield});
    }

    if (!gpuAllocation) {
        return nullptr;
    }

    auto &hwInfo = device.getHardwareInfo();
    auto &productHelper = device.getProductHelper();

    auto success = MemoryTransferHelper::transferMemoryToAllocation(productHelper.isBlitCopyRequiredForLocalMemory(hwInfo, *gpuAllocation),
                                                                    device, gpuAllocation, 0, initData, size);

    UNRECOVERABLE_IF(!success);

    return gpuAllocation;
}

} // namespace NEO
