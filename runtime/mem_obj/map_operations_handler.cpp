/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/mem_obj/map_operations_handler.h"

#include "runtime/helpers/ptr_math.h"

using namespace NEO;

size_t MapOperationsHandler::size() const {
    std::lock_guard<std::mutex> lock(mtx);
    return mappedPointers.size();
}

bool MapOperationsHandler::add(void *ptr, size_t ptrLength, cl_map_flags &mapFlags, MemObjSizeArray &size, MemObjOffsetArray &offset, uint32_t mipLevel) {
    std::lock_guard<std::mutex> lock(mtx);
    MapInfo mapInfo(ptr, ptrLength, size, offset, mipLevel);
    mapInfo.readOnly = (mapFlags == CL_MAP_READ);

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
