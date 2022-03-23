/*
 * Copyright (C) 2020-2022 Intel Corporation
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
void DebuggerL0Hw<GfxFamily>::programSbaTrackingCommands(NEO::LinearStream &cmdStream, const SbaAddresses &sba) {
    using MI_STORE_DATA_IMM = typename GfxFamily::MI_STORE_DATA_IMM;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;
    using MI_ARB_CHECK = typename GfxFamily::MI_ARB_CHECK;
    const auto gpuAddress = NEO::GmmHelper::decanonize(sbaTrackingGpuVa.address);

    PRINT_DEBUGGER_INFO_LOG("Debugger: SBA stored ssh = %" SCNx64
                            " gsba = %" SCNx64
                            " dsba = %" SCNx64
                            " ioba = %" SCNx64
                            " iba = %" SCNx64
                            " bsurfsba = %" SCNx64 "\n",
                            sba.SurfaceStateBaseAddress, sba.GeneralStateBaseAddress, sba.DynamicStateBaseAddress,
                            sba.IndirectObjectBaseAddress, sba.InstructionBaseAddress, sba.BindlessSurfaceStateBaseAddress);

    if (singleAddressSpaceSbaTracking) {
        programSbaTrackingCommandsSingleAddressSpace(cmdStream, sba);
    } else {
        if (sba.GeneralStateBaseAddress) {
            auto generalStateBaseAddress = NEO::GmmHelper::decanonize(sba.GeneralStateBaseAddress);
            NEO::EncodeStoreMemory<GfxFamily>::programStoreDataImm(cmdStream,
                                                                   gpuAddress + offsetof(SbaTrackedAddresses, GeneralStateBaseAddress),
                                                                   static_cast<uint32_t>(generalStateBaseAddress & 0x0000FFFFFFFFULL),
                                                                   static_cast<uint32_t>(generalStateBaseAddress >> 32),
                                                                   true,
                                                                   false);
        }
        if (sba.SurfaceStateBaseAddress) {
            auto surfaceStateBaseAddress = NEO::GmmHelper::decanonize(sba.SurfaceStateBaseAddress);
            NEO::EncodeStoreMemory<GfxFamily>::programStoreDataImm(cmdStream,
                                                                   gpuAddress + offsetof(SbaTrackedAddresses, SurfaceStateBaseAddress),
                                                                   static_cast<uint32_t>(surfaceStateBaseAddress & 0x0000FFFFFFFFULL),
                                                                   static_cast<uint32_t>(surfaceStateBaseAddress >> 32),
                                                                   true,
                                                                   false);
        }
        if (sba.DynamicStateBaseAddress) {
            auto dynamicStateBaseAddress = NEO::GmmHelper::decanonize(sba.DynamicStateBaseAddress);
            NEO::EncodeStoreMemory<GfxFamily>::programStoreDataImm(cmdStream,
                                                                   gpuAddress + offsetof(SbaTrackedAddresses, DynamicStateBaseAddress),
                                                                   static_cast<uint32_t>(dynamicStateBaseAddress & 0x0000FFFFFFFFULL),
                                                                   static_cast<uint32_t>(dynamicStateBaseAddress >> 32),
                                                                   true,
                                                                   false);
        }
        if (sba.IndirectObjectBaseAddress) {
            auto indirectObjectBaseAddress = NEO::GmmHelper::decanonize(sba.IndirectObjectBaseAddress);
            NEO::EncodeStoreMemory<GfxFamily>::programStoreDataImm(cmdStream,
                                                                   gpuAddress + offsetof(SbaTrackedAddresses, IndirectObjectBaseAddress),
                                                                   static_cast<uint32_t>(indirectObjectBaseAddress & 0x0000FFFFFFFFULL),
                                                                   static_cast<uint32_t>(indirectObjectBaseAddress >> 32),
                                                                   true,
                                                                   false);
        }
        if (sba.InstructionBaseAddress) {
            auto instructionBaseAddress = NEO::GmmHelper::decanonize(sba.InstructionBaseAddress);
            NEO::EncodeStoreMemory<GfxFamily>::programStoreDataImm(cmdStream,
                                                                   gpuAddress + offsetof(SbaTrackedAddresses, InstructionBaseAddress),
                                                                   static_cast<uint32_t>(instructionBaseAddress & 0x0000FFFFFFFFULL),
                                                                   static_cast<uint32_t>(instructionBaseAddress >> 32),
                                                                   true,
                                                                   false);
        }
        if (sba.BindlessSurfaceStateBaseAddress) {
            auto bindlessSurfaceStateBaseAddress = NEO::GmmHelper::decanonize(sba.BindlessSurfaceStateBaseAddress);
            NEO::EncodeStoreMemory<GfxFamily>::programStoreDataImm(cmdStream,
                                                                   gpuAddress + offsetof(SbaTrackedAddresses, BindlessSurfaceStateBaseAddress),
                                                                   static_cast<uint32_t>(bindlessSurfaceStateBaseAddress & 0x0000FFFFFFFFULL),
                                                                   static_cast<uint32_t>(bindlessSurfaceStateBaseAddress >> 32),
                                                                   true,
                                                                   false);
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
void DebuggerL0Hw<GfxFamily>::programSbaAddressLoad(NEO::LinearStream &cmdStream, uint64_t sbaGpuVa) {
    if (!singleAddressSpaceSbaTracking) {
        return;
    }
    uint32_t low = sbaGpuVa & 0xffffffff;
    uint32_t high = (sbaGpuVa >> 32) & 0xffffffff;

    NEO::LriHelper<GfxFamily>::program(&cmdStream,
                                       CS_GPR_R15,
                                       low,
                                       true);

    NEO::LriHelper<GfxFamily>::program(&cmdStream,
                                       CS_GPR_R15 + 4,
                                       high,
                                       true);
}

} // namespace L0
