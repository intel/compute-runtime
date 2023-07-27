/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

/*
 * Copyright (C)2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/debug_settings/debug_settings_manager.h"

namespace NEO {
struct EncodeUserInterruptHelper {
    static constexpr int32_t afterSemaphoreMask = 0b01;
    static constexpr int32_t onSignalingFenceMask = 0b10;

    static bool isOperationAllowed(int32_t mode) {
        const int32_t flagValue = NEO::DebugManager.flags.ProgramUserInterruptOnResolvedDependency.get();

        return (flagValue != -1 && (flagValue & mode));
    }
};
} // namespace NEO