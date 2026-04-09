/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {

struct DebugVariables;

void translateDebugSettingsImpl(DebugVariables &);

void translateDebugSettings(DebugVariables &debugVariables);

} // namespace NEO
