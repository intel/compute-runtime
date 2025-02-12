/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/page_fault_manager/tbx_page_fault_manager.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"

namespace NEO {

bool TbxPageFaultManager::verifyAndHandlePageFault(void *ptr, bool handleFault) {
    std::unique_lock<SpinLock> lock{mtxTbx};
    auto allocPtr = getFaultData(memoryDataTbx, ptr, handleFault);
    if (allocPtr == nullptr) {
        return CpuPageFaultManager::verifyAndHandlePageFault(ptr, handleFault);
    }
    if (handleFault) {
        handlePageFault(allocPtr, memoryDataTbx[allocPtr]);
    }
    return true;
}

void TbxPageFaultManager::handlePageFault(void *ptr, PageFaultDataTbx &faultData) {
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
    }
}

void TbxPageFaultManager::removeAllocation(GraphicsAllocation *alloc) {
    std::unique_lock<SpinLock> lock{mtxTbx};
    for (auto &data : memoryDataTbx) {
        auto allocPtr = data.first;
        auto faultData = data.second;
        if (faultData.gfxAllocation == alloc) {
            memoryDataTbx.erase(allocPtr);
            this->allowCPUMemoryAccess(allocPtr, faultData.size);
            return;
        }
    }
}

void TbxPageFaultManager::insertAllocation(CommandStreamReceiver *csr, GraphicsAllocation *alloc, uint32_t bank, void *ptr, size_t size) {
    std::unique_lock<SpinLock> lock{mtxTbx};

    if (this->memoryDataTbx.find(ptr) == this->memoryDataTbx.end()) {
        PageFaultDataTbx pageFaultData{};
        pageFaultData.size = size;
        pageFaultData.gfxAllocation = alloc;
        pageFaultData.bank = bank;
        pageFaultData.csr = csr;
        memoryDataTbx[ptr] = pageFaultData;
    }
    auto &faultData = this->memoryDataTbx[ptr];
    faultData.hasBeenDownloaded = false;
    this->protectCPUMemoryAccess(ptr, size);
}

} // namespace NEO
