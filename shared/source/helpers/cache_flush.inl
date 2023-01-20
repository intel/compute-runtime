/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/l3_range.h"
#include "shared/source/utilities/range.h"

namespace NEO {

template <typename GfxFamily>
inline void flushGpuCache(LinearStream *commandStream, const Range<L3Range> &ranges, uint64_t postSyncAddress, const RootDeviceEnvironment &rootDeviceEnvironment) {
    using L3_FLUSH_ADDRESS_RANGE = typename GfxFamily::L3_FLUSH_ADDRESS_RANGE;
    using L3_FLUSH_EVICTION_POLICY = typename GfxFamily::L3_FLUSH_ADDRESS_RANGE::L3_FLUSH_EVICTION_POLICY;
    auto templ = GfxFamily::cmdInitL3ControlWithPostSync;
    templ.getBase().setHdcPipelineFlush(true);

    auto hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto isA0Stepping = GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo, productHelper);

    for (const L3Range *it = &*ranges.begin(), *last = &*ranges.rbegin(), *end = &*ranges.end(); it != end; ++it) {
        if ((it == last) && (postSyncAddress != 0)) {
            auto l3Control = commandStream->getSpaceForCmd<typename GfxFamily::L3_CONTROL>();
            auto cmd = GfxFamily::cmdInitL3ControlWithPostSync;

            cmd.getBase().setHdcPipelineFlush(templ.getBase().getHdcPipelineFlush());
            cmd.getL3FlushAddressRange().setL3FlushEvictionPolicy(L3_FLUSH_EVICTION_POLICY::L3_FLUSH_EVICTION_POLICY_FLUSH_L3_WITH_EVICTION, isA0Stepping);
            cmd.getL3FlushAddressRange().setAddress(it->getMaskedAddress(), isA0Stepping);
            cmd.getL3FlushAddressRange().setAddressMask(it->getMask(), isA0Stepping);

            cmd.getBase().setPostSyncOperation(GfxFamily::L3_CONTROL_BASE::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA);
            cmd.getPostSyncData().setAddress(postSyncAddress);
            cmd.getPostSyncData().setImmediateData(0);
            *l3Control = cmd;
        } else {
            auto l3Control = commandStream->getSpaceForCmd<typename GfxFamily::L3_CONTROL>();
            templ.getL3FlushAddressRange().setAddress(it->getMaskedAddress(), isA0Stepping);
            templ.getL3FlushAddressRange().setAddressMask(it->getMask(), isA0Stepping);
            templ.getL3FlushAddressRange().setL3FlushEvictionPolicy(L3_FLUSH_EVICTION_POLICY::L3_FLUSH_EVICTION_POLICY_FLUSH_L3_WITH_EVICTION, isA0Stepping);
            *l3Control = templ;
        }
    }
}

template <typename GfxFamily>
inline size_t getSizeNeededToFlushGpuCache(const Range<L3Range> &ranges, bool usePostSync) {
    size_t size = ranges.size() * sizeof(typename GfxFamily::L3_CONTROL);
    if (usePostSync) {
        UNRECOVERABLE_IF(ranges.size() == 0);
    }

    return size;
}
} // namespace NEO
