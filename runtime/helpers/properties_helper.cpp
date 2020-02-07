/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/properties_helper.h"

#include "core/helpers/timestamp_packet.h"
#include "core/memory_manager/memory_manager.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/helpers/mipmap.h"
#include "runtime/mem_obj/image.h"
#include "runtime/mem_obj/mem_obj.h"

namespace NEO {

void EventsRequest::fillCsrDependencies(CsrDependencies &csrDeps, CommandStreamReceiver &currentCsr, CsrDependencies::DependenciesType depsType) const {
    for (cl_uint i = 0; i < this->numEventsInWaitList; i++) {
        auto event = castToObjectOrAbort<Event>(this->eventWaitList[i]);
        if (event->isUserEvent()) {
            continue;
        }

        auto timestampPacketContainer = event->getTimestampPacketNodes();
        if (!timestampPacketContainer || timestampPacketContainer->peekNodes().empty()) {
            continue;
        }

        auto sameCsr = (&event->getCommandQueue()->getGpgpuCommandStreamReceiver() == &currentCsr);
        bool pushDependency = (CsrDependencies::DependenciesType::OnCsr == depsType && sameCsr) ||
                              (CsrDependencies::DependenciesType::OutOfCsr == depsType && !sameCsr) ||
                              (CsrDependencies::DependenciesType::All == depsType);

        if (pushDependency) {
            csrDeps.push_back(timestampPacketContainer);
        }
    }
}

TransferProperties::TransferProperties(MemObj *memObj, cl_command_type cmdType, cl_map_flags mapFlags, bool blocking,
                                       size_t *offsetPtr, size_t *sizePtr, void *ptr, bool doTransferOnCpu)
    : memObj(memObj), ptr(ptr), cmdType(cmdType), mapFlags(mapFlags), blocking(blocking), doTransferOnCpu(doTransferOnCpu) {

    // no size or offset passed for unmap operation
    if (cmdType != CL_COMMAND_UNMAP_MEM_OBJECT) {
        if (memObj->peekClMemObjType() == CL_MEM_OBJECT_BUFFER) {
            size[0] = *sizePtr;
            offset[0] = *offsetPtr;
            if (doTransferOnCpu &&
                (false == MemoryPool::isSystemMemoryPool(memObj->getGraphicsAllocation()->getMemoryPool())) &&
                (memObj->getMemoryManager() != nullptr)) {
                this->lockedPtr = memObj->getMemoryManager()->lockResource(memObj->getGraphicsAllocation());
            }
        } else {
            size = {{sizePtr[0], sizePtr[1], sizePtr[2]}};
            offset = {{offsetPtr[0], offsetPtr[1], offsetPtr[2]}};
            if (isMipMapped(memObj)) {
                // decompose origin to coordinates and miplevel
                mipLevel = findMipLevel(memObj->peekClMemObjType(), offsetPtr);
                mipPtrOffset = getMipOffset(castToObjectOrAbort<Image>(memObj), offsetPtr);
                auto mipLevelIdx = getMipLevelOriginIdx(memObj->peekClMemObjType());
                if (mipLevelIdx < offset.size()) {
                    offset[mipLevelIdx] = 0;
                }
            }
        }
    }
}

void *TransferProperties::getCpuPtrForReadWrite() {
    return ptrOffset(lockedPtr ? ptrOffset(lockedPtr, memObj->getOffset()) : memObj->getCpuAddressForMemoryTransfer(), offset[0]);
}

} // namespace NEO
