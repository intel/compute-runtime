/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/page_fault_manager/tbx_page_fault_manager.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"

namespace NEO {

void TbxPageFaultManager::handlePageFault(void *ptr, PageFaultData &faultData) {
    if (faultData.faultType == FaultMode::cpu) {
        CpuPageFaultManager::handlePageFault(ptr, faultData);
        return;
    }
    unprotectAndTransferMemoryTbx(this, ptr, faultData);
}

void TbxPageFaultManager::unprotectAndTransferMemoryTbx(TbxPageFaultManager *pageFaultHandler, void *allocPtr, PageFaultData &pageFaultData) {
    pageFaultHandler->allowCPUMemoryAccess(allocPtr, pageFaultData.size);
    pageFaultData.csr->downloadAllocation(*pageFaultData.gfxAllocation);
    pageFaultData.gfxAllocation->setTbxWritable(true, GraphicsAllocation::allBanks);
}

void TbxPageFaultManager::removeAllocation(GraphicsAllocation *alloc) {
    std::unique_lock<SpinLock> lock{mtx};
    for (auto &data : memoryData) {
        auto allocPtr = data.first;
        auto faultData = data.second;
        if (faultData.gfxAllocation == alloc) {
            memoryData.erase(allocPtr);
            this->allowCPUMemoryAccess(allocPtr, faultData.size);
            return;
        }
    }
}

void TbxPageFaultManager::insertAllocation(CommandStreamReceiver *csr, GraphicsAllocation *alloc, void *ptr, size_t size) {
    std::unique_lock<SpinLock> lock{mtx};

    if (memoryData.find(ptr) == memoryData.end()) {
        PageFaultData pageFaultData{};
        pageFaultData.faultType = FaultMode::tbx;
        pageFaultData.size = size;
        pageFaultData.gfxAllocation = alloc;
        pageFaultData.csr = csr;
        memoryData[ptr] = pageFaultData;
    }
    this->protectCPUMemoryAccess(ptr, size);
}

} // namespace NEO
