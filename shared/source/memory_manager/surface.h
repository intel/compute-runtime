/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_context.h"

namespace NEO {

class Surface {
  public:
    Surface(bool isCoherent = false) : IsCoherent(isCoherent) {}
    virtual ~Surface() = default;
    virtual void makeResident(CommandStreamReceiver &csr) = 0;
    virtual Surface *duplicate() = 0;
    virtual bool allowsL3Caching() { return true; }
    bool IsCoherent;
};

class NullSurface : public Surface {
  public:
    NullSurface(){};
    ~NullSurface() override = default;

    void makeResident(CommandStreamReceiver &csr) override{};
    Surface *duplicate() override { return new NullSurface(); };
};

class HostPtrSurface : public Surface {
  public:
    HostPtrSurface(const void *ptr, size_t size) : memoryPointer(ptr), surfaceSize(size) {
        UNRECOVERABLE_IF(!ptr);
        gfxAllocation = nullptr;
    }

    HostPtrSurface(const void *ptr, size_t size, bool copyAllowed) : HostPtrSurface(ptr, size) {
        isPtrCopyAllowed = copyAllowed;
    }

    HostPtrSurface(const void *ptr, size_t size, GraphicsAllocation *allocation) : memoryPointer(ptr), surfaceSize(size), gfxAllocation(allocation) {
        DEBUG_BREAK_IF(!ptr);
    }
    ~HostPtrSurface() override = default;

    void makeResident(CommandStreamReceiver &csr) override {
        DEBUG_BREAK_IF(!gfxAllocation);
        gfxAllocation->prepareHostPtrForResidency(&csr);
        csr.makeResidentHostPtrAllocation(gfxAllocation);
    }
    Surface *duplicate() override {
        return new HostPtrSurface(this->memoryPointer, this->surfaceSize, this->gfxAllocation);
    };

    const void *getMemoryPointer() const {
        return memoryPointer;
    }
    size_t getSurfaceSize() const {
        return surfaceSize;
    }

    void setAllocation(GraphicsAllocation *allocation) {
        this->gfxAllocation = allocation;
    }

    GraphicsAllocation *getAllocation() {
        return gfxAllocation;
    }

    bool peekIsPtrCopyAllowed() {
        return isPtrCopyAllowed;
    }

    bool allowsL3Caching() override {
        return isL3Capable(*gfxAllocation);
    }

    void setIsPtrCopyAllowed(bool allowed) {
        this->isPtrCopyAllowed = allowed;
    }

  protected:
    const void *memoryPointer;
    size_t surfaceSize;
    GraphicsAllocation *gfxAllocation;
    bool isPtrCopyAllowed = false;
};

class GeneralSurface : public Surface {
  public:
    GeneralSurface() : Surface(false) {
        gfxAllocation = nullptr;
    }
    GeneralSurface(GraphicsAllocation *gfxAlloc) : Surface(gfxAlloc->isCoherent()) {
        gfxAllocation = gfxAlloc;
    };
    GeneralSurface(GraphicsAllocation *gfxAlloc, bool needsMigration) : GeneralSurface(gfxAlloc) {
        this->needsMigration = needsMigration;
    }
    ~GeneralSurface() override = default;

    void makeResident(CommandStreamReceiver &csr) override {
        if (needsMigration) {
            csr.getMemoryManager()->getPageFaultManager()->moveAllocationToGpuDomain(reinterpret_cast<void *>(gfxAllocation->getGpuAddress()));
        }
        csr.makeResident(*gfxAllocation);
    };
    Surface *duplicate() override { return new GeneralSurface(gfxAllocation); };
    void setGraphicsAllocation(GraphicsAllocation *newAllocation) {
        gfxAllocation = newAllocation;
        IsCoherent = newAllocation->isCoherent();
    }

  protected:
    bool needsMigration = false;
    GraphicsAllocation *gfxAllocation;
};
} // namespace NEO
