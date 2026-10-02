/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/release_helpers/release_helper/release_helper.h"

namespace NEO {

template <>
bool ReleaseHelperHw<release>::isAppTransientCoherentPatRequired() const {
    if (debugManager.flags.EnableOverrideToPat19ForSystemMemory.get() != -1) {
        return debugManager.flags.EnableOverrideToPat19ForSystemMemory.get() == 1;
    }

    return !this->is2WayCoherentPatSupported();
}

} // namespace NEO
