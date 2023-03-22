/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/ze_api.h>

extern "C" __attribute__((visibility("default"))) ze_result_t zelSetDriverTeardown() {
    return ZE_RESULT_SUCCESS;
}
