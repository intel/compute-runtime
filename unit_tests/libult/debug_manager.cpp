/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/debug_settings/debug_settings_manager.h"

#include "os_interface/ocl_reg_path.h"

using namespace std;

namespace NEO {
DebugSettingsManager<globalDebugFunctionalityLevel> DebugManager(oclRegPath);
}
