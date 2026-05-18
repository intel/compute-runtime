/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/driver_experimental/zex_api.h"
#include "level_zero/ze_intel_gpu.h"
#include <level_zero/ze_api.h>

namespace L0 {

ze_result_t ZE_APICALL zexMemFreeRegisterCallbackExt(ze_context_handle_t hContext, zex_memory_free_callback_ext_desc_t *hFreeCallbackDesc, void *ptr);

} // namespace L0
