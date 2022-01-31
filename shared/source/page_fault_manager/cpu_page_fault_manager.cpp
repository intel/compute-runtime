/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/memory_properties_helpers.h"
#include "shared/source/helpers/options.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/unified_memory_manager.h"

#include <mutex>

namespace NEO {
void PageFaultManager::insertAllocation(void *ptr, size_t size, SVMAllocsManager *unifiedMemoryManager, void *cmdQ, const MemoryProperties &memoryProperties) {
    auto initialPlacement = MemoryPropertiesHelper::getUSMInitialPlacement(memoryProperties);
    const auto domain = (initialPlacement == GraphicsAllocation::UsmInitialPlacement::CPU) ? AllocationDomain::Cpu : AllocationDomain::None;

    std::unique_lock<SpinLock> lock{mtx};
    this->memoryData.insert(std::make_pair(ptr, PageFaultData{size, unifiedMemoryManager, cmdQ, domain}));
    if (initialPlacement != GraphicsAllocation::UsmInitialPlacement::CPU) {
        this->protectCPUMemoryAccess(ptr, size);
    }
}

void PageFaultManager::removeAllocation(void *ptr) {
    std::unique_lock<SpinLock> lock{mtx};
    auto alloc = memoryData.find(ptr);
    if (alloc != memoryData.end()) {
        auto &pageFaultData = alloc->second;
        if (pageFaultData.domain == AllocationDomain::Gpu) {
            allowCPUMemoryAccess(ptr, pageFaultData.size);
        }
        this->memoryData.erase(ptr);
    }
}

void PageFaultManager::moveAllocationToGpuDomain(void *ptr) {
    std::unique_lock<SpinLock> lock{mtx};
    auto alloc = memoryData.find(ptr);
    if (alloc != memoryData.end()) {
        auto &pageFaultData = alloc->second;
        if (pageFaultData.domain != AllocationDomain::Gpu) {
            if (pageFaultData.domain == AllocationDomain::Cpu) {
                if (DebugManager.flags.PrintUmdSharedMigration.get()) {
                    printf("UMD transferring shared allocation %llx from CPU to GPU\n", reinterpret_cast<unsigned long long int>(ptr));
                }
                this->transferToGpu(ptr, pageFaultData.cmdQ);
                this->protectCPUMemoryAccess(ptr, pageFaultData.size);
            }
            pageFaultData.domain = AllocationDomain::Gpu;
        }
    }
}

void PageFaultManager::moveAllocationsWithinUMAllocsManagerToGpuDomain(SVMAllocsManager *unifiedMemoryManager) {
    std::unique_lock<SpinLock> lock{mtx};
    for (auto &alloc : this->memoryData) {
        auto allocPtr = alloc.first;
        auto &pageFaultData = alloc.second;
        if (pageFaultData.unifiedMemoryManager == unifiedMemoryManager && pageFaultData.domain != AllocationDomain::Gpu) {
            if (pageFaultData.domain == AllocationDomain::Cpu) {
                if (DebugManager.flags.PrintUmdSharedMigration.get()) {
                    printf("UMD transferring shared allocation %llx from CPU to GPU\n", reinterpret_cast<unsigned long long int>(allocPtr));
                }
                this->transferToGpu(allocPtr, pageFaultData.cmdQ);
                this->protectCPUMemoryAccess(allocPtr, pageFaultData.size);
            }
            pageFaultData.domain = AllocationDomain::Gpu;
        }
    }
}

bool PageFaultManager::verifyPageFault(void *ptr) {
    std::unique_lock<SpinLock> lock{mtx};
    for (auto &alloc : this->memoryData) {
        auto allocPtr = alloc.first;
        auto &pageFaultData = alloc.second;
        if (ptr >= allocPtr && ptr < ptrOffset(allocPtr, pageFaultData.size)) {
            this->setAubWritable(true, allocPtr, pageFaultData.unifiedMemoryManager);
            gpuDomainHandler(this, allocPtr, pageFaultData);
            return true;
        }
    }
    return false;
}

void PageFaultManager::setGpuDomainHandler(gpuDomainHandlerFunc gpuHandlerFuncPtr) {
    this->gpuDomainHandler = gpuHandlerFuncPtr;
}

void PageFaultManager::handleGpuDomainTransferForHw(PageFaultManager *pageFaultHandler, void *allocPtr, PageFaultData &pageFaultData) {
    if (pageFaultData.domain == AllocationDomain::Gpu) {
        if (DebugManager.flags.PrintUmdSharedMigration.get()) {
            printf("UMD transferring shared allocation %llx from GPU to CPU\n", reinterpret_cast<unsigned long long int>(allocPtr));
        }
        pageFaultHandler->transferToCpu(allocPtr, pageFaultData.size, pageFaultData.cmdQ);
    }
    pageFaultData.domain = AllocationDomain::Cpu;
    pageFaultHandler->allowCPUMemoryAccess(allocPtr, pageFaultData.size);
}

void PageFaultManager::handleGpuDomainTransferForAubAndTbx(PageFaultManager *pageFaultHandler, void *allocPtr, PageFaultData &pageFaultData) {
    pageFaultHandler->allowCPUMemoryAccess(allocPtr, pageFaultData.size);

    if (pageFaultData.domain == AllocationDomain::Gpu) {
        if (DebugManager.flags.PrintUmdSharedMigration.get()) {
            printf("UMD transferring shared allocation %llx from GPU to CPU\n", reinterpret_cast<unsigned long long int>(allocPtr));
        }
        pageFaultHandler->transferToCpu(allocPtr, pageFaultData.size, pageFaultData.cmdQ);
    }
    pageFaultData.domain = AllocationDomain::Cpu;
}

void PageFaultManager::selectGpuDomainHandler() {
    if (DebugManager.flags.SetCommandStreamReceiver.get() == CommandStreamReceiverType::CSR_AUB ||
        DebugManager.flags.SetCommandStreamReceiver.get() == CommandStreamReceiverType::CSR_TBX ||
        DebugManager.flags.SetCommandStreamReceiver.get() == CommandStreamReceiverType::CSR_TBX_WITH_AUB) {
        this->gpuDomainHandler = &PageFaultManager::handleGpuDomainTransferForAubAndTbx;
    }
}

void PageFaultManager::setAubWritable(bool writable, void *ptr, SVMAllocsManager *unifiedMemoryManager) {
    UNRECOVERABLE_IF(ptr == nullptr);
    auto gpuAlloc = unifiedMemoryManager->getSVMAlloc(ptr)->gpuAllocations.getDefaultGraphicsAllocation();
    gpuAlloc->setAubWritable(writable, GraphicsAllocation::allBanks);
}

} // namespace NEO
