/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/async_launch_mode.h"

namespace NEO {
std::launch getAsyncLaunchPolicy() {
    return std::launch::deferred;
}
} // namespace NEO
