/*
 * Copyright (C) 2020 Intel Corporation
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
    if (linkerInput != nullptr) {
        globalsAreExported = constant ? linkerInput->getTraits().exportsGlobalConstants : linkerInput->getTraits().exportsGlobalVariables;
    }

    if (globalsAreExported && (svmAllocManager != nullptr)) {
        NEO::SVMAllocsManager::SvmAllocationProperties svmProps = {};
        svmProps.coherent = false;
        svmProps.readOnly = constant;
        svmProps.hostPtrReadOnly = constant;
        auto ptr = svmAllocManager->createSVMAlloc(device.getRootDeviceIndex(), size, svmProps, device.getDeviceBitfield());
        DEBUG_BREAK_IF(ptr == nullptr);
        if (ptr == nullptr) {
            return nullptr;
        }
        auto svmAlloc = svmAllocManager->getSVMAlloc(ptr);
        UNRECOVERABLE_IF(svmAlloc == nullptr);
        auto gpuAlloc = svmAlloc->gpuAllocations.getGraphicsAllocation(device.getRootDeviceIndex());
        UNRECOVERABLE_IF(gpuAlloc == nullptr);
        device.getMemoryManager()->copyMemoryToAllocation(gpuAlloc, initData, static_cast<uint32_t>(size));
        return gpuAlloc;
    } else {
        auto allocationType = constant ? GraphicsAllocation::AllocationType::CONSTANT_SURFACE : GraphicsAllocation::AllocationType::GLOBAL_SURFACE;
        auto gpuAlloc = device.getMemoryManager()->allocateGraphicsMemoryWithProperties({device.getRootDeviceIndex(),
                                                                                         true, // allocateMemory
                                                                                         size, allocationType,
                                                                                         false, // isMultiStorageAllocation
                                                                                         device.getDeviceBitfield()});
        DEBUG_BREAK_IF(gpuAlloc == nullptr);
        if (gpuAlloc == nullptr) {
            return nullptr;
        }
        auto &hwInfo = device.getHardwareInfo();
        auto &helper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
        if (gpuAlloc->isAllocatedInLocalMemoryPool() && helper.isBlitCopyRequiredForLocalMemory(hwInfo)) {
            BlitHelperFunctions::blitMemoryToAllocation(device, gpuAlloc, 0, initData, {size, 1, 1});
        } else {
            memcpy_s(gpuAlloc->getUnderlyingBuffer(), gpuAlloc->getUnderlyingBufferSize(), initData, size);
        }
        return gpuAlloc;
    }
}

} // namespace NEO
