/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/deferred_deleter_helper.h"

bool NEO::isDeferredDeleterEnabled() {
    return NEO::debugManager.flags.EnableDeferredDeleter.get() == 1;
} // namespace NEO
