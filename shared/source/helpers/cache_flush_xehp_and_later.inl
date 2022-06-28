/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/l3_range.h"
#include "shared/source/utilities/range.h"

namespace NEO {

template <typename GfxFamily>
inline size_t getSizeNeededToFlushGpuCache(const Range<L3Range> &ranges, bool usePostSync) {
    size_t size = sizeof(typename GfxFamily::L3_CONTROL) * (ranges.size() / maxFlushSubrangeCount + 1);
    size += ranges.size() * sizeof(typename GfxFamily::L3_FLUSH_ADDRESS_RANGE);
    return size;
}
template <typename GfxFamily>
inline size_t getSizeNeededForL3Control(const Range<L3Range> &ranges) {
    size_t size = sizeof(typename GfxFamily::L3_CONTROL);
    size += ranges.size() * sizeof(typename GfxFamily::L3_FLUSH_ADDRESS_RANGE);
    return size;
}

template <typename GfxFamily>
inline void flushGpuCache(LinearStream *commandStream, const Range<L3Range> &ranges, uint64_t postSyncAddress, const HardwareInfo &hwInfo) {
    using L3_FLUSH_ADDRESS_RANGE = typename GfxFamily::L3_FLUSH_ADDRESS_RANGE;
    using L3_CONTROL = typename GfxFamily::L3_CONTROL;
    using L3_FLUSH_EVICTION_POLICY = typename GfxFamily::L3_FLUSH_ADDRESS_RANGE::L3_FLUSH_EVICTION_POLICY;

    auto l3Control = reinterpret_cast<L3_CONTROL *>(commandStream->getSpace(getSizeNeededForL3Control<GfxFamily>(ranges)));
    auto cmdL3Control = GfxFamily::cmdInitL3Control;

    uint32_t basel3ControlLength = 3;
    uint32_t sizeOfl3FlushAddressRangeInDwords = 2;
    uint32_t length = basel3ControlLength + static_cast<uint32_t>(ranges.size()) * sizeOfl3FlushAddressRangeInDwords;
    cmdL3Control.setLength(length);
    cmdL3Control.setHdcPipelineFlush(true);
    if (postSyncAddress != 0) {
        cmdL3Control.setPostSyncOperation(L3_CONTROL::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA);
        cmdL3Control.getPostSyncData().setAddress(postSyncAddress);
        cmdL3Control.getPostSyncData().setImmediateData(0);
    }
    *l3Control = cmdL3Control;

    l3Control++;
    L3_FLUSH_ADDRESS_RANGE *l3Ranges = reinterpret_cast<L3_FLUSH_ADDRESS_RANGE *>(l3Control);
    L3_FLUSH_ADDRESS_RANGE cmdFlushRange = {};
    for (const L3Range *it = &*ranges.begin(), *end = &*ranges.end(); it != end; ++it, l3Ranges++) {
        cmdFlushRange = GfxFamily::cmdInitL3FlushAddressRange;

        cmdFlushRange.setAddress(it->getMaskedAddress());
        cmdFlushRange.setAddressMask(it->getMask());
        cmdFlushRange.setL3FlushEvictionPolicy(static_cast<L3_FLUSH_EVICTION_POLICY>(it->getPolicy()));
        *l3Ranges = cmdFlushRange;
    }
}
} // namespace NEO
