/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/surface.h"
#include "opencl/source/mem_obj/mem_obj.h"

namespace NEO {

class MemObjSurface : public Surface {
  public:
    MemObjSurface(MemObj *memObj) : Surface(memObj->getGraphicsAllocation()->isCoherent()), memObj(memObj) {
        memObj->incRefInternal();
    }
    ~MemObjSurface() override {
        memObj->decRefInternal();
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

} // namespace NEO
