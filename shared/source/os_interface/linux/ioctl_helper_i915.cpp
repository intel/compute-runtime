/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/drm_wrappers.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/file_descriptor.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/os_time.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/utilities/directory.h"

#include <fcntl.h>
#include <span>
#include <sstream>

namespace NEO {

void IoctlHelperI915::fillExecObject(ExecObject &execObject, uint32_t handle, uint64_t gpuAddress, uint32_t drmContextId, bool bindInfo, bool isMarkedForCapture) {

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

void IoctlHelperI915::logExecObject(const ExecObject &execObject, std::stringstream &logger, size_t size) {
    auto &drmExecObject = *reinterpret_cast<const drm_i915_gem_exec_object2 *>(execObject.data);
    logger << "Buffer Object = { handle: BO-" << drmExecObject.handle
           << ", address range: 0x" << reinterpret_cast<void *>(drmExecObject.offset)
           << " - 0x" << reinterpret_cast<void *>(ptrOffset(drmExecObject.offset, size))
           << ", flags: " << std::hex << drmExecObject.flags << std::dec
           << ", size: " << size << " }\n";
}

void IoctlHelperI915::fillExecBuffer(ExecBuffer &execBuffer, uintptr_t buffersPtr, uint32_t bufferCount, uint32_t startOffset, uint32_t size, uint64_t flags, uint32_t drmContextId) {
    auto &drmExecBuffer = *reinterpret_cast<drm_i915_gem_execbuffer2 *>(execBuffer.data);
    drmExecBuffer.buffers_ptr = buffersPtr;
    drmExecBuffer.buffer_count = bufferCount;
    drmExecBuffer.batch_start_offset = startOffset;
    drmExecBuffer.batch_len = size;
    drmExecBuffer.flags = flags;
    drmExecBuffer.rsvd1 = drmContextId;
}

void IoctlHelperI915::logExecBuffer(const ExecBuffer &execBuffer, std::stringstream &logger) {
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

int IoctlHelperI915::getDrmParamValueBase(DrmParam drmParam) const {
    switch (drmParam) {
    case DrmParam::contextCreateExtSetparam:
        return I915_CONTEXT_CREATE_EXT_SETPARAM;
    case DrmParam::contextCreateFlagsUseExtensions:
        return I915_CONTEXT_CREATE_FLAGS_USE_EXTENSIONS;
    case DrmParam::contextEnginesExtLoadBalance:
        return I915_CONTEXT_ENGINES_EXT_LOAD_BALANCE;
    case DrmParam::contextParamEngines:
        return I915_CONTEXT_PARAM_ENGINES;
    case DrmParam::contextParamGttSize:
        return I915_CONTEXT_PARAM_GTT_SIZE;
    case DrmParam::contextParamPersistence:
        return I915_CONTEXT_PARAM_PERSISTENCE;
    case DrmParam::contextParamPriority:
        return I915_CONTEXT_PARAM_PRIORITY;
    case DrmParam::contextParamRecoverable:
        return I915_CONTEXT_PARAM_RECOVERABLE;
    case DrmParam::contextParamSseu:
        return I915_CONTEXT_PARAM_SSEU;
    case DrmParam::contextParamVm:
        return I915_CONTEXT_PARAM_VM;
    case DrmParam::engineClassRender:
        return drm_i915_gem_engine_class::I915_ENGINE_CLASS_RENDER;
    case DrmParam::engineClassCopy:
        return drm_i915_gem_engine_class::I915_ENGINE_CLASS_COPY;
    case DrmParam::engineClassVideo:
        return drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO;
    case DrmParam::engineClassVideoEnhance:
        return drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO_ENHANCE;
    case DrmParam::engineClassInvalid:
        return drm_i915_gem_engine_class::I915_ENGINE_CLASS_INVALID;
    case DrmParam::engineClassInvalidNone:
        return I915_ENGINE_CLASS_INVALID_NONE;
    case DrmParam::execBlt:
        return I915_EXEC_BLT;
    case DrmParam::execDefault:
        return I915_EXEC_DEFAULT;
    case DrmParam::execNoReloc:
        return I915_EXEC_NO_RELOC;
    case DrmParam::execRender:
        return I915_EXEC_RENDER;
    case DrmParam::memoryClassDevice:
        return drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE;
    case DrmParam::memoryClassSystem:
        return drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM;
    case DrmParam::mmapOffsetWb:
        return I915_MMAP_OFFSET_WB;
    case DrmParam::mmapOffsetWc:
        return I915_MMAP_OFFSET_WC;
    case DrmParam::paramHasPooledEu:
        return I915_PARAM_HAS_POOLED_EU;
    case DrmParam::paramEuTotal:
        return I915_PARAM_EU_TOTAL;
    case DrmParam::paramSubsliceTotal:
        return I915_PARAM_SUBSLICE_TOTAL;
    case DrmParam::paramMinEuInPool:
        return I915_PARAM_MIN_EU_IN_POOL;
    case DrmParam::paramCsTimestampFrequency:
        return I915_PARAM_CS_TIMESTAMP_FREQUENCY;
    case DrmParam::queryEngineInfo:
        return DRM_I915_QUERY_ENGINE_INFO;
    case DrmParam::queryMemoryRegions:
        return DRM_I915_QUERY_MEMORY_REGIONS;
    case DrmParam::queryTopologyInfo:
        return DRM_I915_QUERY_TOPOLOGY_INFO;
    case DrmParam::tilingNone:
        return I915_TILING_NONE;
    case DrmParam::tilingY:
        return I915_TILING_Y;
    case DrmParam::paramOATimestampFrequency:
        return I915_PARAM_OA_TIMESTAMP_FREQUENCY;
    default:
        UNRECOVERABLE_IF(true);
        return 0;
    }
}

EngineCapabilities::Flags IoctlHelperI915::getEngineCapabilitiesFlags(uint64_t capabilities) const {
    return {};
}

std::vector<EngineCapabilities> IoctlHelperI915::translateToEngineCaps(const std::vector<uint64_t> &data) {
    auto engineInfo = reinterpret_cast<const drm_i915_query_engine_info *>(data.data());
    std::vector<EngineCapabilities> engines;
    engines.reserve(engineInfo->num_engines);
    for (uint32_t i = 0; i < engineInfo->num_engines; i++) {
        EngineCapabilities engine{};
        engine.capabilities = getEngineCapabilitiesFlags(engineInfo->engines[i].capabilities);
        engine.engine.engineClass = engineInfo->engines[i].engine.engine_class;
        engine.engine.engineInstance = engineInfo->engines[i].engine.engine_instance;
        engines.push_back(engine);
    }
    return engines;
}

std::vector<MemoryRegion> IoctlHelperI915::translateToMemoryRegions(const std::vector<uint64_t> &regionInfo) {

    auto *data = reinterpret_cast<const drm_i915_query_memory_regions *>(regionInfo.data());
    auto memRegions = std::vector<MemoryRegion>(data->num_regions);
    for (uint32_t i = 0; i < data->num_regions; i++) {
        memRegions[i].probedSize = data->regions[i].probed_size;
        memRegions[i].unallocatedSize = data->regions[i].unallocated_size;
        memRegions[i].cpuVisibleSize = data->regions[i].probed_cpu_visible_size;
        memRegions[i].region.memoryClass = data->regions[i].region.memory_class;
        memRegions[i].region.memoryInstance = data->regions[i].region.memory_instance;
    }
    return memRegions;
}

std::unique_ptr<EngineInfo> IoctlHelperI915::createEngineInfo(bool isSysmanEnabled) {
    auto request = getDrmParamValue(DrmParam::queryEngineInfo);
    auto enginesQuery = drm.query<uint64_t>(request, 0);
    if (enginesQuery.empty()) {
        return {};
    }
    auto engines = translateToEngineCaps(enginesQuery);
    StackVec<std::vector<EngineCapabilities>, 2> engineInfosPerTile{engines};
    auto hwInfo = drm.getRootDeviceEnvironment().getMutableHardwareInfo();

    auto memInfo = drm.getMemoryInfo();

    if (!memInfo) {
        return std::make_unique<EngineInfo>(&drm, engineInfosPerTile);
    }

    auto &memoryRegions = memInfo->getDrmRegionInfos();

    auto tileCount = 0u;
    std::vector<DistanceInfo> distanceInfos;
    for (const auto &region : memoryRegions) {
        if (getDrmParamValue(DrmParam::memoryClassDevice) == region.region.memoryClass) {
            tileCount++;
            DistanceInfo distanceInfo{};
            distanceInfo.region = region.region;

            for (const auto &engine : engines) {
                if (engine.engine.engineClass == getDrmParamValue(DrmParam::engineClassCompute) ||
                    engine.engine.engineClass == getDrmParamValue(DrmParam::engineClassRender) ||
                    engine.engine.engineClass == getDrmParamValue(DrmParam::engineClassCopy)) {
                    distanceInfo.engine = engine.engine;
                    distanceInfos.push_back(distanceInfo);
                } else if (isSysmanEnabled) {

                    if (engine.engine.engineClass == getDrmParamValue(DrmParam::engineClassVideo) ||
                        engine.engine.engineClass == getDrmParamValue(DrmParam::engineClassVideoEnhance)) {
                        distanceInfo.engine = engine.engine;
                        distanceInfos.push_back(distanceInfo);
                    }
                }
            }
        }
    }

    if (tileCount == 0u) {
        return std::make_unique<EngineInfo>(&drm, engineInfosPerTile);
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
        return std::make_unique<EngineInfo>(&drm, engineInfosPerTile);
    }

    memInfo->assignRegionsFromDistances(distanceInfos);

    auto &multiTileArchInfo = hwInfo->gtSystemInfo.MultiTileArchInfo;
    multiTileArchInfo.IsValid = true;
    multiTileArchInfo.TileCount = tileCount;
    multiTileArchInfo.TileMask = static_cast<uint8_t>(maxNBitValue(tileCount));

    return std::make_unique<EngineInfo>(&drm, tileCount, distanceInfos, queryItems, engines);
}

bool IoctlHelperI915::setDomainCpu(uint32_t handle, bool writeEnable) {
    drm_i915_gem_set_domain setDomain{};
    setDomain.handle = handle;
    setDomain.read_domains = I915_GEM_DOMAIN_CPU;
    setDomain.write_domain = writeEnable ? I915_GEM_DOMAIN_CPU : 0;
    return this->ioctl(DrmIoctl::gemSetDomain, &setDomain) == 0;
}

unsigned int IoctlHelperI915::getIoctlRequestValue(DrmIoctl ioctlRequest) const {
    switch (ioctlRequest) {
    case DrmIoctl::getparam:
        return DRM_IOCTL_I915_GETPARAM;
    case DrmIoctl::gemExecbuffer2:
        return DRM_IOCTL_I915_GEM_EXECBUFFER2;
    case DrmIoctl::gemWait:
        return DRM_IOCTL_I915_GEM_WAIT;
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
    case DrmIoctl::gemMmapOffset:
        return DRM_IOCTL_I915_GEM_MMAP_OFFSET;
    case DrmIoctl::gemVmCreate:
        return DRM_IOCTL_I915_GEM_VM_CREATE;
    case DrmIoctl::gemVmDestroy:
        return DRM_IOCTL_I915_GEM_VM_DESTROY;
    case DrmIoctl::perfOpen:
        return DRM_IOCTL_I915_PERF_OPEN;
    case DrmIoctl::perfEnable:
        return I915_PERF_IOCTL_ENABLE;
    case DrmIoctl::perfDisable:
        return I915_PERF_IOCTL_DISABLE;
    default:
        return getIoctlRequestValueBase(ioctlRequest);
    }
}

std::unique_ptr<MemoryInfo> IoctlHelperI915::createMemoryInfo() {
    auto request = getDrmParamValue(DrmParam::queryMemoryRegions);
    auto dataQuery = drm.query<uint64_t>(request, 0);
    if (!dataQuery.empty()) {
        auto memRegions = translateToMemoryRegions(dataQuery);
        return std::make_unique<MemoryInfo>(memRegions, drm);
    }
    return {};
}

size_t IoctlHelperI915::getLocalMemoryRegionsSize(const MemoryInfo *memoryInfo, uint32_t subDevicesCount, uint32_t deviceBitfield) const {
    size_t size = 0;

    for (uint32_t i = 0; i < subDevicesCount; i++) {
        auto memoryBank = (1 << i);

        if (deviceBitfield & memoryBank) {
            size += memoryInfo->getMemoryRegionSize(memoryBank);
        }
    }
    return size;
}

std::string IoctlHelperI915::getDrmParamString(DrmParam drmParam) const {
    switch (drmParam) {
    case DrmParam::paramHasPooledEu:
        return "I915_PARAM_HAS_POOLED_EU";
    case DrmParam::paramEuTotal:
        return "I915_PARAM_EU_TOTAL";
    case DrmParam::paramSubsliceTotal:
        return "I915_PARAM_SUBSLICE_TOTAL";
    case DrmParam::paramMinEuInPool:
        return "I915_PARAM_MIN_EU_IN_POOL";
    case DrmParam::paramCsTimestampFrequency:
        return "I915_PARAM_CS_TIMESTAMP_FREQUENCY";
    case DrmParam::paramOATimestampFrequency:
        return "I915_PARAM_OA_TIMESTAMP_FREQUENCY";
    default:
        UNRECOVERABLE_IF(true);
        return "";
    }
}

std::string IoctlHelperI915::getIoctlString(DrmIoctl ioctlRequest) const {
    switch (ioctlRequest) {
    case DrmIoctl::gemExecbuffer2:
        return "DRM_IOCTL_I915_GEM_EXECBUFFER2";
    case DrmIoctl::gemWait:
        return "DRM_IOCTL_I915_GEM_WAIT";
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
    case DrmIoctl::gemMmapOffset:
        return "DRM_IOCTL_I915_GEM_MMAP_OFFSET";
    case DrmIoctl::gemVmCreate:
        return "DRM_IOCTL_I915_GEM_VM_CREATE";
    case DrmIoctl::gemVmDestroy:
        return "DRM_IOCTL_I915_GEM_VM_DESTROY";
    case DrmIoctl::perfOpen:
        return "DRM_IOCTL_I915_PERF_OPEN";
    case DrmIoctl::perfEnable:
        return "I915_PERF_IOCTL_ENABLE";
    case DrmIoctl::perfDisable:
        return "I915_PERF_IOCTL_DISABLE";
    default:
        return getIoctlStringBase(ioctlRequest);
    }
}

int IoctlHelperI915::createDrmContext(Drm &drm, OsContextLinux &osContext, uint32_t drmVmId, uint32_t deviceIndex, bool allocateInterrupt) {

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
    auto engineFlag = drm.bindDrmContext(drmContextId, deviceIndex, osContext.getEngineType());
    osContext.setEngineFlag(engineFlag);
    return drmContextId;
}

std::string IoctlHelperI915::getFileForMaxGpuFrequency() const {
    return "/gt_max_freq_mhz";
}

std::string IoctlHelperI915::getFileForMaxGpuFrequencyOfSubDevice(int tileId) const {
    return "/gt/gt" + std::to_string(tileId) + "/rps_max_freq_mhz";
}

std::string IoctlHelperI915::getFileForMaxMemoryFrequencyOfSubDevice(int tileId) const {
    return "/gt/gt" + std::to_string(tileId) + "/mem_RP0_freq_mhz";
}

void IoctlHelperI915::configureCcsMode(std::vector<std::string> &files, const std::string expectedFilePrefix, uint32_t ccsMode,
                                       std::vector<std::tuple<std::string, uint32_t>> &deviceCcsModeVec) {

    // On i915 path to ccs_mode is /sys/class/drm/card0/gt/gt*/
    for (const auto &file : files) {
        if (file.find(expectedFilePrefix.c_str()) == std::string::npos) {
            continue;
        }

        std::string gtPath = file + "/gt";
        auto gtFiles = Directory::getFiles(gtPath.c_str());
        auto expectedGtFilePrefix = gtPath + "/gt";
        for (const auto &gtFile : gtFiles) {
            if (gtFile.find(expectedGtFilePrefix.c_str()) == std::string::npos) {
                continue;
            }
            writeCcsMode(gtFile, ccsMode, deviceCcsModeVec);
        }
    }
}

bool IoctlHelperI915::getTopologyDataAndMap(const HardwareInfo &hwInfo, DrmQueryTopologyData &topologyData, TopologyMap &topologyMap) {

    auto request = this->getDrmParamValue(DrmParam::queryTopologyInfo);
    auto dataQuery = drm.query<uint64_t>(request, 0);
    if (dataQuery.empty()) {
        return false;
    }
    auto topologyInfo = reinterpret_cast<QueryTopologyInfo *>(dataQuery.data());

    TopologyMapping mapping;
    auto retVal = this->translateTopologyInfo(topologyInfo, topologyData, mapping);

    topologyMap.clear();
    if (!mapping.sliceIndices.empty()) {
        topologyMap[0] = mapping;
    }

    return retVal;
}

bool IoctlHelperI915::translateTopologyInfo(const QueryTopologyInfo *queryTopologyInfo, DrmQueryTopologyData &topologyData, TopologyMapping &mapping) {
    UNRECOVERABLE_IF(queryTopologyInfo->subsliceOffset != static_cast<uint16_t>(Math::divideAndRoundUp(queryTopologyInfo->maxSlices, 8u)));
    UNRECOVERABLE_IF(queryTopologyInfo->subsliceStride != static_cast<uint16_t>(Math::divideAndRoundUp(queryTopologyInfo->maxSubslices, 8u)));
    UNRECOVERABLE_IF(queryTopologyInfo->euOffset != queryTopologyInfo->subsliceOffset + queryTopologyInfo->maxSlices * queryTopologyInfo->subsliceStride);
    UNRECOVERABLE_IF(queryTopologyInfo->euStride != static_cast<uint16_t>(Math::divideAndRoundUp(queryTopologyInfo->maxEusPerSubslice, 8u)));

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
    topologyData.maxSlices = maxSliceCount;
    topologyData.maxSubSlicesPerSlice = maxSubSliceCountPerSlice;
    topologyData.maxEusPerSubSlice = queryTopologyInfo->maxEusPerSubslice;

    return (sliceCount && subSliceCount && euCount);
}

bool IoctlHelperI915::getFdFromVmExport(uint32_t vmId, uint32_t flags, int32_t *fd) {
    return false;
}

uint32_t IoctlHelperI915::createGem(uint64_t size, uint32_t memoryBanks, std::optional<bool> isCoherent) {
    GemCreate gemCreate = {};
    gemCreate.size = size;
    [[maybe_unused]] auto ret = ioctl(DrmIoctl::gemCreate, &gemCreate);
    DEBUG_BREAK_IF(ret != 0);
    return gemCreate.handle;
}

bool IoctlHelperI915::setGemTiling(void *setTiling) {
    return this->ioctl(DrmIoctl::gemSetTiling, setTiling) == 0;
}

bool IoctlHelperI915::getGemTiling(void *setTiling) {
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

bool getGpuTimeSplit(::NEO::Drm &drm, uint64_t *timestamp) {
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

void IoctlHelperI915::initializeGetGpuTimeFunction() {
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
            this->getGpuTime = &getGpuTimeSplit;
        }
    } else {
        this->getGpuTime = &getGpuTime36;
    }
}

bool IoctlHelperI915::setGpuCpuTimes(TimeStampData *pGpuCpuTime, OSTime *osTime) {
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

void IoctlHelperI915::insertEngineToContextParams(ContextParamEngines<> &contextParamEngines, uint32_t engineId, const EngineClassInstance *engineClassInstance, uint32_t tileId, bool hasVirtualEngines) {
    auto engines = reinterpret_cast<EngineClassInstance *>(contextParamEngines.enginesData);
    if (!engineClassInstance) {
        engines[engineId].engineClass = getDrmParamValue(DrmParam::engineClassInvalid);
        engines[engineId].engineInstance = getDrmParamValue(DrmParam::engineClassInvalidNone);
    } else {
        auto index = engineId;
        if (hasVirtualEngines) {
            index++;
        }
        engines[index] = *engineClassInstance;
    }
}

bool IoctlHelperI915::isPreemptionSupported() {
    int schedulerCap{};
    GetParam getParam{};
    getParam.param = I915_PARAM_HAS_SCHEDULER;
    getParam.value = &schedulerCap;

    int retVal = ioctl(DrmIoctl::getparam, &getParam);
    if (debugManager.flags.PrintIoctlEntries.get()) {
        printf("DRM_IOCTL_I915_GETPARAM: param: I915_PARAM_HAS_SCHEDULER, output value: %d, retCode:% d\n",
               *getParam.value,
               retVal);
    }
    return retVal == 0 && (schedulerCap & I915_SCHEDULER_CAP_PREEMPTION);
}

bool IoctlHelperI915::hasContextFreqHint() {
    int param{};
    GetParam getParam{};
    getParam.param = I915_PARAM_HAS_CONTEXT_FREQ_HINT;
    getParam.value = &param;

    int retVal = ioctl(DrmIoctl::getparam, &getParam);
    if (debugManager.flags.PrintIoctlEntries.get()) {
        printf("DRM_IOCTL_I915_GETPARAM: param: I915_PARAM_HAS_CONTEXT_FREQ_HINT, output value: %d, retCode:% d\n",
               *getParam.value,
               retVal);
    }
    return retVal == 0 && (param == 1);
}

bool IoctlHelperI915::retrieveMmapOffsetForBufferObject(BufferObject &bo, uint64_t flags, uint64_t &offset) {
    constexpr uint64_t mmapOffsetFixed = 4;
    constexpr uint64_t mmapOffsetCoherent = I915_MMAP_OFFSET_WB;
    constexpr uint64_t mmapOffsetNonCoherent = I915_MMAP_OFFSET_WC;

    GemMmapOffset mmapOffset = {};
    mmapOffset.handle = bo.peekHandle();

    auto &rootDeviceEnvironment = drm.getRootDeviceEnvironment();
    auto &productHelper = rootDeviceEnvironment.getProductHelper();
    auto memoryManager = rootDeviceEnvironment.executionEnvironment.memoryManager.get();

    if (memoryManager->isLocalMemorySupported(bo.getRootDeviceIndex())) {
        mmapOffset.flags = mmapOffsetFixed;
    } else {
        if (productHelper.useGemCreateExtInAllocateMemoryByKMD()) {
            switch (bo.peekBOType()) {
            case NEO::BufferObject::BOType::nonCoherent:
                mmapOffset.flags = mmapOffsetNonCoherent;
                break;
            case NEO::BufferObject::BOType::legacy:
            case NEO::BufferObject::BOType::coherent:
            default:
                mmapOffset.flags = mmapOffsetCoherent;
            }
        } else {
            mmapOffset.flags = flags;
        }
    }

    auto ret = ioctl(DrmIoctl::gemMmapOffset, &mmapOffset);
    if (ret != 0 && memoryManager->isLocalMemorySupported(bo.getRootDeviceIndex())) {
        mmapOffset.flags = flags;
        ret = ioctl(DrmIoctl::gemMmapOffset, &mmapOffset);
    }
    if (ret != 0) {
        int err = drm.getErrno();

        CREATE_DEBUG_STRING(str, "ioctl(%s) failed with %d. errno=%d(%s)\n",
                            getIoctlString(DrmIoctl::gemMmapOffset).c_str(), ret, err, strerror(err));
        drm.getRootDeviceEnvironment().executionEnvironment.setErrorDescription(std::string(str.get()));
        PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, str.get());
        DEBUG_BREAK_IF(true);

        return false;
    }

    offset = mmapOffset.offset;
    return true;
};

void IoctlHelperI915::fillExtSetparamLowLatency(GemContextCreateExtSetParam &extSetparam) {
    extSetparam.base.name = getDrmParamValue(DrmParam::contextCreateExtSetparam);
    extSetparam.param.param = I915_CONTEXT_PARAM_LOW_LATENCY;
    extSetparam.param.value = 1;
    return;
}

bool IoctlHelperI915::queryDeviceIdAndRevision(Drm &drm) {

    HardwareInfo *hwInfo = drm.getRootDeviceEnvironment().getMutableHardwareInfo();
    auto fileDescriptor = drm.getFileDescriptor();
    int param{};

    GetParam getParam{};
    getParam.param = I915_PARAM_CHIPSET_ID;
    getParam.value = &param;

    int ret = SysCalls::ioctl(fileDescriptor, DRM_IOCTL_I915_GETPARAM, &getParam);
    if (ret) {
        printDebugString(debugManager.flags.PrintDebugMessages.get(), stderr, "%s", "FATAL: Cannot query device ID parameter!\n");
        return false;
    }

    hwInfo->platform.usDeviceID = param;

    getParam.param = I915_PARAM_REVISION;
    ret = SysCalls::ioctl(fileDescriptor, DRM_IOCTL_I915_GETPARAM, &getParam);

    if (ret != 0) {
        printDebugString(debugManager.flags.PrintDebugMessages.get(), stderr, "%s", "FATAL: Cannot query device Rev ID parameter!\n");
        return false;
    }

    hwInfo->platform.usRevId = param;
    return true;
}

} // namespace NEO
