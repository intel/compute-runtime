/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {
template <>
uint64_t CompilerProductHelperHw<IGFX_PVC>::getHwInfoConfig(const HardwareInfo &hwInfo) const {
    return 0x0;
}

template <>
bool CompilerProductHelperHw<IGFX_PVC>::failBuildProgramWithStatefulAccessPreference() const {
    return true;
}

} // namespace NEO
