/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <string>

namespace NEO {
struct DebugVariables;

void injectDebugKeys(DebugVariables &flags, const std::string &envNamePrefix);
} // namespace NEO
