/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/deferred_deleter_helper.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

namespace NEO {
bool isDeferredDeleterEnabled() {
    return debugManager.flags.EnableDeferredDeleter.get() != 0;
}
} // namespace NEO
