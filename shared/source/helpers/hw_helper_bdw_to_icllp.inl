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
    return false;
}

template <typename GfxFamily>
void LriHelper<GfxFamily>::program(LinearStream *cmdStream, uint32_t address, uint32_t value, bool remap) {
    MI_LOAD_REGISTER_IMM cmd = GfxFamily::cmdInitLoadRegisterImm;
    cmd.setRegisterOffset(address);
    cmd.setDataDword(value);

    auto lri = cmdStream->getSpaceForCmd<MI_LOAD_REGISTER_IMM>();
    *lri = cmd;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::packedFormatsSupported() const {
    return false;
}

template <typename GfxFamily>
size_t HwHelperHw<GfxFamily>::getMaxFillPaternSizeForCopyEngine() const {
    return sizeof(uint32_t);
}

} // namespace NEO
