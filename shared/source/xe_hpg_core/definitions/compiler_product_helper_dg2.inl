/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"

namespace NEO {
template <>
uint64_t CompilerProductHelperHw<IGFX_DG2>::getHwInfoConfig(const HardwareInfo &hwInfo) const {
    if (DG2::isG10(hwInfo)) {
        return 0x800040010;
    }
    return 0x200040010;
}
} // namespace NEO
