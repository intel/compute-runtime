/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/blit_commands_helper.h"

#include "runtime/helpers/timestamp_packet.h"
#include "runtime/memory_manager/surface.h"

#include "CL/cl.h"

namespace NEO {
BlitProperties BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitWithHostPtrDirection copyDirection,
                                                                     CommandStreamReceiver &commandStreamReceiver,
                                                                     GraphicsAllocation *memObjAllocation, void *hostPtr, bool blocking,
                                                                     size_t offset, uint64_t copySize) {
    GraphicsAllocation *hostPtrAllocation = nullptr;
    if (hostPtr) {
        HostPtrSurface hostPtrSurface(hostPtr, static_cast<size_t>(copySize), true);
        bool success = commandStreamReceiver.createAllocationForHostSurface(hostPtrSurface, false);
        UNRECOVERABLE_IF(!success);
        hostPtrAllocation = hostPtrSurface.getAllocation();
    }

    if (BlitterConstants::BlitWithHostPtrDirection::FromHostPtr == copyDirection) {
        return {nullptr, copyDirection, {}, memObjAllocation, hostPtrAllocation, hostPtr, blocking, offset, 0, copySize};
    } else {
        return {nullptr, copyDirection, {}, hostPtrAllocation, memObjAllocation, hostPtr, blocking, 0, offset, copySize};
    }
}

} // namespace NEO
