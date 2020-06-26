/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_allocation.h"

#include "shared/source/memory_manager/residency.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"

#include <sstream>

namespace NEO {
std::string DrmAllocation::getAllocationInfoString() const {
    std::stringstream ss;
    for (auto bo : bufferObjects) {
        if (bo != nullptr) {
            ss << " Handle: " << bo->peekHandle();
        }
    }
    return ss.str();
}

uint64_t DrmAllocation::peekInternalHandle(MemoryManager *memoryManager) {
    return static_cast<uint64_t>((static_cast<DrmMemoryManager *>(memoryManager))->obtainFdFromHandle(getBO()->peekHandle(), this->rootDeviceIndex));
}

void DrmAllocation::getBOsForResidency(uint32_t osContextId, uint32_t handleId, std::vector<BufferObject *> &bufferObjects) {
    if (this->fragmentsStorage.fragmentCount) {
        for (unsigned int f = 0; f < this->fragmentsStorage.fragmentCount; f++) {
            if (!this->fragmentsStorage.fragmentStorageData[f].residency->resident[osContextId]) {
                appendBO(this->fragmentsStorage.fragmentStorageData[f].osHandleStorage->bo, bufferObjects);
                this->fragmentsStorage.fragmentStorageData[f].residency->resident[osContextId] = true;
            }
        }
    } else {
        appendBOs(handleId, bufferObjects);
    }
}

void DrmAllocation::appendBO(BufferObject *bo, std::vector<BufferObject *> &bufferObjects) {
    if (bo) {
        if (bo->peekIsReusableAllocation()) {
            for (auto bufferObject : bufferObjects) {
                if (bufferObject == bo) {
                    return;
                }
            }
        }

        bufferObjects.push_back(bo);
    }
}

} // namespace NEO
