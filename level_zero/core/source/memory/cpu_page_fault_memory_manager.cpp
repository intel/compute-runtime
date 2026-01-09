/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"

namespace NEO {
void CpuPageFaultManager::transferToCpu(void *ptr, size_t size, void *device) {
    L0::Device *l0Device = static_cast<L0::Device *>(device);
    l0Device->getNEODevice()->stopDirectSubmissionForCopyEngine();

    NEO::SvmAllocationData *allocData = l0Device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    UNRECOVERABLE_IF(allocData == nullptr);

    auto ret =
        l0Device->pageFaultCommandList->appendPageFaultCopy(allocData->cpuAllocation,
                                                            allocData->gpuAllocations.getGraphicsAllocation(l0Device->getRootDeviceIndex()),
                                                            allocData->size, true);
    UNRECOVERABLE_IF(ret);
}
void CpuPageFaultManager::transferToGpu(void *ptr, void *device) {
    L0::Device *l0Device = static_cast<L0::Device *>(device);
    l0Device->getNEODevice()->stopDirectSubmissionForCopyEngine();

    NEO::SvmAllocationData *allocData = l0Device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    UNRECOVERABLE_IF(allocData == nullptr);

    auto ret =
        l0Device->pageFaultCommandList->appendPageFaultCopy(allocData->gpuAllocations.getGraphicsAllocation(l0Device->getRootDeviceIndex()),
                                                            allocData->cpuAllocation,
                                                            allocData->size, false);
    UNRECOVERABLE_IF(ret);
}
void CpuPageFaultManager::allowCPUMemoryEviction(bool evict, void *ptr, PageFaultData &pageFaultData) {
    L0::Device *l0Device = static_cast<L0::Device *>(pageFaultData.cmdQ);

    CommandStreamReceiver *csr = nullptr;
    if (l0Device->getActiveDevice()->getInternalCopyEngine()) {
        csr = l0Device->getActiveDevice()->getInternalCopyEngine()->commandStreamReceiver;
    } else {
        csr = l0Device->getActiveDevice()->getInternalEngine().commandStreamReceiver;
    }
    UNRECOVERABLE_IF(csr == nullptr);
    auto osInterface = l0Device->getNEODevice()->getRootDeviceEnvironment().osInterface.get();

    allowCPUMemoryEvictionImpl(evict, ptr, *csr, osInterface);
}
} // namespace NEO

namespace L0 {
void transferAndUnprotectMemoryWithHints(NEO::CpuPageFaultManager *pageFaultHandler, void *allocPtr, NEO::CpuPageFaultManager::PageFaultData &pageFaultData) {
    bool migration = true;
    if (pageFaultData.domain == NEO::CpuPageFaultManager::AllocationDomain::gpu) {
        L0::Device *l0Device = static_cast<L0::Device *>(pageFaultData.cmdQ);
        NEO::SvmAllocationData *allocData = l0Device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(allocPtr);

        if (l0Device->memAdviseSharedAllocations.find(allocData) != l0Device->memAdviseSharedAllocations.end()) {
            if (l0Device->memAdviseSharedAllocations[allocData].readOnly && l0Device->memAdviseSharedAllocations[allocData].devicePreferredLocation) {
                migration = false;
                l0Device->memAdviseSharedAllocations[allocData].cpuMigrationBlocked = 1;
            }
        }
        if (migration) {
            std::chrono::steady_clock::time_point start;
            std::chrono::steady_clock::time_point end;

            start = std::chrono::steady_clock::now();
            pageFaultHandler->transferToCpu(allocPtr, pageFaultData.size, pageFaultData.cmdQ);
            end = std::chrono::steady_clock::now();
            long long elapsedTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            pageFaultData.unifiedMemoryManager->nonGpuDomainAllocs.push_back(allocPtr);

            PRINT_STRING(NEO::debugManager.flags.PrintUmdSharedMigration.get(), stdout, "UMD transferred shared allocation 0x%llx (%zu B) from GPU to CPU (%f us)\n", reinterpret_cast<unsigned long long int>(allocPtr), pageFaultData.size, elapsedTime / 1e3);
        }
    }
    if (migration) {
        pageFaultData.domain = NEO::CpuPageFaultManager::AllocationDomain::cpu;
    }
    pageFaultHandler->allowCPUMemoryAccess(allocPtr, pageFaultData.size);
}

} // namespace L0
