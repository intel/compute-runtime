/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/test/common/mock_gdi/mock_os_library.h"

namespace NEO {

inline const std::string getGdiName() {
    if (DebugManager.flags.OverrideGdiPath.get() != "unk") {
        return DebugManager.flags.OverrideGdiPath.get();
    } else {
        return Os::gdiDllName;
    }
}

NEO::OsLibrary *Gdi::createGdiDLL() {
    return NEO::MockOsLibrary::load(getGdiName());
}

} // namespace NEO
