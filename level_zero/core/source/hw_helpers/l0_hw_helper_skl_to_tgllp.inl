/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"

namespace L0 {

template <typename Family>
bool L0GfxCoreHelperHw<Family>::platformSupportsCmdListHeapSharing() const {
    return false;
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::platformSupportsStateComputeModeTracking() const {
    return false;
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::platformSupportsFrontEndTracking() const {
    return false;
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::platformSupportsPipelineSelectTracking() const {
    return false;
}

template <typename Family>
uint32_t L0GfxCoreHelperHw<Family>::getEventMaxKernelCount(const NEO::HardwareInfo &hwInfo) const {
    return 1;
}

template <typename Family>
uint32_t L0GfxCoreHelperHw<Family>::getEventBaseMaxPacketCount(const NEO::HardwareInfo &hwInfo) const {
    return 1u;
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::platformSupportsRayTracing() const {
    return false;
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::isZebinAllowed(const NEO::Debugger *debugger) const {
    return !debugger;
}

} // namespace L0
