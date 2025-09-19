/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "program_initialization.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/compiler_interface/linker.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/memory_manager/unified_memory_pooling.h"
#include "shared/source/program/program_info.h"

#include <mutex>

namespace NEO {

SharedPoolAllocation *allocateGlobalsSurface(NEO::SVMAllocsManager *const svmAllocManager, NEO::Device &device, size_t totalSize, size_t zeroInitSize, bool constant,
                                             LinkerInput *const linkerInput, const void *initData) {
    size_t allocationOffset{0u};
    size_t allocatedSize{0u};
    bool globalsAreExported = false;
    GraphicsAllocation *gpuAllocation = nullptr;
    bool isAllocatedFromPool = false;
    std::mutex *usmAllocPoolMutex = nullptr;
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

        UsmMemAllocPool *allocPool = nullptr;
        if (allocationType == AllocationType::constantSurface) {
            allocPool = device.getUsmConstantSurfaceAllocPool();
        } else {
            allocPool = device.getUsmGlobalSurfaceAllocPool();
        }

        if (allocPool && device.getProductHelper().is2MBLocalMemAlignmentEnabled()) {
            if (!allocPool->isInitialized()) {
                constexpr size_t alignment = MemoryConstants::pageSize2M;
                constexpr size_t poolSize = MemoryConstants::pageSize2M;
                constexpr size_t minServicedSize = 0u;
                constexpr size_t maxServicedSize = 2 * MemoryConstants::megaByte;

                NEO::SVMAllocsManager::UnifiedMemoryProperties poolMemoryProperties(InternalMemoryType::deviceUnifiedMemory, alignment, rootDeviceIndices, subDeviceBitfields);
                poolMemoryProperties.device = &device;
                poolMemoryProperties.requestedAllocationType = allocationType;
                poolMemoryProperties.isInternalAllocation = true;

                allocPool->initialize(svmAllocManager, poolMemoryProperties, poolSize, minServicedSize, maxServicedSize);
            }

            if (allocPool->isInitialized()) {
                unifiedMemoryProperties.alignment = MemoryConstants::pageSize;
                auto pooledPtr = allocPool->createUnifiedMemoryAllocation(totalSize, unifiedMemoryProperties);
                if (pooledPtr) {
                    allocationOffset = allocPool->getOffsetInPool(pooledPtr);
                    allocatedSize = allocPool->getPooledAllocationSize(pooledPtr);
                    auto usmAlloc = svmAllocManager->getSVMAlloc(reinterpret_cast<void *>(allocPool->getPoolAddress()));
                    UNRECOVERABLE_IF(usmAlloc == nullptr);
                    gpuAllocation = usmAlloc->gpuAllocations.getGraphicsAllocation(rootDeviceIndex);
                    usmAllocPoolMutex = &allocPool->getMutex();
                    isAllocatedFromPool = true;
                }
            }
        }

        if (!gpuAllocation) {
            auto ptr = svmAllocManager->createUnifiedMemoryAllocation(totalSize, unifiedMemoryProperties);
            DEBUG_BREAK_IF(ptr == nullptr);
            if (ptr == nullptr) {
                return nullptr;
            }
            auto usmAlloc = svmAllocManager->getSVMAlloc(ptr);
            UNRECOVERABLE_IF(usmAlloc == nullptr);
            gpuAllocation = usmAlloc->gpuAllocations.getGraphicsAllocation(rootDeviceIndex);
            allocationOffset = 0u;
            allocatedSize = gpuAllocation->getUnderlyingBufferSize();
        }
    } else {
        gpuAllocation = device.getMemoryManager()->allocateGraphicsMemoryWithProperties({rootDeviceIndex,
                                                                                         true, // allocateMemory
                                                                                         totalSize, allocationType,
                                                                                         false, // isMultiStorageAllocation
                                                                                         deviceBitfield});
        if (nullptr == gpuAllocation) {
            return nullptr;
        }
        allocationOffset = 0u;
        allocatedSize = gpuAllocation->getUnderlyingBufferSize();
    }

    auto &rootDeviceEnvironment = device.getRootDeviceEnvironment();
    auto &productHelper = device.getProductHelper();

    bool isOnlyBssData = (totalSize == zeroInitSize);
    if (false == isOnlyBssData) {
        auto initSize = totalSize - zeroInitSize;
        auto success = MemoryTransferHelper::transferMemoryToAllocation(productHelper.isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, *gpuAllocation),
                                                                        device, gpuAllocation, allocationOffset, initData, initSize);
        UNRECOVERABLE_IF(!success);

        if (auto csr = device.getDefaultEngine().commandStreamReceiver;
            isAllocatedFromPool && csr->getType() != NEO::CommandStreamReceiverType::hardware) {
            auto writeMemoryOperation = [&]() {
                constexpr uint32_t allBanks = std::numeric_limits<uint32_t>::max();
                if (gpuAllocation->isTbxWritable(allBanks)) {
                    // initialize full page tables for the first time
                    csr->writeMemory(*gpuAllocation, false, 0, 0);
                }
                gpuAllocation->setTbxWritable(true, allBanks);
                [[maybe_unused]] const auto writeMemoryStatus = csr->writeMemory(*gpuAllocation, true, allocationOffset, allocatedSize);
                DEBUG_BREAK_IF(!writeMemoryStatus);
                gpuAllocation->setTbxWritable(false, allBanks);
            };

            if (usmAllocPoolMutex) {
                std::lock_guard<std::mutex> lock(*usmAllocPoolMutex);
                writeMemoryOperation();
            } else {
                writeMemoryOperation();
            }
        }
    }
    return new SharedPoolAllocation(gpuAllocation, allocationOffset, allocatedSize, nullptr);
}

} // namespace NEO
