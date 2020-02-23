/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/deferred_deleter_helper.h"

namespace NEO {
bool isDeferredDeleterEnabled() {
    return DebugManager.flags.EnableDeferredDeleter.get();
}
} // namespace NEO
