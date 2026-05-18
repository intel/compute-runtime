/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/os_interface/global_teardown_tests.h"

namespace L0 {
namespace ult {

void *backupAdditionalGlobalState() {
    return nullptr;
}

void restoreAdditionalGlobalState(void *) {
}

} // namespace ult
} // namespace L0
