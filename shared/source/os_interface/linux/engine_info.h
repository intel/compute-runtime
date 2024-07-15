/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"

#include "aubstream/engine_node.h"
#include "sku_info.h"

#include <cstdint>
#include <map>
#include <vector>

namespace NEO {
struct RootDeviceEnvironment;
struct HardwareInfo;
class Drm;

struct EngineInfo {
  public:
    using EngineToInstanceMap = std::map<aub_stream::EngineType, EngineClassInstance>;

    EngineInfo(Drm *drm, const StackVec<std::vector<EngineCapabilities>, 2> &engineInfosPerTile);
    EngineInfo(Drm *drm, uint32_t tileCount, const std::vector<DistanceInfo> &distanceInfos, const std::vector<QueryItem> &queryItems, const std::vector<EngineCapabilities> &engineInfos);

    ~EngineInfo() = default;

    const EngineClassInstance *getEngineInstance(uint32_t tile, aub_stream::EngineType engineType) const;
    uint32_t getEngineTileIndex(const EngineClassInstance &engine);
    void getListOfEnginesOnATile(uint32_t tile, std::vector<EngineClassInstance> &listOfEngines);
    const std::multimap<uint32_t, EngineClassInstance> &getEngineTileInfo() const;
    bool hasEngines();
    const std::vector<EngineCapabilities> &getEngineInfos() const;

  protected:
    struct EngineCounters {
        uint32_t numHostLinkCopyEngines = 0;
        uint32_t numScaleUpLinkCopyEngines = 0;
        uint32_t numComputeEngines = 0;
    };

    static aub_stream::EngineType getBaseCopyEngineType(const IoctlHelper *ioctlHelper, EngineCapabilities::Flags capabilities, bool isIntegratedDevice);
    static void setSupportedEnginesInfo(const RootDeviceEnvironment &rootDeviceEnvironment, uint32_t numComputeEngines);

    void assignCopyEngine(aub_stream::EngineType baseEngineType, uint32_t tileId, const EngineClassInstance &engine,
                          BcsInfoMask &bcsInfoMask, EngineCounters &engineCounters, const aub_stream::EngineType *&mappingCopyEngineIt);
    void mapEngine(const NEO::IoctlHelper *ioctlHelper, const EngineCapabilities &engineInfo, const NEO::RootDeviceEnvironment &rootDeviceEnvironment,
                   const aub_stream::EngineType *&mappingCopyEngineIt, EngineCounters &engineCounters, uint32_t tileId);

    std::vector<EngineCapabilities> engines;
    std::vector<EngineToInstanceMap> tileToEngineToInstanceMap;
    std::multimap<uint32_t, EngineClassInstance> tileToEngineMap;
};

} // namespace NEO
