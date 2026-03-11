/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/pdh_interface.h"

namespace NEO {

std::unique_ptr<PdhInterface> PdhInterface::create(ExecutionEnvironment &executionEnvironment) {
    return nullptr;
}

} // namespace NEO
