/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/device/device.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"

namespace NEO {

template <typename GfxFamily>
void DebuggerL0Hw<GfxFamily>::captureStateBaseAddress(NEO::LinearStream &cmdStream, SbaAddresses sba, bool useFirstLevelBB) {
    const auto gmmHelper = device->getGmmHelper();
    const auto gpuAddress = gmmHelper->decanonize(sbaTrackingGpuVa.address);
    SbaAddresses sbaCanonized = sba;
    sbaCanonized.generalStateBaseAddress = gmmHelper->canonize(sba.generalStateBaseAddress);
    sbaCanonized.surfaceStateBaseAddress = gmmHelper->canonize(sba.surfaceStateBaseAddress);
    sbaCanonized.dynamicStateBaseAddress = gmmHelper->canonize(sba.dynamicStateBaseAddress);
    sbaCanonized.indirectObjectBaseAddress = gmmHelper->canonize(sba.indirectObjectBaseAddress);
    sbaCanonized.instructionBaseAddress = gmmHelper->canonize(sba.instructionBaseAddress);
    sbaCanonized.bindlessSurfaceStateBaseAddress = gmmHelper->canonize(sba.bindlessSurfaceStateBaseAddress);
    sbaCanonized.bindlessSamplerStateBaseAddress = gmmHelper->canonize(sba.bindlessSamplerStateBaseAddress);

    PRINT_DEBUGGER_INFO_LOG("Debugger: SBA stored ssh = %" SCNx64
                            " gsba = %" SCNx64
                            " dsba = %" SCNx64
                            " ioba = %" SCNx64
                            " iba = %" SCNx64
                            " bsurfsba = %" SCNx64 "\n",
                            sbaCanonized.surfaceStateBaseAddress, sbaCanonized.generalStateBaseAddress, sbaCanonized.dynamicStateBaseAddress,
                            sbaCanonized.indirectObjectBaseAddress, sbaCanonized.instructionBaseAddress, sbaCanonized.bindlessSurfaceStateBaseAddress);

    if (singleAddressSpaceSbaTracking) {
        programSbaTrackingCommandsSingleAddressSpace(cmdStream, sbaCanonized, useFirstLevelBB);
    } else {
        if (sbaCanonized.generalStateBaseAddress) {
            auto generalStateBaseAddress = sbaCanonized.generalStateBaseAddress;
            NEO::EncodeStoreMemory<GfxFamily>::programStoreDataImm(cmdStream,
                                                                   gpuAddress + offsetof(SbaTrackedAddresses, generalStateBaseAddress),
                                                                   static_cast<uint32_t>(generalStateBaseAddress & 0x0000FFFFFFFFULL),
                                                                   static_cast<uint32_t>(generalStateBaseAddress >> 32),
                                                                   true,
                                                                   false,
                                                                   nullptr);
        }
        if (sbaCanonized.surfaceStateBaseAddress) {
            auto surfaceStateBaseAddress = sbaCanonized.surfaceStateBaseAddress;
            NEO::EncodeStoreMemory<GfxFamily>::programStoreDataImm(cmdStream,
                                                                   gpuAddress + offsetof(SbaTrackedAddresses, surfaceStateBaseAddress),
                                                                   static_cast<uint32_t>(surfaceStateBaseAddress & 0x0000FFFFFFFFULL),
                                                                   static_cast<uint32_t>(surfaceStateBaseAddress >> 32),
                                                                   true,
                                                                   false,
                                                                   nullptr);
        }
        if (sbaCanonized.dynamicStateBaseAddress) {
            auto dynamicStateBaseAddress = sbaCanonized.dynamicStateBaseAddress;
            NEO::EncodeStoreMemory<GfxFamily>::programStoreDataImm(cmdStream,
                                                                   gpuAddress + offsetof(SbaTrackedAddresses, dynamicStateBaseAddress),
                                                                   static_cast<uint32_t>(dynamicStateBaseAddress & 0x0000FFFFFFFFULL),
                                                                   static_cast<uint32_t>(dynamicStateBaseAddress >> 32),
                                                                   true,
                                                                   false,
                                                                   nullptr);
        }
        if (sbaCanonized.indirectObjectBaseAddress) {
            auto indirectObjectBaseAddress = sbaCanonized.indirectObjectBaseAddress;
            NEO::EncodeStoreMemory<GfxFamily>::programStoreDataImm(cmdStream,
                                                                   gpuAddress + offsetof(SbaTrackedAddresses, indirectObjectBaseAddress),
                                                                   static_cast<uint32_t>(indirectObjectBaseAddress & 0x0000FFFFFFFFULL),
                                                                   static_cast<uint32_t>(indirectObjectBaseAddress >> 32),
                                                                   true,
                                                                   false,
                                                                   nullptr);
        }
        if (sbaCanonized.instructionBaseAddress) {
            auto instructionBaseAddress = sbaCanonized.instructionBaseAddress;
            NEO::EncodeStoreMemory<GfxFamily>::programStoreDataImm(cmdStream,
                                                                   gpuAddress + offsetof(SbaTrackedAddresses, instructionBaseAddress),
                                                                   static_cast<uint32_t>(instructionBaseAddress & 0x0000FFFFFFFFULL),
                                                                   static_cast<uint32_t>(instructionBaseAddress >> 32),
                                                                   true,
                                                                   false,
                                                                   nullptr);
        }
        if (sbaCanonized.bindlessSurfaceStateBaseAddress) {
            auto bindlessSurfaceStateBaseAddress = sbaCanonized.bindlessSurfaceStateBaseAddress;
            NEO::EncodeStoreMemory<GfxFamily>::programStoreDataImm(cmdStream,
                                                                   gpuAddress + offsetof(SbaTrackedAddresses, bindlessSurfaceStateBaseAddress),
                                                                   static_cast<uint32_t>(bindlessSurfaceStateBaseAddress & 0x0000FFFFFFFFULL),
                                                                   static_cast<uint32_t>(bindlessSurfaceStateBaseAddress >> 32),
                                                                   true,
                                                                   false,
                                                                   nullptr);
        }
    }
}

template <typename GfxFamily>
DebuggerL0 *DebuggerL0Hw<GfxFamily>::allocate(NEO::Device *device) {
    return new DebuggerL0Hw<GfxFamily>(device);
}

template <typename GfxFamily>
size_t DebuggerL0Hw<GfxFamily>::getSbaAddressLoadCommandsSize() {
    if (!singleAddressSpaceSbaTracking) {
        return 0;
    }
    return 2 * sizeof(typename GfxFamily::MI_LOAD_REGISTER_IMM);
}

template <typename GfxFamily>
void DebuggerL0Hw<GfxFamily>::programSbaAddressLoad(NEO::LinearStream &cmdStream, uint64_t sbaGpuVa, bool isBcs) {
    if (!singleAddressSpaceSbaTracking) {
        return;
    }
    uint32_t low = sbaGpuVa & 0xffffffff;
    uint32_t high = (sbaGpuVa >> 32) & 0xffffffff;

    NEO::LriHelper<GfxFamily>::program(&cmdStream,
                                       DebuggerRegisterOffsets::csGprR15,
                                       low,
                                       true,
                                       isBcs);

    NEO::LriHelper<GfxFamily>::program(&cmdStream,
                                       DebuggerRegisterOffsets::csGprR15 + 4,
                                       high,
                                       true,
                                       isBcs);
}

} // namespace NEO
