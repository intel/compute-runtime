/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/cache_policy.h"
#include "core/memory_manager/graphics_allocation.h"
#include "runtime/command_stream/command_stream_receiver.h"

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
    HostPtrSurface(void *ptr, size_t size) : memoryPointer(ptr), surfaceSize(size) {
        UNRECOVERABLE_IF(!ptr);
        gfxAllocation = nullptr;
    }

    HostPtrSurface(void *ptr, size_t size, bool copyAllowed) : HostPtrSurface(ptr, size) {
        isPtrCopyAllowed = copyAllowed;
    }

    HostPtrSurface(void *ptr, size_t size, GraphicsAllocation *allocation) : memoryPointer(ptr), surfaceSize(size), gfxAllocation(allocation) {
        DEBUG_BREAK_IF(!ptr);
    }
    ~HostPtrSurface() override = default;

    void makeResident(CommandStreamReceiver &csr) override {
        DEBUG_BREAK_IF(!gfxAllocation);
        csr.makeResidentHostPtrAllocation(gfxAllocation);
    }
    Surface *duplicate() override {
        return new HostPtrSurface(this->memoryPointer, this->surfaceSize, this->gfxAllocation);
    };

    void *getMemoryPointer() const {
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

  protected:
    void *memoryPointer;
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
    ~GeneralSurface() override = default;

    void makeResident(CommandStreamReceiver &csr) override {
        csr.makeResident(*gfxAllocation);
    };
    Surface *duplicate() override { return new GeneralSurface(gfxAllocation); };
    void setGraphicsAllocation(GraphicsAllocation *newAllocation) {
        gfxAllocation = newAllocation;
        IsCoherent = newAllocation->isCoherent();
    }

  protected:
    GraphicsAllocation *gfxAllocation;
};
} // namespace NEO
