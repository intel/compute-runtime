/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/mem_obj/mem_obj.h"
#include "runtime/memory_manager/graphics_allocation.h"

namespace OCLRT {

class Surface {
  public:
    Surface(bool isCoherent = false) : IsCoherent(isCoherent) {}
    virtual ~Surface() = default;
    virtual void makeResident(CommandStreamReceiver &csr) = 0;
    virtual void setCompletionStamp(CompletionStamp &cs, Device *pDevice, CommandQueue *pCmdQ) = 0;
    virtual Surface *duplicate() = 0;
    const bool IsCoherent;
};

class NullSurface : public Surface {
  public:
    NullSurface(){};
    ~NullSurface() override = default;

    void makeResident(CommandStreamReceiver &csr) override{};
    void setCompletionStamp(CompletionStamp &cs, Device *pDevice, CommandQueue *pCmdQ) override{};
    Surface *duplicate() override { return new NullSurface(); };
};

class HostPtrSurface : public Surface {
  public:
    HostPtrSurface(void *ptr, size_t size) : memory_pointer(ptr), surface_size(size) {
        DEBUG_BREAK_IF(!ptr);
    }
    ~HostPtrSurface() override = default;

    void makeResident(CommandStreamReceiver &csr) override {
        allocation = csr.createAllocationAndHandleResidency(memory_pointer, surface_size);
        DEBUG_BREAK_IF(!allocation);
    }
    void setCompletionStamp(CompletionStamp &cs, Device *pDevice, CommandQueue *pCmdQ) override {
        DEBUG_BREAK_IF(!allocation);
        allocation->taskCount = cs.taskCount;
    }
    Surface *duplicate() override {
        return new HostPtrSurface(this->memory_pointer, this->surface_size);
    };

    void *getMemoryPointer() const {
        return memory_pointer;
    }
    size_t getSurfaceSize() const {
        return surface_size;
    }

  protected:
    GraphicsAllocation *allocation;
    void *memory_pointer;
    size_t surface_size;
};

class MemObjSurface : public Surface {
  public:
    MemObjSurface(MemObj *memObj) : Surface(memObj->getGraphicsAllocation()->isCoherent()), memory_object(memObj) {
        memory_object->retain();
    }
    ~MemObjSurface() override {
        memory_object->release();
        memory_object = nullptr;
    };

    void makeResident(CommandStreamReceiver &csr) override {
        DEBUG_BREAK_IF(!memory_object);
        csr.makeResident(*memory_object->getGraphicsAllocation());
    }
    void setCompletionStamp(CompletionStamp &cs, Device *pDevice, CommandQueue *pCmdQ) override {
        DEBUG_BREAK_IF(!memory_object);
        memory_object->setCompletionStamp(cs, pDevice, pCmdQ);
    }
    Surface *duplicate() override {
        return new MemObjSurface(this->memory_object);
    };

  protected:
    class MemObj *memory_object;
};

class GeneralSurface : public Surface {
  public:
    GeneralSurface(GraphicsAllocation *gfxAlloc) : Surface(gfxAlloc->isCoherent()) {
        gfxAllocation = gfxAlloc;
    };
    ~GeneralSurface() override = default;

    void makeResident(CommandStreamReceiver &csr) override {
        csr.makeResident(*gfxAllocation);
    };
    void setCompletionStamp(CompletionStamp &cs, Device *pDevice, CommandQueue *pCmdQ) override {
        gfxAllocation->taskCount = cs.taskCount;
    };
    Surface *duplicate() override { return new GeneralSurface(gfxAllocation); };

  protected:
    GraphicsAllocation *gfxAllocation;
};
}
