/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zet_api.h>

namespace L0 {
ze_result_t debugAttach(zet_device_handle_t hDevice, const zet_debug_config_t *config, zet_debug_session_handle_t *phDebug);
}