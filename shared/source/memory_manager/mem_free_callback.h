/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {

struct MemFreeCallback {
    void (*function)(void *userData);
    void *userData;
};

} // namespace NEO
