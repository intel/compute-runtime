/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {
template <>
uint64_t CompilerProductHelperHw<IGFX_METEORLAKE>::getHwInfoConfig(const HardwareInfo &hwInfo) const {
    return 0x0;
}

template <>
bool CompilerProductHelperHw<IGFX_METEORLAKE>::isMatrixMultiplyAccumulateSupported(const ReleaseHelper *releaseHelper) const {
    if (releaseHelper) {
        return releaseHelper->isMatrixMultiplyAccumulateSupported();
    }

    return false;
}

} // namespace NEO
