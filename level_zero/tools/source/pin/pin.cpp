/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "pin.h"

#include "level_zero/core/source/module.h"
#include "level_zero/source/inc/ze_intel_gpu.h"

namespace L0 {

static PinContext *PinContextInstance = nullptr;

void PinContext::init(ze_init_flag_t flag) {
    if (!getenv_tobool("ZE_ENABLE_PROGRAM_INSTRUMENTATION")) {
        return;
    }
    if (PinContextInstance == nullptr) {
        PinContextInstance = new PinContext();
    }
}

} // namespace L0
