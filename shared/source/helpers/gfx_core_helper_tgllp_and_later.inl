/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"

namespace NEO {

template <typename GfxFamily>
inline bool GfxCoreHelperHw<GfxFamily>::isFusedEuDispatchEnabled(const HardwareInfo &hwInfo, bool disableEUFusionForKernel) const {
    auto fusedEuDispatchEnabled = !hwInfo.workaroundTable.flags.waDisableFusedThreadScheduling;
    fusedEuDispatchEnabled &= hwInfo.capabilityTable.fusedEuEnabled;

    if (debugManager.flags.CFEFusedEUDispatch.get() != -1) {
        fusedEuDispatchEnabled = (debugManager.flags.CFEFusedEUDispatch.get() == 0);
    }
    return fusedEuDispatchEnabled;
}

template <typename GfxFamily>
void *LriHelper<GfxFamily>::program(MI_LOAD_REGISTER_IMM *lriCmd, uint32_t address, uint32_t value, bool remap, bool isBcs) {
    MI_LOAD_REGISTER_IMM cmd = Family::cmdInitLoadRegisterImm;
    address += (isBcs && remap) ? RegisterOffsets::bcs0Base : 0x0;
    cmd.setRegisterOffset(address);
    cmd.setDataDword(value);
    cmd.setMmioRemapEnable(remap);

    *lriCmd = cmd;

    return lriCmd;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::packedFormatsSupported() const {
    return true;
}

template <typename GfxFamily>
size_t GfxCoreHelperHw<GfxFamily>::getMaxFillPatternSizeForCopyEngine() const {
    return 4 * sizeof(uint32_t);
}

} // namespace NEO
