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
        NEO::SVMAllocsManager::SvmAllocationProperties svmProps = {};
        svmProps.coherent = false;
        svmProps.readOnly = constant;
        svmProps.hostPtrReadOnly = constant;

        std::set<uint32_t> rootDeviceIndices;
        rootDeviceIndices.insert(rootDeviceIndex);
        std::map<uint32_t, DeviceBitfield> subDeviceBitfields;
        subDeviceBitfields.insert({rootDeviceIndex, deviceBitfield});
        auto ptr = svmAllocManager->createSVMAlloc(size, svmProps, rootDeviceIndices, subDeviceBitfields);
        DEBUG_BREAK_IF(ptr == nullptr);
        if (ptr == nullptr) {
            return nullptr;
        }
        auto svmAlloc = svmAllocManager->getSVMAlloc(ptr);
        UNRECOVERABLE_IF(svmAlloc == nullptr);
        gpuAllocation = svmAlloc->gpuAllocations.getGraphicsAllocation(rootDeviceIndex);
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
    auto &helper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    auto success = MemoryTransferHelper::transferMemoryToAllocation(helper.isBlitCopyRequiredForLocalMemory(hwInfo, *gpuAllocation),
                                                                    device, gpuAllocation, 0, initData, size);

    UNRECOVERABLE_IF(!success);

    return gpuAllocation;
}

} // namespace NEO
