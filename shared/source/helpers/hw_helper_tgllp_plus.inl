/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {

template <>
inline bool HwHelperHw<Family>::isFusedEuDispatchEnabled(const HardwareInfo &hwInfo) const {
    auto fusedEuDispatchEnabled = !hwInfo.workaroundTable.waDisableFusedThreadScheduling;
    if (DebugManager.flags.CFEFusedEUDispatch.get() != -1) {
        fusedEuDispatchEnabled = (DebugManager.flags.CFEFusedEUDispatch.get() == 0);
    }
    return fusedEuDispatchEnabled;
}

template <>
void LriHelper<Family>::program(LinearStream *cmdStream, uint32_t address, uint32_t value, bool remap) {
    MI_LOAD_REGISTER_IMM cmd = Family::cmdInitLoadRegisterImm;
    cmd.setRegisterOffset(address);
    cmd.setDataDword(value);
    cmd.setMmioRemapEnable(remap);

    auto lri = cmdStream->getSpaceForCmd<MI_LOAD_REGISTER_IMM>();
    *lri = cmd;
}

} // namespace NEO
