/*
 * Copyright (C) 2019-2020 Intel Corporation
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
                                                             allocData->gpuAllocation,
                                                             allocData->size, true);
    UNRECOVERABLE_IF(ret);
}
void PageFaultManager::transferToGpu(void *ptr, void *device) {
    L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>(device);

    NEO::SvmAllocationData *allocData = deviceImp->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    UNRECOVERABLE_IF(allocData == nullptr);

    auto ret =
        deviceImp->pageFaultCommandList->appendPageFaultCopy(allocData->gpuAllocation,
                                                             allocData->cpuAllocation,
                                                             allocData->size, false);
    UNRECOVERABLE_IF(ret);
}
} // namespace NEO
