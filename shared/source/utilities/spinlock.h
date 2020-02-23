/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <mutex>

namespace NEO {
using SpinLock = std::mutex;
} // namespace NEO
