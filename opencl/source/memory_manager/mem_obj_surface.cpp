/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/memory_manager/mem_obj_surface.h"

#include "shared/source/command_stream/command_stream_receiver.h"

#include "opencl/source/mem_obj/mem_obj.h"

namespace NEO {

MemObjSurface::MemObjSurface(MemObj *memObj) : Surface(memObj->getMultiGraphicsAllocation().isCoherent()), memObj(memObj) {
    memObj->incRefInternal();
}

MemObjSurface::~MemObjSurface() {
    memObj->decRefInternal();
    memObj = nullptr;
};

void MemObjSurface::makeResident(CommandStreamReceiver &csr) {
    DEBUG_BREAK_IF(!memObj);
    csr.makeResident(*memObj->getGraphicsAllocation(csr.getRootDeviceIndex()));
}

Surface *MemObjSurface::duplicate() {
    return new MemObjSurface(this->memObj);
};

} // namespace NEO