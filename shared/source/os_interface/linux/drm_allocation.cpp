/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_allocation.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/residency.h"
#include "shared/source/os_interface/linux/cache_info.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/i915_prelim.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/product_helper.h"

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

std::string DrmAllocation::getPatIndexInfoString(const ProductHelper &productHelper) const {
    std::stringstream ss;

    auto bo = getBO();
    if (bo) {
        ss << " PATIndex: " << bo->peekPatIndex() << ",";
    }
    auto gmm = getDefaultGmm();
    if (gmm) {
        ss << " Gmm resource usage: "
           << "[ " << gmm->getUsageTypeString() << " ],";
        ss << " Cacheable: " << gmm->resourceParams.Flags.Info.Cacheable;
        ss << " NotLockable: " << gmm->resourceParams.Flags.Info.NotLockable;
    }
    return ss.str();
}

void DrmAllocation::clearInternalHandle(uint32_t handleId) {
    handles[handleId] = std::numeric_limits<uint64_t>::max();
}

int DrmAllocation::createInternalHandle(MemoryManager *memoryManager, uint32_t handleId, uint64_t &handle) {
    return peekInternalHandle(memoryManager, handleId, handle);
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

bool DrmAllocation::setPreferredLocation(Drm *drm, PreferredLocation memoryLocation) {
    auto ioctlHelper = drm->getIoctlHelper();
    auto remainingMemoryBanks = storageInfo.memoryBanks;
    bool success = true;
    auto pHwInfo = drm->getRootDeviceEnvironment().getHardwareInfo();

    if (this->storageInfo.isChunked && debugManager.flags.EnableBOChunkingPreferredLocationHint.get() == 1) {
        prelim_drm_i915_gem_memory_class_instance region{};
        region.memory_class = NEO::PrelimI915::PRELIM_I915_MEMORY_CLASS_DEVICE;
        auto banks = std::bitset<4>(remainingMemoryBanks);
        MemRegionsVec memRegions{};
        size_t currentBank = 0;
        size_t i = 0;
        while (i < banks.count()) {
            if (banks.test(currentBank)) {
                auto regionClassAndInstance = drm->getMemoryInfo()->getMemoryRegionClassAndInstance(1u << currentBank, *pHwInfo);
                memRegions.push_back(regionClassAndInstance);
                i++;
            }
            currentBank++;
        }

        for (uint32_t i = 0; i < this->storageInfo.numOfChunks; i++) {
            // Depth-first
            region.memory_instance = memRegions[i / (this->storageInfo.numOfChunks / memRegions.size())].memoryInstance;
            uint64_t chunkLength = (bufferObjects[0]->peekSize() / this->storageInfo.numOfChunks);
            uint64_t chunkStart = i * chunkLength;
            printDebugString(debugManager.flags.PrintBOChunkingLogs.get(), stdout,
                             "Setting PRELIM_DRM_I915_GEM_VM_ADVISE for BO-%d chunk 0x%lx chunkLength %ld memory_class %d, memory_region %d\n",
                             bufferObjects[0]->peekHandle(),
                             chunkStart,
                             chunkLength,
                             region.memory_class,
                             region.memory_instance);
            success &= ioctlHelper->setVmBoAdviseForChunking(bufferObjects[0]->peekHandle(),
                                                             chunkStart,
                                                             chunkLength,
                                                             ioctlHelper->getPreferredLocationAdvise(),
                                                             &region);
        }
        return success;
    }

    for (uint8_t handleId = 0u; handleId < numHandles; handleId++) {
        auto memoryInstance = Math::getMinLsbSet(static_cast<uint32_t>(remainingMemoryBanks.to_ulong()));

        std::optional<MemoryClassInstance> region = ioctlHelper->getPreferredLocationRegion(memoryLocation, memoryInstance);
        if (region != std::nullopt) {
            auto bo = this->getBOs()[handleId];
            success &= ioctlHelper->setVmBoAdvise(bo->peekHandle(), ioctlHelper->getPreferredLocationAdvise(), &region);
        }

        remainingMemoryBanks.reset(memoryInstance);
    }

    return success;
}

bool DrmAllocation::setCacheRegion(Drm *drm, CacheRegion regionIndex) {
    if (regionIndex == CacheRegion::defaultRegion) {
        return true;
    }

    auto cacheInfo = drm->getCacheInfo();
    if (cacheInfo == nullptr) {
        return false;
    }

    const auto cacheLevel{cacheInfo->getLevelForRegion(regionIndex)};
    const auto maxCacheRegions{cacheInfo->getMaxReservationNumCacheRegions(cacheLevel)};
    const auto maxReservationCacheSize{cacheInfo->getMaxReservationCacheSize(cacheLevel)};
    const auto regionSize{(maxCacheRegions > 0) ? maxReservationCacheSize / maxCacheRegions : 0};
    if (regionSize == 0) {
        return false;
    }

    return setCacheAdvice(drm, regionSize, regionIndex, !isAllocatedInLocalMemoryPool());
}

bool DrmAllocation::setCacheAdvice(Drm *drm, size_t regionSize, CacheRegion regionIndex, bool isSystemMemoryPool) {
    if (!drm->getCacheInfo()->getCacheRegion(regionSize, regionIndex)) {
        return false;
    }

    auto patIndex = drm->getPatIndex(getDefaultGmm(), allocationType, regionIndex, CachePolicy::writeBack, true, isSystemMemoryPool);

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

bool DrmAllocation::prefetchBOWithChunking(Drm *drm) {
    auto getSubDeviceIds = [](const DeviceBitfield &subDeviceBitfield) {
        SubDeviceIdsVec subDeviceIds;
        for (auto subDeviceId = 0u; subDeviceId < subDeviceBitfield.size(); subDeviceId++) {
            if (subDeviceBitfield.test(subDeviceId)) {
                subDeviceIds.push_back(subDeviceId);
            }
        }
        return subDeviceIds;
    };

    auto bo = this->getBO();

    auto ioctlHelper = drm->getIoctlHelper();
    auto memoryClassDevice = ioctlHelper->getDrmParamValue(DrmParam::memoryClassDevice);
    auto subDeviceIds = getSubDeviceIds(storageInfo.subDeviceBitfield);

    uint32_t chunksPerSubDevice = this->storageInfo.numOfChunks / subDeviceIds.size();
    uint64_t chunkLength = (bo->peekSize() / this->storageInfo.numOfChunks);
    bool success = true;
    for (uint32_t i = 0; i < this->storageInfo.numOfChunks; i++) {
        uint64_t chunkStart = bo->peekAddress() + i * chunkLength;
        auto subDeviceId = subDeviceIds[i / chunksPerSubDevice];
        for (auto vmHandleId : subDeviceIds) {
            auto region = static_cast<uint32_t>((memoryClassDevice << 16u) | subDeviceId);
            auto vmId = drm->getVirtualMemoryAddressSpace(vmHandleId);

            PRINT_DEBUG_STRING(debugManager.flags.PrintBOPrefetchingResult.get(), stdout,
                               "prefetching BO=%d to VM %u, drmVmId=%u, range: %llx - %llx, size: %lld, region: %x\n",
                               bo->peekHandle(), vmId, vmHandleId, chunkStart, ptrOffset(chunkStart, chunkLength), chunkLength, region);

            success &= ioctlHelper->setVmPrefetch(chunkStart, chunkLength, region, vmId);

            PRINT_DEBUG_STRING(debugManager.flags.PrintBOPrefetchingResult.get(), stdout,
                               "prefetched BO=%d to VM %u, drmVmId=%u, range: %llx - %llx, size: %lld, region: %x, result: %d\n",
                               bo->peekHandle(), vmId, vmHandleId, chunkStart, ptrOffset(chunkStart, chunkLength), chunkLength, region, success);
        }
    }

    return success;
}

int DrmAllocation::makeBOsResident(OsContext *osContext, uint32_t vmHandleId, std::vector<BufferObject *> *bufferObjects, bool bind, const bool forcePagingFence) {
    if (this->fragmentsStorage.fragmentCount) {
        for (unsigned int f = 0; f < this->fragmentsStorage.fragmentCount; f++) {
            if (!this->fragmentsStorage.fragmentStorageData[f].residency->resident[osContext->getContextId()]) {
                int retVal = bindBO(static_cast<OsHandleLinux *>(this->fragmentsStorage.fragmentStorageData[f].osHandleStorage)->bo, osContext, vmHandleId, bufferObjects, bind, forcePagingFence);
                if (retVal) {
                    return retVal;
                }
                this->fragmentsStorage.fragmentStorageData[f].residency->resident[osContext->getContextId()] = true;
            }
        }
    } else {
        int retVal = bindBOs(osContext, vmHandleId, bufferObjects, bind, forcePagingFence);
        if (retVal) {
            return retVal;
        }
    }

    return 0;
}

int DrmAllocation::bindBO(BufferObject *bo, OsContext *osContext, uint32_t vmHandleId, std::vector<BufferObject *> *bufferObjects, bool bind, const bool forcePagingFence) {
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
                retVal = bo->bind(osContext, vmHandleId, forcePagingFence);
            } else {
                retVal = bo->unbind(osContext, vmHandleId);
            }
        }
    }
    return retVal;
}

