/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/linux/cache_info.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"

#include "third_party/uapi/drm/i915_drm.h"

namespace NEO {

IoctlHelper *IoctlHelperUpstream::clone() {
    return new IoctlHelperUpstream{};
}

bool IoctlHelperUpstream::isVmBindAvailable(Drm *drm) {
    return false;
}

uint32_t IoctlHelperUpstream::createGemExt(Drm *drm, const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle) {
    uint32_t regionsSize = static_cast<uint32_t>(memClassInstances.size());
    std::vector<drm_i915_gem_memory_class_instance> regions(regionsSize);
    for (uint32_t i = 0; i < regionsSize; i++) {
        regions[i].memory_class = memClassInstances[i].memoryClass;
        regions[i].memory_instance = memClassInstances[i].memoryInstance;
    }
    drm_i915_gem_create_ext_memory_regions memRegions{};
    memRegions.num_regions = regionsSize;
    memRegions.regions = reinterpret_cast<uintptr_t>(regions.data());
    memRegions.base.name = I915_GEM_CREATE_EXT_MEMORY_REGIONS;

    drm_i915_gem_create_ext createExt{};
    createExt.size = allocSize;
    createExt.extensions = reinterpret_cast<uintptr_t>(&memRegions);

    printDebugString(DebugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "Performing GEM_CREATE_EXT with { size: %lu",
                     allocSize);

    if (DebugManager.flags.PrintBOCreateDestroyResult.get()) {
        for (uint32_t i = 0; i < regionsSize; i++) {
            auto region = regions[i];
            printDebugString(DebugManager.flags.PrintBOCreateDestroyResult.get(), stdout, ", memory class: %d, memory instance: %d",
                             region.memory_class, region.memory_instance);
        }
        printDebugString(DebugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "%s", " }\n");
    }

    auto ret = ioctl(drm, DRM_IOCTL_I915_GEM_CREATE_EXT, &createExt);

    printDebugString(DebugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "GEM_CREATE_EXT with EXT_MEMORY_REGIONS has returned: %d BO-%u with size: %lu\n", ret, createExt.handle, createExt.size);
    handle = createExt.handle;
    return ret;
}

std::vector<MemoryRegion> IoctlHelperUpstream::translateToMemoryRegions(const std::vector<uint8_t> &regionInfo) {
    auto *data = reinterpret_cast<const drm_i915_query_memory_regions *>(regionInfo.data());
    auto memRegions = std::vector<MemoryRegion>(data->num_regions);
    for (uint32_t i = 0; i < data->num_regions; i++) {
        memRegions[i].probedSize = data->regions[i].probed_size;
        memRegions[i].unallocatedSize = data->regions[i].unallocated_size;
        memRegions[i].region.memoryClass = data->regions[i].region.memory_class;
        memRegions[i].region.memoryInstance = data->regions[i].region.memory_instance;
    }
    return memRegions;
}

CacheRegion IoctlHelperUpstream::closAlloc(Drm *drm) {
    return CacheRegion::None;
}

uint16_t IoctlHelperUpstream::closAllocWays(Drm *drm, CacheRegion closIndex, uint16_t cacheLevel, uint16_t numWays) {
    return 0;
}

CacheRegion IoctlHelperUpstream::closFree(Drm *drm, CacheRegion closIndex) {
    return CacheRegion::None;
}

int IoctlHelperUpstream::waitUserFence(Drm *drm, uint32_t ctxId, uint64_t address,
                                       uint64_t value, uint32_t dataWidth, int64_t timeout, uint16_t flags) {
    return 0;
}

uint32_t IoctlHelperUpstream::getHwConfigIoctlVal() {
    return DRM_I915_QUERY_HWCONFIG_TABLE;
}

uint32_t IoctlHelperUpstream::getAtomicAdvise(bool isNonAtomic) {
    return 0;
}

uint32_t IoctlHelperUpstream::getPreferredLocationAdvise() {
    return 0;
}

bool IoctlHelperUpstream::setVmBoAdvise(Drm *drm, int32_t handle, uint32_t attribute, void *region) {
    return true;
}

bool IoctlHelperUpstream::setVmPrefetch(Drm *drm, uint64_t start, uint64_t length, uint32_t region) {
    return true;
}

uint32_t IoctlHelperUpstream::getDirectSubmissionFlag() {
    return 0u;
}

int32_t IoctlHelperUpstream::getMemRegionsIoctlVal() {
    return DRM_I915_QUERY_MEMORY_REGIONS;
}

int32_t IoctlHelperUpstream::getEngineInfoIoctlVal() {
    return DRM_I915_QUERY_ENGINE_INFO;
}

uint32_t IoctlHelperUpstream::getComputeSlicesIoctlVal() {
    return 0;
}

std::unique_ptr<uint8_t[]> IoctlHelperUpstream::prepareVmBindExt(const StackVec<uint32_t, 2> &bindExtHandles) {
    return {};
}

uint64_t IoctlHelperUpstream::getFlagsForVmBind(bool bindCapture, bool bindImmediate, bool bindMakeResident) {
    return 0u;
}

std::vector<EngineCapabilities> IoctlHelperUpstream::translateToEngineCaps(const std::vector<uint8_t> &data) {
    auto engineInfo = reinterpret_cast<const drm_i915_query_engine_info *>(data.data());
    std::vector<EngineCapabilities> engines;
    engines.reserve(engineInfo->num_engines);
    for (uint32_t i = 0; i < engineInfo->num_engines; i++) {
        EngineCapabilities engine{};
        engine.capabilities = engineInfo->engines[i].capabilities;
        engine.engine.engineClass = engineInfo->engines[i].engine.engine_class;
        engine.engine.engineInstance = engineInfo->engines[i].engine.engine_instance;
        engines.push_back(engine);
    }
    return engines;
}

uint32_t IoctlHelperUpstream::queryDistances(Drm *drm, std::vector<drm_i915_query_item> &queryItems, std::vector<DistanceInfo> &distanceInfos) {
    for (auto &query : queryItems) {
        query.length = -EINVAL;
    }
    return 0;
}

int32_t IoctlHelperUpstream::getComputeEngineClass() {
    return 4;
}

uint16_t IoctlHelperUpstream::getWaitUserFenceSoftFlag() {
    return 0;
}

int IoctlHelperUpstream::execBuffer(Drm *drm, drm_i915_gem_execbuffer2 *execBuffer, uint64_t completionGpuAddress, uint32_t counterValue) {
    return ioctl(drm, DRM_IOCTL_I915_GEM_EXECBUFFER2, execBuffer);
}

bool IoctlHelperUpstream::completionFenceExtensionSupported(const HardwareInfo &hwInfo, const bool isVmBindAvailable) {
    return false;
}

std::optional<int> IoctlHelperUpstream::getHasPageFaultParamId() {
    return std::nullopt;
};

bool IoctlHelperUpstream::getEuStallProperties(std::array<uint64_t, 10u> &properties, uint64_t dssBufferSize, uint64_t samplingRate, uint64_t pollPeriod, uint64_t engineInstance) {
    return false;
}

uint32_t IoctlHelperUpstream::getEuStallFdParameter() {
    return 0u;
}

std::unique_ptr<uint8_t[]> IoctlHelperUpstream::createVmControlExtRegion(const std::optional<MemoryClassInstance> &regionInstanceClass) {
    return {};
}

uint32_t IoctlHelperUpstream::getFlagsForVmCreate(bool disableScratch, bool enablePageFault) {
    return 0u;
}

uint32_t IoctlHelperUpstream::createContextWithAccessCounters(Drm *drm, drm_i915_gem_context_create_ext &gcc) {
    return EINVAL;
}

uint32_t IoctlHelperUpstream::createCooperativeContext(Drm *drm, drm_i915_gem_context_create_ext &gcc) {
    return EINVAL;
}

std::unique_ptr<uint8_t[]> IoctlHelperUpstream::createVmBindExtSetPat() {
    return {};
}

void IoctlHelperUpstream::fillVmBindExtSetPat(const std::unique_ptr<uint8_t[]> &vmBindExtSetPat, uint64_t patIndex, uint64_t nextExtension) {}

std::unique_ptr<uint8_t[]> IoctlHelperUpstream::createVmBindExtUserFence() {
    return {};
}

void IoctlHelperUpstream::fillVmBindExtUserFence(const std::unique_ptr<uint8_t[]> &vmBindExtUserFence, uint64_t fenceAddress, uint64_t fenceValue, uint64_t nextExtension) {}

std::optional<uint64_t> IoctlHelperUpstream::getCopyClassSaturatePCIECapability() {
    return std::nullopt;
}

std::optional<uint64_t> IoctlHelperUpstream::getCopyClassSaturateLinkCapability() {
    return std::nullopt;
}

uint32_t IoctlHelperUpstream::getVmAdviseAtomicAttribute() {
    return 0;
}

int IoctlHelperUpstream::vmBind(Drm *drm, const VmBindParams &vmBindParams) {
    return 0;
}

int IoctlHelperUpstream::vmUnbind(Drm *drm, const VmBindParams &vmBindParams) {
    return 0;
}

UuidRegisterResult IoctlHelperUpstream::registerUuid(Drm *drm, const std::string &uuid, uint32_t uuidClass, uint64_t ptr, uint64_t size) {
    return {0, 0};
}

UuidRegisterResult IoctlHelperUpstream::registerStringClassUuid(Drm *drm, const std::string &uuid, uint64_t ptr, uint64_t size) {
    return {0, 0};
}

int IoctlHelperUpstream::unregisterUuid(Drm *drm, uint32_t handle) {
    return 0;
}

bool IoctlHelperUpstream::isContextDebugSupported(Drm *drm) {
    return false;
}

int IoctlHelperUpstream::setContextDebugFlag(Drm *drm, uint32_t drmContextId) {
    return 0;
}

bool IoctlHelperUpstream::isDebugAttachAvailable() {
    return false;
}

} // namespace NEO
