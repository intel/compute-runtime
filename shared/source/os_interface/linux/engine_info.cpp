/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/engine_info.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/drm_wrappers.h"
#include "shared/source/os_interface/product_helper.h"

namespace NEO {
namespace DrmEngineMappingHelper {
constexpr aub_stream::EngineType engineMapping[] = {aub_stream::ENGINE_BCS, aub_stream::ENGINE_BCS1, aub_stream::ENGINE_BCS2,
                                                    aub_stream::ENGINE_BCS3, aub_stream::ENGINE_BCS4, aub_stream::ENGINE_BCS5,
                                                    aub_stream::ENGINE_BCS6, aub_stream::ENGINE_BCS7, aub_stream::ENGINE_BCS8};

// 3 types of copy engines:
// - Main - BCS (legacy, aka. BCS0)
// - Host (flavor of link copy engine) - BCS1-2
// - Scale-up (flavor of link copy engine) - BCS3-8
constexpr aub_stream::EngineType baseForMainCopyEngine = aub_stream::EngineType::ENGINE_BCS;
constexpr aub_stream::EngineType baseForHostLinkCopyEngine = aub_stream::EngineType::ENGINE_BCS1;
constexpr aub_stream::EngineType baseForScaleUpLinkCopyEngine = aub_stream::EngineType::ENGINE_BCS3;

} // namespace DrmEngineMappingHelper

namespace {
void assignLinkCopyEngine(std::vector<EngineInfo::EngineToInstanceMap> &tileToEngineToInstanceMap, aub_stream::EngineType baseEngineType, uint32_t tileId, const EngineClassInstance &engine,
                          BcsInfoMask &bcsInfoMask, uint32_t &engineCounter) {
    engineCounter++;

    auto engineIndex = (baseEngineType + engineCounter - 1);
    tileToEngineToInstanceMap[tileId][static_cast<aub_stream::EngineType>(engineIndex)] = engine;

    // Example: For BCS5 (3rd scale-up engine): BCS3 + 3 - BCS1 = 5
    size_t engineMaskIndex = (baseEngineType + engineCounter - aub_stream::EngineType::ENGINE_BCS1);
    UNRECOVERABLE_IF(bcsInfoMask.test(engineMaskIndex));
    bcsInfoMask.set(engineMaskIndex, true);
}

auto getCopyEnginesMappingIterator(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) {
    auto mappingCopyEngineIt = DrmEngineMappingHelper::engineMapping;
    if (const auto defaultCopyEngine = rootDeviceEnvironment.getProductHelper().getDefaultCopyEngine(); defaultCopyEngine != *mappingCopyEngineIt) {
        mappingCopyEngineIt++;
    } // Note that BCS0 may not be enabled
    return mappingCopyEngineIt;
}

uint32_t getBcsEngineMaskIndex(const aub_stream::EngineType *mappingCopyEngineIt) {
    if (*mappingCopyEngineIt == aub_stream::EngineType::ENGINE_BCS) {
        return 0u;
    } else
        return *mappingCopyEngineIt - aub_stream::EngineType::ENGINE_BCS1 + 1;
}
} // namespace

void EngineInfo::mapEngine(const NEO::IoctlHelper *ioctlHelper, const EngineClassInstance &engine, BcsInfoMask &bcsInfoMask, const NEO::RootDeviceEnvironment &rootDeviceEnvironment,
                           const aub_stream::EngineType *&mappingCopyEngineIt, uint32_t &computeEnginesCounter, uint32_t tileId) {

    tileToEngineMap.emplace(tileId, engine);
    if (engine.engineClass == ioctlHelper->getDrmParamValue(DrmParam::engineClassRender)) {
        tileToEngineToInstanceMap[tileId][EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_RCS, rootDeviceEnvironment)] = engine;
    } else if (engine.engineClass == ioctlHelper->getDrmParamValue(DrmParam::engineClassCopy)) {
        tileToEngineToInstanceMap[tileId][*(mappingCopyEngineIt)] = engine;
        bcsInfoMask.set(getBcsEngineMaskIndex(mappingCopyEngineIt++), true);
    } else if (engine.engineClass == ioctlHelper->getDrmParamValue(DrmParam::engineClassCompute)) {
        tileToEngineToInstanceMap[tileId][static_cast<aub_stream::EngineType>(aub_stream::ENGINE_CCS + computeEnginesCounter)] = engine;
        computeEnginesCounter++;
    }
}

