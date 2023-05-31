/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"

#include "platforms.h"

namespace NEO {
template <>
uint64_t CompilerProductHelperHw<IGFX_DG2>::getHwInfoConfig(const HardwareInfo &hwInfo) const {
    if (DG2::isG10(hwInfo)) {
        return 0x800040010;
    }
    return 0x200040010;
}

template <>
uint32_t CompilerProductHelperHw<IGFX_DG2>::getDefaultHwIpVersion() const {
    return AOT::DG2_G10_A0;
}

template <>
uint32_t CompilerProductHelperHw<IGFX_DG2>::matchRevisionIdWithProductConfig(HardwareIpVersion ipVersion, uint32_t revisionID) const {
    HardwareIpVersion dg2Config = ipVersion;
    dg2Config.revision = revisionID;
    return dg2Config.value;
}
} // namespace NEO
