/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/debug_settings/definitions/translate_debug_settings.h"

namespace NEO {

void translateDebugSettings(DebugVariables &debugVariables) {
    translateDebugSettingsBase(debugVariables);
}

} // namespace NEO
