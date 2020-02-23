/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/memory_manager/address_mapper.h"

#include "shared/source/helpers/aligned_memory.h"

#include <inttypes.h>
#include <iostream>

namespace NEO {

AddressMapper::AddressMapper() : nextPage(1) {
}
AddressMapper::~AddressMapper() {
    for (auto &m : mapping)
        delete m;
}
uint32_t AddressMapper::map(void *vm, size_t size) {
    void *aligned = alignDown(vm, MemoryConstants::pageSize);
    size_t alignedSize = alignSizeWholePage(vm, size);

    auto it = mapping.begin();
    for (; it != mapping.end(); it++) {
        if ((*it)->vm == aligned) {
            if ((*it)->size == alignedSize) {
                return (*it)->ggtt;
            }
            break;
        }
    }
    if (it != mapping.end()) {
        delete *it;
        mapping.erase(it);
    }
    uint32_t numPages = static_cast<uint32_t>(alignedSize / MemoryConstants::pageSize);
    auto tmp = nextPage.fetch_add(numPages);

    MapInfo *m = new MapInfo;
    m->vm = aligned;
    m->size = alignedSize;
    m->ggtt = static_cast<uint32_t>(tmp * MemoryConstants::pageSize);

    mapping.push_back(m);

    return m->ggtt;
}

void AddressMapper::unmap(void *vm) {
    void *aligned = alignDown(vm, MemoryConstants::pageSize);

    auto it = mapping.begin();
    for (; it != mapping.end(); it++) {
        if ((*it)->vm == aligned) {
            break;
        }
    }
    if (it != mapping.end()) {
        delete *it;
        mapping.erase(it);
    }
}
} // namespace NEO
