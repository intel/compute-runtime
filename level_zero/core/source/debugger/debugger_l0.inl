/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/gmm_helper/gmm_helper.h"

#include "level_zero/core/source/debugger/debugger_l0.h"

#include "hw_cmds.h"

namespace L0 {

template <typename GfxFamily>
size_t DebuggerL0Hw<GfxFamily>::getSbaTrackingCommandsSize(size_t trackedAddressCount) const {
    return trackedAddressCount * sizeof(typename GfxFamily::MI_STORE_DATA_IMM);
}

template <typename GfxFamily>
void DebuggerL0Hw<GfxFamily>::programSbaTrackingCommands(NEO::LinearStream &cmdStream, uint64_t generalStateGpuVa, uint64_t surfaceStateGpuVa) const {
    using MI_STORE_DATA_IMM = typename GfxFamily::MI_STORE_DATA_IMM;
    auto gpuAddress = NEO::GmmHelper::decanonize(sbaTrackingGpuVa.address);

    if (generalStateGpuVa) {
        MI_STORE_DATA_IMM storeDataImmediate = GfxFamily::cmdInitStoreDataImm;
        storeDataImmediate.setAddress(gpuAddress + offsetof(SbaTrackedAddresses, GeneralStateBaseAddress));
        storeDataImmediate.setStoreQword(true);
        storeDataImmediate.setDataDword0(static_cast<uint32_t>(generalStateGpuVa & 0x0000FFFFFFFFULL));
        storeDataImmediate.setDataDword1(static_cast<uint32_t>(generalStateGpuVa >> 32));

        auto storeDataImmediateSpace = cmdStream.getSpaceForCmd<MI_STORE_DATA_IMM>();
        *storeDataImmediateSpace = storeDataImmediate;
    }

    if (surfaceStateGpuVa) {
        MI_STORE_DATA_IMM storeDataImmediate = GfxFamily::cmdInitStoreDataImm;
        storeDataImmediate.setAddress(gpuAddress + offsetof(SbaTrackedAddresses, SurfaceStateBaseAddress));
        storeDataImmediate.setStoreQword(true);
        storeDataImmediate.setDataDword0(static_cast<uint32_t>(surfaceStateGpuVa & 0x0000FFFFFFFFULL));
        storeDataImmediate.setDataDword1(static_cast<uint32_t>(surfaceStateGpuVa >> 32));

        auto storeDataImmediateSpace = cmdStream.getSpaceForCmd<MI_STORE_DATA_IMM>();
        *storeDataImmediateSpace = storeDataImmediate;
    }
}

template <typename GfxFamily>
DebuggerL0 *DebuggerL0Hw<GfxFamily>::allocate(NEO::Device *device) {
    return new DebuggerL0Hw<GfxFamily>(device);
}

} // namespace L0
