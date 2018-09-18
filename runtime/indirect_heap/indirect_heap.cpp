/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "indirect_heap.h"

namespace OCLRT {

IndirectHeap::IndirectHeap(GraphicsAllocation *gfxAllocation) : BaseClass(gfxAllocation) {
}

IndirectHeap::IndirectHeap(GraphicsAllocation *gfxAllocation, bool canBeUtilizedAs4GbHeap) : BaseClass(gfxAllocation), canBeUtilizedAs4GbHeap(canBeUtilizedAs4GbHeap) {
}

IndirectHeap::IndirectHeap(void *buffer, size_t bufferSize) : BaseClass(buffer, bufferSize) {
}

} // namespace OCLRT
