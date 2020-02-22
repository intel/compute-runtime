/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/aub/aub_helper_base.inl"

namespace NEO {

template <typename GfxFamily>
int AubHelperHw<GfxFamily>::getDataHintForPml4Entry() const {
    return AubMemDump::DataTypeHintValues::TraceNotype;
}

template <typename GfxFamily>
int AubHelperHw<GfxFamily>::getDataHintForPdpEntry() const {
    return AubMemDump::DataTypeHintValues::TraceNotype;
}

template <typename GfxFamily>
int AubHelperHw<GfxFamily>::getDataHintForPdEntry() const {
    return AubMemDump::DataTypeHintValues::TraceNotype;
}

template <typename GfxFamily>
int AubHelperHw<GfxFamily>::getDataHintForPtEntry() const {
    return AubMemDump::DataTypeHintValues::TraceNotype;
}

} // namespace NEO
