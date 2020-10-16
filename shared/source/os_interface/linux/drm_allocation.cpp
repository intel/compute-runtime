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
#include "shared/source/os_interface/os_context.h"

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

void DrmAllocation::makeBOsResident(OsContext *osContext, uint32_t vmHandleId, std::vector<BufferObject *> *bufferObjects, bool bind) {
    if (this->fragmentsStorage.fragmentCount) {
        for (unsigned int f = 0; f < this->fragmentsStorage.fragmentCount; f++) {
            if (!this->fragmentsStorage.fragmentStorageData[f].residency->resident[osContext->getContextId()]) {
                bindBO(this->fragmentsStorage.fragmentStorageData[f].osHandleStorage->bo, osContext, vmHandleId, bufferObjects, bind);
                this->fragmentsStorage.fragmentStorageData[f].residency->resident[osContext->getContextId()] = true;
            }
        }
    } else {
        bindBOs(osContext, vmHandleId, bufferObjects, bind);
    }
}

void DrmAllocation::bindBO(BufferObject *bo, OsContext *osContext, uint32_t vmHandleId, std::vector<BufferObject *> *bufferObjects, bool bind) {
    if (bo) {
        if (bufferObjects) {
            if (bo->peekIsReusableAllocation()) {
                for (auto bufferObject : *bufferObjects) {
                    if (bufferObject == bo) {
                        return;
                    }
                }
            }

            bufferObjects->push_back(bo);

        } else {
            auto retVal = 0;
            if (bind) {
                retVal = bo->bind(osContext, vmHandleId);
            } else {
                retVal = bo->unbind(osContext, vmHandleId);
            }
            UNRECOVERABLE_IF(retVal);
        }
    }
}

void DrmAllocation::registerBOBindExtHandle(Drm *drm) {
    if (!drm->resourceRegistrationEnabled()) {
        return;
    }

    Drm::ResourceClass resourceClass = Drm::ResourceClass::MaxSize;

    switch (this->allocationType) {
    case GraphicsAllocation::AllocationType::DEBUG_CONTEXT_SAVE_AREA:
        resourceClass = Drm::ResourceClass::ContextSaveArea;
        break;
    case GraphicsAllocation::AllocationType::DEBUG_SBA_TRACKING_BUFFER:
        resourceClass = Drm::ResourceClass::SbaTrackingBuffer;
        break;
    case GraphicsAllocation::AllocationType::KERNEL_ISA:
        resourceClass = Drm::ResourceClass::Isa;
        break;
    case GraphicsAllocation::AllocationType::DEBUG_MODULE_AREA:
        resourceClass = Drm::ResourceClass::ModuleHeapDebugArea;
        break;
    default:
        break;
    }

    if (resourceClass != Drm::ResourceClass::MaxSize) {
        uint64_t gpuAddress = getGpuAddress();
        auto handle = drm->registerResource(resourceClass, &gpuAddress, sizeof(gpuAddress));
        registeredBoBindHandles.push_back(handle);
        auto &bos = getBOs();

        for (auto bo : bos) {
            if (bo) {
                bo->addBindExtHandle(handle);
                bo->markForCapture();
            }
        }
    }
}

void DrmAllocation::freeRegisteredBOBindExtHandles(Drm *drm) {
    for (auto &i : registeredBoBindHandles) {
        drm->unregisterResource(i);
    }
}
} // namespace NEO
