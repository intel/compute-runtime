/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"

#include "engine_node.h"
#include "sku_info.h"

#include <cstdint>
#include <map>
#include <vector>

namespace NEO {
struct HardwareInfo;
class Drm;

struct EngineInfo {
  public:
    using EngineToInstanceMap = std::map<aub_stream::EngineType, EngineClassInstance>;

    EngineInfo(Drm *drm, HardwareInfo *hwInfo, const std::vector<EngineCapabilities> &engineInfos);
    EngineInfo(Drm *drm, HardwareInfo *hwInfo, uint32_t tileCount, const std::vector<DistanceInfo> &distanceInfos, const std::vector<QueryItem> &queryItems, const std::vector<EngineCapabilities> &engineInfos);

    ~EngineInfo() = default;

    const EngineClassInstance *getEngineInstance(uint32_t tile, aub_stream::EngineType engineType) const;
    uint32_t getEngineTileIndex(const EngineClassInstance &engine);
    void getListOfEnginesOnATile(uint32_t tile, std::vector<EngineClassInstance> &listOfEngines);
    std::vector<EngineCapabilities> engines;

  protected:
    static aub_stream::EngineType getBaseCopyEngineType(IoctlHelper *ioctlHelper, uint64_t capabilities);
    static void setSupportedEnginesInfo(HardwareInfo *hwInfo, uint32_t numComputeEngines, const BcsInfoMask &bcsInfoMask);

    void assignCopyEngine(aub_stream::EngineType baseEngineType, uint32_t tileId, const EngineClassInstance &engine,
                          BcsInfoMask &bcsInfoMask, uint32_t &numHostLinkCopyEngines, uint32_t &numScaleUpLinkCopyEngines);

    std::vector<EngineToInstanceMap> tileToEngineToInstanceMap;
    std::multimap<uint32_t, EngineClassInstance> tileToEngineMap;
};

} // namespace NEO