EngineInfo::EngineInfo(Drm *drm, const std::vector<EngineCapabilities> &engineInfos) : engines(engineInfos), tileToEngineToInstanceMap(1) {
    auto computeEngines = 0u;
    BcsInfoMask bcsInfoMask = 0;
    uint32_t numHostLinkCopyEngines = 0;
    uint32_t numScaleUpLinkCopyEngines = 0;

    auto ioctlHelper = drm->getIoctlHelper();

    auto &rootDeviceEnvironment = drm->getRootDeviceEnvironment();
    for (const auto &engineInfo : engineInfos) {
        auto &engine = engineInfo.engine;
        tileToEngineMap.emplace(0, engine);
        if (engine.engineClass == ioctlHelper->getDrmParamValue(DrmParam::engineClassRender)) {
            tileToEngineToInstanceMap[0][EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_RCS, rootDeviceEnvironment)] = engine;
        } else if (engine.engineClass == ioctlHelper->getDrmParamValue(DrmParam::engineClassCopy)) {
            const auto &hwInfo = rootDeviceEnvironment.getHardwareInfo();
            assignCopyEngine(EngineInfo::getBaseCopyEngineType(ioctlHelper, engineInfo.capabilities, hwInfo->capabilityTable.isIntegratedDevice), 0, engine,
                             bcsInfoMask, numHostLinkCopyEngines, numScaleUpLinkCopyEngines);
        } else if (engine.engineClass == ioctlHelper->getDrmParamValue(DrmParam::engineClassCompute)) {
            tileToEngineToInstanceMap[0][static_cast<aub_stream::EngineType>(aub_stream::ENGINE_CCS + computeEngines)] = engine;
            computeEngines++;
        }
    }
    setSupportedEnginesInfo(rootDeviceEnvironment, computeEngines, bcsInfoMask);
}

EngineInfo::EngineInfo(Drm *drm, const StackVec<std::vector<EngineClassInstance>, 2> &engineClassInstancePerTile) : tileToEngineToInstanceMap(engineClassInstancePerTile.size()) {
    auto ioctlHelper = drm->getIoctlHelper();
    auto &rootDeviceEnvironment = drm->getRootDeviceEnvironment();
    auto computeEnginesPerTile = 0u;
    BcsInfoMask bcsInfoMask = {};

    for (auto tile = 0u; tile < engineClassInstancePerTile.size(); tile++) {
        computeEnginesPerTile = 0u;
        auto copyEnginesMappingIt = getCopyEnginesMappingIterator(rootDeviceEnvironment);

        for (const auto &engine : engineClassInstancePerTile[tile]) {
            mapEngine(ioctlHelper, engine, bcsInfoMask, rootDeviceEnvironment, copyEnginesMappingIt, computeEnginesPerTile, tile);
        }
    }
    setSupportedEnginesInfo(rootDeviceEnvironment, computeEnginesPerTile, bcsInfoMask);
}

EngineInfo::EngineInfo(Drm *drm, uint32_t tileCount, const std::vector<DistanceInfo> &distanceInfos, const std::vector<QueryItem> &queryItems, const std::vector<EngineCapabilities> &engineInfos)
    : engines(engineInfos), tileToEngineToInstanceMap(tileCount) {
    auto tile = 0u;
    auto computeEnginesPerTile = 0u;
    auto ioctlHelper = drm->getIoctlHelper();
    auto &rootDeviceEnvironment = drm->getRootDeviceEnvironment();
    BcsInfoMask bcsInfoMask = {};

    auto copyEnginesMappingIt = getCopyEnginesMappingIterator(rootDeviceEnvironment);
    for (auto i = 0u; i < distanceInfos.size(); i++) {
        if (i > 0u && distanceInfos[i].region.memoryInstance != distanceInfos[i - 1u].region.memoryInstance) {
            tile++;
            computeEnginesPerTile = 0u;
            copyEnginesMappingIt = getCopyEnginesMappingIterator(rootDeviceEnvironment);
        }
        if (queryItems[i].length < 0 || distanceInfos[i].distance != 0) {
            continue;
        }
        auto engine = distanceInfos[i].engine;
        mapEngine(ioctlHelper, engine, bcsInfoMask, rootDeviceEnvironment, copyEnginesMappingIt, computeEnginesPerTile, tile);
    }
    setSupportedEnginesInfo(rootDeviceEnvironment, computeEnginesPerTile, bcsInfoMask);
}

const EngineClassInstance *EngineInfo::getEngineInstance(uint32_t tile, aub_stream::EngineType engineType) const {
    if (tile >= tileToEngineToInstanceMap.size()) {
        return nullptr;
    }
    auto &engineToInstanceMap = tileToEngineToInstanceMap[tile];
    auto iter = engineToInstanceMap.find(engineType);
    if (iter == engineToInstanceMap.end()) {
        return nullptr;
    }
    return &iter->second;
}

