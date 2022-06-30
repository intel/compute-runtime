/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"

namespace NEO {
void PageFaultManager::transferToCpu(void *ptr, size_t size, void *device) {
    L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>(device);

    NEO::SvmAllocationData *allocData = deviceImp->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    UNRECOVERABLE_IF(allocData == nullptr);

    auto ret =
        deviceImp->pageFaultCommandList->appendPageFaultCopy(allocData->cpuAllocation,
                                                             allocData->gpuAllocations.getGraphicsAllocation(deviceImp->getRootDeviceIndex()),
                                                             allocData->size, true);
    UNRECOVERABLE_IF(ret);
}
void PageFaultManager::transferToGpu(void *ptr, void *device) {
    L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>(device);

    NEO::SvmAllocationData *allocData = deviceImp->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    UNRECOVERABLE_IF(allocData == nullptr);

    auto ret =
        deviceImp->pageFaultCommandList->appendPageFaultCopy(allocData->gpuAllocations.getGraphicsAllocation(deviceImp->getRootDeviceIndex()),
                                                             allocData->cpuAllocation,
                                                             allocData->size, false);
    UNRECOVERABLE_IF(ret);

    this->evictMemoryAfterImplCopy(allocData->cpuAllocation, deviceImp->getNEODevice());
}
} // namespace NEO

namespace L0 {
void handleGpuDomainTransferForHwWithHints(NEO::PageFaultManager *pageFaultHandler, void *allocPtr, NEO::PageFaultManager::PageFaultData &pageFaultData) {
    bool migration = true;
    if (pageFaultData.domain == NEO::PageFaultManager::AllocationDomain::Gpu) {
        L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>(pageFaultData.cmdQ);
        NEO::SvmAllocationData *allocData = deviceImp->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(allocPtr);

        if (deviceImp->memAdviseSharedAllocations.find(allocData) != deviceImp->memAdviseSharedAllocations.end()) {
            if (deviceImp->memAdviseSharedAllocations[allocData].read_only && deviceImp->memAdviseSharedAllocations[allocData].device_preferred_location) {
                migration = false;
                deviceImp->memAdviseSharedAllocations[allocData].cpu_migration_blocked = 1;
            }
        }
        if (migration) {
            if (NEO::DebugManager.flags.PrintUmdSharedMigration.get()) {
                printf("UMD transferring shared allocation %llx from GPU to CPU\n", reinterpret_cast<unsigned long long int>(allocPtr));
            }
            pageFaultHandler->transferToCpu(allocPtr, pageFaultData.size, pageFaultData.cmdQ);
        }
    }
    if (migration) {
        pageFaultData.domain = NEO::PageFaultManager::AllocationDomain::Cpu;
    }
    pageFaultHandler->allowCPUMemoryAccess(allocPtr, pageFaultData.size);
}

} // namespace L0
