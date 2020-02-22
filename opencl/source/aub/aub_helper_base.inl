/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/aub/aub_helper.h"
#include "opencl/source/aub_mem_dump/aub_mem_dump.h"

namespace NEO {

template <typename Family>
int AubHelperHw<Family>::getMemTraceForPml4Entry() const {
    if (localMemoryEnabled) {
        return AubMemDump::AddressSpaceValues::TraceLocal;
    }
    return AubMemDump::AddressSpaceValues::TracePml4Entry;
}

template <typename Family>
int AubHelperHw<Family>::getMemTraceForPdpEntry() const {
    if (localMemoryEnabled) {
        return AubMemDump::AddressSpaceValues::TraceLocal;
    }
    return AubMemDump::AddressSpaceValues::TracePhysicalPdpEntry;
}

template <typename Family>
int AubHelperHw<Family>::getMemTraceForPdEntry() const {
    if (localMemoryEnabled) {
        return AubMemDump::AddressSpaceValues::TraceLocal;
    }
    return AubMemDump::AddressSpaceValues::TracePpgttPdEntry;
}

template <typename Family>
int AubHelperHw<Family>::getMemTraceForPtEntry() const {
    if (localMemoryEnabled) {
        return AubMemDump::AddressSpaceValues::TraceLocal;
    }
    return AubMemDump::AddressSpaceValues::TracePpgttEntry;
}

} // namespace NEO
