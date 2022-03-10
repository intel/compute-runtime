/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/mem_obj/map_operations_handler.h"

#include "shared/source/helpers/ptr_math.h"

using namespace NEO;

size_t MapOperationsHandler::size() const {
    std::lock_guard<std::mutex> lock(mtx);
    return mappedPointers.size();
}

bool MapOperationsHandler::add(void *ptr, size_t ptrLength, cl_map_flags &mapFlags, MemObjSizeArray &size, MemObjOffsetArray &offset, uint32_t mipLevel, GraphicsAllocation *graphicsAllocation) {
    std::lock_guard<std::mutex> lock(mtx);
    MapInfo mapInfo(ptr, ptrLength, size, offset, mipLevel);
    mapInfo.readOnly = (mapFlags == CL_MAP_READ);
    mapInfo.graphicsAllocation = graphicsAllocation;

    if (isOverlapping(mapInfo)) {
        return false;
    }

    mappedPointers.push_back(mapInfo);
    return true;
}

bool MapOperationsHandler::isOverlapping(MapInfo &inputMapInfo) {
    if (inputMapInfo.readOnly) {
        return false;
    }
    auto inputStartPtr = inputMapInfo.ptr;
    auto inputEndPtr = ptrOffset(inputStartPtr, inputMapInfo.ptrLength);

    for (auto &mapInfo : mappedPointers) {
        auto mappedStartPtr = mapInfo.ptr;
        auto mappedEndPtr = ptrOffset(mappedStartPtr, mapInfo.ptrLength);

        // Requested ptr starts before or inside existing ptr range and overlapping end
        if (inputStartPtr < mappedEndPtr && inputEndPtr >= mappedStartPtr) {
            return true;
        }
    }
    return false;
}

bool MapOperationsHandler::find(void *mappedPtr, MapInfo &outMapInfo) {
    std::lock_guard<std::mutex> lock(mtx);

    for (auto &mapInfo : mappedPointers) {
        if (mapInfo.ptr == mappedPtr) {
            outMapInfo = mapInfo;
            return true;
        }
    }
    return false;
}

bool NEO::MapOperationsHandler::findInfoForHostPtr(const void *ptr, size_t size, MapInfo &outMapInfo) {
    std::lock_guard<std::mutex> lock(mtx);

    for (auto &mapInfo : mappedPointers) {
        void *ptrStart = mapInfo.ptr;
        void *ptrEnd = ptrOffset(mapInfo.ptr, mapInfo.ptrLength);

        if (ptrStart <= ptr && ptrOffset(ptr, size) <= ptrEnd) {
            outMapInfo = mapInfo;
            return true;
        }
    }
    return false;
}

void MapOperationsHandler::remove(void *mappedPtr) {
    std::lock_guard<std::mutex> lock(mtx);

    auto endIter = mappedPointers.end();
    for (auto it = mappedPointers.begin(); it != endIter; it++) {
        if (it->ptr == mappedPtr) {
            std::iter_swap(it, mappedPointers.end() - 1);
            mappedPointers.pop_back();
            break;
        }
    }
}

MapOperationsHandler &NEO::MapOperationsStorage::getHandler(cl_mem memObj) {
    std::lock_guard<std::mutex> lock(mutex);
    return handlers[memObj];
}

MapOperationsHandler *NEO::MapOperationsStorage::getHandlerIfExists(cl_mem memObj) {
    std::lock_guard<std::mutex> lock(mutex);
    auto iterator = handlers.find(memObj);
    if (iterator == handlers.end()) {
        return nullptr;
    }

    return &iterator->second;
}

bool NEO::MapOperationsStorage::getInfoForHostPtr(const void *ptr, size_t size, MapInfo &outInfo) {
    for (auto &entry : handlers) {
        if (entry.second.findInfoForHostPtr(ptr, size, outInfo)) {
            return true;
        }
    }
    return false;
}

void NEO::MapOperationsStorage::removeHandler(cl_mem memObj) {
    std::lock_guard<std::mutex> lock(mutex);
    auto iterator = handlers.find(memObj);
    handlers.erase(iterator);
}
