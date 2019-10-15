/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/deferred_deleter_helper.h"
#include "runtime/os_interface/debug_settings_manager.h"

namespace NEO {
bool isDeferredDeleterEnabled() {
    return DebugManager.flags.EnableDeferredDeleter.get();
}
} // namespace NEO
