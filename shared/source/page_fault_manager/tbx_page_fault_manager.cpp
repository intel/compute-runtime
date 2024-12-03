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
    auto &graphicsAllocation = *faultData.gfxAllocation;
    auto bank = faultData.bank;
    auto hasBeenDownloaded = faultData.hasBeenDownloaded;
    auto size = faultData.size;
    auto csr = faultData.csr;
    if (!hasBeenDownloaded) {
        this->allowCPUMemoryAccess(ptr, size);
        csr->downloadAllocation(graphicsAllocation);
        this->protectCpuMemoryFromWrites(ptr, size);
        faultData.hasBeenDownloaded = true;
    } else {
        graphicsAllocation.setTbxWritable(true, bank);
        this->allowCPUMemoryAccess(ptr, size);
        this->memoryData.erase(ptr);
    }
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

void TbxPageFaultManager::insertAllocation(CommandStreamReceiver *csr, GraphicsAllocation *alloc, uint32_t bank, void *ptr, size_t size) {
    std::unique_lock<SpinLock> lock{mtx};

    if (this->memoryData.find(ptr) == this->memoryData.end()) {
        PageFaultData pageFaultData{};
        pageFaultData.faultType = FaultMode::tbx;
        pageFaultData.size = size;
        pageFaultData.gfxAllocation = alloc;
        pageFaultData.bank = bank;
        pageFaultData.csr = csr;
        memoryData[ptr] = pageFaultData;
    }
    auto &faultData = this->memoryData[ptr];
    faultData.hasBeenDownloaded = false;
    this->protectCPUMemoryAccess(ptr, size);
}

} // namespace NEO
