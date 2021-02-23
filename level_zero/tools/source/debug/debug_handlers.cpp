/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/debug/debug_session.h"

namespace L0 {

DebugSession *DebugSession::create(const zet_debug_config_t &config, Device *device) {
    return nullptr;
}

ze_result_t debugAttach(zet_device_handle_t hDevice, const zet_debug_config_t *config, zet_debug_session_handle_t *phDebug) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

} // namespace L0