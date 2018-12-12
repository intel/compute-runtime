/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/mipmap.h"
#include "runtime/helpers/properties_helper.h"
#include "runtime/mem_obj/image.h"
#include "runtime/mem_obj/mem_obj.h"
#include "runtime/memory_manager/memory_manager.h"

namespace OCLRT {
TransferProperties::TransferProperties(MemObj *memObj, cl_command_type cmdType, cl_map_flags mapFlags, bool blocking,
                                       size_t *offsetPtr, size_t *sizePtr, void *ptr)
    : memObj(memObj), cmdType(cmdType), mapFlags(mapFlags), blocking(blocking), ptr(ptr) {

    // no size or offset passed for unmap operation
    if (cmdType != CL_COMMAND_UNMAP_MEM_OBJECT) {
        if (memObj->peekClMemObjType() == CL_MEM_OBJECT_BUFFER) {
            size[0] = *sizePtr;
            offset[0] = *offsetPtr;
            if (DebugManager.flags.ForceResourceLockOnTransferCalls.get()) {
                if ((false == MemoryPool::isSystemMemoryPool(memObj->getGraphicsAllocation()->getMemoryPool())) && (memObj->getMemoryManager() != nullptr)) {
                    this->lockedPtr = memObj->getMemoryManager()->lockResource(memObj->getGraphicsAllocation());
                }
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
    return ptrOffset(lockedPtr ? lockedPtr : memObj->getCpuAddressForMemoryTransfer(), offset[0]);
}

} // namespace OCLRT
