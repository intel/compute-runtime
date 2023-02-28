/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
uint64_t CompilerProductHelperHw<IGFX_ROCKETLAKE>::getHwInfoConfig(const HardwareInfo &hwInfo) const {
    return 0x100020010;
}
template <>
bool CompilerProductHelperHw<IGFX_ROCKETLAKE>::isForceEmuInt32DivRemSPRequired() const {
    return true;
}
