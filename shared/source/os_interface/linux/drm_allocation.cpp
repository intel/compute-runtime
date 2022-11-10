/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_allocation.h"

#include "shared/source/memory_manager/residency.h"
#include "shared/source/os_interface/linux/cache_info.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/os_context.h"

#include <sstream>

namespace NEO {

DrmAllocation::~DrmAllocation() {
    [[maybe_unused]] int retCode;
    for (auto &memory : this->memoryToUnmap) {
        retCode = memory.unmapFunction(memory.pointer, memory.size);
        DEBUG_BREAK_IF(retCode != 0);
    }
}

std::string DrmAllocation::getAllocationInfoString() const {
    std::stringstream ss;
    for (auto bo : bufferObjects) {
        if (bo != nullptr) {
            ss << " Handle: " << bo->peekHandle();
        }
    }
    return ss.str();
}

int DrmAllocation::peekInternalHandle(MemoryManager *memoryManager, uint64_t &handle) {
    return peekInternalHandle(memoryManager, 0u, handle);
}

int DrmAllocation::peekInternalHandle(MemoryManager *memoryManager, uint32_t handleId, uint64_t &handle) {
    if (handles[handleId] != std::numeric_limits<uint64_t>::max()) {
        handle = handles[handleId];
        return 0;
    }

    int64_t ret = static_cast<int64_t>((static_cast<DrmMemoryManager *>(memoryManager))->obtainFdFromHandle(getBufferObjectToModify(handleId)->peekHandle(), this->rootDeviceIndex));
    if (ret < 0) {
        return -1;
    }

    handle = handles[handleId] = ret;

    return 0;
}

void DrmAllocation::setCachePolicy(CachePolicy memType) {
    for (auto bo : bufferObjects) {
        if (bo != nullptr) {
            bo->setCachePolicy(memType);
        }
    }
}

bool DrmAllocation::setCacheRegion(Drm *drm, CacheRegion regionIndex) {
    if (regionIndex == CacheRegion::Default) {
        return true;
    }

    auto cacheInfo = drm->getCacheInfo();
    if (cacheInfo == nullptr) {
        return false;
    }

    auto regionSize = (cacheInfo->getMaxReservationNumCacheRegions() > 0) ? cacheInfo->getMaxReservationCacheSize() / cacheInfo->getMaxReservationNumCacheRegions() : 0;
    if (regionSize == 0) {
        return false;
    }

    return setCacheAdvice(drm, regionSize, regionIndex);
}

bool DrmAllocation::setCacheAdvice(Drm *drm, size_t regionSize, CacheRegion regionIndex) {
    if (!drm->getCacheInfo()->getCacheRegion(regionSize, regionIndex)) {
        return false;
    }

    auto patIndex = drm->getPatIndex(getDefaultGmm(), allocationType, regionIndex, CachePolicy::WriteBack, true);

    if (fragmentsStorage.fragmentCount > 0) {
        for (uint32_t i = 0; i < fragmentsStorage.fragmentCount; i++) {
            auto bo = static_cast<OsHandleLinux *>(fragmentsStorage.fragmentStorageData[i].osHandleStorage)->bo;
            bo->setCacheRegion(regionIndex);
            bo->setPatIndex(patIndex);
        }
        return true;
    }

    for (auto bo : bufferObjects) {
        if (bo != nullptr) {
            bo->setCacheRegion(regionIndex);
            bo->setPatIndex(patIndex);
        }
    }
    return true;
}

int DrmAllocation::makeBOsResident(OsContext *osContext, uint32_t vmHandleId, std::vector<BufferObject *> *bufferObjects, bool bind) {
    if (this->fragmentsStorage.fragmentCount) {
        for (unsigned int f = 0; f < this->fragmentsStorage.fragmentCount; f++) {
            if (!this->fragmentsStorage.fragmentStorageData[f].residency->resident[osContext->getContextId()]) {
                int retVal = bindBO(static_cast<OsHandleLinux *>(this->fragmentsStorage.fragmentStorageData[f].osHandleStorage)->bo, osContext, vmHandleId, bufferObjects, bind);
                if (retVal) {
                    return retVal;
                }
                this->fragmentsStorage.fragmentStorageData[f].residency->resident[osContext->getContextId()] = true;
            }
        }
    } else {
        int retVal = bindBOs(osContext, vmHandleId, bufferObjects, bind);
        if (retVal) {
            return retVal;
        }
    }

    return 0;
}

int DrmAllocation::bindBO(BufferObject *bo, OsContext *osContext, uint32_t vmHandleId, std::vector<BufferObject *> *bufferObjects, bool bind) {
    auto retVal = 0;
    if (bo) {
        bo->requireExplicitResidency(bo->peekDrm()->hasPageFaultSupport() && !shouldAllocationPageFault(bo->peekDrm()));
        if (bufferObjects) {
            if (bo->peekIsReusableAllocation()) {
                for (auto bufferObject : *bufferObjects) {
                    if (bufferObject == bo) {
                        return 0;
                    }
                }
            }

            bufferObjects->push_back(bo);

        } else {
            if (bind) {
                retVal = bo->bind(osContext, vmHandleId);
            } else {
                retVal = bo->unbind(osContext, vmHandleId);
            }
        }
    }
    return retVal;
}

int DrmAllocation::bindBOs(OsContext *osContext, uint32_t vmHandleId, std::vector<BufferObject *> *bufferObjects, bool bind) {
    int retVal = 0;
    if (this->storageInfo.getNumBanks() > 1) {
        auto &bos = this->getBOs();
        if (this->storageInfo.tileInstanced) {
            auto bo = bos[vmHandleId];
            retVal = bindBO(bo, osContext, vmHandleId, bufferObjects, bind);
            if (retVal) {
                return retVal;
            }
        } else {
            for (auto bo : bos) {
                retVal = bindBO(bo, osContext, vmHandleId, bufferObjects, bind);
                if (retVal) {
                    return retVal;
                }
            }
        }
    } else {
        auto bo = this->getBO();
        retVal = bindBO(bo, osContext, vmHandleId, bufferObjects, bind);
        if (retVal) {
            return retVal;
        }
    }
    return 0;
}

void DrmAllocation::registerBOBindExtHandle(Drm *drm) {
    if (!drm->resourceRegistrationEnabled()) {
        return;
    }

    DrmResourceClass resourceClass = DrmResourceClass::MaxSize;

    switch (this->allocationType) {
    case AllocationType::DEBUG_CONTEXT_SAVE_AREA:
        resourceClass = DrmResourceClass::ContextSaveArea;
        break;
    case AllocationType::DEBUG_SBA_TRACKING_BUFFER:
        resourceClass = DrmResourceClass::SbaTrackingBuffer;
        break;
    case AllocationType::KERNEL_ISA:
        resourceClass = DrmResourceClass::Isa;
        break;
    case AllocationType::DEBUG_MODULE_AREA:
        resourceClass = DrmResourceClass::ModuleHeapDebugArea;
        break;
    default:
        break;
    }

    if (resourceClass != DrmResourceClass::MaxSize) {
        auto handle = 0;
        if (resourceClass == DrmResourceClass::Isa) {
            auto deviceBitfiled = static_cast<uint32_t>(this->storageInfo.subDeviceBitfield.to_ulong());
            handle = drm->registerResource(resourceClass, &deviceBitfiled, sizeof(deviceBitfiled));
        } else {
            uint64_t gpuAddress = getGpuAddress();
            handle = drm->registerResource(resourceClass, &gpuAddress, sizeof(gpuAddress));
        }
        registeredBoBindHandles.push_back(handle);

        auto &bos = getBOs();

        for (auto bo : bos) {
            if (bo) {
                bo->addBindExtHandle(handle);
                bo->markForCapture();
                if (resourceClass == DrmResourceClass::Isa && storageInfo.tileInstanced == true) {
                    auto cookieHandle = drm->registerIsaCookie(handle);
                    bo->addBindExtHandle(cookieHandle);
                    registeredBoBindHandles.push_back(cookieHandle);
                }

                bo->requireImmediateBinding(true);
            }
        }
    }
}

void DrmAllocation::linkWithRegisteredHandle(uint32_t handle) {
    auto &bos = getBOs();
    for (auto bo : bos) {
        if (bo) {
            bo->addBindExtHandle(handle);
        }
    }
}

void DrmAllocation::freeRegisteredBOBindExtHandles(Drm *drm) {
    for (auto it = registeredBoBindHandles.rbegin(); it != registeredBoBindHandles.rend(); ++it) {
        drm->unregisterResource(*it);
    }
}

void DrmAllocation::markForCapture() {
    auto &bos = getBOs();
    for (auto bo : bos) {
        if (bo) {
            bo->markForCapture();
        }
    }
}

bool DrmAllocation::shouldAllocationPageFault(const Drm *drm) {
    if (!drm->hasPageFaultSupport()) {
        return false;
    }

    if (DebugManager.flags.EnableImplicitMigrationOnFaultableHardware.get() != -1) {
        return DebugManager.flags.EnableImplicitMigrationOnFaultableHardware.get();
    }

    switch (this->allocationType) {
    case AllocationType::UNIFIED_SHARED_MEMORY:
        return DebugManager.flags.UseKmdMigration.get() > 0;
    case AllocationType::BUFFER:
        return DebugManager.flags.UseKmdMigrationForBuffers.get() > 0;
    default:
        return false;
    }
}

bool DrmAllocation::setMemAdvise(Drm *drm, MemAdviseFlags flags) {
    bool success = true;

    if (flags.cached_memory != enabledMemAdviseFlags.cached_memory) {
        CachePolicy memType = flags.cached_memory ? CachePolicy::WriteBack : CachePolicy::Uncached;
        setCachePolicy(memType);
    }

    auto ioctlHelper = drm->getIoctlHelper();
    if (flags.non_atomic != enabledMemAdviseFlags.non_atomic) {
        for (auto bo : bufferObjects) {
            if (bo != nullptr) {
                success &= ioctlHelper->setVmBoAdvise(bo->peekHandle(), ioctlHelper->getAtomicAdvise(flags.non_atomic), nullptr);
            }
        }
    }

    if (flags.device_preferred_location != enabledMemAdviseFlags.device_preferred_location) {
        MemoryClassInstance region{};
        for (auto handleId = 0u; handleId < EngineLimits::maxHandleCount; handleId++) {
            auto bo = bufferObjects[handleId];
            if (bo != nullptr) {
                if (flags.device_preferred_location) {
                    region.memoryClass = ioctlHelper->getDrmParamValue(DrmParam::MemoryClassDevice);
                    region.memoryInstance = handleId;
                } else {
                    region.memoryClass = -1;
                    region.memoryInstance = 0;
                }
                success &= ioctlHelper->setVmBoAdvise(bo->peekHandle(), ioctlHelper->getPreferredLocationAdvise(), &region);
            }
        }
    }

    if (success) {
        enabledMemAdviseFlags = flags;
    }

    return success;
}

bool DrmAllocation::setMemPrefetch(Drm *drm, uint32_t subDeviceId) {
    bool success = true;
    auto ioctlHelper = drm->getIoctlHelper();
    auto memoryClassDevice = ioctlHelper->getDrmParamValue(DrmParam::MemoryClassDevice);

    for (auto bo : bufferObjects) {
        if (bo != nullptr) {
            auto region = static_cast<uint32_t>((memoryClassDevice << 16u) | subDeviceId);
            success &= ioctlHelper->setVmPrefetch(bo->peekAddress(), bo->peekSize(), region);
        }
    }

    return success;
}

void DrmAllocation::registerMemoryToUnmap(void *pointer, size_t size, DrmAllocation::MemoryUnmapFunction unmapFunction) {
    this->memoryToUnmap.push_back({pointer, size, unmapFunction});
}

} // namespace NEO
