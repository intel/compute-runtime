/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <mutex>

namespace NEO {
using SpinLock = std::mutex;
using RecursiveSpinLock = std::recursive_mutex;
} // namespace NEO
