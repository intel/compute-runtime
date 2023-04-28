/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/surface.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"

namespace NEO {

HostPtrSurface::HostPtrSurface(const void *ptr, size_t size) : memoryPointer(ptr), surfaceSize(size) {
    UNRECOVERABLE_IF(!ptr);
    gfxAllocation = nullptr;
}

HostPtrSurface::HostPtrSurface(const void *ptr, size_t size, GraphicsAllocation *allocation) : memoryPointer(ptr), surfaceSize(size), gfxAllocation(allocation) {
    DEBUG_BREAK_IF(!ptr);
}

void HostPtrSurface::makeResident(CommandStreamReceiver &csr) {
    DEBUG_BREAK_IF(!gfxAllocation);
    gfxAllocation->prepareHostPtrForResidency(&csr);
    csr.makeResidentHostPtrAllocation(gfxAllocation);
}

bool HostPtrSurface::allowsL3Caching() {
    return isL3Capable(*gfxAllocation);
}

GeneralSurface::GeneralSurface(GraphicsAllocation *gfxAlloc) : Surface(gfxAlloc->isCoherent()) {
    gfxAllocation = gfxAlloc;
}

GeneralSurface::GeneralSurface(GraphicsAllocation *gfxAlloc, bool needsMigration) : GeneralSurface(gfxAlloc) {
    this->needsMigration = needsMigration;
}

void GeneralSurface::makeResident(CommandStreamReceiver &csr) {
    if (needsMigration) {
        csr.getMemoryManager()->getPageFaultManager()->moveAllocationToGpuDomain(reinterpret_cast<void *>(gfxAllocation->getGpuAddress()));
    }
    csr.makeResident(*gfxAllocation);
};

void GeneralSurface::setGraphicsAllocation(GraphicsAllocation *newAllocation) {
    gfxAllocation = newAllocation;
    isCoherent = newAllocation->isCoherent();
}

} // namespace NEO