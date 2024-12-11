/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"

namespace NEO {

template <>
inline bool AILConfigurationHw<gfxProduct>::isAdjustMicrosecondResolutionRequired() {
    auto iterator = applicationsMicrosecontResolutionAdjustment.find(processName);
    return iterator != applicationsMicrosecontResolutionAdjustment.end();
}
} // namespace NEO