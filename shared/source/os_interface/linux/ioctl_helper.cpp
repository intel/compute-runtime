/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ioctl_helper.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/drm_wrappers.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/os_time.h"

#include <fcntl.h>
#include <sstream>

namespace NEO {

int IoctlHelper::ioctl(DrmIoctl request, void *arg) {
    return drm.ioctl(request, arg);
}

void IoctlHelper::fillExecObject(ExecObject &execObject, uint32_t handle, uint64_t gpuAddress, uint32_t drmContextId, bool bindInfo, bool isMarkedForCapture) {

    auto &drmExecObject = *reinterpret_cast<drm_i915_gem_exec_object2 *>(execObject.data);
    drmExecObject.handle = handle;
    drmExecObject.relocation_count = 0; // No relocations, we are SoftPinning
    drmExecObject.relocs_ptr = 0ul;
    drmExecObject.alignment = 0;
    drmExecObject.offset = gpuAddress;
    drmExecObject.flags = EXEC_OBJECT_PINNED | EXEC_OBJECT_SUPPORTS_48B_ADDRESS;

    if (debugManager.flags.UseAsyncDrmExec.get() == 1) {
        drmExecObject.flags |= static_cast<decltype(drmExecObject.flags)>(EXEC_OBJECT_ASYNC);
    }

    if (isMarkedForCapture) {
        drmExecObject.flags |= static_cast<decltype(drmExecObject.flags)>(EXEC_OBJECT_CAPTURE);
    }
    drmExecObject.rsvd1 = drmContextId;
    drmExecObject.rsvd2 = 0;

    if (bindInfo) {
        drmExecObject.handle = 0u;
    }
}

void IoctlHelper::setupIpVersion() {
    auto &rootDeviceEnvironment = drm.getRootDeviceEnvironment();
    auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();
    auto &compilerProductHelper = rootDeviceEnvironment.getHelper<CompilerProductHelper>();
    hwInfo.ipVersion.value = compilerProductHelper.getHwIpVersion(hwInfo);
}

void IoctlHelper::logExecObject(const ExecObject &execObject, std::stringstream &logger, size_t size) {
    auto &drmExecObject = *reinterpret_cast<const drm_i915_gem_exec_object2 *>(execObject.data);
    logger << "Buffer Object = { handle: BO-" << drmExecObject.handle
           << ", address range: 0x" << reinterpret_cast<void *>(drmExecObject.offset)
           << " - 0x" << reinterpret_cast<void *>(ptrOffset(drmExecObject.offset, size))
           << ", flags: " << std::hex << drmExecObject.flags << std::dec
           << ", size: " << size << " }\n";
}

void IoctlHelper::fillExecBuffer(ExecBuffer &execBuffer, uintptr_t buffersPtr, uint32_t bufferCount, uint32_t startOffset, uint32_t size, uint64_t flags, uint32_t drmContextId) {
    auto &drmExecBuffer = *reinterpret_cast<drm_i915_gem_execbuffer2 *>(execBuffer.data);
    drmExecBuffer.buffers_ptr = buffersPtr;
    drmExecBuffer.buffer_count = bufferCount;
    drmExecBuffer.batch_start_offset = startOffset;
    drmExecBuffer.batch_len = size;
    drmExecBuffer.flags = flags;
    drmExecBuffer.rsvd1 = drmContextId;
}

void IoctlHelper::logExecBuffer(const ExecBuffer &execBuffer, std::stringstream &logger) {
    auto &drmExecBuffer = *reinterpret_cast<const drm_i915_gem_execbuffer2 *>(execBuffer.data);
    logger << "drm_i915_gem_execbuffer2 { "
           << "buffer_ptr: " + std::to_string(drmExecBuffer.buffers_ptr)
           << ", buffer_count: " + std::to_string(drmExecBuffer.buffer_count)
           << ", batch_start_offset: " + std::to_string(drmExecBuffer.batch_start_offset)
           << ", batch_len: " + std::to_string(drmExecBuffer.batch_len)
           << ", flags: " + std::to_string(drmExecBuffer.flags)
           << ", rsvd1: " + std::to_string(drmExecBuffer.rsvd1)
           << " }\n";
}

int IoctlHelper::createDrmContext(Drm &drm, OsContextLinux &osContext, uint32_t drmVmId, uint32_t deviceIndex) {

    const auto numberOfCCS = drm.getRootDeviceEnvironment().getHardwareInfo()->gtSystemInfo.CCSInfo.NumberOfCCSEnabled;
    const bool debuggableContext = drm.isContextDebugSupported() && drm.getRootDeviceEnvironment().executionEnvironment.isDebuggingEnabled() && !osContext.isInternalEngine();
    const bool debuggableContextCooperative = drm.getRootDeviceEnvironment().executionEnvironment.getDebuggingMode() == DebuggingMode::offline ? false : (debuggableContext && numberOfCCS > 0);
    auto drmContextId = drm.createDrmContext(drmVmId, drm.isVmBindAvailable(), osContext.isCooperativeEngine() || debuggableContextCooperative);
    if (drmContextId < 0) {
        return drmContextId;
    }

    if (drm.areNonPersistentContextsSupported()) {
        drm.setNonPersistentContext(drmContextId);
    }

    drm.setUnrecoverableContext(drmContextId);

    if (debuggableContext) {
        drm.setContextDebugFlag(drmContextId);
    }

    if (drm.isPreemptionSupported() && osContext.isLowPriority()) {
        drm.setLowPriorityContextParam(drmContextId);
    }
    auto engineFlag = drm.bindDrmContext(drmContextId, deviceIndex, osContext.getEngineType(), osContext.isEngineInstanced());
    osContext.setEngineFlag(engineFlag);
    return drmContextId;
}

std::vector<EngineCapabilities> IoctlHelper::translateToEngineCaps(const std::vector<uint64_t> &data) {
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

std::vector<MemoryRegion> IoctlHelper::translateToMemoryRegions(const std::vector<uint64_t> &regionInfo) {
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

bool IoctlHelper::setDomainCpu(uint32_t handle, bool writeEnable) {
    drm_i915_gem_set_domain setDomain{};
    setDomain.handle = handle;
    setDomain.read_domains = I915_GEM_DOMAIN_CPU;
    setDomain.write_domain = writeEnable ? I915_GEM_DOMAIN_CPU : 0;
    return this->ioctl(DrmIoctl::gemSetDomain, &setDomain) == 0;
}

uint32_t IoctlHelper::getFlagsForPrimeHandleToFd() const {
    return DRM_CLOEXEC | DRM_RDWR;
}

unsigned int IoctlHelper::getIoctlRequestValueBase(DrmIoctl ioctlRequest) const {
    switch (ioctlRequest) {
    case DrmIoctl::getparam:
        return DRM_IOCTL_I915_GETPARAM;
    case DrmIoctl::gemExecbuffer2:
        return DRM_IOCTL_I915_GEM_EXECBUFFER2;
    case DrmIoctl::gemWait:
        return DRM_IOCTL_I915_GEM_WAIT;
    case DrmIoctl::gemClose:
        return DRM_IOCTL_GEM_CLOSE;
    case DrmIoctl::gemUserptr:
        return DRM_IOCTL_I915_GEM_USERPTR;
    case DrmIoctl::gemCreate:
        return DRM_IOCTL_I915_GEM_CREATE;
    case DrmIoctl::gemSetDomain:
        return DRM_IOCTL_I915_GEM_SET_DOMAIN;
    case DrmIoctl::gemSetTiling:
        return DRM_IOCTL_I915_GEM_SET_TILING;
    case DrmIoctl::gemGetTiling:
        return DRM_IOCTL_I915_GEM_GET_TILING;
    case DrmIoctl::gemContextCreateExt:
        return DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT;
    case DrmIoctl::gemContextDestroy:
        return DRM_IOCTL_I915_GEM_CONTEXT_DESTROY;
    case DrmIoctl::regRead:
        return DRM_IOCTL_I915_REG_READ;
    case DrmIoctl::getResetStats:
        return DRM_IOCTL_I915_GET_RESET_STATS;
    case DrmIoctl::gemContextGetparam:
        return DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM;
    case DrmIoctl::gemContextSetparam:
        return DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM;
    case DrmIoctl::query:
        return DRM_IOCTL_I915_QUERY;
    case DrmIoctl::primeFdToHandle:
        return DRM_IOCTL_PRIME_FD_TO_HANDLE;
    case DrmIoctl::primeHandleToFd:
        return DRM_IOCTL_PRIME_HANDLE_TO_FD;
    case DrmIoctl::gemMmapOffset:
        return DRM_IOCTL_I915_GEM_MMAP_OFFSET;
    case DrmIoctl::gemVmCreate:
        return DRM_IOCTL_I915_GEM_VM_CREATE;
    case DrmIoctl::gemVmDestroy:
        return DRM_IOCTL_I915_GEM_VM_DESTROY;
    default:
        UNRECOVERABLE_IF(true);
        return 0u;
    }
}

int IoctlHelper::getDrmParamValueBase(DrmParam drmParam) const {
    switch (drmParam) {
    case DrmParam::ContextCreateExtSetparam:
        return I915_CONTEXT_CREATE_EXT_SETPARAM;
    case DrmParam::ContextCreateFlagsUseExtensions:
        return I915_CONTEXT_CREATE_FLAGS_USE_EXTENSIONS;
    case DrmParam::ContextEnginesExtLoadBalance:
        return I915_CONTEXT_ENGINES_EXT_LOAD_BALANCE;
    case DrmParam::ContextParamEngines:
        return I915_CONTEXT_PARAM_ENGINES;
    case DrmParam::ContextParamGttSize:
        return I915_CONTEXT_PARAM_GTT_SIZE;
    case DrmParam::ContextParamPersistence:
        return I915_CONTEXT_PARAM_PERSISTENCE;
    case DrmParam::ContextParamPriority:
        return I915_CONTEXT_PARAM_PRIORITY;
    case DrmParam::ContextParamRecoverable:
        return I915_CONTEXT_PARAM_RECOVERABLE;
    case DrmParam::ContextParamSseu:
        return I915_CONTEXT_PARAM_SSEU;
    case DrmParam::ContextParamVm:
        return I915_CONTEXT_PARAM_VM;
    case DrmParam::EngineClassRender:
        return drm_i915_gem_engine_class::I915_ENGINE_CLASS_RENDER;
    case DrmParam::EngineClassCopy:
        return drm_i915_gem_engine_class::I915_ENGINE_CLASS_COPY;
    case DrmParam::EngineClassVideo:
        return drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO;
    case DrmParam::EngineClassVideoEnhance:
        return drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO_ENHANCE;
    case DrmParam::EngineClassInvalid:
        return drm_i915_gem_engine_class::I915_ENGINE_CLASS_INVALID;
    case DrmParam::EngineClassInvalidNone:
        return I915_ENGINE_CLASS_INVALID_NONE;
    case DrmParam::ExecBlt:
        return I915_EXEC_BLT;
    case DrmParam::ExecDefault:
        return I915_EXEC_DEFAULT;
    case DrmParam::ExecNoReloc:
        return I915_EXEC_NO_RELOC;
    case DrmParam::ExecRender:
        return I915_EXEC_RENDER;
    case DrmParam::MemoryClassDevice:
        return drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE;
    case DrmParam::MemoryClassSystem:
        return drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM;
    case DrmParam::MmapOffsetWb:
        return I915_MMAP_OFFSET_WB;
    case DrmParam::MmapOffsetWc:
        return I915_MMAP_OFFSET_WC;
    case DrmParam::ParamChipsetId:
        return I915_PARAM_CHIPSET_ID;
    case DrmParam::ParamRevision:
        return I915_PARAM_REVISION;
    case DrmParam::ParamHasExecSoftpin:
        return I915_PARAM_HAS_EXEC_SOFTPIN;
    case DrmParam::ParamHasPooledEu:
        return I915_PARAM_HAS_POOLED_EU;
    case DrmParam::ParamHasScheduler:
        return I915_PARAM_HAS_SCHEDULER;
    case DrmParam::ParamEuTotal:
        return I915_PARAM_EU_TOTAL;
    case DrmParam::ParamSubsliceTotal:
        return I915_PARAM_SUBSLICE_TOTAL;
    case DrmParam::ParamMinEuInPool:
        return I915_PARAM_MIN_EU_IN_POOL;
    case DrmParam::ParamCsTimestampFrequency:
        return I915_PARAM_CS_TIMESTAMP_FREQUENCY;
    case DrmParam::QueryEngineInfo:
        return DRM_I915_QUERY_ENGINE_INFO;
    case DrmParam::QueryMemoryRegions:
        return DRM_I915_QUERY_MEMORY_REGIONS;
    case DrmParam::QueryTopologyInfo:
        return DRM_I915_QUERY_TOPOLOGY_INFO;
    case DrmParam::SchedulerCapPreemption:
        return I915_SCHEDULER_CAP_PREEMPTION;
    case DrmParam::TilingNone:
        return I915_TILING_NONE;
    case DrmParam::TilingY:
        return I915_TILING_Y;
    case DrmParam::ParamOATimestampFrequency:
        return I915_PARAM_OA_TIMESTAMP_FREQUENCY;
    default:
        UNRECOVERABLE_IF(true);
        return 0;
    }
}

std::string IoctlHelper::getDrmParamStringBase(DrmParam drmParam) const {
    switch (drmParam) {
    case DrmParam::ParamChipsetId:
        return "I915_PARAM_CHIPSET_ID";
    case DrmParam::ParamRevision:
        return "I915_PARAM_REVISION";
    case DrmParam::ParamHasExecSoftpin:
        return "I915_PARAM_HAS_EXEC_SOFTPIN";
    case DrmParam::ParamHasPooledEu:
        return "I915_PARAM_HAS_POOLED_EU";
    case DrmParam::ParamHasScheduler:
        return "I915_PARAM_HAS_SCHEDULER";
    case DrmParam::ParamEuTotal:
        return "I915_PARAM_EU_TOTAL";
    case DrmParam::ParamSubsliceTotal:
        return "I915_PARAM_SUBSLICE_TOTAL";
    case DrmParam::ParamMinEuInPool:
        return "I915_PARAM_MIN_EU_IN_POOL";
    case DrmParam::ParamCsTimestampFrequency:
        return "I915_PARAM_CS_TIMESTAMP_FREQUENCY";
    case DrmParam::ParamOATimestampFrequency:
        return "I915_PARAM_OA_TIMESTAMP_FREQUENCY";
    default:
        UNRECOVERABLE_IF(true);
        return "";
    }
}

std::string IoctlHelper::getIoctlStringBase(DrmIoctl ioctlRequest) const {
    switch (ioctlRequest) {
    case DrmIoctl::gemExecbuffer2:
        return "DRM_IOCTL_I915_GEM_EXECBUFFER2";
    case DrmIoctl::gemWait:
        return "DRM_IOCTL_I915_GEM_WAIT";
    case DrmIoctl::gemClose:
        return "DRM_IOCTL_GEM_CLOSE";
    case DrmIoctl::gemUserptr:
        return "DRM_IOCTL_I915_GEM_USERPTR";
    case DrmIoctl::getparam:
        return "DRM_IOCTL_I915_GETPARAM";
    case DrmIoctl::gemCreate:
        return "DRM_IOCTL_I915_GEM_CREATE";
    case DrmIoctl::gemSetDomain:
        return "DRM_IOCTL_I915_GEM_SET_DOMAIN";
    case DrmIoctl::gemSetTiling:
        return "DRM_IOCTL_I915_GEM_SET_TILING";
    case DrmIoctl::gemGetTiling:
        return "DRM_IOCTL_I915_GEM_GET_TILING";
    case DrmIoctl::gemContextCreateExt:
        return "DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT";
    case DrmIoctl::gemContextDestroy:
        return "DRM_IOCTL_I915_GEM_CONTEXT_DESTROY";
    case DrmIoctl::regRead:
        return "DRM_IOCTL_I915_REG_READ";
    case DrmIoctl::getResetStats:
        return "DRM_IOCTL_I915_GET_RESET_STATS";
    case DrmIoctl::gemContextGetparam:
        return "DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM";
    case DrmIoctl::gemContextSetparam:
        return "DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM";
    case DrmIoctl::query:
        return "DRM_IOCTL_I915_QUERY";
    case DrmIoctl::primeFdToHandle:
        return "DRM_IOCTL_PRIME_FD_TO_HANDLE";
    case DrmIoctl::primeHandleToFd:
        return "DRM_IOCTL_PRIME_HANDLE_TO_FD";
    case DrmIoctl::gemMmapOffset:
        return "DRM_IOCTL_I915_GEM_MMAP_OFFSET";
    case DrmIoctl::gemVmCreate:
        return "DRM_IOCTL_I915_GEM_VM_CREATE";
    case DrmIoctl::gemVmDestroy:
        return "DRM_IOCTL_I915_GEM_VM_DESTROY";
    default:
        UNRECOVERABLE_IF(true);
        return "";
    }
}

std::string IoctlHelper::getFileForMaxGpuFrequency() const {
    return "/gt_max_freq_mhz";
}

std::string IoctlHelper::getFileForMaxGpuFrequencyOfSubDevice(int subDeviceId) const {
    return "/gt/gt" + std::to_string(subDeviceId) + "/rps_max_freq_mhz";
}

std::string IoctlHelper::getFileForMaxMemoryFrequencyOfSubDevice(int subDeviceId) const {
    return "/gt/gt" + std::to_string(subDeviceId) + "/mem_RP0_freq_mhz";
}

bool IoctlHelper::checkIfIoctlReinvokeRequired(int error, DrmIoctl ioctlRequest) const {
    return (error == EINTR || error == EAGAIN || error == EBUSY || error == -EBUSY);
}

std::unique_ptr<MemoryInfo> IoctlHelper::createMemoryInfo() {
    auto request = getDrmParamValue(DrmParam::QueryMemoryRegions);
    auto dataQuery = drm.query<uint64_t>(request, 0);
    if (!dataQuery.empty()) {
        auto memRegions = translateToMemoryRegions(dataQuery);
        return std::make_unique<MemoryInfo>(memRegions, drm);
    }
    return {};
}

bool IoctlHelper::getTopologyDataAndMap(const HardwareInfo &hwInfo, DrmQueryTopologyData &topologyData, TopologyMap &topologyMap) {

    auto request = this->getDrmParamValue(DrmParam::QueryTopologyInfo);
    auto dataQuery = drm.query<uint64_t>(request, 0);
    if (dataQuery.empty()) {
        return false;
    }
    auto topologyInfo = reinterpret_cast<QueryTopologyInfo *>(dataQuery.data());

    TopologyMapping mapping;
    auto retVal = this->translateTopologyInfo(topologyInfo, topologyData, mapping);
    topologyData.maxEuPerSubSlice = topologyInfo->maxEusPerSubslice;

    topologyMap.clear();
    topologyMap[0] = mapping;

    return retVal;
}

bool IoctlHelper::translateTopologyInfo(const QueryTopologyInfo *queryTopologyInfo, DrmQueryTopologyData &topologyData, TopologyMapping &mapping) {
    int sliceCount = 0;
    int subSliceCount = 0;
    int euCount = 0;
    int maxSliceCount = 0;
    int maxSubSliceCountPerSlice = 0;
    std::vector<int> sliceIndices;
    sliceIndices.reserve(maxSliceCount);

    for (int x = 0; x < queryTopologyInfo->maxSlices; x++) {
        bool isSliceEnable = (queryTopologyInfo->data[x / 8] >> (x % 8)) & 1;
        if (!isSliceEnable) {
            continue;
        }
        sliceIndices.push_back(x);
        sliceCount++;

        std::vector<int> subSliceIndices;
        subSliceIndices.reserve(queryTopologyInfo->maxSubslices);

        for (int y = 0; y < queryTopologyInfo->maxSubslices; y++) {
            size_t yOffset = (queryTopologyInfo->subsliceOffset + static_cast<size_t>(x * queryTopologyInfo->subsliceStride) + y / 8);
            bool isSubSliceEnabled = (queryTopologyInfo->data[yOffset] >> (y % 8)) & 1;
            if (!isSubSliceEnabled) {
                continue;
            }
            subSliceCount++;
            subSliceIndices.push_back(y);

            for (int z = 0; z < queryTopologyInfo->maxEusPerSubslice; z++) {
                size_t zOffset = (queryTopologyInfo->euOffset + static_cast<size_t>((x * queryTopologyInfo->maxSubslices + y) * queryTopologyInfo->euStride) + z / 8);
                bool isEUEnabled = (queryTopologyInfo->data[zOffset] >> (z % 8)) & 1;
                if (!isEUEnabled) {
                    continue;
                }
                euCount++;
            }
        }

        if (subSliceIndices.size()) {
            maxSubSliceCountPerSlice = std::max(maxSubSliceCountPerSlice, subSliceIndices[subSliceIndices.size() - 1] + 1);
        }

        // single slice available
        if (sliceCount == 1) {
            mapping.subsliceIndices = std::move(subSliceIndices);
        }
    }

    if (sliceIndices.size()) {
        maxSliceCount = sliceIndices[sliceIndices.size() - 1] + 1;
        mapping.sliceIndices = std::move(sliceIndices);
    }

    if (sliceCount != 1) {
        mapping.subsliceIndices.clear();
    }

    topologyData.sliceCount = sliceCount;
    topologyData.subSliceCount = subSliceCount;
    topologyData.euCount = euCount;
    topologyData.maxSliceCount = maxSliceCount;
    topologyData.maxSubSliceCount = maxSubSliceCountPerSlice;

    return (sliceCount && subSliceCount && euCount);
}

std::unique_ptr<EngineInfo> IoctlHelper::createEngineInfo(bool isSysmanEnabled) {
    auto request = getDrmParamValue(DrmParam::QueryEngineInfo);
    auto enginesQuery = drm.query<uint64_t>(request, 0);
    if (enginesQuery.empty()) {
        return {};
    }
    auto engines = translateToEngineCaps(enginesQuery);
    auto hwInfo = drm.getRootDeviceEnvironment().getMutableHardwareInfo();

    auto memInfo = drm.getMemoryInfo();

    if (!memInfo) {
        return std::make_unique<EngineInfo>(&drm, engines);
    }

    auto &memoryRegions = memInfo->getDrmRegionInfos();

    auto tileCount = 0u;
    std::vector<DistanceInfo> distanceInfos;
    for (const auto &region : memoryRegions) {
        if (getDrmParamValue(DrmParam::MemoryClassDevice) == region.region.memoryClass) {
            tileCount++;
            DistanceInfo distanceInfo{};
            distanceInfo.region = region.region;

            for (const auto &engine : engines) {
                if (engine.engine.engineClass == getDrmParamValue(DrmParam::EngineClassCompute) ||
                    engine.engine.engineClass == getDrmParamValue(DrmParam::EngineClassRender) ||
                    engine.engine.engineClass == getDrmParamValue(DrmParam::EngineClassCopy)) {
                    distanceInfo.engine = engine.engine;
                    distanceInfos.push_back(distanceInfo);
                } else if (isSysmanEnabled) {

                    if (engine.engine.engineClass == getDrmParamValue(DrmParam::EngineClassVideo) ||
                        engine.engine.engineClass == getDrmParamValue(DrmParam::EngineClassVideoEnhance)) {
                        distanceInfo.engine = engine.engine;
                        distanceInfos.push_back(distanceInfo);
                    }
                }
            }
        }
    }

    if (tileCount == 0u) {
        return std::make_unique<EngineInfo>(&drm, engines);
    }

    std::vector<QueryItem> queryItems{distanceInfos.size()};
    auto ret = queryDistances(queryItems, distanceInfos);
    if (ret != 0) {
        return {};
    }

    const bool queryUnsupported = std::all_of(queryItems.begin(), queryItems.end(),
                                              [](const QueryItem &item) { return item.length == -EINVAL; });
    if (queryUnsupported) {
        DEBUG_BREAK_IF(tileCount != 1);
        return std::make_unique<EngineInfo>(&drm, engines);
    }

    memInfo->assignRegionsFromDistances(distanceInfos);

    auto &multiTileArchInfo = hwInfo->gtSystemInfo.MultiTileArchInfo;
    multiTileArchInfo.IsValid = true;
    multiTileArchInfo.TileCount = tileCount;
    multiTileArchInfo.TileMask = static_cast<uint8_t>(maxNBitValue(tileCount));

    return std::make_unique<EngineInfo>(&drm, tileCount, distanceInfos, queryItems, engines);
}

void IoctlHelper::fillBindInfoForIpcHandle(uint32_t handle, size_t size) {}

bool IoctlHelper::getFdFromVmExport(uint32_t vmId, uint32_t flags, int32_t *fd) {
    return false;
}

uint32_t IoctlHelper::createGem(uint64_t size, uint32_t memoryBanks) {
    GemCreate gemCreate = {};
    gemCreate.size = size;
    [[maybe_unused]] auto ret = ioctl(DrmIoctl::gemCreate, &gemCreate);
    DEBUG_BREAK_IF(ret != 0);
    return gemCreate.handle;
}

bool IoctlHelper::setGemTiling(void *setTiling) {
    return this->ioctl(DrmIoctl::gemSetTiling, setTiling) == 0;
}

bool IoctlHelper::getGemTiling(void *setTiling) {
    return this->ioctl(DrmIoctl::gemGetTiling, setTiling) == 0;
}

bool getGpuTime32(::NEO::Drm &drm, uint64_t *timestamp) {
    RegisterRead reg = {};
    reg.offset = RegisterOffsets::globalTimestampLdw;

    if (drm.ioctl(DrmIoctl::regRead, &reg)) {
        return false;
    }
    *timestamp = reg.value >> 32;
    return true;
}

bool getGpuTime36(::NEO::Drm &drm, uint64_t *timestamp) {
    RegisterRead reg = {};
    reg.offset = RegisterOffsets::globalTimestampLdw | 1;

    if (drm.ioctl(DrmIoctl::regRead, &reg)) {
        return false;
    }
    *timestamp = reg.value;
    return true;
}

bool getGpuTimeSplitted(::NEO::Drm &drm, uint64_t *timestamp) {
    RegisterRead regHi = {};
    RegisterRead regLo = {};
    uint64_t tmpHi;
    int err = 0, loop = 3;

    regHi.offset = RegisterOffsets::globalTimestampUn;
    regLo.offset = RegisterOffsets::globalTimestampLdw;

    err += drm.ioctl(DrmIoctl::regRead, &regHi);
    do {
        tmpHi = regHi.value;
        err += drm.ioctl(DrmIoctl::regRead, &regLo);
        err += drm.ioctl(DrmIoctl::regRead, &regHi);
    } while (err == 0 && regHi.value != tmpHi && --loop);

    if (err) {
        return false;
    }

    *timestamp = regLo.value | (regHi.value << 32);
    return true;
}

void IoctlHelper::initializeGetGpuTimeFunction() {
    RegisterRead reg = {};
    int err;

    reg.offset = (RegisterOffsets::globalTimestampLdw | 1);
    err = this->ioctl(DrmIoctl::regRead, &reg);
    if (err) {
        reg.offset = RegisterOffsets::globalTimestampUn;
        err = this->ioctl(DrmIoctl::regRead, &reg);
        if (err) {
            this->getGpuTime = &getGpuTime32;
        } else {
            this->getGpuTime = &getGpuTimeSplitted;
        }
    } else {
        this->getGpuTime = &getGpuTime36;
    }
}

bool IoctlHelper::setGpuCpuTimes(TimeStampData *pGpuCpuTime, OSTime *osTime) {
    if (pGpuCpuTime == nullptr || osTime == nullptr) {
        return false;
    }

    if (this->getGpuTime == nullptr) {
        return false;
    }

    if (!this->getGpuTime(drm, &pGpuCpuTime->gpuTimeStamp)) {
        return false;
    }
    if (!osTime->getCpuTime(&pGpuCpuTime->cpuTimeinNS)) {
        return false;
    }

    return true;
}

} // namespace NEO
