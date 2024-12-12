/*
 * Copyright (C) 2019-2024 Intel Corporation
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
void PageFaultManager::insertAllocation(void *ptr, size_t size, SVMAllocsManager *unifiedMemoryManager, void *cmdQ, const MemoryProperties &memoryProperties) {
    auto initialPlacement = MemoryPropertiesHelper::getUSMInitialPlacement(memoryProperties);
    const auto domain = (initialPlacement == GraphicsAllocation::UsmInitialPlacement::CPU) ? AllocationDomain::cpu : AllocationDomain::none;

    std::unique_lock<SpinLock> lock{mtx};
    this->memoryData.insert(std::make_pair(ptr, PageFaultData{size, unifiedMemoryManager, cmdQ, domain}));
    if (initialPlacement != GraphicsAllocation::UsmInitialPlacement::CPU) {
        this->protectCPUMemoryAccess(ptr, size);
    }
    unifiedMemoryManager->nonGpuDomainAllocs.push_back(ptr);
}

void PageFaultManager::removeAllocation(void *ptr) {
    std::unique_lock<SpinLock> lock{mtx};
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

void PageFaultManager::moveAllocationToGpuDomain(void *ptr) {
    std::unique_lock<SpinLock> lock{mtx};
    auto alloc = memoryData.find(ptr);
    if (alloc != memoryData.end()) {
        auto &pageFaultData = alloc->second;
        if (pageFaultData.domain != AllocationDomain::gpu) {
            this->migrateStorageToGpuDomain(ptr, pageFaultData);

            auto &cpuAllocs = pageFaultData.unifiedMemoryManager->nonGpuDomainAllocs;
            if (auto it = std::find(cpuAllocs.begin(), cpuAllocs.end(), ptr); it != cpuAllocs.end()) {
                cpuAllocs.erase(it);
            }
        }
    }
}

void PageFaultManager::moveAllocationsWithinUMAllocsManagerToGpuDomain(SVMAllocsManager *unifiedMemoryManager) {
    std::unique_lock<SpinLock> lock{mtx};
    for (auto allocPtr : unifiedMemoryManager->nonGpuDomainAllocs) {
        auto &pageFaultData = this->memoryData[allocPtr];
        this->migrateStorageToGpuDomain(allocPtr, pageFaultData);
    }
    unifiedMemoryManager->nonGpuDomainAllocs.clear();
}

inline void PageFaultManager::migrateStorageToGpuDomain(void *ptr, PageFaultData &pageFaultData) {
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

bool PageFaultManager::verifyAndHandlePageFault(void *ptr, bool handlePageFault) {
    std::unique_lock<SpinLock> lock{mtx};
    for (auto &alloc : this->memoryData) {
        auto allocPtr = alloc.first;
        auto &pageFaultData = alloc.second;
        if (ptr >= allocPtr && ptr < ptrOffset(allocPtr, pageFaultData.size)) {
            if (handlePageFault) {
                this->setAubWritable(true, allocPtr, pageFaultData.unifiedMemoryManager);
                gpuDomainHandler(this, allocPtr, pageFaultData);
            }
            return true;
        }
    }
    return false;
}

void PageFaultManager::setGpuDomainHandler(gpuDomainHandlerFunc gpuHandlerFuncPtr) {
    this->gpuDomainHandler = gpuHandlerFuncPtr;
}

void PageFaultManager::transferAndUnprotectMemory(PageFaultManager *pageFaultHandler, void *allocPtr, PageFaultData &pageFaultData) {
    pageFaultHandler->migrateStorageToCpuDomain(allocPtr, pageFaultData);
    pageFaultHandler->allowCPUMemoryAccess(allocPtr, pageFaultData.size);
    pageFaultHandler->setCpuAllocEvictable(true, allocPtr, pageFaultData.unifiedMemoryManager);
    pageFaultHandler->allowCPUMemoryEviction(true, allocPtr, pageFaultData);
}

void PageFaultManager::unprotectAndTransferMemory(PageFaultManager *pageFaultHandler, void *allocPtr, PageFaultData &pageFaultData) {
    pageFaultHandler->allowCPUMemoryAccess(allocPtr, pageFaultData.size);
    pageFaultHandler->migrateStorageToCpuDomain(allocPtr, pageFaultData);
}

inline void PageFaultManager::migrateStorageToCpuDomain(void *ptr, PageFaultData &pageFaultData) {
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

void PageFaultManager::selectGpuDomainHandler() {
    if (debugManager.flags.SetCommandStreamReceiver.get() > static_cast<int32_t>(CommandStreamReceiverType::hardware) || debugManager.flags.NEO_CAL_ENABLED.get()) {
        this->gpuDomainHandler = &PageFaultManager::unprotectAndTransferMemory;
    }
}

void PageFaultManager::setAubWritable(bool writable, void *ptr, SVMAllocsManager *unifiedMemoryManager) {
    UNRECOVERABLE_IF(ptr == nullptr);
    auto gpuAlloc = unifiedMemoryManager->getSVMAlloc(ptr)->gpuAllocations.getDefaultGraphicsAllocation();
    gpuAlloc->setAubWritable(writable, GraphicsAllocation::allBanks);
}

void PageFaultManager::setCpuAllocEvictable(bool evictable, void *ptr, SVMAllocsManager *unifiedMemoryManager) {
    UNRECOVERABLE_IF(ptr == nullptr);
    auto cpuAlloc = unifiedMemoryManager->getSVMAlloc(ptr)->cpuAllocation;
    cpuAlloc->setEvictable(evictable);
}

} // namespace NEO
