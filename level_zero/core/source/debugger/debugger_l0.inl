/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gmm_helper/gmm_helper.h"

#include "level_zero/core/source/debugger/debugger_l0.h"

#include "hw_cmds.h"

namespace L0 {

template <typename GfxFamily>
size_t DebuggerL0Hw<GfxFamily>::getSbaTrackingCommandsSize(size_t trackedAddressCount) {
    return trackedAddressCount * NEO::EncodeStoreMemory<GfxFamily>::getStoreDataImmSize();
}

template <typename GfxFamily>
void DebuggerL0Hw<GfxFamily>::programSbaTrackingCommands(NEO::LinearStream &cmdStream, const SbaAddresses &sba) {
    auto gpuAddress = NEO::GmmHelper::decanonize(sbaTrackingGpuVa.address);

    PRINT_DEBUGGER_INFO_LOG("Debugger: SBA stored ssh = %" SCNx64
                            " gsba = %" SCNx64
                            " dsba = %" SCNx64
                            " ioba = %" SCNx64
                            " iba = %" SCNx64
                            " bsurfsba = %" SCNx64 "\n",
                            sba.SurfaceStateBaseAddress, sba.GeneralStateBaseAddress, sba.DynamicStateBaseAddress,
                            sba.IndirectObjectBaseAddress, sba.InstructionBaseAddress, sba.BindlessSurfaceStateBaseAddress);

    if (sba.GeneralStateBaseAddress) {
        NEO::EncodeStoreMemory<GfxFamily>::programStoreDataImm(cmdStream,
                                                               gpuAddress + offsetof(SbaTrackedAddresses, GeneralStateBaseAddress),
                                                               static_cast<uint32_t>(sba.GeneralStateBaseAddress & 0x0000FFFFFFFFULL),
                                                               static_cast<uint32_t>(sba.GeneralStateBaseAddress >> 32),
                                                               true,
                                                               false);
    }
    if (sba.SurfaceStateBaseAddress) {
        NEO::EncodeStoreMemory<GfxFamily>::programStoreDataImm(cmdStream,
                                                               gpuAddress + offsetof(SbaTrackedAddresses, SurfaceStateBaseAddress),
                                                               static_cast<uint32_t>(sba.SurfaceStateBaseAddress & 0x0000FFFFFFFFULL),
                                                               static_cast<uint32_t>(sba.SurfaceStateBaseAddress >> 32),
                                                               true,
                                                               false);
    }
    if (sba.DynamicStateBaseAddress) {
        NEO::EncodeStoreMemory<GfxFamily>::programStoreDataImm(cmdStream,
                                                               gpuAddress + offsetof(SbaTrackedAddresses, DynamicStateBaseAddress),
                                                               static_cast<uint32_t>(sba.DynamicStateBaseAddress & 0x0000FFFFFFFFULL),
                                                               static_cast<uint32_t>(sba.DynamicStateBaseAddress >> 32),
                                                               true,
                                                               false);
    }
    if (sba.IndirectObjectBaseAddress) {
        NEO::EncodeStoreMemory<GfxFamily>::programStoreDataImm(cmdStream,
                                                               gpuAddress + offsetof(SbaTrackedAddresses, IndirectObjectBaseAddress),
                                                               static_cast<uint32_t>(sba.IndirectObjectBaseAddress & 0x0000FFFFFFFFULL),
                                                               static_cast<uint32_t>(sba.IndirectObjectBaseAddress >> 32),
                                                               true,
                                                               false);
    }
    if (sba.InstructionBaseAddress) {
        NEO::EncodeStoreMemory<GfxFamily>::programStoreDataImm(cmdStream,
                                                               gpuAddress + offsetof(SbaTrackedAddresses, InstructionBaseAddress),
                                                               static_cast<uint32_t>(sba.InstructionBaseAddress & 0x0000FFFFFFFFULL),
                                                               static_cast<uint32_t>(sba.InstructionBaseAddress >> 32),
                                                               true,
                                                               false);
    }
    if (sba.BindlessSurfaceStateBaseAddress) {
        NEO::EncodeStoreMemory<GfxFamily>::programStoreDataImm(cmdStream,
                                                               gpuAddress + offsetof(SbaTrackedAddresses, BindlessSurfaceStateBaseAddress),
                                                               static_cast<uint32_t>(sba.BindlessSurfaceStateBaseAddress & 0x0000FFFFFFFFULL),
                                                               static_cast<uint32_t>(sba.BindlessSurfaceStateBaseAddress >> 32),
                                                               true,
                                                               false);
    }
}

template <typename GfxFamily>
DebuggerL0 *DebuggerL0Hw<GfxFamily>::allocate(NEO::Device *device) {
    return new DebuggerL0Hw<GfxFamily>(device);
}

} // namespace L0
