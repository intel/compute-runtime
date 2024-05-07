/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"

namespace NEO {

template <typename GfxFamily>
inline bool GfxCoreHelperHw<GfxFamily>::isFusedEuDispatchEnabled(const HardwareInfo &hwInfo, bool disableEUFusionForKernel) const {
    return false;
}

template <typename GfxFamily>
void *LriHelper<GfxFamily>::program(MI_LOAD_REGISTER_IMM *lriCmd, uint32_t address, uint32_t value, bool remap, bool isBcs) {
    MI_LOAD_REGISTER_IMM cmd = GfxFamily::cmdInitLoadRegisterImm;
    address += (isBcs && remap) ? RegisterOffsets::bcs0Base : 0x0;
    cmd.setRegisterOffset(address);
    cmd.setDataDword(value);

    *lriCmd = cmd;

    return lriCmd;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::packedFormatsSupported() const {
    return false;
}

template <typename GfxFamily>
size_t GfxCoreHelperHw<GfxFamily>::getMaxFillPaternSizeForCopyEngine() const {
    return sizeof(uint32_t);
}

} // namespace NEO
