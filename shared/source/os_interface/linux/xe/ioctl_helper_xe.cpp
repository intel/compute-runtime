/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"

#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/debugger/debugger.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/engine_control.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/os_time.h"

#include "drm/xe_drm.h"

#include <algorithm>
#include <iostream>
#include <sstream>

#define STRINGIFY_ME(X) return #X
#define RETURN_ME(X) return X

#define XE_USERPTR_FAKE_FLAG 0x800000
#define XE_USERPTR_FAKE_MASK 0x7FFFFF

namespace NEO {

const char *IoctlHelperXe::xeGetClassName(int className) {
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

const char *IoctlHelperXe::xeGetBindFlagsName(int bindFlags) {
    switch (bindFlags) {
    case DRM_XE_VM_BIND_FLAG_READONLY:
        return "READ_ONLY";
    case DRM_XE_VM_BIND_FLAG_IMMEDIATE:
        return "IMMEDIATE";
    case DRM_XE_VM_BIND_FLAG_NULL:
        return "NULL";
    }
    return "Unknown flag";
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

bool IoctlHelperXe::initialize() {
    xeLog("IoctlHelperXe::initialize\n", "");

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

    chipsetId = config->info[DRM_XE_QUERY_CONFIG_REV_AND_DEVICE_ID] & 0xffff;
    revId = static_cast<int>((config->info[DRM_XE_QUERY_CONFIG_REV_AND_DEVICE_ID] >> 16) & 0xff);
    hasVram = config->info[DRM_XE_QUERY_CONFIG_FLAGS] & DRM_XE_QUERY_CONFIG_FLAG_HAS_VRAM ? 1 : 0;

    memset(&queryConfig, 0, sizeof(queryConfig));
    queryConfig.query = DRM_XE_DEVICE_QUERY_HWCONFIG;
    IoctlHelper::ioctl(DrmIoctl::query, &queryConfig);
    auto newSize = queryConfig.size / sizeof(uint32_t);
    hwconfig.resize(newSize);
    queryConfig.data = castToUint64(hwconfig.data());
    IoctlHelper::ioctl(DrmIoctl::query, &queryConfig);

    auto hwInfo = this->drm.getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->platform.usDeviceID = chipsetId;
    hwInfo->platform.usRevId = revId;

    return true;
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

std::unique_ptr<EngineInfo> IoctlHelperXe::createEngineInfo(bool isSysmanEnabled) {
    auto enginesData = queryData<uint64_t>(DRM_XE_DEVICE_QUERY_ENGINES);

    if (enginesData.empty()) {
        return {};
    }

    auto queryEngines = reinterpret_cast<struct drm_xe_query_engines *>(enginesData.data());

    auto numberHwEngines = queryEngines->num_engines;

    xeLog("numberHwEngines=%d\n", numberHwEngines);

    StackVec<std::vector<EngineClassInstance>, 2> enginesPerTile{};
    std::bitset<8> multiTileMask{};

    for (auto i = 0u; i < numberHwEngines; i++) {
        const auto &engine = queryEngines->engines[i].instance;
        auto tile = engine.gt_id;
        multiTileMask.set(tile);
        EngineClassInstance engineClassInstance{};
        engineClassInstance.engineClass = engine.engine_class;
        engineClassInstance.engineInstance = engine.engine_instance;
        xeLog("\t%s:%d:%d\n", xeGetClassName(engineClassInstance.engineClass), engineClassInstance.engineInstance, engine.gt_id);

        if (engineClassInstance.engineClass == getDrmParamValue(DrmParam::engineClassCompute) ||
            engineClassInstance.engineClass == getDrmParamValue(DrmParam::engineClassRender) ||
            engineClassInstance.engineClass == getDrmParamValue(DrmParam::engineClassCopy) ||
            (isSysmanEnabled && (engineClassInstance.engineClass == getDrmParamValue(DrmParam::engineClassVideo) ||
                                 engineClassInstance.engineClass == getDrmParamValue(DrmParam::engineClassVideoEnhance)))) {

            if (enginesPerTile.size() <= tile) {
                enginesPerTile.resize(tile + 1);
            }
            enginesPerTile[tile].push_back(engineClassInstance);
            allEngines.push_back(engine);
        }
    }

    auto hwInfo = drm.getRootDeviceEnvironment().getMutableHardwareInfo();
    if (hwInfo->featureTable.flags.ftrMultiTileArch) {
        auto &multiTileArchInfo = hwInfo->gtSystemInfo.MultiTileArchInfo;
        multiTileArchInfo.IsValid = true;
        multiTileArchInfo.TileCount = multiTileMask.count();
        multiTileArchInfo.TileMask = static_cast<uint8_t>(multiTileMask.to_ulong());
    }

    setDefaultEngine(drm.getRootDeviceEnvironment().getHardwareInfo()->capabilityTable.defaultEngineType);

    return std::make_unique<EngineInfo>(&drm, enginesPerTile);
}

inline MemoryRegion createMemoryRegionFromXeMemRegion(const drm_xe_mem_region &xeMemRegion) {
    MemoryRegion memoryRegion{};
    memoryRegion.region.memoryInstance = xeMemRegion.instance;
    memoryRegion.region.memoryClass = xeMemRegion.mem_class;
    memoryRegion.probedSize = xeMemRegion.total_size;
    memoryRegion.unallocatedSize = xeMemRegion.total_size - xeMemRegion.used;
    return memoryRegion;
}

std::unique_ptr<MemoryInfo> IoctlHelperXe::createMemoryInfo() {
    auto memUsageData = queryData<uint64_t>(DRM_XE_DEVICE_QUERY_MEM_REGIONS);
    auto gtListData = queryData<uint64_t>(DRM_XE_DEVICE_QUERY_GT_LIST);

    if (memUsageData.empty() || gtListData.empty()) {
        return {};
    }

    MemoryInfo::RegionContainer regionsContainer{};
    auto xeMemRegionsData = reinterpret_cast<drm_xe_query_mem_regions *>(memUsageData.data());
    auto xeGtListData = reinterpret_cast<drm_xe_query_gt_list *>(gtListData.data());

    std::array<drm_xe_mem_region *, 64> memoryRegionInstances{};

    for (auto i = 0u; i < xeMemRegionsData->num_mem_regions; i++) {
        auto &region = xeMemRegionsData->mem_regions[i];
        memoryRegionInstances[region.instance] = &region;
        if (region.mem_class == DRM_XE_MEM_REGION_CLASS_SYSMEM) {
            regionsContainer.push_back(createMemoryRegionFromXeMemRegion(region));
        }
    }

    if (regionsContainer.empty()) {
        return {};
    }

    for (auto i = 0u; i < xeGtListData->num_gt; i++) {
        if (xeGtListData->gt_list[i].type != DRM_XE_QUERY_GT_TYPE_MEDIA) {
            uint64_t nearMemRegions = xeGtListData->gt_list[i].near_mem_regions;
            auto regionIndex = Math::log2(nearMemRegions);
            UNRECOVERABLE_IF(!memoryRegionInstances[regionIndex]);
            regionsContainer.push_back(createMemoryRegionFromXeMemRegion(*memoryRegionInstances[regionIndex]));
            xeTimestampFrequency = xeGtListData->gt_list[i].reference_clock;
        }
    }
    return std::make_unique<MemoryInfo>(regionsContainer, drm);
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

bool IoctlHelperXe::getTimestampFrequency(uint64_t &frequency) {
    frequency = xeTimestampFrequency;
    return frequency != 0;
}

void IoctlHelperXe::getTopologyData(size_t nTiles, std::vector<std::bitset<8>> *geomDss, std::vector<std::bitset<8>> *computeDss,
                                    std::vector<std::bitset<8>> *euDss, DrmQueryTopologyData &topologyData, bool &isComputeDssEmpty) {
    int subSliceCount = 0;
    int euPerDss = 0;

    for (auto tileId = 0u; tileId < nTiles; tileId++) {

        int subSliceCountPerTile = 0;

        for (auto byte = 0u; byte < computeDss[tileId].size(); byte++) {
            subSliceCountPerTile += computeDss[tileId][byte].count();
        }

        if (subSliceCountPerTile == 0) {
            isComputeDssEmpty = true;
            for (auto byte = 0u; byte < geomDss[tileId].size(); byte++) {
                subSliceCountPerTile += geomDss[tileId][byte].count();
            }
        }

        int euPerDssPerTile = 0;
        for (auto byte = 0u; byte < euDss[tileId].size(); byte++) {
            euPerDssPerTile += euDss[tileId][byte].count();
        }

        // pick smallest config
        subSliceCount = (subSliceCount == 0) ? subSliceCountPerTile : std::min(subSliceCount, subSliceCountPerTile);
        euPerDss = (euPerDss == 0) ? euPerDssPerTile : std::min(euPerDss, euPerDssPerTile);

        // pick max config
        topologyData.maxSubSliceCount = std::max(topologyData.maxSubSliceCount, subSliceCountPerTile);
        topologyData.maxEuPerSubSlice = std::max(topologyData.maxEuPerSubSlice, euPerDssPerTile);
    }

    topologyData.sliceCount = 1;
    topologyData.subSliceCount = subSliceCount;
    topologyData.euCount = subSliceCount * euPerDss;
    topologyData.maxSliceCount = 1;
}

void IoctlHelperXe::getTopologyMap(size_t nTiles, std::vector<std::bitset<8>> *dssInfo, TopologyMap &topologyMap) {
    for (auto tileId = 0u; tileId < nTiles; tileId++) {
        std::vector<int> sliceIndices;
        std::vector<int> subSliceIndices;

        sliceIndices.push_back(0);

        for (auto byte = 0u; byte < dssInfo[tileId].size(); byte++) {
            for (auto bit = 0u; bit < 8u; bit++) {
                if (dssInfo[tileId][byte].test(bit)) {
                    auto subSliceIndex = byte * 8 + bit;
                    subSliceIndices.push_back(subSliceIndex);
                }
            }
        }

        topologyMap[tileId].sliceIndices = std::move(sliceIndices);
        topologyMap[tileId].subsliceIndices = std::move(subSliceIndices);
    }
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
    StackVec<int, 2> gtIdToTile{-1};

    auto topologySize = queryGtTopology.size();
    auto dataPtr = queryGtTopology.data();

    auto gtsData = queryData<uint64_t>(DRM_XE_DEVICE_QUERY_GT_LIST);
    auto xeGtListData = reinterpret_cast<drm_xe_query_gt_list *>(gtsData.data());
    gtIdToTile.resize(xeGtListData->num_gt, -1);

    auto tileIndex = 0u;
    for (auto gt = 0u; gt < gtIdToTile.size(); gt++) {
        if (xeGtListData->gt_list[gt].type != DRM_XE_QUERY_GT_TYPE_MEDIA) {
            gtIdToTile[gt] = tileIndex++;
        }
    }

    geomDss.resize(tileIndex);
    computeDss.resize(tileIndex);
    euDss.resize(tileIndex);
    while (topologySize >= sizeof(drm_xe_query_topology_mask)) {
        drm_xe_query_topology_mask *topo = reinterpret_cast<drm_xe_query_topology_mask *>(dataPtr);
        UNRECOVERABLE_IF(topo == nullptr);

        uint32_t gtId = topo->gt_id;

        if (xeGtListData->gt_list[gtId].type != DRM_XE_QUERY_GT_TYPE_MEDIA) {
            switch (topo->type) {
            case DRM_XE_TOPO_DSS_GEOMETRY:
                fillMask(geomDss[gtIdToTile[gtId]], topo);
                break;
            case DRM_XE_TOPO_DSS_COMPUTE:
                fillMask(computeDss[gtIdToTile[gtId]], topo);
                break;
            case DRM_XE_TOPO_EU_PER_DSS:
                fillMask(euDss[gtIdToTile[gtId]], topo);
                break;
            default:
                xeLog("Unhandle GT Topo type: %d\n", topo->type);
                return false;
            }
        }

        uint32_t itemSize = sizeof(drm_xe_query_topology_mask) + topo->num_bytes;
        topologySize -= itemSize;
        dataPtr = ptrOffset(dataPtr, itemSize);
    }

    bool isComputeDssEmpty = false;
    getTopologyData(tileIndex, geomDss.begin(), computeDss.begin(), euDss.begin(), topologyData, isComputeDssEmpty);

    auto &dssInfo = isComputeDssEmpty ? geomDss : computeDss;
    getTopologyMap(tileIndex, dssInfo.begin(), topologyMap);

    return true;
}

void IoctlHelperXe::updateBindInfo(uint32_t handle, uint64_t userPtr, uint64_t size) {
    std::unique_lock<std::mutex> lock(xeLock);
    BindInfo b = {handle, userPtr, 0, size};
    bindInfo.push_back(b);
}

void IoctlHelperXe::setDefaultEngine(const aub_stream::EngineType &defaultEngineType) {
    uint32_t defaultEngineClass;

    if (defaultEngineType == aub_stream::EngineType::ENGINE_CCS) {
        defaultEngineClass = DRM_XE_ENGINE_CLASS_COMPUTE;
    } else if (defaultEngineType == aub_stream::EngineType::ENGINE_RCS) {
        defaultEngineClass = DRM_XE_ENGINE_CLASS_RENDER;
    } else {
        /* So far defaultEngineType is either ENGINE_RCS or ENGINE_CCS */
        UNRECOVERABLE_IF(true);
    }

    for (auto i = 0u; i < allEngines.size(); i++) {
        if (allEngines[i].engine_class == defaultEngineClass) {
            defaultEngine = &allEngines[i];
            xeLog("Found default engine of class %s\n", xeGetClassName(defaultEngineClass));
            break;
        }
    }

    if (defaultEngine == nullptr) {
        UNRECOVERABLE_IF(true);
    }
}

uint16_t IoctlHelperXe::getCpuCachingMode() {
    uint16_t cpuCachingMode = DRM_XE_GEM_CPU_CACHING_WC;
    if (debugManager.flags.OverrideCpuCaching.get() != -1) {
        cpuCachingMode = debugManager.flags.OverrideCpuCaching.get();
    }

    return cpuCachingMode;
}

int IoctlHelperXe::createGemExt(const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle, uint64_t patIndex, std::optional<uint32_t> vmId, int32_t pairHandle, bool isChunked, uint32_t numOfChunks, std::optional<uint32_t> memPolicyMode, std::optional<std::vector<unsigned long>> memPolicyNodemask) {
    struct drm_xe_gem_create create = {};
    uint32_t regionsSize = static_cast<uint32_t>(memClassInstances.size());

    if (!regionsSize) {
        xeLog("memClassInstances empty !\n", "");
        return -1;
    }

    if (vmId != std::nullopt) {
        create.vm_id = vmId.value();
    }

    create.size = allocSize;
    MemoryClassInstance mem = memClassInstances[regionsSize - 1];
    std::bitset<32> memoryInstances{};
    for (const auto &memoryClassInstance : memClassInstances) {
        memoryInstances.set(memoryClassInstance.memoryInstance);
    }
    create.placement = static_cast<uint32_t>(memoryInstances.to_ulong());
    create.cpu_caching = this->getCpuCachingMode();
    auto ret = IoctlHelper::ioctl(DrmIoctl::gemCreate, &create);
    handle = create.handle;

    xeLog(" -> IoctlHelperXe::%s [%d,%d] vmid=0x%x s=0x%lx f=0x%x p=0x%x h=0x%x c=%hu r=%d\n", __FUNCTION__,
          mem.memoryClass, mem.memoryInstance,
          create.vm_id, create.size, create.flags, create.placement, handle, create.cpu_caching, ret);
    updateBindInfo(create.handle, 0u, create.size);
    return ret;
}

uint32_t IoctlHelperXe::createGem(uint64_t size, uint32_t memoryBanks) {
    struct drm_xe_gem_create create = {};
    create.size = size;
    auto pHwInfo = drm.getRootDeviceEnvironment().getHardwareInfo();
    auto memoryInfo = drm.getMemoryInfo();
    std::bitset<32> memoryInstances{};
    auto banks = std::bitset<4>(memoryBanks);
    size_t currentBank = 0;
    size_t i = 0;
    while (i < banks.count()) {
        if (banks.test(currentBank)) {
            auto regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(1u << currentBank, *pHwInfo);
            memoryInstances.set(regionClassAndInstance.memoryInstance);
            i++;
        }
        currentBank++;
    }
    if (memoryBanks == 0) {
        auto regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(memoryBanks, *pHwInfo);
        memoryInstances.set(regionClassAndInstance.memoryInstance);
    }
    create.placement = static_cast<uint32_t>(memoryInstances.to_ulong());
    create.cpu_caching = this->getCpuCachingMode();
    [[maybe_unused]] auto ret = ioctl(DrmIoctl::gemCreate, &create);

    xeLog(" -> IoctlHelperXe::%s vmid=0x%x s=0x%lx f=0x%x p=0x%x h=0x%x c=%hu r=%d\n", __FUNCTION__,
          create.vm_id, create.size, create.flags, create.placement, create.handle, create.cpu_caching, ret);
    DEBUG_BREAK_IF(ret != 0);
    updateBindInfo(create.handle, 0u, create.size);
    return create.handle;
}

CacheRegion IoctlHelperXe::closAlloc() {
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

int IoctlHelperXe::xeWaitUserFence(uint32_t ctxId, uint64_t mask, uint16_t op, uint64_t addr, uint64_t value,
                                   int64_t timeout) {
    struct drm_xe_wait_user_fence wait = {};
    wait.addr = addr;
    wait.op = op;
    wait.value = value;
    wait.mask = mask;
    wait.timeout = timeout;
    wait.exec_queue_id = ctxId;
    auto retVal = IoctlHelper::ioctl(DrmIoctl::gemWaitUserFence, &wait);
    xeLog(" -> IoctlHelperXe::%s a=0x%llx v=0x%llx T=0x%llx F=0x%x ctx=0x%x retVal=0x%x\n", __FUNCTION__, addr, value,
          timeout, wait.flags, ctxId, retVal);
    return retVal;
}

int IoctlHelperXe::waitUserFence(uint32_t ctxId, uint64_t address,
                                 uint64_t value, uint32_t dataWidth, int64_t timeout, uint16_t flags) {
    xeLog(" -> IoctlHelperXe::%s a=0x%llx v=0x%llx w=0x%x T=0x%llx F=0x%x ctx=0x%x\n", __FUNCTION__, address, value, dataWidth, timeout, flags, ctxId);
    uint64_t mask;
    switch (dataWidth) {
    case static_cast<uint32_t>(Drm::ValueWidth::u64):
        mask = DRM_XE_UFENCE_WAIT_MASK_U64;
        break;
    case static_cast<uint32_t>(Drm::ValueWidth::u32):
        mask = DRM_XE_UFENCE_WAIT_MASK_U32;
        break;
    case static_cast<uint32_t>(Drm::ValueWidth::u16):
        mask = DRM_XE_UFENCE_WAIT_MASK_U16;
        break;
    default:
        mask = DRM_XE_UFENCE_WAIT_MASK_U8;
        break;
    }
    if (timeout == -1) {
        /* expected in i915 but not in xe where timeout is an unsigned long */
        timeout = TimeoutControls::maxTimeout;
    }
    if (address) {
        return xeWaitUserFence(ctxId, mask, DRM_XE_UFENCE_WAIT_OP_GTE, address, value, timeout);
    }
    return 0;
}

uint32_t IoctlHelperXe::getAtomicAdvise(bool isNonAtomic) {
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
    return false;
}

bool IoctlHelperXe::setVmBoAdviseForChunking(int32_t handle, uint64_t start, uint64_t length, uint32_t attribute, void *region) {
    return false;
}

bool IoctlHelperXe::setVmPrefetch(uint64_t start, uint64_t length, uint32_t region, uint32_t vmId) {
    xeLog(" -> IoctlHelperXe::%s\n", __FUNCTION__);
    return false;
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

uint64_t IoctlHelperXe::getFlagsForVmBind(bool bindCapture, bool bindImmediate, bool bindMakeResident) {
    uint64_t ret = 0;
    xeLog(" -> IoctlHelperXe::%s %d %d %d\n", __FUNCTION__, bindCapture, bindImmediate, bindMakeResident);
    if (bindCapture) {
        ret |= XE_NEO_BIND_CAPTURE_FLAG;
    }
    if (bindImmediate) {
        ret |= XE_NEO_BIND_IMMEDIATE_FLAG;
    }
    if (bindMakeResident) {
        ret |= XE_NEO_BIND_MAKERESIDENT_FLAG;
    }
    return ret;
}

int IoctlHelperXe::queryDistances(std::vector<QueryItem> &queryItems, std::vector<DistanceInfo> &distanceInfos) {
    xeLog(" -> IoctlHelperXe::%s\n", __FUNCTION__);
    return 0;
}

std::optional<DrmParam> IoctlHelperXe::getHasPageFaultParamId() {
    xeLog(" -> IoctlHelperXe::%s\n", __FUNCTION__);
    return {};
};

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
    uint32_t flags = 0u;
    if (disableScratch) {
        flags |= XE_NEO_VMCREATE_DISABLESCRATCH_FLAG;
    }
    if (enablePageFault) {
        flags |= XE_NEO_VMCREATE_ENABLEPAGEFAULT_FLAG;
    }
    if (useVmBind) {
        flags |= XE_NEO_VMCREATE_USEVMBIND_FLAG;
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

std::optional<uint64_t> IoctlHelperXe::getCopyClassSaturatePCIECapability() {
    xeLog(" -> IoctlHelperXe::%s\n", __FUNCTION__);
    return {};
}

std::optional<uint64_t> IoctlHelperXe::getCopyClassSaturateLinkCapability() {
    xeLog(" -> IoctlHelperXe::%s\n", __FUNCTION__);
    return {};
}

uint32_t IoctlHelperXe::getVmAdviseAtomicAttribute() {
    xeLog(" -> IoctlHelperXe::%s\n", __FUNCTION__);
    return 0;
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
        struct GetParam *d = (struct GetParam *)arg;
        ret = 0;
        switch (d->param) {
        case static_cast<int>(DrmParam::paramChipsetId):
            *d->value = chipsetId;
            break;
        case static_cast<int>(DrmParam::paramRevision):
            *d->value = revId;
            break;
        case static_cast<int>(DrmParam::paramHasPageFault):
            *d->value = 0;
            break;
        case static_cast<int>(DrmParam::paramHasExecSoftpin):
            *d->value = 1;
            break;
        case static_cast<int>(DrmParam::paramHasScheduler):
            *d->value = static_cast<int>(0x80000037);
            break;
        case static_cast<int>(DrmParam::paramCsTimestampFrequency): {
            uint64_t frequency = 0;
            if (getTimestampFrequency(frequency)) {
                *d->value = static_cast<int>(frequency);
            }
        } break;
        default:
            ret = -1;
        }
        xeLog(" -> IoctlHelperXe::ioctl Getparam 0x%x/0x%x r=%d\n", d->param, *d->value, ret);
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
        d->handle = userPtrHandle++ | XE_USERPTR_FAKE_FLAG;
        updateBindInfo(d->handle, d->userPtr, d->userSize);
        ret = 0;
        xeLog(" -> IoctlHelperXe::ioctl GemUserptr p=0x%llx s=0x%llx f=0x%x h=0x%x r=%d\n", d->userPtr,
              d->userSize, d->flags, d->handle, ret);
        xeShowBindTable();
    } break;
    case DrmIoctl::gemContextCreateExt: {
        UNRECOVERABLE_IF(true);
    } break;
    case DrmIoctl::gemContextDestroy: {
        GemContextDestroy *d = static_cast<GemContextDestroy *>(arg);
        struct drm_xe_exec_queue_destroy destroy = {};
        destroy.exec_queue_id = d->contextId;
        if (d->contextId != 0xffffffff)
            ret = IoctlHelper::ioctl(request, &destroy);
        else
            ret = 0;
        xeLog(" -> IoctlHelperXe::ioctl GemContextDestroryExt ctx=0x%x r=%d\n",
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
        case static_cast<int>(DrmParam::contextParamSseu):
            d->value = 0x55fdd94d4e40;
            break;
        case static_cast<int>(DrmParam::contextParamPersistence):
            d->value = 0x1;
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
        case static_cast<int>(DrmParam::contextParamPersistence):
            if (gemContextParam->value == 0)
                ret = 0;
            break;
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
        case contextPrivateParamBoost:
            ret = 0;
            break;
        default:
            ret = -1;
            break;
        }
        xeLog(" -> IoctlHelperXe::ioctl GemContextSetparam r=%d\n", ret);
    } break;
    case DrmIoctl::gemClose: {
        struct GemClose *d = static_cast<struct GemClose *>(arg);
        int found = -1;
        xeShowBindTable();
        for (unsigned int i = 0; i < bindInfo.size(); i++) {
            if (d->handle == bindInfo[i].handle) {
                found = i;
                break;
            }
        }
        if (found != -1) {
            xeLog(" removing %d: 0x%x 0x%lx 0x%lx\n",
                  found,
                  bindInfo[found].handle,
                  bindInfo[found].userptr,
                  bindInfo[found].addr);
            {
                std::unique_lock<std::mutex> lock(xeLock);
                bindInfo.erase(bindInfo.begin() + found);
            }
            if (d->handle & XE_USERPTR_FAKE_FLAG) {
                // nothing to do under XE
                ret = 0;
            } else {
                ret = IoctlHelper::ioctl(request, arg);
            }
        } else {
            ret = 0; // let it pass trough for now
        }
        xeLog(" -> IoctlHelperXe::ioctl GemClose found=%d h=0x%x r=%d\n", found, d->handle, ret);
    } break;
    case DrmIoctl::gemVmCreate: {
        GemVmControl *d = static_cast<GemVmControl *>(arg);
        struct drm_xe_vm_create args = {};
        args.flags = DRM_XE_VM_CREATE_FLAG_LR_MODE;
        if (drm.hasPageFaultSupport()) {
            args.flags |= DRM_XE_VM_CREATE_FLAG_FAULT_MODE;
        }

        if (drm.getRootDeviceEnvironment().executionEnvironment.getDebuggingMode() != DebuggingMode::disabled) {
            args.extensions = reinterpret_cast<unsigned long long>(allocateDebugMetadata());
        }
        ret = IoctlHelper::ioctl(request, &args);
        if (drm.getRootDeviceEnvironment().executionEnvironment.getDebuggingMode() != DebuggingMode::disabled) {
            args.extensions = reinterpret_cast<unsigned long long>(freeDebugMetadata(reinterpret_cast<void *>(args.extensions)));
        }

        d->vmId = ret ? 0 : args.vm_id;
        d->flags = ret ? 0 : args.flags;
        xeLog(" -> IoctlHelperXe::ioctl gemVmCreate f=0x%x vmid=0x%x r=%d\n", d->flags, d->vmId, ret);

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
        ResetStats *d = static_cast<ResetStats *>(arg);
        //    d->batchActive = 1; // fake gpu hang
        ret = 0;
        xeLog(" -> IoctlHelperXe::ioctl GetResetStats ctx=0x%x r=%d\n",
              d->contextId, ret);
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

    default:
        xeLog("Not handled 0x%x\n", request);
        UNRECOVERABLE_IF(true);
    }

    return ret;
}

void IoctlHelperXe::xeShowBindTable() {
    if (debugManager.flags.PrintXeLogs.get()) {
        std::unique_lock<std::mutex> lock(xeLock);
        xeLog("show bind: (<index> <handle> <userptr> <addr> <size>)\n", "");
        for (unsigned int i = 0; i < bindInfo.size(); i++) {
            xeLog(" %3d x%08x x%016lx x%016lx x%016lx\n", i,
                  bindInfo[i].handle,
                  bindInfo[i].userptr,
                  bindInfo[i].addr,
                  bindInfo[i].size);
        }
    }
}

int IoctlHelperXe::createDrmContext(Drm &drm, OsContextLinux &osContext, uint32_t drmVmId, uint32_t deviceIndex) {
    drm_xe_exec_queue_create create = {};
    uint32_t drmContextId = 0;

    xeLog("createDrmContext VM=0x%x\n", drmVmId);
    drm.bindDrmContext(drmContextId, deviceIndex, osContext.getEngineType(), osContext.isEngineInstanced());

    size_t n = contextParamEngine.size();
    UNRECOVERABLE_IF(n == 0);
    create.vm_id = drmVmId;
    create.width = 1;
    create.instances = castToUint64(contextParamEngine.data());
    create.num_placements = contextParamEngine.size();

    struct drm_xe_ext_set_property ext {};
    auto &gfxCoreHelper = drm.getRootDeviceEnvironment().getHelper<GfxCoreHelper>();
    if ((contextParamEngine[0].engine_class == DRM_XE_ENGINE_CLASS_RENDER) || (contextParamEngine[0].engine_class == DRM_XE_ENGINE_CLASS_COMPUTE)) {
        if (gfxCoreHelper.isRunaloneModeRequired(drm.getRootDeviceEnvironment().executionEnvironment.getDebuggingMode())) {
            ext.base.next_extension = 0;
            ext.base.name = DRM_XE_EXEC_QUEUE_EXTENSION_SET_PROPERTY;
            ext.property = getRunaloneExtProperty();
            ext.value = 1;

            create.extensions = castToUint64(&ext);
        }
    }

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
    constexpr int invalidIndex = -1;
    auto gmmHelper = drm.getRootDeviceEnvironment().getGmmHelper();
    int ret = -1;
    const char *operation = isBind ? "bind" : "unbind";
    int index = invalidIndex;

    if (isBind) {
        for (auto i = 0u; i < bindInfo.size(); i++) {
            if (vmBindParams.handle == bindInfo[i].handle) {
                index = i;
                break;
            }
        }
    } else // unbind
    {
        auto address = gmmHelper->decanonize(vmBindParams.start);
        for (auto i = 0u; i < bindInfo.size(); i++) {
            if (address == bindInfo[i].addr) {
                index = i;
                break;
            }
        }
    }

    if (index != invalidIndex) {

        drm_xe_sync sync[1] = {};
        sync[0].type = DRM_XE_SYNC_TYPE_USER_FENCE;
        sync[0].flags = DRM_XE_SYNC_FLAG_SIGNAL;
        auto xeBindExtUserFence = reinterpret_cast<UserFenceExtension *>(vmBindParams.userFence);
        UNRECOVERABLE_IF(!xeBindExtUserFence);
        UNRECOVERABLE_IF(xeBindExtUserFence->tag != UserFenceExtension::tagValue);
        sync[0].addr = xeBindExtUserFence->addr;
        sync[0].timeline_value = xeBindExtUserFence->value;

        drm_xe_vm_bind bind = {};
        bind.vm_id = vmBindParams.vmId;
        bind.num_binds = 1;
        bind.num_syncs = 1;
        bind.syncs = reinterpret_cast<uintptr_t>(&sync);
        bind.bind.range = vmBindParams.length;
        bind.bind.addr = gmmHelper->decanonize(vmBindParams.start);
        bind.bind.obj_offset = vmBindParams.offset;
        bind.bind.pat_index = static_cast<uint16_t>(vmBindParams.patIndex);
        bind.bind.extensions = vmBindParams.extensions;

        if (isBind) {
            bind.bind.op = DRM_XE_VM_BIND_OP_MAP;
            bind.bind.obj = vmBindParams.handle;
            if (bindInfo[index].handle & XE_USERPTR_FAKE_FLAG) {
                bind.bind.op = DRM_XE_VM_BIND_OP_MAP_USERPTR;
                bind.bind.obj = 0;
                bind.bind.obj_offset = bindInfo[index].userptr;
            }
        } else {
            bind.bind.op = DRM_XE_VM_BIND_OP_UNMAP;
            bind.bind.obj = 0;
            if (bindInfo[index].handle & XE_USERPTR_FAKE_FLAG) {
                bind.bind.obj_offset = bindInfo[index].userptr;
            }
        }

        bindInfo[index].addr = bind.bind.addr;

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
              xeGetBindFlagsName(bind.bind.flags),
              bind.num_syncs,
              bind.bind.pat_index,
              ret);

        if (ret != 0) {
            xeLog("error: %s\n", operation);
            return ret;
        }

        return xeWaitUserFence(bind.exec_queue_id, DRM_XE_UFENCE_WAIT_MASK_U64, DRM_XE_UFENCE_WAIT_OP_EQ,
                               sync[0].addr,
                               sync[0].timeline_value, XE_ONE_SEC);
    }

    xeLog("error:  -> IoctlHelperXe::%s %s index=%d vmid=0x%x h=0x%x s=0x%llx o=0x%llx l=0x%llx f=0x%llx pat=%hu r=%d\n",
          __FUNCTION__, operation, index, vmBindParams.vmId,
          vmBindParams.handle, vmBindParams.start, vmBindParams.offset,
          vmBindParams.length, vmBindParams.flags, vmBindParams.patIndex, ret);

    return ret;
}

std::string IoctlHelperXe::getDrmParamString(DrmParam drmParam) const {
    switch (drmParam) {
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
    case DrmParam::paramChipsetId:
        return "ParamChipsetId";
    case DrmParam::paramRevision:
        return "ParamRevision";
    case DrmParam::paramHasExecSoftpin:
        return "ParamHasExecSoftpin";
    case DrmParam::paramHasPooledEu:
        return "ParamHasPooledEu";
    case DrmParam::paramHasScheduler:
        return "ParamHasScheduler";
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
    case DrmParam::schedulerCapPreemption:
        return "SchedulerCapPreemption";
    case DrmParam::tilingNone:
        return "TilingNone";
    case DrmParam::tilingY:
        return "TilingY";
    default:
        return "DrmParam::<missing>";
    }
}

std::string IoctlHelperXe::getFileForMaxGpuFrequency() const {
    return "/device/gt0/freq_max";
}

std::string IoctlHelperXe::getFileForMaxGpuFrequencyOfSubDevice(int subDeviceId) const {
    return "/device/gt" + std::to_string(subDeviceId) + "/freq_max";
}

std::string IoctlHelperXe::getFileForMaxMemoryFrequencyOfSubDevice(int subDeviceId) const {
    return "/device/gt" + std::to_string(subDeviceId) + "/freq_rp0";
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

void IoctlHelperXe::fillBindInfoForIpcHandle(uint32_t handle, size_t size) {
    xeLog(" -> IoctlHelperXe::%s s=0x%lx h=0x%x\n", __FUNCTION__, size, handle);
    updateBindInfo(handle, 0, size);
}

bool IoctlHelperXe::isImmediateVmBindRequired() const {
    return true;
}

void IoctlHelperXe::insertEngineToContextParams(ContextParamEngines<> &contextParamEngines, uint32_t engineId, const EngineClassInstance *engineClassInstance, uint32_t tileId, bool hasVirtualEngines) {
    auto engines = reinterpret_cast<drm_xe_engine_class_instance *>(contextParamEngines.enginesData);
    if (engineClassInstance) {
        engines[engineId].engine_class = engineClassInstance->engineClass;
        engines[engineId].engine_instance = engineClassInstance->engineInstance;
        engines[engineId].gt_id = tileId;
        contextParamEngines.numEnginesInContext = std::max(contextParamEngines.numEnginesInContext, engineId + 1);
    }
}
} // namespace NEO
