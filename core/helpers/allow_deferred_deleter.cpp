/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "debug_settings/debug_settings_manager.h"
#include "helpers/deferred_deleter_helper.h"

namespace NEO {
bool isDeferredDeleterEnabled() {
    return DebugManager.flags.EnableDeferredDeleter.get();
}
} // namespace NEO
