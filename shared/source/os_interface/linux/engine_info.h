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

    EngineInfo(Drm *drm, const StackVec<std::vector<EngineClassInstance>, 2> &engineClassInstancePerTile);
    EngineInfo(Drm *drm, const std::vector<EngineCapabilities> &engineInfos);
    EngineInfo(Drm *drm, uint32_t tileCount, const std::vector<DistanceInfo> &distanceInfos, const std::vector<QueryItem> &queryItems, const std::vector<EngineCapabilities> &engineInfos);

    ~EngineInfo() = default;

    const EngineClassInstance *getEngineInstance(uint32_t tile, aub_stream::EngineType engineType) const;
    uint32_t getEngineTileIndex(const EngineClassInstance &engine);
    void getListOfEnginesOnATile(uint32_t tile, std::vector<EngineClassInstance> &listOfEngines);
    std::multimap<uint32_t, EngineClassInstance> getEngineTileInfo();
    bool hasEngines();
    const std::vector<EngineCapabilities> &getEngineInfos() const;

  protected:
    static aub_stream::EngineType getBaseCopyEngineType(IoctlHelper *ioctlHelper, uint64_t capabilities, bool isIntegratedDevice);
    static void setSupportedEnginesInfo(const RootDeviceEnvironment &rootDeviceEnvironment, uint32_t numComputeEngines, const BcsInfoMask &bcsInfoMask);

    void assignCopyEngine(aub_stream::EngineType baseEngineType, uint32_t tileId, const EngineClassInstance &engine,
                          BcsInfoMask &bcsInfoMask, uint32_t &numHostLinkCopyEngines, uint32_t &numScaleUpLinkCopyEngines);
    void mapEngine(const NEO::IoctlHelper *ioctlHelper, const EngineClassInstance &engine, BcsInfoMask &bcsInfoMask, const NEO::RootDeviceEnvironment &rootDeviceEnvironment,
                   const aub_stream::EngineType *&mappingCopyEngineIt, uint32_t &computeEnginesCounter, uint32_t tileId = 0u);

    std::vector<EngineCapabilities> engines;
    std::vector<EngineToInstanceMap> tileToEngineToInstanceMap;
    std::multimap<uint32_t, EngineClassInstance> tileToEngineMap;
};

} // namespace NEO
