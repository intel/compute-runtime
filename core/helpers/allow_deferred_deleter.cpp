/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/debug_settings/debug_settings_manager.h"
#include "core/helpers/deferred_deleter_helper.h"

namespace NEO {
bool isDeferredDeleterEnabled() {
    return DebugManager.flags.EnableDeferredDeleter.get();
}
} // namespace NEO
