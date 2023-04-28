/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstddef>

namespace NEO {

class GraphicsAllocation;
class CommandStreamReceiver;

class Surface {
  public:
    Surface(bool isCoherent = false) : isCoherent(isCoherent) {}
    virtual ~Surface() = default;
    virtual void makeResident(CommandStreamReceiver &csr) = 0;
    virtual Surface *duplicate() = 0;
    virtual bool allowsL3Caching() { return true; }
    bool isCoherent;
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
    HostPtrSurface(const void *ptr, size_t size);

    HostPtrSurface(const void *ptr, size_t size, bool copyAllowed) : HostPtrSurface(ptr, size) {
        isPtrCopyAllowed = copyAllowed;
    }

    HostPtrSurface(const void *ptr, size_t size, GraphicsAllocation *allocation);

    ~HostPtrSurface() override = default;

    void makeResident(CommandStreamReceiver &csr) override;

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

    bool allowsL3Caching() override;

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

    GeneralSurface(GraphicsAllocation *gfxAlloc);

    GeneralSurface(GraphicsAllocation *gfxAlloc, bool needsMigration);

    ~GeneralSurface() override = default;

    void makeResident(CommandStreamReceiver &csr) override;

    Surface *duplicate() override { return new GeneralSurface(gfxAllocation); };
    void setGraphicsAllocation(GraphicsAllocation *newAllocation);

  protected:
    bool needsMigration = false;
    GraphicsAllocation *gfxAllocation;
};
} // namespace NEO
