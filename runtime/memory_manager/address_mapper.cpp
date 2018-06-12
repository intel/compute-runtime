/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#include "runtime/helpers/aligned_memory.h"
#include "runtime/memory_manager/address_mapper.h"

#include <inttypes.h>
#include <iostream>

namespace OCLRT {

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
} // namespace OCLRT
