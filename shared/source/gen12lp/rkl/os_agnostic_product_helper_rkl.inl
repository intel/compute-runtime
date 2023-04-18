/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"

namespace NEO {
template <>
uint32_t ProductHelperHw<gfxProduct>::getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) const {
    switch (stepping) {
    case REVISION_A0:
        return 0x0;
    case REVISION_B:
        return 0x1;
    case REVISION_C:
        return 0x4;
    }
    return CommonConstants::invalidStepping;
}

template <>
uint32_t ProductHelperHw<gfxProduct>::getSteppingFromHwRevId(const HardwareInfo &hwInfo) const {
    switch (hwInfo.platform.usRevId) {
    case 0x0:
        return REVISION_A0;
    case 0x1:
        return REVISION_B;
    case 0x4:
        return REVISION_C;
    }
    return CommonConstants::invalidStepping;
}

template <>
bool ProductHelperHw<gfxProduct>::isForceEmuInt32DivRemSPWARequired(const HardwareInfo &hwInfo) const {
    return GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_C, hwInfo, *this);
}

template <>
bool ProductHelperHw<gfxProduct>::is3DPipelineSelectWARequired() const {
    return true;
}
} // namespace NEO