int DrmAllocation::bindBOs(OsContext *osContext, uint32_t vmHandleId, std::vector<BufferObject *> *bufferObjects, bool bind, const bool forcePagingFence) {
    int retVal = 0;
    if (this->storageInfo.getNumBanks() > 1) {
        auto &bos = this->getBOs();
        if (this->storageInfo.tileInstanced) {
            auto bo = bos[vmHandleId];
            retVal = bindBO(bo, osContext, vmHandleId, bufferObjects, bind, forcePagingFence);
            if (retVal) {
                return retVal;
            }
        } else {
            for (auto bo : bos) {
                retVal = bindBO(bo, osContext, vmHandleId, bufferObjects, bind, forcePagingFence);
                if (retVal) {
                    return retVal;
                }
            }
        }
    } else {
        auto bo = this->getBO();
        retVal = bindBO(bo, osContext, vmHandleId, bufferObjects, bind, forcePagingFence);
        if (retVal) {
            return retVal;
        }
    }
    return 0;
}

bool DrmAllocation::prefetchBO(BufferObject *bo, uint32_t vmHandleId, uint32_t subDeviceId) {
    auto drm = bo->peekDrm();
    auto ioctlHelper = drm->getIoctlHelper();
    auto memoryClassDevice = ioctlHelper->getDrmParamValue(DrmParam::memoryClassDevice);
    auto region = static_cast<uint32_t>((memoryClassDevice << 16u) | subDeviceId);
    auto vmId = drm->getVirtualMemoryAddressSpace(vmHandleId);

    auto result = ioctlHelper->setVmPrefetch(bo->peekAddress(), bo->peekSize(), region, vmId);

    PRINT_DEBUG_STRING(debugManager.flags.PrintBOPrefetchingResult.get(), stdout,
                       "prefetch BO=%d to VM %u, drmVmId=%u, range: %llx - %llx, size: %lld, region: %x, result: %d\n",
                       bo->peekHandle(), vmId, vmHandleId, bo->peekAddress(), ptrOffset(bo->peekAddress(), bo->peekSize()), bo->peekSize(), region, result);
    return result;
}

