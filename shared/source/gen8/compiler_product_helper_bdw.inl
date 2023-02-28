/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
bool CompilerProductHelperHw<IGFX_BROADWELL>::isStatelessToStatefulBufferOffsetSupported() const {
    return false;
}

template <>
uint64_t CompilerProductHelperHw<IGFX_BROADWELL>::getHwInfoConfig(const HardwareInfo &hwInfo) const {
    return 0x100030008;
}
