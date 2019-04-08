/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/mem_obj/mem_obj.h"
#include "runtime/memory_manager/graphics_allocation.h"

namespace NEO {
class CommandQueue;
class Surface {
  public:
    Surface(bool isCoherent = false) : IsCoherent(isCoherent) {}
    virtual ~Surface() = default;
    virtual void makeResident(CommandStreamReceiver &csr) = 0;
    virtual Surface *duplicate() = 0;
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
        DEBUG_BREAK_IF(!ptr);
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

  protected:
    void *memoryPointer;
    size_t surfaceSize;
    GraphicsAllocation *gfxAllocation;
    bool isPtrCopyAllowed = false;
};

class MemObjSurface : public Surface {
  public:
    MemObjSurface(MemObj *memObj) : Surface(memObj->getGraphicsAllocation()->isCoherent()), memObj(memObj) {
        memObj->retain();
    }
    ~MemObjSurface() override {
        memObj->release();
        memObj = nullptr;
    };

    void makeResident(CommandStreamReceiver &csr) override {
        DEBUG_BREAK_IF(!memObj);
        csr.makeResident(*memObj->getGraphicsAllocation());
    }

    Surface *duplicate() override {
        return new MemObjSurface(this->memObj);
    };

  protected:
    class MemObj *memObj;
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
