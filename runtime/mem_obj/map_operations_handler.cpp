/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/mem_obj/map_operations_handler.h"
#include "runtime/helpers/ptr_math.h"

using namespace OCLRT;

size_t MapOperationsHandler::size() const {
    std::lock_guard<std::mutex> lock(mtx);
    return mappedPointers.size();
}

bool MapOperationsHandler::add(void *ptr, size_t ptrLength, cl_map_flags &mapFlags, MemObjSizeArray &size, MemObjOffsetArray &offset) {
    std::lock_guard<std::mutex> lock(mtx);
    MapInfo mapInfo(ptr, ptrLength, size, offset);
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
