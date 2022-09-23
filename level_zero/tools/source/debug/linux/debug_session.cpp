/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/debug/debug_session.h"

#include "level_zero/core/source/device/device.h"

namespace L0 {

DebugSession *DebugSession::create(const zet_debug_config_t &config, Device *device, ze_result_t &result, bool isRootAttach) {
    result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    return nullptr;
}
} // namespace L0
