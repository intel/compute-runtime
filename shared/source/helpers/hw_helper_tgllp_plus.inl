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

} // namespace NEO
