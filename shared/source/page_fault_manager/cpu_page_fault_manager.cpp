/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/memory_properties_helpers.h"
#include "shared/source/helpers/options.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/utilities/spinlock.h"

#include <algorithm>

namespace NEO {

void CpuPageFaultManager::insertAllocation(void *ptr, size_t size, SVMAllocsManager *unifiedMemoryManager, void *cmdQ, const MemoryProperties &memoryProperties) {
    auto initialPlacement = MemoryPropertiesHelper::getUSMInitialPlacement(memoryProperties);
    const auto domain = (initialPlacement == GraphicsAllocation::UsmInitialPlacement::CPU) ? AllocationDomain::cpu : AllocationDomain::none;

    std::unique_lock<RecursiveSpinLock> lock{mtx};
    PageFaultData faultData{};
    faultData.size = size;
    faultData.unifiedMemoryManager = unifiedMemoryManager;
    faultData.cmdQ = cmdQ;
    faultData.domain = domain;
    this->memoryData.insert(std::make_pair(ptr, faultData));
    unifiedMemoryManager->nonGpuDomainAllocs.push_back(ptr);
    if (initialPlacement != GraphicsAllocation::UsmInitialPlacement::CPU) {
        this->protectCPUMemoryAccess(ptr, size);
    }
}

void CpuPageFaultManager::removeAllocation(void *ptr) {
    std::unique_lock<RecursiveSpinLock> lock{mtx};
    auto alloc = memoryData.find(ptr);
    if (alloc != memoryData.end()) {
        auto &pageFaultData = alloc->second;
        if (pageFaultData.domain == AllocationDomain::gpu) {
            allowCPUMemoryAccess(ptr, pageFaultData.size);
        } else {
            auto &cpuAllocs = pageFaultData.unifiedMemoryManager->nonGpuDomainAllocs;
            if (auto it = std::find(cpuAllocs.begin(), cpuAllocs.end(), ptr); it != cpuAllocs.end()) {
                cpuAllocs.erase(it);
            }
        }
        this->memoryData.erase(ptr);
    }
}

void CpuPageFaultManager::moveAllocationToGpuDomain(void *ptr) {
    std::unique_lock<RecursiveSpinLock> lock{mtx};
    auto alloc = memoryData.find(ptr);
    if (alloc != memoryData.end()) {
        auto &pageFaultData = alloc->second;
        if (pageFaultData.domain == AllocationDomain::cpu || pageFaultData.domain == AllocationDomain::none) {
            this->migrateStorageToGpuDomain(ptr, pageFaultData);

            auto &cpuAllocs = pageFaultData.unifiedMemoryManager->nonGpuDomainAllocs;
            if (auto it = std::find(cpuAllocs.begin(), cpuAllocs.end(), ptr); it != cpuAllocs.end()) {
                cpuAllocs.erase(it);
            }
        }
    }
}

void CpuPageFaultManager::moveAllocationsWithinUMAllocsManagerToGpuDomain(SVMAllocsManager *unifiedMemoryManager) {
    std::unique_lock<RecursiveSpinLock> lock{mtx};
    for (auto allocPtr : unifiedMemoryManager->nonGpuDomainAllocs) {
        auto &pageFaultData = this->memoryData[allocPtr];
        this->migrateStorageToGpuDomain(allocPtr, pageFaultData);
    }
    unifiedMemoryManager->nonGpuDomainAllocs.clear();
}

inline void CpuPageFaultManager::migrateStorageToGpuDomain(void *ptr, PageFaultData &pageFaultData) {
    if (pageFaultData.domain == AllocationDomain::cpu) {
        this->setCpuAllocEvictable(false, ptr, pageFaultData.unifiedMemoryManager);
        this->allowCPUMemoryEviction(false, ptr, pageFaultData);

        std::chrono::steady_clock::time_point start;
        std::chrono::steady_clock::time_point end;

        if (debugManager.flags.RegisterPageFaultHandlerOnMigration.get()) {
            if (this->checkFaultHandlerFromPageFaultManager() == false) {
                this->registerFaultHandler();
            }
        }

        start = std::chrono::steady_clock::now();
        this->transferToGpu(ptr, pageFaultData.cmdQ);
        end = std::chrono::steady_clock::now();
        long long elapsedTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

        PRINT_DEBUG_STRING(debugManager.flags.PrintUmdSharedMigration.get(), stdout, "UMD transferred shared allocation 0x%llx (%zu B) from CPU to GPU (%f us)\n", reinterpret_cast<unsigned long long int>(ptr), pageFaultData.size, elapsedTime / 1e3);

        this->protectCPUMemoryAccess(ptr, pageFaultData.size);
    }
    pageFaultData.domain = AllocationDomain::gpu;
}

void CpuPageFaultManager::handlePageFault(void *ptr, PageFaultData &faultData) {
    this->setAubWritable(true, ptr, faultData.unifiedMemoryManager);
    gpuDomainHandler(this, ptr, faultData);
}

bool CpuPageFaultManager::verifyAndHandlePageFault(void *ptr, bool handleFault) {
    std::unique_lock<RecursiveSpinLock> lock{mtx};
    auto allocPtr = getFaultData(memoryData, ptr, handleFault);
    if (allocPtr == nullptr) {
        return false;
    }
    if (handleFault) {
        handlePageFault(allocPtr, memoryData[allocPtr]);
    }
    return true;
}

void CpuPageFaultManager::setGpuDomainHandler(gpuDomainHandlerFunc gpuHandlerFuncPtr) {
    this->gpuDomainHandler = gpuHandlerFuncPtr;
}

void CpuPageFaultManager::transferAndUnprotectMemory(CpuPageFaultManager *pageFaultHandler, void *allocPtr, PageFaultData &pageFaultData) {
    pageFaultHandler->migrateStorageToCpuDomain(allocPtr, pageFaultData);
    pageFaultHandler->allowCPUMemoryAccess(allocPtr, pageFaultData.size);
    pageFaultHandler->setCpuAllocEvictable(true, allocPtr, pageFaultData.unifiedMemoryManager);
    pageFaultHandler->allowCPUMemoryEviction(true, allocPtr, pageFaultData);
}

void CpuPageFaultManager::unprotectAndTransferMemory(CpuPageFaultManager *pageFaultHandler, void *allocPtr, PageFaultData &pageFaultData) {
    pageFaultHandler->allowCPUMemoryAccess(allocPtr, pageFaultData.size);
    pageFaultHandler->migrateStorageToCpuDomain(allocPtr, pageFaultData);
}

inline void CpuPageFaultManager::migrateStorageToCpuDomain(void *ptr, PageFaultData &pageFaultData) {
    if (pageFaultData.domain == AllocationDomain::gpu) {
        std::chrono::steady_clock::time_point start;
        std::chrono::steady_clock::time_point end;

        start = std::chrono::steady_clock::now();
        this->transferToCpu(ptr, pageFaultData.size, pageFaultData.cmdQ);
        end = std::chrono::steady_clock::now();
        long long elapsedTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

        PRINT_DEBUG_STRING(debugManager.flags.PrintUmdSharedMigration.get(), stdout, "UMD transferred shared allocation 0x%llx (%zu B) from GPU to CPU (%f us)\n", reinterpret_cast<unsigned long long int>(ptr), pageFaultData.size, elapsedTime / 1e3);
        pageFaultData.unifiedMemoryManager->nonGpuDomainAllocs.push_back(ptr);
    }
    pageFaultData.domain = AllocationDomain::cpu;
}

void CpuPageFaultManager::selectGpuDomainHandler() {
    if (debugManager.flags.SetCommandStreamReceiver.get() > static_cast<int32_t>(CommandStreamReceiverType::hardware) || debugManager.flags.NEO_CAL_ENABLED.get()) {
        this->gpuDomainHandler = &CpuPageFaultManager::unprotectAndTransferMemory;
    }
}

void CpuPageFaultManager::setAubWritable(bool writable, void *ptr, SVMAllocsManager *unifiedMemoryManager) {
    UNRECOVERABLE_IF(ptr == nullptr);
    auto gpuAlloc = unifiedMemoryManager->getSVMAlloc(ptr)->gpuAllocations.getDefaultGraphicsAllocation();
    gpuAlloc->setAubWritable(writable, GraphicsAllocation::allBanks);
}

void CpuPageFaultManager::setCpuAllocEvictable(bool evictable, void *ptr, SVMAllocsManager *unifiedMemoryManager) {
    UNRECOVERABLE_IF(ptr == nullptr);
    auto cpuAlloc = unifiedMemoryManager->getSVMAlloc(ptr)->cpuAllocation;
    cpuAlloc->setEvictable(evictable);
}

} // namespace NEO
