/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"

#include "shared/source/debugger/debugger.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/engine_control.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/file_descriptor.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/linux/xe/xedrm.h"
#include "shared/source/os_interface/os_time.h"
#include "shared/source/utilities/directory.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <sstream>

#define STRINGIFY_ME(X) return #X
#define RETURN_ME(X) return X

namespace NEO {

const char *IoctlHelperXe::xeGetClassName(int className) const {
    switch (className) {
    case DRM_XE_ENGINE_CLASS_RENDER:
        return "rcs";
    case DRM_XE_ENGINE_CLASS_COPY:
        return "bcs";
    case DRM_XE_ENGINE_CLASS_VIDEO_DECODE:
        return "vcs";
    case DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE:
        return "vecs";
    case DRM_XE_ENGINE_CLASS_COMPUTE:
        return "ccs";
    }
    return "Unknown class name";
}

const char *IoctlHelperXe::xeGetBindOperationName(int bindOperation) {
    switch (bindOperation) {
    case DRM_XE_VM_BIND_OP_MAP:
        return "MAP";
    case DRM_XE_VM_BIND_OP_UNMAP:
        return "UNMAP";
    case DRM_XE_VM_BIND_OP_MAP_USERPTR:
        return "MAP_USERPTR";
    case DRM_XE_VM_BIND_OP_UNMAP_ALL:
        return "UNMAP ALL";
    case DRM_XE_VM_BIND_OP_PREFETCH:
        return "PREFETCH";
    }
    return "Unknown operation";
}

const char *IoctlHelperXe::xeGetAdviseOperationName(int adviseOperation) {
    return "Unknown operation";
}

std::string IoctlHelperXe::xeGetBindFlagNames(int bindFlags) {
    if (bindFlags == 0) {
        return "";
    }

    std::string flags;
    if (bindFlags & DRM_XE_VM_BIND_FLAG_READONLY) {
        bindFlags &= ~DRM_XE_VM_BIND_FLAG_READONLY;
        flags += "READONLY ";
    }
    if (bindFlags & DRM_XE_VM_BIND_FLAG_IMMEDIATE) {
        bindFlags &= ~DRM_XE_VM_BIND_FLAG_IMMEDIATE;
        flags += "IMMEDIATE ";
    }
    if (bindFlags & DRM_XE_VM_BIND_FLAG_NULL) {
        bindFlags &= ~DRM_XE_VM_BIND_FLAG_NULL;
        flags += "NULL ";
    }
    if (bindFlags & DRM_XE_VM_BIND_FLAG_DUMPABLE) {
        bindFlags &= ~DRM_XE_VM_BIND_FLAG_DUMPABLE;
        flags += "DUMPABLE ";
    }

    if (bindFlags != 0) {
        flags += "Unknown flag ";
    }

    // Remove the trailing space
    if (!flags.empty() && flags.back() == ' ') {
        flags.pop_back();
    }

    return flags;
}

const char *IoctlHelperXe::xeGetengineClassName(uint32_t engineClass) {
    switch (engineClass) {
    case DRM_XE_ENGINE_CLASS_RENDER:
        return "DRM_XE_ENGINE_CLASS_RENDER";
    case DRM_XE_ENGINE_CLASS_COPY:
        return "DRM_XE_ENGINE_CLASS_COPY";
    case DRM_XE_ENGINE_CLASS_VIDEO_DECODE:
        return "DRM_XE_ENGINE_CLASS_VIDEO_DECODE";
    case DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE:
        return "DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE";
    case DRM_XE_ENGINE_CLASS_COMPUTE:
        return "DRM_XE_ENGINE_CLASS_COMPUTE";
    default:
        return "Unknown engine class";
    }
}

IoctlHelperXe::IoctlHelperXe(Drm &drmArg) : IoctlHelper(drmArg) {
    xeLog("IoctlHelperXe::IoctlHelperXe\n", "");
}

bool IoctlHelperXe::queryDeviceIdAndRevision(Drm &drm) {
    auto fileDescriptor = drm.getFileDescriptor();

    drm_xe_device_query queryConfig = {};
    queryConfig.query = DRM_XE_DEVICE_QUERY_CONFIG;

    int ret = SysCalls::ioctl(fileDescriptor, DRM_IOCTL_XE_DEVICE_QUERY, &queryConfig);
    if (ret || queryConfig.size == 0) {
        printDebugString(debugManager.flags.PrintDebugMessages.get(), stderr, "%s", "FATAL: Cannot query size for device config!\n");
        return false;
    }

    auto data = std::vector<uint64_t>(Math::divideAndRoundUp(sizeof(drm_xe_query_config) + sizeof(uint64_t) * queryConfig.size, sizeof(uint64_t)), 0);
    struct drm_xe_query_config *config = reinterpret_cast<struct drm_xe_query_config *>(data.data());
    queryConfig.data = castToUint64(config);

    ret = SysCalls::ioctl(fileDescriptor, DRM_IOCTL_XE_DEVICE_QUERY, &queryConfig);

    if (ret) {
        printDebugString(debugManager.flags.PrintDebugMessages.get(), stderr, "%s", "FATAL: Cannot query device ID and revision!\n");
        return false;
    }

    auto hwInfo = drm.getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->platform.usDeviceID = config->info[DRM_XE_QUERY_CONFIG_REV_AND_DEVICE_ID] & 0xffff;
    hwInfo->platform.usRevId = static_cast<int>((config->info[DRM_XE_QUERY_CONFIG_REV_AND_DEVICE_ID] >> 16) & 0xff);

    if ((debugManager.flags.EnableRecoverablePageFaults.get() != 0) && (debugManager.flags.EnableSharedSystemUsmSupport.get() == 1) && (config->info[DRM_XE_QUERY_CONFIG_FLAGS] & DRM_XE_QUERY_CONFIG_FLAG_HAS_CPU_ADDR_MIRROR)) {
        drm.setSharedSystemAllocEnable(true);
        drm.setPageFaultSupported(true);
    }
    return true;
}

uint32_t IoctlHelperXe::getGtIdFromTileId(uint32_t tileId, uint16_t engineClass) const {

    if (engineClass == DRM_XE_ENGINE_CLASS_VIDEO_DECODE || engineClass == DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE) {
        return static_cast<uint32_t>(tileIdToMediaGtId[tileId]);
    }
    return static_cast<uint32_t>(tileIdToGtId[tileId]);
}

bool IoctlHelperXe::initialize() {
    xeLog("IoctlHelperXe::initialize\n", "");

    euDebugInterface = EuDebugInterface::create(drm.getSysFsPciPath());

    drm_xe_device_query queryConfig = {};
    queryConfig.query = DRM_XE_DEVICE_QUERY_CONFIG;

    auto retVal = IoctlHelper::ioctl(DrmIoctl::query, &queryConfig);
    if (retVal != 0 || queryConfig.size == 0) {
        return false;
    }
    auto data = std::vector<uint64_t>(Math::divideAndRoundUp(sizeof(drm_xe_query_config) + sizeof(uint64_t) * queryConfig.size, sizeof(uint64_t)), 0);
    struct drm_xe_query_config *config = reinterpret_cast<struct drm_xe_query_config *>(data.data());
    queryConfig.data = castToUint64(config);
    IoctlHelper::ioctl(DrmIoctl::query, &queryConfig);
    xeLog("DRM_XE_QUERY_CONFIG_REV_AND_DEVICE_ID\t%#llx\n",
          config->info[DRM_XE_QUERY_CONFIG_REV_AND_DEVICE_ID]);
    xeLog("  REV_ID\t\t\t\t%#llx\n",
          (config->info[DRM_XE_QUERY_CONFIG_REV_AND_DEVICE_ID] >> 16) & 0xff);
    xeLog("  DEVICE_ID\t\t\t\t%#llx\n",
          config->info[DRM_XE_QUERY_CONFIG_REV_AND_DEVICE_ID] & 0xffff);
    xeLog("DRM_XE_QUERY_CONFIG_FLAGS\t\t\t%#llx\n",
          config->info[DRM_XE_QUERY_CONFIG_FLAGS]);
    xeLog("  DRM_XE_QUERY_CONFIG_FLAG_HAS_VRAM\t%s\n",
          config->info[DRM_XE_QUERY_CONFIG_FLAGS] &
                  DRM_XE_QUERY_CONFIG_FLAG_HAS_VRAM
              ? "ON"
              : "OFF");
    xeLog("DRM_XE_QUERY_CONFIG_MIN_ALIGNMENT\t\t%#llx\n",
          config->info[DRM_XE_QUERY_CONFIG_MIN_ALIGNMENT]);
    xeLog("DRM_XE_QUERY_CONFIG_VA_BITS\t\t%#llx\n",
          config->info[DRM_XE_QUERY_CONFIG_VA_BITS]);

    xeLog("DRM_XE_QUERY_CONFIG_MAX_EXEC_QUEUE_PRIORITY\t\t%#llx\n",
          config->info[DRM_XE_QUERY_CONFIG_MAX_EXEC_QUEUE_PRIORITY]);

    maxExecQueuePriority = config->info[DRM_XE_QUERY_CONFIG_MAX_EXEC_QUEUE_PRIORITY] & 0xffff;

    memset(&queryConfig, 0, sizeof(queryConfig));
    queryConfig.query = DRM_XE_DEVICE_QUERY_HWCONFIG;
    IoctlHelper::ioctl(DrmIoctl::query, &queryConfig);
    auto newSize = queryConfig.size / sizeof(uint32_t);
    hwconfig.resize(newSize);
    queryConfig.data = castToUint64(hwconfig.data());
    IoctlHelper::ioctl(DrmIoctl::query, &queryConfig);

    auto hwInfo = this->drm.getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.gpuAddressSpace = (1ull << config->info[DRM_XE_QUERY_CONFIG_VA_BITS]) - 1;

    hwInfo->capabilityTable.cxlType = 0;
    if (getCxlType() && config->num_params > *getCxlType()) {
        hwInfo->capabilityTable.cxlType = static_cast<uint32_t>(config->info[*getCxlType()]);
    }

    queryGtListData = queryData<uint64_t>(DRM_XE_DEVICE_QUERY_GT_LIST);

    if (queryGtListData.empty()) {
        return false;
    }
    xeGtListData = reinterpret_cast<drm_xe_query_gt_list *>(queryGtListData.data());

    auto assignValue = [](auto &container, uint16_t id, uint16_t value) {
        if (container.size() < id + 1u) {
            container.resize(id + 1, invalidIndex);
        }
        container[id] = value;
    };

    gtIdToTileId.resize(xeGtListData->num_gt, invalidIndex);
    for (auto i = 0u; i < xeGtListData->num_gt; i++) {
        const auto &gt = xeGtListData->gt_list[i];
        if (gt.type == DRM_XE_QUERY_GT_TYPE_MAIN) {
            gtIdToTileId[gt.gt_id] = gt.tile_id;

            assignValue(tileIdToGtId, gt.tile_id, gt.gt_id);
        } else if (isMediaGt(gt.type)) {
            assignValue(mediaGtIdToTileId, gt.gt_id, gt.tile_id);
            assignValue(tileIdToMediaGtId, gt.tile_id, gt.gt_id);
        }
    }
    return true;
}

bool IoctlHelperXe::isMediaGt(uint16_t gtType) const {
    return (gtType == DRM_XE_QUERY_GT_TYPE_MEDIA);
}

IoctlHelperXe::~IoctlHelperXe() {
    xeLog("IoctlHelperXe::~IoctlHelperXe\n", "");
}

bool IoctlHelperXe::isSetPairAvailable() {
    return false;
}

bool IoctlHelperXe::isChunkingAvailable() {
    return false;
}

bool IoctlHelperXe::isVmBindAvailable() {
    return true;
}

bool IoctlHelperXe::setDomainCpu(uint32_t handle, bool writeEnable) {
    return false;
}

template <typename DataType>
std::vector<DataType> IoctlHelperXe::queryData(uint32_t queryId) {
    struct drm_xe_device_query deviceQuery = {};
    deviceQuery.query = queryId;

    IoctlHelper::ioctl(DrmIoctl::query, &deviceQuery);

    std::vector<DataType> retVal(Math::divideAndRoundUp(deviceQuery.size, sizeof(DataType)));

    deviceQuery.data = castToUint64(retVal.data());
    IoctlHelper::ioctl(DrmIoctl::query, &deviceQuery);

    return retVal;
}
template std::vector<uint8_t> IoctlHelperXe::queryData(uint32_t queryId);
template std::vector<uint64_t> IoctlHelperXe::queryData(uint32_t queryId);

uint32_t IoctlHelperXe::getNumEngines(uint64_t *enginesData) const {
    return reinterpret_cast<struct drm_xe_query_engines *>(enginesData)->num_engines;
}

std::unique_ptr<EngineInfo> IoctlHelperXe::createEngineInfo(bool isSysmanEnabled) {
    auto enginesData = queryData<uint64_t>(DRM_XE_DEVICE_QUERY_ENGINES);

    if (enginesData.empty()) {
        return {};
    }

    auto queryEngines = reinterpret_cast<struct drm_xe_query_engines *>(enginesData.data());

    auto numberHwEngines = getNumEngines(enginesData.data());

    xeLog("numberHwEngines=%d\n", numberHwEngines);

    StackVec<std::vector<EngineCapabilities>, 2> enginesPerTile{};
    std::bitset<8> multiTileMask{};

    auto hwInfo = drm.getRootDeviceEnvironment().getMutableHardwareInfo();
    auto defaultEngineClass = getDefaultEngineClass(hwInfo->capabilityTable.defaultEngineType);

    auto containsGtId = [](const auto &container, uint16_t gtId) {
        return ((container.size() > gtId) && (container[gtId] != invalidIndex));
    };

    for (auto i = 0u; i < numberHwEngines; i++) {
        const auto &engine = queryEngines->engines[i].instance;

        uint16_t tile = 0;
        const bool mediaEngine = isMediaEngine(engine.engine_class);
        const bool videoEngine = (engine.engine_class == getDrmParamValue(DrmParam::engineClassVideo) || engine.engine_class == getDrmParamValue(DrmParam::engineClassVideoEnhance));

        if (containsGtId(gtIdToTileId, engine.gt_id) && !mediaEngine) {
            tile = static_cast<uint16_t>(gtIdToTileId[engine.gt_id]);
        } else if (containsGtId(mediaGtIdToTileId, engine.gt_id) && (mediaEngine || videoEngine)) {
            tile = static_cast<uint16_t>(mediaGtIdToTileId[engine.gt_id]);
        } else {
            continue;
        }

        multiTileMask.set(tile);
        EngineClassInstance engineClassInstance{};
        engineClassInstance.engineClass = engine.engine_class;
        engineClassInstance.engineInstance = engine.engine_instance;
        xeLog("\tclass: %s, instance: %d, gt_id: %d, tile: %d\n", xeGetClassName(engineClassInstance.engineClass), engineClassInstance.engineInstance, engine.gt_id, tile);

        const bool isBaseEngineClass = engineClassInstance.engineClass == getDrmParamValue(DrmParam::engineClassCompute) ||
                                       engineClassInstance.engineClass == getDrmParamValue(DrmParam::engineClassRender) ||
                                       engineClassInstance.engineClass == getDrmParamValue(DrmParam::engineClassCopy);

        const bool isSysmanEngineClass = isSysmanEnabled && videoEngine;

        if (isBaseEngineClass || isSysmanEngineClass || mediaEngine) {
            if (enginesPerTile.size() <= tile) {
                enginesPerTile.resize(tile + 1);
            }
            enginesPerTile[tile].push_back({engineClassInstance, {}});
            if (!defaultEngine && engineClassInstance.engineClass == defaultEngineClass) {
                defaultEngine = std::make_unique<drm_xe_engine_class_instance>();
                *defaultEngine = engine;
            }
        }
    }
    UNRECOVERABLE_IF(!defaultEngine);
    if (hwInfo->featureTable.flags.ftrMultiTileArch) {
        auto &multiTileArchInfo = hwInfo->gtSystemInfo.MultiTileArchInfo;
        multiTileArchInfo.IsValid = true;
        multiTileArchInfo.TileCount = multiTileMask.count();
        multiTileArchInfo.TileMask = static_cast<uint8_t>(multiTileMask.to_ulong());
    }

    return std::make_unique<EngineInfo>(&drm, enginesPerTile);
}

inline MemoryRegion createMemoryRegionFromXeMemRegion(const drm_xe_mem_region &xeMemRegion, std::bitset<4> tilesMask) {
    return {
        .region{
            .memoryClass = xeMemRegion.mem_class,
            .memoryInstance = xeMemRegion.instance},
        .probedSize = xeMemRegion.total_size,
        .unallocatedSize = xeMemRegion.total_size - xeMemRegion.used,
        .cpuVisibleSize = xeMemRegion.cpu_visible_size,
        .tilesMask = tilesMask,
    };
}

std::unique_ptr<MemoryInfo> IoctlHelperXe::createMemoryInfo() {
    auto memUsageData = queryData<uint64_t>(DRM_XE_DEVICE_QUERY_MEM_REGIONS);

    if (memUsageData.empty()) {
        return {};
    }

    constexpr auto maxSupportedTilesNumber{4u};
    std::array<std::bitset<maxSupportedTilesNumber>, 64> regionTilesMask{};

    for (auto i{0u}; i < xeGtListData->num_gt; i++) {
        const auto &gtEntry = xeGtListData->gt_list[i];
        if (gtEntry.type != DRM_XE_QUERY_GT_TYPE_MAIN) {
            continue;
        }

        uint64_t nearMemRegions{gtEntry.near_mem_regions};
        auto regionIndex{Math::log2(nearMemRegions)};
        regionTilesMask[regionIndex].set(gtEntry.tile_id);
    }

    MemoryInfo::RegionContainer regionsContainer{};

    auto xeMemRegionsData = reinterpret_cast<drm_xe_query_mem_regions *>(memUsageData.data());
    for (auto i = 0u; i < xeMemRegionsData->num_mem_regions; i++) {
        auto &xeMemRegion{xeMemRegionsData->mem_regions[i]};

        if (xeMemRegion.mem_class == DRM_XE_MEM_REGION_CLASS_SYSMEM) {
            // Make sure sysmem is always put at the first position
            regionsContainer.insert(regionsContainer.begin(), createMemoryRegionFromXeMemRegion(xeMemRegion, 0u));
        } else {
            auto regionIndex = xeMemRegion.instance;
            UNRECOVERABLE_IF(regionIndex >= regionTilesMask.size());
            if (auto tilesMask = regionTilesMask[regionIndex]; tilesMask.any()) {
                regionsContainer.push_back(createMemoryRegionFromXeMemRegion(xeMemRegion, tilesMask));
            }
        }
    }

    if (regionsContainer.empty()) {
        return {};
    }

    return std::make_unique<MemoryInfo>(regionsContainer, drm);
}

size_t IoctlHelperXe::getLocalMemoryRegionsSize(const MemoryInfo *memoryInfo, uint32_t subDevicesCount, uint32_t tileMask) const {
    size_t size = 0;
    for (const auto &memoryRegion : memoryInfo->getLocalMemoryRegions()) {
        if ((memoryRegion.tilesMask & std::bitset<4>{tileMask}).any()) {
            size += memoryRegion.probedSize;
        }
    }
    return size;
}

void IoctlHelperXe::setupIpVersion() {
    auto &rootDeviceEnvironment = drm.getRootDeviceEnvironment();
    auto hwInfo = rootDeviceEnvironment.getMutableHardwareInfo();

    if (auto hwIpVersion = GtIpVersion{}; queryHwIpVersion(hwIpVersion)) {
        hwInfo->ipVersion.architecture = hwIpVersion.major;
        hwInfo->ipVersion.release = hwIpVersion.minor;
        hwInfo->ipVersion.revision = hwIpVersion.revision;
    } else {
        xeLog("No HW IP version received from drm_xe_gt. Falling back to default value.");
        IoctlHelper::setupIpVersion();
    }
}

bool IoctlHelperXe::queryHwIpVersion(GtIpVersion &gtIpVersion) {
    auto gtListData = queryData<uint64_t>(DRM_XE_DEVICE_QUERY_GT_LIST);
    if (gtListData.empty()) {
        return false;
    }

    auto xeGtListData = reinterpret_cast<drm_xe_query_gt_list *>(gtListData.data());
    for (auto i = 0u; i < xeGtListData->num_gt; i++) {
        auto &gtEntry = xeGtListData->gt_list[i];

        if (gtEntry.type == DRM_XE_QUERY_GT_TYPE_MEDIA || gtEntry.ip_ver_major == 0u) {
            continue;
        }
        gtIpVersion.major = gtEntry.ip_ver_major;
        gtIpVersion.minor = gtEntry.ip_ver_minor;
        gtIpVersion.revision = gtEntry.ip_ver_rev;
        return true;
    }
    return false;
}

bool IoctlHelperXe::setGpuCpuTimes(TimeStampData *pGpuCpuTime, OSTime *osTime) {
    if (pGpuCpuTime == nullptr || osTime == nullptr) {
        return false;
    }

    drm_xe_device_query deviceQuery = {};
    deviceQuery.query = DRM_XE_DEVICE_QUERY_ENGINE_CYCLES;

    auto ret = IoctlHelper::ioctl(DrmIoctl::query, &deviceQuery);

    if (ret != 0) {
        xeLog(" -> IoctlHelperXe::%s s=0x%lx r=%d\n", __FUNCTION__, deviceQuery.size, ret);
        return false;
    }

    std::vector<uint8_t> retVal(deviceQuery.size);
    deviceQuery.data = castToUint64(retVal.data());

    drm_xe_query_engine_cycles *queryEngineCycles = reinterpret_cast<drm_xe_query_engine_cycles *>(retVal.data());
    queryEngineCycles->clockid = CLOCK_MONOTONIC_RAW;
    queryEngineCycles->eci = *this->defaultEngine;

    ret = IoctlHelper::ioctl(DrmIoctl::query, &deviceQuery);

    auto nValidBits = queryEngineCycles->width;
    auto gpuTimestampValidBits = maxNBitValue(nValidBits);
    auto gpuCycles = queryEngineCycles->engine_cycles & gpuTimestampValidBits;

    xeLog(" -> IoctlHelperXe::%s [%d,%d] clockId=0x%x s=0x%lx nValidBits=0x%x gpuCycles=0x%x cpuTimeInNS=0x%x r=%d\n", __FUNCTION__,
          queryEngineCycles->eci.engine_class, queryEngineCycles->eci.engine_instance,
          queryEngineCycles->clockid, deviceQuery.size, nValidBits, gpuCycles, queryEngineCycles->cpu_timestamp, ret);

    pGpuCpuTime->gpuTimeStamp = gpuCycles;
    pGpuCpuTime->cpuTimeinNS = queryEngineCycles->cpu_timestamp;

    return ret == 0;
}

bool IoctlHelperXe::getTopologyDataAndMap(const HardwareInfo &hwInfo, DrmQueryTopologyData &topologyData, TopologyMap &topologyMap) {

    auto queryGtTopology = queryData<uint8_t>(DRM_XE_DEVICE_QUERY_GT_TOPOLOGY);

    auto fillMask = [](std::vector<std::bitset<8>> &vec, drm_xe_query_topology_mask *topo) {
        for (uint32_t j = 0; j < topo->num_bytes; j++) {
            vec.push_back(topo->mask[j]);
        }
    };

    StackVec<std::vector<std::bitset<8>>, 2> geomDss;
    StackVec<std::vector<std::bitset<8>>, 2> computeDss;
    StackVec<std::vector<std::bitset<8>>, 2> euDss;
    StackVec<std::vector<std::bitset<8>>, 2> l3Banks;

    auto topologySize = queryGtTopology.size();
    auto dataPtr = queryGtTopology.data();

    auto numTiles = tileIdToGtId.size();
    geomDss.resize(numTiles);
    computeDss.resize(numTiles);
    euDss.resize(numTiles);
    l3Banks.resize(numTiles);
    bool receivedDssInfo = false;
    while (topologySize >= sizeof(drm_xe_query_topology_mask)) {
        drm_xe_query_topology_mask *topo = reinterpret_cast<drm_xe_query_topology_mask *>(dataPtr);
        UNRECOVERABLE_IF(topo == nullptr);

        uint32_t gtId = topo->gt_id;
        auto tileId = gtIdToTileId[gtId];

        if (tileId != invalidIndex) {
            switch (topo->type) {
            case DRM_XE_TOPO_DSS_GEOMETRY:
                fillMask(geomDss[tileId], topo);
                receivedDssInfo = true;
                break;
            case DRM_XE_TOPO_DSS_COMPUTE:
                fillMask(computeDss[tileId], topo);
                receivedDssInfo = true;
                break;
            case DRM_XE_TOPO_L3_BANK:
                fillMask(l3Banks[tileId], topo);
                break;
            case DRM_XE_TOPO_EU_PER_DSS:
            case DRM_XE_TOPO_SIMD16_EU_PER_DSS:
                fillMask(euDss[tileId], topo);
                break;
            default:
                xeLog("Unhandle GT Topo type: %d\n", topo->type);
            }
        }

        uint32_t itemSize = sizeof(drm_xe_query_topology_mask) + topo->num_bytes;
        topologySize -= itemSize;
        dataPtr = ptrOffset(dataPtr, itemSize);
    }

    int sliceCount = 0;
    int subSliceCount = 0;
    int euPerDss = 0;
    int l3BankCount = 0;
    uint32_t hwMaxSubSliceCount = hwInfo.gtSystemInfo.MaxSubSlicesSupported;
    topologyData.maxSlices = hwInfo.gtSystemInfo.MaxSlicesSupported ? hwInfo.gtSystemInfo.MaxSlicesSupported : 1;
    topologyData.maxSubSlicesPerSlice = hwMaxSubSliceCount / topologyData.maxSlices;

    for (auto tileId = 0u; tileId < numTiles; tileId++) {

        int subSliceCountPerTile = 0;

        std::vector<int> sliceIndices;
        std::vector<int> subSliceIndices;

        int previouslyEnabledSlice = -1;

        auto processSubSliceInfo = [&](const std::vector<std::bitset<8>> &subSliceInfo) -> void {
            for (auto subSliceId = 0u; subSliceId < std::min(hwMaxSubSliceCount, static_cast<uint32_t>(subSliceInfo.size() * 8)); subSliceId++) {
                auto byte = subSliceId / 8;
                auto bit = subSliceId & 0b111;
                int sliceId = static_cast<int>(subSliceId / topologyData.maxSubSlicesPerSlice);
                if (subSliceInfo[byte].test(bit)) {
                    subSliceIndices.push_back(subSliceId);
                    subSliceCountPerTile++;
                    if (sliceId != previouslyEnabledSlice) {
                        previouslyEnabledSlice = sliceId;
                        sliceIndices.push_back(sliceId);
                    }
                }
            }
        };
        processSubSliceInfo(computeDss[tileId]);

        if (subSliceCountPerTile == 0) {
            processSubSliceInfo(geomDss[tileId]);
        }

        topologyMap[tileId].sliceIndices = std::move(sliceIndices);
        if (topologyMap[tileId].sliceIndices.size() < 2u) {
            topologyMap[tileId].subsliceIndices = std::move(subSliceIndices);
        }
        int sliceCountPerTile = static_cast<int>(topologyMap[tileId].sliceIndices.size());

        int euPerDssPerTile = 0;
        for (auto byte = 0u; byte < euDss[tileId].size(); byte++) {
            euPerDssPerTile += euDss[tileId][byte].count();
        }

        int l3BankCountPerTile = 0;
        for (auto byte = 0u; byte < l3Banks[tileId].size(); byte++) {
            l3BankCountPerTile += l3Banks[tileId][byte].count();
        }

        // pick smallest config
        sliceCount = (sliceCount == 0) ? sliceCountPerTile : std::min(sliceCount, sliceCountPerTile);
        subSliceCount = (subSliceCount == 0) ? subSliceCountPerTile : std::min(subSliceCount, subSliceCountPerTile);
        euPerDss = (euPerDss == 0) ? euPerDssPerTile : std::min(euPerDss, euPerDssPerTile);
        l3BankCount = (l3BankCount == 0) ? l3BankCountPerTile : std::min(l3BankCount, l3BankCountPerTile);

        // pick max config
        topologyData.maxEusPerSubSlice = std::max(topologyData.maxEusPerSubSlice, euPerDssPerTile);
    }

    topologyData.sliceCount = sliceCount;
    topologyData.subSliceCount = subSliceCount;
    topologyData.euCount = subSliceCount * euPerDss;
    topologyData.numL3Banks = l3BankCount;
    return receivedDssInfo;
}

void IoctlHelperXe::updateBindInfo(uint64_t userPtr) {
    std::unique_lock<std::mutex> lock(xeLock);
    BindInfo b = {userPtr, 0};
    bindInfo.push_back(b);
}

uint16_t IoctlHelperXe::getDefaultEngineClass(const aub_stream::EngineType &defaultEngineType) {
    if (defaultEngineType == aub_stream::EngineType::ENGINE_CCS) {
        return DRM_XE_ENGINE_CLASS_COMPUTE;
    } else if (defaultEngineType == aub_stream::EngineType::ENGINE_RCS) {
        return DRM_XE_ENGINE_CLASS_RENDER;
    } else {
        /* So far defaultEngineType is either ENGINE_RCS or ENGINE_CCS */
        UNRECOVERABLE_IF(true);
        return 0;
    }
}

/**
 * @brief returns caching policy for new allocation.
 * For system memory caching policy is write-back, otherwise it's write-combined.
 *
 * @param[in] allocationInSystemMemory flag that indicates if allocation will be allocated in system memory
 *
 * @return returns caching policy defined as DRM_XE_GEM_CPU_CACHING_WC or DRM_XE_GEM_CPU_CACHING_WB
 */
uint16_t IoctlHelperXe::getCpuCachingMode(std::optional<bool> isCoherent, bool allocationInSystemMemory) const {
    uint16_t cpuCachingMode = DRM_XE_GEM_CPU_CACHING_WC;
    if (allocationInSystemMemory) {
        if ((isCoherent.value_or(true) == true)) {
            cpuCachingMode = DRM_XE_GEM_CPU_CACHING_WB;
        }
    }

    if (debugManager.flags.OverrideCpuCaching.get() != -1) {
        cpuCachingMode = debugManager.flags.OverrideCpuCaching.get();
    }

    return cpuCachingMode;
}

int IoctlHelperXe::createGemExt(const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle, uint64_t patIndex, std::optional<uint32_t> vmId, int32_t pairHandle, bool isChunked, uint32_t numOfChunks, std::optional<uint32_t> memPolicyMode, std::optional<std::vector<unsigned long>> memPolicyNodemask, std::optional<bool> isCoherent) {
    struct drm_xe_gem_create create = {};
    uint32_t regionsSize = static_cast<uint32_t>(memClassInstances.size());

    if (!regionsSize) {
        xeLog("memClassInstances empty !\n", "");
        return -1;
    }

    create.size = allocSize;
    MemoryClassInstance mem = memClassInstances[regionsSize - 1];
    std::bitset<32> memoryInstances{};
    bool isSysMemOnly = true;
    for (const auto &memoryClassInstance : memClassInstances) {
        memoryInstances.set(memoryClassInstance.memoryInstance);
        if (memoryClassInstance.memoryClass != drm_xe_memory_class::DRM_XE_MEM_REGION_CLASS_SYSMEM) {
            isSysMemOnly = false;
        }
    }
    create.placement = static_cast<uint32_t>(memoryInstances.to_ulong());
    create.cpu_caching = this->getCpuCachingMode(isCoherent, isSysMemOnly);

    if (debugManager.flags.EnableDeferBacking.get()) {
        create.flags |= DRM_XE_GEM_CREATE_FLAG_DEFER_BACKING;
    }

    printDebugString(debugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "Performing DRM_IOCTL_XE_GEM_CREATE with {vmid=0x%x size=0x%lx flags=0x%x placement=0x%x caching=%hu }",
                     create.vm_id, create.size, create.flags, create.placement, create.cpu_caching);

    auto ret = IoctlHelper::ioctl(DrmIoctl::gemCreate, &create);
    handle = create.handle;

    printDebugString(debugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "DRM_IOCTL_XE_GEM_CREATE has returned: %d BO-%u with size: %lu\n", ret, handle, create.size);

    xeLog(" -> IoctlHelperXe::%s [%d,%d] vmid=0x%x s=0x%lx f=0x%x p=0x%x h=0x%x c=%hu r=%d\n", __FUNCTION__,
          mem.memoryClass, mem.memoryInstance,
          create.vm_id, create.size, create.flags, create.placement, handle, create.cpu_caching, ret);
    return ret;
}

uint32_t IoctlHelperXe::createGem(uint64_t size, uint32_t memoryBanks, std::optional<bool> isCoherent) {
    struct drm_xe_gem_create create = {};
    create.size = size;
    auto pHwInfo = drm.getRootDeviceEnvironment().getHardwareInfo();
    auto memoryInfo = drm.getMemoryInfo();
    std::bitset<32> memoryInstances{};
    auto banks = std::bitset<4>(memoryBanks);
    size_t currentBank = 0;
    size_t i = 0;
    bool isSysMemOnly = true;
    while (i < banks.count()) {
        if (banks.test(currentBank)) {
            auto regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(1u << currentBank, *pHwInfo);
            memoryInstances.set(regionClassAndInstance.memoryInstance);
            if (regionClassAndInstance.memoryClass != drm_xe_memory_class::DRM_XE_MEM_REGION_CLASS_SYSMEM) {
                isSysMemOnly = false;
            }
            i++;
        }
        currentBank++;
    }
    if (memoryBanks == 0) {
        auto regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(memoryBanks, *pHwInfo);
        memoryInstances.set(regionClassAndInstance.memoryInstance);
    }
    create.placement = static_cast<uint32_t>(memoryInstances.to_ulong());
    create.cpu_caching = this->getCpuCachingMode(isCoherent, isSysMemOnly);

    if (debugManager.flags.EnableDeferBacking.get()) {
        create.flags |= DRM_XE_GEM_CREATE_FLAG_DEFER_BACKING;
    }

    printDebugString(debugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "Performing DRM_IOCTL_XE_GEM_CREATE with {vmid=0x%x size=0x%lx flags=0x%x placement=0x%x caching=%hu }",
                     create.vm_id, create.size, create.flags, create.placement, create.cpu_caching);

    [[maybe_unused]] auto ret = ioctl(DrmIoctl::gemCreate, &create);

    printDebugString(debugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "DRM_IOCTL_XE_GEM_CREATE has returned: %d BO-%u with size: %lu\n", ret, create.handle, create.size);

    xeLog(" -> IoctlHelperXe::%s vmid=0x%x s=0x%lx f=0x%x p=0x%x h=0x%x c=%hu r=%d\n", __FUNCTION__,
          create.vm_id, create.size, create.flags, create.placement, create.handle, create.cpu_caching, ret);
    DEBUG_BREAK_IF(ret != 0);
    return create.handle;
}

CacheRegion IoctlHelperXe::closAlloc(CacheLevel cacheLevel) {
    xeLog(" -> IoctlHelperXe::%s\n", __FUNCTION__);
    return CacheRegion::none;
}

uint16_t IoctlHelperXe::closAllocWays(CacheRegion closIndex, uint16_t cacheLevel, uint16_t numWays) {
    xeLog(" -> IoctlHelperXe::%s\n", __FUNCTION__);
    return 0;
}

CacheRegion IoctlHelperXe::closFree(CacheRegion closIndex) {
    xeLog(" -> IoctlHelperXe::%s\n", __FUNCTION__);
    return CacheRegion::none;
}

void IoctlHelperXe::setupXeWaitUserFenceStruct(void *arg, uint32_t ctxId, uint16_t op, uint64_t addr, uint64_t value, int64_t timeout) {
    auto waitUserFence = reinterpret_cast<drm_xe_wait_user_fence *>(arg);
    waitUserFence->addr = addr;
    waitUserFence->op = op;
    waitUserFence->value = value;
    waitUserFence->mask = std::numeric_limits<uint64_t>::max();
    waitUserFence->timeout = timeout;
    waitUserFence->exec_queue_id = ctxId;
}

int IoctlHelperXe::xeWaitUserFence(uint32_t ctxId, uint16_t op, uint64_t addr, uint64_t value, int64_t timeout, bool userInterrupt, uint32_t externalInterruptId, GraphicsAllocation *allocForInterruptWait) {
    UNRECOVERABLE_IF(addr == 0x0)
    drm_xe_wait_user_fence waitUserFence = {};

    setupXeWaitUserFenceStruct(&waitUserFence, ctxId, op, addr, value, timeout);

    auto retVal = IoctlHelper::ioctl(DrmIoctl::gemWaitUserFence, &waitUserFence);
    xeLog(" -> IoctlHelperXe::%s a=0x%llx v=0x%llx T=0x%llx F=0x%x ctx=0x%x retVal=0x%x\n", __FUNCTION__,
          addr, value, timeout, waitUserFence.flags, ctxId, retVal);
    return retVal;
}

int IoctlHelperXe::waitUserFence(uint32_t ctxId, uint64_t address,
                                 uint64_t value, uint32_t dataWidth, int64_t timeout, uint16_t flags,
                                 bool userInterrupt, uint32_t externalInterruptId, GraphicsAllocation *allocForInterruptWait) {
    xeLog(" -> IoctlHelperXe::%s a=0x%llx v=0x%llx w=0x%x T=0x%llx F=0x%x ctx=0x%x\n", __FUNCTION__, address, value, dataWidth, timeout, flags, ctxId);
    UNRECOVERABLE_IF(dataWidth != static_cast<uint32_t>(Drm::ValueWidth::u64));
    if (address) {
        return xeWaitUserFence(ctxId, DRM_XE_UFENCE_WAIT_OP_GTE, address, value, timeout, userInterrupt, externalInterruptId, allocForInterruptWait);
    }
    return 0;
}

uint32_t IoctlHelperXe::getAtomicAdvise(bool /* isNonAtomic */) {
    xeLog(" -> IoctlHelperXe::%s\n", __FUNCTION__);
    return 0;
}

uint32_t IoctlHelperXe::getAtomicAccess(AtomicAccessMode mode) {
    xeLog(" -> IoctlHelperXe::%s\n", __FUNCTION__);
    return 0;
}

uint32_t IoctlHelperXe::getPreferredLocationAdvise() {
    xeLog(" -> IoctlHelperXe::%s\n", __FUNCTION__);
    return 0;
}

std::optional<MemoryClassInstance> IoctlHelperXe::getPreferredLocationRegion(PreferredLocation memoryLocation, uint32_t memoryInstance) {
    return std::nullopt;
}

bool IoctlHelperXe::setVmBoAdvise(int32_t handle, uint32_t attribute, void *region) {
    xeLog(" -> IoctlHelperXe::%s\n", __FUNCTION__);
    // There is no vmAdvise attribute in Xe, so return success
    return true;
}

bool IoctlHelperXe::setVmSharedSystemMemAdvise(uint64_t handle, const size_t size, const uint32_t attribute, const uint64_t param, const std::vector<uint32_t> &vmIds) {
    std::string vmIdsStr = "[";
    for (size_t i = 0; i < vmIds.size(); ++i) {
        {
            std::stringstream ss;
            ss << std::hex << vmIds[i];
            vmIdsStr += "0x" + ss.str();
        }
        if (i != vmIds.size() - 1) {
            vmIdsStr += ", ";
        }
    }
    vmIdsStr += "]";
    xeLog(" -> IoctlHelperXe::%s h=0x%x s=0x%lx vmids=%s\n", __FUNCTION__, handle, size, vmIdsStr.c_str());
    // There is no vmAdvise attribute in Xe, so return success
    return true;
}

bool IoctlHelperXe::setVmBoAdviseForChunking(int32_t handle, uint64_t start, uint64_t length, uint32_t attribute, void *region) {
    xeLog(" -> IoctlHelperXe::%s\n", __FUNCTION__);
    // There is no vmAdvise attribute in Xe, so return success
    return true;
}

bool IoctlHelperXe::setVmPrefetch(uint64_t start, uint64_t length, uint32_t region, uint32_t vmId) {
    xeLog(" -> IoctlHelperXe::%s s=0x%llx l=0x%llx align_s=0x%llx align_l=0x%llx vmid=0x%x\n", __FUNCTION__, start, length, alignDown(start, MemoryConstants::pageSize), alignSizeWholePage(reinterpret_cast<void *>(start), length), vmId);
    drm_xe_vm_bind bind = {};
    bind.vm_id = vmId;
    bind.num_binds = 1;

    bind.bind.range = alignSizeWholePage(reinterpret_cast<void *>(start), length);
    bind.bind.addr = alignDown(start, MemoryConstants::pageSize);
    bind.bind.op = DRM_XE_VM_BIND_OP_PREFETCH;

    auto pHwInfo = this->drm.getRootDeviceEnvironment().getHardwareInfo();
    constexpr uint32_t subDeviceMaskSize = DeviceBitfield().size();
    constexpr uint32_t subDeviceMaskMax = (1u << subDeviceMaskSize) - 1u;
    uint32_t subDeviceId = region & subDeviceMaskMax;
    DeviceBitfield subDeviceMask = (1u << subDeviceId);
    MemoryClassInstance regionInstanceClass = this->drm.getMemoryInfo()->getMemoryRegionClassAndInstance(subDeviceMask, *pHwInfo);
    bind.bind.prefetch_mem_region_instance = regionInstanceClass.memoryInstance;

    int ret = IoctlHelper::ioctl(DrmIoctl::gemVmBind, &bind);

    xeLog(" vm=%d addr=0x%lx range=0x%lx region=0x%x operation=%d(%s) ret=%d\n",
          bind.vm_id,
          bind.bind.addr,
          bind.bind.range,
          bind.bind.prefetch_mem_region_instance,
          bind.bind.op,
          xeGetBindOperationName(bind.bind.op),
          ret);

    if (ret != 0) {
        xeLog("error: %s ret=%d\n", xeGetBindOperationName(bind.bind.op), ret);
        return false;
    }

    return true;
}

uint32_t IoctlHelperXe::getDirectSubmissionFlag() {
    xeLog(" -> IoctlHelperXe::%s\n", __FUNCTION__);
    return 0;
}

uint16_t IoctlHelperXe::getWaitUserFenceSoftFlag() {
    xeLog(" -> IoctlHelperXe::%s\n", __FUNCTION__);
    return 0;
};

void IoctlHelperXe::fillExecObject(ExecObject &execObject, uint32_t handle, uint64_t gpuAddress, uint32_t drmContextId, bool bindInfo, bool isMarkedForCapture) {

    auto execObjectXe = reinterpret_cast<ExecObjectXe *>(execObject.data);
    execObjectXe->gpuAddress = gpuAddress;
    execObjectXe->handle = handle;
}

void IoctlHelperXe::logExecObject(const ExecObject &execObject, std::stringstream &logger, size_t size) {
    auto execObjectXe = reinterpret_cast<const ExecObjectXe *>(execObject.data);
    logger << "ExecBufferXe = { handle: BO-" << execObjectXe->handle
           << ", address range: 0x" << reinterpret_cast<void *>(execObjectXe->gpuAddress) << " }\n";
}

void IoctlHelperXe::fillExecBuffer(ExecBuffer &execBuffer, uintptr_t buffersPtr, uint32_t bufferCount, uint32_t startOffset, uint32_t size, uint64_t flags, uint32_t drmContextId) {
    auto execBufferXe = reinterpret_cast<ExecBufferXe *>(execBuffer.data);
    execBufferXe->execObject = reinterpret_cast<ExecObjectXe *>(buffersPtr);
    execBufferXe->startOffset = startOffset;
    execBufferXe->drmContextId = drmContextId;
}

void IoctlHelperXe::logExecBuffer(const ExecBuffer &execBuffer, std::stringstream &logger) {
    auto execBufferXe = reinterpret_cast<const ExecBufferXe *>(execBuffer.data);
    logger << "ExecBufferXe { "
           << "exec object: " + std::to_string(reinterpret_cast<uintptr_t>(execBufferXe->execObject))
           << ", start offset: " + std::to_string(execBufferXe->startOffset)
           << ", drm context id: " + std::to_string(execBufferXe->drmContextId)
           << " }\n";
}

int IoctlHelperXe::execBuffer(ExecBuffer *execBuffer, uint64_t completionGpuAddress, TaskCountType counterValue) {
    xeLog(" -> IoctlHelperXe::%s\n", __FUNCTION__);
    int ret = 0;
    if (execBuffer) {
        auto execBufferXe = reinterpret_cast<ExecBufferXe *>(execBuffer->data);
        if (execBufferXe) {
            auto execObject = execBufferXe->execObject;
            uint32_t engine = execBufferXe->drmContextId;

            xeLog("EXEC ofs=%d ctx=0x%x ptr=0x%p\n",
                  execBufferXe->startOffset, execBufferXe->drmContextId, execBufferXe->execObject);

            xeLog(" -> IoctlHelperXe::%s CA=0x%llx v=0x%x ctx=0x%x\n", __FUNCTION__,
                  completionGpuAddress, counterValue, engine);

            struct drm_xe_sync sync[1] = {};
            sync[0].type = DRM_XE_SYNC_TYPE_USER_FENCE;
            sync[0].flags = DRM_XE_SYNC_FLAG_SIGNAL;
            sync[0].addr = completionGpuAddress;
            sync[0].timeline_value = counterValue;
            struct drm_xe_exec exec = {};

            exec.exec_queue_id = engine;
            exec.num_syncs = 1;
            exec.syncs = reinterpret_cast<uintptr_t>(&sync);
            exec.address = execObject->gpuAddress + execBufferXe->startOffset;
            exec.num_batch_buffer = 1;

            ret = IoctlHelper::ioctl(DrmIoctl::gemExecbuffer2, &exec);
            xeLog("r=0x%x batch=0x%lx\n", ret, exec.address);

            if (debugManager.flags.PrintCompletionFenceUsage.get()) {
                std::cout << "Completion fence submitted."
                          << " GPU address: " << std::hex << completionGpuAddress << std::dec
                          << ", value: " << counterValue << std::endl;
            }
        }
    }
    return ret;
}

bool IoctlHelperXe::completionFenceExtensionSupported(const bool isVmBindAvailable) {
    xeLog(" -> IoctlHelperXe::%s\n", __FUNCTION__);
    return isVmBindAvailable;
}

uint64_t IoctlHelperXe::getFlagsForVmBind(bool bindCapture, bool bindImmediate, bool bindMakeResident, bool bindLock, bool readOnlyResource) {
    uint64_t flags = 0;
    xeLog(" -> IoctlHelperXe::%s %d %d %d %d %d\n", __FUNCTION__, bindCapture, bindImmediate, bindMakeResident, bindLock, readOnlyResource);
    if (bindCapture) {
        flags |= DRM_XE_VM_BIND_FLAG_DUMPABLE;
    }
    if (bindImmediate) {
        flags |= DRM_XE_VM_BIND_FLAG_IMMEDIATE;
    }

    if (readOnlyResource) {
        flags |= DRM_XE_VM_BIND_FLAG_READONLY;
    }
    if (bindMakeResident) {
        flags |= DRM_XE_VM_BIND_FLAG_IMMEDIATE;
    }
    return flags;
}

int IoctlHelperXe::queryDistances(std::vector<QueryItem> &queryItems, std::vector<DistanceInfo> &distanceInfos) {
    xeLog(" -> IoctlHelperXe::%s\n", __FUNCTION__);
    return 0;
}

bool IoctlHelperXe::isPageFaultSupported() {
    auto checkVmCreateFlagsSupport = [&](uint32_t flags) -> bool {
        struct drm_xe_vm_create vmCreate = {};
        vmCreate.flags = flags;

        auto ret = IoctlHelper::ioctl(DrmIoctl::gemVmCreate, &vmCreate);
        if (ret == 0) {
            struct drm_xe_vm_destroy vmDestroy = {};
            vmDestroy.vm_id = vmCreate.vm_id;
            ret = IoctlHelper::ioctl(DrmIoctl::gemVmDestroy, &vmDestroy);
            DEBUG_BREAK_IF(ret != 0);
            return true;
        }
        return false;
    };
    bool pageFaultSupport = checkVmCreateFlagsSupport(DRM_XE_VM_CREATE_FLAG_LR_MODE | DRM_XE_VM_CREATE_FLAG_FAULT_MODE);

    xeLog(" -> IoctlHelperXe::%s %d\n", __FUNCTION__, pageFaultSupport);

    return pageFaultSupport;
}

uint32_t IoctlHelperXe::getEuStallFdParameter() {
    xeLog(" -> IoctlHelperXe::%s\n", __FUNCTION__);
    return 0u;
}

std::unique_ptr<uint8_t[]> IoctlHelperXe::createVmControlExtRegion(const std::optional<MemoryClassInstance> &regionInstanceClass) {
    xeLog(" -> IoctlHelperXe::%s\n", __FUNCTION__);
    return {};
}

uint32_t IoctlHelperXe::getFlagsForVmCreate(bool disableScratch, bool enablePageFault, bool useVmBind) {
    xeLog(" -> IoctlHelperXe::%s %d,%d,%d\n", __FUNCTION__, disableScratch, enablePageFault, useVmBind);
    uint32_t flags = DRM_XE_VM_CREATE_FLAG_LR_MODE;
    bool debuggingEnabled = drm.getRootDeviceEnvironment().executionEnvironment.isDebuggingEnabled();
    if (enablePageFault || debuggingEnabled) {
        flags |= DRM_XE_VM_CREATE_FLAG_FAULT_MODE;
    }
    if (!disableScratch) {
        flags |= DRM_XE_VM_CREATE_FLAG_SCRATCH_PAGE;
    }
    return flags;
}

uint32_t IoctlHelperXe::createContextWithAccessCounters(GemContextCreateExt &gcc) {
    xeLog(" -> IoctlHelperXe::%s\n", __FUNCTION__);
    return 0;
}

uint32_t IoctlHelperXe::createCooperativeContext(GemContextCreateExt &gcc) {
    xeLog(" -> IoctlHelperXe::%s\n", __FUNCTION__);
    return 0;
}

void IoctlHelperXe::fillVmBindExtSetPat(VmBindExtSetPatT &vmBindExtSetPat, uint64_t patIndex, uint64_t nextExtension) {
    xeLog(" -> IoctlHelperXe::%s\n", __FUNCTION__);
}

void IoctlHelperXe::fillVmBindExtUserFence(VmBindExtUserFenceT &vmBindExtUserFence, uint64_t fenceAddress, uint64_t fenceValue, uint64_t nextExtension) {
    xeLog(" -> IoctlHelperXe::%s 0x%lx 0x%lx\n", __FUNCTION__, fenceAddress, fenceValue);
    auto xeBindExtUserFence = reinterpret_cast<UserFenceExtension *>(vmBindExtUserFence);
    UNRECOVERABLE_IF(!xeBindExtUserFence);
    xeBindExtUserFence->tag = UserFenceExtension::tagValue;
    xeBindExtUserFence->addr = fenceAddress;
    xeBindExtUserFence->value = fenceValue;
}

void IoctlHelperXe::setVmBindUserFence(VmBindParams &vmBind, VmBindExtUserFenceT vmBindUserFence) {
    xeLog(" -> IoctlHelperXe::%s\n", __FUNCTION__);
    vmBind.userFence = castToUint64(vmBindUserFence);
    return;
}

std::optional<uint32_t> IoctlHelperXe::getVmAdviseAtomicAttribute() {
    xeLog(" -> IoctlHelperXe::%s\n", __FUNCTION__);
    // There is no vmAdvise attribute in Xe
    return {};
}

int IoctlHelperXe::vmBind(const VmBindParams &vmBindParams) {
    return xeVmBind(vmBindParams, true);
}

int IoctlHelperXe::vmUnbind(const VmBindParams &vmBindParams) {
    return xeVmBind(vmBindParams, false);
}

int IoctlHelperXe::getResetStats(ResetStats &resetStats, uint32_t *status, ResetStatsFault *resetStatsFault) {
    return ioctl(DrmIoctl::getResetStats, &resetStats);
}

UuidRegisterResult IoctlHelperXe::registerUuid(const std::string &uuid, uint32_t uuidClass, uint64_t ptr, uint64_t size) {
    xeLog(" -> IoctlHelperXe::%s\n", __FUNCTION__);
    return {};
}

UuidRegisterResult IoctlHelperXe::registerStringClassUuid(const std::string &uuid, uint64_t ptr, uint64_t size) {
    xeLog(" -> IoctlHelperXe::%s\n", __FUNCTION__);
    return {};
}

int IoctlHelperXe::unregisterUuid(uint32_t handle) {
    xeLog(" -> IoctlHelperXe::%s\n", __FUNCTION__);
    return 0;
}

bool IoctlHelperXe::isContextDebugSupported() {
    return false;
}

int IoctlHelperXe::setContextDebugFlag(uint32_t drmContextId) {
    return 0;
}

bool IoctlHelperXe::isDebugAttachAvailable() {
    return true;
}

int IoctlHelperXe::getDrmParamValue(DrmParam drmParam) const {
    xeLog(" -> IoctlHelperXe::%s 0x%x %s\n", __FUNCTION__, drmParam, getDrmParamString(drmParam).c_str());
    switch (drmParam) {
    case DrmParam::atomicClassUndefined:
        return -1;
    case DrmParam::atomicClassDevice:
        return -1;
    case DrmParam::atomicClassGlobal:
        return -1;
    case DrmParam::atomicClassSystem:
        return -1;
    case DrmParam::memoryClassDevice:
        return DRM_XE_MEM_REGION_CLASS_VRAM;
    case DrmParam::memoryClassSystem:
        return DRM_XE_MEM_REGION_CLASS_SYSMEM;
    case DrmParam::engineClassRender:
        return DRM_XE_ENGINE_CLASS_RENDER;
    case DrmParam::engineClassCopy:
        return DRM_XE_ENGINE_CLASS_COPY;
    case DrmParam::engineClassVideo:
        return DRM_XE_ENGINE_CLASS_VIDEO_DECODE;
    case DrmParam::engineClassVideoEnhance:
        return DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE;
    case DrmParam::engineClassCompute:
        return DRM_XE_ENGINE_CLASS_COMPUTE;
    case DrmParam::engineClassInvalid:
        return -1;
    case DrmParam::execDefault:
        return DRM_XE_ENGINE_CLASS_COMPUTE;
    case DrmParam::execBlt:
        return DRM_XE_ENGINE_CLASS_COPY;
    case DrmParam::execRender:
        return DRM_XE_ENGINE_CLASS_RENDER;

    default:
        return getDrmParamValueBase(drmParam);
    }
}

int IoctlHelperXe::getDrmParamValueBase(DrmParam drmParam) const {
    return static_cast<int>(drmParam);
}

int IoctlHelperXe::ioctl(DrmIoctl request, void *arg) {
    int ret = -1;
    xeLog(" => IoctlHelperXe::%s 0x%x\n", __FUNCTION__, request);
    switch (request) {
    case DrmIoctl::getparam: {
        auto getParam = reinterpret_cast<GetParam *>(arg);
        ret = 0;
        switch (getParam->param) {
        case static_cast<int>(DrmParam::paramCsTimestampFrequency): {
            *getParam->value = xeGtListData->gt_list[defaultEngine->gt_id].reference_clock;
        } break;
        default:
            ret = -1;
        }
        xeLog(" -> IoctlHelperXe::ioctl Getparam 0x%x/0x%x r=%d\n", getParam->param, *getParam->value, ret);
    } break;

    case DrmIoctl::query: {

        Query *query = static_cast<Query *>(arg);

        QueryItem *queryItems = reinterpret_cast<QueryItem *>(query->itemsPtr);
        for (auto i = 0u; i < query->numItems; i++) {
            auto &queryItem = queryItems[i];

            if (queryItem.queryId != static_cast<int>(DrmParam::queryHwconfigTable)) {
                xeLog("error: bad query 0x%x\n", queryItem.queryId);
                return -1;
            }
            auto queryDataSize = static_cast<int32_t>(hwconfig.size() * sizeof(uint32_t));
            if (queryItem.length == 0) {
                queryItem.length = queryDataSize;
            } else {
                UNRECOVERABLE_IF(queryItem.length != queryDataSize);
                memcpy_s(reinterpret_cast<void *>(queryItem.dataPtr),
                         queryItem.length, hwconfig.data(), queryItem.length);
            }
            xeLog(" -> IoctlHelperXe::ioctl Query id=0x%x f=0x%x len=%d\n",
                  static_cast<int>(queryItem.queryId), static_cast<int>(queryItem.flags), queryItem.length);
            ret = 0;
        }
    } break;
    case DrmIoctl::gemUserptr: {
        GemUserPtr *d = static_cast<GemUserPtr *>(arg);
        updateBindInfo(d->userPtr);
        ret = 0;
        xeLog(" -> IoctlHelperXe::ioctl GemUserptr p=0x%llx s=0x%llx f=0x%x h=0x%x r=%d\n", d->userPtr,
              d->userSize, d->flags, d->handle, ret);
        xeShowBindTable();
    } break;
    case DrmIoctl::gemContextDestroy: {
        GemContextDestroy *d = static_cast<GemContextDestroy *>(arg);
        struct drm_xe_exec_queue_destroy destroy = {};
        destroy.exec_queue_id = d->contextId;
        ret = IoctlHelper::ioctl(request, &destroy);
        xeLog(" -> IoctlHelperXe::ioctl GemContextDestrory ctx=0x%x r=%d\n",
              d->contextId, ret);
    } break;
    case DrmIoctl::gemContextGetparam: {
        GemContextParam *d = static_cast<GemContextParam *>(arg);

        auto addressSpace = drm.getRootDeviceEnvironment().getHardwareInfo()->capabilityTable.gpuAddressSpace;
        ret = 0;
        switch (d->param) {
        case static_cast<int>(DrmParam::contextParamGttSize):
            d->value = addressSpace + 1u;
            break;
        default:
            ret = -1;
            break;
        }
        xeLog(" -> IoctlHelperXe::ioctl GemContextGetparam r=%d\n", ret);
    } break;
    case DrmIoctl::gemContextSetparam: {
        GemContextParam *gemContextParam = static_cast<GemContextParam *>(arg);
        switch (gemContextParam->param) {
        case static_cast<int>(DrmParam::contextParamEngines): {
            auto contextEngine = reinterpret_cast<ContextParamEngines<> *>(gemContextParam->value);
            if (!contextEngine || contextEngine->numEnginesInContext == 0) {
                break;
            }
            auto numEngines = contextEngine->numEnginesInContext;
            contextParamEngine.resize(numEngines);
            memcpy_s(contextParamEngine.data(), numEngines * sizeof(uint64_t), contextEngine->enginesData, numEngines * sizeof(uint64_t));
            ret = 0;
        } break;
        default:
            ret = -1;
            break;
        }
        xeLog(" -> IoctlHelperXe::ioctl GemContextSetparam r=%d\n", ret);
    } break;
    case DrmIoctl::gemClose: {
        std::unique_lock<std::mutex> lock(gemCloseLock);
        struct GemClose *d = static_cast<struct GemClose *>(arg);
        xeShowBindTable();
        bool isUserptr = false;
        if (d->userptr) {
            std::unique_lock<std::mutex> lock(xeLock);
            for (unsigned int i = 0; i < bindInfo.size(); i++) {
                if (d->userptr == bindInfo[i].userptr) {
                    isUserptr = true;
                    xeLog(" removing 0x%x 0x%lx\n",
                          bindInfo[i].userptr,
                          bindInfo[i].addr);
                    bindInfo.erase(bindInfo.begin() + i);
                    ret = 0;
                    break;
                }
            }
        }
        if (!isUserptr) {
            ret = IoctlHelper::ioctl(request, arg);
        }
        xeLog(" -> IoctlHelperXe::ioctl GemClose h=0x%x r=%d\n", d->handle, ret);
    } break;
    case DrmIoctl::gemVmCreate: {
        GemVmControl *vmControl = static_cast<GemVmControl *>(arg);
        struct drm_xe_vm_create args = {};
        args.flags = vmControl->flags;
        ret = IoctlHelper::ioctl(request, &args);
        vmControl->vmId = args.vm_id;
        xeLog(" -> IoctlHelperXe::ioctl gemVmCreate f=0x%x vmid=0x%x r=%d\n", vmControl->flags, vmControl->vmId, ret);

    } break;
    case DrmIoctl::gemVmDestroy: {
        GemVmControl *d = static_cast<GemVmControl *>(arg);
        struct drm_xe_vm_destroy args = {};
        args.vm_id = d->vmId;
        ret = IoctlHelper::ioctl(request, &args);
        xeLog(" -> IoctlHelperXe::ioctl GemVmDestroy vmid=0x%x r=%d\n", d->vmId, ret);

    } break;

    case DrmIoctl::gemMmapOffset: {
        GemMmapOffset *d = static_cast<GemMmapOffset *>(arg);
        struct drm_xe_gem_mmap_offset mmo = {};
        mmo.handle = d->handle;
        ret = IoctlHelper::ioctl(request, &mmo);
        d->offset = mmo.offset;
        xeLog(" -> IoctlHelperXe::ioctl GemMmapOffset h=0x%x o=0x%x f=0x%x r=%d\n",
              d->handle, d->offset, d->flags, ret);
    } break;
    case DrmIoctl::getResetStats: {
        ResetStats *resetStats = static_cast<ResetStats *>(arg);
        drm_xe_exec_queue_get_property getProperty{};
        getProperty.exec_queue_id = resetStats->contextId;
        getProperty.property = DRM_XE_EXEC_QUEUE_GET_PROPERTY_BAN;
        ret = IoctlHelper::ioctl(request, &getProperty);
        resetStats->batchPending = static_cast<uint32_t>(getProperty.value);
        xeLog(" -> IoctlHelperXe::ioctl GetResetStats ctx=0x%x r=%d value=%llu\n",
              resetStats->contextId, ret, getProperty.value);
    } break;
    case DrmIoctl::primeFdToHandle: {
        PrimeHandle *prime = static_cast<PrimeHandle *>(arg);
        ret = IoctlHelper::ioctl(request, arg);
        xeLog(" ->PrimeFdToHandle h=0x%x f=0x%x d=0x%x r=%d\n",
              prime->handle, prime->flags, prime->fileDescriptor, ret);
    } break;
    case DrmIoctl::primeHandleToFd: {
        PrimeHandle *prime = static_cast<PrimeHandle *>(arg);
        ret = IoctlHelper::ioctl(request, arg);
        xeLog(" ->PrimeHandleToFd h=0x%x f=0x%x d=0x%x r=%d\n",
              prime->handle, prime->flags, prime->fileDescriptor, ret);
    } break;
    case DrmIoctl::syncObjFdToHandle: {
        ret = IoctlHelper::ioctl(request, arg);
        xeLog(" -> IoctlHelperXe::ioctl SyncObjFdToHandle r=%d\n", ret);
    } break;
    case DrmIoctl::syncObjTimelineWait: {
        ret = IoctlHelper::ioctl(request, arg);
        xeLog(" -> IoctlHelperXe::ioctl SyncObjTimelineWait r=%d\n", ret);
    } break;
    case DrmIoctl::syncObjWait: {
        ret = IoctlHelper::ioctl(request, arg);
        xeLog(" -> IoctlHelperXe::ioctl SyncObjWait r=%d\n", ret);
    } break;
    case DrmIoctl::syncObjSignal: {
        ret = IoctlHelper::ioctl(request, arg);
        xeLog(" -> IoctlHelperXe::ioctl SyncObjSignal r=%d\n", ret);
    } break;
    case DrmIoctl::syncObjTimelineSignal: {
        ret = IoctlHelper::ioctl(request, arg);
        xeLog(" -> IoctlHelperXe::ioctl SyncObjTimelineSignal r=%d\n", ret);
    } break;
    case DrmIoctl::gemCreate: {
        drm_xe_gem_create *gemCreate = static_cast<drm_xe_gem_create *>(arg);
        ret = IoctlHelper::ioctl(request, arg);
        xeLog(" -> IoctlHelperXe::ioctl GemCreate h=0x%x s=0x%lx p=0x%x f=0x%x vmid=0x%x r=%d\n",
              gemCreate->handle, gemCreate->size, gemCreate->placement, gemCreate->flags, gemCreate->vm_id, ret);
    } break;
    case DrmIoctl::debuggerOpen: {
        ret = debuggerOpenIoctl(request, arg);
    } break;
    case DrmIoctl::metadataCreate: {
        ret = debuggerMetadataCreateIoctl(request, arg);
    } break;
    case DrmIoctl::metadataDestroy: {
        ret = debuggerMetadataDestroyIoctl(request, arg);
    } break;
    case DrmIoctl::perfQuery:
    case DrmIoctl::perfOpen: {
        ret = perfOpenIoctl(request, arg);
    } break;

    default:
        xeLog("Not handled 0x%x\n", request);
        UNRECOVERABLE_IF(true);
    }

    return ret;
}

void IoctlHelperXe::xeShowBindTable() {
    if (debugManager.flags.PrintXeLogs.get()) {
        std::unique_lock<std::mutex> lock(xeLock);
        xeLog("show bind: (<index> <userptr> <addr>)\n", "");
        for (unsigned int i = 0; i < bindInfo.size(); i++) {
            xeLog(" %3d x%016lx x%016lx\n", i,
                  bindInfo[i].userptr,
                  bindInfo[i].addr);
        }
    }
}

int IoctlHelperXe::createDrmContext(Drm &drm, OsContextLinux &osContext, uint32_t drmVmId, uint32_t deviceIndex, bool allocateInterrupt) {
    uint32_t drmContextId = 0;

    xeLog("createDrmContext VM=0x%x\n", drmVmId);
    drm.bindDrmContext(drmContextId, deviceIndex, osContext.getEngineType());

    UNRECOVERABLE_IF(contextParamEngine.empty());

    std::array<drm_xe_ext_set_property, maxContextSetProperties> extProperties{};
    uint32_t extPropertyIndex{0U};
    setOptionalContextProperties(drm, &extProperties, extPropertyIndex);
    setContextProperties(osContext, deviceIndex, &extProperties, extPropertyIndex);

    drm_xe_exec_queue_create create{};
    create.width = 1;
    create.num_placements = contextParamEngine.size();
    create.vm_id = drmVmId;
    create.instances = castToUint64(contextParamEngine.data());
    create.extensions = (extPropertyIndex > 0U ? castToUint64(extProperties.data()) : 0UL);
    applyContextFlags(&create, allocateInterrupt);

    int ret = IoctlHelper::ioctl(DrmIoctl::gemContextCreateExt, &create);
    drmContextId = create.exec_queue_id;

    xeLog("%s:%d (%d) vmid=0x%x ctx=0x%x r=0x%x\n", xeGetClassName(contextParamEngine[0].engine_class),
          contextParamEngine[0].engine_instance, create.num_placements, drmVmId, drmContextId, ret);
    if (ret != 0) {
        UNRECOVERABLE_IF(true);
    }
    return drmContextId;
}

int IoctlHelperXe::xeVmBind(const VmBindParams &vmBindParams, bool isBind) {
    auto gmmHelper = drm.getRootDeviceEnvironment().getGmmHelper();
    int ret = -1;
    const char *operation = isBind ? "bind" : "unbind";

    uint64_t userptr = 0u;
    {
        std::unique_lock<std::mutex> lock(xeLock);
        if (isBind) {
            if (vmBindParams.userptr) {
                for (auto i = 0u; i < bindInfo.size(); i++) {
                    if (vmBindParams.userptr == bindInfo[i].userptr) {
                        userptr = bindInfo[i].userptr;
                        bindInfo[i].addr = gmmHelper->decanonize(vmBindParams.start);
                        break;
                    }
                }
            }
        } else // unbind
        {
            auto address = gmmHelper->decanonize(vmBindParams.start);
            for (auto i = 0u; i < bindInfo.size(); i++) {
                if (address == bindInfo[i].addr) {
                    userptr = bindInfo[i].userptr;
                    break;
                }
            }
        }
    }

    drm_xe_vm_bind bind = {};
    bind.vm_id = vmBindParams.vmId;

    bind.num_binds = 1;

    bind.bind.range = vmBindParams.length;
    bind.bind.obj_offset = vmBindParams.offset;
    bind.bind.pat_index = static_cast<uint16_t>(vmBindParams.patIndex);
    bind.bind.extensions = vmBindParams.extensions;
    bind.bind.flags = static_cast<uint32_t>(vmBindParams.flags);

    drm_xe_sync sync[1] = {};
    if (vmBindParams.sharedSystemUsmBind == true) {
        bind.bind.addr = 0;
    } else {
        bind.bind.addr = gmmHelper->decanonize(vmBindParams.start);
    }
    bind.num_syncs = 1;
    UNRECOVERABLE_IF(vmBindParams.userFence == 0x0);
    auto xeBindExtUserFence = reinterpret_cast<UserFenceExtension *>(vmBindParams.userFence);
    UNRECOVERABLE_IF(xeBindExtUserFence->tag != UserFenceExtension::tagValue);
    sync[0].type = DRM_XE_SYNC_TYPE_USER_FENCE;
    sync[0].flags = DRM_XE_SYNC_FLAG_SIGNAL;
    sync[0].addr = xeBindExtUserFence->addr;
    sync[0].timeline_value = xeBindExtUserFence->value;
    bind.syncs = reinterpret_cast<uintptr_t>(&sync);

    if (isBind) {
        bind.bind.op = DRM_XE_VM_BIND_OP_MAP;
        bind.bind.obj = vmBindParams.handle;
        if (userptr) {
            bind.bind.op = DRM_XE_VM_BIND_OP_MAP_USERPTR;
            bind.bind.obj = 0;
            bind.bind.obj_offset = userptr;
        }
    } else {
        if (vmBindParams.sharedSystemUsmEnabled) {
            // Use of MAP on unbind required for restoring the address space to the system allocator
            bind.bind.op = DRM_XE_VM_BIND_OP_MAP;
            bind.bind.flags |= DRM_XE_VM_BIND_FLAG_CPU_ADDR_MIRROR;
        } else {
            bind.bind.op = DRM_XE_VM_BIND_OP_UNMAP;
            if (userptr) {
                bind.bind.obj_offset = userptr;
            }
        }
        bind.bind.obj = 0;
    }

    ret = IoctlHelper::ioctl(DrmIoctl::gemVmBind, &bind);

    xeLog(" vm=%d obj=0x%x off=0x%llx range=0x%llx addr=0x%llx operation=%d(%s) flags=%d(%s) nsy=%d pat=%hu ret=%d\n",
          bind.vm_id,
          bind.bind.obj,
          bind.bind.obj_offset,
          bind.bind.range,
          bind.bind.addr,
          bind.bind.op,
          xeGetBindOperationName(bind.bind.op),
          bind.bind.flags,
          xeGetBindFlagNames(bind.bind.flags).c_str(),
          bind.num_syncs,
          bind.bind.pat_index,
          ret);

    if (ret != 0) {
        xeLog("error: %s\n", operation);
        return ret;
    }

    constexpr auto oneSecTimeout = 1000000000ll;
    constexpr auto infiniteTimeout = -1;
    bool debuggingEnabled = drm.getRootDeviceEnvironment().executionEnvironment.isDebuggingEnabled();
    uint64_t timeout = debuggingEnabled ? infiniteTimeout : oneSecTimeout;
    if (debugManager.flags.VmBindWaitUserFenceTimeout.get() != -1) {
        timeout = debugManager.flags.VmBindWaitUserFenceTimeout.get();
    }
    return xeWaitUserFence(bind.exec_queue_id, DRM_XE_UFENCE_WAIT_OP_EQ,
                           sync[0].addr,
                           sync[0].timeline_value, timeout,
                           false, NEO::InterruptId::notUsed, nullptr);
}

std::string IoctlHelperXe::getDrmParamString(DrmParam drmParam) const {
    switch (drmParam) {
    case DrmParam::atomicClassUndefined:
        return "AtomicClassUndefined";
    case DrmParam::atomicClassDevice:
        return "AtomicClassDevice";
    case DrmParam::atomicClassGlobal:
        return "AtomicClassGlobal";
    case DrmParam::atomicClassSystem:
        return "AtomicClassSystem";
    case DrmParam::contextCreateExtSetparam:
        return "ContextCreateExtSetparam";
    case DrmParam::contextCreateFlagsUseExtensions:
        return "ContextCreateFlagsUseExtensions";
    case DrmParam::contextEnginesExtLoadBalance:
        return "ContextEnginesExtLoadBalance";
    case DrmParam::contextParamEngines:
        return "ContextParamEngines";
    case DrmParam::contextParamGttSize:
        return "ContextParamGttSize";
    case DrmParam::contextParamPersistence:
        return "ContextParamPersistence";
    case DrmParam::contextParamPriority:
        return "ContextParamPriority";
    case DrmParam::contextParamRecoverable:
        return "ContextParamRecoverable";
    case DrmParam::contextParamSseu:
        return "ContextParamSseu";
    case DrmParam::contextParamVm:
        return "ContextParamVm";
    case DrmParam::engineClassRender:
        return "EngineClassRender";
    case DrmParam::engineClassCompute:
        return "EngineClassCompute";
    case DrmParam::engineClassCopy:
        return "EngineClassCopy";
    case DrmParam::engineClassVideo:
        return "EngineClassVideo";
    case DrmParam::engineClassVideoEnhance:
        return "EngineClassVideoEnhance";
    case DrmParam::engineClassInvalid:
        return "EngineClassInvalid";
    case DrmParam::engineClassInvalidNone:
        return "EngineClassInvalidNone";
    case DrmParam::execBlt:
        return "ExecBlt";
    case DrmParam::execDefault:
        return "ExecDefault";
    case DrmParam::execNoReloc:
        return "ExecNoReloc";
    case DrmParam::execRender:
        return "ExecRender";
    case DrmParam::memoryClassDevice:
        return "MemoryClassDevice";
    case DrmParam::memoryClassSystem:
        return "MemoryClassSystem";
    case DrmParam::mmapOffsetWb:
        return "MmapOffsetWb";
    case DrmParam::mmapOffsetWc:
        return "MmapOffsetWc";
    case DrmParam::paramHasPooledEu:
        return "ParamHasPooledEu";
    case DrmParam::paramEuTotal:
        return "ParamEuTotal";
    case DrmParam::paramSubsliceTotal:
        return "ParamSubsliceTotal";
    case DrmParam::paramMinEuInPool:
        return "ParamMinEuInPool";
    case DrmParam::paramCsTimestampFrequency:
        return "ParamCsTimestampFrequency";
    case DrmParam::paramHasVmBind:
        return "ParamHasVmBind";
    case DrmParam::paramHasPageFault:
        return "ParamHasPageFault";
    case DrmParam::queryEngineInfo:
        return "QueryEngineInfo";
    case DrmParam::queryHwconfigTable:
        return "QueryHwconfigTable";
    case DrmParam::queryComputeSlices:
        return "QueryComputeSlices";
    case DrmParam::queryMemoryRegions:
        return "QueryMemoryRegions";
    case DrmParam::queryTopologyInfo:
        return "QueryTopologyInfo";
    case DrmParam::tilingNone:
        return "TilingNone";
    case DrmParam::tilingY:
        return "TilingY";
    default:
        return "DrmParam::<missing>";
    }
}

inline std::string getDirectoryWithFrequencyFiles(int tileId, int gtId) {
    return "/device/tile" + std::to_string(tileId) + "/gt" + std::to_string(gtId) + "/freq0";
}

std::string IoctlHelperXe::getFileForMaxGpuFrequency() const {
    return getFileForMaxGpuFrequencyOfSubDevice(0 /* tileId */);
}

std::string IoctlHelperXe::getFileForMaxGpuFrequencyOfSubDevice(int tileId) const {
    return getDirectoryWithFrequencyFiles(tileId, tileIdToGtId[tileId]) + "/max_freq";
}

std::string IoctlHelperXe::getFileForMaxMemoryFrequencyOfSubDevice(int tileId) const {
    return getDirectoryWithFrequencyFiles(tileId, tileIdToGtId[tileId]) + "/rp0_freq";
}

void IoctlHelperXe::configureCcsMode(std::vector<std::string> &files, const std::string expectedFilePrefix, uint32_t ccsMode,
                                     std::vector<std::tuple<std::string, uint32_t>> &deviceCcsModeVec) {

    // On Xe, path is /sys/class/drm/card0/device/tile*/gt*/ccs_mode
    for (const auto &file : files) {
        if (file.find(expectedFilePrefix.c_str()) == std::string::npos) {
            continue;
        }

        std::string tilePath = file + "/device/tile";
        auto tileFiles = Directory::getFiles(tilePath.c_str());
        for (const auto &tileFile : tileFiles) {
            std::string gtPath = tileFile + "/gt";
            auto gtFiles = Directory::getFiles(gtPath.c_str());
            for (const auto &gtFile : gtFiles) {
                writeCcsMode(gtFile, ccsMode, deviceCcsModeVec);
            }
        }
    }
}

bool IoctlHelperXe::getFabricLatency(uint32_t fabricId, uint32_t &latency, uint32_t &bandwidth) {
    return false;
}

bool IoctlHelperXe::isWaitBeforeBindRequired(bool bind) const {
    return true;
}

bool IoctlHelperXe::setGemTiling(void *setTiling) {
    return true;
}

bool IoctlHelperXe::getGemTiling(void *setTiling) {
    return true;
}

bool IoctlHelperXe::isImmediateVmBindRequired() const {
    return true;
}

bool IoctlHelperXe::makeResidentBeforeLockNeeded() const {
    auto makeResidentBeforeLockNeeded = false;
    if (debugManager.flags.EnableDeferBacking.get()) {
        makeResidentBeforeLockNeeded = true;
    }
    return makeResidentBeforeLockNeeded;
}

void IoctlHelperXe::insertEngineToContextParams(ContextParamEngines<> &contextParamEngines, uint32_t engineId, const EngineClassInstance *engineClassInstance, uint32_t tileId, bool hasVirtualEngines) {
    auto engines = reinterpret_cast<drm_xe_engine_class_instance *>(contextParamEngines.enginesData);
    if (engineClassInstance) {
        engines[engineId].engine_class = engineClassInstance->engineClass;
        engines[engineId].engine_instance = engineClassInstance->engineInstance;
        engines[engineId].gt_id = tileIdToGtId[tileId];
        contextParamEngines.numEnginesInContext = std::max(contextParamEngines.numEnginesInContext, engineId + 1);
    }
}

void IoctlHelperXe::registerBOBindHandle(Drm *drm, DrmAllocation *drmAllocation) {
    DrmResourceClass resourceClass = DrmResourceClass::maxSize;

    switch (drmAllocation->getAllocationType()) {
    case AllocationType::debugContextSaveArea:
        resourceClass = DrmResourceClass::contextSaveArea;
        break;
    case AllocationType::debugSbaTrackingBuffer:
        resourceClass = DrmResourceClass::sbaTrackingBuffer;
        break;
    case AllocationType::debugModuleArea:
        resourceClass = DrmResourceClass::moduleHeapDebugArea;
        break;
    case AllocationType::kernelIsa:
        if (drmAllocation->storageInfo.tileInstanced) {
            auto &bos = drmAllocation->getBOs();
            for (auto bo : bos) {
                if (!bo) {
                    continue;
                }
                bo->setRegisteredBindHandleCookie(drmAllocation->storageInfo.subDeviceBitfield.to_ulong());
            }
        }
        return;
    default:
        return;
    }

    uint64_t gpuAddress = drmAllocation->getGpuAddress();
    auto handle = drm->registerResource(resourceClass, &gpuAddress, sizeof(gpuAddress));
    drmAllocation->addRegisteredBoBindHandle(handle);
    auto &bos = drmAllocation->getBOs();
    for (auto bo : bos) {
        if (!bo) {
            continue;
        }
        bo->addBindExtHandle(handle);
        bo->markForCapture();
        bo->requireImmediateBinding(true);
    }
}

bool IoctlHelperXe::getFdFromVmExport(uint32_t vmId, uint32_t flags, int32_t *fd) {
    return false;
}

void IoctlHelperXe::setOptionalContextProperties(Drm &drm, void *extProperties, uint32_t &extIndexInOut) {

    auto &ext = *reinterpret_cast<std::array<drm_xe_ext_set_property, maxContextSetProperties> *>(extProperties);

    if ((contextParamEngine[0].engine_class == DRM_XE_ENGINE_CLASS_RENDER) || (contextParamEngine[0].engine_class == DRM_XE_ENGINE_CLASS_COMPUTE)) {
        if (drm.getRootDeviceEnvironment().executionEnvironment.isDebuggingEnabled()) {
            ext[extIndexInOut].base.next_extension = 0;
            ext[extIndexInOut].base.name = DRM_XE_EXEC_QUEUE_EXTENSION_SET_PROPERTY;
            ext[extIndexInOut].property = getEudebugExtProperty();
            ext[extIndexInOut].value = getEudebugExtPropertyValue();
            extIndexInOut++;
        }
    }
}

void IoctlHelperXe::setContextProperties(const OsContextLinux &osContext, uint32_t deviceIndex, void *extProperties, uint32_t &extIndexInOut) {

    auto &ext = *reinterpret_cast<std::array<drm_xe_ext_set_property, maxContextSetProperties> *>(extProperties);

    if (osContext.isLowPriority()) {
        ext[extIndexInOut].base.name = DRM_XE_EXEC_QUEUE_EXTENSION_SET_PROPERTY;
        ext[extIndexInOut].property = DRM_XE_EXEC_QUEUE_SET_PROPERTY_PRIORITY;
        ext[extIndexInOut].value = 0;
        if (extIndexInOut > 0) {
            ext[extIndexInOut - 1].base.next_extension = castToUint64(&ext[extIndexInOut]);
        }
        extIndexInOut++;
    }
}

unsigned int IoctlHelperXe::getIoctlRequestValue(DrmIoctl ioctlRequest) const {
    xeLog(" -> IoctlHelperXe::%s 0x%x\n", __FUNCTION__, ioctlRequest);
    switch (ioctlRequest) {
    case DrmIoctl::gemClose:
        RETURN_ME(DRM_IOCTL_GEM_CLOSE);
    case DrmIoctl::gemVmCreate:
        RETURN_ME(DRM_IOCTL_XE_VM_CREATE);
    case DrmIoctl::gemVmDestroy:
        RETURN_ME(DRM_IOCTL_XE_VM_DESTROY);
    case DrmIoctl::gemMmapOffset:
        RETURN_ME(DRM_IOCTL_XE_GEM_MMAP_OFFSET);
    case DrmIoctl::gemCreate:
        RETURN_ME(DRM_IOCTL_XE_GEM_CREATE);
    case DrmIoctl::gemExecbuffer2:
        RETURN_ME(DRM_IOCTL_XE_EXEC);
    case DrmIoctl::gemVmBind:
        RETURN_ME(DRM_IOCTL_XE_VM_BIND);
    case DrmIoctl::query:
        RETURN_ME(DRM_IOCTL_XE_DEVICE_QUERY);
    case DrmIoctl::gemContextCreateExt:
        RETURN_ME(DRM_IOCTL_XE_EXEC_QUEUE_CREATE);
    case DrmIoctl::gemContextDestroy:
        RETURN_ME(DRM_IOCTL_XE_EXEC_QUEUE_DESTROY);
    case DrmIoctl::gemWaitUserFence:
        RETURN_ME(DRM_IOCTL_XE_WAIT_USER_FENCE);
    case DrmIoctl::primeFdToHandle:
        RETURN_ME(DRM_IOCTL_PRIME_FD_TO_HANDLE);
    case DrmIoctl::primeHandleToFd:
        RETURN_ME(DRM_IOCTL_PRIME_HANDLE_TO_FD);
    case DrmIoctl::syncObjFdToHandle:
        RETURN_ME(DRM_IOCTL_SYNCOBJ_FD_TO_HANDLE);
    case DrmIoctl::syncObjWait:
        RETURN_ME(DRM_IOCTL_SYNCOBJ_WAIT);
    case DrmIoctl::syncObjSignal:
        RETURN_ME(DRM_IOCTL_SYNCOBJ_SIGNAL);
    case DrmIoctl::syncObjTimelineWait:
        RETURN_ME(DRM_IOCTL_SYNCOBJ_TIMELINE_WAIT);
    case DrmIoctl::syncObjTimelineSignal:
        RETURN_ME(DRM_IOCTL_SYNCOBJ_TIMELINE_SIGNAL);
    case DrmIoctl::getResetStats:
        RETURN_ME(DRM_IOCTL_XE_EXEC_QUEUE_GET_PROPERTY);
    case DrmIoctl::debuggerOpen:
    case DrmIoctl::metadataCreate:
    case DrmIoctl::metadataDestroy:
        return getIoctlRequestValueDebugger(ioctlRequest);
    case DrmIoctl::perfOpen:
    case DrmIoctl::perfEnable:
    case DrmIoctl::perfDisable:
    case DrmIoctl::perfQuery:
        return getIoctlRequestValuePerf(ioctlRequest);
    default:
        UNRECOVERABLE_IF(true);
        return 0;
    }
}

int IoctlHelperXe::ioctl(int fd, DrmIoctl request, void *arg) {
    return NEO::SysCalls::ioctl(fd, getIoctlRequestValue(request), arg);
}

std::string IoctlHelperXe::getIoctlString(DrmIoctl ioctlRequest) const {
    switch (ioctlRequest) {
    case DrmIoctl::gemClose:
        STRINGIFY_ME(DRM_IOCTL_GEM_CLOSE);
    case DrmIoctl::gemVmCreate:
        STRINGIFY_ME(DRM_IOCTL_XE_VM_CREATE);
    case DrmIoctl::gemVmDestroy:
        STRINGIFY_ME(DRM_IOCTL_XE_VM_DESTROY);
    case DrmIoctl::gemMmapOffset:
        STRINGIFY_ME(DRM_IOCTL_XE_GEM_MMAP_OFFSET);
    case DrmIoctl::gemCreate:
        STRINGIFY_ME(DRM_IOCTL_XE_GEM_CREATE);
    case DrmIoctl::gemExecbuffer2:
        STRINGIFY_ME(DRM_IOCTL_XE_EXEC);
    case DrmIoctl::gemVmBind:
        STRINGIFY_ME(DRM_IOCTL_XE_VM_BIND);
    case DrmIoctl::query:
        STRINGIFY_ME(DRM_IOCTL_XE_DEVICE_QUERY);
    case DrmIoctl::gemContextCreateExt:
        STRINGIFY_ME(DRM_IOCTL_XE_EXEC_QUEUE_CREATE);
    case DrmIoctl::gemContextDestroy:
        STRINGIFY_ME(DRM_IOCTL_XE_EXEC_QUEUE_DESTROY);
    case DrmIoctl::gemWaitUserFence:
        STRINGIFY_ME(DRM_IOCTL_XE_WAIT_USER_FENCE);
    case DrmIoctl::primeFdToHandle:
        STRINGIFY_ME(DRM_IOCTL_PRIME_FD_TO_HANDLE);
    case DrmIoctl::primeHandleToFd:
        STRINGIFY_ME(DRM_IOCTL_PRIME_HANDLE_TO_FD);
    case DrmIoctl::syncObjFdToHandle:
        STRINGIFY_ME(DRM_IOCTL_SYNCOBJ_FD_TO_HANDLE);
    case DrmIoctl::syncObjWait:
        STRINGIFY_ME(DRM_IOCTL_SYNCOBJ_WAIT);
    case DrmIoctl::syncObjSignal:
        STRINGIFY_ME(DRM_IOCTL_SYNCOBJ_SIGNAL);
    case DrmIoctl::syncObjTimelineWait:
        STRINGIFY_ME(DRM_IOCTL_SYNCOBJ_TIMELINE_WAIT);
    case DrmIoctl::syncObjTimelineSignal:
        STRINGIFY_ME(DRM_IOCTL_SYNCOBJ_TIMELINE_SIGNAL);
    case DrmIoctl::debuggerOpen:
        STRINGIFY_ME(DRM_IOCTL_XE_EUDEBUG_CONNECT);
    case DrmIoctl::metadataCreate:
        STRINGIFY_ME(DRM_IOCTL_XE_DEBUG_METADATA_CREATE);
    case DrmIoctl::metadataDestroy:
        STRINGIFY_ME(DRM_IOCTL_XE_DEBUG_METADATA_DESTROY);
    case DrmIoctl::getResetStats:
        STRINGIFY_ME(DRM_IOCTL_XE_EXEC_QUEUE_GET_PROPERTY);
    default:
        return "???";
    }
}

} // namespace NEO
