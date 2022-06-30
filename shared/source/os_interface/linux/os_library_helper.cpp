/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/linux/os_library_linux.h"

#include <dlfcn.h>

namespace NEO {
namespace Linux {
void adjustLibraryFlags(int &dlopenFlag) {
    if (DebugManager.flags.DisableDeepBind.get()) {
        dlopenFlag &= ~RTLD_DEEPBIND;
    }
}
} // namespace Linux
} // namespace NEO
