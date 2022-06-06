/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/core/source/compiler_interface/l0_reg_path.h"

namespace NEO {
DebugSettingsManager<globalDebugFunctionalityLevel> DebugManager(L0::registryPath);
}
