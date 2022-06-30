/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/helpers/properties_helper.h"

#include <mutex>
#include <unordered_map>
#include <vector>

namespace NEO {

class MapOperationsHandler {
  public:
    virtual ~MapOperationsHandler() = default;

    bool add(void *ptr, size_t ptrLength, cl_map_flags &mapFlags, MemObjSizeArray &size, MemObjOffsetArray &offset, uint32_t mipLevel, GraphicsAllocation *graphicsAllocation);
    void remove(void *mappedPtr);
    bool find(void *mappedPtr, MapInfo &outMapInfo);
    bool findInfoForHostPtr(const void *ptr, size_t size, MapInfo &outMapInfo);
    size_t size() const;

  protected:
    bool isOverlapping(MapInfo &inputMapInfo);
    std::vector<MapInfo> mappedPointers;
    mutable std::mutex mtx;
};

class MapOperationsStorage {
  public:
    using HandlersMap = std::unordered_map<cl_mem, MapOperationsHandler>;

    MapOperationsHandler &getHandler(cl_mem memObj);
    MapOperationsHandler *getHandlerIfExists(cl_mem memObj);
    bool getInfoForHostPtr(const void *ptr, size_t size, MapInfo &outInfo);
    void removeHandler(cl_mem memObj);

  protected:
    std::mutex mutex;
    HandlersMap handlers{};
};

} // namespace NEO