void DrmAllocation::registerBOBindExtHandle(Drm *drm) {
    if (!drm->getIoctlHelper()->resourceRegistrationEnabled()) {
        return;
    }

    drm->getIoctlHelper()->registerBOBindHandle(drm, this);
}

void DrmAllocation::setAsReadOnly() {
    auto &bos = getBOs();
    for (auto &bo : bos) {
        if (bo) {
            bo->setAsReadOnly(true);
        }
    }
}

void DrmAllocation::linkWithRegisteredHandle(uint32_t handle) {
    auto &bos = getBOs();
    for (auto bo : bos) {
        if (bo) {
            bo->addBindExtHandle(handle);
            bo->requireImmediateBinding(true);
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

    if (debugManager.flags.EnableImplicitMigrationOnFaultableHardware.get() != -1) {
        return debugManager.flags.EnableImplicitMigrationOnFaultableHardware.get();
    }

    switch (this->allocationType) {
    case AllocationType::unifiedSharedMemory:
        return drm->hasKmdMigrationSupport();
    case AllocationType::buffer:
        return debugManager.flags.UseKmdMigrationForBuffers.get() > 0;
    default:
        return false;
    }
}

bool DrmAllocation::setMemAdvise(Drm *drm, MemAdviseFlags flags) {
    bool success = true;

    if (flags.cachedMemory != enabledMemAdviseFlags.cachedMemory) {
        CachePolicy memType = flags.cachedMemory ? CachePolicy::writeBack : CachePolicy::uncached;
        setCachePolicy(memType);
    }

    auto ioctlHelper = drm->getIoctlHelper();
    if (flags.nonAtomic != enabledMemAdviseFlags.nonAtomic) {
        for (auto bo : bufferObjects) {
            if (bo != nullptr) {
                success &= ioctlHelper->setVmBoAdvise(bo->peekHandle(), ioctlHelper->getAtomicAdvise(flags.nonAtomic), nullptr);
            }
        }
    }

    if (flags.devicePreferredLocation != enabledMemAdviseFlags.devicePreferredLocation) {
        success &= setPreferredLocation(drm, flags.devicePreferredLocation ? PreferredLocation::device : PreferredLocation::clear);
    }

    if (flags.systemPreferredLocation != enabledMemAdviseFlags.systemPreferredLocation) {
        success &= setPreferredLocation(drm, flags.systemPreferredLocation ? PreferredLocation::system : PreferredLocation::defaultLocation);
    }

    if (success) {
        enabledMemAdviseFlags = flags;
    }

    return success;
}

bool DrmAllocation::setAtomicAccess(Drm *drm, size_t size, AtomicAccessMode mode) {
    bool success = true;

    if (mode == AtomicAccessMode::host) {
        // Host mode not currently supported by KMD
        return success;
    }

    auto ioctlHelper = drm->getIoctlHelper();

    for (auto bo : bufferObjects) {
        if (bo != nullptr) {
            success &= ioctlHelper->setVmBoAdvise(bo->peekHandle(), ioctlHelper->getAtomicAccess(mode), nullptr);
        }
    }
    return success;
}

bool DrmAllocation::setMemPrefetch(Drm *drm, SubDeviceIdsVec &subDeviceIds) {
    UNRECOVERABLE_IF(subDeviceIds.size() == 0);

    bool success = true;

    if (numHandles > 1) {
        for (uint8_t handleId = 0u; handleId < numHandles; handleId++) {
            auto bo = this->getBOs()[handleId];
            auto subDeviceId = handleId;
            if (debugManager.flags.KMDSupportForCrossTileMigrationPolicy.get() > 0) {
                subDeviceId = subDeviceIds[handleId % subDeviceIds.size()];
            }
            for (auto vmHandleId : subDeviceIds) {
                success &= prefetchBO(bo, vmHandleId, subDeviceId);
            }
        }
    } else {
        auto bo = this->getBO();
        if (bo->isChunked()) {
            auto drm = bo->peekDrm();
            success = prefetchBOWithChunking(const_cast<Drm *>(drm));
        } else {
            success = prefetchBO(bo, subDeviceIds[0], subDeviceIds[0]);
        }
    }

    return success;
}

void DrmAllocation::registerMemoryToUnmap(void *pointer, size_t size, DrmAllocation::MemoryUnmapFunction unmapFunction) {
    this->memoryToUnmap.push_back({pointer, size, unmapFunction});
}

uint64_t DrmAllocation::getHandleAddressBase(uint32_t handleIndex) {
    return bufferObjects[handleIndex]->peekAddress();
}

size_t DrmAllocation::getHandleSize(uint32_t handleIndex) {
    return bufferObjects[handleIndex]->peekSize();
}

} // namespace NEO
