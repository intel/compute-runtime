/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_helper_base.inl"

namespace NEO {

template <typename GfxFamily>
int AubHelperHw<GfxFamily>::getDataHintForPml4Entry() const {
    if (localMemoryEnabled) {
        return AubMemDump::DataTypeHintValues::TracePpgttLevel4;
    }
    return AubMemDump::DataTypeHintValues::TraceNotype;
}

template <typename GfxFamily>
int AubHelperHw<GfxFamily>::getDataHintForPdpEntry() const {
    if (localMemoryEnabled) {
        return AubMemDump::DataTypeHintValues::TracePpgttLevel3;
    }
    return AubMemDump::DataTypeHintValues::TraceNotype;
}

template <typename GfxFamily>
int AubHelperHw<GfxFamily>::getDataHintForPdEntry() const {
    if (localMemoryEnabled) {
        return AubMemDump::DataTypeHintValues::TracePpgttLevel2;
    }
    return AubMemDump::DataTypeHintValues::TraceNotype;
}

template <typename GfxFamily>
int AubHelperHw<GfxFamily>::getDataHintForPtEntry() const {
    if (localMemoryEnabled) {
        return AubMemDump::DataTypeHintValues::TracePpgttLevel1;
    }
    return AubMemDump::DataTypeHintValues::TraceNotype;
}

} // namespace NEO
