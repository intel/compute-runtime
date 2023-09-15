/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <variant>

namespace NEO {
using UnifiedHandle = std::variant<int, void *>;
}; // namespace NEO
