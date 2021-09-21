/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/properties_helper.h"

#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/helpers/mipmap.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/mem_obj/mem_obj.h"

namespace NEO {

void EventsRequest::fillCsrDependenciesForTimestampPacketContainer(CsrDependencies &csrDeps, CommandStreamReceiver &currentCsr, CsrDependencies::DependenciesType depsType) const {
    for (cl_uint i = 0; i < this->numEventsInWaitList; i++) {
        auto event = castToObjectOrAbort<Event>(this->eventWaitList[i]);
        if (event->isUserEvent()) {
            continue;
        }

        auto timestampPacketContainer = event->getTimestampPacketNodes();
        if (!timestampPacketContainer || timestampPacketContainer->peekNodes().empty()) {
            continue;
        }

        auto sameRootDevice = event->getCommandQueue()->getClDevice().getRootDeviceIndex() == currentCsr.getRootDeviceIndex();
        if (!sameRootDevice) {
            continue;
        }

        auto sameCsr = (&event->getCommandQueue()->getGpgpuCommandStreamReceiver() == &currentCsr);
        bool pushDependency = (CsrDependencies::DependenciesType::OnCsr == depsType && sameCsr) ||
                              (CsrDependencies::DependenciesType::OutOfCsr == depsType && !sameCsr) ||
                              (CsrDependencies::DependenciesType::All == depsType);

        if (pushDependency) {
            csrDeps.timestampPacketContainer.push_back(timestampPacketContainer);
        }
    }
}

void EventsRequest::fillCsrDependenciesForTaskCountContainer(CsrDependencies &csrDeps, CommandStreamReceiver &currentCsr) const {
    for (cl_uint i = 0; i < this->numEventsInWaitList; i++) {
        auto event = castToObjectOrAbort<Event>(this->eventWaitList[i]);
        if (event->isUserEvent() || CompletionStamp::notReady == event->peekTaskCount()) {
            continue;
        }

        if (event->getCommandQueue() && event->getCommandQueue()->getDevice().getRootDeviceIndex() != currentCsr.getRootDeviceIndex()) {
            auto taskCountPreviousRootDevice = event->peekTaskCount();
            auto tagAddressPreviousRootDevice = event->getCommandQueue()->getGpgpuCommandStreamReceiver().getTagAddress();

            csrDeps.taskCountContainer.push_back({taskCountPreviousRootDevice, reinterpret_cast<uint64_t>(tagAddressPreviousRootDevice)});

            auto graphicsAllocation = event->getCommandQueue()->getGpgpuCommandStreamReceiver().getTagsMultiAllocation()->getGraphicsAllocation(currentCsr.getRootDeviceIndex());
            currentCsr.getResidencyAllocations().push_back(graphicsAllocation);
        }
    }
}

void EventsRequest::setupBcsCsrForOutputEvent(CommandStreamReceiver &bcsCsr) const {
    if (outEvent) {
        auto event = castToObjectOrAbort<Event>(*outEvent);
        event->setupBcs(bcsCsr.getOsContext().getEngineType());
    }
}

TransferProperties::TransferProperties(MemObj *memObj, cl_command_type cmdType, cl_map_flags mapFlags, bool blocking,
                                       size_t *offsetPtr, size_t *sizePtr, void *ptr, bool doTransferOnCpu, uint32_t rootDeviceIndex)
    : memObj(memObj), ptr(ptr), cmdType(cmdType), mapFlags(mapFlags), blocking(blocking), doTransferOnCpu(doTransferOnCpu) {
    // no size or offset passed for unmap operation
    if (cmdType != CL_COMMAND_UNMAP_MEM_OBJECT) {
        if (memObj->peekClMemObjType() == CL_MEM_OBJECT_BUFFER) {
            size[0] = *sizePtr;
            offset[0] = *offsetPtr;
            if (doTransferOnCpu &&
                (false == MemoryPool::isSystemMemoryPool(memObj->getGraphicsAllocation(rootDeviceIndex)->getMemoryPool())) &&
                (memObj->getMemoryManager() != nullptr)) {
                this->lockedPtr = memObj->getMemoryManager()->lockResource(memObj->getGraphicsAllocation(rootDeviceIndex));
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
