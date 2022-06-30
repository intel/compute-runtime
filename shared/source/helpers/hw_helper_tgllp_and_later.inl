/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"

namespace NEO {

template <typename GfxFamily>
inline bool HwHelperHw<GfxFamily>::isFusedEuDispatchEnabled(const HardwareInfo &hwInfo, bool disableEUFusionForKernel) const {
    auto fusedEuDispatchEnabled = !hwInfo.workaroundTable.flags.waDisableFusedThreadScheduling;
    fusedEuDispatchEnabled &= hwInfo.capabilityTable.fusedEuEnabled;

    if (DebugManager.flags.CFEFusedEUDispatch.get() != -1) {
        fusedEuDispatchEnabled = (DebugManager.flags.CFEFusedEUDispatch.get() == 0);
    }
    return fusedEuDispatchEnabled;
}

template <typename GfxFamily>
void LriHelper<GfxFamily>::program(LinearStream *cmdStream, uint32_t address, uint32_t value, bool remap) {
    MI_LOAD_REGISTER_IMM cmd = Family::cmdInitLoadRegisterImm;
    cmd.setRegisterOffset(address);
    cmd.setDataDword(value);
    cmd.setMmioRemapEnable(remap);

    auto lri = cmdStream->getSpaceForCmd<MI_LOAD_REGISTER_IMM>();
    *lri = cmd;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::packedFormatsSupported() const {
    return true;
}

template <typename GfxFamily>
size_t HwHelperHw<GfxFamily>::getMaxFillPaternSizeForCopyEngine() const {
    return 4 * sizeof(uint32_t);
}

} // namespace NEO
