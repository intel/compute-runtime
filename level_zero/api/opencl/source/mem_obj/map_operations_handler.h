/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/api/opencl/source/helpers/properties_helper.h"

#include <mutex>
#include <vector>

namespace NEO {
namespace LEO {

class MapOperationsHandler {
  public:
    virtual ~MapOperationsHandler() = default;

    bool add(void *ptr, size_t ptrLength, cl_map_flags &mapFlags, MemObjSizeArray &size, MemObjOffsetArray &offset, uint32_t mipLevel = 0);
    void remove(void *mappedPtr);
    bool find(void *mappedPtr, MapInfo &outMapInfo);
    size_t size() const;

  protected:
    bool isOverlapping(MapInfo &inputMapInfo);
    std::vector<MapInfo> mappedPointers;
    mutable std::mutex mtx;
};

} // namespace LEO
} // namespace NEO