void EngineInfo::setSupportedEnginesInfo(const RootDeviceEnvironment &rootDeviceEnvironment, uint32_t numComputeEngines, const BcsInfoMask &bcsInfoMask) {
    auto hwInfo = rootDeviceEnvironment.getMutableHardwareInfo();
    auto &ccsInfo = hwInfo->gtSystemInfo.CCSInfo;

    if (numComputeEngines > 0u) {
        hwInfo->featureTable.flags.ftrCCSNode = true;

        ccsInfo.IsValid = true;
        ccsInfo.NumberOfCCSEnabled = numComputeEngines;
        ccsInfo.Instances.CCSEnableMask = static_cast<uint32_t>(maxNBitValue(numComputeEngines));
    } else {
        hwInfo->capabilityTable.defaultEngineType = EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_RCS, rootDeviceEnvironment);
        hwInfo->featureTable.flags.ftrCCSNode = false;

        ccsInfo.IsValid = false;
        ccsInfo.NumberOfCCSEnabled = 0;
        ccsInfo.Instances.CCSEnableMask = 0;
    }
    hwInfo->featureTable.ftrBcsInfo = bcsInfoMask;
}

uint32_t EngineInfo::getEngineTileIndex(const EngineClassInstance &engine) {
    uint32_t tile = 0;
    if (tileToEngineMap.empty()) {
        return tile; // Empty map
    }

    for (auto itr = tileToEngineMap.begin(); itr != tileToEngineMap.end(); itr++) {
        if ((itr->second.engineClass == engine.engineClass) && (itr->second.engineInstance == engine.engineInstance)) {
            tile = itr->first;
            break;
        }
    }
    return tile;
}

void EngineInfo::getListOfEnginesOnATile(uint32_t tile, std::vector<EngineClassInstance> &listOfEngines) {
    auto range = tileToEngineMap.equal_range(tile);
    for (auto itr = range.first; itr != range.second; ++itr) {
        listOfEngines.push_back(itr->second);
    }
}

std::multimap<uint32_t, EngineClassInstance> EngineInfo::getEngineTileInfo() {
    return tileToEngineMap;
}

void EngineInfo::assignCopyEngine(aub_stream::EngineType baseEngineType, uint32_t tileId, const EngineClassInstance &engine,
                                  BcsInfoMask &bcsInfoMask, uint32_t &numHostLinkCopyEngines, uint32_t &numScaleUpLinkCopyEngines) {
    // Link copy engines:
    if (baseEngineType == DrmEngineMappingHelper::baseForHostLinkCopyEngine) {
        assignLinkCopyEngine(tileToEngineToInstanceMap, baseEngineType, tileId, engine, bcsInfoMask, numHostLinkCopyEngines);
        return;
    }

    if (baseEngineType == DrmEngineMappingHelper::baseForScaleUpLinkCopyEngine) {
        assignLinkCopyEngine(tileToEngineToInstanceMap, baseEngineType, tileId, engine, bcsInfoMask, numScaleUpLinkCopyEngines);
        return;
    }

    // Main copy engine:
    UNRECOVERABLE_IF(baseEngineType != DrmEngineMappingHelper::baseForMainCopyEngine);
    UNRECOVERABLE_IF(bcsInfoMask.test(0));
    tileToEngineToInstanceMap[tileId][aub_stream::ENGINE_BCS] = engine;
    bcsInfoMask.set(0, true);
}

bool EngineInfo::hasEngines() {
    return (tileToEngineMap.size() > 0LU);
}

// EngineIndex = (Base + EngineCounter - 1)
aub_stream::EngineType EngineInfo::getBaseCopyEngineType(IoctlHelper *ioctlHelper, uint64_t capabilities, bool isIntegratedDevice) {
    if (!isIntegratedDevice) {
        if (const auto capa = ioctlHelper->getCopyClassSaturatePCIECapability(); capa && isValueSet(capabilities, *capa)) {
            return DrmEngineMappingHelper::baseForHostLinkCopyEngine;
        }

        if (const auto capa = ioctlHelper->getCopyClassSaturateLinkCapability(); capa && isValueSet(capabilities, *capa)) {
            return DrmEngineMappingHelper::baseForScaleUpLinkCopyEngine;
        }
    }
    // no capabilites check for BCS0, to be backward compatible
    return DrmEngineMappingHelper::baseForMainCopyEngine;
}

} // namespace NEO
