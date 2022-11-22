/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"

namespace L0 {

template <typename Family>
bool L0HwHelperHw<Family>::platformSupportsCmdListHeapSharing() const {
    return false;
}

template <typename Family>
bool L0HwHelperHw<Family>::platformSupportsStateComputeModeTracking() const {
    return false;
}

template <typename Family>
bool L0HwHelperHw<Family>::platformSupportsFrontEndTracking() const {
    return false;
}

template <typename Family>
bool L0HwHelperHw<Family>::platformSupportsPipelineSelectTracking() const {
    return false;
}

template <typename Family>
uint32_t L0HwHelperHw<Family>::getEventMaxKernelCount(const NEO::HardwareInfo &hwInfo) const {
    return 1;
}

template <typename Family>
uint32_t L0HwHelperHw<Family>::getEventBaseMaxPacketCount(const NEO::HardwareInfo &hwInfo) const {
    return 1u;
}

template <typename Family>
bool L0HwHelperHw<Family>::platformSupportsRayTracing() const {
    return false;
}

} // namespace L0
