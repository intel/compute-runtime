/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/os_inc.h"

namespace NEO {

inline const std::string getGdiName() {
    if (debugManager.flags.OverrideGdiPath.get() != "unk") {
        return debugManager.flags.OverrideGdiPath.get();
    } else {
        return Os::gdiDllName;
    }
}

NEO::OsLibrary *Gdi::createGdiDLL() {
    return NEO::OsLibrary::loadFunc(getGdiName());
}

} // namespace NEO
